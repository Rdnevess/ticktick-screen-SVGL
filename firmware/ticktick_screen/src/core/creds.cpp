#include "creds.h"

#include <ArduinoJson.h>

#include <cstring>

// Zera memoria pelo ponteiro volatile, que o otimizador nao remove.
static void secure_zero(void *p, size_t n) {
    volatile unsigned char *vp = reinterpret_cast<volatile unsigned char *>(p);
    for (size_t i = 0; i < n; i++) vp[i] = 0;
}

static int b64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62; // '-' e '_': variante url-safe
    if (c == '/' || c == '_') return 63;
    return -1;
}

int base64_decode(const char *in, uint8_t *out, size_t outMax) {
    if (in == nullptr) return -1;
    uint32_t acc = 0; // so os ultimos 14 bits importam; o resto pode transbordar
    int bits = 0;
    size_t n = 0;
    for (const char *p = in; *p; p++) {
        const char c = *p;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if (c == '=') break;
        const int v = b64_value(c);
        if (v < 0) return -1;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= outMax) return -1;
            out[n++] = (uint8_t)((acc >> bits) & 0xFF);
        }
    }
    return (int)n;
}

bool creds_valid(const Creds &c) {
    return c.cid[0] != '\0' && c.csec[0] != '\0' && c.atok[0] != '\0';
}

// Copia um campo string. false se faltar (sendo obrigatorio) ou nao couber.
static bool take(JsonVariantConst v, char *dst, size_t cap, bool required) {
    const char *s = v | "";
    const size_t n = std::strlen(s);
    if (n == 0) {
        dst[0] = '\0';
        return !required;
    }
    if (n > cap) return false;
    std::memcpy(dst, s, n + 1);
    return true;
}

bool creds_from_json(const char *json, size_t len, Creds &out) {
    if (json == nullptr || len == 0) return false;
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;

    Creds c{};
    const bool ok = take(doc["cid"], c.cid, CRED_ID_BYTES, true) &&
                    take(doc["csec"], c.csec, CRED_SECRET_BYTES, true) &&
                    take(doc["atok"], c.atok, CRED_TOKEN_BYTES, true) &&
                    take(doc["rtok"], c.rtok, CRED_TOKEN_BYTES, false);
    if (ok) {
        c.exp = doc["exp"] | (int64_t)0;
        out = c;
    }
    creds_wipe(c);
    return ok;
}

bool creds_from_blob(const char *blob, Creds &out) {
    uint8_t raw[CREDS_JSON_MAX];
    const int n = base64_decode(blob, raw, sizeof(raw));
    bool ok = n > 0 && creds_from_json((const char *)raw, (size_t)n, out);
    secure_zero(raw, sizeof(raw));
    return ok;
}

int creds_to_json(const Creds &c, char *out, size_t outMax) {
    JsonDocument doc;
    doc["cid"] = c.cid;
    doc["csec"] = c.csec;
    doc["atok"] = c.atok;
    doc["rtok"] = c.rtok;
    doc["exp"] = c.exp;
    if (measureJson(doc) + 1 > outMax) return -1;
    return (int)serializeJson(doc, out, outMax);
}

bool creds_apply_token_response(Creds &c, const char *json, size_t len,
                                int64_t nowUtc) {
    if (json == nullptr || len == 0) return false;
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) return false;

    char atok[CRED_TOKEN_BYTES + 1];
    char rtok[CRED_TOKEN_BYTES + 1];
    if (!take(doc["access_token"], atok, CRED_TOKEN_BYTES, true)) return false;
    if (!take(doc["refresh_token"], rtok, CRED_TOKEN_BYTES, false)) return false;

    std::memcpy(c.atok, atok, sizeof(atok));
    if (rtok[0] != '\0') std::memcpy(c.rtok, rtok, sizeof(rtok));
    const int64_t expiresIn = doc["expires_in"] | (int64_t)0;
    // Sem hora valida nao da para calcular quando o token vence: fica marcado
    // como desconhecido (0) em vez de um exp errado calculado a partir de 0.
    c.exp = nowUtc > 0 ? nowUtc + (expiresIn > 0 ? expiresIn : CREDS_DEFAULT_LIFETIME_S) : 0;

    secure_zero(atok, sizeof(atok));
    secure_zero(rtok, sizeof(rtok));
    return true;
}

void creds_wipe(Creds &c) {
    secure_zero(&c, sizeof(c));
}
