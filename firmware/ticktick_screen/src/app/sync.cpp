#include "sync.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <cstdlib>
#include <cstring>

#include "../core/net_policy.h"
#include "../core/task_order.h"
#include "../net/oauth_store.h"
#include "../platform/clock.h"
#include "../platform/console.h"
#include "../platform/settings.h"
#include "../ui/i18n.h"
#include "../ui/page_pair.h"
#include "../ui/shell.h"
#include "app.h"
#include "pairing.h"
#include "pomo.h"

static TaskStore s_store; // ~11 KB estatico: nunca na pilha (task_store.h)
static void (*s_onProjects)(bool, int, const ProjectList *) = nullptr;

// ---- Agenda (spec 6.5 e 8) ----
static int64_t s_nextMs = 0;     // proximo ciclo; 0 = o quanto antes
static int64_t s_intervalMs = 0; // intervalo corrente (maior que o normal em recuo)
static bool s_inFlight = false;
static int64_t s_lastOkUtc = 0;
static int s_lastHttp = 0;
static int64_t s_lastBarMs = 0;
static ProjectList s_names; // nomes das listas do ultimo ciclo bom (~5 KB)

// ---- Tela ----
static Task *s_visible = nullptr; // PSRAM, TASK_LIST_MAX
static int s_visibleCount = 0;
static int s_doneToday = 0;
static int64_t s_doneDay = -1; // dia local do contador; outro dia zera
static bool s_demo = false;
static int s_realDone = 0; // contador real guardado durante a demonstracao

TaskStore &sync_store() { return s_store; }
bool sync_stale() { return s_store.stale; }
bool sync_has_data() { return s_lastOkUtc != 0; }
int64_t sync_last_ok_utc() { return s_lastOkUtc; }
int sync_last_http() { return s_lastHttp; }

static int64_t normal_ms() { return (int64_t)settings().pollMin * 60 * 1000; }

const char *sync_list_name(const char *projectId) {
    if (std::strncmp(projectId, "inbox", 5) == 0) return TRS("Entrada", "Inbox");
    for (int i = 0; i < s_names.count; i++)
        if (std::strcmp(s_names.ids[i], projectId) == 0) return s_names.names[i];
    return "";
}

bool sync_fetch_projects(void (*done)(bool ok, int http, const ProjectList *pl)) {
    if (s_onProjects) return false;
    s_onProjects = done;
    if (!net_request(NetCmd::FetchProjects, clock_now_utc(), clock_tz_offset())) {
        s_onProjects = nullptr;
        return false;
    }
    return true;
}

bool sync_refresh_now() {
    if (s_demo) return false; // demonstracao: nada de rede
    if (s_inFlight || !clock_has_time() || !net_has_creds() || settings().listCount == 0)
        return false;
    if (!net_request(NetCmd::Refresh, clock_now_utc(), clock_tz_offset())) return false;
    s_inFlight = true;
    return true;
}

static int64_t today_index() { return clock_has_time() ? clock_now_local() / 86400 : -1; }

static void roll_day() {
    const int64_t day = today_index();
    if (day != s_doneDay) {
        s_doneDay = day;
        s_doneToday = 0; // virou o dia: o contador recomeca
    }
}

// Toda mudanca no store passa por aqui: recalcula o que a tela mostra, atualiza
// o header e pede a reconstrucao, que acontece fora do callback.
static void after_store_change() {
    if (s_visible) s_visibleCount = store_visible(s_store, s_visible, TASK_LIST_MAX);
    roll_day();
    // s_store.count inclui as pendentes: uma conclusao em voo nao deve fazer o
    // total cair por um instante (revisao da Task 12).
    shell_set_counter(s_doneToday, s_doneToday + s_store.count + s_store.truncated);
    shell_set_stale(s_store.stale && sync_has_data());
    pomo_app_on_tasks_changed(); // a tarefa do pomodoro saiu do dia? (spec 5.6)
    shell_request_rebuild();
}

const Task *sync_visible(int *count) {
    *count = s_visibleCount;
    return s_visible;
}

int sync_truncated() { return s_store.truncated; }
int sync_done_today() { return s_doneToday; }
bool sync_is_pinned(const Task &t) { return pin_contains(settings().pins, t); }

void sync_complete(const Task &t) {
    // Copia os ids antes de mexer no store: `t` pode apontar para s_visible.
    char pid[TASK_ID_BYTES + 1];
    char tid[TASK_ID_BYTES + 1];
    task_set_id(pid, t.projectId);
    task_set_id(tid, t.id);
    if (!store_mark_pending(s_store, pid, tid)) return;
    if (s_demo) { // so local: confirma na hora
        store_confirm_pending(s_store, pid, tid);
        s_doneToday++;
    } else if (!net_request(NetCmd::Complete, clock_now_utc(), clock_tz_offset(), pid, tid)) {
        store_revert_pending(s_store, pid, tid);
        shell_toast(TRS("Muitos pedidos de uma vez. Tente de novo.", "Too many requests at once. Try again."));
    }
    after_store_change();
}

