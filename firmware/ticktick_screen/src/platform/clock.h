// Hora do aparelho. O nucleo trabalha em epoch LOCAL (= UTC + offset), e este
// modulo e quem fornece os dois numeros.
//
// clock_has_time() fica false ate o primeiro SNTP (ou um clock_set_time).
#ifndef PLATFORM_CLOCK_H
#define PLATFORM_CLOCK_H

#include <stdint.h>

void clock_set_tz_offset(int32_t sec);
int32_t clock_tz_offset();

// Milissegundos desde o boot, em 64 bits. Nao usar millis() para medir tempo
// longo: ele e de 32 bits e volta a zero a cada ~49,7 dias. E tambem a fonte
// do `nowMs` que o pomodoro recebe.
int64_t clock_uptime_ms();

// Fixa a referencia: `epochUtc` era a hora UTC no instante `atMillis`
// (clock_uptime_ms()). Dai em diante a hora avanca com o relogio interno.
void clock_set_time(int64_t epochUtc, int64_t atMillis);
bool clock_has_time();

int64_t clock_now_utc();
int64_t clock_now_local();

// SNTP (spec 5.2: sem hora nao existe "hoje", e o TLS nao valida certificado).
// clock_begin_sntp() depois do WiFi. clock_poll() no loop copia a hora do
// sistema para a referencia assim que o SNTP responde, e de novo a cada
// 10 min para acompanhar os ajustes do servidor.
void clock_begin_sntp();
void clock_poll();

#endif // PLATFORM_CLOCK_H
