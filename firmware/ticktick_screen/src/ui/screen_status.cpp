#include "screen_status.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../../config.h"
#include "../app/app.h"
#include "../app/sync.h"
#include "../core/clock_format.h"
#include "../core/settings_values.h"
#include "../net/net_worker.h"
#include "../platform/clock.h"
#include "../platform/settings.h"
#include "../platform/wifi_manager.h"
#include "i18n.h"
#include "shell.h"
#include "theme.h"

static const int ROW_STEP = 24;

// Uma linha rotulo/valor em 14 px. O valor e copiado pelo label.
static void row(lv_obj_t *parent, const char *label, const char *value, int y,
                uint32_t color = COL_TEXT) {
    lv_obj_t *l = ui_label(parent, label, &font_pt_14, COL_DIM);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 14, y);
    lv_obj_t *v = ui_label(parent, value, &font_pt_14, color);
    lv_obj_set_width(v, SCREEN_WIDTH - 150 - 14);
    lv_obj_set_height(v, lv_font_get_line_height(&font_pt_14));
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    lv_obj_align(v, LV_ALIGN_TOP_LEFT, 150, y);
}

static void on_refresh(lv_event_t *e) {
    (void)e;
    shell_toast(sync_refresh_now() ? TRS("Atualizando…", "Refreshing…")
                                   : TRS("Agora não dá: sem rede, hora ou lista.",
                                         "Not now: no network, time or list."));
}

static void on_portal(lv_event_t *e) {
    (void)e;
    app_open_child(Page::Pair); // mesma pagina de "Parear de novo"
}

void status_build(lv_obj_t *parent) {
    char buf[96];
    char t[8];
    char tz[16];
    int y = 6;
    const bool online = wifi().isConnected();
    const bool demo = sync_demo(); // item 2 da revisao: nunca mostrar a rede real na captura

    if (online) {
        snprintf(buf, sizeof(buf), "%s · %s · %d dBm",
                 demo ? sync_demo_ssid() : wifi().getSSID().c_str(),
                 demo ? sync_demo_ip() : wifi().getIP().c_str(), wifi().getRSSI());
        row(parent, TRS("Rede", "Network"), buf, y);
    } else {
        row(parent, TRS("Rede", "Network"), TRS("desconectado", "disconnected"), y, COL_LATE);
    }
    y += ROW_STEP;

    tz_format(settings().tzMin, tz, sizeof(tz));
    clock_hhmm(clock_now_local(), clock_has_time(), t, sizeof(t));
    snprintf(buf, sizeof(buf), "%s · %s", clock_has_time() ? t : TRS("sincronizando…", "syncing…"), tz);
    row(parent, TRS("Hora", "Clock"), buf, y, clock_has_time() ? COL_TEXT : COL_MED);
    y += ROW_STEP;

    if (!sync_has_data()) {
        row(parent, TRS("Último refresh", "Last refresh"), TRS("ainda não", "not yet"), y, COL_DIM);
    } else {
        clock_hhmm(sync_last_ok_utc() + clock_tz_offset(), true, t, sizeof(t));
        if (sync_stale())
            snprintf(buf, sizeof(buf), TRS("%s · depois falhou (%d)", "%s · then failed (%d)"), t,
                     sync_last_http());
        else
            snprintf(buf, sizeof(buf), "%s · ok", t);
        row(parent, TRS("Último refresh", "Last refresh"), buf, y, sync_stale() ? COL_MED : COL_TEXT);
    }
    y += ROW_STEP;

    snprintf(buf, sizeof(buf), TRS("%d marcada(s) · a cada %d min", "%d selected · every %d min"),
             settings().listCount, settings().pollMin);
    row(parent, TRS("Listas", "Lists"), buf, y);
    y += ROW_STEP;

    Creds c{};
    if (net_get_creds(c) && c.exp > 0 && clock_has_time()) {
        const int64_t days = (c.exp - clock_now_utc()) / 86400;
        if (days < 0) {
            row(parent, TRS("Token", "Token"), TRS("expirado", "expired"), y, COL_LATE);
        } else {
            snprintf(buf, sizeof(buf), TRS("válido por mais %d dias", "valid for %d more days"), (int)days);
            row(parent, TRS("Token", "Token"), buf, y, days < 7 ? COL_MED : COL_TEXT);
        }
    } else {
        row(parent, TRS("Token", "Token"),
            net_has_creds() ? TRS("prazo desconhecido", "expiry unknown") : TRS("não pareado", "not paired"),
            y, COL_DIM);
    }
    creds_wipe(c);
    y += ROW_STEP;

    // Spec 7.4: a URL do portal, com o IP para quando o mDNS falhar.
    if (online) snprintf(buf, sizeof(buf), MDNS_NAME ".local · %s", demo ? sync_demo_ip() : wifi().getIP().c_str());
    else snprintf(buf, sizeof(buf), "%s", TRS("sem rede", "no network"));
    row(parent, TRS("Portal", "Portal"), buf, y, online ? COL_TEXT : COL_DIM);
    y += ROW_STEP;

    snprintf(buf, sizeof(buf), "v%s · RAM %u KB · PSRAM %u KB", FW_VERSION,
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    row(parent, TRS("Firmware", "Firmware"), buf, y);

    lv_obj_t *refresh = ui_button(parent, TRS("Atualizar agora", "Refresh now"), 220, 44, COL_ACCENT);
    lv_obj_align(refresh, LV_ALIGN_BOTTOM_LEFT, 12, -6);
    lv_obj_add_event_cb(refresh, on_refresh, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *portal = ui_button(parent, TRS("Abrir portal", "Open portal"), 220, 44, COL_SURFACE);
    lv_obj_align(portal, LV_ALIGN_BOTTOM_RIGHT, -12, -6);
    lv_obj_add_event_cb(portal, on_portal, LV_EVENT_CLICKED, nullptr);
}
