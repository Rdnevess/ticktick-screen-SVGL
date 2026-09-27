#include "tinytest.h"

int main() {
    int total = 0;
    for (const TinyTestCase &tc : tinytest_cases()) {
        int before = tinytest_failures();
        std::printf("%s\n", tc.name);
        tc.fn();
        if (tinytest_failures() == before) std::printf("  ok\n");
        total++;
    }
    std::printf("\n%d testes, %d falhas\n", total, tinytest_failures());
    return tinytest_failures() == 0 ? 0 : 1;
}
