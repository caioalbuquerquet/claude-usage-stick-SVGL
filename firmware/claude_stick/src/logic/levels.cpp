#include "logic/levels.h"

#include <stdio.h>
#include <string.h>

Level level_for(float pct) {
    if (pct >= 80.0f) return LEVEL_BAD;
    if (pct >= 50.0f) return LEVEL_WARN;
    return LEVEL_OK;
}

void text_bar(float pct, int cells, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = 0;
    if (cells <= 0) return;
    int filled = (int)(pct * cells / 100 + 0.5f);
    if (filled < 0) filled = 0;
    if (filled > cells) filled = cells;
    static const char FULL[] = "\xE2\x96\x88";    // █
    static const char EMPTY[] = "\xE2\x96\x91";   // ░
    size_t pos = 0;
    for (int i = 0; i < cells; i++) {
        if (pos + 3 >= outSize) break;             // bloco inteiro + '\0'
        memcpy(out + pos, i < filled ? FULL : EMPTY, 3);
        pos += 3;
    }
    out[pos] = 0;
}

const char* text_bar_split(const char* bar, char* on, size_t onSize) {
    if (on && onSize) on[0] = 0;
    if (!bar) return "";
    const char* off = strstr(bar, "\xE2\x96\x91");   // primeiro "░"
    size_t k = off ? (size_t)(off - bar) : strlen(bar);
    if (on && onSize) {
        if (k > onSize - 1) {
            k = onSize - 1;
            while (k > 0 && ((unsigned char)bar[k] & 0xC0) == 0x80) k--;   // bloco inteiro
        }
        memcpy(on, bar, k);
        on[k] = 0;
    }
    return off ? off : "";
}

LayoutKind layout_for_count(int n) {
    if (n <= 1) return LAYOUT_1;
    if (n == 2) return LAYOUT_2;
    if (n == 3) return LAYOUT_3;
    return LAYOUT_4;
}

// Bytes do code point que começa em `c` (byte de continuação solto conta 1).
static size_t cp_bytes(unsigned char c) {
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}

// Avança um code point, sem passar do '\0'.
static const char* cp_next(const char* p) {
    size_t len = cp_bytes((unsigned char)*p);
    for (size_t i = 0; i < len && *p; i++) p++;
    return p;
}

int utf8_len(const char* s) {
    if (!s) return 0;
    int n = 0;
    while (*s) { s = cp_next(s); n++; }
    return n;
}

void fit_label(const char* in, int maxChars, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = 0;
    if (!in || maxChars <= 0) return;
    static const char ELL[] = "\xE2\x80\xA6";   // …
    bool cut = utf8_len(in) > maxChars;
    int keep = cut ? maxChars - 1 : maxChars;
    size_t room = outSize - 1 - (cut ? 3 : 0);    // bytes para o texto mantido
    if (cut && outSize < 4) room = 0;
    size_t pos = 0;
    const char* p = in;
    for (int i = 0; i < keep && *p; i++) {
        const char* q = cp_next(p);
        size_t len = (size_t)(q - p);
        if (pos + len > room) break;
        memcpy(out + pos, p, len);
        pos += len;
        p = q;
    }
    if (cut && outSize >= 4) { memcpy(out + pos, ELL, 3); pos += 3; }
    out[pos] = 0;
}

void age_text(uint32_t ageSec, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    if (ageSec < 60)        snprintf(out, outSize, "%us", (unsigned)ageSec);
    else if (ageSec < 3600) snprintf(out, outSize, "%um", (unsigned)(ageSec / 60));
    else                    snprintf(out, outSize, "%uh", (unsigned)(ageSec / 3600));
}

const char* status_mark(const char* s, uint8_t lang, char* buf, size_t bufSize, Tone* tone) {
    Tone t = TONE_DIM;
    const char* r = "";
    if (!s || !s[0]) {
        r = "";
    } else if (!strcmp(s, "allowed")) {
        r = "ok"; t = TONE_OK;
    } else if (!strcmp(s, "rejected") || !strcmp(s, "rate_limited") || !strcmp(s, "exceeded")) {
        r = lang ? "limit" : "limite"; t = TONE_BAD;
    } else if (strstr(s, "warning")) {
        r = lang ? "warning" : "aten\xC3\xA7\xC3\xA3o"; t = TONE_WARN;
    } else if (buf && bufSize) {
        fit_label(s, 8, buf, bufSize);
        r = buf;
    }
    if (tone) *tone = t;
    return r;
}
