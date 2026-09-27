#include "crypto.h"

#include <esp_random.h>
#include <mbedtls/gcm.h>
#include <mbedtls/sha256.h>

#include <cstring>

#include "../../config.h"

// Zera memoria pelo ponteiro volatile, que o otimizador nao remove (mesmo
// padrao de core/creds.cpp: chave e hash intermediario sao segredo).
static void secure_zero(void *p, size_t n) {
    volatile unsigned char *vp = reinterpret_cast<volatile unsigned char *>(p);
    for (size_t i = 0; i < n; i++) vp[i] = 0;
}

static void derive_key(const char *pin, const uint8_t *salt, size_t saltLen, uint8_t key[32]) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    uint8_t hash[32];

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t *)pin, std::strlen(pin));
    mbedtls_sha256_update(&ctx, salt, saltLen);
    mbedtls_sha256_finish(&ctx, hash);
    for (int i = 1; i < KDF_ROUNDS; i++) {
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, hash, sizeof(hash));
        mbedtls_sha256_finish(&ctx, hash);
    }
    mbedtls_sha256_free(&ctx);
    std::memcpy(key, hash, 32);
    secure_zero(hash, sizeof(hash));
}

bool crypto_encrypt(const char *plain, const char *pin, EncryptedBlob &out) {
    const size_t len = std::strlen(plain);
    if (len > sizeof(out.ciphertext)) return false;

    esp_fill_random(out.salt, sizeof(out.salt));
    esp_fill_random(out.iv, sizeof(out.iv));
    uint8_t key[32];
    derive_key(pin, out.salt, sizeof(out.salt), key);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret == 0)
        ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, len, out.iv, sizeof(out.iv),
                                        nullptr, 0, (const uint8_t *)plain, out.ciphertext,
                                        sizeof(out.tag), out.tag);
    mbedtls_gcm_free(&gcm);
    secure_zero(key, sizeof(key));
    out.len = (uint16_t)len;
    return ret == 0;
}

bool crypto_decrypt(const EncryptedBlob &b, const char *pin, char *out, size_t outMax) {
    if (b.len > sizeof(b.ciphertext) || (size_t)b.len + 1 > outMax) return false;

    uint8_t key[32];
    derive_key(pin, b.salt, sizeof(b.salt), key);

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret == 0)
        ret = mbedtls_gcm_auth_decrypt(&gcm, b.len, b.iv, sizeof(b.iv), nullptr, 0, b.tag,
                                       sizeof(b.tag), b.ciphertext, (uint8_t *)out);
    mbedtls_gcm_free(&gcm);
    secure_zero(key, sizeof(key));
    if (ret != 0) {
        secure_zero(out, outMax);
        return false;
    }
    out[b.len] = '\0';
    return true;
}
