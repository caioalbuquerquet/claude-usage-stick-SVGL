#pragma once
// Servidor HTTP local (onboarding do token, /window, /tokens, /token/status e
// /health em THEP_DEV). Fase 2: o POST /token não valida mais no handler: ele
// enfileira um job VALIDATE na net_task e responde 202; a página consulta
// GET /token/status até o resultado chegar (item 6 do plano da v3).
// Não chama o LVGL: o que mexe na tela passa pelos callbacks do .ino.
#include <stdint.h>

void stop_web();
void ensure_mdns();
bool web_running();
// Servidor do onboarding: / (formulário), POST /token, GET /token/status,
// 302 para / no resto. Derruba o servidor anterior, se houver.
void web_start_onboarding();
// Servidor da sessão (desbloqueio): / (formulário), /info, GET /window,
// POST /tokens, POST /token, GET /token/status, /health (THEP_DEV), 404 no
// resto. Derruba o servidor anterior, se houver.
void web_start_session();
// Atende o cliente pendente, se houver servidor. Chamar no loop().
void web_api_poll();

// ── Estado da validação do token (idle/pending/ok/rejected) ──
// Tela de token abriu: passa a aceitar POST /token.
void web_token_begin();
// Tela de token fechou (qualquer caminho): para de aceitar. Se havia validação
// em andamento, zera o token em validação, faz seq++ (o resultado em voo passa
// a ser ignorado) e volta a idle. ok/rejected ficam, para a página ler.
void web_token_reset();
// seq da validação atual (o gen do job VALIDATE).
uint16_t web_token_seq();
// Grava ok, ou rejected com reason (copiado).
void web_token_set_result(bool ok, const char* reason);

// Callbacks registrados pelo .ino, para web_api.cpp não chamar o LVGL:
//   token_msg: troca o texto de status da tela de token;
//   tok_stats: chegou contagem de tokens nova (POST /tokens).
void web_on_token_msg(void (*cb)(const char *));
void web_on_tok_stats(void (*cb)());

#ifdef THEP_DEV
// Métricas que só o dono do LVGL (loop/.ino) sabe ler, para o GET /health.
// O tempo de quadro vem de hw/frame_stats.h.
struct WebHealthUi { int lvMemUsedPct; };
void web_on_health_ui(void (*cb)(WebHealthUi *out));
#endif