PinResult sync_toggle_pin(const Task &t) {
    const PinResult r = pin_toggle(settings().pins, t);
    if (r != PinResult::Full) {
        settings_save();
        after_store_change();
    }
    return r;
}

// Lista marcada que sumiu do GET /project foi apagada no app: sai da selecao
// para nao pedir /data dela a cada ciclo (desvio 5 do plano).
static void prune_lists(const ProjectList &pl) {
    if (pl.count == 0 || pl.count >= MAX_PROJECTS) return; // vazia ou cortada em 64: pode faltar lista viva na resposta
    Settings &s = settings();
    int kept = 0;
    for (int i = 0; i < s.listCount; i++) {
        bool alive = std::strcmp(s.lists[i], "inbox") == 0;
        for (int k = 0; k < pl.count && !alive; k++) alive = std::strcmp(pl.ids[k], s.lists[i]) == 0;
        if (!alive) continue;
        if (kept != i) std::memcpy(s.lists[kept], s.lists[i], sizeof(s.lists[i]));
        kept++;
    }
    if (kept == s.listCount) return;
    Serial.printf("[sync] %d lista(s) apagada(s) no app saem da selecao\n", s.listCount - kept);
    s.listCount = kept;
    settings_save();
    net_set_lists(s.lists, s.listCount);
    if (s.listCount == 0) {
        Serial.println("[sync] selecao ficou vazia: de volta a tela de listas");
        app_advance();
    }
}

static void on_projects(const NetEvt &e) {
    auto done = s_onProjects;
    s_onProjects = nullptr;
    if (done) done(e.ok, e.http, e.ok ? &net_projects() : nullptr);
    if (e.ok) net_release();
}

static void on_refreshed(const NetEvt &e) {
    s_inFlight = false;
    s_lastHttp = e.http;
    if (s_demo) { // ciclo que ja estava em voo quando a demonstracao comecou
        if (e.ok) net_release();
        return;
    }
    if (!e.ok) {
        // Mantem o conjunto anterior com o selo (spec 8). 429 recua dobrando.
        store_mark_stale(s_store);
        s_intervalMs = (e.http == 429)
                           ? policy_backoff(s_intervalMs / 1000, normal_ms() / 1000) * 1000
                           : normal_ms();
        // 401: a renovacao ja levou ao pareamento (on_auth_failed); ao voltar
        // com credencial nova, nao esperar o resto do intervalo antigo.
        s_nextMs = (e.http == 401) ? 0 : clock_uptime_ms() + s_intervalMs;
        Serial.printf("[sync] refresh falhou (%d); proximo em %d s\n", e.http,
                      (int)(s_intervalMs / 1000));
        after_store_change();
        return;
    }

    const RefreshData &d = net_refresh_data();
    const int seen = d.seen;
    store_replace(s_store, d.tasks, d.count, d.seen);
    s_names = d.projects;
    net_release();

    prune_lists(s_names);
    // Com o teto de 64 batido, a lista de tarefas visiveis nao e a conta
    // toda: nao apagar pin de tarefa que so nao apareceu por causa do corte.
    if (s_store.truncated == 0 && pin_prune(settings().pins, s_store.tasks, s_store.count) > 0)
        settings_save();

    s_lastOkUtc = clock_now_utc();
    s_intervalMs = normal_ms();
    s_nextMs = clock_uptime_ms() + s_intervalMs;
    Serial.printf("[sync] %d tarefas do dia, %d visiveis, +%d fora do teto\n", seen,
                  store_visible_count(s_store), s_store.truncated);
    after_store_change();
}

static void on_completed(const NetEvt &e) {
    if (e.ok) {
        store_confirm_pending(s_store, e.projectId, e.taskId);
        // Tarefa fixada concluida: solta o slot ja, sem esperar o proximo refresh.
        Task done{};
        task_set_id(done.projectId, e.projectId);
        task_set_id(done.id, e.taskId);
        if (pin_contains(settings().pins, done)) {
            pin_toggle(settings().pins, done);
            settings_save();
        }
        roll_day();
        s_doneToday++;
        // Coalesce em vez de zerar: um ciclo por tarefa concluida atropelaria o
        // recuo do 429. So antecipa (3 s) quando nao esta em recuo.
        if (s_intervalMs <= normal_ms()) {
            const int64_t soon = clock_uptime_ms() + 3000;
            if (soon < s_nextMs) s_nextMs = soon;
        }
    } else {
        store_revert_pending(s_store, e.projectId, e.taskId);
        shell_toast(TRS("Não deu para concluir. Tente de novo.", "Couldn't complete it. Try again."));
        Serial.printf("[sync] complete falhou: %d\n", e.http);
    }
    after_store_change();
}

