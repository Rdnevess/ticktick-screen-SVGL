#include "pairing.h"

#include <Arduino.h>

#include <cstring>

#include "../../config.h"
#include "../net/net_worker.h"
#include "../net/oauth_store.h"
#include "../platform/clock.h"
#include "../platform/console.h"
#include "../ui/page_pair.h"
#include "app.h"

static Creds s_pending;
static PairState s_state = PairState::Idle;
static int s_http = 0;
static int64_t s_advanceAtMs = 0;
static char s_sessionPin[PIN_LEN + 1];

PairState pairing_state() { return s_state; }
int pairing_http() { return s_http; }

bool pairing_submit(const Creds &c) {
    if (s_state == PairState::Validating || !creds_valid(c)) return false;
    s_pending = c;
    s_state = PairState::Validating;
    s_http = 0;
    s_advanceAtMs = 0;
    if (!net_validate(c)) {
        // fila do worker cheia: nao ha Validated a caminho, volta ao Idle.
        creds_wipe(s_pending);
        s_state = PairState::Idle;
        return false;
    }
    Serial.println("[pair] validando credenciais...");
    return true;
}

void pairing_on_validated(bool ok, int http) {
    if (s_state != PairState::Validating) return;
    s_http = http;
    if (!ok) {
        creds_wipe(s_pending);
        s_state = PairState::Rejected;
        Serial.printf("[pair] recusado: HTTP %d\n", http);
        return;
    }
    s_state = PairState::Accepted;
    s_advanceAtMs = clock_uptime_ms() + 2500;
    Serial.println("[pair] credenciais validas");
}

void pairing_finish(const char *pin) {
    // So faz sentido depois de uma validacao aceita; chamar fora disso (ordem
    // trocada, ou uma segunda chamada acidental) nao regrava nada.
    if (s_state != PairState::Accepted) return;

    page_pair_set_reason(nullptr); // o motivo antigo ("token recusado") nao vale mais

    const bool withPin = pin != nullptr && pin[0] != '\0';
    const bool saved = withPin ? oauth_save_encrypted(s_pending, pin) : oauth_save_plain(s_pending);
    if (saved) {
        Serial.printf("[pair] credenciais gravadas (%s)\n", withPin ? "cifradas" : "texto puro");
    } else {
        Serial.println("[pair] falha ao gravar credenciais");
    }
    pairing_set_session_pin(withPin ? pin : "");
    net_set_creds(s_pending);
    creds_wipe(s_pending);
    s_state = PairState::Idle;
}

const char *pairing_session_pin() { return s_sessionPin; }

void pairing_set_session_pin(const char *pin) {
    std::memset(s_sessionPin, 0, sizeof(s_sessionPin));
    strlcpy(s_sessionPin, pin ? pin : "", sizeof(s_sessionPin));
}

// Validacao passou e a pagina ja mostrou: agora a escolha do PIN (spec 6.2).
// A pagina do PIN chama pairing_finish() com o PIN ou com nullptr ("pular").
static void on_accepted() {
    app_go(Page::PinSet);
}

void pairing_tick() {
    if (s_state != PairState::Accepted || s_advanceAtMs == 0) return;
    if (clock_uptime_ms() < s_advanceAtMs) return;
    s_advanceAtMs = 0;
    on_accepted();
}

void pairing_reset() {
    if (s_state == PairState::Validating) return; // o worker ainda vai responder
    creds_wipe(s_pending);
    s_state = PairState::Idle;
    s_http = 0;
    s_advanceAtMs = 0;
}

static void cmd_pair(const char *blob) {
    Creds c{};
    if (!creds_from_blob(blob, c)) {
        Serial.println("blob invalido: cole a linha inteira que o helper imprimiu");
        return;
    }
    if (!pairing_submit(c)) Serial.println("ja existe uma validacao em andamento");
    creds_wipe(c);
}

void pairing_begin() {
    console_add("pair", "pair <blob>  pareia com o blob do helper", cmd_pair);
}
