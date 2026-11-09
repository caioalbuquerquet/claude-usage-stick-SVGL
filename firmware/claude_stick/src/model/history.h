#pragma once
// Histórico (ring buffer), heatmap por hora/dia e persistência em LittleFS.
// Fase 0: o estado continua em globais (extern abaixo) da conta ativa; na fase 1
// passa para o AccountState, com assinaturas por conta.
#include <stdint.h>
#include <stddef.h>

// ---- Histórico (ring buffer; persistido em LittleFS) ----
#define HIST_MAX 160
struct Sample { uint32_t t; uint8_t h5; uint8_t d7; };   // t = epoch (0 = relógio não sincronizado)
extern Sample g_hist[HIST_MAX];
extern int g_histN;
extern int g_histHead;
extern float g_hourBurn[24];   // consumo por hora do dia (todo o tempo)
extern float g_lastH5;         // última utilização 5h (delta do heatmap)

// ---- Heatmap por dia (para o filtro hoje/7d/30d) ----
#define NDAYS 31
struct DayHeat { uint32_t day; float burn[24]; };   // day = dias locais desde epoch
extern DayHeat g_days[NDAYS];
extern int g_dayN;

void hist_push(float h5, float d7);
int hist_idx(int i);
uint32_t day_key();
int day_slot(uint32_t dk);
void accumulate_heat(float h5);
void heat_mode_data(int mode, float out[24]);
void hist_path(char *out, size_t sz);
void reset_history_ram();
void save_history();
void load_history();
bool copy_file(const char *from, const char *to);
// Migração /hist.bin -> /hist0.bin (multi-conta). Chamar com o LittleFS montado.
void hist_migrate_legacy();
