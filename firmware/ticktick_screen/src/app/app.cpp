#include "app.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <cstdlib>
#include <cstring>

#include "../../config.h"
#include "../platform/clock.h"
#include "../platform/console.h"
#include "../platform/display.h"
#include "../platform/screenshot.h"
#include "../platform/settings.h"
#include "../platform/wifi_manager.h"
#include "../net/net_worker.h"
#include "../net/oauth_store.h"
#include "../ui/i18n.h"
#include "../ui/page_message.h"
#include "../ui/page_pair.h"
#include "../ui/page_pin.h"
#include "../ui/page_lists.h"
#include "../ui/page_wifi.h"
#include "../ui/page_clock.h"
#include "../ui/page_settings.h"
#include "../ui/pomo_overlay.h"
#include "../ui/shell.h"
#include "../ui/theme.h"
#include "pairing.h"
#include "pomo.h"
#include "sync.h"

static PageOps s_ops[(int)Page::Count];
static Page s_page = Page::Boot;
static Page s_next = Page::Boot;
static bool s_switch = false;
static bool s_wifiReady = false; // conectou ao menos uma vez neste boot
static Page s_back = Page::Count; // para onde app_back() volta; Count = ninguem

void app_register(Page p, const PageOps &ops) { s_ops[(int)p] = ops; }

void app_go(Page p) {
    s_next = p;
    s_switch = true;
}

void app_go_root(Page p) {
    s_back = Page::Count; // sem isso, o "Voltar" da nova pagina ficaria orfao
    app_go(p);
}

Page app_page() { return s_page; }

// Fluxo do primeiro boot (spec 7.9). Cada task acrescenta a sua etapa aqui,
// na ordem em que ela acontece; o que sobra e a tela principal.
static Page flow_next() {
    if (!s_wifiReady) return Page::Wifi;
    if (!clock_has_time()) return Page::WaitTime;
    if (!net_has_creds())
        return oauth_state() == CredsState::Locked ? Page::PinEnter : Page::Pair;
    if (settings().listCount == 0) return Page::Lists;
    return Page::Main;
}

void app_advance() {
    s_back = Page::Count; // seguir o fluxo encerra a pagina filha (ex.: PIN errado ate o limite)
    app_go(flow_next());
}

void app_open_child(Page child) {
    s_back = s_page;
    app_go(child);
}

bool app_is_child() { return s_back != Page::Count; }

void app_back() {
    if (s_back == Page::Count) {
        app_advance();
        return;
    }
    const Page b = s_back;
    s_back = Page::Count;
    app_go(b);
}

