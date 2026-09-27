// Valores que o Settings oferece (adendo do Plano C, secao 4) e o passo de
// cada controle. Nucleo puro, testado no PC.
#ifndef CORE_SETTINGS_VALUES_H
#define CORE_SETTINGS_VALUES_H

#include <cstddef>

constexpr int TZ_MIN_MINUTES = -720;  // UTC-12:00
constexpr int TZ_MAX_MINUTES = 840;   // UTC+14:00
constexpr int TZ_STEP_MINUTES = 15;

// Proximo valor da lista na direcao `dir` (+1 ou -1). Satura nas pontas. Um
// valor fora da lista vai para o vizinho mais proximo naquela direcao.
int poll_step(int cur, int dir); // 1, 2, 5, 10, 15, 30 min
int pomo_step(int cur, int dir); // 15, 20, 25, 30, 45, 50 min

// Fuso em passos de 15 min, limitado a UTC-12:00 .. UTC+14:00. Um valor fora
// do passo e alinhado na direcao pedida.
int tz_step(int curMin, int dir);

// "UTC-3:00", "UTC+5:30" (hifen ASCII: a fonte nao tem o sinal de menos).
void tz_format(int min, char *out, size_t n);

#endif // CORE_SETTINGS_VALUES_H
