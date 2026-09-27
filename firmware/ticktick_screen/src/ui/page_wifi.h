// Tela de WiFi (spec 7.9): lista as redes, pede a senha no teclado e conecta.
// A conexao bloqueia ate 15 s e roda no tick da pagina, que bombeia o LVGL;
// dentro de um callback o lv_timer_handler nao reentra e o spinner congelaria.
#ifndef UI_PAGE_WIFI_H
#define UI_PAGE_WIFI_H

#include <lvgl.h>

void page_wifi_build(lv_obj_t *scr);
void page_wifi_tick();
void page_wifi_leave();

#endif // UI_PAGE_WIFI_H
