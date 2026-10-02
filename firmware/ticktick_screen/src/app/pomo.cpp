#include "pomo.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "../platform/clock.h"
#include "../platform/console.h"
#include "../platform/settings.h"
#include "../ui/pomo_overlay.h"
#include "../ui/shell.h"
#include "../ui/theme.h"
#include "sync.h"

static Pomodoro s_p; // zero-inicializado: Idle
static PomoView s_view = PomoView::None;
static Task s_pending;      // tarefa que espera a confirmacao de troca
static int s_shownSec = -1; // segundo mostrado no header; -1 forca, -2 = header livre

const Pomodoro &pomo_app_state() { return s_p; }
PomoView pomo_app_view() { return s_view; }
void pomo_app_set_view(PomoView v) { s_view = v; }

static int64_t duration_ms() { return (int64_t)settings().pomoMin * 60 * 1000; }

static void start(const Task &t) {
    pomo_start(s_p, t, clock_uptime_ms(), duration_ms());
    s_view = PomoView::Expanded; // abre a contagem; tocar fora dos botoes recolhe
    s_shownSec = -1;
}

void pomo_app_request_start(const Task &t) {
    if (s_p.state == PomoState::Idle) {
        start(t);
        return;
    }
    if (pomo_is_for(s_p, t)) { // o mesmo: so mostra
        s_view = PomoView::Expanded;
        return;
    }
    s_pending = t;
    s_view = PomoView::ConfirmSwap; // um ciclo por vez (spec 5.6)
}

void pomo_app_confirm_swap(bool swap) {
    s_view = PomoView::None;
    if (!swap) return;
    pomo_cancel(s_p);
    start(s_pending);
}

void pomo_app_toggle_expanded() {
    if (s_p.state != PomoState::Running) return;
    s_view = (s_view == PomoView::Expanded) ? PomoView::None : PomoView::Expanded;
}

void pomo_app_cancel() {
    pomo_cancel(s_p);
    s_view = PomoView::None;
    s_shownSec = -1;
}

// Header: "mm:ss" que falta, no lugar do contador do dia (spec 7.6). Reescreve
// so quando o segundo muda.
static void update_header() {
    if (s_p.state == PomoState::Idle) {
        if (s_shownSec != -2) {
            shell_set_counter_override(nullptr, COL_ACCENT);
            s_shownSec = -2;
        }
        return;
    }
    const int64_t ms = pomo_remaining_ms(s_p, clock_uptime_ms());
    const int sec = (int)((ms + 999) / 1000); // o ultimo segundo mostra 00:01
    if (sec == s_shownSec) return;
    s_shownSec = sec;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", sec / 60, sec % 60);
    shell_set_counter_override(buf, s_p.state == PomoState::Finished ? COL_OK : COL_ACCENT);
}

void pomo_app_tick() {
    const PomoState before = s_p.state;
    pomo_tick(s_p, clock_uptime_ms());
    if (before == PomoState::Running && s_p.state == PomoState::Finished) {
        s_view = PomoView::Finished; // dialogo em qualquer pagina, sem expirar
        Serial.println("[pomo] ciclo terminou");
    }
    update_header();
    pomo_overlay_tick();
}

void pomo_app_renew() {
    pomo_renew(s_p, clock_uptime_ms()); // mesma tarefa, mesma duracao (spec 5.6)
    s_view = PomoView::None;
    s_shownSec = -1;
}

void pomo_app_complete() {
    if (!pomo_can_complete(s_p)) return;
    int n = 0;
    const Task *v = sync_visible(&n);
    for (int i = 0; i < n; i++) {
        if (pomo_is_for(s_p, v[i])) {
            const Task t = v[i]; // copia: o store muda dentro de sync_complete
            pomo_app_cancel();
            sync_complete(t);   // conclusao otimista de sempre
            return;
        }
    }
    // Sumiu entre o fim do ciclo e o toque (concluida no celular): o dialogo
    // se redesenha sem o Concluir e explica.
    pomo_mark_task_gone(s_p);
    pomo_overlay_rebuild();
}

void pomo_app_on_tasks_changed() {
    if (s_p.state == PomoState::Idle || s_p.taskGone) return;
    const TaskStore &st = sync_store();
    for (int i = 0; i < st.count; i++)
        if (pomo_is_for(s_p, st.tasks[i]) && !st.tasks[i].pending) return;
    // Concluida no celular, ou pelo check enquanto o ciclo rodava (spec 5.6):
    // o contador segue, mas o fim nao oferece Concluir.
    pomo_mark_task_gone(s_p);
    if (s_view == PomoView::Finished) pomo_overlay_rebuild();
}

// ---- Console (atalhos de verificacao) ----

static void cmd_pomo(const char *a) {
    if (std::strcmp(a, "start") == 0) {
        int n = 0;
        const Task *v = sync_visible(&n);
        if (n == 0) {
            Serial.println("pomo: nenhuma tarefa visivel");
            return;
        }
        pomo_app_request_start(v[0]);
        Serial.println("pomo: pedido de inicio na primeira tarefa");
    } else if (std::strcmp(a, "stop") == 0) {
        pomo_app_cancel();
        Serial.println("pomo: cancelado");
    } else if (std::strcmp(a, "fim") == 0) {
        if (s_p.state != PomoState::Running) {
            Serial.println("pomo: nada rodando");
            return;
        }
        s_p.startMs = clock_uptime_ms() - s_p.durationMs; // termina no proximo tick
        Serial.println("pomo: fim antecipado");
    } else {
        Serial.println("uso: pomo start|stop|fim");
    }
}

void pomo_app_begin() {
    console_add("pomo", "pomo start|stop|fim  atalhos do pomodoro", cmd_pomo);
}
