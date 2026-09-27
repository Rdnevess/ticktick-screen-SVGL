// Cromo comum das telas e navegacao por swipe (spec 7.1).
//
// Alturas: header 40 + barra de refresh 4 + conteudo 252 + pontinhos 24 = 320.
// Header: titulo · contador do dia (ou do pomodoro) · ... · selo "desatualizado"
// · horario · engrenagem (adendo do Plano C, secao 2.1).
#ifndef UI_SHELL_H
#define UI_SHELL_H

#include <lvgl.h>
#include <stdint.h>

enum ShellScreen { SCR_FOCUS = 0, SCR_TODAY = 1, SCR_STATUS = 2, SCR_COUNT = 3 };

void shell_build();
// O app vai destruir a tela: solta os ponteiros e o callback de gesto da tela
// ativa (lv_obj_clean nao remove eventos da propria tela).
void shell_leave();
void shell_go(int screen);
void shell_next();
void shell_prev();
int shell_current();

// Progresso ate o proximo refresh, 0.0 a 1.0, na barra fina do topo.
void shell_set_refresh_progress(float frac);

// Pede reconstrucao da tela corrente no proximo shell_tick. Nunca reconstroi de
// dentro de um callback de evento: destruir o objeto que disparou o evento
// enquanto o LVGL ainda o esta processando e caminho para crash.
void shell_request_rebuild();
void shell_tick();

// Quem cada toque no header chama. O app liga: barra -> refresh, engrenagem ->
// Settings, horario -> relogio, contador -> pomodoro expandido. Os callbacks
// rodam dentro do evento do LVGL: so podem pedir trocas (app_go etc.).
void shell_set_refresh_handler(void (*fn)());
void shell_set_gear_handler(void (*fn)());
void shell_set_clock_handler(void (*fn)());
void shell_set_counter_handler(void (*fn)());

// Selo "desatualizado" no header (spec 5.5).
void shell_set_stale(bool stale);

// Contador do header (spec 7.1): concluidas hoje pelo aparelho / total do dia.
void shell_set_counter(int done, int total);

// Texto que ocupa o lugar do contador enquanto o pomodoro roda (spec 7.6).
// nullptr (ou "") volta ao contador do dia.
void shell_set_counter_override(const char *text, uint32_t color);

// Aviso passageiro de 3 s acima dos pontinhos (spec 8: "aviso passageiro").
void shell_toast(const char *msg);

#endif // UI_SHELL_H
