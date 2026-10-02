#include "page_settings.h"

#include <Arduino.h>

#include <cstdint>
#include <cstdio>

#include "../../config.h"
#include "../app/app.h"
#include "../app/sync.h"
#include "../core/settings_values.h"
#include "../net/net_worker.h"
#include "../net/oauth_store.h"
#include "../platform/clock.h"
#include "../platform/display.h"
#include "../platform/settings.h"
#include "../platform/wifi_manager.h"
#include "i18n.h"
#include "page_pin.h"
#include "theme.h"

enum class Ctl : uint8_t {
    None, Back,
    LangPt, LangEn, TzMinus, TzPlus, PollMinus, PollPlus, PomoMinus, PomoPlus,
    BriLow, BriMid, BriHigh,
    // Task 6:
    Lists, Wifi, Repair, Pin, Tls, Reset, ResetYes, ResetNo,
    IdleClock,
};

static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_tzVal = nullptr;
static lv_obj_t *s_pollVal = nullptr;
static lv_obj_t *s_pomoVal = nullptr;
static lv_obj_t *s_briB[3] = {nullptr, nullptr, nullptr}; // Baixo, Medio, Alto
static Ctl s_ctl = Ctl::None;
static bool s_tzChanged = false;
static lv_obj_t *s_tlsWarn = nullptr;
static lv_obj_t *s_tlsSwitch = nullptr;
static lv_obj_t *s_idleSwitch = nullptr;
static lv_obj_t *s_confirm = nullptr; // "Apagar tudo e reiniciar?" no layer_top

static void on_ctl(lv_event_t *e) { s_ctl = (Ctl)(intptr_t)lv_event_get_user_data(e); }

static void bind(lv_obj_t *obj, Ctl c) {
    lv_obj_add_event_cb(obj, on_ctl, LV_EVENT_CLICKED, (void *)(intptr_t)c);
}

