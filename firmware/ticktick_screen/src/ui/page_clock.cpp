#include "page_clock.h"

#include "../../config.h"
#include "../app/app.h"
#include "../core/clock_format.h"
#include "../platform/clock.h"
#include "i18n.h"
#include "theme.h"

static lv_obj_t *s_time = nullptr;
static lv_obj_t *s_date = nullptr;
static int64_t s_minute = -2; // minuto mostrado; -2 forca redesenho
static bool s_backPending = false;

static const int TIME_Y = -24; // posicao base; clock_drift soma o deslocamento
static const int DATE_Y = 76;

static void on_tap(lv_event_t *e) {
    (void)e;
    s_backPending = true; // a troca de pagina acontece no tick
}

static void refresh() {
    const bool has = clock_has_time();
    const int64_t now = clock_now_local();
    const int64_t minute = has ? now / 60 : -1;
    if (minute == s_minute) return;
    s_minute = minute;
    char buf[48];
    clock_hhmm(now, has, buf, sizeof(buf));
    lv_label_set_text(s_time, buf);
    if (has) clock_long_date(now, lang_is_en(), buf, sizeof(buf));
    else buf[0] = '\0';
    lv_label_set_text(s_date, buf);
    // Hora e data andam juntas alguns pixels a cada minuto (contra marcacao).
    int dx = 0, dy = 0;
    clock_drift(minute, &dx, &dy);
    lv_obj_align(s_time, LV_ALIGN_CENTER, dx, TIME_Y + dy);
    lv_obj_align(s_date, LV_ALIGN_CENTER, dx, DATE_Y + dy);
}

void page_clock_build(lv_obj_t *scr) {
    // Area de toque do tamanho da tela, filha da tela: some com ela no
    // lv_obj_clean. (Um callback na propria tela sobreviveria a troca.)
    lv_obj_t *area = lv_obj_create(scr);
    lv_obj_set_size(area, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(area, 0, 0);
    lv_obj_set_style_bg_opa(area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(area, 0, 0);
    lv_obj_set_style_pad_all(area, 0, 0);
    lv_obj_remove_flag(area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(area, on_tap, LV_EVENT_CLICKED, nullptr);

    s_time = ui_label(area, "--:--", &font_clock_120, COL_TEXT);
        s_date = ui_label(area, "", &font_pt_24, COL_DIM);
    
    s_minute = -2;
    s_backPending = false;
    refresh();
}

void page_clock_tick() {
    if (!s_time) return;
    if (s_backPending) {
        s_backPending = false;
        app_go(Page::Main);
        return;
    }
    refresh();
}

void page_clock_leave() {
    s_time = s_date = nullptr;
    s_backPending = false;
}
