// PIN de 4 digitos (spec 6.2), dois modos na mesma pagina:
//  Set   — depois do pareamento: "Definir PIN" e "Pular" lado a lado; definir
//          pede o PIN duas vezes.
//  Enter — no boot com credenciais cifradas: bloqueio crescente a cada erro e
//          apagamento na MAX_PIN_ATTEMPTS-esima.
// Decifrar leva ~0,2 s (KDF): roda no tick, nunca no callback do teclado.
#ifndef UI_PAGE_PIN_H
#define UI_PAGE_PIN_H

#include <lvgl.h>

void page_pin_set_build(lv_obj_t *scr);
void page_pin_enter_build(lv_obj_t *scr);
void page_pin_tick();
void page_pin_leave();

// Registra o comando de console "pin <digitos>": digita o PIN pela serial na
// tela de desbloqueio, passando pelo mesmo contador de tentativas e bloqueio
// (decisao 3 do Plano C). Quem tem o cabo USB ja tem acesso fisico.
void page_pin_begin();

// Pelo Settings (adendo do Plano C, secao 4): remove = false define um PIN novo
// (pede duas vezes e cifra as credenciais em uso); remove = true pede o PIN
// atual, com o mesmo contador e bloqueio do desbloqueio, e grava em texto puro.
// Chamar prepare antes de app_open_child(Page::PinAdmin).
void page_pin_admin_prepare(bool remove);
void page_pin_admin_build(lv_obj_t *scr);

#endif // UI_PAGE_PIN_H
