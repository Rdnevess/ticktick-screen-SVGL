#include "tinytest.h"
#include "task_store.h"

static const int64_t DIA = 20720LL * 86400;

static Task mk(const char *id, int priority = PRIO_MED) {
    Task t{};
    task_set_id(t.id, id);
    task_set_id(t.projectId, "p1");
    task_set_title(t, id);
    t.priority = priority;
    t.dueTs = DIA + 9 * 3600;
    return t;
}

TEST(replace_copia_a_lista_e_conta) {
    TaskStore s{};
    Task in[3] = {mk("a"), mk("b"), mk("c")};
    store_replace(s, in, 3);
    CHECK_EQ_INT(s.count, 3);
    CHECK_EQ_INT(s.truncated, 0);
    CHECK_EQ_STR(s.tasks[1].id, "b");
    CHECK(!s.stale);
}

TEST(replace_aplica_o_teto_e_informa_quantas_ficaram_de_fora) {
    TaskStore s{};
    Task in[TASK_LIST_MAX + 7];
    for (int i = 0; i < TASK_LIST_MAX + 7; i++) in[i] = mk("x");
    store_replace(s, in, TASK_LIST_MAX + 7);
    CHECK_EQ_INT(s.count, TASK_LIST_MAX);
    CHECK_EQ_INT(s.truncated, 7);
}

TEST(replace_limpa_o_selo_de_desatualizado) {
    TaskStore s{};
    store_mark_stale(s);
    CHECK(s.stale);
    Task in[1] = {mk("a")};
    store_replace(s, in, 1);
    CHECK(!s.stale);
}

TEST(mark_stale_preserva_o_conjunto_anterior) {
    TaskStore s{};
    Task in[2] = {mk("a"), mk("b")};
    store_replace(s, in, 2);
    store_mark_stale(s);
    CHECK(s.stale);
    CHECK_EQ_INT(s.count, 2); // o ultimo dado bom continua valido
    CHECK_EQ_STR(s.tasks[0].id, "a");
}

TEST(mark_pending_esconde_a_tarefa_da_visao) {
    TaskStore s{};
    Task in[3] = {mk("a"), mk("b"), mk("c")};
    store_replace(s, in, 3);
    CHECK(store_mark_pending(s, "p1", "b"));
    CHECK_EQ_INT(store_visible_count(s), 2);

    Task vis[TASK_LIST_MAX];
    CHECK_EQ_INT(store_visible(s, vis, TASK_LIST_MAX), 2);
    CHECK_EQ_STR(vis[0].id, "a");
    CHECK_EQ_STR(vis[1].id, "c");
}

TEST(mark_pending_de_tarefa_inexistente_devolve_false) {
    TaskStore s{};
    Task in[1] = {mk("a")};
    store_replace(s, in, 1);
    CHECK(!store_mark_pending(s, "p1", "nao_existe"));
    CHECK(!store_mark_pending(s, "outro_projeto", "a"));
}

TEST(confirm_remove_de_vez) {
    TaskStore s{};
    Task in[3] = {mk("a"), mk("b"), mk("c")};
    store_replace(s, in, 3);
    store_mark_pending(s, "p1", "b");
    CHECK(store_confirm_pending(s, "p1", "b"));
    CHECK_EQ_INT(s.count, 2);
    CHECK_EQ_STR(s.tasks[0].id, "a");
    CHECK_EQ_STR(s.tasks[1].id, "c"); // ordem preservada
}

TEST(revert_traz_a_tarefa_de_volta_com_marcador_de_erro) {
    TaskStore s{};
    Task in[2] = {mk("a"), mk("b")};
    store_replace(s, in, 2);
    store_mark_pending(s, "p1", "b");
    CHECK_EQ_INT(store_visible_count(s), 1);

    CHECK(store_revert_pending(s, "p1", "b"));
    CHECK_EQ_INT(store_visible_count(s), 2);
    CHECK(!s.tasks[1].pending);
    CHECK(s.tasks[1].syncError);
}

TEST(replace_carrega_o_pending_para_o_novo_conjunto) {
    // Um refresh pode chegar antes do servidor processar a conclusao: a tarefa
    // ainda vem como aberta. Sem carregar o pending, ela ressuscitaria na tela.
    TaskStore s{};
    Task in[2] = {mk("a"), mk("b")};
    store_replace(s, in, 2);
    store_mark_pending(s, "p1", "b");

    Task novo[2] = {mk("a"), mk("b")}; // servidor ainda devolve "b"
    store_replace(s, novo, 2);
    CHECK_EQ_INT(s.count, 2);
    CHECK(s.tasks[1].pending); // continua escondida
    CHECK_EQ_INT(store_visible_count(s), 1);
}

TEST(replace_carrega_o_pending_quando_a_tarefa_muda_de_posicao) {
    // O pending segue a tarefa pelo id, nao pela posicao no array.
    TaskStore s{};
    Task in[3] = {mk("a"), mk("b"), mk("c")};
    store_replace(s, in, 3);
    store_mark_pending(s, "p1", "a");
    store_mark_pending(s, "p1", "c");

    Task novo[3] = {mk("c"), mk("d"), mk("a")};
    store_replace(s, novo, 3);
    CHECK(s.tasks[0].pending);  // "c"
    CHECK(!s.tasks[1].pending); // "d" chegou agora
    CHECK(s.tasks[2].pending);  // "a"
    CHECK_EQ_INT(store_visible_count(s), 1);
}

TEST(replace_nao_carrega_o_marcador_de_erro) {
    // Erro antigo nao deve grudar: se a tarefa voltou no payload novo, o estado
    // dela e o do servidor.
    TaskStore s{};
    Task in[1] = {mk("a")};
    store_replace(s, in, 1);
    store_mark_pending(s, "p1", "a");
    store_revert_pending(s, "p1", "a");
    CHECK(s.tasks[0].syncError);

    Task novo[1] = {mk("a")};
    store_replace(s, novo, 1);
    CHECK(!s.tasks[0].syncError);
}

TEST(store_visible_respeita_o_limite_de_saida) {
    TaskStore s{};
    Task in[3] = {mk("a"), mk("b"), mk("c")};
    store_replace(s, in, 3);
    Task vis[2];
    CHECK_EQ_INT(store_visible(s, vis, 2), 2);
}

TEST(store_vazio_nao_estoura) {
    TaskStore s{};
    Task vis[4];
    CHECK_EQ_INT(store_visible(s, vis, 4), 0);
    CHECK_EQ_INT(store_visible_count(s), 0);
    CHECK(!store_confirm_pending(s, "p1", "a"));
    CHECK(!store_revert_pending(s, "p1", "a"));
    store_replace(s, nullptr, 0);
    CHECK_EQ_INT(s.count, 0);
}

TEST(replace_usa_o_total_visto_para_o_mais_n) {
    // O parse ja cortou em 64 pela ordenacao; o store so fica sabendo quantas
    // existiam pelo totalSeen.
    TaskStore s{};
    Task in[3] = {mk("a"), mk("b"), mk("c")};
    store_replace(s, in, 3, 10);
    CHECK_EQ_INT(s.count, 3);
    CHECK_EQ_INT(s.truncated, 7);
}
