#include "tinytest.h"
#include "day_filter.h"

// Referencias fixas, sem depender do relogio do host.
static const int64_t D_2026_09_24_UTC = 1790208000LL;
static const int32_t TZ_SP = -3 * 3600;       // America/Sao_Paulo
static const int32_t TZ_IN = 5 * 3600 + 1800; // Asia/Kolkata, UTC+5:30

// Numeros conferidos: 24/09/2026 e o dia 20720 desde a epoca (23/09 e 20719) e
// 2026-09-24T00:00:00Z = 1790208000. "agora" nos testes e sempre
// 20720 * 86400 + 10h, isto e, 24/09 as 10:00 em epoch LOCAL.

TEST(days_from_civil_bate_com_valores_conhecidos) {
    CHECK_EQ_INT(days_from_civil(1970, 1, 1), 0);
    CHECK_EQ_INT(days_from_civil(2000, 3, 1), 11017);
    CHECK_EQ_INT(days_from_civil(2026, 9, 24), 20720);
    CHECK_EQ_INT(days_from_civil(2024, 2, 29), 19782); // ano bissexto
}

TEST(parse_iso8601_utc_le_data_hora_e_offset) {
    int64_t ts = 0;
    CHECK(parse_iso8601_utc("2026-09-24T00:00:00.000+0000", &ts));
    CHECK_EQ_INT(ts, D_2026_09_24_UTC);

    CHECK(parse_iso8601_utc("2026-09-24T14:30:00.000+0000", &ts));
    CHECK_EQ_INT(ts, D_2026_09_24_UTC + 14 * 3600 + 30 * 60);

    // offset nao-zero e subtraido para chegar em UTC
    CHECK(parse_iso8601_utc("2026-09-24T14:30:00.000-0300", &ts));
    CHECK_EQ_INT(ts, D_2026_09_24_UTC + 17 * 3600 + 30 * 60);

    // formas aceitas: sem milissegundos e com Z
    CHECK(parse_iso8601_utc("2026-09-24T00:00:00Z", &ts));
    CHECK_EQ_INT(ts, D_2026_09_24_UTC);
}

TEST(parse_iso8601_utc_rejeita_lixo) {
    int64_t ts = 123;
    CHECK(!parse_iso8601_utc("", &ts));
    CHECK(!parse_iso8601_utc(nullptr, &ts));
    CHECK(!parse_iso8601_utc("nao e data", &ts));
    CHECK(!parse_iso8601_utc("2026-13-45T99:99:99.000+0000", &ts));
}

TEST(parse_iso8601_date_ignora_hora_e_fuso) {
    int y = 0, m = 0, d = 0;
    CHECK(parse_iso8601_date("2026-09-24T00:00:00.000+0000", &y, &m, &d));
    CHECK_EQ_INT(y, 2026);
    CHECK_EQ_INT(m, 9);
    CHECK_EQ_INT(d, 24);
}

TEST(limites_do_dia_local_caem_na_meia_noite) {
    int64_t now = 20720LL * 86400 + 10 * 3600; // 24/09 10:00, epoch local
    CHECK_EQ_INT(start_of_local_day(now), 20720LL * 86400);
    CHECK_EQ_INT(end_of_local_day(now), 20720LL * 86400 + 86399);
}

// ---- A pegadinha do all-day ----

TEST(all_day_de_hoje_em_utc_menos_3_nao_e_atrasada) {
    // O TickTick manda dia inteiro como meia-noite UTC da data. Convertido
    // ingenuamente para UTC-3 isso daria 23/09 21:00 -> "atrasada de ontem".
    int64_t now = 20720LL * 86400 + 10 * 3600; // 24/09 10:00 local
    int64_t due = 0;
    DayClass c = day_classify("2026-09-24T00:00:00.000+0000", true, 0, now, TZ_SP, &due);
    CHECK(c == DayClass::Today);
    // Para ordenacao, all-day recebe 23:59:59 local do seu dia.
    CHECK_EQ_INT(due, 20720LL * 86400 + 86399);
}

TEST(all_day_de_hoje_em_utc_mais_5_30_tambem_e_hoje) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    DayClass c = day_classify("2026-09-24T00:00:00.000+0000", true, 0, now, TZ_IN, &due);
    CHECK(c == DayClass::Today);
}

TEST(all_day_de_ontem_e_atrasada) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    DayClass c = day_classify("2026-09-23T00:00:00.000+0000", true, 0, now, TZ_SP, &due);
    CHECK(c == DayClass::Overdue);
    CHECK_EQ_INT(due, 20719LL * 86400 + 86399);
}

TEST(all_day_de_amanha_fica_fora_do_dia) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    CHECK(day_classify("2026-09-25T00:00:00.000+0000", true, 0, now, TZ_SP, &due) ==
          DayClass::OutOfDay);
}

