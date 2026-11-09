#pragma once
// Projeção da janela de 5h (Technical Plan §3, item 9). Função pura, sem
// Arduino nem LVGL: a tela inicial e /janela usam a mesma conta.
#include <stddef.h>
#include <stdint.h>
#include "logic/sample.h"

struct ProjResult {
    // JANELA_PASSOU: o reset da 5h já passou (resetEpoch <= now). A % e as
    // amostras são da janela que acabou, então não há o que projetar até a
    // próxima leitura trazer o reset novo.
    enum State { SEM_DADOS, OK_ATE_RESET, ESGOTA, ESGOTADA, JANELA_PASSOU } st;
    uint32_t etaEpoch;   // ESGOTA: hora em que esgota; demais: resetEpoch (0 em SEM_DADOS)
    int minutes;         // minutos de now até etaEpoch (nunca negativo)
    int pctAtReset;      // OK_ATE_RESET: % prevista no reset (0..100)
    float rate;          // %/min da regra 4; 0 quando não calculada
};

// hist: ring buffer de HIST_MAX amostras; n: amostras válidas; head: próxima
// posição de escrita. A amostra i (0 = mais antiga) fica em
// (head - n + i + 2*HIST_MAX) % HIST_MAX.
// Regra 1b (depois da 1, antes da 2): resetEpoch <= now -> JANELA_PASSOU, com
// etaEpoch = resetEpoch e minutes = 0.
ProjResult projection(const Sample* hist, int n, int head, float pctNow,
                      uint32_t resetEpoch, uint32_t now);

// Texto curto da projeção (tabela do item 32). lang: 0 = português, != 0 =
// inglês. A hora sai de gmtime_r(etaEpoch + tzOffsetSec). Se o texto passa de
// maxChars code points, o trecho " · <tempo>" sai.
void projection_text(const ProjResult& r, uint8_t lang, int32_t tzOffsetSec,
                     int maxChars, char* out, size_t outSize);
