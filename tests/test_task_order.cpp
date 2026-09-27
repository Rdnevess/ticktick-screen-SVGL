#include "tinytest.h"
#include "task_order.h"

#include <cstring>

// Fabrica de tarefa para os testes. `dueTs` em epoch local.
static Task mk(const char *id, bool overdue, int priority, int64_t dueTs,
               int64_t sortOrder = 0, bool allDay = false) {
    Task t{};
    task_set_id(t.id, id);
    task_set_id(t.projectId, "p1");
    task_set_title(t, id);
    t.overdue = overdue;
    t.priority = priority;
    t.dueTs = dueTs;
    t.sortOrder = sortOrder;
    t.isAllDay = allDay;
    return t;
}

static const int64_t DIA = 20720LL * 86400; // 24/09/2026

TEST(atrasada_vem_antes_de_hoje_mesmo_com_prioridade_menor) {
    Task atrasadaBaixa = mk("atrasada", true, PRIO_LOW, DIA - 3600);
    Task hojeAlta = mk("hoje", false, PRIO_HIGH, DIA + 9 * 3600);
    CHECK(task_less(atrasadaBaixa, hojeAlta));
    CHECK(!task_less(hojeAlta, atrasadaBaixa));
}

TEST(prioridade_decrescente_dentro_do_mesmo_grupo) {
    Task alta = mk("alta", false, PRIO_HIGH, DIA + 20 * 3600);
    Task media = mk("media", false, PRIO_MED, DIA + 9 * 3600);
    Task baixa = mk("baixa", false, PRIO_LOW, DIA + 8 * 3600);
    Task nenhuma = mk("nenhuma", false, PRIO_NONE, DIA + 7 * 3600);
    CHECK(task_less(alta, media));
    CHECK(task_less(media, baixa));
    CHECK(task_less(baixa, nenhuma));
}

TEST(entre_iguais_o_horario_mais_proximo_vem_primeiro) {
    Task cedo = mk("cedo", false, PRIO_MED, DIA + 8 * 3600);
    Task tarde = mk("tarde", false, PRIO_MED, DIA + 16 * 3600);
    CHECK(task_less(cedo, tarde));
}

TEST(entre_atrasadas_a_mais_antiga_vem_primeiro) {
    // Mesma regra de timestamp crescente, sem ramo especial.
    Task antiga = mk("antiga", true, PRIO_MED, DIA - 5 * 86400);
    Task recente = mk("recente", true, PRIO_MED, DIA - 3600);
    CHECK(task_less(antiga, recente));
}

TEST(all_day_cai_no_fim_do_seu_grupo_de_prioridade) {
    // all-day recebe 23:59:59 do dia (Task 3), entao perde de qualquer hora.
    Task comHora = mk("com_hora", false, PRIO_MED, DIA + 16 * 3600);
    Task diaInteiro = mk("dia_inteiro", false, PRIO_MED, DIA + 86399, 0, true);
    CHECK(task_less(comHora, diaInteiro));
}

TEST(desempate_final_e_o_sortOrder_do_ticktick) {
    Task primeira = mk("primeira", false, PRIO_MED, DIA + 9 * 3600, /*sortOrder*/ 100);
    Task segunda = mk("segunda", false, PRIO_MED, DIA + 9 * 3600, /*sortOrder*/ 200);
    CHECK(task_less(primeira, segunda));
    CHECK(!task_less(segunda, primeira));
}

TEST(comparador_e_irreflexivo) {
    // Exigencia de std::sort: !less(x, x) para todo x.
    Task t = mk("t", false, PRIO_MED, DIA + 9 * 3600, 42);
    CHECK(!task_less(t, t));
}

TEST(task_sort_ordena_a_lista_inteira) {
    Task v[5] = {
        mk("hoje_media_tarde", false, PRIO_MED, DIA + 16 * 3600),
        mk("hoje_alta", false, PRIO_HIGH, DIA + 20 * 3600),
        mk("atrasada_nenhuma", true, PRIO_NONE, DIA - 86400),
        mk("hoje_media_cedo", false, PRIO_MED, DIA + 8 * 3600),
        mk("atrasada_alta", true, PRIO_HIGH, DIA - 3600),
    };
    task_sort(v, 5);
    CHECK_EQ_STR(v[0].id, "atrasada_alta");
    CHECK_EQ_STR(v[1].id, "atrasada_nenhuma");
    CHECK_EQ_STR(v[2].id, "hoje_alta");
    CHECK_EQ_STR(v[3].id, "hoje_media_cedo");
    CHECK_EQ_STR(v[4].id, "hoje_media_tarde");
}

TEST(task_sort_aguenta_lista_vazia_e_de_um) {
    Task v[1] = {mk("so_essa", false, PRIO_MED, DIA)};
    task_sort(v, 0); // nao deve estourar
    task_sort(v, 1);
    CHECK_EQ_STR(v[0].id, "so_essa");
}

// ---- TaskTop: as melhores N pela ordenacao (spec 5.1) ----

TEST(top_guarda_as_melhores_pela_ordenacao) {
    Task buf[2];
    TaskTop top;
    top_init(top, buf, 2);
    top_offer(top, mk("baixa", false, PRIO_LOW, DIA + 9 * 3600));
    top_offer(top, mk("media", false, PRIO_MED, DIA + 9 * 3600));
    top_offer(top, mk("atrasada", true, PRIO_NONE, DIA - 3600));
    CHECK_EQ_INT(top.count, 2);
    CHECK_EQ_INT(top.seen, 3);
    CHECK_EQ_STR(top.items[0].id, "atrasada");
    CHECK_EQ_STR(top.items[1].id, "media");
}

TEST(top_mantem_a_ordem_de_chegada_entre_empatadas) {
    Task buf[3];
    TaskTop top;
    top_init(top, buf, 3);
    top_offer(top, mk("a", false, PRIO_MED, DIA, 5));
    top_offer(top, mk("b", false, PRIO_MED, DIA, 5));
    CHECK_EQ_STR(top.items[0].id, "a");
    CHECK_EQ_STR(top.items[1].id, "b");
}

TEST(top_cheio_descarta_a_pior_que_chega) {
    Task buf[1];
    TaskTop top;
    top_init(top, buf, 1);
    top_offer(top, mk("alta", false, PRIO_HIGH, DIA));
    top_offer(top, mk("baixa", false, PRIO_LOW, DIA));
    CHECK_EQ_INT(top.count, 1);
    CHECK_EQ_INT(top.seen, 2);
    CHECK_EQ_STR(top.items[0].id, "alta");
}
