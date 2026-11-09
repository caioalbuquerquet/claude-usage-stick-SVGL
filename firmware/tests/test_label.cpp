// Testes de src/logic/label (rótulo de conta em UTF-8, achado 3).
#include "check.h"
#include "logic/label.h"

namespace {
const size_t L = 17;   // ACCT_LBL_MAX
}

TEST(label_aceita_utf8) {
    char o[L];
    label_sanitize("Fam\xC3\xADlia", o, L);                  // "Família"
    CHECK_STR(o, "Fam\xC3\xADlia");
    label_sanitize("Caf\xC3\xA9 \xE2\x82\xAC 1", o, L);      // "Café € 1"
    CHECK_STR(o, "Caf\xC3\xA9 \xE2\x82\xAC 1");
    label_sanitize("TI \xF0\x9F\x90\xA6", o, L);             // pássaro (4 bytes)
    CHECK_STR(o, "TI \xF0\x9F\x90\xA6");
    label_sanitize("Pessoal", o, L);
    CHECK_STR(o, "Pessoal");
}

TEST(label_descarta_controle_e_proibidos) {
    char o[L];
    label_sanitize("a\tb\nc\x7F" "d", o, L);
    CHECK_STR(o, "abcd");
    label_sanitize("a\"b\\c<d>e", o, L);
    CHECK_STR(o, "abcde");
    label_sanitize("a\xC2\x85" "b", o, L);                   // C1 (NEL)
    CHECK_STR(o, "ab");
    label_sanitize("x   ", o, L);
    CHECK_STR(o, "x");
    label_sanitize("", o, L);
    CHECK_STR(o, "");
    label_sanitize(nullptr, o, L);
    CHECK_STR(o, "");
}

TEST(label_descarta_malformado) {
    char o[L];
    label_sanitize("a\xC0\xAF" "b", o, L);                   // overlong de '/'
    CHECK_STR(o, "ab");
    label_sanitize("a\xED\xA0\x80" "b", o, L);               // surrogate
    CHECK_STR(o, "ab");
    label_sanitize("a\xF4\x90\x80\x80" "b", o, L);           // > U+10FFFF
    CHECK_STR(o, "ab");
    label_sanitize("a\x80" "b", o, L);                       // continuação solta
    CHECK_STR(o, "ab");
    label_sanitize("a\xC3", o, L);                           // cortada no fim
    CHECK_STR(o, "a");
    label_sanitize("a\xE2\x82" "b", o, L);                   // cortada no meio
    CHECK_STR(o, "ab");
    label_sanitize("\xFF\xFE" "ok", o, L);
    CHECK_STR(o, "ok");
}

TEST(label_corta_em_fronteira) {
    char o[L];
    // 9 x "Á" = 18 bytes -> 8 (16 bytes)
    label_sanitize("\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81", o, L);
    CHECK_STR(o, "\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81");
    CHECK_EQ(strlen(o), 16u);
    // 15 ASCII + "€" (3 bytes): o € não cabe e não é partido
    label_sanitize("abcdefghijklmno\xE2\x82\xAC", o, L);
    CHECK_STR(o, "abcdefghijklmno");
    label_sanitize("abcdefghijklmnopqrstu", o, L);
    CHECK_STR(o, "abcdefghijklmnop");
    char small[4];
    label_sanitize("a\xE2\x82\xAC", small, sizeof(small));  // 1 + 3 > 3
    CHECK_STR(small, "a");
}

TEST(label_sufixo_em_fronteira) {
    char o[L];
    label_with_suffix("Trabalho", 2, o, L);
    CHECK_STR(o, "Trabalho 2");
    label_with_suffix("abcdefghijklmnop", 3, o, L);
    CHECK_STR(o, "abcdefghijklmn 3");
    // 8 x "Á" (16 bytes): sobram 14 bytes para a base -> 7 "Á"
    label_with_suffix("\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81", 2, o, L);
    CHECK_STR(o, "\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81\xC3\x81 2");
    // 13 ASCII + "€": base cabe em 14 bytes só sem o €
    label_with_suffix("abcdefghijklm\xE2\x82\xAC", 4, o, L);
    CHECK_STR(o, "abcdefghijklm 4");
    // base == out
    char b[L] = "Fam\xC3\xADlia";
    label_with_suffix(b, 5, b, L);
    CHECK_STR(b, "Fam\xC3\xADlia 5");
}

TEST(utf8_fit_bytes_fronteira) {
    CHECK_EQ(utf8_fit_bytes("a\xC3\xA9", 2), 1u);
    CHECK_EQ(utf8_fit_bytes("a\xC3\xA9", 3), 3u);
    CHECK_EQ(utf8_fit_bytes("\xF0\x9F\x90\xA6", 3), 0u);
    CHECK_EQ(utf8_fit_bytes(nullptr, 3), 0u);
}
