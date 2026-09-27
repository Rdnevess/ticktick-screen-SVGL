// Console serial de manutencao (115200, uma linha por comando).
//
// Verifica cada etapa antes de a UI dela existir e fica como caminho de
// socorro (colar o blob de pareamento quando a rede local atrapalha). Cada
// modulo registra os seus comandos; o app chama console_poll() no loop.
#ifndef PLATFORM_CONSOLE_H
#define PLATFORM_CONSOLE_H

typedef void (*ConsoleFn)(const char *args);

// `name` e `help` precisam viver para sempre (use literais).
void console_add(const char *name, const char *help, ConsoleFn fn);

// Le o que chegou no Serial sem bloquear e executa a linha no '\n' ou '\r'.
// "help" e embutido e lista os comandos registrados.
void console_poll();

#endif // PLATFORM_CONSOLE_H
