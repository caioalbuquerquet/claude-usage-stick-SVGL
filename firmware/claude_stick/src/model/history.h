#pragma once
// Histórico (ring buffer), heatmap por hora/dia e persistência em LittleFS.
// Fase 1: o estado fica no AccountState de cada conta (model/account_state.h);
// as funções recebem a conta e, quando gravam/leem arquivo, o slot.
#include <stdint.h>
#include <stddef.h>
#include "logic/sample.h"          // HIST_MAX, Sample
#include "model/account_state.h"   // AccountState, NDAYS, DayHeat

// Grava uma amostra (t = 0 se o relógio não estiver sincronizado).
void hist_push(AccountState& a, float h5, float d7, uint32_t now);
// Heatmap: atribui o consumo (Δ utilização 5h) à hora do dia local.
void accumulate_heat(AccountState& a, float h5, uint32_t now, int32_t tzOffsetSec);
// Soma o heatmap conforme o período (0=hoje 1=7d 2=30d 3=tudo).
void heat_mode_data(const AccountState& a, int mode, uint32_t now, int32_t tzOffsetSec,
                    float out[24]);
// Ring buffer cru: amostra i (0 = mais antiga) em (head - n + i + 2*HIST_MAX) % HIST_MAX.
const Sample* hist_data(const AccountState& a, int* n, int* head);

void hist_path(int slot, char* out, size_t sz);
void hist_reset_ram(AccountState& a);
// Grava /histN.bin e marca lastSaveEpoch/lastSavedReset (regra de gravação).
void hist_save(int slot, AccountState& a);
// Fase 2: grava no máximo a cada HIST_SAVE_MIN_SEC, e também quando o reset
// da janela de 5h muda (logic/schedule.h, hist_save_is_due).
bool hist_save_due(const AccountState& a, uint32_t now);
void hist_load(int slot, AccountState& a);

bool copy_file(const char *from, const char *to);
// Migração /hist.bin -> /hist0.bin (multi-conta). Chamar com o LittleFS montado.
void hist_migrate_legacy();
