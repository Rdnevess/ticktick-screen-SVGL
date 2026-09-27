// Pomodoro local (spec 5.6). O tempo e sempre injetado: este modulo nunca le
// millis(). Nada aqui conversa com o TickTick — a API nao tem endpoint de foco.
#ifndef CORE_POMODORO_H
#define CORE_POMODORO_H

#include "task.h"

enum class PomoState { Idle, Running, Finished };

struct Pomodoro {
    PomoState state;
    char projectId[TASK_ID_BYTES + 1];
    char taskId[TASK_ID_BYTES + 1];
    char title[TASK_TITLE_BYTES + 1];
    int64_t startMs;
    int64_t durationMs;
    bool taskGone; // a tarefa saiu do dia durante o ciclo
};

void pomo_start(Pomodoro &p, const Task &t, int64_t nowMs, int64_t durationMs);

// Avanca a maquina. Unica transicao automatica: Running -> Finished no fim do
// tempo. Finished nunca expira sozinho.
void pomo_tick(Pomodoro &p, int64_t nowMs);

int64_t pomo_remaining_ms(const Pomodoro &p, int64_t nowMs);

void pomo_renew(Pomodoro &p, int64_t nowMs);
void pomo_cancel(Pomodoro &p);

// O botao Concluir so aparece se a tarefa ainda existe.
bool pomo_can_complete(const Pomodoro &p);
void pomo_mark_task_gone(Pomodoro &p);

bool pomo_is_for(const Pomodoro &p, const Task &t);

#endif // CORE_POMODORO_H
