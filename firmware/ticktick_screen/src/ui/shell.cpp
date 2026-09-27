#include "shell.h"

#include <cstring>

#include "../../config.h"
#include "../core/clock_format.h"
#include "../platform/clock.h"
#include "i18n.h"
#include "screen_focus.h"
#include "screen_status.h"
#include "screen_today.h"
#include "theme.h"

// Logo do TickTick no centro do header: so existe quando gerado localmente por
// tools/logo2c.py (marca de terceiros, fora do git). Sem ele, header sem logo.
#if __has_include("../assets/logo_ticktick.h")
#include "../assets/logo_ticktick.h"
#define SHELL_HAS_LOGO 1
#endif

static int s_screen = SCR_FOCUS;
static bool s_rebuild = false;

static lv_obj_t *s_content = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_counter = nullptr;
static lv_obj_t *s_clock = nullptr;
static lv_obj_t *s_bar = nullptr;
static lv_obj_t *s_dots[SCR_COUNT] = {nullptr, nullptr, nullptr};
static lv_obj_t *s_staleBadge = nullptr;
static lv_obj_t *s_toast = nullptr;

static bool s_stale = false;
static int s_done = 0;
static int s_total = 0;
static char s_override[16];        // texto do pomodoro no lugar do contador; "" = nenhum
static uint32_t s_overrideColor = COL_ACCENT;
static int64_t s_clockMinute = -2; // minuto local mostrado; -2 forca redesenho
static int64_t s_toastUntilMs = 0;

static void (*s_onRefresh)() = nullptr;
static void (*s_onGear)() = nullptr;
static void (*s_onClock)() = nullptr;
static void (*s_onCounter)() = nullptr;

static const char *screen_title(int s) {
    switch (s) {
        case SCR_FOCUS: return TRS("FOCO", "FOCUS");
        case SCR_TODAY: return TRS("HOJE", "TODAY");
        default:        return TRS("STATUS", "STATUS");
    }
}

static void on_gesture(lv_event_t *e) {
    (void)e;
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_LEFT) shell_next();
    else if (dir == LV_DIR_RIGHT) shell_prev();
}

static void on_bar_click(lv_event_t *e) {
    (void)e;
    if (s_onRefresh) s_onRefresh(); // refresh imediato (spec 7.1)
    shell_set_refresh_progress(1.0f);
}

static void on_gear(lv_event_t *e) {
    (void)e;
    if (s_onGear) s_onGear();
}

static void on_clock(lv_event_t *e) {
    (void)e;
    if (s_onClock) s_onClock();
}

static void on_counter(lv_event_t *e) {
    (void)e;
    if (s_onCounter) s_onCounter();
}

static void update_dots() {
    for (int i = 0; i < SCR_COUNT; i++) {
        bool active = (i == s_screen);
        lv_obj_set_size(s_dots[i], active ? 18 : 8, 8);
        lv_obj_set_style_bg_color(s_dots[i], theme_color(active ? COL_ACCENT : COL_DIM), 0);
    }
}

static void apply_counter() {
    if (!s_counter) return;
    if (s_override[0]) {
        lv_label_set_text(s_counter, s_override);
        lv_obj_set_style_text_color(s_counter, theme_color(s_overrideColor), 0);
    } else {
        lv_label_set_text_fmt(s_counter, "%d / %d", s_done, s_total);
        lv_obj_set_style_text_color(s_counter, theme_color(COL_DIM), 0);
    }
}

// Reescreve o horario so quando o minuto muda (adendo 2.1).
static void update_clock() {
    if (!s_clock) return;
    const bool has = clock_has_time();
    const int64_t now = clock_now_local();
    const int64_t minute = has ? now / 60 : -1;
    if (minute == s_clockMinute) return;
    s_clockMinute = minute;
    char buf[8];
    clock_hhmm(now, has, buf, sizeof(buf));
    lv_label_set_text(s_clock, buf);
    if (s_screen == SCR_STATUS) s_rebuild = true; // a linha "Hora" do Status
}

