#include "screen_today.h"

#include <cstdio>
#include <cstring>

#include "../../config.h"
#include "../app/pomo.h"
#include "../app/sync.h"
#include "../platform/clock.h"
#include "i18n.h"
#include "shell.h"
#include "task_view.h"
#include "theme.h"

// Linhas e folha guardam a tarefa pelos ids (ver screen_focus.cpp).
struct RowRef {
    char projectId[TASK_ID_BYTES + 1];
    char taskId[TASK_ID_BYTES + 1];
};
static RowRef s_rows[TASK_LIST_MAX];
static RowRef s_sheetRef;
static lv_obj_t *s_sheet = nullptr; // fundo escurecido no layer_top
static bool s_scrolling = false;
static int64_t s_scrollStartMs = 0;
static int32_t s_scrollY = 0;

static const Task *find(const RowRef &r) {
    int n = 0;
    const Task *v = sync_visible(&n);
    for (int i = 0; i < n; i++)
        if (std::strcmp(v[i].id, r.taskId) == 0 && std::strcmp(v[i].projectId, r.projectId) == 0)
            return &v[i];
    return nullptr;
}

// ---- Folha de acao (spec 7.3) ----

void today_close_sheet() {
    if (s_sheet) lv_obj_delete_async(s_sheet); // pode ser chamado de dentro do evento dela
    s_sheet = nullptr;
    s_scrolling = false;
}

void today_forget() {
    s_sheet = nullptr;
    s_scrolling = false;
}

static void on_sheet_close(lv_event_t *e) {
    (void)e;
    today_close_sheet();
}

static void on_sheet_done(lv_event_t *e) {
    (void)e;
    const Task *t = find(s_sheetRef);
    today_close_sheet();
    if (t) sync_complete(*t);
}

static void on_sheet_pin(lv_event_t *e) {
    (void)e;
    const Task *t = find(s_sheetRef);
    today_close_sheet();
    if (t && sync_toggle_pin(*t) == PinResult::Full)
        shell_toast(TRS("Só dá para fixar duas. Solte uma antes.",
                        "Only two can be pinned. Unpin one first."));
}

static void on_sheet_pomo(lv_event_t *e) {
    (void)e;
    const Task *t = find(s_sheetRef);
    today_close_sheet();
    if (t) pomo_app_request_start(*t);
}

