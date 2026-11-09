#include "logic/label.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace {

// Tamanho da sequência UTF-8 bem formada em p (1 a 4) e o code point em *cp;
// 0 se malformada (inclui a sequência cortada pelo '\0').
int utf8_decode(const unsigned char* p, uint32_t* cp) {
    unsigned char c = p[0];
    if (c < 0x80) { *cp = c; return 1; }
    int len;
    uint32_t v, min;
    if ((c & 0xE0) == 0xC0)      { len = 2; v = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { len = 3; v = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { len = 4; v = c & 0x07; min = 0x10000; }
    else return 0;                                   // continuação solta ou 0xF8+
    for (int i = 1; i < len; i++) {
        if ((p[i] & 0xC0) != 0x80) return 0;         // inclui o '\0'
        v = (v << 6) | (p[i] & 0x3F);
    }
    if (v < min || v > 0x10FFFF || (v >= 0xD800 && v <= 0xDFFF)) return 0;
    *cp = v;
    return len;
}

bool allowed(uint32_t cp) {
    if (cp < 0x20 || (cp >= 0x7F && cp <= 0x9F)) return false;   // C0, DEL, C1
    return cp != '"' && cp != '\\' && cp != '<' && cp != '>';
}

// Tamanho do caractere em p para andar numa string já bem formada; byte
// malformado conta como 1.
int step(const unsigned char* p) {
    uint32_t cp;
    int n = utf8_decode(p, &cp);
    return n > 0 ? n : 1;
}

}  // namespace

void label_sanitize(const char* in, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = 0;
    if (!in) return;
    const unsigned char* p = (const unsigned char*)in;
    size_t n = 0;
    while (*p) {
        uint32_t cp;
        int len = utf8_decode(p, &cp);
        if (len == 0) { p++; continue; }             // descarta o byte e segue
        if (allowed(cp)) {
            if (n + (size_t)len > outSize - 1) break; // não parte o caractere
            memcpy(out + n, p, (size_t)len);
            n += (size_t)len;
        }
        p += len;
    }
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}

size_t utf8_fit_bytes(const char* s, size_t maxBytes) {
    if (!s) return 0;
    const unsigned char* p = (const unsigned char*)s;
    size_t n = 0;
    while (p[n]) {
        size_t len = (size_t)step(p + n);
        if (n + len > maxBytes) break;
        n += len;
    }
    return n;
}

void label_with_suffix(const char* base, int n, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    char sfx[16];
    int sl = snprintf(sfx, sizeof(sfx), " %d", n);
    if (sl < 0) sl = 0;
    if ((size_t)sl > outSize - 1) { out[0] = 0; return; }
    size_t k = utf8_fit_bytes(base ? base : "", outSize - 1 - (size_t)sl);
    if (base && base != out) memmove(out, base, k);  // memmove: base pode ser out
    memcpy(out + k, sfx, (size_t)sl);
    out[k + (size_t)sl] = 0;
}
