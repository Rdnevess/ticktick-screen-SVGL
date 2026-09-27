// Portal de pareamento (spec 6.2): WebServer na porta 80 + mDNS, ligado so
// enquanto a pagina de pareamento esta aberta. HTTP simples na LAN e o elo mais
// fraco da cadeia, e por isso o portal nao fica ligado depois de parear.
#ifndef UI_ONBOARDING_WEB_H
#define UI_ONBOARDING_WEB_H

void portal_start();
void portal_stop();
bool portal_running();
void portal_tick(); // handleClient; chamar no loop enquanto roda

#endif // UI_ONBOARDING_WEB_H
