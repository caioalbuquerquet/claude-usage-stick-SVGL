// Estado por conta (Technical Plan §3, itens 3 e 14).
#include "model/account_state.h"
#include <string.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "logic/schedule.h"
#include "model/history.h"
#include "net/poller.h"

AccountState* acct = nullptr;

ModelInfo g_models[NMODELS] = {
  {"Haiku",  "claude-haiku-4-5-20251001"},
  {"Sonnet", "claude-sonnet-5"},
  {"Opus",   "claude-opus-4-8"},
  {"Fable",  "claude-fable-5"},
};
ModelStatus g_status = {true, true, true, true, false, SEV_NONE, {{0}, {0}, {0}, {0}}};

bool acct_init() {
  if (acct) return true;
  acct = (AccountState*)heap_caps_calloc(ACCT_MAX, sizeof(AccountState), MALLOC_CAP_SPIRAM);
  if (!acct) return false;
  for (int i = 0; i < ACCT_MAX; i++) acct[i].lastH5 = -1.0f;
  return true;
}

void acct_bump_gen(int slot) {
  if (slot < 0 || slot >= ACCT_MAX) return;
  acct[slot].gen++;
}

void acct_clear(int slot) {
  if (slot < 0 || slot >= ACCT_MAX) return;
  uint32_t gen = acct[slot].gen;
  memset(&acct[slot], 0, sizeof(AccountState));
  acct[slot].gen = gen;
  acct[slot].lastH5 = -1.0f;
}

void acct_reset_runtime(int slot) {
  if (slot < 0 || slot >= ACCT_MAX) return;
  AccountState& a = acct[slot];
  memset(&a.usage, 0, sizeof(a.usage));
  a.usageAtEpoch = 0;
  a.lastClass = FC_NONE;
  a.fails = 0;
  a.backoffCycles = 0;
  memset(&a.thr, 0, sizeof(a.thr));
  memset(a.probe, 0, sizeof(a.probe));   // token novo: sonda de novo
}

bool acct_apply_result(const NetResult& r, uint32_t intervalSec, int32_t tzOffsetSec,
                       ThrHit* hit) {
  if (hit) *hit = ThrHit{-1, 0};
  if (r.type != JOB_USAGE || r.cls == FC_DISCARDED) return false;
  if (r.slot < 0 || r.slot >= ACCT_MAX) return false;
  AccountState& a = acct[r.slot];
  if (r.gen != (uint16_t)a.gen) return false;

  a.lastClass = r.cls;
  a.lastHttp = (int16_t)r.http;
  a.lastTryEndEpoch = r.endEpoch;
  if (r.cls == FC_OK) {
    a.usage = r.u;
    a.usageAtEpoch = r.endEpoch;
    a.fails = 0;
    a.backoffCycles = 0;
    hist_push(a, a.usage.h5, a.usage.d7, r.endEpoch);
    accumulate_heat(a, a.usage.h5, r.endEpoch, tzOffsetSec);
    if (hist_save_due(a, r.endEpoch)) hist_save(r.slot, a);
    ThrHit h = check_thresholds(a);
    if (hit) *hit = h;
  } else if (r.cls == FC_AUTH) {
    if (a.fails < 255) a.fails++;
    a.backoffCycles = auth_backoff_cycles(a.fails, intervalSec, AUTH_BACKOFF_MAX_SEC);
  } else {
    a.fails = 0;
    a.backoffCycles = 0;
  }
  return true;
}

// ---- Limiares ----
const uint8_t THR[4] = {25, 50, 70, 100};

ThrHit check_thresholds(AccountState& a) {
  ThrHit best = {-1, 0};
  float c[2] = {a.usage.h5, a.usage.d7};
  for (int w = 0; w < 2; w++) {
    if (!a.thr.base || (a.thr.prev[w] - c[w]) > 15.0f) {
      a.thr.fired[w] = 0;
      for (int i = 0; i < 4; i++) if (c[w] >= THR[i]) a.thr.fired[w] |= 1 << i;
    } else {
      int hit = -1;
      for (int i = 0; i < 4; i++)
        if (c[w] >= THR[i] && !(a.thr.fired[w] & (1 << i))) { a.thr.fired[w] |= 1 << i; hit = i; }
      // maior limiar vence; empate fica com a 5h (w = 0 vem primeiro)
      if (hit >= 0 && THR[hit] > best.thr) { best.win = (int8_t)w; best.thr = THR[hit]; }
    }
    a.thr.prev[w] = c[w];
  }
  a.thr.base = true;
  return best;
}

