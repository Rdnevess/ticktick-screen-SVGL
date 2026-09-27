#include "screen_focus.h"

#include <cstring>

#include "../../config.h"
#include "../app/pomo.h"
#include "../app/sync.h"
#include "../platform/settings.h"
#include "i18n.h"
#include "shell.h"
#include "task_view.h"
#include "theme.h"

// Os cards guardam a tarefa pelos ids: o array visivel muda a cada refresh, e
// o callback procura de novo em vez de confiar num ponteiro velho.
struct CardRef {
    char projectId[TASK_ID_BYTES + 1];
    char taskId[TASK_ID_BYTES + 1];
};
static CardRef s_refs[FOCUS_SLOTS];

static const Task *find(const CardRef &r) {
    int n = 0;
    const Task *v = sync_visible(&n);
    for (int i = 0; i < n; i++)
        if (std::strcmp(v[i].id, r.taskId) == 0 && std::strcmp(v[i].projectId, r.projectId) == 0)
            return &v[i];
    return nullptr;
}

static void on_done(lv_event_t *e) {
    const Task *t = find(*(const CardRef *)lv_event_get_user_data(e));
    if (t) sync_complete(*t); // some da tela no mesmo instante (spec 5.5)
}

static void on_pomo(lv_event_t *e) {
    const Task *t = find(*(const CardRef *)lv_event_get_user_data(e));
    if (t) pomo_app_request_start(*t); // spec 7.2: o segundo alvo do card
}

static void on_long(lv_event_t *e) {
    const Task *t = find(*(const CardRef *)lv_event_get_user_data(e));
    if (t && sync_toggle_pin(*t) == PinResult::Full)
        shell_toast(TRS("Só dá para fixar duas. Solte uma antes.",
                        "Only two can be pinned. Unpin one first."));
}

