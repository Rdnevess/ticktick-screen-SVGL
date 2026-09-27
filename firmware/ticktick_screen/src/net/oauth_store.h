// Credenciais na NVS "tt" (spec 6.3). Um registro so: o JSON de core/creds, em
// texto puro ("creds") ou cifrado com o PIN ("blob", Task 9). Gravar um apaga
// o outro, entao o estado e sempre um dos tres abaixo.
#ifndef NET_OAUTH_STORE_H
#define NET_OAUTH_STORE_H

#include <stdint.h>

#include "../core/creds.h"

enum class CredsState : uint8_t { None, Plain, Locked };

CredsState oauth_state();
bool oauth_load_plain(Creds &out);
bool oauth_save_plain(const Creds &c);

// Grava cifrado com o PIN (apaga a versao em texto puro).
bool oauth_save_encrypted(const Creds &c, const char *pin);
// Decifra. false = PIN errado (ou registro corrompido).
bool oauth_unlock(const char *pin, Creds &out);
// false quando ha um registro cifrado de tamanho diferente do atual (versao
// antiga do firmware): nao da para decifrar com PIN nenhum.
bool oauth_blob_compatible();

// Tentativas erradas seguidas, persistidas: desligar o aparelho nao zera.
int oauth_pin_attempts();
void oauth_set_pin_attempts(int n);

void oauth_wipe(); // apaga o namespace inteiro (credenciais e tentativas de PIN)

#endif // NET_OAUTH_STORE_H