// Aplica a troca pedida. Roda fora de callback do LVGL: no app_loop e no boot.
static void apply_switch() {
    if (!s_switch) return;
    s_switch = false;
    if (s_ops[(int)s_page].leave) s_ops[(int)s_page].leave();
    s_page = s_next;
    if (s_page == Page::Main) s_back = Page::Count; // a principal nunca e filha

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(lv_layer_top()); // overlay nao vaza entre paginas (spec 7.6)
    pomo_overlay_invalidate();    // ...mas o pomodoro e redesenhado por cima da nova
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, theme_color(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    if (s_ops[(int)s_page].build) s_ops[(int)s_page].build(scr);
}

static void main_build(lv_obj_t *scr) {
    (void)scr; // o shell monta direto na tela ativa
    shell_build();
}

// ---- Console ----

static void cmd_lang(const char *a) {
    if (std::strcmp(a, "en") == 0) {
        settings().langEn = true;
    } else if (std::strcmp(a, "pt") == 0) {
        settings().langEn = false;
    } else {
        Serial.println("uso: lang pt|en");
        return;
    }
    settings_save();
    lang_set_en(settings().langEn);
    app_go(app_page()); // reconstroi a pagina atual no idioma novo (spec 7.7)
    Serial.printf("idioma: %s\n", a);
}

static void cmd_tz(const char *a) {
    char *end = nullptr;
    const long min = std::strtol(a, &end, 10);
    if (end == a || min < -720 || min > 840) {
        Serial.println("uso: tz <minutos>   ex.: tz -180 (UTC-3), tz 330 (UTC+5:30)");
        return;
    }
    settings().tzMin = (int)min;
    settings_save();
    clock_set_tz_offset((int32_t)min * 60);
    app_go(app_page());
    Serial.printf("fuso: %ld min\n", min);
}

static void cmd_mem(const char *) {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    Serial.printf("lvgl: %u de %u KB livres (frag %u%%)\n", (unsigned)(m.free_size / 1024),
                  (unsigned)(m.total_size / 1024), (unsigned)m.frag_pct);
    Serial.printf("heap interno: %u KB   psram: %u KB\n",
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    Serial.printf("worker: %u bytes de pilha livres (minimo)\n",
                  (unsigned)net_stack_free_bytes());
}

static void cmd_reboot(const char *) { ESP.restart(); }

static void cmd_wifi(const char *) {
    if (wifi().isConnected())
        Serial.printf("conectado a '%s'  IP %s  %d dBm\n", wifi().getSSID().c_str(),
                      wifi().getIP().c_str(), wifi().getRSSI());
    else
        Serial.println("desconectado");
    Serial.printf("redes salvas: %d   hora: %s\n", wifi().getSavedCount(),
                  clock_has_time() ? "sincronizada" : "pendente");
}

static void cmd_wififorget(const char *) {
    wifi().forgetAll();
    WiFi.disconnect(true, true); // tambem apaga a rede que o esp-idf guarda em nvs.net80211
    Serial.println("reiniciando...");
    delay(200);
    ESP.restart();
}

static void cmd_portal(const char *) {
    app_go(Page::Pair); // reabre o pareamento (trocar de conta, testar o portal)
    Serial.println("pagina de pareamento aberta");
}

static void cmd_lists(const char *) {
    app_go(Page::Lists);
    Serial.println("selecao de listas aberta");
}

static void cmd_go(const char *a) {
    if (flow_next() != Page::Main) {
        Serial.println("go: aparelho ainda nao esta pronto");
        return;
    }
    if (std::strcmp(a, "foco") == 0) { shell_go(SCR_FOCUS); app_go(Page::Main); }
    else if (std::strcmp(a, "hoje") == 0) { shell_go(SCR_TODAY); app_go(Page::Main); }
    else if (std::strcmp(a, "status") == 0) { shell_go(SCR_STATUS); app_go(Page::Main); }
    else if (std::strcmp(a, "relogio") == 0) app_go(Page::Clock);
    else if (std::strcmp(a, "settings") == 0) app_go(Page::Settings);
    else {
        Serial.println("uso: go foco|hoje|status|relogio|settings");
        return;
    }
    Serial.printf("indo para %s\n", a);
}

// ---- Hora ----

static void wait_time_build(lv_obj_t *scr) {
    page_message_set(TRS("Sincronizando hora", "Syncing time"),
                     TRS("Sem a hora certa não existe \"hoje\".",
                         "Without the right time there is no \"today\"."));
    page_message_build(scr);
}

static void wait_time_tick() {
    if (clock_has_time()) app_advance();
}

void app_wifi_connected() {
    s_wifiReady = true;
    clock_begin_sntp();
    app_back(); // volta ao Settings, ou segue o fluxo no boot
}

// ---- Boot ----

static void boot_wifi_tick(const char *ssid, int idx, int total) {
    char buf[96];
    snprintf(buf, sizeof(buf), TRS("Conectando a %s (%d/%d)…", "Connecting to %s (%d/%d)…"),
             ssid, idx, total);
    page_message_update(buf);
    lv_timer_handler();
}

// O que precisa acontecer depois do display e antes da primeira pagina. Com
// rede salva, conecta aqui mesmo, com o progresso na pagina de boot.
static void boot_sequence() {
    wifi().begin();
    if (wifi().getSavedCount() > 0 &&
        wifi().autoConnect(WIFI_CONNECT_TIMEOUT_MS, boot_wifi_tick)) {
        s_wifiReady = true;
        clock_begin_sntp();
    }
    app_advance();
}

void app_begin() {
    settings_load();
    display_set_brightness(settings().briIdx);
    lang_set_en(settings().langEn);
    clock_set_tz_offset((int32_t)settings().tzMin * 60);

    app_register(Page::Boot, {page_message_build, nullptr, page_message_leave});
    app_register(Page::Main, {main_build, shell_tick, shell_leave});
    app_register(Page::Wifi, {page_wifi_build, page_wifi_tick, page_wifi_leave});
    app_register(Page::WaitTime, {wait_time_build, wait_time_tick, page_message_leave});
    app_register(Page::Pair, {page_pair_build, page_pair_tick, page_pair_leave});
    app_register(Page::PinSet, {page_pin_set_build, page_pin_tick, page_pin_leave});
    app_register(Page::PinEnter, {page_pin_enter_build, page_pin_tick, page_pin_leave});
    app_register(Page::Lists, {page_lists_build, page_lists_tick, page_lists_leave});
    app_register(Page::Clock, {page_clock_build, page_clock_tick, page_clock_leave});
    app_register(Page::Settings, {page_settings_build, page_settings_tick, page_settings_leave});
    app_register(Page::PinAdmin, {page_pin_admin_build, page_pin_tick, page_pin_leave});

    console_add("lang", "lang pt|en  idioma da interface", cmd_lang);
    console_add("tz", "tz <min>    fuso em minutos (-180 = UTC-3)", cmd_tz);
    console_add("mem", "memoria livre (LVGL, interna, PSRAM)", cmd_mem);
    console_add("reboot", "reinicia o aparelho", cmd_reboot);
    console_add("wifi", "estado do WiFi e da hora", cmd_wifi);
    console_add("wififorget", "apaga as redes salvas e reinicia", cmd_wififorget);
    console_add("portal", "abre a pagina de pareamento", cmd_portal);
    console_add("lists", "abre a selecao de listas", cmd_lists);
    console_add("go", "go foco|hoje|status|relogio|settings  navega sem tocar", cmd_go);
    screenshot_begin();
    page_pin_begin();

    sync_begin();    // o worker precisa existir antes de qualquer pagina pedir rede
    pairing_begin();
    shell_set_refresh_handler([]() { sync_refresh_now(); });
    shell_set_clock_handler([]() { app_go(Page::Clock); });
    shell_set_counter_handler([]() { pomo_app_toggle_expanded(); });
    shell_set_gear_handler([]() { app_go(Page::Settings); });
    pomo_app_begin();

    page_message_set("TickTick Screen", "v" FW_VERSION);
    app_go(Page::Boot);
    apply_switch();
    lv_refr_now(nullptr);

    boot_sequence();
}

void app_loop() {
    console_poll();
    clock_poll();
    sync_tick();
    pairing_tick();
    apply_switch();
    if (s_ops[(int)s_page].tick) s_ops[(int)s_page].tick();
    pomo_app_tick(); // depois da pagina: a sobreposicao fica por cima dela
}
