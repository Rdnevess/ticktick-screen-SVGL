// Politica de rede sem rede: quando renovar o token (spec 6.4) e quanto
// esperar depois de um 429 (spec 8). O worker pergunta, estas funcoes decidem.
#ifndef CORE_NET_POLICY_H
#define CORE_NET_POLICY_H

#include <cstdint>

constexpr int64_t RENEW_BEFORE_S = 7LL * 86400; // renova com menos de 7 dias
constexpr int64_t BACKOFF_MAX_S = 30LL * 60;    // teto do recuo exponencial

// true se faltam menos de RENEW_BEFORE_S para `exp`. exp = 0 (desconhecido)
// ou nowUtc = 0 (sem hora) devolvem false: nesse caso a autoridade e o 401.
bool policy_should_renew(int64_t nowUtc, int64_t exp);

// Proximo intervalo depois de um 429: dobra o maior entre o corrente e o
// normal, com teto BACKOFF_MAX_S, e nunca abaixo do normal.
int64_t policy_backoff(int64_t currentS, int64_t normalS);

#endif // CORE_NET_POLICY_H
