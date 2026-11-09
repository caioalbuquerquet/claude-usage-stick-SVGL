// Testes de src/logic/reset_text (texto do reset nos cards da tela inicial).
#include "check.h"
#include "logic/reset_text.h"

namespace {
// 2023-11-14 (terça) 00:00:00 UTC
const uint32_t DAY = 1699920000UL;
const uint32_t NOW = DAY + 12 * 3600;   // 12:00 UTC
}  // namespace

TEST(reset_text_contagem_e_hora) {
    char b[32];
    reset_text(NOW + 72 * 60, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "1h12m \xC2\xB7 13:12");
    reset_text(NOW + 50 * 60 + 59, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));   // minutos inteiros
    CHECK_STR(b, "0h50m \xC2\xB7 12:50");
    reset_text(NOW + 30, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "0h00m \xC2\xB7 12:00");
    reset_text(NOW + 26 * 3600, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "1d02h \xC2\xB7 14:00");
    // fuso: hora de gmtime(epoch + tz)
    reset_text(NOW + 72 * 60, NOW, -3 * 3600, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "1h12m \xC2\xB7 10:12");
    reset_text(NOW + 72 * 60, NOW, 0, 0, RESET_AT, b, sizeof(b));
    CHECK_STR(b, "13:12");
}

// Bug do aparelho: "5h: reseta em agora" ficava parado enquanto a leitura
// guardada tinha o reset no passado. Agora diz que passou e quando.
TEST(reset_text_reset_ja_passou) {
    char b[32];
    CHECK(reset_passed(NOW - 120, NOW));
    CHECK(reset_passed(NOW, NOW));
    CHECK(!reset_passed(NOW + 1, NOW));
    CHECK(!reset_passed(0, NOW));
    CHECK(!reset_passed(NOW - 120, 999999999UL));
    reset_text(NOW - 120, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "passou \xC2\xB7 11:58");
    reset_text(NOW - 120, NOW, 0, 1, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "passed \xC2\xB7 11:58");
    reset_text(NOW, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "passou \xC2\xB7 12:00");
    reset_text(NOW - 120, NOW, 0, 0, RESET_AT, b, sizeof(b));
    CHECK_STR(b, "11:58");
}

TEST(reset_text_semana_e_sem_relogio) {
    char b[32];
    // quinta 09:00 UTC = DAY + 2 dias + 9 h
    reset_text(DAY + 2 * 86400 + 9 * 3600, NOW, 0, 0, RESET_WEEKDAY_AT, b, sizeof(b));
    CHECK_STR(b, "qui 09:00");
    reset_text(DAY + 2 * 86400 + 9 * 3600, NOW, 0, 1, RESET_WEEKDAY_AT, b, sizeof(b));
    CHECK_STR(b, "thu 09:00");
    // sábado com acento; o fuso pode trocar o dia (sáb 01:00 UTC -> sex 22:00)
    reset_text(DAY + 4 * 86400 + 3600, NOW, 0, 0, RESET_WEEKDAY_AT, b, sizeof(b));
    CHECK_STR(b, "s\xC3\xA1" "b 01:00");
    reset_text(DAY + 4 * 86400 + 3600, NOW, -3 * 3600, 0, RESET_WEEKDAY_AT, b, sizeof(b));
    CHECK_STR(b, "sex 22:00");
    reset_text(0, NOW, 0, 0, RESET_IN_AT, b, sizeof(b));
    CHECK_STR(b, "--");
    reset_text(NOW + 60, 999999999UL, 0, 0, RESET_WEEKDAY_AT, b, sizeof(b));
    CHECK_STR(b, "--");
    reset_text(NOW + 72 * 60, NOW, 0, 0, RESET_IN_AT, b, 4);   // corta sem estourar
    CHECK_STR(b, "1h1");
}
