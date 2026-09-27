#include "settings_values.h"

#include <cstdio>

static const int POLL_CHOICES[] = {1, 2, 5, 10, 15, 30};
static const int POMO_CHOICES[] = {15, 20, 25, 30, 45, 50};

static int choice_step(const int *v, int n, int cur, int dir) {
    if (dir > 0) {
        for (int i = 0; i < n; i++)
            if (v[i] > cur) return v[i];
        return v[n - 1];
    }
    for (int i = n - 1; i >= 0; i--)
        if (v[i] < cur) return v[i];
    return v[0];
}

int poll_step(int cur, int dir) {
    return choice_step(POLL_CHOICES, (int)(sizeof(POLL_CHOICES) / sizeof(int)), cur, dir);
}

int pomo_step(int cur, int dir) {
    return choice_step(POMO_CHOICES, (int)(sizeof(POMO_CHOICES) / sizeof(int)), cur, dir);
}

int tz_step(int cur, int dir) {
    int next;
    if (cur % TZ_STEP_MINUTES == 0) {
        // Alinhado: anda um passo na direcao pedida
        next = cur + (dir > 0 ? TZ_STEP_MINUTES : -TZ_STEP_MINUTES);
    } else {
        // Fora do passo: alinha no multiplo de 15 na direcao pedida.
        // Calcula o piso da divisao (divisao com piso).
        int q = cur / TZ_STEP_MINUTES;
        if ((cur % TZ_STEP_MINUTES != 0) && ((cur < 0) != (TZ_STEP_MINUTES < 0))) {
            q--;
        }
        // q * TZ_STEP_MINUTES eh o multiplo imediatamente menor que cur
        // (q + 1) * TZ_STEP_MINUTES eh o multiplo imediatamente maior que cur
        if (dir > 0) {
            // Alinha para cima (teto): proximo multiplo maior que cur
            next = (q + 1) * TZ_STEP_MINUTES;
        } else {
            // Alinha para baixo (piso): multiplo menor ou igual a cur
            next = q * TZ_STEP_MINUTES;
        }
    }
    if (next < TZ_MIN_MINUTES) next = TZ_MIN_MINUTES;
    if (next > TZ_MAX_MINUTES) next = TZ_MAX_MINUTES;
    return next;
}

void tz_format(int min, char *out, size_t n) {
    const int a = min < 0 ? -min : min;
    std::snprintf(out, n, "UTC%c%d:%02d", min < 0 ? '-' : '+', a / 60, a % 60);
}
