// Arnes de teste minimo, sem dependencias: registra funcoes de teste via
// construtor estatico e conta falhas. Compila em g++, clang++ e MSVC.
#ifndef TINYTEST_H
#define TINYTEST_H

#include <cstdio>
#include <cstring>
#include <vector>

struct TinyTestCase {
    const char *name;
    void (*fn)();
};

inline std::vector<TinyTestCase> &tinytest_cases() {
    static std::vector<TinyTestCase> cases;
    return cases;
}

inline int &tinytest_failures() {
    static int failures = 0;
    return failures;
}

struct TinyTestAdder {
    TinyTestAdder(const char *name, void (*fn)()) { tinytest_cases().push_back({name, fn}); }
};

#define TEST(name)                                                    \
    static void name();                                               \
    static TinyTestAdder tinytest_adder_##name(#name, name);          \
    static void name()

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::printf("  FALHOU %s:%d  CHECK(%s)\n", __FILE__, __LINE__,   \
                        #cond);                                              \
            tinytest_failures()++;                                           \
        }                                                                    \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                     \
    do {                                                                       \
        long long va = (long long)(a), vb = (long long)(b);                    \
        if (va != vb) {                                                        \
            std::printf("  FALHOU %s:%d  %s == %s  (%lld != %lld)\n",          \
                        __FILE__, __LINE__, #a, #b, va, vb);                   \
            tinytest_failures()++;                                             \
        }                                                                      \
    } while (0)

#define CHECK_EQ_STR(a, b)                                                     \
    do {                                                                       \
        const char *va = (a), *vb = (b);                                       \
        if (std::strcmp(va, vb) != 0) {                                        \
            std::printf("  FALHOU %s:%d  %s == %s  (\"%s\" != \"%s\")\n",      \
                        __FILE__, __LINE__, #a, #b, va, vb);                   \
            tinytest_failures()++;                                             \
        }                                                                      \
    } while (0)

#endif // TINYTEST_H
