/**
 * lv_conf.h do TickTick Screen (Guition JC4832W535, LVGL 9.2.2).
 *
 * Compile com -DLV_CONF_INCLUDE_SIMPLE e -I<pasta do sketch> (ver build.sh),
 * inclusive em compiler.S.extra_flags.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Guarda obrigatoria: lv_conf_internal.h tambem e incluido ao montar os .S do
   LVGL. Sem ela o assembler recebe os typedef de stdint.h e falha com
   "unknown opcode or format name 'typedef'". */
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

#define LV_COLOR_DEPTH 16
/* Sem troca de bytes: o flush copia RGB565 direto. */

#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN
/* Pool de objetos/estilos na PSRAM. A lista de Hoje com 64 linhas passa dos
   96 KB que cabiam na RAM interna, e a interna fica para TLS e WiFi. O buffer
   de render 480x320x2 e alocado a parte, tambem na PSRAM. */
#define LV_MEM_SIZE            (256 * 1024U)
#define LV_MEM_POOL_INCLUDE    <esp_heap_caps.h>
#define LV_MEM_POOL_ALLOC(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

#define LV_USE_OS LV_OS_NONE
/* tick vem de lv_tick_set_cb(millis) no sketch */
#define LV_USE_DRAW_SW 1

/* ---- Fontes ----
   As nossas (font_pt_*) tem Latin-1 e sao usadas para TODO texto.
   A montserrat_18 de fabrica fica habilitada por um motivo especifico: os
   LV_SYMBOL_* (engrenagem, wifi, etc.) sao glifos que vivem nas fontes
   internas, na faixa 0xF000+. Nossas fontes nao os tem, entao labels de
   icone usam lv_font_montserrat_18 e labels de texto usam font_pt_*. */
#define LV_FONT_MONTSERRAT_18 1

#define LV_FONT_CUSTOM_DECLARE \
    LV_FONT_DECLARE(font_pt_12) \
    LV_FONT_DECLARE(font_pt_14) \
    LV_FONT_DECLARE(font_pt_18) \
    LV_FONT_DECLARE(font_pt_24) \
    LV_FONT_DECLARE(font_pt_28) \
    LV_FONT_DECLARE(font_pt_36) \
    LV_FONT_DECLARE(font_pt_48) \
    LV_FONT_DECLARE(font_clock_120)

#define LV_FONT_DEFAULT &font_pt_18

/* ---- Widgets usados no Plano A e B ---- */
#define LV_USE_LABEL    1
#define LV_USE_BUTTON   1
#define LV_USE_BAR      1
#define LV_USE_LIST     1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1
#define LV_USE_IMAGE    1
#define LV_USE_SPINNER  1
#define LV_USE_SWITCH   1
#define LV_USE_CHECKBOX 1

#endif /*LV_CONF_H*/