static void build_content() {
    lv_obj_clean(s_content);
    lv_label_set_text(s_title, screen_title(s_screen));
    switch (s_screen) {
        case SCR_FOCUS: focus_build(s_content); break;
        case SCR_TODAY: today_build(s_content); break;
        default:        status_build(s_content); break;
    }
    update_dots();
}

// Rotulo do header que responde a toque, com area extra de clique: alvo
// pequeno numa tela capacitiva de 3,5" e frustracao garantida.
static lv_obj_t *tappable_label(lv_obj_t *parent, const char *text, lv_event_cb_t cb) {
    lv_obj_t *l = ui_label(parent, text, &font_pt_18, COL_DIM);
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(l, 12);
    lv_obj_add_event_cb(l, cb, LV_EVENT_CLICKED, nullptr);
    return l;
}

void shell_build() {
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, theme_color(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_clean nao remove os eventos da propria tela: sem isto, cada
    // shell_build() somaria um callback e o swipe pularia telas.
    lv_obj_remove_event_cb(scr, on_gesture);
    lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, nullptr);

    // ---- Header (40 px) ----
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    s_title = ui_label(header, screen_title(s_screen), &font_pt_18, COL_TEXT);
    lv_obj_align(s_title, LV_ALIGN_LEFT_MID, 12, 0);

    // Contador do dia; o pomodoro ocupa este lugar enquanto roda (spec 7.6).
    s_counter = tappable_label(header, "", on_counter);
    lv_obj_align(s_counter, LV_ALIGN_LEFT_MID, 110, 0);
    apply_counter();

#ifdef SHELL_HAS_LOGO
    // So o icone (28 px) cabe entre o contador e o selo "desatualizado".
    lv_obj_t *logo = lv_image_create(header);
    lv_image_set_src(logo, &logo_ticktick);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, 0);
    lv_obj_remove_flag(logo, LV_OBJ_FLAG_CLICKABLE);
