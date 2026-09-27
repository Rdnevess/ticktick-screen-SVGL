#include "day_filter.h"

#include <cstring>

int64_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);                      // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          // [0, 146096]
    return era * 146097 + (int64_t)doe - 719468;
}

// Le exatamente `n` digitos. Avanca `p`. Devolve false se faltar digito.
static bool read_int(const char *&p, int n, int *out) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (*p - '0');
        p++;
    }
    *out = v;
    return true;
}

static int days_in_month(int y, int m) {
    static const int k[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2) {
        const bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
        return leap ? 29 : 28;
    }
    return k[m - 1];
}

// Recusa 31/02 e companhia: days_from_civil normalizaria para marco em silencio.
static bool valid_ymd(int y, int m, int d) {
    if (y < 1970 || y > 2999) return false;
    if (m < 1 || m > 12) return false;
    if (d < 1 || d > days_in_month(y, m)) return false;
    return true;
}

bool parse_iso8601_date(const char *s, int *y, int *m, int *d) {
    if (s == nullptr || std::strlen(s) < 10) return false;
    const char *p = s;
    int yy = 0, mm = 0, dd = 0;
    if (!read_int(p, 4, &yy) || *p++ != '-') return false;
    if (!read_int(p, 2, &mm) || *p++ != '-') return false;
    if (!read_int(p, 2, &dd)) return false;
    if (!valid_ymd(yy, mm, dd)) return false;
    *y = yy; *m = mm; *d = dd;
    return true;
}

bool parse_iso8601_utc(const char *s, int64_t *outUtc) {
    int y = 0, m = 0, d = 0;
    if (!parse_iso8601_date(s, &y, &m, &d)) return false;

    const char *p = s + 10;
    int hh = 0, mi = 0, ss = 0;
    if (*p == 'T' || *p == ' ') {
        p++;
        if (!read_int(p, 2, &hh) || *p++ != ':') return false;
        if (!read_int(p, 2, &mi)) return false;
        if (*p == ':') { p++; if (!read_int(p, 2, &ss)) return false; }
        if (*p == '.') { p++; while (*p >= '0' && *p <= '9') p++; }
    }
    if (hh > 23 || mi > 59 || ss > 59) return false;

    int32_t offset = 0; // segundos a subtrair para chegar em UTC
    if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int oh = 0, om = 0;
        if (!read_int(p, 2, &oh)) return false;
        if (*p == ':') p++;
        if (*p >= '0' && *p <= '9') { if (!read_int(p, 2, &om)) return false; }
        if (oh > 14 || om > 59) return false; // nenhum fuso real passa de +14
        offset = sign * (oh * 3600 + om * 60);
    } else if (*p == 'Z') {
        p++;
    }
    if (*p != '\0') return false; // lixo depois do fuso: melhor recusar que adivinhar

    *outUtc = days_from_civil(y, (unsigned)m, (unsigned)d) * 86400 + hh * 3600 +
              mi * 60 + ss - offset;
    return true;
}

// Divisao inteira arredondando para baixo, correta para epoch negativo.
static int64_t floor_div(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q--;
    return q;
}

int64_t start_of_local_day(int64_t nowLocal) { return floor_div(nowLocal, 86400) * 86400; }
int64_t end_of_local_day(int64_t nowLocal) { return start_of_local_day(nowLocal) + 86399; }

DayClass day_classify(const char *dueDate, bool isAllDay, int status,
                      int64_t nowLocal, int32_t tzOffsetSec,
                      int64_t *outDueTsLocal) {
    *outDueTsLocal = 0;
    if (status != 0) return DayClass::OutOfDay; // defesa: a API promete, nao garante
    if (dueDate == nullptr || dueDate[0] == '\0') return DayClass::OutOfDay;

    const int64_t startToday = start_of_local_day(nowLocal);
    const int64_t endToday = startToday + 86399;
    int64_t dueLocal = 0;

    if (isAllDay) {
        // A PEGADINHA (spec 5.2). O dia inteiro chega como um instante: meia-noite
        // UTC da data OU meia-noite no fuso da tarefa convertida para UTC — a doc
        // nao diz qual, e a Task 3 do Plano B registra a que aparece. As duas caem
        // a menos de 14h da meia-noite local certa, entao somar o fuso e mais 12h
        // e tomar a data civil acerta ambas para qualquer |fuso| < 12h.
        int64_t utc = 0;
        if (!parse_iso8601_utc(dueDate, &utc)) return DayClass::OutOfDay;
        const int64_t day = floor_div(utc + tzOffsetSec + 43200, 86400);
        dueLocal = day * 86400 + 86399;
    } else {
        int64_t utc = 0;
        if (!parse_iso8601_utc(dueDate, &utc)) return DayClass::OutOfDay;
        dueLocal = utc + tzOffsetSec;
    }

    if (dueLocal < startToday) { *outDueTsLocal = dueLocal; return DayClass::Overdue; }
    if (dueLocal <= endToday) { *outDueTsLocal = dueLocal; return DayClass::Today; }
    return DayClass::OutOfDay;
}
