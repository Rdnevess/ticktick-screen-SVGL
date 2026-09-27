// Orquestrador: decide a pagina do fluxo de primeiro boot (spec 7.9), troca de
// pagina fora dos callbacks do LVGL e bombeia os ticks. O .ino so chama
// app_begin() e app_loop().
#ifndef APP_APP_H
#define APP_APP_H

#include <lvgl.h>
#include <stdint.h>

enum class Page : uint8_t { Boot, Wifi, WaitTime, Pair, PinSet, PinEnter, Lists, Main,
                            Clock, Settings, PinAdmin, Count };

struct PageOps {
    void (*build)(lv_obj_t *scr); // monta na tela ja limpa e pintada de COL_BG
    void (*tick)();               // a cada volta do loop enquanto ativa (ou nullptr)
    void (*leave)();              // ao sair (ou nullptr): soltar ponteiros, desligar portal
};

void app_register(Page p, const PageOps &ops);
void app_begin();
void app_loop();

// Troca de pagina diferida para o proximo app_loop: nunca destruir a tela de
// dentro do callback que a esta usando. Pedir a pagina atual a reconstroi.
void app_go(Page p);
Page app_page();

// Como app_go, mas esquece qualquer volta pendente antes: usar quando a nova
// pagina nao e filha de ninguem (ex.: token recusado durante o uso normal),
// senao o "Voltar" ficaria apontando para uma pagina orfa (item 5 da revisao
// final).
void app_go_root(Page p);

// Reavalia o fluxo de primeiro boot e vai para a pagina da etapa pendente.
void app_advance();

// A pagina de WiFi conectou: inicia o SNTP e segue o fluxo.
void app_wifi_connected();

// Pagina filha (adendo do Plano C, secao 4): o Settings abre Listas, WiFi,
// Pareamento e PIN "por cima" e eles voltam para quem os abriu. app_back() vai
// para quem abriu ou, se ninguem abriu, segue o fluxo (app_advance). Entrar na
// tela principal esquece qualquer volta pendente.
void app_open_child(Page child);
bool app_is_child();
void app_back();

#endif // APP_APP_H
