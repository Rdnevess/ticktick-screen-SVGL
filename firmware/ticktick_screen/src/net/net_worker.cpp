#include "net_worker.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <cstdio>
#include <cstring>

#include "../../config.h"
#include "../core/net_policy.h"
#include "../core/task_order.h"
#include "psram_sink.h"
#include "ticktick_api.h"

namespace {

struct CmdMsg {
    NetCmd cmd;
    int64_t nowUtc;
    int32_t tz;
    char projectId[TASK_ID_BYTES + 1];
    char taskId[TASK_ID_BYTES + 1];
};

QueueHandle_t s_cmdQ = nullptr;
QueueHandle_t s_evtQ = nullptr;
SemaphoreHandle_t s_lock = nullptr;    // creds, candidato, listas e TLS
SemaphoreHandle_t s_bufFree = nullptr; // buffers de resultado livres

Creds s_creds;         // em uso; vazia = sem credenciais
Creds s_candidate;     // em validacao no pareamento
uint32_t s_credsGen = 0; // incrementada a cada net_set_creds/net_clear_creds
bool s_tlsInsecure = false;
bool s_tlsDirty = true;
int s_listCount = 0;
char s_lists[MAX_LISTS][TASK_ID_BYTES + 1];

RefreshData *s_data = nullptr;     // PSRAM
ProjectList *s_projects = nullptr; // PSRAM
PsramSink s_sink;

TaskHandle_t s_workerTask = nullptr;

struct Lock {
    Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_lock); }
};

void post(NetEvtKind kind, bool ok, int http, const char *pid = nullptr,
          const char *tid = nullptr) {
    NetEvt e{};
    e.kind = kind;
    e.ok = ok;
    e.http = http;
    if (pid) task_set_id(e.projectId, pid);
    if (tid) task_set_id(e.taskId, tid);
    xQueueSend(s_evtQ, &e, portMAX_DELAY);
}

NetEvtKind reply_kind(NetCmd c) {
    switch (c) {
        case NetCmd::Validate:      return NetEvtKind::Validated;
        case NetCmd::FetchProjects: return NetEvtKind::Projects;
        case NetCmd::Refresh:       return NetEvtKind::Refreshed;
        default:                    return NetEvtKind::Completed;
    }
}

// Devolve tambem a geracao das credenciais no instante da copia: renew()
// usa para saber se s_creds ainda e a mesma fonte quando ele terminar.
uint32_t snapshot(Creds &c) {
    Lock l;
    c = s_creds;
    return s_credsGen;
}

// Renova com o refresh token e publica as credenciais novas (spec 6.4). So
// aplica se s_creds nao mudou (wipe ou novo pareamento) enquanto a chamada de
// rede estava em voo: sem isso, uma renovacao que termina depois de um
// "wipe" ou de um pareamento novo ressuscitaria credenciais apagadas ao
// sobrescrever s_creds e disparar CredsChanged, que as regravaria na NVS.
bool renew(Creds &c, int64_t nowUtc, uint32_t gen) {
    if (c.rtok[0] == '\0') return false;
    const int code = api_refresh_token(c, s_sink);
    if (code != 200) {
        Serial.printf("[net] renovacao do token: HTTP %d\n", code);
        return false;
    }
    const bool applied = creds_apply_token_response(c, s_sink.data(), s_sink.length(), nowUtc);
    s_sink.wipe(); // a resposta trazia atok/rtok em texto puro
    if (!applied) return false;
    {
        Lock l;
        if (s_credsGen != gen) {
            creds_wipe(c); // credenciais mudaram durante a renovacao: descarta
            return false;
        }
        s_creds = c;
    }
    Serial.println("[net] token renovado");
    post(NetEvtKind::CredsChanged, true, 200);
    return true;
}

// GET autenticado: um 401 tenta renovar UMA vez e repete.
int authed_get(const char *path, Creds &c, int64_t nowUtc, uint32_t gen, bool *authFailed) {
    int code = api_get(path, c.atok, s_sink);
    if (code != 401) return code;
    if (renew(c, nowUtc, gen)) code = api_get(path, c.atok, s_sink);
    if (code == 401) *authFailed = true;
    return code;
}

bool listed(const ProjectList &pl, const char *id) {
    for (int i = 0; i < pl.count; i++)
        if (std::strcmp(pl.ids[i], id) == 0) return true;
    return false;
}

void do_validate() {
    Creds c;
    {
        Lock l;
        c = s_candidate;
        creds_wipe(s_candidate); // nao deixa uma segunda copia parada na RAM
    }
    const int code = api_get("/project", c.atok, s_sink);
    api_close();
    creds_wipe(c);
    post(NetEvtKind::Validated, code == 200, code);
}

