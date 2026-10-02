// Settings (spec 7.5; adendo do Plano C, secao 4), pela engrenagem, fora do
// swipe. Preferencias (idioma, fuso, refresh, pomodoro) e, na Task 6, conta e
// aparelho. Os controles so marcam a acao; ela roda no tick.
#ifndef UI_PAGE_SETTINGS_H
#define UI_PAGE_SETTINGS_H

#include <lvgl.h>

void page_settings_build(lv_obj_t *scr);
void page_settings_tick();
void page_settings_leave();
// Rolagem inicial da lista na proxima construcao (console "go settings <px>").
void page_settings_set_scroll(int px);

#endif // UI_PAGE_SETTINGS_H
