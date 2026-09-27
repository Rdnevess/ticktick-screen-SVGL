// Modelo de tarefa. Nucleo puro: sem LVGL, sem Arduino, sem globais.
#ifndef CORE_TASK_H
#define CORE_TASK_H

#include <cstddef>
#include <cstdint>

// Tetos de tamanho. Struct de tamanho fixo: sem heap, sem fragmentacao.
constexpr int TASK_ID_BYTES = 32;
constexpr int TASK_TITLE_BYTES = 80;
constexpr int TASK_LIST_MAX = 64; // teto de tarefas por ciclo (spec 5.1)

// Prioridades do TickTick: 0 nenhuma, 1 baixa, 3 media, 5 alta.
constexpr int PRIO_NONE = 0;
constexpr int PRIO_LOW = 1;
constexpr int PRIO_MED = 3;
constexpr int PRIO_HIGH = 5;

struct Task {
    char id[TASK_ID_BYTES + 1];
    char projectId[TASK_ID_BYTES + 1];
    char title[TASK_TITLE_BYTES + 1];
    int64_t dueTs;     // epoch LOCAL em segundos; 0 = sem data
    int64_t sortOrder; // desempate final, vem do TickTick
    int priority;      // 0/1/3/5
    bool isAllDay;
    bool overdue;      // derivado pelo filtro do dia
    bool pending;      // conclusao otimista em voo
    bool syncError;    // a conclusao falhou e a tarefa voltou para a lista
};

// Quantos bytes de `s` cabem em `max_bytes` sem partir um caractere UTF-8.
size_t utf8_truncate_len(const char *s, size_t max_bytes);

// Copia `src` para `dst` deixando so o que as fontes font_pt_* desenham: ASCII
// imprimivel, Latin-1 (0xA0-0xFF) e os simbolos extras do gen_fonts.sh
// (0x2022 0x2014 0x2026 0x201C 0x201D). Emoji e outros scripts somem, controle
// vira espaco, espacos repetidos colapsam e as pontas sao aparadas. Para no
// ultimo caractere que cabe inteiro em dstSize-1 bytes. Devolve o tamanho.
size_t title_sanitize(char *dst, size_t dstSize, const char *src);

// Copia `src` para `t.title` truncando em TASK_TITLE_BYTES, sempre em
// fronteira de caractere. `src` nulo resulta em string vazia.
void task_set_title(Task &t, const char *src);

// Copia `src` para um campo de id, truncando em TASK_ID_BYTES.
void task_set_id(char *dst, const char *src);

#endif // CORE_TASK_H
