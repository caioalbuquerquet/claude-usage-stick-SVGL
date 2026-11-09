#include "logic/bridge_route.h"
#include <stdio.h>
#include <string.h>

int bridge_find_label(const bool* used, const char* labels, size_t lblSize, int n,
                      const char* lbl) {
    if (!used || !labels || !lbl) return -1;
    for (int s = 0; s < n; s++) {
        if (used[s] && strcmp(labels + (size_t)s * lblSize, lbl) == 0) return s;
    }
    return -1;
}

int bridge_target_slot(const bool* used, const char* labels, size_t lblSize, int n,
                       int active, const char* lbl) {
    if (!lbl || !lbl[0]) {
        return (used && active >= 0 && active < n && used[active]) ? active : -1;
    }
    return bridge_find_label(used, labels, lblSize, n, lbl);
}

size_t json_escape(const char* s, char* out, size_t outSize) {
    if (!out || outSize == 0) return 0;
    size_t o = 0;
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        char esc[8];
        size_t len;
        if (c == '"' || c == '\\') {
            esc[0] = '\\'; esc[1] = (char)c; len = 2;
        } else if (c < 0x20) {
            snprintf(esc, sizeof(esc), "\\u%04x", c); len = 6;
        } else {
            esc[0] = (char)c; len = 1;
        }
        if (o + len >= outSize) break;
        memcpy(out + o, esc, len);
        o += len;
    }
    out[o] = 0;
    return o;
}
