#pragma once
// Níveis de uso, barras de texto, layouts e rótulos (Technical Plan §5 e §6,
// item 12). Funções puras, sem Arduino nem LVGL.
#include <stddef.h>
#include <stdint.h>
#include "logic/classify.h"   // Tone

// Degraus fixos: verde < 50, aviso 50..<80, vermelho >= 80.
enum Level : uint8_t { LEVEL_OK, LEVEL_WARN, LEVEL_BAD };
Level level_for(float pct);

// Barra de `cells` blocos em UTF-8: filled = (int)(pct*cells/100 + 0.5f),
// limitado a 0..cells, "█" acesos e "░" apagados. Precisa de 3*cells+1 bytes;
// se não couber, corta em bloco inteiro.
void text_bar(float pct, int cells, char* out, size_t outSize);

// Divide a barra do text_bar no primeiro "░" (acesos numa cor, apagados em
// outra): copia o trecho aceso para on (onSize bytes, sem partir um bloco) e
// devolve o trecho apagado, que aponta para dentro de bar ("" se não há).
const char* text_bar_split(const char* bar, char* on, size_t onSize);

// Layout da tela inicial por quantidade de contas: n <= 1 -> LAYOUT_1,
// n >= 4 -> LAYOUT_4.
enum LayoutKind : uint8_t { LAYOUT_1, LAYOUT_2, LAYOUT_3, LAYOUT_4 };
LayoutKind layout_for_count(int n);

// Conta code points UTF-8. Se `in` tem mais que maxChars, corta e põe "…",
// que conta como 1 dentro do limite. Sempre termina em '\0' e nunca corta um
// code point no meio, mesmo se outSize for pequeno.
void fit_label(const char* in, int maxChars, char* out, size_t outSize);

// Número de code points UTF-8 de `s` (nullptr -> 0).
int utf8_len(const char* s);

// Idade dos dados: "%us" < 60 s, "%um" < 3600 s, "%uh" a partir daí.
void age_text(uint32_t ageSec, char* out, size_t outSize);

// Marca do header anthropic-ratelimit-unified-status (tabela do §5). Devolve
// um literal ou, para valor desconhecido, `buf` com o valor cortado em 8.
const char* status_mark(const char* unifiedStatus, uint8_t lang, char* buf,
                        size_t bufSize, Tone* tone);
