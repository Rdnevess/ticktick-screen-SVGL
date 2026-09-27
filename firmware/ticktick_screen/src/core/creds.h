// Credenciais do TickTick e o blob de pareamento (spec 6.1 a 6.4).
//
// O blob e o contrato com helper/pair.py: base64 de um JSON compacto com cid,
// csec, atok, rtok e exp. O mesmo JSON (sem o base64) e o que a NVS guarda,
// em texto puro ou cifrado com o PIN (net/oauth_store).
#ifndef CORE_CREDS_H
#define CORE_CREDS_H

#include <cstddef>
#include <cstdint>

constexpr int CRED_ID_BYTES = 64;      // client_id
constexpr int CRED_SECRET_BYTES = 96;  // client_secret
constexpr int CRED_TOKEN_BYTES = 256;  // tamanho dos tokens nao e documentado: folga
constexpr int CREDS_JSON_MAX = 1024;   // cabe o registro inteiro com folga
constexpr int64_t CREDS_DEFAULT_LIFETIME_S = 150LL * 86400; // spec 6.4

struct Creds {
    char cid[CRED_ID_BYTES + 1];
    char csec[CRED_SECRET_BYTES + 1];
    char atok[CRED_TOKEN_BYTES + 1];
    char rtok[CRED_TOKEN_BYTES + 1]; // vazio = sem refresh token
    int64_t exp;                     // epoch UTC; 0 = desconhecido
};

// base64 padrao ou url-safe. Ignora espacos e quebras de linha (colar do
// terminal traz) e para no primeiro '='. Devolve os bytes escritos, ou -1 para
// caractere invalido ou saida pequena.
int base64_decode(const char *in, uint8_t *out, size_t outMax);

// cid, csec e atok presentes. rtok e exp sao opcionais.
bool creds_valid(const Creds &c);

// Blob do helper -> Creds. false se o base64 ou o JSON forem invalidos, se faltar
// campo obrigatorio ou se algum nao couber — truncar um token o estragaria em
// silencio.
bool creds_from_blob(const char *blob, Creds &out);

// JSON compacto para a NVS. Devolve o tamanho, ou -1 se nao couber em outMax.
int creds_to_json(const Creds &c, char *out, size_t outMax);
bool creds_from_json(const char *json, size_t len, Creds &out);

// Resposta de POST /oauth/token: troca atok, troca rtok so se vier um novo e
// recalcula exp (expires_in ou o padrao de 150 dias). false = resposta sem
// access_token, token maior que CRED_TOKEN_BYTES, ou JSON invalido; nesse caso
// `c` fica intocado.
bool creds_apply_token_response(Creds &c, const char *json, size_t len,
                                int64_t nowUtc);

// Zera a struct inteira por um ponteiro volatile, que o otimizador nao remove.
void creds_wipe(Creds &c);

#endif // CORE_CREDS_H
