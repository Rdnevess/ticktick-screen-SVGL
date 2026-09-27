// Chamadas HTTPS ao TickTick (spec 6.5 e 6.6). SO o worker de rede usa este
// modulo: nada aqui e seguro para duas tasks ao mesmo tempo.
//
// Uma conexao por ciclo: WiFiClientSecure + HTTPClient com setReuse(true), e
// as N listas pagam um handshake TLS so. api_close() no fim do ciclo.
#ifndef NET_TICKTICK_API_H
#define NET_TICKTICK_API_H

#include "../core/creds.h"
#include "psram_sink.h"

// Codigos negativos proprios. Os do HTTPClient vao de -1 a -11.
constexpr int NET_ERR_PARSE = -100;   // 200 com corpo que o parser recusou
constexpr int NET_ERR_NOWIFI = -101;
constexpr int NET_ERR_NOCREDS = -102;
constexpr int NET_ERR_MEMORY = -103;  // corpo maior que o PsramSink aceita

// true = setInsecure() no lugar da cadeia embutida (interruptor da spec 6.5).
void api_set_insecure(bool insecure);

// GET API_BASE + path com Bearer. Corpo em `sink` (limpo antes). Devolve o
// status HTTP (> 0) ou um codigo negativo de falha de rede/TLS.
int api_get(const char *path, const char *token, PsramSink &sink);

// POST API_BASE + path sem corpo (complete, spec 6.6).
int api_post_empty(const char *path, const char *token);

// POST OAUTH_TOKEN_URL com grant_type=refresh_token e Basic auth (spec 6.4).
int api_refresh_token(const Creds &c, PsramSink &sink);

void api_close();

#endif // NET_TICKTICK_API_H
