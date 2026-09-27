// Camada de display: QSPI + Canvas + LVGL + touch.
//
// Encapsula a receita validada (docs/REFERENCIA-HARDWARE-LVGL.md): Canvas com
// rotation=0 e rotacao manual 270 CW no flush. Trocar isso da cores erradas.
#ifndef PLATFORM_DISPLAY_H
#define PLATFORM_DISPLAY_H

#include <cstdint>

// Inicia display, backlight, LVGL (buffer full-screen na PSRAM) e touch.
// Devolve false em falha fatal (QSPI ou PSRAM).
bool display_begin();

// Brilho do backlight: 0 baixo, 1 medio, 2 alto (outro valor vira medio).
void display_set_brightness(int level);

// Chamar no loop(). Roda lv_task_handler.
void display_tick();

// Buffer de render da LVGL (modo FULL: o ultimo quadro inteiro, 480x320
// RGB565, linha a linha). nullptr antes de display_begin().
const uint16_t *display_framebuffer();

#endif // PLATFORM_DISPLAY_H
