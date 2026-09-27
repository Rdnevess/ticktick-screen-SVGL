#include "task_view.h"

#include <cstdio>

#include "i18n.h"
#include "theme.h"

const char *task_when(const Task &t, char *buf, size_t n, uint32_t *color) {
    if (t.overdue) {
        *color = COL_LATE;
        return TRS("ATRASADA", "OVERDUE");
    }
    *color = COL_DIM;
    if (t.isAllDay) return TRS("Dia todo", "All day");
    std::snprintf(buf, n, "%02d:%02d", (int)((t.dueTs / 3600) % 24), (int)((t.dueTs / 60) % 60));
    return buf;
}
