// Estado por conta (Technical Plan §3, itens 3 e 14).
#include "model/account_state.h"
#include <string.h>
#include <esp_heap_caps.h>

AccountState* acct = nullptr;

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

