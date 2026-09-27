// Selecao de listas (spec 6.5 e 7.9): busca GET /project, mostra uma caixa por
// lista com a contagem de marcadas e o custo por ciclo, e grava a escolha.
#ifndef UI_PAGE_LISTS_H
#define UI_PAGE_LISTS_H

#include <lvgl.h>

void page_lists_build(lv_obj_t *scr);
void page_lists_tick();
void page_lists_leave();

#endif // UI_PAGE_LISTS_H
