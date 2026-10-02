// Hora do header e data por extenso da tela do relogio (adendo do Plano C,
// secao 2.4). Nucleo puro: recebe o epoch LOCAL (UTC + fuso) e o idioma.
#ifndef CORE_CLOCK_FORMAT_H
#define CORE_CLOCK_FORMAT_H

#include <cstddef>
#include <cstdint>

// "HH:MM" em 24 h; hasTime = false devolve "--:--". Precisa de 6 bytes.
void clock_hhmm(int64_t localEpoch, bool hasTime, char *out, size_t n);

// Data civil de um dia contado desde 01/01/1970 (inverso de days_from_civil).
void civil_from_days(int64_t days, int *y, unsigned *m, unsigned *d);

// 0 = domingo ... 6 = sabado.
int weekday_from_days(int64_t days);

// "sábado, 26 de setembro" (en = false) ou "Saturday, September 26". Trunca
// para caber em n (sempre termina em '\0') e devolve o tamanho escrito.
size_t clock_long_date(int64_t localEpoch, bool en, char *out, size_t n);

// Deslocamento da tela do relogio contra marcacao do painel: um passo de um
// ciclo fixo a cada minuto (minuto pode ser negativo). Sempre dentro de
// +-CLOCK_DRIFT_X / +-CLOCK_DRIFT_Y, nunca igual ao do minuto anterior.
#define CLOCK_DRIFT_X 10
#define CLOCK_DRIFT_Y 8
#define CLOCK_DRIFT_PERIOD 12
void clock_drift(int64_t minute, int *dx, int *dy);

#endif // CORE_CLOCK_FORMAT_H