// Uma linha de 44 px: rotulo a esquerda; o controle vai a direita.
static lv_obj_t *row(lv_obj_t *list, const char *label) {
    lv_obj_t *r = lv_obj_create(list);
    lv_obj_set_size(r, SCREEN_WIDTH - 24, 44);
    lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(r, 0, 0);
    lv_obj_set_style_pad_all(r, 0, 0);
    lv_obj_set_style_border_side(r, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(r, 1, 0);
    lv_obj_set_style_border_color(r, theme_color(COL_SURFACE), 0);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = ui_label(r, label, &font_pt_18, COL_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 4, 0);
    return r;
}

static void section(lv_obj_t *list, const char *title) {
    lv_obj_t *s = ui_label(list, title, &font_pt_14, COL_DIM);
    lv_obj_set_style_pad_top(s, 10, 0);
}

static lv_obj_t *small_button(lv_obj_t *parent, const char *txt, uint32_t bg, int w, Ctl c) {
    lv_obj_t *b = ui_button(parent, txt, w, 36, bg);
    bind(b, c);
    return b;
}

// [-] valor [+] alinhados a direita. Devolve o label do valor.
static lv_obj_t *stepper(lv_obj_t *r, Ctl minus, Ctl plus) {
    lv_obj_t *p = small_button(r, "+", COL_SURFACE, 44, plus);
    lv_obj_align(p, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_t *v = ui_label(r, "", &font_pt_18, COL_TEXT);
    lv_obj_set_width(v, 110);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, -52, 0);
    lv_obj_t *m = small_button(r, "-", COL_SURFACE, 44, minus);
    lv_obj_align(m, LV_ALIGN_RIGHT_MID, -166, 0);
    return v;
}

static void show_values() {
    for (int i = 0; i < 3; i++)
        lv_obj_set_style_bg_color(s_briB[i], theme_color(i == settings().briIdx ? COL_ACCENT : COL_SURFACE), 0);
    char b[24];
    tz_format(settings().tzMin, b, sizeof(b));
    lv_label_set_text(s_tzVal, b);
    lv_label_set_text_fmt(s_pollVal, "%d min", settings().pollMin);
    lv_label_set_text_fmt(s_pomoVal, "%d min", settings().pomoMin);
}

static void build_prefs(lv_obj_t *list) {
    section(list, TRS("Preferências", "Preferences"));

    lv_obj_t *lang = row(list, TRS("Idioma", "Language"));
    const bool en = settings().langEn;
    lv_obj_t *enB = small_button(lang, "EN", en ? COL_ACCENT : COL_SURFACE, 64, Ctl::LangEn);
    lv_obj_align(enB, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_t *ptB = small_button(lang, "PT", en ? COL_SURFACE : COL_ACCENT, 64, Ctl::LangPt);
    lv_obj_align(ptB, LV_ALIGN_RIGHT_MID, -76, 0);

    s_tzVal = stepper(row(list, TRS("Fuso", "Time zone")), Ctl::TzMinus, Ctl::TzPlus);
    s_pollVal = stepper(row(list, TRS("Atualizar a cada", "Refresh every")), Ctl::PollMinus, Ctl::PollPlus);
    s_pomoVal = stepper(row(list, TRS("Pomodoro", "Pomodoro")), Ctl::PomoMinus, Ctl::PomoPlus);
    lv_obj_t *bri = row(list, TRS("Brilho", "Brightness"));
    static const Ctl BRI_CTL[3] = {Ctl::BriLow, Ctl::BriMid, Ctl::BriHigh};
    const char *briTxt[3] = {TRS("Baixo", "Low"), TRS("Médio", "Medium"), TRS("Alto", "High")};
    for (int i = 0; i < 3; i++) {
        s_briB[i] = small_button(bri, briTxt[i], COL_SURFACE, 76, BRI_CTL[i]);
        lv_obj_align(s_briB[i], LV_ALIGN_RIGHT_MID, -4 - (2 - i) * 82, 0);
    }
    lv_obj_t *idle = row(list, TRS("Relógio em descanso", "Clock screensaver"));
    s_idleSwitch = lv_switch_create(idle);
    lv_obj_align(s_idleSwitch, LV_ALIGN_RIGHT_MID, -4, 0);
    if (settings().idleClock) lv_obj_add_state(s_idleSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_idleSwitch, on_ctl, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)Ctl::IdleClock);
    show_values();
}

// Grupo "Conta e aparelho": Task 6.

// Linha que abre outra pagina: rotulo, valor em cinza e a seta.
static lv_obj_t *nav_row(lv_obj_t *list, const char *label, const char *value, Ctl c) {
    lv_obj_t *r = row(list, label);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    bind(r, c);
    lv_obj_t *arrow = ui_icon(r, LV_SYMBOL_RIGHT, COL_DIM);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -4, 0);
    if (value && value[0]) {
        lv_obj_t *v = ui_label(r, value, &font_pt_14, COL_DIM);
        // largura explicita antes do long mode (mesma causa do item 1 da
        // revisao: com max_width o "..." troca o texto desde a letra 0).
        lv_point_t sz;
        lv_text_get_size(&sz, value, &font_pt_14, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_obj_set_width(v, LV_MIN(sz.x, 200));
        lv_obj_set_height(v, lv_font_get_line_height(&font_pt_14));
        lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
        lv_obj_align(v, LV_ALIGN_RIGHT_MID, -30, 0);
    }
    return r;
}

static void show_tls() {
    const bool insecure = settings().tlsInsecure;
    lv_label_set_text(s_tlsWarn, insecure ? TRS("desligada: inseguro", "off: insecure")
                                          : TRS("cadeia embutida", "built-in chain"));
    lv_obj_set_style_text_color(s_tlsWarn, theme_color(insecure ? COL_LATE : COL_DIM), 0);
}

static void on_tls(lv_event_t *e) {
    (void)e;
    s_ctl = Ctl::Tls; // aplica no tick
}

static void build_account(lv_obj_t *list) {
    section(list, TRS("Conta e aparelho", "Account and device"));

    char buf[48];
    snprintf(buf, sizeof(buf), TRS("%d marcada(s)", "%d selected"), settings().listCount);
    nav_row(list, TRS("Listas do dia", "Lists for the day"), buf, Ctl::Lists);
    nav_row(list, TRS("Trocar rede WiFi", "Change WiFi network"),
            wifi().isConnected() ? (sync_demo() ? sync_demo_ssid() : wifi().getSSID().c_str()) : "",
            Ctl::Wifi);
    nav_row(list, TRS("Parear de novo", "Pair again"), "", Ctl::Repair);

    const bool locked = oauth_state() == CredsState::Locked;
    nav_row(list, TRS("PIN", "PIN"),
            locked ? TRS("ativo · remover", "on · remove") : TRS("desligado · definir", "off · set"),
            Ctl::Pin);

    lv_obj_t *tls = row(list, TRS("Verificar certificado", "Verify certificate"));
    s_tlsSwitch = lv_switch_create(tls);
    lv_obj_align(s_tlsSwitch, LV_ALIGN_RIGHT_MID, -4, 0);
    if (!settings().tlsInsecure) lv_obj_add_state(s_tlsSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(s_tlsSwitch, on_tls, LV_EVENT_VALUE_CHANGED, nullptr);
    s_tlsWarn = ui_label(tls, "", &font_pt_14, COL_DIM);
    lv_obj_align(s_tlsWarn, LV_ALIGN_RIGHT_MID, -70, 0);
    show_tls();

    lv_obj_t *about = row(list, TRS("Sobre", "About"));
    lv_obj_t *ver = ui_label(about, "TickTick Screen v" FW_VERSION " · MIT", &font_pt_14, COL_DIM);
    lv_obj_align(ver, LV_ALIGN_RIGHT_MID, -4, 0);

    lv_obj_t *reset = row(list, TRS("Reset de fábrica", "Factory reset"));
    lv_obj_add_flag(reset, LV_OBJ_FLAG_CLICKABLE);
    bind(reset, Ctl::Reset);
    lv_obj_t *resetIcon = ui_icon(reset, LV_SYMBOL_WARNING, COL_LATE);
    lv_obj_align(resetIcon, LV_ALIGN_RIGHT_MID, -4, 0);
}

static void open_reset_confirm() {
    s_confirm = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_confirm, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_confirm, theme_color(COL_BG), 0);
    lv_obj_set_style_bg_opa(s_confirm, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_confirm, 0, 0);
    lv_obj_set_style_radius(s_confirm, 0, 0);
    lv_obj_remove_flag(s_confirm, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *q = ui_label(s_confirm, TRS("Apagar tudo e reiniciar?", "Erase everything and restart?"),
                           &font_pt_28, COL_LATE);
    lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_t *why = ui_label(s_confirm,
                             TRS("Apaga as credenciais do TickTick, o PIN, as listas, as "
                                 "preferências e as redes WiFi salvas.",
                                 "Erases the TickTick credentials, the PIN, the lists, the "
                                 "preferences and the saved WiFi networks."),
                             &font_pt_18, COL_DIM);
    lv_obj_set_width(why, SCREEN_WIDTH - 60);
    lv_label_set_long_mode(why, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(why, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(why, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *yes = ui_button(s_confirm, TRS("Apagar", "Erase"), 200, 60, COL_LATE);
    lv_obj_align(yes, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    bind(yes, Ctl::ResetYes);
    lv_obj_t *no = ui_button(s_confirm, TRS("Cancelar", "Cancel"), 200, 60, COL_SURFACE);
    lv_obj_align(no, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
    bind(no, Ctl::ResetNo);
}

static void factory_reset() {
    Serial.println("[settings] reset de fabrica");
    oauth_wipe();
    settings_reset();
    wifi().forgetAll();
    WiFi.disconnect(true, true); // tambem apaga a rede que o esp-idf guarda em nvs.net80211
    delay(200);
    ESP.restart();
}

void page_settings_build(lv_obj_t *scr) {
    lv_obj_t *back = ui_back_button(scr);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 8, 6);
    bind(back, Ctl::Back);

    lv_obj_t *title = ui_label(scr, TRS("Configurações", "Settings"), &font_pt_24, COL_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 30, 12);

    s_list = lv_obj_create(scr);
    lv_obj_set_size(s_list, SCREEN_WIDTH, SCREEN_HEIGHT - 52);
    lv_obj_set_pos(s_list, 0, 52);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);
    lv_obj_set_style_pad_hor(s_list, 12, 0);
    lv_obj_set_style_pad_ver(s_list, 0, 0);
    lv_obj_set_style_pad_row(s_list, 0, 0);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_list, LV_DIR_VER);

    build_prefs(s_list);
    build_account(s_list);
    s_ctl = Ctl::None;
}

static void run(Ctl c) {
    switch (c) {
        case Ctl::Back: app_go(Page::Main); return;
        case Ctl::LangPt:
        case Ctl::LangEn:
            settings().langEn = (c == Ctl::LangEn);
            settings_save();
            lang_set_en(settings().langEn);
            app_go(Page::Settings); // reconstroi no idioma novo (spec 7.7)
            return;
        case Ctl::BriLow:
        case Ctl::BriMid:
        case Ctl::BriHigh:
            settings().briIdx = (int)c - (int)Ctl::BriLow;
            settings_save();
            display_set_brightness(settings().briIdx); // efeito imediato
            break;
        case Ctl::TzMinus:
        case Ctl::TzPlus:
            settings().tzMin = tz_step(settings().tzMin, c == Ctl::TzPlus ? +1 : -1);
            settings_save();
            clock_set_tz_offset((int32_t)settings().tzMin * 60);
            s_tzChanged = true; // o "hoje" mudou: refresh ao sair
            break;
        case Ctl::PollMinus:
        case Ctl::PollPlus:
            sync_set_poll(poll_step(settings().pollMin, c == Ctl::PollPlus ? +1 : -1));
            break;
        case Ctl::PomoMinus:
        case Ctl::PomoPlus:
            settings().pomoMin = pomo_step(settings().pomoMin, c == Ctl::PomoPlus ? +1 : -1);
            settings_save(); // vale para o proximo ciclo
            break;
        case Ctl::Lists:  app_open_child(Page::Lists); return;
        case Ctl::Wifi:   app_open_child(Page::Wifi); return;
        case Ctl::Repair: app_open_child(Page::Pair); return;
        case Ctl::Pin:
            page_pin_admin_prepare(oauth_state() == CredsState::Locked);
            app_open_child(Page::PinAdmin);
            return;
        case Ctl::Tls:
            settings().tlsInsecure = !lv_obj_has_state(s_tlsSwitch, LV_STATE_CHECKED);
            settings_save();
            net_set_tls_insecure(settings().tlsInsecure);
            show_tls();
            return;
        case Ctl::IdleClock:
            settings().idleClock = lv_obj_has_state(s_idleSwitch, LV_STATE_CHECKED);
            settings_save(); // vale ja: idle_tick le a cada volta
            return;
        case Ctl::Reset:    open_reset_confirm(); return;
        case Ctl::ResetYes: factory_reset(); return;
        case Ctl::ResetNo:
            lv_obj_delete(s_confirm); // no tick, fora do callback do botao
            s_confirm = nullptr;
            return;
        default: return;
    }
    show_values();
}

void page_settings_tick() {
    if (!s_list || s_ctl == Ctl::None) return;
    const Ctl c = s_ctl;
    s_ctl = Ctl::None;
    run(c);
}

void page_settings_leave() {
    if (s_tzChanged) sync_refresh_now(); // outro fuso, outro "hoje"
    s_tzChanged = false;
    s_list = s_tzVal = s_pollVal = s_pomoVal = nullptr;
    s_briB[0] = s_briB[1] = s_briB[2] = nullptr;
    s_tlsWarn = s_tlsSwitch = s_idleSwitch = s_confirm = nullptr; // o layer_top e limpo pelo app
    s_ctl = Ctl::None;
}
