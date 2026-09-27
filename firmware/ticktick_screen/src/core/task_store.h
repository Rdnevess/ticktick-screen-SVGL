// Dono do array de tarefas e do estado de sincronizacao (spec 5.5).
//
// Existe para que conclusao otimista e "dado desatualizado" nao vazem para a
// UI: a tela pergunta o que mostrar e recebe resposta.
#ifndef CORE_TASK_STORE_H
#define CORE_TASK_STORE_H

#include "task.h"

// Tem ~11 KB: alocar como estatico ou em PSRAM, nunca como variavel local.
struct TaskStore {
    Task tasks[TASK_LIST_MAX];
    int count;
    int truncated; // quantas passaram do teto (alimenta o "+N" da UI)
    bool stale;    // o ultimo refresh nao completou
};

// Substitui o conjunto, aplicando o teto de TASK_LIST_MAX. Carrega o `pending`
// das tarefas que ainda estavam em voo e limpa `stale` e `syncError`.
// `totalSeen` e quantas tarefas do dia existiam antes do corte (TaskTop::seen);
// -1 = usar `n`. Alimenta `truncated`, o "+N" da UI.
void store_replace(TaskStore &s, const Task *src, int n, int totalSeen = -1);

// Conclusao otimista: esconde da visao. Devolve false se a tarefa nao existe.
bool store_mark_pending(TaskStore &s, const char *projectId, const char *taskId);

// O POST voltou 200: remove de vez, preservando a ordem.
bool store_confirm_pending(TaskStore &s, const char *projectId, const char *taskId);

// O POST falhou: traz de volta com syncError.
bool store_revert_pending(TaskStore &s, const char *projectId, const char *taskId);

void store_mark_stale(TaskStore &s);

// Copia para `out` as tarefas visiveis (nao pendentes), ate `max`. Devolve
// quantas copiou.
int store_visible(const TaskStore &s, Task *out, int max);
int store_visible_count(const TaskStore &s);

#endif // CORE_TASK_STORE_H
