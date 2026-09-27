// Dono do pomodoro na casca (spec 5.6 e 7.6; a maquina e core/pomodoro). Um
// ciclo por vez, tempo por clock_uptime_ms(), reboot perde o ciclo. Decide o
// texto do header e qual sobreposicao aparece; ui/pomo_overlay so desenha.
#ifndef APP_POMO_H
#define APP_POMO_H

#include <stdint.h>

#include "../core/pomodoro.h"

enum class PomoView : uint8_t { None, Expanded, ConfirmSwap, Finished };

void pomo_app_begin(); // registra o console "pomo"
void pomo_app_tick();  // no loop, em qualquer pagina

// Foco/Hoje: inicia; com outro ciclo rodando, pede para trocar (spec 5.6).
void pomo_app_request_start(const Task &t);
void pomo_app_toggle_expanded(); // toque no contador do header

const Pomodoro &pomo_app_state();
PomoView pomo_app_view();
void pomo_app_set_view(PomoView v);

// Acoes da sobreposicao (chamadas no tick dela, fora de callback do LVGL).
void pomo_app_confirm_swap(bool swap);
void pomo_app_cancel();
void pomo_app_renew();    // Task 4
void pomo_app_complete(); // Task 4

// O conjunto do dia mudou: a tarefa do pomodoro ainda esta nele? (Task 4)
void pomo_app_on_tasks_changed();

#endif // APP_POMO_H
