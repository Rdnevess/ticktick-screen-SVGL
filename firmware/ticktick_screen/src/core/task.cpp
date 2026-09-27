#include "task.h"

#include <cstring>

// Um byte de continuacao UTF-8 tem os bits altos 10xxxxxx.
static inline bool is_continuation(unsigned char c) { return (c & 0xC0) == 0x80; }

size_t utf8_truncate_len(const char *s, size_t max_bytes) {
    if (s == nullptr) return 0;
    size_t len = std::strlen(s);
    if (len <= max_bytes) return len;

    size_t cut = max_bytes;
    // Recua enquanto o byte no ponto de corte for continuacao: assim o corte
    // cai no inicio de um caractere e o anterior sai inteiro.
    while (cut > 0 && is_continuation((unsigned char)s[cut])) cut--;
    return cut;
}

// Faixas que as font_pt_* desenham (tools/gen_fonts.sh, RANGES).
static bool glyph_supported(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) return true;
    if (cp >= 0xA0 && cp <= 0xFF) return true;
    return cp == 0x2022 || cp == 0x2014 || cp == 0x2026 || cp == 0x201C ||
           cp == 0x201D;
}

// Decodifica um caractere UTF-8. Devolve quantos bytes consumiu (sempre >= 1)
// e o ponto de codigo em *cp. Sequencia invalida consome 1 byte e devolve
// 0xFFFD, que glyph_supported recusa — o lixo some sem arrastar o vizinho.
static size_t utf8_decode(const unsigned char *s, uint32_t *cp) {
    const unsigned char c = s[0];
    size_t n = 0;
    uint32_t v = 0;
    if (c < 0x80) { *cp = c; return 1; }
    if ((c & 0xE0) == 0xC0)      { n = 2; v = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 3; v = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 4; v = c & 0x07; }
    else { *cp = 0xFFFD; return 1; }
    for (size_t i = 1; i < n; i++) {
        // is_continuation tambem recusa o '\0' final: nunca le alem da string.
        if (!is_continuation(s[i])) { *cp = 0xFFFD; return 1; }
        v = (v << 6) | (s[i] & 0x3F);
    }
    *cp = v;
    return n;
}

size_t title_sanitize(char *dst, size_t dstSize, const char *src) {
    if (dst == nullptr || dstSize == 0) return 0;
    const size_t limit = dstSize - 1;
    size_t out = 0;
    bool lastSpace = true; // true no inicio: descarta espacos da ponta esquerda

    if (src != nullptr) {
        const unsigned char *p = (const unsigned char *)src;
        while (*p) {
            uint32_t cp = 0;
            const unsigned char *ch = p;
            const size_t n = utf8_decode(p, &cp);
            p += n;

            if (cp < 0x20) cp = ' '; // \n, \t e companhia viram espaco
            if (cp == ' ') {
                if (lastSpace) continue;
                if (out + 1 > limit) break;
                dst[out++] = ' ';
                lastSpace = true;
                continue;
            }
            if (!glyph_supported(cp)) continue;
            if (out + n > limit) break; // nao cabe inteiro: o corte e aqui
            std::memcpy(dst + out, ch, n);
            out += n;
            lastSpace = false;
        }
    }
    while (out > 0 && dst[out - 1] == ' ') out--; // ponta direita
    dst[out] = '\0';
    return out;
}

void task_set_title(Task &t, const char *src) {
    title_sanitize(t.title, sizeof(t.title), src);
}

void task_set_id(char *dst, const char *src) {
    if (src == nullptr) { dst[0] = '\0'; return; }
    size_t n = std::strlen(src);
    if (n > (size_t)TASK_ID_BYTES) n = (size_t)TASK_ID_BYTES;
    std::memcpy(dst, src, n);
    dst[n] = '\0';
}
