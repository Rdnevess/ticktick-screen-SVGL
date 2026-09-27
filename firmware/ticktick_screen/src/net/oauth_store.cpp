#include "oauth_store.h"

#include <Preferences.h>

#include <cstring>

#include "../../config.h"
#include "crypto.h"

// Zera memoria pelo ponteiro volatile, que o otimizador nao remove (mesmo
// padrao de core/creds.cpp: o JSON em texto puro e segredo ate a cifra).
static void secure_zero(void *p, size_t n) {
    volatile unsigned char *vp = reinterpret_cast<volatile unsigned char *>(p);
    for (size_t i = 0; i < n; i++) vp[i] = 0;
}

CredsState oauth_state() {
    Preferences p;
    if (!p.begin(NVS_TT, true)) return CredsState::None;
    CredsState s = CredsState::None;
    if (p.isKey("blob")) s = CredsState::Locked;
    else if (p.isKey("creds")) s = CredsState::Plain;
    p.end();
    return s;
}

bool oauth_load_plain(Creds &out) {
    Preferences p;
    if (!p.begin(NVS_TT, true)) return false;
    char buf[CREDS_JSON_MAX];
    const size_t n = p.getString("creds", buf, sizeof(buf));
    p.end();
    const bool ok = n > 0 && creds_from_json(buf, std::strlen(buf), out);
    secure_zero(buf, sizeof(buf));
    return ok;
}

bool oauth_save_plain(const Creds &c) {
    char buf[CREDS_JSON_MAX];
    if (creds_to_json(c, buf, sizeof(buf)) < 0) return false;
    Preferences p;
    bool ok = false;
    if (p.begin(NVS_TT, false)) {
        // Mesma ordem segura do oauth_save_encrypted: grava "creds" antes de
        // mexer no "blob" existente, entao uma falha na escrita nao deixa o
        // aparelho sem nenhuma credencial legivel.
        ok = p.putString("creds", buf) > 0;
        if (ok) p.remove("blob");
        p.end();
    }
    secure_zero(buf, sizeof(buf));
    return ok;
}

bool oauth_save_encrypted(const Creds &c, const char *pin) {
    char buf[CREDS_JSON_MAX];
    if (creds_to_json(c, buf, sizeof(buf)) < 0) return false;
    EncryptedBlob blob{};
    const bool enc = crypto_encrypt(buf, pin, blob);
    secure_zero(buf, sizeof(buf));
    if (!enc) return false;

    Preferences p;
    if (!p.begin(NVS_TT, false)) return false;
    // Grava o blob novo antes de mexer no que ja existia: se a escrita falhar
    // (NVS cheia, por exemplo), o registro em texto puro anterior sobrevive
    // em vez de deixar o aparelho sem nenhuma credencial legivel.
    const bool ok = p.putBytes("blob", &blob, sizeof(blob)) == sizeof(blob);
    if (ok) {
        p.remove("creds");
        p.putInt("pinatt", 0);
    }
    p.end();
    return ok;
}

bool oauth_unlock(const char *pin, Creds &out) {
    Preferences p;
    if (!p.begin(NVS_TT, true)) return false;
    EncryptedBlob blob{};
    const bool read = p.getBytesLength("blob") == sizeof(blob) &&
                      p.getBytes("blob", &blob, sizeof(blob)) == sizeof(blob);
    p.end();
    if (!read) return false;

    char buf[CREDS_JSON_MAX + 1];
    const bool ok = crypto_decrypt(blob, pin, buf, sizeof(buf)) &&
                    creds_from_json(buf, std::strlen(buf), out);
    secure_zero(buf, sizeof(buf));
    return ok;
}

bool oauth_blob_compatible() {
    Preferences p;
    if (!p.begin(NVS_TT, true)) return true;
    const bool ok = !p.isKey("blob") || p.getBytesLength("blob") == sizeof(EncryptedBlob);
    p.end();
    return ok;
}

int oauth_pin_attempts() {
    Preferences p;
    if (!p.begin(NVS_TT, true)) return 0;
    const int n = p.getInt("pinatt", 0);
    p.end();
    return n;
}

void oauth_set_pin_attempts(int n) {
    Preferences p;
    if (p.begin(NVS_TT, false)) {
        p.putInt("pinatt", n);
        p.end();
    }
}

void oauth_wipe() {
    Preferences p;
    if (p.begin(NVS_TT, false)) {
        p.clear();
        p.end();
    }
}
