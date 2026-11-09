// Histórico (ring buffer), heatmap e persistência em LittleFS.
// Fase 1: estado por conta (AccountState). O formato de /hist{N}.bin
// (HistFileV2) continua byte a byte igual ao da v2.3 (G8).
#include "model/history.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include <string.h>
#include "config.h"
#include "logic/schedule.h"

// ============================================================
// Histórico / heatmap
// ============================================================
void hist_push(AccountState& a, float h5, float d7, uint32_t now) {
  a.hist[a.histHead].t  = (now > 1000000000UL) ? now : 0;
  a.hist[a.histHead].h5 = (uint8_t)(h5 + 0.5f);
  a.hist[a.histHead].d7 = (uint8_t)(d7 + 0.5f);
  a.histHead = (a.histHead + 1) % HIST_MAX;
  if (a.histN < HIST_MAX) a.histN++;
}

const Sample* hist_data(const AccountState& a, int* n, int* head) {
  if (n) *n = a.histN;
  if (head) *head = a.histHead;
  return a.hist;
}

// dia local (dias desde epoch, corrigido pelo fuso configurado)
static uint32_t day_key(uint32_t now, int32_t tzOffsetSec) {
  if (now < 1000000000UL) return 0;
  return (uint32_t)(((int64_t)now + tzOffsetSec) / 86400);
}
// índice do dia em a.days (cria/rotaciona se preciso)
static int day_slot(AccountState& a, uint32_t dk) {
  for (int i = 0; i < a.dayN; i++)
    if (a.days[i].day == dk) return i;
  if (a.dayN == NDAYS) {              // descarta o mais antigo (array cronológico)
    memmove(&a.days[0], &a.days[1], sizeof(DayHeat) * (NDAYS - 1));
    a.dayN--;
  }
  int i = a.dayN++;
  a.days[i].day = dk;
  memset(a.days[i].burn, 0, sizeof(a.days[i].burn));
  return i;
}

// Heatmap: atribui o consumo (Δ utilização 5h) à hora do dia local.
void accumulate_heat(AccountState& a, float h5, uint32_t now, int32_t tzOffsetSec) {
  if (a.lastH5 >= 0 && now > 1000000000UL) {
    float d = h5 - a.lastH5;
    if (d > 0 && d < 100) {
      time_t tn = (time_t)now;
      struct tm tmv; localtime_r(&tn, &tmv);
      a.hourBurn[tmv.tm_hour] += d;
      uint32_t dk = day_key(now, tzOffsetSec);
      if (dk) a.days[day_slot(a, dk)].burn[tmv.tm_hour] += d;
    }
  }
  a.lastH5 = h5;
}

// soma o heatmap conforme o período escolhido (0=hoje 1=7d 2=30d 3=tudo)
void heat_mode_data(const AccountState& a, int mode, uint32_t now, int32_t tzOffsetSec,
                    float out[24]) {
  memset(out, 0, sizeof(float) * 24);
  if (mode == 3) { memcpy(out, a.hourBurn, sizeof(float) * 24); return; }
  uint32_t today = day_key(now, tzOffsetSec);
  if (!today) return;
  uint32_t minDay = (mode == 0) ? today : (mode == 1) ? today - 6 : today - 29;
  for (int i = 0; i < a.dayN; i++) {
    if (a.days[i].day < minDay || a.days[i].day > today) continue;
    for (int h = 0; h < 24; h++) out[h] += a.days[i].burn[h];
  }
}

