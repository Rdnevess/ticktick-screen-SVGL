#include "page_lists.h"

#include <cstdio>
#include <cstring>

#include "../../config.h"
#include "../app/app.h"
#include "../app/sync.h"
#include "../net/net_worker.h"
#include "../platform/settings.h"
#include "i18n.h"
#include "theme.h"

constexpr int MAX_ROWS = MAX_PROJECTS + 1; // + Entrada

static ProjectList s_pl; // copia: o buffer do worker volta logo depois do callback
static bool s_arrived = false;
static bool s_ok = false;
static int s_http = 0;
static bool s_savePending = false;
static bool s_retryPending = false;
static bool s_backPending = false;

static lv_obj_t *s_scr = nullptr;
static lv_obj_t *s_body = nullptr;
static lv_obj_t *s_count = nullptr;
static lv_obj_t *s_save = nullptr;
static lv_obj_t *s_boxes[MAX_ROWS];
static const char *s_ids[MAX_ROWS];
static int s_rows = 0;

static void on_projects(bool ok, int http, const ProjectList *pl) {
    s_arrived = true;
    s_ok = ok;
    s_http = http;
    if (ok) s_pl = *pl;
}

static int checked_count() {
    int n = 0;
    for (int i = 0; i < s_rows; i++)
        if (lv_obj_has_state(s_boxes[i], LV_STATE_CHECKED)) n++;
    return n;
}

// "3 marcadas · 4 requisições por ciclo": o custo e sentido na hora (spec 6.5).
static void update_count() {
    const int n = checked_count();
    lv_label_set_text_fmt(s_count, TRS("%d marcada(s) · %d requisições por ciclo",
                                       "%d selected · %d requests per cycle"),
                          n, n + 1); // + o GET /project do inicio do ciclo
    lv_obj_set_style_text_color(s_count, theme_color(n > 6 ? COL_MED : COL_DIM), 0);
    if (n == 0) lv_obj_add_state(s_save, LV_STATE_DISABLED);
    else lv_obj_remove_state(s_save, LV_STATE_DISABLED);
}

static void on_check(lv_event_t *e) {
    lv_obj_t *cb = (lv_obj_t *)lv_event_get_target(e);
    if (lv_obj_has_state(cb, LV_STATE_CHECKED) && checked_count() > MAX_LISTS) {
        lv_obj_remove_state(cb, LV_STATE_CHECKED); // teto: cada lista e um GET por ciclo
    }
    update_count();
}

static void on_save(lv_event_t *e) {
    (void)e;
    s_savePending = true;
}

static void on_retry(lv_event_t *e) {
    (void)e;
    s_retryPending = true;
}

static void on_back(lv_event_t *e) {
    (void)e;
    s_backPending = true;
}

static bool was_selected(const char *id) {
    const Settings &s = settings();
    for (int i = 0; i < s.listCount; i++)
        if (std::strcmp(s.lists[i], id) == 0) return true;
    return false;
}

