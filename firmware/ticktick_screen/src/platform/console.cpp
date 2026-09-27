#include "console.h"

#include <Arduino.h>

#include <cstring>

namespace {
struct Command {
    const char *name;
    const char *help;
    ConsoleFn fn;
};
constexpr int MAX_COMMANDS = 24;
// Nome sem sufixo _MAX: LINE_MAX ja e macro do limits.h do toolchain xtensa.
constexpr size_t CONSOLE_LINE_MAX = 1200; // o blob de pareamento tem umas 400 letras

Command s_cmds[MAX_COMMANDS];
int s_count = 0;
char s_line[CONSOLE_LINE_MAX];
size_t s_len = 0;
bool s_overflow = false;
} // namespace

void console_add(const char *name, const char *help, ConsoleFn fn) {
    if (s_count < MAX_COMMANDS) s_cmds[s_count++] = {name, help, fn};
}

// Zera memoria pelo ponteiro volatile, que o otimizador nao remove (mesmo
// padrao de core/creds.cpp): a linha pode ter tido um segredo colado.
static void wipe_line() {
    volatile char *v = s_line;
    for (size_t i = 0; i < CONSOLE_LINE_MAX; i++) v[i] = 0;
}

static void run_line(char *line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    char *args = std::strchr(line, ' ');
    if (args) {
        *args++ = '\0';
        while (*args == ' ') args++;
    } else {
        args = line + std::strlen(line); // string vazia
    }

    if (std::strcmp(line, "help") == 0) {
        for (int i = 0; i < s_count; i++)
            Serial.printf("  %-11s %s\n", s_cmds[i].name, s_cmds[i].help);
        return;
    }
    for (int i = 0; i < s_count; i++) {
        if (std::strcmp(line, s_cmds[i].name) == 0) {
            s_cmds[i].fn(args);
            return;
        }
    }
    // O "comando" pode ser um blob de pareamento colado sem o prefixo "pair "
    // (segredo inteiro): ecoa no maximo os 4 primeiros caracteres seguidos de
    // "…", nunca a linha toda (constraint global de segredo no Serial).
    char head[5];
    const size_t n = std::strlen(line);
    const size_t take = n < 4 ? n : 4;
    std::memcpy(head, line, take);
    head[take] = '\0';
    Serial.printf("comando desconhecido: %s… ('help' lista os comandos)\n", head);
}

void console_poll() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (s_overflow) Serial.println("linha longa demais; ignorada");
            else if (s_len > 0) {
                s_line[s_len] = '\0';
                run_line(s_line);
            }
            wipe_line(); // linha usada (ou descartada por excesso): zera antes do proximo char
            s_len = 0;
            s_overflow = false;
            continue;
        }
        if (s_len + 1 < CONSOLE_LINE_MAX) s_line[s_len++] = (char)c;
        else s_overflow = true;
    }
}
