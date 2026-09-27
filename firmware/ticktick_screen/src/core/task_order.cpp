#include "task_order.h"

#include <algorithm>
#include <cstring>

bool task_less(const Task &a, const Task &b) {
    if (a.overdue != b.overdue) return a.overdue;                 // atrasada primeiro
    if (a.priority != b.priority) return a.priority > b.priority; // decrescente
    if (a.dueTs != b.dueTs) return a.dueTs < b.dueTs;
    return a.sortOrder < b.sortOrder;
}

void task_sort(Task *tasks, int count) {
    if (tasks == nullptr || count < 2) return;
    std::sort(tasks, tasks + count, task_less);
}

// ---- Slots do Foco e pins ----

static bool same_task(const Pin &p, const Task &t) {
    return std::strcmp(p.projectId, t.projectId) == 0 &&
           std::strcmp(p.taskId, t.id) == 0;
}

bool pin_contains(const PinSet &pins, const Task &t) {
    for (int i = 0; i < pins.count; i++)
        if (same_task(pins.pins[i], t)) return true;
    return false;
}

PinResult pin_toggle(PinSet &pins, const Task &t) {
    for (int i = 0; i < pins.count; i++) {
        if (same_task(pins.pins[i], t)) {
            // Solta, preservando a ordem dos demais.
            for (int j = i; j < pins.count - 1; j++) pins.pins[j] = pins.pins[j + 1];
            pins.count--;
            return PinResult::Unpinned;
        }
    }
    if (pins.count >= FOCUS_SLOTS) return PinResult::Full;
    Pin &p = pins.pins[pins.count];
    task_set_id(p.projectId, t.projectId);
    task_set_id(p.taskId, t.id);
    pins.count++;
    return PinResult::Pinned;
}

int pin_prune(PinSet &pins, const Task *tasks, int count) {
    int removed = 0;
    for (int i = 0; i < pins.count;) {
        bool found = false;
        for (int k = 0; k < count; k++) {
            if (same_task(pins.pins[i], tasks[k])) { found = true; break; }
        }
        if (found) { i++; continue; }
        for (int j = i; j < pins.count - 1; j++) pins.pins[j] = pins.pins[j + 1];
        pins.count--;
        removed++;
    }
    return removed;
}

int focus_select(const Task *tasks, int count, const PinSet &pins, int *outIdx) {
    int filled = 0;
    if (tasks == nullptr || count <= 0) return 0;

    // 1. Pins, na ordem em que foram fixados.
    for (int i = 0; i < pins.count && filled < FOCUS_SLOTS; i++) {
        for (int k = 0; k < count; k++) {
            if (same_task(pins.pins[i], tasks[k])) { outIdx[filled++] = k; break; }
        }
    }
    // 2. O resto pela ordem da lista, sem repetir o que ja entrou.
    for (int k = 0; k < count && filled < FOCUS_SLOTS; k++) {
        bool already = false;
        for (int i = 0; i < filled; i++) if (outIdx[i] == k) { already = true; break; }
        if (!already) outIdx[filled++] = k;
    }
    return filled;
}

// ---- TaskTop ----

void top_init(TaskTop &top, Task *storage, int cap) {
    top.items = storage;
    top.cap = cap;
    top.count = 0;
    top.seen = 0;
}

void top_offer(TaskTop &top, const Task &t) {
    top.seen++;
    if (top.cap <= 0) return;
    // Cheio e nao melhor que a pior: fica de fora (so conta no seen).
    if (top.count == top.cap && !task_less(t, top.items[top.count - 1])) return;

    // Posicao livre no fim, ou a da pior quando cheio (ela sai).
    int i = (top.count < top.cap) ? top.count : top.cap - 1;
    // Empate nao passa na frente (task_less estrito): ordem de chegada mantida.
    while (i > 0 && task_less(t, top.items[i - 1])) {
        top.items[i] = top.items[i - 1];
        i--;
    }
    top.items[i] = t;
    if (top.count < top.cap) top.count++;
}
