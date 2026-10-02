#include "clock_format.h"

#include <cstdio>
#include <cstring>

static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

void clock_hhmm(int64_t localEpoch, bool hasTime, char *out, size_t n) {
    if (n == 0) return;
    if (!hasTime) {
        std::snprintf(out, n, "--:--");
        return;
    }
    const int64_t secOfDay = localEpoch - floor_div(localEpoch, 86400) * 86400;
    std::snprintf(out, n, "%02d:%02d", (int)(secOfDay / 3600), (int)((secOfDay / 60) % 60));
}

// Algoritmo de Howard Hinnant (civil_from_days), o par do days_from_civil.
void civil_from_days(int64_t z, int *y, unsigned *m, unsigned *d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);                         // [0, 146096]
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; // [0, 399]
    const int64_t yy = (int64_t)yoe + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100); // [0, 365]
    const unsigned mp = (5 * doy + 2) / 153;                      // [0, 11]
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int)(yy + (*m <= 2 ? 1 : 0));
}

int weekday_from_days(int64_t z) {
    // 01/01/1970 foi quinta (4).
    const int64_t w = (z + 4) % 7;
    return (int)(w < 0 ? w + 7 : w);
}

static const char *const WEEK_PT[7] = {"domingo", "segunda-feira", "terça-feira",
                                       "quarta-feira", "quinta-feira", "sexta-feira",
                                       "sábado"};
static const char *const WEEK_EN[7] = {"Sunday", "Monday", "Tuesday", "Wednesday",
                                       "Thursday", "Friday", "Saturday"};
static const char *const MONTH_PT[12] = {"janeiro", "fevereiro", "março", "abril",
                                         "maio", "junho", "julho", "agosto",
                                         "setembro", "outubro", "novembro", "dezembro"};
static const char *const MONTH_EN[12] = {"January", "February", "March", "April",
                                         "May", "June", "July", "August",
                                         "September", "October", "November", "December"};

size_t clock_long_date(int64_t localEpoch, bool en, char *out, size_t n) {
    if (n == 0) return 0;
    const int64_t days = floor_div(localEpoch, 86400);
    int y = 0;
    unsigned m = 0, d = 0;
    civil_from_days(days, &y, &m, &d);
    const int wd = weekday_from_days(days);
    int w = en ? std::snprintf(out, n, "%s, %s %u", WEEK_EN[wd], MONTH_EN[m - 1], d)
               : std::snprintf(out, n, "%s, %u de %s", WEEK_PT[wd], d, MONTH_PT[m - 1]);
    if (w < 0) {
        out[0] = '\0';
        return 0;
    }
    if ((size_t)w >= n) {
        // snprintf cortou: recua ate a fronteira de caractere UTF-8.
        size_t len = n - 1;
        while (len > 0 && (((unsigned char)out[len]) & 0xC0) == 0x80) len--;
        out[len] = '\0';
        return len;
    }
    return (size_t)w;
}

// Volta pelos quatro quadrantes em passos curtos; o ultimo emenda no primeiro.
static const int8_t DRIFT[CLOCK_DRIFT_PERIOD][2] = {
    {0, 0},  {6, -4},  {10, 2}, {4, 8},   {-4, 6},   {-10, 0},
    {-6, -8}, {2, -6}, {8, 6},  {-2, 2},  {-8, 8},   {-2, -2},
};

void clock_drift(int64_t minute, int *dx, int *dy) {
    int64_t i = minute % CLOCK_DRIFT_PERIOD;
    if (i < 0) i += CLOCK_DRIFT_PERIOD;
    *dx = DRIFT[i][0];
    *dy = DRIFT[i][1];
}
