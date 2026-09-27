// Pecas de texto de uma tarefa usadas por Foco e Hoje.
#ifndef UI_TASK_VIEW_H
#define UI_TASK_VIEW_H

#include <stddef.h>
#include <stdint.h>

#include "../core/task.h"

// "ATRASADA" em coral, "Dia todo" para dia inteiro, senao a hora local "HH:MM"
// (spec 7.2 e 7.3). Devolve o texto (em `buf` ou literal) e a cor em *color.
const char *task_when(const Task &t, char *buf, size_t n, uint32_t *color);

#endif // UI_TASK_VIEW_H