#endif

    // Engrenagem: 58x40 + 12 px de area extra de clique (spec 7.1).
    lv_obj_t *gear = lv_button_create(header);
    lv_obj_set_size(gear, 58, HEADER_H);
    lv_obj_align(gear, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(gear, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gear, 0, 0);
    lv_obj_set_ext_click_area(gear, 12);
    lv_obj_add_event_cb(gear, on_gear, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *gearIcon = ui_icon(gear, LV_SYMBOL_SETTINGS, COL_DIM);
    lv_obj_center(gearIcon);

    // Horario logo a esquerda da engrenagem (adendo 2.1).
    s_clock = tappable_label(header, "--:--", on_clock);
    lv_obj_align(s_clock, LV_ALIGN_RIGHT_MID, -64, 0);
    s_clockMinute = -2;
    update_clock();

    // Selo "desatualizado" a esquerda do horario.
    s_staleBadge = ui_label(header, TRS("desatualizado", "stale"), &font_pt_14, COL_MED);
    lv_obj_align(s_staleBadge, LV_ALIGN_RIGHT_MID, -126, 0);
    if (!s_stale) lv_obj_add_flag(s_staleBadge, LV_OBJ_FLAG_HIDDEN);

    // ---- Barra de refresh (4 px), tocavel ----
    s_bar = lv_obj_create(scr);
    lv_obj_set_size(s_bar, SCREEN_WIDTH, REFRESHBAR_H);
    lv_obj_set_pos(s_bar, 0, HEADER_H);
    lv_obj_set_style_bg_color(s_bar, theme_color(COL_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bar, 0, 0);
    lv_obj_set_style_radius(s_bar, 0, 0);
    lv_obj_add_flag(s_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_bar, 10); // 4 px de alvo seria impossivel
    lv_obj_add_event_cb(s_bar, on_bar_click, LV_EVENT_CLICKED, nullptr);
    shell_set_refresh_progress(0.0f);

    // ---- Conteudo (252 px) ----
    s_content = lv_obj_create(scr);
    lv_obj_set_size(s_content, SCREEN_WIDTH, CONTENT_H);
    lv_obj_set_pos(s_content, 0, HEADER_H + REFRESHBAR_H);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_style_pad_all(s_content, 0, 0);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Pontinhos (24 px) ----
    lv_obj_t *dots = lv_obj_create(scr);
    lv_obj_set_size(dots, SCREEN_WIDTH, DOTS_H);
    lv_obj_set_pos(dots, 0, HEADER_H + REFRESHBAR_H + CONTENT_H);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots, 0, 0);
    lv_obj_set_style_pad_all(dots, 0, 0);
    lv_obj_remove_flag(dots, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(dots, 8, 0);

    for (int i = 0; i < SCR_COUNT; i++) {
        s_dots[i] = lv_obj_create(dots);
        lv_obj_set_style_border_width(s_dots[i], 0, 0);
        lv_obj_set_style_radius(s_dots[i], 4, 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        lv_obj_remove_flag(s_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // ---- Aviso passageiro (acima dos pontinhos, por cima do conteudo) ----
    s_toast = ui_label(scr, "", &font_pt_18, COL_TEXT);
    lv_obj_set_style_bg_color(s_toast, theme_color(COL_SURFACE), 0);
    lv_obj_set_style_bg_opa(s_toast, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_toast, 8, 0);
    lv_obj_set_style_pad_hor(s_toast, 14, 0);
    lv_obj_set_style_pad_ver(s_toast, 8, 0);
    lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);

    build_content();
}

void shell_go(int screen) {
    if (screen < 0 || screen >= SCR_COUNT || screen == s_screen) return;
    s_screen = screen;
    today_close_sheet(); // a folha e do Hoje
    s_rebuild = true;    // nunca reconstruir dentro do callback de gesto
}

void shell_next() { shell_go((s_screen + 1) % SCR_COUNT); }
void shell_prev() { shell_go((s_screen + SCR_COUNT - 1) % SCR_COUNT); }
int shell_current() { return s_screen; }

void shell_set_refresh_progress(float frac) {
    if (!s_bar) return;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    int w = (int)(SCREEN_WIDTH * frac);
    lv_obj_set_width(s_bar, w > 0 ? w : 1);
}

void shell_request_rebuild() { s_rebuild = true; }

void shell_leave() {
    lv_obj_remove_event_cb(lv_screen_active(), on_gesture);
    s_content = s_title = s_counter = s_clock = s_bar = nullptr;
    s_staleBadge = nullptr;
    s_toast = nullptr;
    s_toastUntilMs = 0;
    today_forget();
    for (int i = 0; i < SCR_COUNT; i++) s_dots[i] = nullptr;
    s_rebuild = false;
}

void shell_tick() {
    if (s_toast && s_toastUntilMs != 0 && clock_uptime_ms() > s_toastUntilMs) {
        s_toastUntilMs = 0;
        lv_obj_add_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    }
    update_clock();
    if (!s_rebuild || !s_content) return; // fora da tela principal, nada a fazer
    if (s_screen == SCR_TODAY && today_busy()) return; // espera a rolagem (spec 7.1)
    s_rebuild = false;
    build_content();
}

void shell_set_refresh_handler(void (*fn)()) { s_onRefresh = fn; }
void shell_set_gear_handler(void (*fn)()) { s_onGear = fn; }
void shell_set_clock_handler(void (*fn)()) { s_onClock = fn; }
void shell_set_counter_handler(void (*fn)()) { s_onCounter = fn; }

void shell_set_stale(bool stale) {
    s_stale = stale;
    if (!s_staleBadge) return;
    if (stale) lv_obj_remove_flag(s_staleBadge, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_staleBadge, LV_OBJ_FLAG_HIDDEN);
}

void shell_set_counter(int done, int total) {
    s_done = done;
    s_total = total;
    apply_counter();
}

void shell_set_counter_override(const char *text, uint32_t color) {
    strlcpy(s_override, text ? text : "", sizeof(s_override));
    s_overrideColor = color;
    apply_counter();
}

void shell_toast(const char *msg) {
    if (!s_toast) return;
    lv_label_set_text(s_toast, msg);
    lv_obj_remove_flag(s_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_toast, LV_ALIGN_BOTTOM_MID, 0, -(DOTS_H + 6));
    s_toastUntilMs = clock_uptime_ms() + 3000;
}
