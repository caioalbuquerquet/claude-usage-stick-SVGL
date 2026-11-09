#pragma once
// Tempo de quadro do display (spike da fase 3, item 34). Sem LVGL: o .ino
// chama frame_refr_* nos eventos REFR_START/REFR_READY do display e
// frame_flush_add() dentro do disp_flush_cb. Só contam os quadros que
// chegaram a chamar o flush (passadas sem nada a desenhar ficam de fora).
//
// Leitura:
//   - [HEAP] no fim de cada ciclo (scheduler.cpp): máximo e número de quadros
//     desde o último log, e o modo do display;
//   - GET /health (THEP_DEV): frame_ms_max e flush_ms_max desde o boot.
#include <stdint.h>

// Modo do display, para o log ("full" ou "partial"). Ponteiro para literal.
void frame_set_mode(const char* mode);
const char* frame_mode();

void frame_refr_start();
void frame_refr_end();
// Duração de uma chamada do flush (rotação + envio), em microssegundos.
void frame_flush_add(uint32_t us);
// Partes do flush, somadas no quadro: rotação para o framebuffer nativo e
// envio ao painel (com o número de linhas nativas enviadas, de 0 a 480).
void frame_rotate_add(uint32_t us);
void frame_send_add(uint32_t us, uint32_t rows);

// Janela desde a última leitura; zera a janela.
struct FrameWindow {
    uint32_t frames;      // quadros com flush
    uint32_t frameMaxUs;  // REFR_START -> REFR_READY (layout + render + flush)
    uint32_t flushMaxUs;  // soma das chamadas do flush num quadro
    uint32_t rotateMaxUs; // soma das rotações num quadro
    uint32_t sendMaxUs;   // soma dos envios num quadro
    uint32_t rowsMax;     // linhas nativas enviadas num quadro
};
FrameWindow frame_window_take();

// Máximos desde o boot, em ms (arredondado para cima).
uint32_t frame_ms_max();
uint32_t frame_flush_ms_max();
