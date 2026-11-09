// Testes da espera crescente (AUTH) e da regra de gravação do histórico.
#include "check.h"
#include "logic/schedule.h"

TEST(backoff_zero_fails_no_wait) {
    CHECK_EQ(auth_backoff_cycles(0, 30, 1800), 0);
}

TEST(backoff_doubles_1_2_4) {
    CHECK_EQ(auth_backoff_cycles(1, 30, 1800), 1);
    CHECK_EQ(auth_backoff_cycles(2, 30, 1800), 2);
    CHECK_EQ(auth_backoff_cycles(3, 30, 1800), 4);
    CHECK_EQ(auth_backoff_cycles(6, 30, 1800), 32);
}

TEST(backoff_caps_at_30_min) {
    // 30 s: teto de 60 ciclos (30 min).
    CHECK_EQ(auth_backoff_cycles(7, 30, 1800), 60);
    CHECK_EQ(auth_backoff_cycles(20, 30, 1800), 60);
    CHECK_EQ(auth_backoff_cycles(255, 30, 1800), 60);
    // 120 s: teto de 15 ciclos.
    CHECK_EQ(auth_backoff_cycles(4, 120, 1800), 8);
    CHECK_EQ(auth_backoff_cycles(5, 120, 1800), 15);
    // 300 s: teto de 6 ciclos.
    CHECK_EQ(auth_backoff_cycles(3, 300, 1800), 4);
    CHECK_EQ(auth_backoff_cycles(4, 300, 1800), 6);
}

TEST(backoff_ceil_and_edges) {
    // 1800 / 7 = 257,1 -> ceil 258 -> limitado a 255 (uint8_t).
    CHECK_EQ(auth_backoff_cycles(9, 7, 1800), 255);
    CHECK_EQ(auth_backoff_cycles(9, 1000, 1800), 2);    // ceil(1,8) = 2
    CHECK_EQ(auth_backoff_cycles(1, 0, 1800), 1);       // intervalo 0 vira 1
    CHECK_EQ(auth_backoff_cycles(3, 30, 0), 1);         // teto 0: pula 1
}

TEST(hist_save_never_saved) {
    CHECK(hist_save_is_due(1000, 0, 5000, 0, 300));
}

TEST(hist_save_min_interval) {
    const uint32_t T = 1700000000u;
    CHECK(!hist_save_is_due(T + 30, T, 9, 9, 300));
    CHECK(!hist_save_is_due(T + 299, T, 9, 9, 300));
    CHECK(hist_save_is_due(T + 300, T, 9, 9, 300));
    CHECK(hist_save_is_due(T + 5000, T, 9, 9, 300));
}

TEST(hist_save_on_window_reset) {
    const uint32_t T = 1700000000u;
    CHECK(hist_save_is_due(T + 30, T, T + 18000, T, 300));
}

TEST(hist_save_clock_went_back) {
    const uint32_t T = 1700000000u;
    CHECK(hist_save_is_due(T - 10, T, 9, 9, 300));
}

// Índices do /histN.bin (achado 6): fora da faixa = arquivo corrompido.
TEST(hist_file_ok_faixas) {
    CHECK(hist_file_ok(0, 0, 160, 0, 31));          // vazio
    CHECK(hist_file_ok(42, 42, 160, 3, 31));
    CHECK(hist_file_ok(160, 159, 160, 31, 31));     // ring cheio
    CHECK(hist_file_ok(160, 0, 160, 0, 31));
    CHECK(!hist_file_ok(-1, 0, 160, 0, 31));
    CHECK(!hist_file_ok(161, 0, 160, 0, 31));
    CHECK(!hist_file_ok(10, 160, 160, 0, 31));      // head == cap
    CHECK(!hist_file_ok(10, -1, 160, 0, 31));
    CHECK(!hist_file_ok(10, 10, 160, -1, 31));
    CHECK(!hist_file_ok(10, 10, 160, 32, 31));
    CHECK(!hist_file_ok(0x7FFFFFFF, 0x7FFFFFFF, 160, 0, 31));
    CHECK(!hist_file_ok(0, 0, 0, 0, 31));           // cap inválido
    // v1: sem heatmap por dia (maxDays < 0 ignora dayN)
    CHECK(hist_file_ok(120, 7, 120, 12345, -1));
    CHECK(!hist_file_ok(121, 7, 120, 0, -1));
    CHECK(!hist_file_ok(5, -3, 120, 0, -1));
}
