#include "page_message.h"

#include <cstring>

#include "../../config.h"
#include "theme.h"

static char s_title[64];
static char s_sub[128];
static lv_obj_t *s_subLabel = nullptr;

void page_message_set(const char *title, const char *sub) {
    strlcpy(s_title, title ? title : "", sizeof(s_title));
    strlcpy(s_sub, sub ? sub : "", sizeof(s_sub));
}

void page_message_build(lv_obj_t *scr) {
    lv_obj_t *t = ui_label(scr, s_title, &font_pt_28, COL_TEXT);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, -30);

    s_subLabel = ui_label(scr, s_sub, &font_pt_18, COL_DIM);
    lv_obj_set_width(s_subLabel, SCREEN_WIDTH - 40);
    lv_label_set_long_mode(s_subLabel, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_subLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_subLabel, LV_ALIGN_CENTER, 0, 24);
}

void page_message_update(const char *sub) {
    strlcpy(s_sub, sub ? sub : "", sizeof(s_sub));
    if (s_subLabel) lv_label_set_text(s_subLabel, s_sub);
}

void page_message_leave() { s_subLabel = nullptr; }