// Persistência do histórico/heatmap em LittleFS (sobrevive reboot).
// v2 = v1 + heatmap por dia. Carrega v1 antigo para não perder histórico.
#define HIST_MAGIC_V1 0xC1A0DE01
#define HIST_MAGIC_V2 0xC1A0DE02
#define HIST_MAX_V1 120
struct HistFileV1 { uint32_t magic; int n, head; Sample hist[HIST_MAX_V1]; float hourBurn[24]; float lastH5; };
struct HistFileV2 {
  uint32_t magic; int n, head; Sample hist[HIST_MAX]; float hourBurn[24]; float lastH5;
  int dayN; DayHeat days[NDAYS];
};
void hist_path(int slot, char *out, size_t sz) {
  snprintf(out, sz, "/hist%d.bin", slot);
}
void hist_reset_ram(AccountState& a) {
  memset(a.hist, 0, sizeof(a.hist));
  a.histN = 0; a.histHead = 0;
  memset(a.hourBurn, 0, sizeof(a.hourBurn));
  a.lastH5 = -1.0f;
  memset(a.days, 0, sizeof(a.days));
  a.dayN = 0;
}
bool hist_save_due(const AccountState& a, uint32_t now) {
  return hist_save_is_due(now, a.lastSaveEpoch, a.usage.h5ResetEpoch, a.lastSavedReset,
                          HIST_SAVE_MIN_SEC);
}
void hist_save(int slot, AccountState& a) {
  a.lastSaveEpoch = (uint32_t)time(nullptr);
  a.lastSavedReset = a.usage.h5ResetEpoch;
  char pth[16]; hist_path(slot, pth, sizeof(pth));
  File f = LittleFS.open(pth, "w");
  if (!f) return;
  static HistFileV2 hf;                       // grande demais p/ stack
  hf.magic = HIST_MAGIC_V2; hf.n = a.histN; hf.head = a.histHead;
  memcpy(hf.hist, a.hist, sizeof(a.hist));
  memcpy(hf.hourBurn, a.hourBurn, sizeof(a.hourBurn));
  hf.lastH5 = a.lastH5;
  hf.dayN = a.dayN;
  memcpy(hf.days, a.days, sizeof(a.days));
  f.write((uint8_t *)&hf, sizeof(hf));
  f.close();
}
void hist_load(int slot, AccountState& a) {
  char pth[16]; hist_path(slot, pth, sizeof(pth));
  File f = LittleFS.open(pth, "r");
  if (!f) return;
  uint32_t magic = 0;
  f.read((uint8_t *)&magic, sizeof(magic));
  f.seek(0);
  if (magic == HIST_MAGIC_V2) {
    static HistFileV2 hf;
    if (f.read((uint8_t *)&hf, sizeof(hf)) == (int)sizeof(hf)) {
      a.histN = hf.n; a.histHead = hf.head;
      memcpy(a.hist, hf.hist, sizeof(a.hist));
      memcpy(a.hourBurn, hf.hourBurn, sizeof(a.hourBurn));
      a.lastH5 = hf.lastH5;
      a.dayN = (hf.dayN >= 0 && hf.dayN <= NDAYS) ? hf.dayN : 0;
      memcpy(a.days, hf.days, sizeof(a.days));
    }
  } else if (magic == HIST_MAGIC_V1) {
    static HistFileV1 hf;
    if (f.read((uint8_t *)&hf, sizeof(hf)) == (int)sizeof(hf)) {
      int n = (hf.n > HIST_MAX_V1) ? HIST_MAX_V1 : hf.n;
      for (int i = 0; i < n; i++)
        a.hist[i] = hf.hist[(hf.head - n + i + HIST_MAX_V1 * 2) % HIST_MAX_V1];
      a.histN = n; a.histHead = n % HIST_MAX;
      memcpy(a.hourBurn, hf.hourBurn, sizeof(a.hourBurn));
      a.lastH5 = hf.lastH5;
      Serial.println("[HIST] migrado v1 -> v2");
    }
  }
  f.close();
}

// Copia byte a byte (o LittleFS nao tem copy). Usada na migracao para multi-conta:
// /hist.bin vira /hist0.bin mas o original FICA, para que um firmware anterior ao
// multi-conta (que so conhece /hist.bin) ainda ache o historico se a placa voltar.
// Custo: ~4,5 KB duplicados numa particao de ~14 MB.
bool copy_file(const char *from, const char *to) {
  File src = LittleFS.open(from, "r");
  if (!src) return false;
  File dst = LittleFS.open(to, "w");
  if (!dst) { src.close(); return false; }
  uint8_t buf[512];
  bool ok = true;
  for (;;) {
    int n = src.read(buf, sizeof(buf));
    if (n <= 0) break;
    if (dst.write(buf, (size_t)n) != (size_t)n) { ok = false; break; }
  }
  dst.close();
  src.close();
  return ok;
}

void hist_migrate_legacy() {
  // Migracao para multi-conta, feita em DOIS passos de proposito: copia para um
  // .tmp e so entao renomeia. Se faltar energia no meio, o que sobra e um .tmp
  // truncado — /hist0.bin ainda nao existe, entao o proximo boot refaz a
  // migracao. Copiar direto para o destino final deixaria um /hist0.bin pela
  // metade que a guarda "exists" tomaria por migracao concluida.
  // O /hist.bin original nunca e apagado (rollback p/ firmware anterior).
  if (LittleFS.exists("/hist.bin") && !LittleFS.exists("/hist0.bin")) {
    LittleFS.remove("/hist0.tmp");                       // sobra de tentativa anterior
    bool ok = copy_file("/hist.bin", "/hist0.tmp") &&
              LittleFS.rename("/hist0.tmp", "/hist0.bin");
    if (!ok) LittleFS.remove("/hist0.tmp");
    Serial.printf("[HIST] migrando p/ multi-conta: %s\n", ok ? "ok" : "FALHOU");
  }
}
