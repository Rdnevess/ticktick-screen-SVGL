// Pagina de uma mensagem so: boot, "sincronizando hora" e avisos de fluxo.
#ifndef UI_PAGE_MESSAGE_H
#define UI_PAGE_MESSAGE_H

#include <lvgl.h>

// Texto da proxima montagem (copiado). Chamar antes de app_go().
void page_message_set(const char *title, const char *sub);
void page_message_build(lv_obj_t *scr);
// Troca a linha de baixo com a pagina ja montada (progresso do boot).
void page_message_update(const char *sub);
void page_message_leave();

#endif // UI_PAGE_MESSAGE_H
