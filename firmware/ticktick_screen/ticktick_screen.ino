/**
 * TickTick Screen — firmware principal.
 * Placa: Guition JC4832W535 (ESP32-S3, AXS15231B QSPI), 480x320 paisagem.
 *
 * Este arquivo fica curto de proposito: os prototipos automaticos que o
 * pre-processador do Arduino gera para o .ino quebram com tipos proprios. A
 * logica mora em src/, e o orquestrador e src/app/app.cpp.
 */
#include <Arduino.h>
#include <lvgl.h>

#include "config.h"
#include "src/app/app.h"
#include "src/platform/display.h"

void setup() {
    Serial.begin(115200);
    delay(1200);
    Serial.printf("\n=== TickTick Screen %s ===\n", FW_VERSION);

    if (!display_begin()) {
        Serial.println("FATAL: display nao inicializou");
        while (true) delay(1000);
    }
    app_begin();
    Serial.println("pronto. 'help' no console lista os comandos.");
}

void loop() {
    app_loop();
    display_tick();
    delay(5);
}
