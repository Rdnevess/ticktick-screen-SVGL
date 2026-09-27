#include "pomo_overlay.h"

#include <cstdint>
#include <cstdio>

#include "../../config.h"
#include "../app/pomo.h"
#include "../platform/clock.h"
#include "i18n.h"
#include "screen_today.h"
#include "theme.h"

enum class Act : uint8_t { None, Collapse, CancelCycle, Swap, Keep, Renew, CancelFinished, Complete };

static lv_obj_t *s_root = nullptr;  // raiz da sobreposicao no layer_top
static lv_obj_t *s_count = nullptr; // contagem do expandido
static PomoView s_built = PomoView::None;
static bool s_dirty = false;
static Act s_act = Act::None; // botoes so marcam; a acao roda no tick
static int s_shownSec = -1;

static void on_act(lv_event_t *e) { s_act = (Act)(intptr_t)lv_event_get_user_data(e); }

void pomo_overlay_invalidate() {
    s_root = nullptr;
    s_count = nullptr;
    s_built = PomoView::None;
    s_dirty = true;
}

void pomo_overlay_rebuild() { s_dirty = true; } // clear() apaga o que existe

static lv_obj_t *make_root(lv_opa_t opa) {
    lv_obj_t *r = lv_obj_create(lv_layer_top());
    lv_obj_set_size(r, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(r, 0, 0);
    lv_obj_set_style_bg_color(r, theme_color(COL_BG), 0);
    lv_obj_set_style_bg_opa(r, opa, 0);
    lv_obj_set_style_border_width(r, 0, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE); // absorve toques da pagina de baixo
    return r;
}

static lv_obj_t *act_button(lv_obj_t *parent, const char *txt, uint32_t bg, Act a, int w) {
    lv_obj_t *b = ui_button(parent, txt, w, 56, bg);
    lv_obj_add_event_cb(b, on_act, LV_EVENT_CLICKED, (void *)(intptr_t)a);
    return b;
}

// Titulo da tarefa centralizado, com ate `lines` linhas e reticencias no fim.
static lv_obj_t *task_title(lv_obj_t *parent, const char *title, int lines) {
    lv_obj_t *t = ui_label(parent, title, &font_pt_24, COL_TEXT);
    lv_obj_set_width(t, SCREEN_WIDTH - 40);
    lv_obj_set_height(t, lines * lv_font_get_line_height(&font_pt_24));
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    return t;
}

// Contagem em 48 px e o titulo; tocar fora dos botoes recolhe (spec 7.6).
static void build_expanded() {
    s_root = make_root(LV_OPA_COVER);
    lv_obj_add_event_cb(s_root, on_act, LV_EVENT_CLICKED, (void *)(intptr_t)Act::Collapse);

    lv_obj_t *label = ui_label(s_root, TRS("Pomodoro", "Pomodoro"), &font_pt_18, COL_DIM);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 18);

    // Contagem logo abaixo do rotulo e titulo em ate 3 linhas: titulos longos
    // nao cortam e o espaco acima dos botoes e aproveitado.
    s_count = ui_label(s_root, "", &font_pt_48, COL_ACCENT);
    lv_obj_align(s_count, LV_ALIGN_TOP_MID, 0, 46);
    s_shownSec = -1;

    lv_obj_t *t = task_title(s_root, pomo_app_state().title, 3);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 118);

    lv_obj_t *hint = ui_label(s_root, TRS("Toque para recolher", "Tap to collapse"), &font_pt_14, COL_DIM);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 16, -30);

    // Decisao 2 do Plano C: sem isto nao haveria como parar antes do fim.
    lv_obj_t *cancel = act_button(s_root, TRS("Cancelar", "Cancel"), COL_SURFACE, Act::CancelCycle, 180);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_RIGHT, -14, -14);
}

