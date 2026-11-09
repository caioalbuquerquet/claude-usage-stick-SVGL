// Testes de classify_usage, probe_needs_retry e model_status_text (item 11).
#include <cstring>

#include "check.h"
#include "logic/classify.h"

#define STR_EQ(a, b) (std::strcmp((a), (b)) == 0)

TEST(classify_ok_with_headers) {
    CHECK_EQ(classify_usage(200, true), FC_OK);
    CHECK_EQ(classify_usage(429, true), FC_OK);  // conta no limite, dado válido
}

TEST(classify_auth_always) {
    CHECK_EQ(classify_usage(401, true), FC_AUTH);
    CHECK_EQ(classify_usage(401, false), FC_AUTH);
    CHECK_EQ(classify_usage(403, true), FC_AUTH);
    CHECK_EQ(classify_usage(403, false), FC_AUTH);
}

TEST(classify_rate_without_headers) {
    CHECK_EQ(classify_usage(429, false), FC_RATE);
}

TEST(classify_other_4xx_err) {
    CHECK_EQ(classify_usage(400, false), FC_ERR);
    CHECK_EQ(classify_usage(404, false), FC_ERR);
    CHECK_EQ(classify_usage(413, false), FC_ERR);
    CHECK_EQ(classify_usage(400, true), FC_ERR);
    CHECK_EQ(classify_usage(404, true), FC_ERR);
}

TEST(classify_other_codes_err) {
    CHECK_EQ(classify_usage(200, false), FC_ERR);
    CHECK_EQ(classify_usage(302, false), FC_ERR);
    CHECK_EQ(classify_usage(302, true), FC_ERR);
    CHECK_EQ(classify_usage(201, true), FC_ERR);
}

TEST(classify_5xx_srv) {
    CHECK_EQ(classify_usage(500, false), FC_SRV);
    CHECK_EQ(classify_usage(503, false), FC_SRV);
    CHECK_EQ(classify_usage(503, true), FC_SRV);
    CHECK_EQ(classify_usage(599, false), FC_SRV);
    CHECK_EQ(classify_usage(600, false), FC_ERR);
}

TEST(classify_transport_net) {
    CHECK_EQ(classify_usage(-1, false), FC_NET);
    CHECK_EQ(classify_usage(-11, false), FC_NET);
    CHECK_EQ(classify_usage(-1, true), FC_NET);
    CHECK_EQ(classify_usage(0, false), FC_NET);
    CHECK_EQ(classify_usage(0, true), FC_NET);
}

TEST(probe_retry_rules) {
    CHECK(probe_needs_retry(false, "claude-opus-4", "claude-haiku-4"));
    CHECK(!probe_needs_retry(true, "claude-opus-4", "claude-haiku-4"));
    CHECK(!probe_needs_retry(false, "claude-haiku-4", "claude-haiku-4"));
    CHECK(!probe_needs_retry(true, "claude-haiku-4", "claude-haiku-4"));
}

static void mst(int http, uint16_t ms, uint8_t lang, const char* want, Tone wantTone) {
    char buf[32];
    Tone t = TONE_OK;
    if (wantTone == TONE_OK) t = TONE_BAD;  // garante que a função escreve o tom
    model_status_text(http, ms, lang, buf, sizeof(buf), &t);
    CHECK(STR_EQ(buf, want));
    CHECK_EQ(t, wantTone);
    if (!STR_EQ(buf, want)) std::printf("  http=%d lang=%u got '%s' want '%s'\n", http, lang, buf, want);
}

TEST(model_status_text_table) {
    for (uint8_t lang = 0; lang < 2; lang++) {
        mst(0, 0, lang, "\xE2\x80\x94", TONE_DIM);
        mst(200, 900, lang, "ok \xC2\xB7 0.9s", TONE_OK);
        mst(200, 1234, lang, "ok \xC2\xB7 1.2s", TONE_OK);
        mst(401, 0, lang, "AUTH", TONE_BAD);
        mst(403, 0, lang, "AUTH", TONE_BAD);
        mst(-1, 0, lang, "NET", TONE_BAD);
        mst(-11, 0, lang, "NET", TONE_BAD);
    }
    mst(429, 0, 0, "limitado", TONE_WARN);
    mst(429, 0, 1, "limited", TONE_WARN);
    mst(404, 0, 0, "n/d", TONE_DIM);
    mst(404, 0, 1, "n/a", TONE_DIM);
    mst(503, 0, 0, "erro 503", TONE_BAD);
    mst(503, 0, 1, "error 503", TONE_BAD);
    mst(400, 0, 0, "erro 400", TONE_BAD);
    mst(500, 0, 1, "error 500", TONE_BAD);
}

TEST(model_status_text_truncates) {
    char buf[6];
    Tone t = TONE_DIM;
    model_status_text(503, 0, 1, buf, sizeof(buf), &t);
    CHECK(STR_EQ(buf, "error"));
    CHECK_EQ(t, TONE_BAD);
    model_status_text(503, 0, 1, buf, sizeof(buf), nullptr);  // tom opcional
    CHECK(STR_EQ(buf, "error"));
}
