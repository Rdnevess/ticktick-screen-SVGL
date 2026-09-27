/**
 * lv_conf.h do BRING-UP (Guition JC4832W535).
 *
 * Deliberadamente separado do lv_conf.h do firmware: este sketch e um
 * diagnostico descartavel e nao deve quebrar quando o firmware passar a
 * declarar fontes geradas. Usa as Montserrat de fabrica.
 *
 * Compile com -DLV_CONF_INCLUDE_SIMPLE e -I<esta pasta> (ver build.sh).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Guarda obrigatoria: lv_conf_internal.h tambem e incluido durante a montagem
   dos .S do LVGL. Sem ela o assembler recebe os typedef de stdint.h e falha com
   "unknown opcode or format name 'typedef'". */
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

#define LV_COLOR_DEPTH 16
/* Sem troca de bytes: o flush copia RGB565 direto (ver docs/REFERENCIA-HARDWARE-LVGL.md). */

#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN
#define LV_MEM_SIZE            (48 * 1024U)

#define LV_USE_OS LV_OS_NONE
#define LV_USE_DRAW_SW 1

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LABEL 1

#endif /*LV_CONF_H*/
