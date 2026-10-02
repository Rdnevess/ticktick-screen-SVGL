// Configuracao persistente (NVS "cfg", spec 6.3).
//
// Uma struct so, carregada no boot e salva inteira quando algo muda. Mudancas
// sao raras (um toque em Settings, um pin) e a NVS nivela o desgaste.
#ifndef PLATFORM_SETTINGS_H
#define PLATFORM_SETTINGS_H

#include "../../config.h"
#include "../core/task_order.h"

struct Settings {
    int pollMin;      // intervalo do refresh, em minutos
    int pomoMin;      // duracao do pomodoro (Plano C)
    bool langEn;
    int tzMin;        // fuso em minutos (UTC-3 = -180)
    bool tlsInsecure; // true = setInsecure() em vez da cadeia embutida (spec 6.5)
    int briIdx;       // brilho da tela: 0 baixo, 1 medio, 2 alto
    bool idleClock;   // relogio como tela de descanso (app.cpp, idle_tick)
    int listCount;
    char lists[MAX_LISTS][TASK_ID_BYTES + 1];
    PinSet pins;
};

Settings &settings(); // a instancia unica do aparelho
void settings_load();
void settings_save();
void settings_reset(); // volta aos padroes e apaga o namespace

#endif // PLATFORM_SETTINGS_H
