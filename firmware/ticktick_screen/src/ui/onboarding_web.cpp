#include "onboarding_web.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <cstring>

#include "../../config.h"
#include "../app/pairing.h"
#include "../core/creds.h"
#include "../platform/clock.h"
#include "i18n.h"

static WebServer *s_web = nullptr;
static bool s_mdns = false;

static const char CSS[] =
    ":root{--bg:#0E1116;--card:#161B22;--bd:#30363D;--tx:#E6EDF3;--mut:#8B949E;"
    "--acc:#4772FA;--ok:#3FB950;--bad:#F85149}*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--tx);font-family:-apple-system,"
    "Segoe UI,Roboto,sans-serif;display:flex;min-height:100vh;align-items:center;"
    "justify-content:center}.card{background:var(--card);border:1px solid var(--bd);"
    "border-radius:14px;padding:24px;max-width:520px;width:92%}"
    "h1{font-size:19px;margin:0 0 8px}p{color:var(--mut);font-size:14px;line-height:1.5}"
    "textarea,input{width:100%;background:var(--bg);color:var(--tx);border:1px solid "
    "var(--bd);border-radius:10px;padding:12px;font-family:ui-monospace,monospace;"
    "font-size:13px;margin:6px 0}textarea{min-height:110px;resize:vertical}"
    "button{margin-top:12px;width:100%;background:var(--acc);color:#fff;border:0;"
    "border-radius:10px;padding:14px;font-size:16px;font-weight:700}"
    "summary{color:var(--mut);cursor:pointer;margin-top:10px}"
    ".ok{color:var(--ok)}.error{color:var(--bad)}code{color:var(--acc)}";

static void send_page(const String &body, int status = 200) {
    String h = F("<!doctype html><html><head><meta charset=utf-8>"
                 "<meta name=viewport content='width=device-width,initial-scale=1'>"
                 "<title>TickTick Screen</title><style>");
    h += CSS;
    h += F("</style></head><body><div class=card>");
    h += body;
    h += F("</div></body></html>");
    s_web->send(status, "text/html; charset=utf-8", h);
}

static void handle_root() {
    String b = "<h1>TickTick Screen</h1><p>";
    b += TRS("Rode <code>python helper/pair.py</code> no PC, copie o blob que ele imprime "
             "e cole abaixo. O aparelho valida com o TickTick antes de gravar.",
             "Run <code>python helper/pair.py</code> on your PC, copy the blob it prints "
             "and paste it below. The device checks it with TickTick before saving.");
    b += "</p><form method=POST action=/pair><textarea name=blob autofocus placeholder='";
    b += TRS("cole o blob aqui", "paste the blob here");
    b += "'></textarea><details><summary>";
    b += TRS("Avançado: campos separados", "Advanced: separate fields");
    b += "</summary><input name=cid placeholder=client_id autocomplete=off>"
         "<input name=csec placeholder=client_secret autocomplete=off>"
         "<input name=atok placeholder=access_token autocomplete=off>"
         "<input name=rtok placeholder='refresh_token (";
    b += TRS("opcional", "optional");
    b += ")' autocomplete=off></details><button type=submit>";
    b += TRS("Parear", "Pair");
    b += "</button></form>";
    send_page(b);
}

// Pagina de acompanhamento: consulta /result a cada segundo ate sair de
// "validating". O resultado tambem aparece na tela do aparelho.
static void send_waiting() {
    String b = "<h1>TickTick Screen</h1><p id=s>";
    b += TRS("Validando com o TickTick…", "Checking with TickTick…");
    b += F("</p><script>async function f(){try{const r=await fetch('/result');"
           "const j=await r.json();const s=document.getElementById('s');"
           "s.textContent=j.msg;s.className=j.state;"
           "if(j.state=='validating')setTimeout(f,1000);}catch(e){}}f()</script>");
    send_page(b);
}

static void send_error(const char *msg, int status = 200) {
    String b = "<h1>TickTick Screen</h1><p class=error>";
    b += msg;
    b += "</p><p><a href='/' style='color:#4772FA'>";
    b += TRS("Voltar", "Back");
    b += "</a></p>";
    send_page(b, status);
}

// Zera o buffer da String pelo ponteiro volatile, que o otimizador nao
// remove (mesmo padrao de core/creds.cpp e platform/page_wifi). `rawLen` e o
// tamanho ANTES de qualquer trim(): trim() so faz memmove + encolhe o
// length() logico, sem realocar, entao a cauda entre o novo length() e o
// tamanho original continua no buffer com pedaco do segredo. Zeramos
// max(rawLen, length()) bytes pelo ponteiro para cobrir essa cauda antes de
// soltar a String. O buffer e o da propria String (begin() nao-const devolve
// o wbuffer do core do ESP32); isso nao alcanca a copia que o WebServer
// guarda internamente em _currentArgs, limitacao conhecida e fora do nosso
// controle.
static void wipe_string(String &s, size_t rawLen) {
    const size_t total = rawLen > s.length() ? rawLen : s.length();
    volatile char *v = s.begin();
    size_t n = total;
    while (n--) *v++ = 0;
    s = String();
}

// Campo do formulario avancado, em cima da propria String (sem copia
// intermediaria) para dentro de um char[]; false se nao couber. Quem chama e
// dono de `v` e quem limpa: precisa do tamanho antes do trim() pra zerar
// direito (ver wipe_string).
static bool take_field(String &v, char *dst, size_t cap) {
    v.trim();
    if (v.length() > cap) return false;
    strlcpy(dst, v.c_str(), cap + 1);
    return true;
}

