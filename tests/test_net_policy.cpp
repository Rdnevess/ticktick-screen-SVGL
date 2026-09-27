#include "tinytest.h"
#include "net_policy.h"

static const int64_t DIA = 86400;

TEST(renova_quando_faltam_menos_de_7_dias) {
    CHECK(policy_should_renew(1000, 1000 + 6 * DIA));
    CHECK(!policy_should_renew(1000, 1000 + 8 * DIA));
    CHECK(policy_should_renew(1000, 500)); // ja venceu
}

TEST(sem_prazo_conhecido_nao_renova_por_conta_propria) {
    // exp desconhecido: quem manda e o 401 (spec 6.4).
    CHECK(!policy_should_renew(1000, 0));
    CHECK(!policy_should_renew(0, 1000)); // sem hora nao ha como comparar
}

TEST(recuo_dobra_a_partir_do_intervalo_normal) {
    CHECK_EQ_INT(policy_backoff(300, 300), 600);
    CHECK_EQ_INT(policy_backoff(600, 300), 1200);
}

TEST(recuo_para_em_30_minutos) {
    CHECK_EQ_INT(policy_backoff(1200, 300), 1800);
    CHECK_EQ_INT(policy_backoff(1800, 300), 1800);
}

TEST(recuo_nunca_fica_abaixo_do_intervalo_configurado) {
    // Poll configurado acima do teto: o recuo nao encurta o ciclo.
    CHECK_EQ_INT(policy_backoff(3600, 3600), 3600);
}