static void open_sheet(const Task &t) {
    today_close_sheet();
    task_set_id(s_sheetRef.projectId, t.projectId);
    task_set_id(s_sheetRef.taskId, t.id);

    s_sheet = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_sheet, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_sheet, theme_color(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_sheet, LV_OPA_60, 0);
    lv_obj_set_style_border_width(s_sheet, 0, 0);
    lv_obj_set_style_radius(s_sheet, 0, 0);
    lv_obj_set_style_pad_all(s_sheet, 0, 0);
    lv_obj_remove_flag(s_sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_sheet, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_sheet, on_sheet_close, LV_EVENT_CLICKED, nullptr); // toque fora fecha

    lv_obj_t *panel = ui_panel(s_sheet, SCREEN_WIDTH, 150);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE); // absorve o toque: nao fecha

    lv_obj_t *title = ui_label(panel, t.title, &font_pt_18, COL_TEXT);
    lv_obj_set_width(title, SCREEN_WIDTH - 20);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_height(title, lv_font_get_line_height(&font_pt_18));
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    const int bw = 146;
    const int bh = 64;
    lv_obj_t *done = ui_button(panel, TRS("Concluir", "Complete"), bw, bh, COL_OK);
    lv_obj_align(done, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_event_cb(done, on_sheet_done, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *pin = ui_button(panel, sync_is_pinned(t) ? TRS("Soltar", "Unpin") : TRS("Fixar", "Pin"),
                              bw, bh, COL_ACCENT);
    lv_obj_align(pin, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(pin, on_sheet_pin, LV_EVENT_CLICKED, nullptr);

    // Spec 7.3: Concluir · Pomodoro · Fixar. Fechar = tocar fora da folha.
    lv_obj_t *pomo = ui_button(panel, TRS("Pomodoro", "Pomodoro"), bw, bh, COL_MED);
    lv_obj_align(pomo, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(pomo, on_sheet_pomo, LV_EVENT_CLICKED, nullptr);
}

// ---- Lista ----

static void on_row(lv_event_t *e) {
    const Task *t = find(*(const RowRef *)lv_event_get_user_data(e));
    if (t) open_sheet(*t);
}

static void on_row_done(lv_event_t *e) {
    const Task *t = find(*(const RowRef *)lv_event_get_user_data(e));
    if (t) sync_complete(*t);
}

static void on_scroll(lv_event_t *e) {
    lv_obj_t *list = (lv_obj_t *)lv_event_get_target(e);
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        s_scrolling = true;
        s_scrollStartMs = clock_uptime_ms();
    } else {
        s_scrolling = false;
        s_scrollY = lv_obj_get_scroll_y(list);
    }
}

bool today_busy() {
    return s_scrolling && clock_uptime_ms() - s_scrollStartMs < 3000;
}

// Uma linha de 44 px (spec 7.3): faixa de prioridade (4 px) · hora em 14 ·
// titulo em 18 com reticencias · botao concluir.
static void build_row(lv_obj_t *list, const Task &t, RowRef &ref) {
    task_set_id(ref.projectId, t.projectId);
    task_set_id(ref.taskId, t.id);

    lv_obj_t *row = lv_obj_create(list);
    lv_obj_set_size(row, SCREEN_WIDTH, 44);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, theme_color(COL_SURFACE), 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(row, on_row, LV_EVENT_CLICKED, &ref);

    lv_obj_t *stripe = lv_obj_create(row);
    lv_obj_set_size(stripe, 4, 44);
    lv_obj_set_pos(stripe, 0, 0);
    lv_obj_set_style_bg_color(stripe, theme_color(prio_color(t.priority)), 0);
    lv_obj_set_style_bg_opa(stripe, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(stripe, 0, 0);
    lv_obj_set_style_radius(stripe, 0, 0);
    lv_obj_remove_flag(stripe, LV_OBJ_FLAG_CLICKABLE); // o toque vai para a linha

    char when[8];
    uint32_t whenColor = COL_DIM;
    const char *w = task_when(t, when, sizeof(when), &whenColor);
    // "ATRASADA" em 14 px passa dos 70 px da coluna e perde o ultimo A.
    lv_obj_t *wl = ui_label(row, w, t.overdue ? &font_pt_12 : &font_pt_14, whenColor);
    lv_obj_set_width(wl, 70);
    lv_label_set_long_mode(wl, LV_LABEL_LONG_CLIP);
    lv_obj_align(wl, LV_ALIGN_LEFT_MID, 12, 0);

    if (sync_is_pinned(t)) {
        lv_obj_t *dot = ui_label(row, "•", &font_pt_18, COL_ACCENT);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, 80, 0);
    }

    const int titleX = 92;
    lv_obj_t *title = ui_label(row, t.title, &font_pt_18, t.syncError ? COL_LATE : COL_TEXT);
    lv_obj_set_width(title, SCREEN_WIDTH - titleX - 52);
    lv_obj_set_height(title, lv_font_get_line_height(&font_pt_18));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, titleX, 0);

    lv_obj_t *done = lv_button_create(row);
    lv_obj_set_size(done, 44, 40);
    lv_obj_align(done, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_bg_color(done, theme_color(COL_SURFACE), 0);
    lv_obj_set_style_border_width(done, 0, 0);
    lv_obj_t *ok = ui_icon(done, LV_SYMBOL_OK, COL_OK);
    lv_obj_center(ok);
    lv_obj_add_event_cb(done, on_row_done, LV_EVENT_CLICKED, &ref);
}

static void build_empty(lv_obj_t *parent) {
    const char *txt = TRS("Nada para hoje", "Nothing for today");
    if (!sync_has_data()) txt = TRS("Buscando tarefas…", "Fetching tasks…");
    lv_obj_t *msg = ui_label(parent, txt, &font_pt_24, COL_DIM);
    lv_obj_center(msg);
}

void today_build(lv_obj_t *parent) {
    int n = 0;
    const Task *v = sync_visible(&n);
    if (n == 0) {
        build_empty(parent);
        return;
    }

    lv_obj_t *list = lv_obj_create(parent);
    lv_obj_set_size(list, SCREEN_WIDTH, CONTENT_H);
    lv_obj_set_pos(list, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_add_event_cb(list, on_scroll, LV_EVENT_SCROLL_BEGIN, nullptr);
    lv_obj_add_event_cb(list, on_scroll, LV_EVENT_SCROLL_END, nullptr);

    for (int i = 0; i < n && i < TASK_LIST_MAX; i++) build_row(list, v[i], s_rows[i]);

    const int extra = sync_truncated();
    if (extra > 0) { // spec 5.1: "+N" discreto
        char buf[64];
        std::snprintf(buf, sizeof(buf), TRS("+%d além do limite de %d", "+%d beyond the %d limit"),
                      extra, TASK_LIST_MAX);
        lv_obj_t *more = ui_label(list, buf, &font_pt_14, COL_DIM);
        lv_obj_set_style_pad_all(more, 12, 0);
    }

    lv_obj_update_layout(list);
    lv_obj_scroll_to_y(list, s_scrollY, LV_ANIM_OFF); // preserva a posicao entre refreshes
}
