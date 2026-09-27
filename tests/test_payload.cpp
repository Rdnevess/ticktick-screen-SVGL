#include "tinytest.h"
#include "payload.h"

#include <cstring>
#include <string>

static const int64_t NOW = 20720LL * 86400 + 10 * 3600; // 24/09/2026 10:00 local
static const int32_t TZ = -3 * 3600;

// Resposta reduzida de GET /project/{id}/data, com campos extras que o filtro
// tem de descartar (content, items, reminders, columns).
static const char *PROJECT_DATA = R"({
  "project": {"id":"p1","name":"Trabalho"},
  "columns": [{"id":"c1","name":"A fazer"}],
  "tasks": [
    {"id":"t1","projectId":"p1","title":"Revisão do contrato",
     "dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false,
     "priority":5,"status":0,"sortOrder":100,
     "content":"texto longo que nao deve ser lido",
     "items":[{"id":"i1","title":"sub"}],
     "reminders":["TRIGGER:PT0S"]},
    {"id":"t2","projectId":"p1","title":"Comprar café",
     "dueDate":"2026-09-24T00:00:00.000+0000","isAllDay":true,
     "priority":1,"status":0,"sortOrder":200},
    {"id":"t3","projectId":"p1","title":"Tarefa de amanha",
     "dueDate":"2026-09-25T12:00:00.000+0000","isAllDay":false,
     "priority":3,"status":0,"sortOrder":300},
    {"id":"t4","projectId":"p1","title":"Sem data","priority":0,"status":0},
    {"id":"t5","projectId":"p1","title":"Ja concluida",
     "dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false,
     "priority":5,"status":2,"sortOrder":400}
  ]
})";

// Atalho: parse num TaskTop de capacidade `cap` apoiado em `buf`.
static int parse(const char *json, size_t len, Task *buf, int cap, TaskTop &top) {
    top_init(top, buf, cap);
    return payload_parse_project(json, len, NOW, TZ, top);
}

TEST(extrai_so_as_tarefas_do_dia) {
    Task buf[TASK_LIST_MAX];
    TaskTop top;
    // t1 (hoje com hora) e t2 (hoje all-day). Fora: t3 amanha, t4 sem data,
    // t5 concluida.
    CHECK_EQ_INT(parse(PROJECT_DATA, std::strlen(PROJECT_DATA), buf, TASK_LIST_MAX, top), 2);
    CHECK_EQ_INT(top.count, 2);
    CHECK_EQ_INT(top.seen, 2);
}

TEST(preenche_os_campos_do_task) {
    Task buf[TASK_LIST_MAX];
    TaskTop top;
    parse(PROJECT_DATA, std::strlen(PROJECT_DATA), buf, TASK_LIST_MAX, top);
    const Task &t = top.items[0];
    CHECK_EQ_STR(t.id, "t1");
    CHECK_EQ_STR(t.projectId, "p1");
    CHECK_EQ_STR(t.title, "Revisão do contrato");
    CHECK_EQ_INT(t.priority, 5);
    CHECK_EQ_INT(t.sortOrder, 100);
    CHECK(!t.isAllDay);
    CHECK(!t.overdue);
    CHECK_EQ_INT(t.dueTs, 20720LL * 86400 + 8 * 3600); // 11:00 UTC = 08:00 em UTC-3
}

TEST(all_day_recebe_23_59_59_e_nao_vira_atrasada) {
    Task buf[TASK_LIST_MAX];
    TaskTop top;
    parse(PROJECT_DATA, std::strlen(PROJECT_DATA), buf, TASK_LIST_MAX, top);
    const Task &t = top.items[1];
    CHECK_EQ_STR(t.id, "t2");
    CHECK(t.isAllDay);
    CHECK(!t.overdue);
    CHECK_EQ_INT(t.dueTs, 20720LL * 86400 + 86399);
}

TEST(atrasada_vem_marcada) {
    const char *j = R"({"project":{"id":"p1"},"tasks":[
      {"id":"t9","projectId":"p1","title":"Atrasada","status":0,"priority":3,
       "dueDate":"2026-09-24T02:00:00.000+0000","isAllDay":false,"sortOrder":1}]})";
    Task buf[4];
    TaskTop top;
    CHECK_EQ_INT(parse(j, std::strlen(j), buf, 4, top), 1);
    CHECK(top.items[0].overdue);
}

TEST(json_malformado_devolve_menos_um_e_nao_toca_no_acumulador) {
    Task buf[4];
    TaskTop top;
    const char *lixo = "{\"tasks\": [ isso nao e json";
    CHECK_EQ_INT(parse(lixo, std::strlen(lixo), buf, 4, top), -1);
    CHECK_EQ_INT(top.count, 0);
    CHECK_EQ_INT(top.seen, 0);
}

