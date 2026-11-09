#pragma once
// Servidor HTTP local (onboarding do token, /window, /tokens).
// Fase 0: movido de claude_stick.ino. Fase 1: lê/grava acct[g_accts.active].
#include "net/api.h"

// Setado por POST /token quando a API aceita o token; o loop() consome.
extern volatile bool g_tokenGot;
// UsageData do token validado por POST /token. finalize_pending_token() (.ino)
// copia para acct[g_tokenTargetSlot].usage. Sai na fase 2 (fluxo assíncrono).
extern UsageData g_validatedUsage;

void stop_web();
void ensure_mdns();
// Servidor de dados do dashboard: / (info), GET /window, POST /tokens.
void start_data_web();
// Servidor do onboarding: / (formulario), POST /token, 302 para / no resto.
void web_start_onboarding();
// Atende o cliente pendente, se houver servidor. Chamar no loop().
void web_api_poll();

// Callbacks registrados pelo .ino, para web_api.cpp não chamar lv_* nem mexer
// no estado do PIN:
//   token_msg: troca o texto de status da tela de token;
//   tok_stats: chegou contagem de tokens nova (POST /tokens);
//   token_ok:  POST /token validou o token (zera o estado do PIN).
void web_on_token_msg(void (*cb)(const char *));
void web_on_tok_stats(void (*cb)());
void web_on_token_ok(void (*cb)());
