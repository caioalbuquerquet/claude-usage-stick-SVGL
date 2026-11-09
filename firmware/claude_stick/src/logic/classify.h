#pragma once
// Classificação das respostas da API (Technical Plan §4) e texto de estado
// dos modelos (/modelos, item 38). Funções puras, sem Arduino nem LVGL.
#include <stddef.h>
#include <stdint.h>
#include "logic/fetch_class.h"

// Tom de um texto; vira cor na interface.
enum Tone : uint8_t { TONE_DIM, TONE_OK, TONE_WARN, TONE_BAD };

// Classe de um resultado USAGE. http <= 0 é erro de transporte (FC_NET).
FetchClass classify_usage(int http, bool hasUnifiedHeaders);

// Verdadeiro só se a resposta veio sem headers unified-* e o modelo consultado
// não é o modelo de sonda (o firmware passa PROBE_MODEL em probeModel).
bool probe_needs_retry(bool hasUnifiedHeaders, const char* model, const char* probeModel);

// Texto e tom de um modelo em /modelos. lang: 0 = português, != 0 = inglês.
void model_status_text(int http, uint16_t ms, uint8_t lang, char* out, size_t outSize, Tone* tone);
