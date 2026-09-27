#include "page_wifi.h"

#include <cstring>

#include "../../config.h"
#include "../app/app.h"
#include "../platform/clock.h"
#include "../platform/wifi_manager.h"
#include "i18n.h"
#include "theme.h"

static lv_obj_t *s_list = nullptr;
static lv_obj_t *s_ta = nullptr;
static lv_obj_t *s_kb = nullptr;
static lv_obj_t *s_status = nullptr;
static char s_ssid[33];
static bool s_scanPending = false;
static bool s_connectPending = false;
static int64_t s_nextRetryMs = 0; // proxima tentativa silenciosa das redes salvas
static bool s_backPending = false;

static void on_back(lv_event_t *e) {
    (void)e;
    s_backPending = true;
}

static void pump() { lv_timer_handler(); }

// Zera memoria pelo ponteiro volatile, que o otimizador nao remove (mesmo
// padrao de core/creds.cpp).
static void wipe(char *p, size_t n) {
    volatile char *v = p;
    while (n--) *v++ = 0;
}

static void show_list(const char *msg) {
    lv_obj_add_flag(s_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_status, msg);
}

static void on_pick(lv_event_t *e) {
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    const char *txt = lv_list_get_button_text(s_list, btn);
    if (!txt) return;
    strlcpy(s_ssid, txt, sizeof(s_ssid));
    lv_label_set_text_fmt(s_status, TRS("Senha de \"%s\":", "Password for \"%s\":"), s_ssid);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(s_ta, "");
    lv_obj_remove_flag(s_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(s_kb, s_ta);
}

static void on_kb(lv_event_t *e) {
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) s_connectPending = true; // conecta no tick
    else if (code == LV_EVENT_CANCEL) show_list(TRS("Toque na sua rede", "Tap your network"));
}

static void on_rescan(lv_event_t *e) {
    (void)e;
    s_scanPending = true;
}

// Adaptador do tick de autoConnect (ssid/indice/total) para a pagina: so
// precisa bombear o LVGL e avisar que esta tentando as redes salvas sozinho.
static void on_retry_tick(const char *, int, int) {
    lv_label_set_text(s_status, TRS("Tentando redes salvas…", "Trying saved networks…"));
    lv_timer_handler();
}

static void scan() {
    lv_obj_clean(s_list);
    show_list(TRS("Procurando redes…", "Scanning networks…"));
    lv_refr_now(nullptr);

    WiFiManager::NetworkInfo nets[12];
    const int n = wifi().scanNetworks(nets, 12);
    for (int i = 0; i < n; i++) {
        // Sem icone: o LV_SYMBOL_WIFI nao existe nas font_pt_*.
        lv_obj_t *b = lv_list_add_button(s_list, nullptr, nets[i].ssid);
        lv_obj_set_style_bg_color(b, theme_color(COL_SURFACE), 0);
        lv_obj_set_style_text_color(b, theme_color(COL_TEXT), 0);
        lv_obj_set_style_text_font(b, &font_pt_18, 0);
        lv_obj_add_event_cb(b, on_pick, LV_EVENT_CLICKED, nullptr);
    }
    lv_label_set_text(s_status, n > 0 ? TRS("Toque na sua rede", "Tap your network")
                                      : TRS("Nenhuma rede. Toque em Procurar.",
                                            "No networks found. Tap Scan."));
}

static void connect() {
    char pass[65];
    strlcpy(pass, lv_textarea_get_text(s_ta), sizeof(pass));
    lv_textarea_set_text(s_ta, "");
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ta, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(s_status, TRS("Conectando a %s…", "Connecting to %s…"), s_ssid);
    lv_refr_now(nullptr);

    const bool ok = wifi().connectTo(s_ssid, pass, 15000, pump);
    wipe(pass, sizeof(pass));
    if (ok) {
        app_wifi_connected();
        return;
    }
    const char *fail = TRS("Não conectou. Confira a senha e toque na rede de novo.",
                          "Couldn't connect. Check the password and tap the network again.");
    show_list(fail);
    // connectTo() derruba a rede anterior ao falhar (WiFi.disconnect() no fim
    // do wifi_manager.h) e nao grava a rede nova (_addNetwork so roda no
    // sucesso, la dentro): sem isto o aparelho ficava offline ate a tentativa
    // silenciosa de 60 s (fix round 1, revisao do Voltar sem derrubar a rede).
    if (wifi().getSavedCount() > 0 && wifi().autoConnect(WIFI_CONNECT_TIMEOUT_MS, on_retry_tick) &&
        !app_is_child()) {
        app_wifi_connected(); // primeiro boot: segue o fluxo normalmente
        return;
    }
    show_list(fail); // restaura por cima do texto de "tentando" do retry (se rodou)
}