static void on_creds_changed() {
    Creds c{};
    if (net_get_creds(c)) {
        const char *pin = pairing_session_pin();
        if (pin[0] != '\0') oauth_save_encrypted(c, pin);
        else oauth_save_plain(c);
    }
    creds_wipe(c);
}

// 401 que a renovacao nao resolveu (spec 6.4 e 8): falha alta e recuperavel.
static void on_auth_failed() {
    Serial.println("[sync] token recusado e renovacao falhou: pareamento");
    s_lastHttp = 401; // Status mostra o codigo certo mesmo vindo de Complete/FetchProjects
    store_mark_stale(s_store);
    page_pair_set_reason(TRS("O TickTick recusou o token. Refaça o pareamento.",
                             "TickTick refused the token. Please pair again."));
    app_go_root(Page::Pair); // nao e pagina filha: sem isso sobra um "Voltar" orfao
}

void sync_tick() {
    NetEvt e;
    while (net_poll(e)) {
        switch (e.kind) {
            case NetEvtKind::Validated:    pairing_on_validated(e.ok, e.http); break;
            case NetEvtKind::Projects:     on_projects(e); break;
            case NetEvtKind::Refreshed:    on_refreshed(e); break;
            case NetEvtKind::Completed:    on_completed(e); break;
            case NetEvtKind::CredsChanged: on_creds_changed(); break;
            case NetEvtKind::AuthFailed:   on_auth_failed(); break;
        }
    }

    // Virada da meia-noite sem refresh: o contador do dia recomeca sozinho.
    if (s_doneDay != -1 && today_index() != s_doneDay) after_store_change();

    // A agenda corre com a tela principal ou o relogio na frente (adendo 2.2):
    // no pareamento, na selecao de listas ou no PIN, pedir dados nao faz sentido.
    if (app_page() != Page::Main && app_page() != Page::Clock) return;
    const int64_t now = clock_uptime_ms();
    if (!s_inFlight && now >= s_nextMs) sync_refresh_now();

    if (now - s_lastBarMs >= 250) {
        s_lastBarMs = now;
        float frac = 1.0f; // cheia enquanto o ciclo esta em voo
        if (!s_inFlight && s_intervalMs > 0)
            frac = 1.0f - (float)(s_nextMs - now) / (float)s_intervalMs;
        shell_set_refresh_progress(frac);
    }
}

// ---- Demonstracao ----

struct DemoTask {
    const char *pt;
    const char *en;
    const char *list; // "inbox", "demo-casa" ou "demo-trab"
    int dayOffset;    // -1 = ontem (atrasada)
    int minute;       // minuto do dia; -1 = dia inteiro
    int priority;
};

static const DemoTask DEMO[] = {
    {"Revisar a proposta do cliente", "Review the client proposal", "demo-trab", 0, 10 * 60, 5},
    {"Pagar a conta de luz", "Pay the electricity bill", "demo-casa", -1, -1, 3},
    {"Responder os e-mails pendentes", "Answer pending emails", "demo-trab", 0, 11 * 60, 1},
    {"Ligar para o dentista", "Call the dentist", "inbox", 0, 14 * 60 + 30, 1},
    {"Preparar a apresentação de sexta", "Prepare Friday's presentation", "demo-trab", 0, 16 * 60, 3},
    {"Comprar pão e frutas", "Buy bread and fruit", "demo-casa", 0, -1, 0},
    {"Regar as plantas", "Water the plants", "demo-casa", 0, -1, 0},
    {"Separar a roupa para lavar", "Sort the laundry", "inbox", 0, -1, 0},
};
static const int DEMO_COUNT = sizeof(DEMO) / sizeof(DEMO[0]);

bool sync_demo() { return s_demo; }

const char *sync_demo_ssid() { return TRS("MinhaRede", "MyNetwork"); }
const char *sync_demo_ip() { return "192.168.0.42"; }