void do_projects(const CmdMsg &m) {
    Creds c;
    const uint32_t gen = snapshot(c);
    if (!creds_valid(c)) {
        post(NetEvtKind::Projects, false, NET_ERR_NOCREDS);
        return;
    }
    xSemaphoreTake(s_bufFree, portMAX_DELAY);
    bool authFailed = false;
    int code = authed_get("/project", c, m.nowUtc, gen, &authFailed);
    api_close();
    creds_wipe(c);
    if (code == 200) {
        ProjectList &pl = *s_projects;
        pl.count = payload_parse_projects(s_sink.data(), s_sink.length(), pl.ids, pl.names,
                                          MAX_PROJECTS);
        if (pl.count < 0) {
            pl.count = 0;
            code = NET_ERR_PARSE;
        }
    }
    if (code != 200) xSemaphoreGive(s_bufFree); // nada emprestado
    if (authFailed) post(NetEvtKind::AuthFailed, false, 401);
    post(NetEvtKind::Projects, code == 200, code);
}

void do_refresh(const CmdMsg &m) {
    Creds c;
    const uint32_t gen = snapshot(c);
    if (!creds_valid(c)) {
        post(NetEvtKind::Refreshed, false, NET_ERR_NOCREDS);
        return;
    }
    int n = 0;
    char lists[MAX_LISTS][TASK_ID_BYTES + 1];
    {
        Lock l;
        n = s_listCount;
        std::memcpy(lists, s_lists, sizeof(lists));
    }
    // Renovacao preventiva: falhar aqui nao e fatal, o 401 decide depois.
    if (policy_should_renew(m.nowUtc, c.exp)) renew(c, m.nowUtc, gen);

    xSemaphoreTake(s_bufFree, portMAX_DELAY);
    RefreshData &d = *s_data;
    bool authFailed = false;

    // 1) GET /project: nomes das listas e deteccao de lista apagada (desvio 5).
    int code = authed_get("/project", c, m.nowUtc, gen, &authFailed);
    if (code == 200) {
        d.projects.count = payload_parse_projects(s_sink.data(), s_sink.length(),
                                                  d.projects.ids, d.projects.names,
                                                  MAX_PROJECTS);
        if (d.projects.count < 0) code = NET_ERR_PARSE;
    }

    // 2) Uma GET /project/{id}/data por lista marcada, na mesma conexao.
    TaskTop top;
    top_init(top, d.tasks, TASK_LIST_MAX);
    // Com 64+ listas na conta, a resposta do GET /project vem cortada e uma
    // lista marcada pode so nao estar nela por causa do teto, nao por ter
    // sido apagada: nesse caso busca todas as marcadas, sem filtrar.
    const bool projectsTruncated = d.projects.count >= MAX_PROJECTS;
    for (int i = 0; i < n && code == 200; i++) {
        const bool inbox = std::strcmp(lists[i], "inbox") == 0;
        if (!inbox && !projectsTruncated && !listed(d.projects, lists[i])) continue; // apagada no app
        char path[80];
        snprintf(path, sizeof(path), "/project/%s/data", lists[i]);
        code = authed_get(path, c, m.nowUtc, gen, &authFailed);
        if (code == 404 && !inbox) {
            // Lista apagada que o GET /project cortado em 64 ainda nao deixou
            // ver, ou apagada entre as duas chamadas: pula, o resto do ciclo vale.
            api_close(); // o corpo do 404 pode ter ficado por ler
            code = 200;
            continue;
        }
        if (code == 200 && payload_parse_project(s_sink.data(), s_sink.length(),
                                                 m.nowUtc + m.tz, m.tz, top) < 0)
            code = NET_ERR_PARSE;
    }
    api_close();
    creds_wipe(c);

    d.count = top.count;
    d.seen = top.seen;
    if (code != 200) xSemaphoreGive(s_bufFree);
    if (authFailed) post(NetEvtKind::AuthFailed, false, 401);
    post(NetEvtKind::Refreshed, code == 200, code);
}

void do_complete(const CmdMsg &m) {
    Creds c;
    const uint32_t gen = snapshot(c);
    char path[112];
    snprintf(path, sizeof(path), "/project/%s/task/%s/complete", m.projectId, m.taskId);
    bool authFailed = false;
    int code = creds_valid(c) ? api_post_empty(path, c.atok) : NET_ERR_NOCREDS;
    if (code == 401) {
        if (renew(c, m.nowUtc, gen)) {
            // O 401 anterior nao teve o corpo drenado (POST sem sink): fecha
            // a conexao para nao reusar um socket com bytes por ler.
            api_close();
            code = api_post_empty(path, c.atok);
        }
        if (code == 401) authFailed = true;
    }
    api_close();
    creds_wipe(c);
    if (authFailed) post(NetEvtKind::AuthFailed, false, 401);
    post(NetEvtKind::Completed, code == 200, code, m.projectId, m.taskId);
}

