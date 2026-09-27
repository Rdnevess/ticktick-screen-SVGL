#include "display.h"

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#include "../../config.h"
#include "touch.h"

static Arduino_Canvas *s_gfx = nullptr;
static uint16_t *s_canvas_fb = nullptr;
static lv_color_t *s_lvbuf = nullptr;
static AXS15231B_Touch s_touch(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR,
                               TOUCH_ROTATION);

// Flush: LVGL 480x320 -> gira 270 CW -> Canvas 320x480 -> QSPI.
// O Canvas fica em rotation=0 e a rotacao e feita aqui, na mao: e o unico
// arranjo em que as cores saem certas nesta placa.
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area,
                          uint8_t *px_map) {
    (void)area; // render mode FULL: a area e sempre a tela inteira
    uint16_t *src = (uint16_t *)px_map;
    for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {
        uint16_t *src_row = src + ly * SCREEN_WIDTH;
        for (int lx = 0; lx < SCREEN_WIDTH; lx++) {
            s_canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];
        }
    }
    s_gfx->flush();
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    uint16_t x = 0, y = 0;
    if (s_touch.touched()) {
        s_touch.readData(&x, &y);
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Mesmos tres niveis do claude-usage-stick (duty de 8 bits).
static const uint8_t BRI_LEVELS[3] = {60, 160, 255};

void display_set_brightness(int level) {
    if (level < 0 || level > 2) level = 1;
    ledcWrite(TFT_BL, BRI_LEVELS[level]);
}

bool display_begin() {
    Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0,
                                                 TFT_SDA1, TFT_SDA2, TFT_SDA3);
    Arduino_GFX *panel =
        new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
    s_gfx = new Arduino_Canvas(320, 480, panel, 0, 0, 0); // rotation = 0
    if (!s_gfx->begin(QSPI_FREQ)) {
        Serial.println("FATAL: display init falhou");
        return false;
    }
    s_gfx->fillScreen(0x0000);
    s_gfx->flush();
    s_canvas_fb = s_gfx->getFramebuffer();

    // Backlight por PWM (brilho ajustavel no Settings). Comeca no medio; o app
    // aplica o nivel salvo depois de carregar as configuracoes.
    ledcAttach(TFT_BL, 5000, 8);
    display_set_brightness(1);

    if (!s_touch.begin()) Serial.println("AVISO: touch nao respondeu no I2C");

    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
    lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
        bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        Serial.println("FATAL: alloc PSRAM falhou (FQBN precisa de PSRAM=opi)");
        return false;
    }
    s_lvbuf = buf;

    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf, NULL, bufSize,
                           LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    return true;
}

void display_tick() { lv_task_handler(); }

const uint16_t *display_framebuffer() { return (const uint16_t *)s_lvbuf; }
