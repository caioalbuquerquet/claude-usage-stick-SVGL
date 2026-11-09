#pragma once
// Rotação de 270° do quadro do LVGL (paisagem) para o framebuffer nativo do
// AXS15231B (retrato). Sem LVGL e sem Arduino: roda nos testes de host.
//
// Geometria (firmware/REFERENCIA-HARDWARE-LVGL.md, USB à esquerda):
//   paisagem LW x LH = 480 x 320 (o que o LVGL desenha, x da esquerda p/ a
//   direita, y de cima p/ baixo);
//   nativo   NW x NH = 320 x 480 (o que o painel varre: NH linhas de NW px).
//   O pixel de paisagem (lx, ly) vai para a linha nativa r = LW-1-lx, coluna
//   nativa c = ly:   dst[(LW-1-lx) * NW + ly] = src(lx, ly).
//
// Consequência usada pelo modo de faixas (DISP_BANDS): uma COLUNA de
// paisagem lx é uma LINHA nativa inteira (r = LW-1-lx), e uma LINHA de
// paisagem ly é uma COLUNA nativa. Então uma área de paisagem
// [x1..x2] x [y1..y2] toca as linhas nativas [LW-1-x2 .. LW-1-x1] (a ordem
// inverte: a borda DIREITA da paisagem, lx = 479, é a linha nativa 0), e
// dentro de cada uma dessas linhas só as colunas nativas [y1..y2].
// O y da área não muda quais linhas nativas são tocadas.
#include <stdint.h>

// Linhas nativas [*r0 .. *r1] tocadas pela faixa de paisagem [x1..x2].
inline void rot270_rows(int x1, int x2, int lw, int* r0, int* r1) {
    *r0 = lw - 1 - x2;
    *r1 = lw - 1 - x1;
}

// Gira a área de paisagem [x1..x2] x [y1..y2] para dst (framebuffer nativo
// de nw x lw px, nw = altura da paisagem). src são os pixels da área, linha
// a linha de paisagem, com srcStride px por linha (>= x2-x1+1).
//
// rot270_naive: pixel a pixel, a referência (o que o flush fazia até aqui).
// rot270_fast: mesmo resultado, em blocos de 16x16 (as 16 linhas nativas
//   tocadas por um bloco ficam no cache enquanto ele é escrito) e gravando 2
//   pixels por vez (32 bits: as linhas de paisagem ly e ly+1 viram pixels
//   vizinhos na linha nativa). dst precisa de alinhamento de 4 bytes e nw par.
void rot270_naive(const uint16_t* src, int srcStride, int x1, int y1, int x2, int y2,
                  uint16_t* dst, int lw, int nw);
void rot270_fast(const uint16_t* src, int srcStride, int x1, int y1, int x2, int y2,
                 uint16_t* dst, int lw, int nw);
