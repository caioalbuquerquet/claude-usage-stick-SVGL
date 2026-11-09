#pragma once
// Estado por conta (Technical Plan §3). Fase 1: AccountState[] é a fonte única
// dos dados de cada conta. O slot salvo e o rótulo NÃO são copiados aqui: a
// fonte é g_accts.used[slot] / g_accts.label[slot] (model/accounts.h).
// Na fase 1 a tela ainda mostra só a conta em foco (g_accts.active).
#include <stdint.h>
#include "logic/fetch_class.h"
#include "logic/sample.h"
#include "net/api.h"
#include "net/status.h"
#include "model/accounts.h"   // ACCT_MAX

// ---- Heatmap por dia (para o filtro hoje/7d/30d) ----
#define NDAYS 31
struct DayHeat { uint32_t day; float burn[24]; };   // day = dias locais desde epoch

// ---- Tokens por sessao (vindos do bridge via POST /tokens) ----
struct TokenStats { long long tin, tout, cache; int sessions; uint32_t atMs; };
#define TOK_FRESH_MS (15UL * 60UL * 1000UL)

// ---- Limiares (momentos 25/50/70/100%) já disparados por janela ----
struct ThrState {
  uint8_t fired[2];   // bits já disparados por janela (0 = 5h, 1 = 7d)
  float prev[2];      // última % vista por janela
  bool base;          // baseline já feita (1º fetch não dispara)
};

struct AccountState {
  uint32_t gen;                  // geração (resultados com gen velho são descartados, fase 2)
  bool keyOk;                    // o token deste slot decifrou

  UsageData usage;               // último UsageData bom
  uint32_t usageAtEpoch;         // quando chegou (0 = nenhum ainda)

  uint32_t lastTryEndEpoch;      // resultado da última tentativa
  FetchClass lastClass;
  int16_t lastHttp;

  uint8_t fails;                 // falhas AUTH seguidas
  uint8_t backoffCycles;         // espera atual

  uint32_t lastSaveEpoch;        // regra de gravação do histórico (fase 2)
  uint32_t lastSavedReset;

  // Mesmo conteúdo do HistFileV2 (model/history.cpp)
  Sample hist[HIST_MAX];
  int histN, histHead;
  float hourBurn[24];            // consumo por hora do dia (todo o tempo)
  float lastH5;                  // última utilização 5h (delta do heatmap)
  DayHeat days[NDAYS];
  int dayN;

  ThrState thr;
  TokenStats tok;
};

// ---- Modelos sondados e status.claude.com (fase 2: vieram do .ino) ----
// A sonda de cada modelo vem do USAGE da conta em foco (rodízio no agendador).
#define NMODELS 4
struct ModelInfo { const char *name; const char *id; ProbeResult pr; uint32_t atMs; };
extern ModelInfo g_models[NMODELS];
extern ModelStatus g_status;   // última leitura boa do status.claude.com

// ACCT_MAX registros em PSRAM, alocados por acct_init(). Só o loop() mexe.
extern AccountState* acct;

// Aloca acct[] em PSRAM, zerado. Devolve false se a alocação falhar.
bool acct_init();
void acct_bump_gen(int slot);
// Zera o registro preservando o gen (slot reaproveitado continua com gen maior).
void acct_clear(int slot);
// Volta a conta a "pendente" sem mexer no histórico.
void acct_reset_runtime(int slot);

// ---- Limiares ----
extern const uint8_t THR[4];
struct ThrHit { int8_t win; uint8_t thr; };

// Aplica um resultado USAGE (tabela do item 18). Devolve falso, sem mexer em
// nada, se não é USAGE, se é FC_DISCARDED, se o slot é inválido ou se o gen
// não bate (resultado atrasado de conta removida ou token trocado).
//   FC_OK:    usage, usageAtEpoch, fails/backoff = 0, hist_push,
//             accumulate_heat, hist_save se hist_save_due, check_thresholds
//             (o cruzamento vai em *hit; {-1, 0} se nenhum);
//   FC_AUTH:  fails++, backoffCycles = auth_backoff_cycles(fails, intervalo);
//   outras:   fails = backoffCycles = 0.
// Em todas: lastClass, lastHttp, lastTryEndEpoch. A sonda do modelo
// (g_models) fica com quem chama (poller_apply_results).
struct NetResult;
bool acct_apply_result(const NetResult& r, uint32_t intervalSec, int32_t tzOffsetSec,
                       ThrHit* hit);
// Detecta cruzamento de limiar após cada fetch. Baseline no 1º fetch (não
// dispara pelo que já estava acima); zera quando a janela reseta (queda >15pp).
// Devolve {win, thr} do cruzamento ou {-1, 0}. Se as duas janelas cruzam na
// mesma consulta, vale o maior limiar; em empate, a de 5h.
ThrHit check_thresholds(AccountState& a);

