#pragma once
// Regras numéricas do agendador e da gravação do histórico (Technical Plan §2
// e item 5). Funções puras, sem Arduino: os valores do config.h entram como
// parâmetro (AUTH_BACKOFF_MAX_SEC, HIST_SAVE_MIN_SEC, intervalo de consulta).
#include <stdint.h>

// Ciclos que a conta pula depois da n-ésima falha AUTH seguida (fails = n):
// 2^(n-1), limitado a ceil(maxSec / intervalSec) — com 30 s e 1800 s, 60
// ciclos = 30 min. fails == 0 => 0. intervalSec == 0 é tratado como 1.
// O resultado também é limitado a 255 (AccountState::backoffCycles é uint8_t).
uint8_t auth_backoff_cycles(uint8_t fails, uint32_t intervalSec, uint32_t maxSec);

// Gravação do histórico de uma conta (item 5, fase 2). Verdadeiro quando:
//   · nunca gravou (lastSaveEpoch == 0);
//   · passaram minSec desde a última gravação (ou o relógio voltou atrás);
//   · o reset da janela de 5h mudou desde a última gravação.
bool hist_save_is_due(uint32_t now, uint32_t lastSaveEpoch,
                      uint32_t h5ResetNow, uint32_t lastSavedReset, uint32_t minSec);

// Índices lidos do /histN.bin (model/history.cpp, hist_load): o ring de cap
// amostras precisa de 0 <= n <= cap e 0 <= head < cap, e o heatmap por dia de
// 0 <= dayN <= maxDays (maxDays < 0: arquivo sem heatmap por dia, a v1).
// Falso = arquivo corrompido; o hist_load o trata como vazio. É o que basta
// para hist_push e projection não lerem nem escreverem fora do array.
bool hist_file_ok(int n, int head, int cap, int dayN, int maxDays);
