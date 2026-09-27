// Stream que acumula o corpo de uma resposta HTTP na PSRAM.
//
// HTTPClient::writeToStream() desfaz o chunked, e o parser le do buffer
// pronto (core/payload recebe const char* + len). O /data de uma lista cheia
// passa facil de 100 KB, que nao cabem na RAM interna.
#ifndef NET_PSRAM_SINK_H
#define NET_PSRAM_SINK_H

#include <Arduino.h>
#include <esp_heap_caps.h>

#include <cstring>

class PsramSink : public Stream {
public:
    explicit PsramSink(size_t cap = 512 * 1024) : _cap(cap) {}
    ~PsramSink() { if (_buf) heap_caps_free(_buf); }

    void clear() {
        _len = 0;
        if (_buf) _buf[0] = '\0';
    }

    // Zera o buffer inteiro ate _alloc (nao so ate _len) por um ponteiro
    // volatile, que o otimizador nao remove: usado apos ler algo com segredo
    // (ex.: a resposta de renovacao de token, que traz atok/rtok em texto
    // puro e pode deixar sobras depois de _len se um corpo menor reusar o
    // buffer).
    void wipe() {
        if (_buf) {
            volatile char *vp = _buf;
            for (size_t i = 0; i < _alloc; i++) vp[i] = 0;
        }
        _len = 0;
    }

    const char *data() const { return _buf ? _buf : ""; }
    size_t length() const { return _len; }

    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t *d, size_t n) override {
        if (!reserve(_len + n + 1)) return 0; // cheio: writeToStream devolve erro
        std::memcpy(_buf + _len, d, n);
        _len += n;
        _buf[_len] = '\0';
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }

private:
    bool reserve(size_t need) {
        if (need <= _alloc) return true;
        if (need > _cap) return false;
        size_t n = _alloc ? _alloc : 16 * 1024;
        while (n < need) n *= 2;
        if (n > _cap) n = _cap;
        char *nb = (char *)heap_caps_realloc(_buf, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!nb) return false;
        _buf = nb;
        _alloc = n;
        return true;
    }

    char *_buf = nullptr;
    size_t _len = 0;
    size_t _alloc = 0;
    size_t _cap;
};

#endif // NET_PSRAM_SINK_H
