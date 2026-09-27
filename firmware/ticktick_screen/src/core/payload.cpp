#include "payload.h"

#include <ArduinoJson.h>

#include <cstring>

#include "day_filter.h"

// No ArduinoJson 7 o JsonDocument cresce sozinho conforme o parse avanca; nao
// existe capacidade fixa a declarar. O que mantem o consumo baixo e o filtro
// abaixo: os campos nao listados nunca entram na arvore.

int payload_parse_project(const char *json, size_t len, int64_t nowLocal,
                          int32_t tzOffsetSec, TaskTop &acc) {
    if (json == nullptr || len == 0) return -1;

    // Filtro: deixa passar SO o que o Task usa. content, items, reminders,
    // columns e companhia sao descartados durante o parse, nao depois.
    JsonDocument filter;
    JsonObject task = filter["tasks"][0].to<JsonObject>();
    task["id"] = true;
    task["projectId"] = true;
    task["title"] = true;
    task["dueDate"] = true;
    task["isAllDay"] = true;
    task["priority"] = true;
    task["status"] = true;
    task["sortOrder"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, json, len, DeserializationOption::Filter(filter),
        DeserializationOption::NestingLimit(8));
    if (err) return -1;

    JsonArray tasks = doc["tasks"].as<JsonArray>();
    if (tasks.isNull()) return -1; // corpo de erro, ou nem objeto e

    int seen = 0;
    for (JsonObject jt : tasks) {
        const char *id = jt["id"] | "";
        const char *projectId = jt["projectId"] | "";
        if (id[0] == '\0' || projectId[0] == '\0') continue;

        const char *dueDate = jt["dueDate"] | "";
        const bool isAllDay = jt["isAllDay"] | false;
        const int status = jt["status"] | 0;

        int64_t dueTs = 0;
        DayClass cls = day_classify(dueDate, isAllDay, status, nowLocal,
                                    tzOffsetSec, &dueTs);
        if (cls == DayClass::OutOfDay) continue;

        Task t{};
        task_set_id(t.id, id);
        task_set_id(t.projectId, projectId);
        task_set_title(t, jt["title"] | "");
        t.dueTs = dueTs;
        t.sortOrder = jt["sortOrder"] | (int64_t)0;
        t.priority = jt["priority"] | 0;
        t.isAllDay = isAllDay;
        t.overdue = (cls == DayClass::Overdue);
        top_offer(acc, t);
        seen++;
    }
    return seen;
}

int payload_parse_projects(const char *json, size_t len,
                           char ids[][TASK_ID_BYTES + 1],
                           char names[][PROJECT_NAME_BYTES + 1], int max) {
    if (json == nullptr || len == 0) return -1;

    JsonDocument filter;
    JsonObject item = filter[0].to<JsonObject>();
    item["id"] = true;
    item["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, json, len, DeserializationOption::Filter(filter));
    if (err) return -1;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return -1;

    int n = 0;
    for (JsonObject p : arr) {
        if (n >= max) break;
        const char *id = p["id"] | "";
        const char *name = p["name"] | "";
        if (id[0] == '\0') continue;

        task_set_id(ids[n], id);

        size_t nameLen = utf8_truncate_len(name, PROJECT_NAME_BYTES);
        if (nameLen > 0) std::memcpy(names[n], name, nameLen);
        names[n][nameLen] = '\0';
        n++;
    }
    return n;
}
