#include "page_pair.h"

#include <cstring>

#include "../../config.h"
#include "../app/app.h"
#include "../app/pairing.h"
#include "../platform/clock.h"
#include "../platform/wifi_manager.h"
#include "i18n.h"
#include "onboarding_web.h"
#include "theme.h"

static lv_obj_t *s_status = nullptr;
static lv_obj_t *s_spin = nullptr;
static lv_obj_t *s_reopen = nullptr;
static int s_shown = -1; // PairState exibido; -1 forca redesenho, -2 = portal fechado
static int64_t s_deadlineMs = 0;
static bool s_reopenPending = false;
static bool s_backPending = false;
static char s_reason[96];

static void on_back(lv_event_t *e) {
    (void)e;
    s_backPending = true;
}

void page_pair_set_reason(const char *reason) {
    strlcpy(s_reason, reason ? reason : "", sizeof(s_reason));
}

static void set_status(const char *txt, uint32_t color) {
    lv_label_set_text(s_status, txt);
    lv_obj_set_style_text_color(s_status, theme_color(color), 0);
}

static void show_state(PairState st) {
    char buf[128];
    switch (st) {
        case PairState::Validating:
            set_status(TRS("Validando com o TickTick…", "Checking with TickTick…"), COL_TEXT);
            break;
        case PairState::Accepted:
            set_status(TRS("Pareado!", "Paired!"), COL_OK);
            break;
        case PairState::Rejected:
            snprintf(buf, sizeof(buf),
                     TRS("Recusado (HTTP %d). Gere um blob novo.", "Refused (HTTP %d). Generate a new blob."),
                     pairing_http());
            set_status(buf, COL_LATE);
            break;
        default:
            set_status(TRS("Aguardando o blob…", "Waiting for the blob…"), COL_DIM);
            break;
    }
}

static void start_portal() {
    pairing_reset();
    portal_start();
    s_deadlineMs = clock_uptime_ms() + (int64_t)PORTAL_TIMEOUT_MIN * 60 * 1000;
}

static void on_reopen(lv_event_t *e) {
    (void)e;
    s_reopenPending = true;
}

void page_pair_build(lv_obj_t *scr) {
    lv_obj_t *title = ui_label(scr, TRS("Parear com o TickTick", "Pair with TickTick"),
                               &font_pt_24, COL_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    if (app_is_child()) { // "Parear de novo" pelo Settings: as credenciais atuais ficam
        lv_obj_t *back = ui_back_button(scr);
        lv_obj_align(back, LV_ALIGN_TOP_LEFT, 8, 6);
        lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
    }

    int y = 48;
    if (s_reason[0]) {
        lv_obj_t *r = ui_label(scr, s_reason, &font_pt_14, COL_LATE);
        lv_obj_align(r, LV_ALIGN_TOP_MID, 0, y);
        y += 22;
    }

    lv_obj_t *hint = ui_label(scr, TRS("No PC ou celular, na mesma rede WiFi, abra:",
                                       "On a PC or phone on the same WiFi, open:"),
                              &font_pt_14, COL_DIM);
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, y);

    lv_obj_t *url = ui_label(scr, "http://" MDNS_NAME ".local", &font_pt_28, COL_ACCENT);
    lv_obj_align(url, LV_ALIGN_TOP_MID, 0, y + 24);

    char ipLine[64];
    snprintf(ipLine, sizeof(ipLine), TRS("ou http://%s", "or http://%s"), wifi().getIP().c_str());
    lv_obj_t *ip = ui_label(scr, ipLine, &font_pt_18, COL_TEXT);
    lv_obj_align(ip, LV_ALIGN_TOP_MID, 0, y + 64);

    lv_obj_t *how = ui_label(scr, TRS("O blob vem de python helper/pair.py, no PC.",
                                      "The blob comes from python helper/pair.py, on the PC."),
                             &font_pt_14, COL_DIM);
    lv_obj_align(how, LV_ALIGN_TOP_MID, 0, y + 96);

    s_spin = lv_spinner_create(scr);
    lv_spinner_set_anim_params(s_spin, 1200, 70);
    lv_obj_set_size(s_spin, 32, 32);
    lv_obj_align(s_spin, LV_ALIGN_BOTTOM_LEFT, 20, -18);
    lv_obj_set_style_arc_color(s_spin, theme_color(COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_spin, theme_color(COL_ACCENT), LV_PART_INDICATOR);

    s_status = ui_label(scr, "", &font_pt_18, COL_DIM);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_LEFT, 64, -24);

    s_reopen = ui_button(scr, TRS("Reabrir portal", "Reopen portal"), 200, 44, COL_ACCENT);
    lv_obj_align(s_reopen, LV_ALIGN_BOTTOM_RIGHT, -14, -12);
    lv_obj_add_flag(s_reopen, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_reopen, on_reopen, LV_EVENT_CLICKED, nullptr);

    start_portal();
    s_shown = -1;
}

void page_pair_tick() {
    if (!s_status) return;

    if (s_backPending) {
        s_backPending = false;
        app_back();
        return;
    }
    if (s_reopenPending) {
        s_reopenPending = false;
        start_portal();
        lv_obj_add_flag(s_reopen, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_spin, LV_OBJ_FLAG_HIDDEN);
        s_shown = -1;
    }

    if (portal_running()) {
        portal_tick();
        const bool busy = pairing_state() == PairState::Validating ||
                          pairing_state() == PairState::Accepted;
        if (!busy && clock_uptime_ms() > s_deadlineMs) {
            portal_stop();
            lv_obj_add_flag(s_spin, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(s_reopen, LV_OBJ_FLAG_HIDDEN);
            set_status(TRS("Portal fechado por segurança.", "Portal closed for safety."), COL_DIM);
            s_shown = -2; // mantem a mensagem ate reabrir
            return;
        }
    }
    if (s_shown == -2) return;

    const PairState st = pairing_state();
    if ((int)st != s_shown) {
        s_shown = (int)st;
        show_state(st);
    }
    // So descarta o motivo do AuthFailed quando o pareamento deu certo: a
    // pagina pode ser reconstruida (app_go) com o pareamento ainda em curso,
    // e leave() nao e mais o lugar certo para limpar isso (revisao final).
    if (st == PairState::Accepted) s_reason[0] = '\0';
}

void page_pair_leave() {
    portal_stop();
    s_status = s_spin = s_reopen = nullptr;
    s_reopenPending = false;
    s_backPending = false;
}
