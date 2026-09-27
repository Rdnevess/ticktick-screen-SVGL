// AES-256-GCM com chave derivada do PIN: SHA-256 iterado KDF_ROUNDS vezes
// sobre PIN + sal. Adaptado de claude-usage-stick-SVGL (crypto.cpp), com sal
// aleatorio a cada gravacao em vez do MAC fixo.
//
// Limite honesto: um PIN de 4 digitos tem 10^4 combinacoes. Quem copiar a
// flash testa todas offline. O que protege e o bloqueio e o apagamento na
// tela, nao a cifra: e defesa contra olhar casual e contra quem pega o
// aparelho, nao contra analise da flash.
#ifndef NET_CRYPTO_H
#define NET_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#include "../core/creds.h"

struct EncryptedBlob {
    uint8_t salt[16];
    uint8_t iv[12];
    uint8_t tag[16];
    uint16_t len;
    uint8_t ciphertext[CREDS_JSON_MAX];
};

bool crypto_encrypt(const char *plain, const char *pin, EncryptedBlob &out);
// false = PIN errado ou blob corrompido (a tag do GCM nao confere).
bool crypto_decrypt(const EncryptedBlob &b, const char *pin, char *out, size_t outMax);

#endif // NET_CRYPTO_H
