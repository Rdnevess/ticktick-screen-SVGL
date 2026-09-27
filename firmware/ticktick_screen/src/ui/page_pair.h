// Pagina de pareamento (spec 7.9): liga o portal, mostra onde abri-lo (nome
// mDNS e IP numerico) e acompanha a validacao. O pairing segue o fluxo sozinho
// quando as credenciais sao aceitas.
#ifndef UI_PAGE_PAIR_H
#define UI_PAGE_PAIR_H

#include <lvgl.h>

// Linha extra em vermelho na proxima montagem (ex.: "token expirado").
// nullptr limpa.
void page_pair_set_reason(const char *reason);

void page_pair_build(lv_obj_t *scr);
void page_pair_tick();
void page_pair_leave();

#endif // UI_PAGE_PAIR_H
