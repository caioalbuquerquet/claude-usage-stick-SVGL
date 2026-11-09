// Testes de src/logic/rot270: a rotação rápida tem de dar exatamente o mesmo
// framebuffer que a pixel a pixel, na tela cheia e em áreas parciais com
// bordas ímpares (o modo de faixas entrega áreas arbitrárias do LVGL).
#include "check.h"
#include "logic/rot270.h"

#include <cstdint>
#include <vector>

namespace {
const int LW = 480, LH = 320;

uint16_t pix(int lx, int ly) { return (uint16_t)(lx * 131 + ly * 7919 + 1); }

// Rotaciona a área com as duas funções sobre um fundo igual e compara tudo.
bool same_area(int x1, int y1, int x2, int y2, int extraStride) {
    const int w = x2 - x1 + 1, stride = w + extraStride;
    std::vector<uint16_t> src((size_t)stride * (y2 - y1 + 1), 0xDEAD);
    for (int ly = y1; ly <= y2; ly++)
        for (int lx = x1; lx <= x2; lx++) src[(size_t)(ly - y1) * stride + (lx - x1)] = pix(lx, ly);
    std::vector<uint32_t> a32(LW * LH / 2, 0xA5A5A5A5u), b32(LW * LH / 2, 0xA5A5A5A5u);
    uint16_t* a = (uint16_t*)a32.data();
    uint16_t* b = (uint16_t*)b32.data();
    rot270_naive(src.data(), stride, x1, y1, x2, y2, a, LW, LH);
    rot270_fast(src.data(), stride, x1, y1, x2, y2, b, LW, LH);
    return a32 == b32;
}
}  // namespace

TEST(rot270_full_screen_matches_naive) {
    CHECK(same_area(0, 0, LW - 1, LH - 1, 0));
}

TEST(rot270_orientation) {
    // (0,0) da paisagem vai para a última linha nativa, coluna 0; a borda
    // direita (lx = 479) é a linha nativa 0.
    std::vector<uint16_t> src(LW * LH);
    for (int ly = 0; ly < LH; ly++)
        for (int lx = 0; lx < LW; lx++) src[ly * LW + lx] = pix(lx, ly);
    std::vector<uint32_t> d32(LW * LH / 2, 0);
    uint16_t* d = (uint16_t*)d32.data();
    rot270_fast(src.data(), LW, 0, 0, LW - 1, LH - 1, d, LW, LH);
    CHECK_EQ(d[(LW - 1) * LH + 0], pix(0, 0));
    CHECK_EQ(d[0 * LH + 0], pix(LW - 1, 0));
    CHECK_EQ(d[0 * LH + (LH - 1)], pix(LW - 1, LH - 1));
    CHECK_EQ(d[(LW - 1 - 100) * LH + 37], pix(100, 37));
}

TEST(rot270_partial_areas_match_naive) {
    // bordas pares/ímpares, áreas de 1 px, 1 linha, 1 coluna, cantos
    const int cases[][4] = {
        {0, 0, 0, 0},       {479, 319, 479, 319}, {1, 1, 1, 1},     {5, 3, 5, 3},
        {0, 7, 479, 7},     {0, 8, 479, 8},       {33, 0, 33, 319}, {10, 11, 25, 12},
        {10, 11, 25, 13},   {11, 10, 26, 13},     {368, 6, 378, 16}, {367, 5, 379, 17},
        {0, 0, 479, 1},     {1, 1, 478, 318},     {100, 1, 116, 2}, {470, 300, 479, 319},
    };
    for (const auto& c : cases) {
        CHECK(same_area(c[0], c[1], c[2], c[3], 0));
        CHECK(same_area(c[0], c[1], c[2], c[3], 3));   // stride maior que a área
    }
}

TEST(rot270_rows_mapping) {
    int r0 = -1, r1 = -1;
    rot270_rows(0, 479, LW, &r0, &r1);
    CHECK_EQ(r0, 0);
    CHECK_EQ(r1, 479);
    rot270_rows(368, 378, LW, &r0, &r1);   // ponto do cabeçalho
    CHECK_EQ(r0, 101);
    CHECK_EQ(r1, 111);
    rot270_rows(0, 0, LW, &r0, &r1);       // borda esquerda = última linha nativa
    CHECK_EQ(r0, 479);
    CHECK_EQ(r1, 479);
}
