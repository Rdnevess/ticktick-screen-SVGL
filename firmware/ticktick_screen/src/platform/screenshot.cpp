#include "screenshot.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstring>

#include "../../config.h"
#include "console.h"
#include "display.h"

namespace {

const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Junta bytes em grupos de 57 e imprime cada grupo como uma linha de 76.
struct B64Lines {
    uint8_t in[57];
    int n = 0;

    void put(uint8_t b) {
        in[n++] = b;
        if (n == (int)sizeof(in)) flush();
    }

    void flush() {
        if (n == 0) return;
        char out[80];
        int o = 0;
        for (int i = 0; i < n; i += 3) {
            const uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) |
                               (i + 2 < n ? (uint32_t)in[i + 2] : 0);
            out[o++] = B64[(v >> 18) & 63];
            out[o++] = B64[(v >> 12) & 63];
            out[o++] = i + 1 < n ? B64[(v >> 6) & 63] : '=';
            out[o++] = i + 2 < n ? B64[v & 63] : '=';
        }
        out[o] = '\0';
        Serial.println(out);
        n = 0;
    }
};

void put_u16(B64Lines &b, uint16_t v) {
    b.put((uint8_t)(v & 0xFF));
    b.put((uint8_t)(v >> 8));
}

bool valid_name(const char *s) {
    const size_t len = std::strlen(s);
    if (len == 0 || len > 32) return false;
    for (size_t i = 0; i < len; i++) {
        const char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

void cmd_shot(const char *args) {
    if (!valid_name(args)) {
        Serial.println("uso: shot <nome>  (a-z 0-9 - _, ate 32)");
        return;
    }
    // Redesenha tudo agora: o buffer passa a ter o quadro inteiro, com o layer_top.
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(nullptr);
    const uint16_t *fb = display_framebuffer();
    if (!fb) {
        Serial.println("sem framebuffer");
        return;
    }
    const uint32_t total = (uint32_t)SCREEN_WIDTH * SCREEN_HEIGHT;
    Serial.printf("-----BEGIN SHOT %s %d %d-----\n", args, SCREEN_WIDTH, SCREEN_HEIGHT);
    B64Lines out;
    uint32_t i = 0;
    while (i < total) {
        const uint16_t px = fb[i];
        uint32_t j = i + 1;
        while (j < total && fb[j] == px && j - i < 65535) j++;
        put_u16(out, (uint16_t)(j - i));
        put_u16(out, px);
        i = j;
    }
    out.flush();
    Serial.println("-----END SHOT-----");
}

} // namespace

void screenshot_begin() {
    console_add("shot", "shot <nome>  manda a tela pela serial (tools/shot2png.py)", cmd_shot);
}
