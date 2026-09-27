// Paleta, fontes e construtores de widget (spec 7.7 e 7.8).
#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>
#include <stdint.h>

// Paleta (spec 7.8)
constexpr uint32_t COL_BG      = 0x0E1116;
constexpr uint32_t COL_SURFACE = 0x161B22;
constexpr uint32_t COL_ACCENT  = 0x4772FA; // azul TickTick; tambem prioridade baixa
constexpr uint32_t COL_OK      = 0x3FB950;
constexpr uint32_t COL_LATE    = 0xF85149; // atrasada e prioridade alta
constexpr uint32_t COL_MED     = 0xD29922;
constexpr uint32_t COL_TEXT    = 0xE6EDF3;
constexpr uint32_t COL_DIM     = 0x8B949E; // secundario e prioridade nenhuma

lv_color_t theme_color(uint32_t hex);

// Cor e rotulo da prioridade do TickTick (0/1/3/5). Prioridade nenhuma devolve
// string vazia: a UI omite em vez de escrever "nenhuma" (spec 7.2).
uint32_t prio_color(int priority);
const char *prio_label(int priority);

// Painel sem borda, fundo COL_SURFACE, cantos arredondados.
lv_obj_t *ui_panel(lv_obj_t *parent, int w, int h);

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                   uint32_t color);

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, int w, int h,
                    uint32_t bg);

// Label de icone. Usa lv_font_montserrat_18 porque os LV_SYMBOL_* vivem na
// faixa 0xF000+ das fontes de fabrica e as font_pt_* nao os tem.
lv_obj_t *ui_icon(lv_obj_t *parent, const char *symbol, uint32_t color);

// Botao "< Voltar" das paginas do Settings e das filhas (Plano C). A seta e um
// LV_SYMBOL_* (fonte de fabrica) e o texto usa font_pt_18.
lv_obj_t *ui_back_button(lv_obj_t *parent);

#endif // UI_THEME_H