// Origem aceita pelo POST /pair: sem cabecalho Origin (curl, helper/pair.py)
// ou exatamente o proprio aparelho, por IP ou por mDNS. Uma pagina aberta em
// outra aba do navegador na mesma LAN nao pode mandar credenciais pro
// aparelho por baixo dos panos (spec 6.2: HTTP simples e o elo mais fraco).
static bool origin_allowed(const String &origin) {
    if (origin.length() == 0) return true;
    if (origin == "http://" + WiFi.localIP().toString()) return true;
    if (origin == "http://" MDNS_NAME ".local") return true;
    return false;
}

static void handle_pair() {
    // Checa a origem antes de tocar em qualquer campo: se for recusada, nada
    // do POST (blob ou campos avancados) chega a ser lido, entao nao ha nada
    // para limpar nesse caminho.
    if (!origin_allowed(s_web->header("Origin"))) {
        send_error(TRS("Origem recusada.", "Origin refused."), 403);
        return;
    }

    Creds c{};
    bool ok = false;
    String blob = s_web->arg("blob");
    const size_t blobRaw = blob.length();
    blob.trim();
    if (blob.length() > 0) {
        ok = creds_from_blob(blob.c_str(), c);
    } else {
        // Strings nomeadas: take_field trabalha em cima delas mesmas (sem
        // copia intermediaria), e guardamos o tamanho de cada uma antes do
        // trim() pra limpar a cauda direito, mesmo que a chamada seguinte
        // da cadeia && nunca rode (curto-circuito).
        String cid = s_web->arg("cid");
        String csec = s_web->arg("csec");
        String atok = s_web->arg("atok");
        String rtok = s_web->arg("rtok");
        const size_t cidRaw = cid.length();
        const size_t csecRaw = csec.length();
        const size_t atokRaw = atok.length();
        const size_t rtokRaw = rtok.length();
        ok = take_field(cid, c.cid, CRED_ID_BYTES) &&
             take_field(csec, c.csec, CRED_SECRET_BYTES) &&
             take_field(atok, c.atok, CRED_TOKEN_BYTES) &&
             take_field(rtok, c.rtok, CRED_TOKEN_BYTES) && creds_valid(c);
        if (ok && clock_has_time()) c.exp = clock_now_utc() + CREDS_DEFAULT_LIFETIME_S;
        wipe_string(cid, cidRaw);
        wipe_string(csec, csecRaw);
        wipe_string(atok, atokRaw);
        wipe_string(rtok, rtokRaw);
    }
    if (!ok) {
        send_error(TRS("Não entendi o que foi colado. Confira se o blob veio inteiro.",
                       "Couldn't read what was pasted. Make sure the whole blob is there."));
    } else if (!pairing_submit(c)) {
        send_error(TRS("Já existe uma validação em andamento. Aguarde e tente de novo.",
                       "A check is already running. Wait and try again."));
    } else {
        send_waiting();
    }
    creds_wipe(c);
    wipe_string(blob, blobRaw);
}

static void handle_result() {
    const char *state = "idle";
    char msg[160];
    strlcpy(msg, TRS("Aguardando o blob.", "Waiting for the blob."), sizeof(msg));
    switch (pairing_state()) {
        case PairState::Validating:
            state = "validating";
            strlcpy(msg, TRS("Validando com o TickTick…", "Checking with TickTick…"), sizeof(msg));
            break;
        case PairState::Accepted:
            state = "ok";
            strlcpy(msg, TRS("Pronto! Continue na tela do aparelho.",
                             "Done! Continue on the device screen."), sizeof(msg));
            break;
        case PairState::Rejected:
            state = "error";
            snprintf(msg, sizeof(msg),
                     TRS("O TickTick recusou (HTTP %d). Gere um blob novo e tente de novo.",
                         "TickTick refused it (HTTP %d). Generate a new blob and retry."),
                     pairing_http());
            break;
        default: break;
    }
    String j = "{\"state\":\"";
    j += state;
    j += "\",\"msg\":\"";
    j += msg; // textos nossos: sem aspas nem barra invertida
    j += "\"}";
    s_web->send(200, "application/json; charset=utf-8", j);
}

void portal_start() {
    if (s_web) return;
    if (!s_mdns && MDNS.begin(MDNS_NAME)) {
        MDNS.addService("http", "tcp", 80);
        s_mdns = true;
    }
    s_web = new WebServer(80);
    static const char *kHeaders[] = {"Origin"};
    s_web->collectHeaders(kHeaders, 1); // handle_pair confere contra CSRF
    s_web->on("/", HTTP_GET, handle_root);
    s_web->on("/pair", HTTP_POST, handle_pair);
    s_web->on("/result", HTTP_GET, handle_result);
    s_web->onNotFound([]() {
        s_web->sendHeader("Location", "/");
        s_web->send(302, "text/plain", "");
    });
    s_web->begin();
    Serial.println("[portal] ligado na porta 80");
}

void portal_stop() {
    if (!s_web) return;
    s_web->stop();
    delete s_web;
    s_web = nullptr;
    Serial.println("[portal] desligado");
}

bool portal_running() { return s_web != nullptr; }

void portal_tick() {
    if (s_web) s_web->handleClient();
}