void page_wifi_build(lv_obj_t *scr) {
    lv_obj_t *title = ui_label(scr, TRS("Configurar WiFi", "Set up WiFi"), &font_pt_24, COL_TEXT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

    lv_obj_t *rescan = ui_button(scr, TRS("Procurar", "Scan"), 130, 40, COL_SURFACE);
    lv_obj_align(rescan, LV_ALIGN_TOP_RIGHT, -12, 6);
    lv_obj_add_event_cb(rescan, on_rescan, LV_EVENT_CLICKED, nullptr);

    if (app_is_child()) { // aberta pelo Settings: da para desistir
        lv_obj_t *back = ui_back_button(scr);
        lv_obj_align(back, LV_ALIGN_TOP_RIGHT, -144, 6);
        lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
    }

    s_status = ui_label(scr, "", &font_pt_18, COL_DIM);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 14, 52);

    s_list = lv_list_create(scr);
    lv_obj_set_size(s_list, SCREEN_WIDTH - 24, SCREEN_HEIGHT - 90);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_style_bg_color(s_list, theme_color(COL_BG), 0);
    lv_obj_set_style_border_width(s_list, 0, 0);

    s_ta = lv_textarea_create(scr);
    lv_textarea_set_one_line(s_ta, true);
    lv_textarea_set_password_mode(s_ta, true); // o bullet e U+2022: a fonte tem
    lv_textarea_set_placeholder_text(s_ta, TRS("senha do WiFi", "WiFi password"));
    lv_obj_set_style_text_font(s_ta, &font_pt_18, 0);
    lv_obj_set_size(s_ta, SCREEN_WIDTH - 24, 44);
    lv_obj_align(s_ta, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_add_flag(s_ta, LV_OBJ_FLAG_HIDDEN);

    s_kb = lv_keyboard_create(scr);
    // Apagar, OK e setas sao LV_SYMBOL_*: so a fonte de fabrica os tem. Ela
    // nao tem acento, o que nao importa para senha de WiFi.
    lv_obj_set_style_text_font(s_kb, &lv_font_montserrat_18, LV_PART_ITEMS);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_kb, on_kb, LV_EVENT_ALL, nullptr);

    s_scanPending = true; // escaneia no primeiro tick, com a pagina ja visivel
    s_nextRetryMs = clock_uptime_ms() + 60000; // primeira tentativa das redes salvas, 60 s depois
}

void page_wifi_tick() {
    if (s_backPending) {
        s_backPending = false;
        app_back();
        return;
    }
    const bool wasPending = s_scanPending || s_connectPending;
    if (s_scanPending) {
        s_scanPending = false;
        scan();
    }
    if (s_connectPending) {
        s_connectPending = false;
        connect();
    }

    // Router voltou depois de uma queda de energia: tenta as redes salvas de
    // novo sozinho, em vez de deixar a pessoa presa aqui ate digitar a senha
    // de novo (revisao final, item 2). So quando ninguem esta no meio de um
    // scan/conexao e a pessoa nao esta digitando senha (teclado e campo
    // escondidos).
    if (!wifi().isConnected() && !wasPending && wifi().getSavedCount() > 0 &&
        lv_obj_has_flag(s_kb, LV_OBJ_FLAG_HIDDEN) && lv_obj_has_flag(s_ta, LV_OBJ_FLAG_HIDDEN) &&
        clock_uptime_ms() >= s_nextRetryMs) {
        s_nextRetryMs = clock_uptime_ms() + 60000;
        if (wifi().autoConnect(WIFI_CONNECT_TIMEOUT_MS, on_retry_tick)) app_wifi_connected();
        else lv_label_set_text(s_status, TRS("Toque na sua rede", "Tap your network"));
    }
}

void page_wifi_leave() {
    s_list = s_ta = s_kb = s_status = nullptr;
    s_scanPending = s_connectPending = false;
    s_backPending = false;
}