void worker(void *) {
    CmdMsg m;
    for (;;) {
        if (xQueueReceive(s_cmdQ, &m, portMAX_DELAY) != pdTRUE) continue;

        bool dirty = false;
        bool insecure = false;
        {
            Lock l;
            dirty = s_tlsDirty;
            insecure = s_tlsInsecure;
            s_tlsDirty = false;
        }
        if (dirty) api_set_insecure(insecure);

        if (WiFi.status() != WL_CONNECTED) {
            // Sem wifi, do_validate() nunca roda e nunca zera s_candidate: ele
            // ficaria parado em RAM com um segredo ate o proximo pareamento.
            if (m.cmd == NetCmd::Validate) {
                Lock l;
                creds_wipe(s_candidate);
            }
            post(reply_kind(m.cmd), false, NET_ERR_NOWIFI, m.projectId, m.taskId);
            continue;
        }
        switch (m.cmd) {
            case NetCmd::Validate:      do_validate(); break;
            case NetCmd::FetchProjects: do_projects(m); break;
            case NetCmd::Refresh:       do_refresh(m); break;
            case NetCmd::Complete:      do_complete(m); break;
        }
    }
}

} // namespace

void net_begin(bool tlsInsecure) {
    if (s_cmdQ) return;
    s_tlsInsecure = tlsInsecure;
    s_cmdQ = xQueueCreate(8, sizeof(CmdMsg));
    s_evtQ = xQueueCreate(8, sizeof(NetEvt));
    s_lock = xSemaphoreCreateMutex();
    s_bufFree = xSemaphoreCreateBinary();
    xSemaphoreGive(s_bufFree);
    s_data = (RefreshData *)heap_caps_calloc(1, sizeof(RefreshData), MALLOC_CAP_SPIRAM);
    s_projects = (ProjectList *)heap_caps_calloc(1, sizeof(ProjectList), MALLOC_CAP_SPIRAM);
    if (!s_data || !s_projects) Serial.println("FATAL: sem PSRAM para o worker de rede");
    xTaskCreatePinnedToCore(worker, "net", NET_TASK_STACK, nullptr, 1, &s_workerTask, NET_TASK_CORE);
}

uint32_t net_stack_free_bytes() {
    if (!s_workerTask) return 0;
    return (uint32_t)uxTaskGetStackHighWaterMark(s_workerTask) * sizeof(StackType_t);
}

void net_set_creds(const Creds &c) {
    Lock l;
    s_creds = c;
    s_credsGen++;
}

bool net_get_creds(Creds &out) {
    Lock l;
    out = s_creds;
    return creds_valid(out);
}

bool net_has_creds() {
    if (!s_lock) return false;
    Lock l;
    return creds_valid(s_creds);
}

void net_clear_creds() {
    Lock l;
    creds_wipe(s_creds);
    s_credsGen++;
}

bool net_validate(const Creds &candidate) {
    {
        Lock l;
        s_candidate = candidate;
    }
    return net_request(NetCmd::Validate, 0, 0);
}

void net_set_lists(const char (*ids)[TASK_ID_BYTES + 1], int n) {
    Lock l;
    s_listCount = n < MAX_LISTS ? n : MAX_LISTS;
    for (int i = 0; i < s_listCount; i++) task_set_id(s_lists[i], ids[i]);
}

void net_set_tls_insecure(bool insecure) {
    Lock l;
    s_tlsInsecure = insecure;
    s_tlsDirty = true;
}

bool net_request(NetCmd cmd, int64_t nowUtc, int32_t tzOffsetSec, const char *projectId,
                 const char *taskId) {
    if (!s_cmdQ) return false;
    CmdMsg m{};
    m.cmd = cmd;
    m.nowUtc = nowUtc;
    m.tz = tzOffsetSec;
    if (projectId) task_set_id(m.projectId, projectId);
    if (taskId) task_set_id(m.taskId, taskId);
    return xQueueSend(s_cmdQ, &m, 0) == pdTRUE;
}

bool net_poll(NetEvt &out) { return s_evtQ && xQueueReceive(s_evtQ, &out, 0) == pdTRUE; }

const RefreshData &net_refresh_data() { return *s_data; }
const ProjectList &net_projects() { return *s_projects; }
void net_release() { xSemaphoreGive(s_bufFree); }
