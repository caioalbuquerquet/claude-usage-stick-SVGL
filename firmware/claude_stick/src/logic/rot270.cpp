// Rotação de 270° paisagem -> nativo. Ver rot270.h.
#include "logic/rot270.h"

namespace {

// Gravação de 32 bits num buffer de uint16_t sem violar strict aliasing.
typedef uint32_t __attribute__((may_alias)) u32_alias;

const int TILE = 16;   // par: os blocos preservam os pares de linhas

// Uma linha de paisagem ly, pixel a pixel (bordas de linha ímpar).
inline void row_scalar(const uint16_t* src, int srcStride, int x1, int y1, int x2, int ly,
                       uint16_t* dst, int lw, int nw) {
    const uint16_t* s = src + (ly - y1) * srcStride;
    uint16_t* d = dst + (lw - 1 - x1) * nw + ly;
    for (int lx = x1; lx <= x2; lx++, d -= nw) *d = s[lx - x1];
}

}  // namespace

void rot270_naive(const uint16_t* src, int srcStride, int x1, int y1, int x2, int y2,
                  uint16_t* dst, int lw, int nw) {
    for (int ly = y1; ly <= y2; ly++) {
        const uint16_t* row = src + (ly - y1) * srcStride;
        for (int lx = x1; lx <= x2; lx++) dst[(lw - 1 - lx) * nw + ly] = row[lx - x1];
    }
}

void rot270_fast(const uint16_t* src, int srcStride, int x1, int y1, int x2, int y2,
                 uint16_t* dst, int lw, int nw) {
    if (x2 < x1 || y2 < y1) return;
    // A gravação de 32 bits cai em dst[r*nw + ly] com ly par (nw é par e dst
    // alinhado em 4). Uma linha inicial ímpar e uma final sem par vão pixel a
    // pixel; o miolo [ya..yb] tem número par de linhas e começa em ly par.
    int ya = y1, yb = y2;
    if (ya & 1) row_scalar(src, srcStride, x1, y1, x2, ya++, dst, lw, nw);
    if (ya <= yb && ((yb - ya + 1) & 1)) row_scalar(src, srcStride, x1, y1, x2, yb--, dst, lw, nw);
    if (ya > yb) return;

    const int dstep = nw / 2;   // uma linha nativa, em palavras de 32 bits
    for (int ty = ya; ty <= yb; ty += TILE) {
        const int tyEnd = (ty + TILE - 1 < yb) ? ty + TILE - 1 : yb;   // tyEnd-ty+1 é par
        for (int tx = x1; tx <= x2; tx += TILE) {
            const int n = ((tx + TILE - 1 < x2) ? tx + TILE - 1 : x2) - tx + 1;
            for (int ly = ty; ly < tyEnd; ly += 2) {
                const uint16_t* s0 = src + (ly - y1) * srcStride + (tx - x1);
                const uint16_t* s1 = s0 + srcStride;
                u32_alias* d = (u32_alias*)(dst + (lw - 1 - tx) * nw + ly);
                // little-endian: metade baixa = coluna nativa ly, alta = ly+1
                for (int i = 0; i < n; i++, d -= dstep) *d = (uint32_t)s0[i] | ((uint32_t)s1[i] << 16);
            }
        }
    }
}