void sync_set_demo(bool on) {
    if (on == s_demo) return;
    if (on) {
        if (!clock_has_time()) {
            Serial.println("demo: espere a hora sincronizar");
            return;
        }
        static Task demo[DEMO_COUNT];
        const int64_t day0 = clock_now_local() / 86400 * 86400;
        for (int i = 0; i < DEMO_COUNT; i++) {
            const DemoTask &d = DEMO[i];
            Task &t = demo[i];
            t = Task{};
            char id[12];
            snprintf(id, sizeof(id), "demo%d", i);
            task_set_id(t.id, id);
            task_set_id(t.projectId, d.list);
            task_set_title(t, settings().langEn ? d.en : d.pt);
            t.isAllDay = d.minute < 0;
            t.dueTs = day0 + (int64_t)d.dayOffset * 86400 + (t.isAllDay ? 86399 : (int64_t)d.minute * 60);
            t.overdue = d.dayOffset < 0;
            t.priority = d.priority;
            t.sortOrder = i;
        }
        task_sort(demo, DEMO_COUNT);
        s_demo = true;
        s_realDone = s_doneToday;
        s_doneToday = 3;
        store_replace(s_store, demo, DEMO_COUNT);
        s_names.count = 2;
        task_set_id(s_names.ids[0], "demo-casa");
        strlcpy(s_names.names[0], TRS("Casa", "Home"), sizeof(s_names.names[0]));
        task_set_id(s_names.ids[1], "demo-trab");
        strlcpy(s_names.names[1], TRS("Trabalho", "Work"), sizeof(s_names.names[1]));
        if (s_lastOkUtc == 0) s_lastOkUtc = clock_now_utc();
        Serial.println("demo ligado: refresh pausado, conclusao so local");
    } else {
        s_demo = false;
        s_doneToday = s_realDone;
        store_replace(s_store, nullptr, 0);
        s_names.count = 0;
        s_nextMs = 0; // busca os dados reais no proximo tick
        Serial.println("demo desligado: buscando os dados reais");
    }
    after_store_change();
}

static void cmd_demo(const char *a) {
    if (std::strcmp(a, "on") == 0) sync_set_demo(true);
    else if (std::strcmp(a, "off") == 0) sync_set_demo(false);
    else Serial.println("uso: demo on|off");
}

// ---- Console ----

static void print_projects(bool ok, int http, const ProjectList *pl) {
    if (!ok) {
        Serial.printf("GET /project falhou: %d\n", http);
        return;
    }
    Serial.printf("%d listas:\n", pl->count);
    for (int i = 0; i < pl->count; i++) Serial.printf("  %s  %s\n", pl->ids[i], pl->names[i]);
}

static void cmd_projects(const char *) {
    if (!sync_fetch_projects(print_projects)) Serial.println("ocupado; tente de novo");
}

static void cmd_refresh(const char *) {
    Serial.println(sync_refresh_now() ? "refresh pedido"
                                      : "agora nao (ciclo em voo, sem hora, sem credencial ou sem lista)");
}

static void cmd_wipe(const char *) {
    oauth_wipe();
    net_clear_creds();
    pairing_set_session_pin("");
    Serial.println("credenciais apagadas");
    app_advance(); // sem credencial: sai da tela principal para o pareamento
}

static void cmd_tls(const char *a) {
    const bool insecure = std::strcmp(a, "inseguro") == 0;
    if (!insecure && std::strcmp(a, "cadeia") != 0) {
        Serial.println("uso: tls cadeia|inseguro");
        return;
    }
    settings().tlsInsecure = insecure;
    settings_save();
    net_set_tls_insecure(insecure);
    Serial.printf("TLS: %s\n", insecure ? "setInsecure (sem validar certificado)" : "cadeia embutida");
}

void sync_set_poll(int minutes) {
    settings().pollMin = minutes;
    settings_save();
    s_intervalMs = normal_ms();
    s_nextMs = clock_uptime_ms() + s_intervalMs;
}

static void cmd_poll(const char *a) {
    const int min = atoi(a);
    if (min < 1 || min > 120) {
        Serial.println("uso: poll <minutos 1-120>");
        return;
    }
    sync_set_poll(min);
    Serial.printf("refresh a cada %d min\n", min);
}

void sync_begin() {
    net_begin(settings().tlsInsecure);
    s_visible = (Task *)heap_caps_calloc(TASK_LIST_MAX, sizeof(Task), MALLOC_CAP_SPIRAM);
    net_set_lists(settings().lists, settings().listCount);
    if (oauth_state() == CredsState::Plain) {
        Creds c{};
        if (oauth_load_plain(c)) net_set_creds(c);
        creds_wipe(c);
    }
    console_add("projects", "lista as listas da conta (GET /project)", cmd_projects);
    console_add("refresh", "busca as tarefas do dia agora", cmd_refresh);
    console_add("wipe", "apaga as credenciais do TickTick", cmd_wipe);
    console_add("tls", "tls cadeia|inseguro  validacao do certificado", cmd_tls);
    console_add("poll", "poll <min>  intervalo do refresh", cmd_poll);
    console_add("demo", "demo on|off  tarefas de exemplo (capturas)", cmd_demo);
}
