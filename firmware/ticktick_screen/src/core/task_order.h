// Ordenacao das tarefas do dia (spec 5.3).
#ifndef CORE_TASK_ORDER_H
#define CORE_TASK_ORDER_H

#include "task.h"

// Comparador estrito da cadeia de quatro criterios:
//   1. atrasada antes de hoje
//   2. prioridade decrescente (5 > 3 > 1 > 0)
//   3. dueTs crescente
//   4. sortOrder do TickTick
bool task_less(const Task &a, const Task &b);

void task_sort(Task *tasks, int count);

// ---- Slots do Foco e pins (spec 5.4) ----

constexpr int FOCUS_SLOTS = 2;

struct Pin {
    char projectId[TASK_ID_BYTES + 1];
    char taskId[TASK_ID_BYTES + 1];
};

struct PinSet {
    Pin pins[FOCUS_SLOTS];
    int count;
};

bool pin_contains(const PinSet &pins, const Task &t);

enum class PinResult { Pinned, Unpinned, Full };

// Fixa se nao estiver fixada, solta se estiver. Full: ja ha FOCUS_SLOTS pins e
// este nao entrou — a UI precisa distinguir isso de "soltou" para avisar.
PinResult pin_toggle(PinSet &pins, const Task &t);

// Remove pins que nao correspondem a nenhuma tarefa da lista. Devolve quantos
// removeu. Chamar a cada ciclo, depois do filtro do dia.
int pin_prune(PinSet &pins, const Task *tasks, int count);

// Escolhe as tarefas dos slots do Foco: pins primeiro (na ordem em que foram
// fixados), o resto pela ordem da lista (que deve vir de task_sort). Escreve os
// indices escolhidos em outIdx e devolve quantos slots foram preenchidos.
int focus_select(const Task *tasks, int count, const PinSet &pins, int *outIdx);

// ---- As melhores N pela ordenacao (spec 5.1) ----

// Acumula tarefas de varias listas guardando so as `cap` primeiras pela cadeia
// de task_less, sempre em ordem. `seen` conta todas as oferecidas: seen - count
// e o "+N" da UI. Insercao ordenada: com cap = 64 e barato e dispensa ordenar
// no fim.
struct TaskTop {
    Task *items;
    int cap;
    int count;
    int seen;
};

void top_init(TaskTop &top, Task *storage, int cap);
void top_offer(TaskTop &top, const Task &t);

#endif // CORE_TASK_ORDER_H