static void add_box(const char *id, const char *name) {
    if (s_rows >= MAX_ROWS) return;
    lv_obj_t *cb = lv_checkbox_create(s_body);
    lv_checkbox_set_text(cb, name);
    lv_obj_set_style_text_font(cb, &font_pt_18, 0);
    lv_obj_set_style_text_color(cb, theme_color(COL_TEXT), 0);
    // O "check" do marcador e LV_SYMBOL_OK, que so a fonte de fabrica tem. O
    // seletor precisa do estado CHECKED para vencer o estilo do tema.
    lv_obj_set_style_text_font(cb, &lv_font_montserrat_18, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_pad_ver(cb, 8, 0);
    if (was_selected(id)) lv_obj_add_state(cb, LV_STATE_CHECKED);
    lv_obj_add_event_cb(cb, on_check, LV_EVENT_VALUE_CHANGED, nullptr);
    s_boxes[s_rows] = cb;
    s_ids[s_rows] = id;
    s_rows++;
}

static void show_loading() {
    lv_obj_clean(s_body);
    s_rows = 0;
    lv_obj_t *sp = lv_spinner_create(s_body);
    lv_spinner_set_anim_params(sp, 1200, 70);
    lv_obj_set_size(sp, 40, 40);
    lv_obj_set_style_arc_color(sp, theme_color(COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, theme_color(COL_ACCENT), LV_PART_INDICATOR);
    lv_label_set_text(s_count, TRS("Buscando suas listas…", "Fetching your lists…"));
    lv_obj_add_state(s_save, LV_STATE_DISABLED);
    s_arrived = false;
    if (!sync_fetch_projects(on_projects)) {
        s_arrived = true; // ja havia pedido em curso: trata como falha e oferece tentar de novo
        s_ok = false;
        s_http = 0;
    }
}

static void show_error() {
    lv_obj_clean(s_body);
    s_rows = 0;
    char m[96];
    snprintf(m, sizeof(m), TRS("Não deu para buscar as listas (%d).", "Couldn't fetch the lists (%d)."), s_http);
    ui_label(s_body, m, &font_pt_18, COL_LATE);
    lv_obj_t *retry = ui_button(s_body, TRS("Tentar de novo", "Try again"), 220, 48, COL_SURFACE);
    lv_obj_add_event_cb(retry, on_retry, LV_EVENT_CLICKED, nullptr);
    lv_label_set_text(s_count, "");
}

static void show_lists() {
    lv_obj_clean(s_body);
    s_rows = 0;
#if INBOX_SUPPORTED
    add_box("inbox", TRS("Entrada", "Inbox"));
#endif
    for (int i = 0; i < s_pl.count; i++) add_box(s_pl.ids[i], s_pl.names[i]);
    update_count();
}

static void save() {
    Settings &s = settings();
    s.listCount = 0;
    for (int i = 0; i < s_rows && s.listCount < MAX_LISTS; i++)
        if (lv_obj_has_state(s_boxes[i], LV_STATE_CHECKED)) task_set_id(s.lists[s.listCount++], s_ids[i]);
    settings_save();
    net_set_lists(s.lists, s.listCount);
    sync_refresh_now(); // o primeiro ciclo ja com as listas novas
    app_back(); // volta ao Settings, ou segue o fluxo no primeiro boot
}

void page_lists_build(lv_obj_t *scr) {
    s_scr = scr;
    lv_obj_t *title = ui_label(scr, TRS("Listas do dia", "Lists for the day"), &font_pt_24, COL_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

    if (app_is_child()) {
        lv_obj_t *back = ui_back_button(scr);
        lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -8, 6);
        lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t *sub = ui_label(scr, TRS("Marque as listas que entram no dia.",
                                      "Pick the lists that make up your day."),
                             &font_pt_14, COL_DIM);
    lv_obj_align(sub, LV_ALIGN_TOP_LEFT, 14, 44);

    s_body = lv_obj_create(scr);
    lv_obj_set_size(s_body, SCREEN_WIDTH - 20, 196);
    lv_obj_align(s_body, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_set_style_bg_color(s_body, theme_color(COL_SURFACE), 0);
    lv_obj_set_style_border_width(s_body, 0, 0);
    lv_obj_set_style_radius(s_body, 10, 0);
    lv_obj_set_flex_flow(s_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_body, LV_DIR_VER);

    s_count = ui_label(scr, "", &font_pt_14, COL_DIM);
    lv_obj_align(s_count, LV_ALIGN_BOTTOM_LEFT, 14, -20);

    s_save = ui_button(scr, TRS("Salvar", "Save"), 150, 44, COL_ACCENT);
    lv_obj_align(s_save, LV_ALIGN_BOTTOM_RIGHT, -10, -8);
    lv_obj_add_event_cb(s_save, on_save, LV_EVENT_CLICKED, nullptr);

    show_loading();
}

void page_lists_tick() {
    if (!s_scr) return;
    if (s_backPending) {
        s_backPending = false;
        app_back();
        return;
    }
    if (s_retryPending) {
        s_retryPending = false;
        show_loading();
        return;
    }
    if (s_arrived) {
        s_arrived = false;
        if (s_ok) show_lists();
        else show_error();
        return;
    }
    if (s_savePending) {
        s_savePending = false;
        if (checked_count() > 0) save();
    }
}

void page_lists_leave() {
    s_scr = s_body = s_count = s_save = nullptr;
    s_rows = 0;
    s_savePending = s_retryPending = false;
    s_backPending = false;
}
