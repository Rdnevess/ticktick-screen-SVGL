#include "tinytest.h"
#include "clock_format.h"
#include "day_filter.h"

#include <cstring>
#include <string>

// 24/09/2026 e o dia 20720 desde a epoca (quinta-feira).
static const int64_t D0924 = 20720LL * 86400;

TEST(civil_from_days_e_o_inverso_de_days_from_civil) {
    int y = 0;
    unsigned m = 0, d = 0;
    civil_from_days(0, &y, &m, &d);
    CHECK_EQ_INT(y, 1970); CHECK_EQ_INT(m, 1); CHECK_EQ_INT(d, 1);
    civil_from_days(20720, &y, &m, &d);
    CHECK_EQ_INT(y, 2026); CHECK_EQ_INT(m, 9); CHECK_EQ_INT(d, 24);
    civil_from_days(19782, &y, &m, &d); // bissexto
    CHECK_EQ_INT(y, 2024); CHECK_EQ_INT(m, 2); CHECK_EQ_INT(d, 29);
    for (int64_t z = 20000; z < 21000; z++) { // ida e volta num intervalo inteiro
        civil_from_days(z, &y, &m, &d);
        CHECK_EQ_INT(days_from_civil(y, m, d), z);
    }
}

TEST(dia_da_semana_conhecido) {
    CHECK_EQ_INT(weekday_from_days(0), 4);     // 01/01/1970, quinta
    CHECK_EQ_INT(weekday_from_days(20720), 4); // 24/09/2026, quinta
    CHECK_EQ_INT(weekday_from_days(20722), 6); // 26/09/2026, sabado
    CHECK_EQ_INT(weekday_from_days(20723), 0); // 27/09/2026, domingo
}

TEST(hhmm_com_e_sem_hora) {
    char b[8];
    clock_hhmm(D0924 + 14 * 3600 + 32 * 60 + 59, true, b, sizeof(b));
    CHECK_EQ_STR(b, "14:32");
    clock_hhmm(D0924 + 5 * 60, true, b, sizeof(b));
    CHECK_EQ_STR(b, "00:05");
    clock_hhmm(D0924, false, b, sizeof(b));
    CHECK_EQ_STR(b, "--:--");
}

TEST(data_por_extenso_em_portugues) {
    char b[64];
    clock_long_date(D0924 + 2 * 86400 + 10 * 3600, false, b, sizeof(b));
    CHECK_EQ_STR(b, "sábado, 26 de setembro");
    // 2027-01-01 e dia 20819, sexta
    clock_long_date(20819LL * 86400, false, b, sizeof(b));
    CHECK_EQ_STR(b, "sexta-feira, 1 de janeiro");
    // 2026-03-02 (dia 20514), segunda
    clock_long_date(20514LL * 86400 + 60, false, b, sizeof(b));
    CHECK_EQ_STR(b, "segunda-feira, 2 de março");
}

TEST(data_por_extenso_em_ingles) {
    char b[64];
    clock_long_date(D0924 + 2 * 86400, true, b, sizeof(b));
    CHECK_EQ_STR(b, "Saturday, September 26");
    clock_long_date(20819LL * 86400, true, b, sizeof(b));
    CHECK_EQ_STR(b, "Friday, January 1");
}

TEST(data_em_buffer_pequeno_nao_estoura) {
    char b[8];
    const size_t n = clock_long_date(D0924, false, b, sizeof(b));
    CHECK(n < sizeof(b));
    CHECK_EQ_INT(std::strlen(b), n);
}

TEST(deriva_do_relogio_fica_nos_limites) {
    for (int64_t m = -100; m < 2000; m++) {
        int dx = 99, dy = 99;
        clock_drift(m, &dx, &dy);
        CHECK(dx >= -CLOCK_DRIFT_X && dx <= CLOCK_DRIFT_X);
        CHECK(dy >= -CLOCK_DRIFT_Y && dy <= CLOCK_DRIFT_Y);
    }
}

TEST(deriva_do_relogio_muda_a_cada_minuto) {
    for (int64_t m = -100; m < 2000; m++) {
        int ax = 0, ay = 0, bx = 0, by = 0;
        clock_drift(m, &ax, &ay);
        clock_drift(m + 1, &bx, &by);
        CHECK(ax != bx || ay != by);
    }
}

TEST(deriva_do_relogio_e_ciclica_e_espalhada) {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    clock_drift(5, &x0, &y0);
    clock_drift(5 + CLOCK_DRIFT_PERIOD, &x1, &y1);
    CHECK_EQ_INT(x0, x1);
    CHECK_EQ_INT(y0, y1);
    // Cobre os quatro quadrantes: nenhum pixel fica aceso sempre do mesmo lado.
    bool q[4] = {false, false, false, false};
    for (int64_t m = 0; m < CLOCK_DRIFT_PERIOD; m++) {
        int dx = 0, dy = 0;
        clock_drift(m, &dx, &dy);
        if (dx > 0 && dy > 0) q[0] = true;
        if (dx < 0 && dy > 0) q[1] = true;
        if (dx < 0 && dy < 0) q[2] = true;
        if (dx > 0 && dy < 0) q[3] = true;
    }
    CHECK(q[0] && q[1] && q[2] && q[3]);
}
