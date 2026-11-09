#pragma once
// Estado por conta. Fase 0: só recebe as globais da conta ativa, com o mesmo
// comportamento da v2.3, declaradas extern aqui até a fase 1 (AccountState).
#include <stdint.h>
#include "logic/fetch_class.h"
#include "net/api.h"

// ---- Dados de uso da conta ativa ----
extern UsageData g_usage;

// ---- Tokens por sessao (vindos do bridge via POST /tokens) ----
struct TokenStats { long long tin, tout, cache; int sessions; uint32_t atMs; };
extern TokenStats g_tok;
#define TOK_FRESH_MS (15UL * 60UL * 1000UL)

// ---- Limiares (momentos 25/50/70/100%) já disparados por janela ----
extern uint8_t g_thrFired[2];   // bits já disparados por janela
extern float g_thrPrev[2];
extern bool g_thrBase;
