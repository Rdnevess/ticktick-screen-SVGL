#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// TickTick Screen — Guition JC4832W535 (ESP32-S3, AXS15231B)
// ============================================================

#define FW_VERSION "0.1"

// ── Display QSPI (AXS15231B) ─────────────────────────────
#define TFT_CS   45
#define TFT_SCK  47
#define TFT_SDA0 21
#define TFT_SDA1 48
#define TFT_SDA2 40
#define TFT_SDA3 39
#define TFT_BL   1
#define TFT_TE   38

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 320
#define QSPI_FREQ     40000000UL

// ── Touch I2C (AXS15231B) ────────────────────────────────
#define TOUCH_SDA  4
#define TOUCH_SCL  8
#define TOUCH_INT  3
#define TOUCH_ADDR 0x3B
// rotation=3 = USB a esquerda; tem de casar com o flush 270 CW do display.
#define TOUCH_ROTATION 3

// ── Layout comum das telas (spec 7.1) ────────────────────
#define HEADER_H     40
#define REFRESHBAR_H 4
#define CONTENT_H    252
#define DOTS_H       24
// 40 + 4 + 252 + 24 = 320

// ── NVS ──────────────────────────────────────────────────
#define NVS_CFG "cfg"
#define NVS_TT  "tt"

// ── Configuracao padrao (NVS "cfg", ver platform/settings) ──
#define DEFAULT_POLL_MIN 5
#define DEFAULT_POMO_MIN 25
#define DEFAULT_TZ_MIN   (-180)   // UTC-3
#define MAX_LISTS        16       // listas marcadas: cada uma e um GET por ciclo
#define IDLE_CLOCK_MIN   30       // sem toque na principal: abre o relogio (descanso)

// ── WiFi e hora ──────────────────────────────────────────
#define NVS_WIFI                "wifi"
#define WIFI_CONNECT_TIMEOUT_MS 10000   // por rede salva, no boot
#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "time.google.com"

// ── TickTick ─────────────────────────────────────────────
// A Entrada nao aparece em GET /project; GET /project/inbox/data foi testado
// e responde HTTP 200.
#define INBOX_SUPPORTED 1

// ── Rede ─────────────────────────────────────────────────
#define API_BASE        "https://api.ticktick.com/open/v1"
#define OAUTH_TOKEN_URL "https://ticktick.com/oauth/token"
#define HTTP_TIMEOUT_MS 12000
#define NET_TASK_STACK  16384   // TLS + parse; o heap do mbedTLS e a parte
#define NET_TASK_CORE   0       // o loop() do Arduino roda no core 1

// ── Portal de pareamento (spec 6.2) ──────────────────────
#define MDNS_NAME          "ticktick-screen"   // http://ticktick-screen.local
#define PORTAL_TIMEOUT_MIN 15                  // desliga sozinho se ninguem parear

// ── PIN (cifra das credenciais, spec 6.2) ────────────────
#define PIN_LEN          4
#define MAX_PIN_ATTEMPTS 10     // na decima errada, as credenciais sao apagadas
#define KDF_ROUNDS       10000  // SHA-256 iterado: ~0,2 s por tentativa no S3
#define LOCKOUT_BASE_SEC 30     // espera dobra a cada erro, teto de 1 h

#endif // CONFIG_H
