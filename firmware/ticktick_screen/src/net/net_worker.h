// Worker de rede: uma task FreeRTOS dona do TLS, do HTTPClient e do parse.
//
// A thread da UI (loop) nunca espera a rede: pede com net_request() e recebe o
// resultado em net_poll(). Nenhuma funcao lv_* e chamada daqui.
//
// Posse dos buffers: um evento Projects ou Refreshed com ok = true EMPRESTA o
// buffer correspondente a UI. Leia o que precisar e chame net_release(); ate
// la o worker nao comeca outro FetchProjects nem Refresh.
#ifndef NET_NET_WORKER_H
#define NET_NET_WORKER_H

#include "../core/creds.h"
#include "../core/payload.h"

enum class NetCmd : uint8_t { Validate, FetchProjects, Refresh, Complete };
enum class NetEvtKind : uint8_t { Validated, Projects, Refreshed, Completed, CredsChanged, AuthFailed };

struct NetEvt {
    NetEvtKind kind;
    bool ok;
    int http; // ultimo status HTTP, ou NET_ERR_* / negativo do HTTPClient
    char projectId[TASK_ID_BYTES + 1]; // Completed
    char taskId[TASK_ID_BYTES + 1];
};

constexpr int MAX_PROJECTS = 64;

struct ProjectList {
    int count;
    char ids[MAX_PROJECTS][TASK_ID_BYTES + 1];
    char names[MAX_PROJECTS][PROJECT_NAME_BYTES + 1];
};

struct RefreshData {
    Task tasks[TASK_LIST_MAX]; // as melhores pela ordenacao (TaskTop), ja ordenadas
    int count;
    int seen;                  // tarefas do dia antes do teto: o "+N"
    ProjectList projects;      // GET /project do mesmo ciclo
};

void net_begin(bool tlsInsecure);

void net_set_creds(const Creds &c);
bool net_get_creds(Creds &out); // depois de CredsChanged
bool net_has_creds();
void net_clear_creds();

// Pareamento: valida `candidate` com GET /project sem tocar nas credenciais em
// uso. Responde com Validated. false = fila cheia; quem chama nao deve
// esperar por Validated.
bool net_validate(const Creds &candidate);

void net_set_lists(const char (*ids)[TASK_ID_BYTES + 1], int n);
void net_set_tls_insecure(bool insecure);

// Enfileira um pedido. nowUtc e o fuso vao junto porque o parse do dia e a
// decisao de renovar precisam deles, e o relogio mora na thread da UI.
bool net_request(NetCmd cmd, int64_t nowUtc, int32_t tzOffsetSec,
                 const char *projectId = nullptr, const char *taskId = nullptr);

bool net_poll(NetEvt &out); // nao bloqueia

const RefreshData &net_refresh_data();
const ProjectList &net_projects();
void net_release();

// Pilha livre (minimo historico) da task do worker, em bytes; 0 se ainda nao subiu.
uint32_t net_stack_free_bytes();

#endif // NET_NET_WORKER_H
