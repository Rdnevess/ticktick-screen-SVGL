#include "theme.h"

#include "../core/task.h"
#include "i18n.h"

lv_color_t theme_color(uint32_t hex) { return lv_color_hex(hex); }

uint32_t prio_color(int priority) {
    switch (priority) {
        case PRIO_HIGH: return COL_LATE;
        case PRIO_MED:  return COL_MED;
        case PRIO_LOW:  return COL_ACCENT;
        default:        return COL_DIM;
    }
}

const char *prio_label(int priority) {
    switch (priority) {
        case PRIO_HIGH: return TRS("ALTA", "HIGH");
        case PRIO_MED:  return TRS("MÉDIA", "MEDIUM");
        case PRIO_LOW:  return TRS("BAIXA", "LOW");
        default:        return ""; // omitida, nao escrita
    }
}

lv_obj_t *ui_panel(lv_obj_t *parent, int w, int h) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_color(p, theme_color(COL_SURFACE), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_radius(p, 10, 0);
    lv_obj_set_style_pad_all(p, 10, 0);
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                   uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, theme_color(color), 0);
    return l;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, int w, int h,
                    uint32_t bg) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, theme_color(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_t *l = ui_label(b, text, &font_pt_24, COL_TEXT);
    lv_obj_center(l);
    return b;
}

lv_obj_t *ui_icon(lv_obj_t *parent, const char *symbol, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(l, theme_color(color), 0);
    return l;
}

lv_obj_t *ui_back_button(lv_obj_t *parent) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 120, 40);
    lv_obj_set_style_bg_color(b, theme_color(COL_SURFACE), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(b, 6, 0);
    ui_icon(b, LV_SYMBOL_LEFT, COL_TEXT);
    ui_label(b, TRS("Voltar", "Back"), &font_pt_18, COL_TEXT);
    return b;
}
