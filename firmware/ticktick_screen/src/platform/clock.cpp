#include "clock.h"

#include <Arduino.h> // configTime
#include <esp_timer.h>
#include <time.h>

#include "../../config.h"

static int32_t s_tzOffset = 0;
static int64_t s_epochRef = 0;   // hora UTC da referencia
static int64_t s_millisRef = 0;  // clock_uptime_ms() no instante da referencia
static bool s_hasTime = false;

void clock_set_tz_offset(int32_t sec) { s_tzOffset = sec; }
int32_t clock_tz_offset() { return s_tzOffset; }

void clock_set_time(int64_t epochUtc, int64_t atMillis) {
    s_epochRef = epochUtc;
    s_millisRef = atMillis;
    s_hasTime = true;
}

bool clock_has_time() { return s_hasTime; }

int64_t clock_uptime_ms() { return esp_timer_get_time() / 1000; }

int64_t clock_now_utc() {
    if (!s_hasTime) return 0;
    return s_epochRef + (clock_uptime_ms() - s_millisRef) / 1000;
}

int64_t clock_now_local() {
    if (!s_hasTime) return 0;
    return clock_now_utc() + s_tzOffset;
}

static bool s_sntpStarted = false;
static int64_t s_lastPollMs = 0;

void clock_begin_sntp() {
    if (s_sntpStarted) return;
    // UTC: o fuso e aplicado por nos (clock_now_local), nao pela libc.
    configTime(0, 0, NTP_SERVER_1, NTP_SERVER_2);
    s_sntpStarted = true;
}

void clock_poll() {
    if (!s_sntpStarted) return;
    const int64_t nowMs = clock_uptime_ms();
    const int64_t every = s_hasTime ? 10LL * 60 * 1000 : 500; // antes da 1a: insiste
    if (s_lastPollMs != 0 && nowMs - s_lastPollMs < every) return;
    s_lastPollMs = nowMs;

    const time_t t = time(nullptr);
    if (t < 1704067200) return; // antes de 2024: o SNTP ainda nao respondeu
    clock_set_time((int64_t)t, clock_uptime_ms());
}
