// Parse das respostas do TickTick (spec 6.5).
//
// Unica excecao a regra "core nao inclui biblioteca do Arduino": ArduinoJson e
// header-only, compila no PC e nao puxa nada do hardware.
#ifndef CORE_PAYLOAD_H
#define CORE_PAYLOAD_H

#include <cstddef>
#include <cstdint>

#include "task_order.h"

constexpr int PROJECT_NAME_BYTES = 48;

// Le GET /project/{id}/data e oferece ao `acc` as tarefas do dia (hoje +
// atrasadas), ja com dueTs local e overdue preenchidos. O `acc` guarda as
// melhores pela ordenacao mesmo somando varias listas (spec 5.1).
//
// Devolve quantas tarefas do dia havia no payload, ou -1 se o JSON for
// invalido ou nao tiver o array "tasks" — o corpo de um erro da API nao pode
// virar "dia limpo". Com -1 o `acc` fica intocado. Tarefa sem id ou sem
// projectId e descartada: nao haveria como concluir.
int payload_parse_project(const char *json, size_t len, int64_t nowLocal,
                          int32_t tzOffsetSec, TaskTop &acc);

// Le GET /project (array de listas). Devolve quantas escreveu, ou -1.
int payload_parse_projects(const char *json, size_t len,
                           char ids[][TASK_ID_BYTES + 1],
                           char names[][PROJECT_NAME_BYTES + 1], int max);

#endif // CORE_PAYLOAD_H
