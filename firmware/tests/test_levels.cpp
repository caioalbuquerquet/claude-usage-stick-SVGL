// Testes de src/logic/levels (item 12).
#include "check.h"
#include "logic/levels.h"

#include <cstring>
#include <string>

static std::string repeat(const char* s, int n) {
    std::string r;
    for (int i = 0; i < n; i++) r += s;
    return r;
}
#define FULL "\xE2\x96\x88"
#define EMPTY "\xE2\x96\x91"
#define ELL "\xE2\x80\xA6"

TEST(levels_level_for) {
    CHECK_EQ(level_for(0.0f), LEVEL_OK);
    CHECK_EQ(level_for(49.9f), LEVEL_OK);
    CHECK_EQ(level_for(50.0f), LEVEL_WARN);
    CHECK_EQ(level_for(79.9f), LEVEL_WARN);
    CHECK_EQ(level_for(80.0f), LEVEL_BAD);
    CHECK_EQ(level_for(100.0f), LEVEL_BAD);
}

TEST(levels_text_bar) {
    char b[64];
    text_bar(0.0f, 9, b, sizeof(b));
    CHECK_EQ(std::string(b), repeat(EMPTY, 9));
    text_bar(100.0f, 9, b, sizeof(b));
    CHECK_EQ(std::string(b), repeat(FULL, 9));
    text_bar(50.0f, 9, b, sizeof(b));                      // 4.5 + 0.5 -> 5 acesos
    CHECK_EQ(std::string(b), (repeat(FULL, 5) + repeat(EMPTY, 4)));
    text_bar(105.0f, 9, b, sizeof(b));                     // limitado a 9
    CHECK_EQ(std::string(b), repeat(FULL, 9));
    text_bar(-5.0f, 3, b, sizeof(b));                      // limitado a 0
    CHECK_EQ(std::string(b), repeat(EMPTY, 3));
    text_bar(100.0f, 18, b, sizeof(b));                    // layout de 1 conta: 54 bytes
    CHECK_EQ(strlen(b), (size_t)54);
    char small[8];                                         // cabem 2 blocos + '\0'
    text_bar(100.0f, 9, small, sizeof(small));
    CHECK_STR(small, FULL FULL);
    text_bar(50.0f, 0, b, sizeof(b));
    CHECK_STR(b, "");
}

TEST(levels_layout_for_count) {
    CHECK_EQ(layout_for_count(0), LAYOUT_1);
    CHECK_EQ(layout_for_count(1), LAYOUT_1);
    CHECK_EQ(layout_for_count(2), LAYOUT_2);
    CHECK_EQ(layout_for_count(3), LAYOUT_3);
    CHECK_EQ(layout_for_count(4), LAYOUT_4);
    CHECK_EQ(layout_for_count(5), LAYOUT_4);
    CHECK_EQ(layout_for_count(-1), LAYOUT_1);
}

TEST(levels_fit_label) {
    char b[64];
    const char* ascii16 = "abcdefghijklmnop";
    fit_label(ascii16, 16, b, sizeof(b));                  // limite exato: sem corte
    CHECK_STR(b, ascii16);
    fit_label(ascii16, 15, b, sizeof(b));
    CHECK_STR(b, "abcdefghijklmn" ELL);
    CHECK_EQ(utf8_len(b), 15);
    fit_label(ascii16, 8, b, sizeof(b));
    CHECK_STR(b, "abcdefg" ELL);

    const char* acc = "Jo\xC3\xA3o C\xC3\xA2mara";       // "João Câmara", 11 code points
    CHECK_EQ(utf8_len(acc), 11);
    fit_label(acc, 11, b, sizeof(b));
    CHECK_STR(b, acc);
    fit_label(acc, 10, b, sizeof(b));
    CHECK_STR(b, "Jo\xC3\xA3o C\xC3\xA2ma" ELL);
    fit_label(acc, 4, b, sizeof(b));
    CHECK_STR(b, "Jo\xC3\xA3" ELL);                        // não corta o "ã" no meio

    fit_label("abc", 1, b, sizeof(b));
    CHECK_STR(b, ELL);
    fit_label("abc", 0, b, sizeof(b));
    CHECK_STR(b, "");
    fit_label(nullptr, 5, b, sizeof(b));
    CHECK_STR(b, "");

    char tiny[5];                                          // buffer pequeno: code point inteiro
    fit_label("\xC3\xA3\xC3\xA3\xC3\xA3", 10, tiny, sizeof(tiny));
    CHECK_STR(tiny, "\xC3\xA3\xC3\xA3");
}

TEST(levels_age_text) {
    char b[16];
    age_text(0, b, sizeof(b));    CHECK_STR(b, "0s");
    age_text(59, b, sizeof(b));   CHECK_STR(b, "59s");
    age_text(60, b, sizeof(b));   CHECK_STR(b, "1m");
    age_text(3599, b, sizeof(b)); CHECK_STR(b, "59m");
    age_text(3600, b, sizeof(b)); CHECK_STR(b, "1h");
    age_text(90000, b, sizeof(b)); CHECK_STR(b, "25h");
}

TEST(levels_status_mark) {
    char buf[32];
    Tone t = TONE_OK;
    for (uint8_t lang = 0; lang < 2; lang++) {
        t = TONE_OK;
        CHECK_STR(status_mark("", lang, buf, sizeof(buf), &t), "");
        CHECK_EQ(t, TONE_DIM);
        t = TONE_OK;
        CHECK_STR(status_mark(nullptr, lang, buf, sizeof(buf), &t), "");
        CHECK_EQ(t, TONE_DIM);

        CHECK_STR(status_mark("allowed", lang, buf, sizeof(buf), &t), "ok");
        CHECK_EQ(t, TONE_OK);

        const char* warn = lang ? "warning" : "aten\xC3\xA7\xC3\xA3o";
        CHECK_STR(status_mark("allowed_warning", lang, buf, sizeof(buf), &t), warn);
        CHECK_EQ(t, TONE_WARN);

        const char* lim = lang ? "limit" : "limite";
        CHECK_STR(status_mark("rejected", lang, buf, sizeof(buf), &t), lim);
        CHECK_EQ(t, TONE_BAD);
        CHECK_STR(status_mark("rate_limited", lang, buf, sizeof(buf), &t), lim);
        CHECK_EQ(t, TONE_BAD);
        CHECK_STR(status_mark("exceeded", lang, buf, sizeof(buf), &t), lim);
        CHECK_EQ(t, TONE_BAD);

        t = TONE_OK;
        const char* r = status_mark("throttled_soon", lang, buf, sizeof(buf), &t);
        CHECK_STR(r, "throttl" ELL);                       // 8 code points com o "…"
        CHECK(r == buf);
        CHECK_EQ(t, TONE_DIM);
        CHECK_STR(status_mark("queued", lang, buf, sizeof(buf), &t), "queued");
        CHECK_EQ(t, TONE_DIM);
    }
    CHECK_STR(status_mark("allowed", 0, buf, sizeof(buf), nullptr), "ok");   // tone opcional
}
