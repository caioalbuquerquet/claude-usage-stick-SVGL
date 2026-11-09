#pragma once
// Amostra do histórico de uso (ring buffer por conta, persistido em
// /hist{0..3}.bin). Formato inalterado desde a v2.3 (G8).
#include <stdint.h>

#define HIST_MAX 160
struct Sample { uint32_t t; uint8_t h5; uint8_t d7; };   // t = epoch (0 = relógio não sincronizado)
