// Ponte entre o worker de rede e o resto do aparelho. Drena os eventos no
// loop, e dono do TaskStore e, a partir da Task 11, da agenda de refresh.
#ifndef APP_SYNC_H
#define APP_SYNC_H

#include "../core/task_store.h"
#include "../net/net_worker.h"

void sync_begin(); // sobe o worker, entrega listas e credenciais em texto puro
void sync_tick();  // no loop

TaskStore &sync_store();

// Pede GET /project. `done` e chamado uma vez, no loop; `pl` so vale durante a
// chamada (o buffer volta ao worker logo depois). false = ja ha pedido em curso.
bool sync_fetch_projects(void (*done)(bool ok, int http, const ProjectList *pl));

// Pede um ciclo ja. false se ha um em voo, se falta hora ou credencial, ou se
// nao ha lista marcada.
bool sync_refresh_now();

// ---- Agenda e estado (Task 11) ----
bool sync_stale();          // o ultimo ciclo falhou; a tela mostra o anterior
bool sync_has_data();       // ja houve ao menos um ciclo bom neste boot
int64_t sync_last_ok_utc(); // 0 = nunca
int sync_last_http();       // resultado do ultimo ciclo (200, 429, NET_ERR_*...)

// Nome da lista para os cards. "Entrada" para a Inbox; "" se desconhecida.
const char *sync_list_name(const char *projectId);

// ---- Tarefas da tela e acoes (Task 12) ----

// As tarefas visiveis (sem as pendentes), na ordem do dia. O ponteiro vale ate a
// proxima mudanca no store: callbacks devem guardar ids, nao ponteiros.
const Task *sync_visible(int *count);
int sync_truncated(); // o "+N"

// Conclusao otimista (spec 5.5): some da tela ja; o worker faz o POST. Se ele
// falhar, a tarefa volta com syncError e um aviso passageiro.
void sync_complete(const Task &t);

// Fixa/solta e grava na NVS. Full = os dois slots ja estao ocupados.
PinResult sync_toggle_pin(const Task &t);
bool sync_is_pinned(const Task &t);

int sync_done_today();

// Novo intervalo de refresh (Settings ou console): grava e reagenda.
void sync_set_poll(int minutes);

// Modo demonstracao (capturas do README): tarefas de exemplo no lugar das
// reais, refresh pausado e conclusao so local. Nada vai para a nuvem.
bool sync_demo();
void sync_set_demo(bool on);

// Rede falsa para as capturas: nunca mostra a rede real do usuario (item 2 da
// revisao final). Validas mesmo fora do modo demo (strings fixas).
const char *sync_demo_ssid();
const char *sync_demo_ip();

#endif // APP_SYNC_H
