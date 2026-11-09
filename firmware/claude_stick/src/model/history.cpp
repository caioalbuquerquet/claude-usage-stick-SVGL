// Histórico (ring buffer), heatmap e persistência em LittleFS.
// Movido de claude_stick.ino sem mudança de comportamento (v3, fase 0).
#include "model/history.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>
#include <string.h>
#include "model/accounts.h"

// ficam no .ino (item 2 do plano da v3)
extern AccountSlots g_accts;   // hist_path
extern int g_tzOffset;         // day_key

Sample g_hist[HIST_MAX];
int g_histN = 0;
int g_histHead = 0;
float g_hourBurn[24] = {0};
float g_lastH5 = -1.0f;

DayHeat g_days[NDAYS];
int g_dayN = 0;

// ============================================================
// Histórico / heatmap
// ============================================================
void hist_push(float h5, float d7) {
  time_t now = time(nullptr);
  g_hist[g_histHead].t  = (now > 1000000000L) ? (uint32_t)now : 0;
  g_hist[g_histHead].h5 = (uint8_t)(h5 + 0.5f);
  g_hist[g_histHead].d7 = (uint8_t)(d7 + 0.5f);
  g_histHead = (g_histHead + 1) % HIST_MAX;
  if (g_histN < HIST_MAX) g_histN++;
}
int hist_idx(int i) { return (g_histHead - g_histN + i + HIST_MAX * 2) % HIST_MAX; }

// dia local (dias desde epoch, corrigido pelo fuso configurado)
uint32_t day_key() {
  time_t now = time(nullptr);
  if (now < 1000000000L) return 0;
  return (uint32_t)((now + (long)g_tzOffset * 3600) / 86400);
}
// retorna o índice do dia em g_days (cria/rotaciona se preciso) — retorna int
// em vez de DayHeat* para não tropeçar nos protótipos automáticos do .ino
int day_slot(uint32_t dk) {
  for (int i = 0; i < g_dayN; i++)
    if (g_days[i].day == dk) return i;
  if (g_dayN == NDAYS) {              // descarta o mais antigo (array cronológico)
    memmove(&g_days[0], &g_days[1], sizeof(DayHeat) * (NDAYS - 1));
    g_dayN--;
  }
  int i = g_dayN++;
  g_days[i].day = dk;
  memset(g_days[i].burn, 0, sizeof(g_days[i].burn));
  return i;
}

// Heatmap: atribui o consumo (Δ utilização 5h) à hora do dia local.
void accumulate_heat(float h5) {
  time_t now = time(nullptr);
  if (g_lastH5 >= 0 && now > 1000000000L) {
    float d = h5 - g_lastH5;
    if (d > 0 && d < 100) {
      struct tm tmv; localtime_r(&now, &tmv);
      g_hourBurn[tmv.tm_hour] += d;
      uint32_t dk = day_key();
      if (dk) g_days[day_slot(dk)].burn[tmv.tm_hour] += d;
    }
  }
  g_lastH5 = h5;
}

// soma o heatmap conforme o período escolhido (0=hoje 1=7d 2=30d 3=tudo)
void heat_mode_data(int mode, float out[24]) {
  memset(out, 0, sizeof(float) * 24);
  if (mode == 3) { memcpy(out, g_hourBurn, sizeof(float) * 24); return; }
  uint32_t today = day_key();
  if (!today) return;
  uint32_t minDay = (mode == 0) ? today : (mode == 1) ? today - 6 : today - 29;
  for (int i = 0; i < g_dayN; i++) {
    if (g_days[i].day < minDay || g_days[i].day > today) continue;
    for (int h = 0; h < 24; h++) out[h] += g_days[i].burn[h];
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
void hist_path(char *out, size_t sz) {
  snprintf(out, sz, "/hist%d.bin", g_accts.active);
}
void reset_history_ram() {
  memset(g_hist, 0, sizeof(g_hist));
  g_histN = 0; g_histHead = 0;
  memset(g_hourBurn, 0, sizeof(g_hourBurn));
  g_lastH5 = -1.0f;
  memset(g_days, 0, sizeof(g_days));
  g_dayN = 0;
}
void save_history() {
  char pth[16]; hist_path(pth, sizeof(pth));
  File f = LittleFS.open(pth, "w");
  if (!f) return;
  static HistFileV2 hf;                       // grande demais p/ stack
  hf.magic = HIST_MAGIC_V2; hf.n = g_histN; hf.head = g_histHead;
  memcpy(hf.hist, g_hist, sizeof(g_hist));
  memcpy(hf.hourBurn, g_hourBurn, sizeof(g_hourBurn));
  hf.lastH5 = g_lastH5;
  hf.dayN = g_dayN;
  memcpy(hf.days, g_days, sizeof(g_days));
  f.write((uint8_t *)&hf, sizeof(hf));
  f.close();
}
void load_history() {
  char pth[16]; hist_path(pth, sizeof(pth));
  File f = LittleFS.open(pth, "r");
  if (!f) return;
  uint32_t magic = 0;
  f.read((uint8_t *)&magic, sizeof(magic));
  f.seek(0);
  if (magic == HIST_MAGIC_V2) {
    static HistFileV2 hf;
    if (f.read((uint8_t *)&hf, sizeof(hf)) == (int)sizeof(hf)) {
      g_histN = hf.n; g_histHead = hf.head;
      memcpy(g_hist, hf.hist, sizeof(g_hist));
      memcpy(g_hourBurn, hf.hourBurn, sizeof(g_hourBurn));
      g_lastH5 = hf.lastH5;
      g_dayN = (hf.dayN >= 0 && hf.dayN <= NDAYS) ? hf.dayN : 0;
      memcpy(g_days, hf.days, sizeof(g_days));
    }
  } else if (magic == HIST_MAGIC_V1) {
    static HistFileV1 hf;
    if (f.read((uint8_t *)&hf, sizeof(hf)) == (int)sizeof(hf)) {
      int n = (hf.n > HIST_MAX_V1) ? HIST_MAX_V1 : hf.n;
      for (int i = 0; i < n; i++)
        g_hist[i] = hf.hist[(hf.head - n + i + HIST_MAX_V1 * 2) % HIST_MAX_V1];
      g_histN = n; g_histHead = n % HIST_MAX;
      memcpy(g_hourBurn, hf.hourBurn, sizeof(g_hourBurn));
      g_lastH5 = hf.lastH5;
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
