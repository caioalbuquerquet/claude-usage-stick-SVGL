// Runner dos testes de host: roda todo TEST(...) registrado pelos
// firmware/tests/test_*.cpp (ver check.h) e sai com 1 se algum CHECK falhou.
#include "check.h"

int g_check_total = 0;
int g_check_failures = 0;

namespace {
struct TestEntry { const char* name; check_test_fn fn; };
const int MAX_TESTS = 512;
TestEntry& slot(int i) {
    static TestEntry tests[MAX_TESTS];
    return tests[i];
}
int& count() {
    static int n = 0;
    return n;
}
}  // namespace

int check_register(const char* name, check_test_fn fn) {
    if (count() >= MAX_TESTS) {
        std::printf("check.h: mais de %d testes; aumente MAX_TESTS\n", MAX_TESTS);
        return 0;
    }
    slot(count()) = TestEntry{name, fn};
    return ++count();
}

int main() {
    for (int i = 0; i < count(); i++) {
        int before = g_check_failures;
        slot(i).fn();
        if (g_check_failures != before) std::printf("  ^ em %s\n", slot(i).name);
    }
    std::printf("%d testes, %d checks, %d falhas\n", count(), g_check_total, g_check_failures);
    return g_check_failures ? 1 : 0;
}
