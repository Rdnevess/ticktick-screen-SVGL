#include "tinytest.h"
#include "task_order.h"

static const int64_t DIA = 20720LL * 86400;

static Task mk(const char *id, bool overdue, int priority, int64_t dueTs) {
    Task t{};
    task_set_id(t.id, id);
    task_set_id(t.projectId, "p1");
    task_set_title(t, id);
    t.overdue = overdue;
    t.priority = priority;
    t.dueTs = dueTs;
    return t;
}

// Lista ja ordenada: alta, media, baixa.
static int fill(Task *v) {
    v[0] = mk("alta", false, PRIO_HIGH, DIA + 9 * 3600);
    v[1] = mk("media", false, PRIO_MED, DIA + 10 * 3600);
    v[2] = mk("baixa", false, PRIO_LOW, DIA + 11 * 3600);
    return 3;
}

TEST(sem_pin_os_dois_slots_sao_as_duas_primeiras) {
    Task v[3]; int n = fill(v);
    PinSet pins{};
    int idx[FOCUS_SLOTS] = {-1, -1};
    CHECK_EQ_INT(focus_select(v, n, pins, idx), 2);
    CHECK_EQ_STR(v[idx[0]].id, "alta");
    CHECK_EQ_STR(v[idx[1]].id, "media");
}

TEST(um_pin_ocupa_o_primeiro_slot_e_o_automatico_completa) {
    Task v[3]; int n = fill(v);
    PinSet pins{};
    CHECK(pin_toggle(pins, v[2]) == PinResult::Pinned); // fixa "baixa"
    int idx[FOCUS_SLOTS] = {-1, -1};
    CHECK_EQ_INT(focus_select(v, n, pins, idx), 2);
    CHECK_EQ_STR(v[idx[0]].id, "baixa"); // o pin vem primeiro
    CHECK_EQ_STR(v[idx[1]].id, "alta");  // o automatico completa
}

TEST(dois_pins_ocupam_os_dois_slots_na_ordem_em_que_foram_fixados) {
    Task v[3]; int n = fill(v);
    PinSet pins{};
    pin_toggle(pins, v[2]); // baixa
    pin_toggle(pins, v[1]); // media
    int idx[FOCUS_SLOTS] = {-1, -1};
    CHECK_EQ_INT(focus_select(v, n, pins, idx), 2);
    CHECK_EQ_STR(v[idx[0]].id, "baixa");
    CHECK_EQ_STR(v[idx[1]].id, "media");
}

TEST(a_tarefa_fixada_nao_aparece_duas_vezes) {
    Task v[3]; int n = fill(v);
    PinSet pins{};
    pin_toggle(pins, v[0]); // fixa a que ja seria a primeira
    int idx[FOCUS_SLOTS] = {-1, -1};
    CHECK_EQ_INT(focus_select(v, n, pins, idx), 2);
    CHECK_EQ_STR(v[idx[0]].id, "alta");
    CHECK_EQ_STR(v[idx[1]].id, "media");
    CHECK(idx[0] != idx[1]);
}

TEST(terceiro_pin_e_recusado_porque_ha_so_dois_slots) {
    Task v[3]; fill(v);
    PinSet pins{};
    CHECK(pin_toggle(pins, v[0]) == PinResult::Pinned);
    CHECK(pin_toggle(pins, v[1]) == PinResult::Pinned);
    CHECK(pin_toggle(pins, v[2]) == PinResult::Full); // recusado, nao fixou
    CHECK_EQ_INT(pins.count, 2);
    CHECK(!pin_contains(pins, v[2]));
}

TEST(toggle_no_mesmo_item_solta_o_pin) {
    Task v[3]; fill(v);
    PinSet pins{};
    CHECK(pin_toggle(pins, v[1]) == PinResult::Pinned);
    CHECK(pin_contains(pins, v[1]));
    CHECK(pin_toggle(pins, v[1]) == PinResult::Unpinned); // segundo toggle solta
    CHECK(!pin_contains(pins, v[1]));
    CHECK_EQ_INT(pins.count, 0);
}

TEST(pin_orfao_e_limpo_quando_a_tarefa_sai_do_dia) {
    Task v[3]; fill(v);
    PinSet pins{};
    pin_toggle(pins, v[1]); // fixa "media"
    CHECK_EQ_INT(pins.count, 1);

    // Novo ciclo em que "media" nao esta mais na lista.
    Task depois[2] = {v[0], v[2]};
    CHECK_EQ_INT(pin_prune(pins, depois, 2), 1);
    CHECK_EQ_INT(pins.count, 0);
}

TEST(pin_de_outra_lista_com_mesmo_taskId_nao_confunde) {
    // O par (projectId, taskId) e a identidade; taskId sozinho nao basta.
    Task a = mk("x", false, PRIO_MED, DIA + 9 * 3600);
    Task b = a;
    task_set_id(b.projectId, "p2");
    PinSet pins{};
    pin_toggle(pins, a);
    CHECK(pin_contains(pins, a));
    CHECK(!pin_contains(pins, b));
}

TEST(dia_com_uma_tarefa_preenche_um_slot_so) {
    Task v[1] = {mk("unica", false, PRIO_MED, DIA + 9 * 3600)};
    PinSet pins{};
    int idx[FOCUS_SLOTS] = {-1, -1};
    CHECK_EQ_INT(focus_select(v, 1, pins, idx), 1);
    CHECK_EQ_STR(v[idx[0]].id, "unica");
}

TEST(dia_vazio_preenche_zero_slots) {
    PinSet pins{};
    int idx[FOCUS_SLOTS] = {-1, -1};
    CHECK_EQ_INT(focus_select(nullptr, 0, pins, idx), 0);
}