static void build_confirm() {
    s_root = make_root(LV_OPA_70);

    lv_obj_t *panel = ui_panel(s_root, 440, 200);
    lv_obj_center(panel);

    lv_obj_t *q = ui_label(panel, TRS("Trocar o pomodoro atual?", "Replace the current pomodoro?"),
                           &font_pt_24, COL_TEXT);
    lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *cur = ui_label(panel, pomo_app_state().title, &font_pt_14, COL_DIM);
    lv_obj_set_width(cur, 400);
    lv_label_set_long_mode(cur, LV_LABEL_LONG_DOT);
    lv_obj_set_height(cur, lv_font_get_line_height(&font_pt_14));
    lv_obj_set_style_text_align(cur, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(cur, LV_ALIGN_TOP_MID, 0, 44);

    lv_obj_t *swap = act_button(panel, TRS("Trocar", "Replace"), COL_ACCENT, Act::Swap, 190);
    lv_obj_align(swap, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *keep = act_button(panel, TRS("Manter", "Keep"), COL_SURFACE, Act::Keep, 190);
    lv_obj_align(keep, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// Fim do ciclo (spec 7.6): tela cheia, sem expirar; so os botoes decidem.
static void build_finished() {
    const Pomodoro &p = pomo_app_state();
    s_root = make_root(LV_OPA_COVER);

    lv_obj_t *head = ui_label(s_root, TRS("Pomodoro concluído!", "Pomodoro done!"), &font_pt_28, COL_OK);
    lv_obj_align(head, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *t = task_title(s_root, p.title, 2);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 76);

    const bool canDone = pomo_can_complete(p);
    if (!canDone) {
        lv_obj_t *why = ui_label(s_root,
                                 TRS("A tarefa saiu do dia — concluída em outro lugar?",
                                     "The task left today's list — completed elsewhere?"),
                                 &font_pt_18, COL_MED);
        lv_obj_set_width(why, SCREEN_WIDTH - 40);
        lv_label_set_long_mode(why, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(why, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(why, LV_ALIGN_TOP_MID, 0, 164);
    }

    const int w = canDone ? 144 : 220;
    lv_obj_t *renew = act_button(s_root, TRS("Renovar", "Renew"), COL_ACCENT, Act::Renew, w);
    lv_obj_align(renew, LV_ALIGN_BOTTOM_LEFT, 14, -16);
    lv_obj_t *cancel = act_button(s_root, TRS("Cancelar", "Cancel"), COL_SURFACE, Act::CancelFinished, w);
    lv_obj_align(cancel, canDone ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_BOTTOM_RIGHT, canDone ? 0 : -14, -16);
    if (canDone) {
        lv_obj_t *done = act_button(s_root, TRS("Concluir", "Complete"), COL_OK, Act::Complete, w);
        lv_obj_align(done, LV_ALIGN_BOTTOM_RIGHT, -14, -16);
    }
}

static void clear() {
    if (s_root) lv_obj_delete(s_root); // no tick, nunca dentro do callback
    s_root = nullptr;
    s_count = nullptr;
}

static void build(PomoView v) {
    clear();
    s_built = v;
    if (v == PomoView::None) return;
    today_close_sheet(); // a folha do Hoje nao fica por baixo de um dialogo
    switch (v) {
        case PomoView::Expanded:    build_expanded(); break;
        case PomoView::ConfirmSwap: build_confirm(); break;
        case PomoView::Finished:    build_finished(); break;
        default: break;
    }
}

static void run_action(Act a) {
    switch (a) {
        case Act::Collapse:    pomo_app_set_view(PomoView::None); break;
        case Act::CancelCycle: pomo_app_cancel(); break;
        case Act::Swap:        pomo_app_confirm_swap(true); break;
        case Act::Keep:        pomo_app_confirm_swap(false); break;
        case Act::Renew:          pomo_app_renew(); break;
        case Act::CancelFinished: pomo_app_cancel(); break;
        case Act::Complete:       pomo_app_complete(); break;
        default: break;
    }
}

void pomo_overlay_tick() {
    if (s_act != Act::None) {
        const Act a = s_act;
        s_act = Act::None;
        run_action(a);
    }
    const PomoView v = pomo_app_view();
    if (v != s_built || s_dirty) {
        s_dirty = false;
        build(v);
    }
    if (s_count) {
        const int64_t ms = pomo_remaining_ms(pomo_app_state(), clock_uptime_ms());
        const int sec = (int)((ms + 999) / 1000);
        if (sec != s_shownSec) {
            s_shownSec = sec;
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, sec % 60);
            lv_label_set_text(s_count, buf);
        }
    }
}