// ---- Tarefas com hora ----

TEST(tarefa_de_hoje_as_8_com_agora_10_e_do_dia_nao_atrasada) {
    // Atrasada e apenas o que venceu ANTES do inicio de hoje (spec 5.2).
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    // 24/09 08:00 em Sao Paulo = 11:00 UTC
    DayClass c = day_classify("2026-09-24T11:00:00.000+0000", false, 0, now, TZ_SP, &due);
    CHECK(c == DayClass::Today);
    CHECK_EQ_INT(due, 20720LL * 86400 + 8 * 3600);
}

TEST(tarefa_de_ontem_as_23_e_atrasada) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    // 23/09 23:00 em Sao Paulo = 24/09 02:00 UTC
    DayClass c = day_classify("2026-09-24T02:00:00.000+0000", false, 0, now, TZ_SP, &due);
    CHECK(c == DayClass::Overdue);
    CHECK_EQ_INT(due, 20719LL * 86400 + 23 * 3600);
}

TEST(tarefa_de_amanha_fica_fora) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    // 25/09 09:00 em Sao Paulo = 25/09 12:00 UTC
    CHECK(day_classify("2026-09-25T12:00:00.000+0000", false, 0, now, TZ_SP, &due) ==
          DayClass::OutOfDay);
}

TEST(tarefa_de_hoje_as_23_59_ainda_entra) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    // 24/09 23:59 em Sao Paulo = 25/09 02:59 UTC
    CHECK(day_classify("2026-09-25T02:59:00.000+0000", false, 0, now, TZ_SP, &due) ==
          DayClass::Today);
}

// ---- Descartes ----

TEST(sem_data_fica_fora_do_dia) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 123;
    CHECK(day_classify("", false, 0, now, TZ_SP, &due) == DayClass::OutOfDay);
    CHECK(day_classify(nullptr, false, 0, now, TZ_SP, &due) == DayClass::OutOfDay);
}

TEST(status_diferente_de_zero_e_descartado) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    // Mesma tarefa que seria de hoje, mas marcada como concluida.
    CHECK(day_classify("2026-09-24T11:00:00.000+0000", false, 2, now, TZ_SP, &due) ==
          DayClass::OutOfDay);
}

// ---- Regra robusta do dia inteiro (Plano B, Task 1) ----

TEST(all_day_na_convencao_meia_noite_local_tambem_e_hoje) {
    // Se o TickTick mandar a meia-noite do fuso da tarefa: 24/09 00:00 em SP
    // chega como 03:00 UTC.
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    CHECK(day_classify("2026-09-24T03:00:00.000+0000", true, 0, now, TZ_SP, &due) ==
          DayClass::Today);
    CHECK_EQ_INT(due, 20720LL * 86400 + 86399);
}

TEST(all_day_meia_noite_local_em_utc_mais_5_30_e_hoje) {
    // 24/09 00:00 em Kolkata = 23/09 18:30 UTC.
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    CHECK(day_classify("2026-09-23T18:30:00.000+0000", true, 0, now, TZ_IN, &due) ==
          DayClass::Today);
}

TEST(all_day_so_com_a_data_tambem_funciona) {
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    CHECK(day_classify("2026-09-24", true, 0, now, TZ_SP, &due) == DayClass::Today);
}

TEST(data_impossivel_e_rejeitada) {
    int64_t ts = 0;
    int y = 0, m = 0, d = 0;
    CHECK(!parse_iso8601_utc("2026-02-31T00:00:00Z", &ts));
    CHECK(!parse_iso8601_date("2026-04-31", &y, &m, &d));
    CHECK(parse_iso8601_date("2024-02-29", &y, &m, &d)); // bissexto
    CHECK(!parse_iso8601_date("2026-02-29", &y, &m, &d));
}

TEST(offset_absurdo_ou_lixo_no_fim_e_rejeitado) {
    int64_t ts = 0;
    CHECK(!parse_iso8601_utc("2026-09-24T00:00:00+9900", &ts));
    CHECK(!parse_iso8601_utc("2026-09-24T00:00:00Zlixo", &ts));
    CHECK(!parse_iso8601_utc("2026-09-24T00:00:00.000+0000 x", &ts));
}

TEST(tarefa_exatamente_a_meia_noite_de_hoje_e_do_dia) {
    // Fronteira: 00:00:00 local de hoje nao e atrasada. 24/09 00:00 SP = 03:00 UTC.
    int64_t now = 20720LL * 86400 + 10 * 3600;
    int64_t due = 0;
    CHECK(day_classify("2026-09-24T03:00:00.000+0000", false, 0, now, TZ_SP, &due) ==
          DayClass::Today);
    CHECK_EQ_INT(due, 20720LL * 86400);
}