static lv_obj_t *meta_row(lv_obj_t *card, int width) {
    lv_obj_t *meta = lv_obj_create(card);
    lv_obj_set_size(meta, width, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(meta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(meta, 0, 0);
    lv_obj_set_style_pad_all(meta, 0, 0);
    lv_obj_set_style_pad_column(meta, 10, 0);
    lv_obj_remove_flag(meta, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(meta, LV_OBJ_FLAG_CLICKABLE); // o toque longo e do card
    lv_obj_set_flex_flow(meta, LV_FLEX_FLOW_ROW);
    lv_obj_align(meta, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return meta;
}

// Um card (spec 7.2): titulo em ate 2 linhas; embaixo, quando · lista ·
// prioridade (omitida se nenhuma) · selos; a direita, os alvos de pomodoro e
// de concluir.
static void build_card(lv_obj_t *parent, const Task &t, int slot, int y, int h,
                       const lv_font_t *titleFont) {
    CardRef &ref = s_refs[slot];
    task_set_id(ref.projectId, t.projectId);
    task_set_id(ref.taskId, t.id);

    lv_obj_t *card = ui_panel(parent, SCREEN_WIDTH, h);
    lv_obj_set_pos(card, 0, y);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, on_long, LV_EVENT_LONG_PRESSED, &ref);
    if (t.syncError) {
        lv_obj_set_style_border_width(card, 2, 0);
        lv_obj_set_style_border_color(card, theme_color(COL_LATE), 0);
    }

    const int btn = 56;
    const int textW = SCREEN_WIDTH - 20 - 2 * btn - 20; // dois alvos + folgas

    lv_obj_t *title = ui_label(card, t.title, titleFont, COL_TEXT);
    lv_obj_set_width(title, textW);
    lv_obj_set_height(title, 2 * lv_font_get_line_height(titleFont));
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT); // 2 linhas, quebra por palavra
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *meta = meta_row(card, textW);
    char when[8];
    uint32_t whenColor = COL_DIM;
    const char *w = task_when(t, when, sizeof(when), &whenColor);
    ui_label(meta, w, &font_pt_14, whenColor);
    const char *list = sync_list_name(t.projectId);
    if (list[0]) {
        lv_obj_t *ln = ui_label(meta, list, &font_pt_14, COL_DIM);
        // largura explicita antes do long mode: com max_width o label ainda
        // tem largura 0 no layout e o "..." troca o texto desde a letra 0.
        lv_point_t sz;
        lv_text_get_size(&sz, list, &font_pt_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_obj_set_width(ln, LV_MIN(sz.x, 140)); // nome longo nao empurra o resto
        lv_obj_set_height(ln, lv_font_get_line_height(&font_pt_14));
        lv_label_set_long_mode(ln, LV_LABEL_LONG_DOT);
    }
    const char *prio = prio_label(t.priority);
    if (prio[0]) ui_label(meta, prio, &font_pt_14, prio_color(t.priority));
    if (sync_is_pinned(t)) ui_label(meta, TRS("FIXADA", "PINNED"), &font_pt_14, COL_ACCENT);
    if (t.syncError) ui_label(meta, TRS("NÃO CONCLUIU", "NOT DONE"), &font_pt_14, COL_LATE);

    lv_obj_t *pomo = lv_button_create(card);
    lv_obj_set_size(pomo, btn, btn);
    lv_obj_align(pomo, LV_ALIGN_RIGHT_MID, -(btn + 8), 0);
    lv_obj_set_style_bg_color(pomo, theme_color(COL_ACCENT), 0);
    lv_obj_set_style_radius(pomo, btn / 2, 0);
    lv_obj_set_style_border_width(pomo, 0, 0);
    lv_obj_t *play = ui_icon(pomo, LV_SYMBOL_PLAY, COL_BG);
    lv_obj_center(play);
    lv_obj_add_event_cb(pomo, on_pomo, LV_EVENT_CLICKED, &ref);

    lv_obj_t *done = lv_button_create(card);
    lv_obj_set_size(done, btn, btn);
    lv_obj_align(done, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(done, theme_color(COL_OK), 0);
    lv_obj_set_style_radius(done, btn / 2, 0);
    lv_obj_set_style_border_width(done, 0, 0);
    lv_obj_t *ok = ui_icon(done, LV_SYMBOL_OK, COL_BG);
    lv_obj_center(ok);
    lv_obj_add_event_cb(done, on_done, LV_EVENT_CLICKED, &ref);
}

static void build_empty(lv_obj_t *parent) {
    // Antes do primeiro ciclo bom, "dia limpo" seria mentira (spec 5.2).
    const char *title = TRS("Dia limpo", "All clear");
    const char *sub = TRS("Nenhuma tarefa para hoje", "No tasks for today");
    if (!sync_has_data()) {
        title = TRS("Buscando tarefas…", "Fetching tasks…");
        sub = sync_stale() ? TRS("Sem resposta do TickTick ainda.", "No answer from TickTick yet.") : "";
    }
    lv_obj_t *msg = ui_label(parent, title, &font_pt_36, COL_TEXT);
    lv_obj_align(msg, LV_ALIGN_CENTER, 0, -18);
    lv_obj_t *s = ui_label(parent, sub, &font_pt_18, COL_DIM);
    lv_obj_align(s, LV_ALIGN_CENTER, 0, 24);
}

void focus_build(lv_obj_t *parent) {
    int n = 0;
    const Task *v = sync_visible(&n);
    int idx[FOCUS_SLOTS] = {-1, -1};
    const int filled = focus_select(v, n, settings().pins, idx);

    if (filled == 0) {
        build_empty(parent);
    } else if (filled == 1) {
        build_card(parent, v[idx[0]], 0, 0, CONTENT_H, &font_pt_36); // spec 7.2: uma so
    } else {
        build_card(parent, v[idx[0]], 0, 0, 120, &font_pt_28);
        build_card(parent, v[idx[1]], 1, 132, 120, &font_pt_28);
    }
}
