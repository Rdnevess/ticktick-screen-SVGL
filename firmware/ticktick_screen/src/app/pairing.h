// Ciclo de vida de credenciais novas (spec 6.2). Chegam do portal ou do
// console, sao validadas pelo worker com GET /project, e so depois do 200 vao
// para a NVS. Aceitas, esperam ~2,5 s antes de seguir o fluxo: e o tempo de a
// pagina do portal, que consulta /result a cada segundo, mostrar "pronto".
#ifndef APP_PAIRING_H
#define APP_PAIRING_H

#include <stdint.h>

#include "../core/creds.h"

enum class PairState : uint8_t { Idle, Validating, Accepted, Rejected };

void pairing_begin(); // registra o comando "pair" no console

// false se ja ha validacao em curso ou as credenciais estao incompletas.
bool pairing_submit(const Creds &c);
PairState pairing_state();
int pairing_http(); // status da validacao, para a mensagem de erro

void pairing_on_validated(bool ok, int http); // chamado pelo sync
void pairing_tick();                          // no loop

// Grava as credenciais aceitas (texto puro com pin = nullptr) e as entrega ao
// worker. Quem chama segue o fluxo com app_advance().
void pairing_finish(const char *pin);
void pairing_reset(); // volta a Idle e descarta o que estava pendente

// PIN da sessao: fica na RAM do desbloqueio ate o reboot, para re-cifrar as
// credenciais quando o worker renova o token (sync, CredsChanged). Vazio =
// credenciais em texto puro.
const char *pairing_session_pin();
void pairing_set_session_pin(const char *pin);

#endif // APP_PAIRING_H
