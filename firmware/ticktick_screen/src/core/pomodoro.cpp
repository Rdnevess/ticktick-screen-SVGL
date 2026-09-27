#include "pomodoro.h"

#include <cstring>

void pomo_start(Pomodoro &p, const Task &t, int64_t nowMs, int64_t durationMs) {
    p.state = PomoState::Running;
    task_set_id(p.projectId, t.projectId);
    task_set_id(p.taskId, t.id);
    std::memcpy(p.title, t.title, std::strlen(t.title) + 1);
    p.startMs = nowMs;
    p.durationMs = durationMs;
    p.taskGone = false;
}

void pomo_tick(Pomodoro &p, int64_t nowMs) {
    if (p.state != PomoState::Running) return; // Finished espera a escolha
    if (nowMs - p.startMs >= p.durationMs) p.state = PomoState::Finished;
}

int64_t pomo_remaining_ms(const Pomodoro &p, int64_t nowMs) {
    if (p.state == PomoState::Idle) return 0;
    int64_t left = p.durationMs - (nowMs - p.startMs);
    return left > 0 ? left : 0;
}

void pomo_renew(Pomodoro &p, int64_t nowMs) {
    p.state = PomoState::Running;
    p.startMs = nowMs; // mesma tarefa, mesma duracao, mesmo taskGone
}

void pomo_cancel(Pomodoro &p) {
    p.state = PomoState::Idle;
    p.projectId[0] = '\0';
    p.taskId[0] = '\0';
    p.title[0] = '\0';
    p.startMs = 0;
    p.durationMs = 0;
    p.taskGone = false;
}

bool pomo_can_complete(const Pomodoro &p) {
    return p.state != PomoState::Idle && !p.taskGone;
}

void pomo_mark_task_gone(Pomodoro &p) { p.taskGone = true; }

bool pomo_is_for(const Pomodoro &p, const Task &t) {
    return std::strcmp(p.projectId, t.projectId) == 0 &&
           std::strcmp(p.taskId, t.id) == 0;
}
