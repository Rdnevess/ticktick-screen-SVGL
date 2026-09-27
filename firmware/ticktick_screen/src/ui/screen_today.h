#ifndef UI_SCREEN_TODAY_H
#define UI_SCREEN_TODAY_H
#include <lvgl.h>
// Monta a tela dentro de `parent` (a area util de 252 px).
void today_build(lv_obj_t *parent);

// true enquanto o usuario rola a lista: o shell adia a reconstrucao para nao
// perder a posicao no meio do gesto (spec 7.1). Tem teto de 3 s, para um
// SCROLL_END perdido nao travar a tela.
bool today_busy();

// Fecha a folha de acao (troca de tela no swipe).
void today_close_sheet();

// O app destruiu a tela e o layer_top: esquece os ponteiros sem apagar nada.
void today_forget();
#endif // UI_SCREEN_TODAY_H
