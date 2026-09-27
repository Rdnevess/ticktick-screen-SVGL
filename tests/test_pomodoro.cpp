#include "tinytest.h"
#include "pomodoro.h"

static const int64_t MIN25 = 25LL * 60 * 1000;

static Task mk(const char *id, const char *title) {
    Task t{};
    task_set_id(t.id, id);
    task_set_id(t.projectId, "p1");
    task_set_title(t, title);
    return t;
}

TEST(comeca_parado) {
    Pomodoro p{};
    CHECK(p.state == PomoState::Idle);
}

TEST(start_leva_a_rodando_e_guarda_a_tarefa) {
    Pomodoro p{};
    Task t = mk("t1", "Revisão do contrato");
    pomo_start(p, t, 1000, MIN25);
    CHECK(p.state == PomoState::Running);
    CHECK_EQ_STR(p.taskId, "t1");
    CHECK_EQ_STR(p.title, "Revisão do contrato");
    CHECK(pomo_is_for(p, t));
}

TEST(restante_diminui_com_o_tempo) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    CHECK_EQ_INT(pomo_remaining_ms(p, 0), MIN25);
    CHECK_EQ_INT(pomo_remaining_ms(p, 60 * 1000), MIN25 - 60 * 1000);
}

TEST(restante_nunca_fica_negativo) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    CHECK_EQ_INT(pomo_remaining_ms(p, MIN25 + 999999), 0);
}

TEST(tick_antes_do_fim_mantem_rodando) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_tick(p, MIN25 - 1);
    CHECK(p.state == PomoState::Running);
}

TEST(tick_no_fim_leva_a_terminado) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_tick(p, MIN25);
    CHECK(p.state == PomoState::Finished);
}

TEST(terminado_nao_expira_sozinho) {
    // O dialogo espera a escolha do usuario por quanto tempo for preciso.
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_tick(p, MIN25);
    pomo_tick(p, MIN25 + 10LL * 3600 * 1000); // dez horas depois
    CHECK(p.state == PomoState::Finished);
}

TEST(renovar_reinicia_com_a_mesma_duracao_e_a_mesma_tarefa) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "Revisão"), 0, MIN25);
    pomo_tick(p, MIN25);
    pomo_renew(p, 999000);
    CHECK(p.state == PomoState::Running);
    CHECK_EQ_INT(p.durationMs, MIN25);
    CHECK_EQ_STR(p.taskId, "t1");
    CHECK_EQ_INT(pomo_remaining_ms(p, 999000), MIN25);
}

TEST(cancelar_volta_a_parado_e_esquece_a_tarefa) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_cancel(p);
    CHECK(p.state == PomoState::Idle);
    CHECK_EQ_STR(p.taskId, "");
}

TEST(cancelar_durante_o_ciclo_tambem_funciona) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_tick(p, 5000);
    pomo_cancel(p);
    CHECK(p.state == PomoState::Idle);
}

TEST(concluir_e_oferecido_quando_a_tarefa_ainda_existe) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_tick(p, MIN25);
    CHECK(pomo_can_complete(p));
}

TEST(tarefa_que_sumiu_no_meio_do_ciclo_suprime_o_concluir) {
    // Concluida no celular: o contador segue, mas o dialogo perde o Concluir.
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_mark_task_gone(p);
    CHECK(p.state == PomoState::Running); // o contador NAO para
    pomo_tick(p, MIN25);
    CHECK(p.state == PomoState::Finished);
    CHECK(!pomo_can_complete(p));
}

TEST(renovar_depois_de_a_tarefa_sumir_nao_ressuscita_o_concluir) {
    Pomodoro p{};
    pomo_start(p, mk("t1", "x"), 0, MIN25);
    pomo_mark_task_gone(p);
    pomo_tick(p, MIN25);
    pomo_renew(p, MIN25);
    pomo_tick(p, 2 * MIN25);
    CHECK(!pomo_can_complete(p));
}

TEST(start_em_outra_tarefa_substitui_o_ciclo) {
    // A UI pergunta antes; o nucleo so obedece.
    Pomodoro p{};
    pomo_start(p, mk("t1", "primeira"), 0, MIN25);
    pomo_start(p, mk("t2", "segunda"), 5000, MIN25);
    CHECK_EQ_STR(p.taskId, "t2");
    CHECK_EQ_INT(pomo_remaining_ms(p, 5000), MIN25);
}

TEST(pomo_is_for_usa_o_par_projeto_e_tarefa) {
    Pomodoro p{};
    Task a = mk("x", "a");
    Task b = a;
    task_set_id(b.projectId, "p2");
    pomo_start(p, a, 0, MIN25);
    CHECK(pomo_is_for(p, a));
    CHECK(!pomo_is_for(p, b));
}
