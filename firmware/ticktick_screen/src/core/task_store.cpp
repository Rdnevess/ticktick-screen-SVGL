#include "task_store.h"

#include <cstring>

static bool same(const Task &t, const char *projectId, const char *taskId) {
    return std::strcmp(t.projectId, projectId) == 0 && std::strcmp(t.id, taskId) == 0;
}

static int find(const TaskStore &s, const char *projectId, const char *taskId) {
    for (int i = 0; i < s.count; i++)
        if (same(s.tasks[i], projectId, taskId)) return i;
    return -1;
}

void store_replace(TaskStore &s, const Task *src, int n, int totalSeen) {
    int keep = n;
    if (keep < 0) keep = 0;
    if (keep > TASK_LIST_MAX) keep = TASK_LIST_MAX;

    // Antes de sobrescrever, marca quais das novas ainda estavam em voo. Um
    // bit por posicao em vez de copiar as Task: uma copia local seriam ~11 KB,
    // mais que a pilha inteira do loopTask do ESP32.
    static_assert(TASK_LIST_MAX <= 64, "a mascara de em-voo tem 64 bits");
    uint64_t inflight = 0;
    for (int j = 0; j < s.count; j++) {
        if (!s.tasks[j].pending) continue;
        for (int i = 0; i < keep; i++) {
            if (same(src[i], s.tasks[j].projectId, s.tasks[j].id)) {
                inflight |= (uint64_t)1 << i;
                break;
            }
        }
    }

    for (int i = 0; i < keep; i++) {
        s.tasks[i] = src[i];
        s.tasks[i].syncError = false; // o estado que vale e o do servidor
        s.tasks[i].pending = (inflight >> i) & 1; // em voo continua escondida
    }
    s.count = keep;
    const int total = (totalSeen < 0) ? n : totalSeen;
    s.truncated = (total > keep) ? (total - keep) : 0;
    s.stale = false;
}

bool store_mark_pending(TaskStore &s, const char *projectId, const char *taskId) {
    int i = find(s, projectId, taskId);
    if (i < 0) return false;
    s.tasks[i].pending = true;
    s.tasks[i].syncError = false;
    return true;
}

bool store_confirm_pending(TaskStore &s, const char *projectId, const char *taskId) {
    int i = find(s, projectId, taskId);
    if (i < 0) return false;
    for (int j = i; j < s.count - 1; j++) s.tasks[j] = s.tasks[j + 1];
    s.count--;
    return true;
}

bool store_revert_pending(TaskStore &s, const char *projectId, const char *taskId) {
    int i = find(s, projectId, taskId);
    if (i < 0) return false;
    s.tasks[i].pending = false;
    s.tasks[i].syncError = true;
    return true;
}

void store_mark_stale(TaskStore &s) { s.stale = true; }

int store_visible(const TaskStore &s, Task *out, int max) {
    int n = 0;
    for (int i = 0; i < s.count && n < max; i++)
        if (!s.tasks[i].pending) out[n++] = s.tasks[i];
    return n;
}

int store_visible_count(const TaskStore &s) {
    int n = 0;
    for (int i = 0; i < s.count; i++) if (!s.tasks[i].pending) n++;
    return n;
}
