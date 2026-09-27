#include "tinytest.h"
#include "settings_values.h"

TEST(poll_anda_pela_lista_e_satura_nas_pontas) {
    CHECK_EQ_INT(poll_step(5, +1), 10);
    CHECK_EQ_INT(poll_step(5, -1), 2);
    CHECK_EQ_INT(poll_step(30, +1), 30);
    CHECK_EQ_INT(poll_step(1, -1), 1);
}

TEST(poll_fora_da_lista_vai_para_o_vizinho_na_direcao) {
    // Valor vindo do console (poll 7) ou de versao antiga.
    CHECK_EQ_INT(poll_step(7, +1), 10);
    CHECK_EQ_INT(poll_step(7, -1), 5);
    CHECK_EQ_INT(poll_step(90, -1), 30);
}

TEST(pomodoro_anda_pela_lista) {
    CHECK_EQ_INT(pomo_step(25, +1), 30);
    CHECK_EQ_INT(pomo_step(25, -1), 20);
    CHECK_EQ_INT(pomo_step(50, +1), 50);
    CHECK_EQ_INT(pomo_step(15, -1), 15);
}

TEST(fuso_em_passos_de_15_min_limitado) {
    CHECK_EQ_INT(tz_step(-180, +1), -165);
    CHECK_EQ_INT(tz_step(-180, -1), -195);
    CHECK_EQ_INT(tz_step(840, +1), 840);
    CHECK_EQ_INT(tz_step(-720, -1), -720);
    CHECK_EQ_INT(tz_step(-181, +1), -180); // fora do passo: alinha na direcao
    CHECK_EQ_INT(tz_step(-181, -1), -195);
}

TEST(fuso_positivo_fora_do_passo_avanca) {
    CHECK_EQ_INT(tz_step(181, +1), 195);
}

TEST(fuso_positivo_fora_do_passo_recua) {
    CHECK_EQ_INT(tz_step(181, -1), 180);
}

TEST(fuso_formatado) {
    char b[16];
    tz_format(-180, b, sizeof(b)); CHECK_EQ_STR(b, "UTC-3:00");
    tz_format(330, b, sizeof(b));  CHECK_EQ_STR(b, "UTC+5:30");
    tz_format(0, b, sizeof(b));    CHECK_EQ_STR(b, "UTC+0:00");
    tz_format(-30, b, sizeof(b));  CHECK_EQ_STR(b, "UTC-0:30");
    tz_format(345, b, sizeof(b));  CHECK_EQ_STR(b, "UTC+5:45");
}