TEST(resposta_de_erro_da_api_nao_vira_dia_limpo) {
    // JSON valido sem "tasks": e o corpo de um erro, nao uma lista vazia.
    const char *erro = R"({"errorCode":"unauthorized","errorMessage":"token invalido"})";
    Task buf[4];
    TaskTop top;
    CHECK_EQ_INT(parse(erro, std::strlen(erro), buf, 4, top), -1);
    const char *array = R"([{"id":"x"}])";
    CHECK_EQ_INT(parse(array, std::strlen(array), buf, 4, top), -1);
}

TEST(lista_sem_tarefas_devolve_zero) {
    const char *j = R"({"project":{"id":"p1","name":"Vazia"},"tasks":[]})";
    Task buf[4];
    TaskTop top;
    CHECK_EQ_INT(parse(j, std::strlen(j), buf, 4, top), 0);
    CHECK_EQ_INT(top.count, 0);
}

TEST(respeita_a_capacidade_e_relata_o_total_visto) {
    std::string j = R"({"project":{"id":"p1"},"tasks":[)";
    for (int i = 0; i < 3; i++) {
        if (i) j += ",";
        j += R"({"id":"t)" + std::to_string(i) +
             R"(","projectId":"p1","title":"x","status":0,"priority":3,)" +
             R"("dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false,"sortOrder":1})";
    }
    j += "]}";
    Task buf[1];
    TaskTop top;
    CHECK_EQ_INT(parse(j.c_str(), j.size(), buf, 1, top), 3);
    CHECK_EQ_INT(top.count, 1);
    CHECK_EQ_INT(top.seen, 3);
}

TEST(com_mais_que_a_capacidade_mantem_as_melhores_pela_ordenacao) {
    // A atrasada e a ULTIMA do JSON. Cortar na ordem do JSON a perderia.
    const char *j = R"({"tasks":[
      {"id":"a","projectId":"p1","title":"a","status":0,"priority":1,
       "dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false,"sortOrder":1},
      {"id":"b","projectId":"p1","title":"b","status":0,"priority":1,
       "dueDate":"2026-09-24T12:00:00.000+0000","isAllDay":false,"sortOrder":2},
      {"id":"c","projectId":"p1","title":"c","status":0,"priority":0,
       "dueDate":"2026-09-23T12:00:00.000+0000","isAllDay":false,"sortOrder":3}]})";
    Task buf[2];
    TaskTop top;
    CHECK_EQ_INT(parse(j, std::strlen(j), buf, 2, top), 3);
    CHECK_EQ_STR(top.items[0].id, "c");
    CHECK_EQ_STR(top.items[1].id, "a");
    CHECK_EQ_INT(top.seen - top.count, 1);
}

TEST(tarefa_sem_id_e_descartada) {
    // Sem id (ou sem projectId) nao da para concluir: nao entra.
    const char *j = R"({"tasks":[
      {"id":"","projectId":"p1","title":"sem id","status":0,"priority":3,
       "dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false},
      {"id":"t1","title":"sem projeto","status":0,"priority":3,
       "dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false},
      {"id":"t2","projectId":"p1","title":"ok","status":0,"priority":3,
       "dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false}]})";
    Task buf[4];
    TaskTop top;
    CHECK_EQ_INT(parse(j, std::strlen(j), buf, 4, top), 1);
    CHECK_EQ_STR(top.items[0].id, "t2");
}

TEST(titulo_gigante_e_truncado_sem_partir_acento) {
    std::string titulo(79, 'a');
    titulo += "ç";
    std::string j = R"({"project":{"id":"p1"},"tasks":[{"id":"t1","projectId":"p1",)"
                    R"("title":")" + titulo + R"(","status":0,"priority":3,)"
                    R"("dueDate":"2026-09-24T11:00:00.000+0000","isAllDay":false,"sortOrder":1}]})";
    Task buf[2];
    TaskTop top;
    CHECK_EQ_INT(parse(j.c_str(), j.size(), buf, 2, top), 1);
    CHECK_EQ_INT(std::strlen(top.items[0].title), 79);
}

TEST(le_a_lista_de_projetos) {
    const char *j = R"([
      {"id":"p1","name":"Trabalho","color":"#FF0000","kind":"TASK"},
      {"id":"p2","name":"Pessoal — casa","kind":"TASK"}
    ])";
    char ids[8][33];
    char names[8][49];
    int n = payload_parse_projects(j, std::strlen(j), ids, names, 8);
    CHECK_EQ_INT(n, 2);
    CHECK_EQ_STR(ids[0], "p1");
    CHECK_EQ_STR(names[0], "Trabalho");
    CHECK_EQ_STR(ids[1], "p2");
    CHECK_EQ_STR(names[1], "Pessoal — casa");
}

TEST(lista_de_projetos_malformada_devolve_menos_um) {
    char ids[4][33];
    char names[4][49];
    const char *lixo = "nao e json";
    CHECK_EQ_INT(payload_parse_projects(lixo, std::strlen(lixo), ids, names, 4), -1);
}
