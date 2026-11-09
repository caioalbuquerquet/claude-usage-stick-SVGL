#include "logic/classify.h"

#include <stdio.h>
#include <string.h>

FetchClass classify_usage(int http, bool hasUnifiedHeaders) {
    if (http <= 0) return FC_NET;
    if (http == 401 || http == 403) return FC_AUTH;
    if (hasUnifiedHeaders && (http == 200 || http == 429)) return FC_OK;
    if (http == 429) return FC_RATE;
    if (http >= 500 && http <= 599) return FC_SRV;
    return FC_ERR;
}

bool probe_needs_retry(bool hasUnifiedHeaders, const char* model, const char* probeModel) {
    if (hasUnifiedHeaders) return false;
    if (!model || !probeModel) return model != probeModel;
    return strcmp(model, probeModel) != 0;
}

void model_status_text(int http, uint16_t ms, uint8_t lang, char* out, size_t outSize, Tone* tone) {
    const bool en = lang != 0;
    Tone t;
    if (!out || outSize == 0) {
        if (tone) *tone = TONE_DIM;
        return;
    }
    if (http == 0) {
        snprintf(out, outSize, "%s", "\xE2\x80\x94");  // — (nunca sondado)
        t = TONE_DIM;
    } else if (http == 200) {
        snprintf(out, outSize, "ok \xC2\xB7 %.1fs", ms / 1000.0);
        t = TONE_OK;
    } else if (http == 429) {
        snprintf(out, outSize, "%s", en ? "limited" : "limitado");
        t = TONE_WARN;
    } else if (http == 404) {
        snprintf(out, outSize, "%s", en ? "n/a" : "n/d");
        t = TONE_DIM;
    } else if (http == 401 || http == 403) {
        snprintf(out, outSize, "%s", "AUTH");
        t = TONE_BAD;
    } else if (http < 0) {
        snprintf(out, outSize, "%s", "NET");
        t = TONE_BAD;
    } else {
        snprintf(out, outSize, en ? "error %d" : "erro %d", http);
        t = TONE_BAD;
    }
    if (tone) *tone = t;
}
