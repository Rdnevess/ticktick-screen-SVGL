// Filtro do dia: hoje + atrasadas, conforme spec 5.2.
//
// O nucleo trabalha em EPOCH LOCAL = epoch UTC + tzOffsetSec. A casca fornece o
// offset vigente, o que deixa este modulo alheio a horario de verao.
#ifndef CORE_DAY_FILTER_H
#define CORE_DAY_FILTER_H

#include <cstdint>

enum class DayClass { OutOfDay, Today, Overdue };

// Dias desde 1970-01-01 para uma data civil (algoritmo de Howard Hinnant).
int64_t days_from_civil(int y, unsigned m, unsigned d);

// "2026-09-24T14:30:00.000+0000" | "...Z" | "...-0300" -> epoch UTC.
// Devolve false para entrada nula, vazia ou malformada.
bool parse_iso8601_utc(const char *s, int64_t *outUtc);

// Le apenas a parte YYYY-MM-DD, ignorando hora e fuso. E o que salva as
// tarefas de dia inteiro da conversao de fuso (ver day_classify).
bool parse_iso8601_date(const char *s, int *y, int *m, int *d);

int64_t start_of_local_day(int64_t nowLocal);
int64_t end_of_local_day(int64_t nowLocal);

// Classifica uma tarefa e devolve em *outDueTsLocal o instante local usado para
// ordenar (all-day recebe 23:59:59 do seu dia). *outDueTsLocal fica 0 quando a
// tarefa nao pertence ao dia.
DayClass day_classify(const char *dueDate, bool isAllDay, int status,
                      int64_t nowLocal, int32_t tzOffsetSec,
                      int64_t *outDueTsLocal);

#endif // CORE_DAY_FILTER_H
