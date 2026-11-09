// check.h — mini framework de teste do firmware (roda no computador, g++).
// Uso: CHECK(expr), CHECK_EQ(a, b) e CHECK_STR(a, b) (strings C). Cada falha
// é impressa e contada em g_check_failures; test_main.cpp reporta o total e sai com código != 0.
#pragma once

#include <cstdio>
#include <cstring>

extern int g_check_total;
extern int g_check_failures;

#define CHECK(expr)                                                        \
    do {                                                                   \
        ++g_check_total;                                                   \
        if (!(expr)) {                                                     \
            ++g_check_failures;                                            \
            std::printf("FAIL %s:%d: CHECK(%s)\n", __FILE__, __LINE__,     \
                        #expr);                                            \
        }                                                                  \
    } while (0)

#define CHECK_EQ(a, b)                                                     \
    do {                                                                   \
        ++g_check_total;                                                   \
        if (!((a) == (b))) {                                               \
            ++g_check_failures;                                            \
            std::printf("FAIL %s:%d: CHECK_EQ(%s, %s)\n", __FILE__,        \
                        __LINE__, #a, #b);                                 \
        }                                                                  \
    } while (0)

// Igualdade de strings C; imprime os dois valores na falha.
#define CHECK_STR(a, b)                                                    \
    do {                                                                   \
        ++g_check_total;                                                   \
        const char* check_a_ = (a);                                        \
        const char* check_b_ = (b);                                        \
        if (!check_a_ || !check_b_ || std::strcmp(check_a_, check_b_)) {   \
            ++g_check_failures;                                            \
            std::printf("FAIL %s:%d: CHECK_STR(%s, %s): \"%s\" != \"%s\"\n", \
                        __FILE__, __LINE__, #a, #b,                        \
                        check_a_ ? check_a_ : "(null)",                    \
                        check_b_ ? check_b_ : "(null)");                   \
        }                                                                  \
    } while (0)

// Registro automático: escreva TEST(nome) { CHECK(...); } em qualquer
// firmware/tests/test_*.cpp. test_main.cpp roda todos os TEST registrados;
// não é preciso declarar nada em test_main.cpp.
typedef void (*check_test_fn)();
int check_register(const char* name, check_test_fn fn);

#define TEST(name)                                                         \
    static void name();                                                    \
    [[maybe_unused]] static int name##_registered =                       \
        check_register(#name, name);                                       \
    static void name()
