// Tela do relogio (adendo do Plano C, secao 2.2): HH:MM em 120 px e a data por
// extenso. Fora do swipe; qualquer toque volta para a tela principal. Tambem e
// a tela de descanso (app.cpp, idle_tick) e se desloca a cada minuto.
#ifndef UI_PAGE_CLOCK_H
#define UI_PAGE_CLOCK_H

#include <lvgl.h>

void page_clock_build(lv_obj_t *scr);
void page_clock_tick();
void page_clock_leave();

#endif // UI_PAGE_CLOCK_H
