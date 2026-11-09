/**
 * Claude Usage Stick — tela touch LVGL
 * Placa: Guition JC4832W535 (ESP32-S3, AXS15231B QSPI), 480x320 paisagem.
 *
 * Dashboard do rate-limit do Claude Code (janelas 5h e 7d, headers unified-*),
 * sonda real por modelo (latencia + HTTP), projecao de esgotamento da janela
 * 5h e ritmo de uso por hora com filtro de periodo. Incidentes do
 * status.claude.com viram dialog na tela inicial. Token OAuth digitado na tela
 * e guardado cifrado (AES-256-GCM, chave derivada de um PIN de 4 digitos).
 *
 * Tokens por sessao: a API nao expoe contagem para conta de assinatura; um
 * bridge opcional (tools/token_bridge.py) soma os transcripts locais do
 * Claude Code e faz POST /tokens neste device (mDNS claude-stick.local).
 *
 * Sem botao fisico: navegacao 100% touch (swipe entre telas + slideshow).
 * Init de display/touch validado no bring-up (ver REFERENCIA-HARDWARE-LVGL.md).
 */
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>
#include <math.h>
#include "config.h"
#include "hw/touch.h"
#include "hw/wifi_manager.h"
#include "net/api.h"
#include "net/status.h"
#include "sec/crypto.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "model/history.h"
#include "logic/projection.h"
#include "logic/levels.h"
#include "net/poller.h"
#include "net/scheduler.h"
#include "net/token_store.h"
#include "app/web_api.h"
#include <mbedtls/platform_util.h>
#include "logo_assets.h"   // Clawd + logotipo oficiais (gerado por tools/gen_logo_assets.py)
#include "clawd_dead.h"    // Clawd "morto" do dialog de incidente (gerado por tools/gen_dead_clawd.py)

// ---- Paleta (escuro, minimalista; acento coral do Claude) ----
#define C_BG       0x0F0F12
#define C_SURFACE  0x1A1A20   // cards sem borda
#define C_SURFACE2 0x24242C   // teclas / botoes secundarios
#define C_TRACK    0x26262E   // trilho de barras
#define C_GRID     0x232329   // linhas de grade dentro de cards
#define C_BORDER   0x30303A   // hairlines raras
#define C_TEXT     0xF2F0EC
#define C_MUTED    0x8C8C98
#define C_FAINT    0x5C5C68
#define C_ACCENT   0xD97757   // coral Claude
#define C_OK       0x4ADE80
#define C_WARN     0xFBBF24
#define C_BAD      0xF87171
#define C_SEV_MAJOR 0xFB923C   // laranja: gravidade "major" do status.claude.com

// ---- Idioma (0 = portugues, 1 = english; Ajustes -> NVS "lang") ----
uint8_t g_lang = 0;                          // lido por web_api.cpp (TRS)
#define TRS(pt, en) (g_lang ? (en) : (pt))

// ---- Hardware ----
Arduino_Canvas *gfx = nullptr;
static uint16_t *canvas_fb = nullptr;
AXS15231B_Touch touch_dev(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR, TOUCH_ROTATION);
WiFiManager g_wifi;
Preferences g_prefs;

// ---- Estado da aplicação ----
enum State {
  ST_BOOT, ST_PIN, ST_SETUP_PIN, ST_WIFI, ST_TOKEN,
  ST_MAIN, ST_SETTINGS, ST_ACCOUNTS, ST_ACCT_NAME, ST_ABOUT
};
static State g_state = ST_BOOT;
static State g_pending = ST_BOOT;
static bool  g_dirty = false;
static bool  g_uiDirty = false;   // chegou resultado da net_task: refresh_ui_values() em ST_MAIN
static void request_state(State s) { g_pending = s; g_dirty = true; }

// ---- Dados: g_status e g_models[] ficam em src/model/account_state ----

// ---- Uso, tokens por sessao, limiares e historico por conta: acct[] (src/model/account_state.h).
// Fase 1: a tela mostra so a conta em foco, acct[g_accts.active]. ----

// ---- Token / segurança ----
// Fase 2 (item 23): os tokens decifrados ficam em src/net/token_store (sob
// mutex, lidos pela net_task). Aqui fica só a chave da sessão, derivada uma
// vez no desbloqueio e usada para cifrar/decifrar todos os slots; zerada no
// factory reset. Nada disso vai para o NVS.
AccountSlots g_accts;               // lida por web_api.cpp
static bool g_onboarding = false;            // primeiro setup em andamento
static uint8_t g_sessionKey[32] = {0};
static bool g_unlocked = false;
static bool session_unlocked() { return g_unlocked; }
static bool has_accounts() { return accountCount(g_accts) > 0; }
int  g_tokenTargetSlot = 0;                  // lido por web_api.cpp (VALIDATE)
char g_pendingLabel[ACCT_LBL_MAX] = {0};
static char g_pinEntry[PIN_LEN + 1] = {0};   // dígitos sendo digitados
static char g_pinFirst[PIN_LEN + 1] = {0};   // 1ª entrada no setup de PIN
static bool g_pinConfirming = false;         // setup: confirmando 2ª vez
static int  g_pinAttempts = 0;               // tentativas erradas (persistido)
static uint32_t g_lockoutUntil = 0;          // millis até liberar nova tentativa
static bool g_lockPainted = false;           // aviso ja esta em vermelho
static int  g_lockSecs = -1;                 // ultimo segundo renderizado

static lv_obj_t *g_hdrStatus = nullptr;   // texto de status no cabeçalho do dashboard

// ---- Brilho ----
static const uint8_t BRI_LEVELS[3] = {60, 160, 255};
static int g_briIdx = 1;

int g_pollSec = DEFAULT_POLL_SEC;         // intervalo de atualização (config, NVS); lido pelo agendador
int g_tzOffset = -3;                      // fuso GMT (horas), config NVS
static int g_slideSec = 0;                // slideshow: 0=off, 5/10/15/30s (config, NVS)
static int g_heatMode = 3;                // 0=hoje 1=7d 2=30d 3=tudo (config, NVS)
static uint32_t g_lastTouchMs = 0;        // ultimo toque (pausa o slideshow)
static uint32_t g_lastSlideMs = 0;

// ---- Histórico / heatmap: src/model/history.{h,cpp} ----

// ---- Mascotes Clawd oficiais (pagina de modelos; humor por status) ----
// mood: 0=nunca sondado, 1=ok, 2=limitado(429), 3=erro/incidente, 4=n/d(404)
struct Mascot { lv_obj_t *cont, *img, *lid[2], *drop; int baseY, mood; };
static Mascot g_masc[NMODELS];
static int g_mascN = 0;
static lv_point_precise_t g_mXPts[NMODELS][4][2];   // olhos em X (mood 3)

// ---- Ponteiros de UI do dashboard (zerados a cada build de ST_MAIN) ----
#define NTILES 4
#define NSEG 18                       // segmentos do medidor de janela
struct DashUI {
  lv_obj_t *tv, *tile[NTILES], *dots[NTILES];
  // agora: uma coluna por slot salvo (fase 2, item 24)
  struct Col { lv_obj_t *card, *name, *pct, *bar, *l5, *l7, *st; int slot; } col[ACCT_MAX];
  int colN;
  lv_obj_t *agTok;
  // modelos
  lv_obj_t *mChip[NMODELS];
  // tendência da janela 5h (linhas custom)
  lv_obj_t *trHist, *trProj, *trDot, *trCap, *trT0, *trT1;
  // ritmo por hora
  lv_obj_t *heat[24], *heatBtn[4];
};
static DashUI g_ui;
static lv_obj_t *g_pinDots = nullptr, *g_pinMsg = nullptr;
static int g_curTile = 0;

// pontos das linhas do gráfico de tendência (precisam persistir)
static lv_point_precise_t g_trPts[HIST_MAX];
static lv_point_precise_t g_trProjPts[2];

// ---- Forward declarations ----
static void render_state();
static void refresh_ui_values();
static void dash_tick();
static void set_hdr_status();
static void ui_pin();
static void pin_lock_tick();
static void ui_wifi();
static void ui_token();
static void ui_main();
static void ui_settings();
static void ui_accounts();
static void ui_account_name();
static void nav_cb(lv_event_t *e);
static void trend_redraw();
static void heat_redraw();
static void update_tok_row();
static void show_moment(int slot, int win, int thr);
static void moment_tick();
static void moment_close();
static void incident_sync();

// ============================================================
// Pipeline de display/touch (validado no bring-up)
// ============================================================
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint16_t *src = (uint16_t *)px_map;
  for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {
    uint16_t *src_row = src + ly * SCREEN_WIDTH;
    for (int lx = 0; lx < SCREEN_WIDTH; lx++)
      canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];
  }
  gfx->flush();
  lv_disp_flush_ready(disp);
}
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t x, y;
  if (touch_dev.touched()) {
    touch_dev.readData(&x, &y);
    data->point.x = x; data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
    g_lastTouchMs = millis();          // pausa o slideshow enquanto ha interacao
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

#ifdef THEP_DEV
// GET /health (item 6, só THEP_DEV): tempo de quadro medido pelos eventos de
// refresh do display (a passada inteira, inclusive o flush) e uso do pool do
// LVGL, lidos aqui porque o loop() é o dono do LVGL.
static uint32_t g_frameStartMs = 0, g_frameMsMax = 0;
static void frame_evt_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_REFR_START) { g_frameStartMs = millis(); return; }
  if (!g_frameStartMs) return;
  uint32_t d = millis() - g_frameStartMs;
  if (d > g_frameMsMax) g_frameMsMax = d;
}
static void health_ui_cb(WebHealthUi *o) {
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  o->lvMemUsedPct = m.used_pct;
  o->frameMsMax = g_frameMsMax;
}
#endif

// ============================================================
// Helpers de UI
// ============================================================
static lv_obj_t *mklabel(lv_obj_t *p, const char *txt, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = lv_label_create(p);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}
static void no_box(lv_obj_t *o) {
  lv_obj_set_style_bg_opa(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
// Botão pílula com label centralizado; user_data leva o State alvo (nav_cb).
static lv_obj_t *mkbtn(lv_obj_t *p, const char *txt, const lv_font_t *font,
                       uint32_t bg, uint32_t fg) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_center(mklabel(b, txt, font, fg));
  return b;
}
// gradiente contínuo verde -> âmbar -> vermelho conforme o uso cresce
static lv_color_t grad_color(float p) {
  if (p < 0) p = 0; if (p > 100) p = 100;
  if (p <= 50.0f)
    return lv_color_mix(lv_color_hex(C_WARN), lv_color_hex(C_OK), (uint8_t)(p * 255.0f / 50.0f));
  return lv_color_mix(lv_color_hex(C_BAD), lv_color_hex(C_WARN), (uint8_t)((p - 50.0f) * 255.0f / 50.0f));
}
// acende os segmentos do medidor; acesos ganham a cor do gradiente,
// apagados ficam no trilho escuro
static void set_meter(lv_obj_t **seg, float pct) {
  int filled = (int)(pct / 100.0f * NSEG + 0.5f);
  if (pct > 0.5f && filled == 0) filled = 1;
  if (filled > NSEG) filled = NSEG;
  lv_color_t col = grad_color(pct);
  for (int i = 0; i < NSEG; i++) {
    if (!seg[i]) continue;
    lv_obj_set_style_bg_color(seg[i], (i < filled) ? col : lv_color_hex(C_TRACK), 0);
    lv_obj_set_style_bg_opa(seg[i], (i < filled) ? LV_OPA_COVER : 160, 0);
  }
}
// "reseta em 1h 23m" / "2d 4h" / "agora" / "--" (relógio não sincronizado)
static void fmt_eta(uint32_t epoch, char *out, int sz) {
  time_t now = time(nullptr);
  if (now < 1000000000L || epoch == 0) { snprintf(out, sz, "--"); return; }
  long d = (long)epoch - (long)now;
  if (d <= 0) { snprintf(out, sz, "%s", TRS("agora", "now")); return; }
  int days = d / 86400; d %= 86400;
  int hrs  = d / 3600;  d %= 3600;
  int mins = d / 60;
  if (days > 0)      snprintf(out, sz, "%dd %dh", days, hrs);
  else if (hrs > 0)  snprintf(out, sz, "%dh %02dm", hrs, mins);
  else               snprintf(out, sz, "%dm", mins);
}
static void fmt_hm(uint32_t epoch, char *out, int sz) {
  if (epoch == 0 || time(nullptr) < 1000000000L) { strlcpy(out, "--:--", sz); return; }
  time_t t = (time_t)epoch; struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(out, sz, "%H:%M", &tmv);
}
// 1234 -> "1.2k", 2345678 -> "2.3M"
static void fmt_tok(long long v, char *out, int sz) {
  if (v >= 100000000LL)     snprintf(out, sz, "%lldM", v / 1000000LL);
  else if (v >= 1000000LL)  snprintf(out, sz, "%.1fM", v / 1e6);
  else if (v >= 10000LL)    snprintf(out, sz, "%lldk", v / 1000LL);
  else if (v >= 1000LL)     snprintf(out, sz, "%.1fk", v / 1e3);
  else                      snprintf(out, sz, "%lld", v);
}

// ============================================================
// NVS / persistência
// ============================================================
static void load_persisted() {
  g_prefs.begin(NVS_NAMESPACE, false);
  accountsLoad(g_prefs, g_accts);
  g_pinAttempts = g_prefs.getInt("pinatt", 0);
  g_briIdx = g_prefs.getInt("bri", 1);
  if (g_briIdx < 0 || g_briIdx > 2) g_briIdx = 1;
  g_pollSec = g_prefs.getInt("poll", DEFAULT_POLL_SEC);
  if (g_pollSec < MIN_POLL_SEC || g_pollSec > MAX_POLL_SEC) g_pollSec = DEFAULT_POLL_SEC;
  g_tzOffset = g_prefs.getInt("tz", -3);
  if (g_tzOffset < -12 || g_tzOffset > 14) g_tzOffset = -3;
  g_slideSec = g_prefs.getInt("slide", 0);
  if (g_slideSec != 0 && g_slideSec != 5 && g_slideSec != 10 &&
      g_slideSec != 15 && g_slideSec != 30) g_slideSec = 0;
  g_heatMode = g_prefs.getInt("heatm", 3);
  if (g_heatMode < 0 || g_heatMode > 3) g_heatMode = 3;
  g_lang = g_prefs.getInt("lang", 0) ? 1 : 0;
}
static void save_attempts() { g_prefs.putInt("pinatt", g_pinAttempts); }
static void apply_brightness() { ledcWrite(TFT_BL, BRI_LEVELS[g_briIdx]); }

static void factory_reset() {
  g_prefs.clear();              // apaga blob, pinatt, bri do namespace claude
  g_wifi.forgetAll();
  for (int i = 0; i < ACCT_MAX; i++) {
    char pth[16]; snprintf(pth, sizeof(pth), "/hist%d.bin", i);
    LittleFS.remove(pth);
  }
  memset(&g_accts, 0, sizeof(g_accts));
  scheduler_stop();
  stop_web();
  web_token_reset();
  token_store_clear_all();
  token_store_clear_pending();
  mbedtls_platform_zeroize(g_sessionKey, sizeof(g_sessionKey));
  g_unlocked = false;
  g_pendingLabel[0] = 0;
  g_tokenTargetSlot = 0;
  // gen++ antes de zerar: um USAGE em voo deste slot é descartado e não
  // recria o /histN.bin recém-apagado.
  for (int i = 0; i < ACCT_MAX; i++) { acct_bump_gen(i); acct_clear(i); }
  g_pinAttempts = 0;
  g_lockoutUntil = 0;                        // senao contamina a tela de setup
  g_onboarding = true;
  Serial.println("[RESET] tudo apagado");
}

// ============================================================
// Tela: PIN (keypad touch) — entra PIN p/ decifrar OU define novo no setup
// ============================================================
static const char *pin_map[] = {
  "1", "2", "3", "\n",
  "4", "5", "6", "\n",
  "7", "8", "9", "\n",
  LV_SYMBOL_LEFT, "0", LV_SYMBOL_OK, ""
};

static void pin_update_dots() {
  if (!g_pinDots) return;
  char dots[24] = {0};
  int len = strlen(g_pinEntry);
  for (int i = 0; i < PIN_LEN; i++) {
    strcat(dots, i < len ? "*" : "_");
    if (i < PIN_LEN - 1) strcat(dots, " ");
  }
  lv_label_set_text(g_pinDots, dots);
}

// millis() vira em ~49 dias e o deadline pode cair do outro lado da volta:
// comparar por delta assinado. O g_lockoutUntil == 0 precisa de guard proprio,
// senao o delta fica negativo sozinho a partir de 2^31 ms de uptime (~24 dias)
// e o teclado morreria sem nunca ter havido bloqueio.
static bool pin_locked() {
  return g_lockoutUntil != 0 && (int32_t)(millis() - g_lockoutUntil) < 0;
}

// O teclado destrava sozinho quando o deadline passa, mas a mensagem era
// escrita uma unica vez em pin_submit(): ficava "Aguarde 60s" na tela com o PIN
// ja aceitando digito. Este tick roda no loop() e cuida das duas pontas — conta
// regressiva enquanto trava, aviso neutro ao liberar — e e dono tambem da cor,
// para os dois estados nascerem no mesmo lugar.
static void pin_lock_tick() {
  if (!g_pinMsg) return;
  char m[64];
  if (pin_locked()) {
    if (!g_lockPainted) {
      g_lockPainted = true;
      lv_obj_set_style_text_color(g_pinMsg, lv_color_hex(C_BAD), 0);
    }
    int rem = (int)((g_lockoutUntil - millis() + 999) / 1000);
    if (rem == g_lockSecs) return;      // so reescreve quando o segundo vira
    g_lockSecs = rem;
    snprintf(m, sizeof(m), TRS("PIN errado (%d/%d). Aguarde %ds", "Wrong PIN (%d/%d). Wait %ds"),
             g_pinAttempts, MAX_PIN_ATTEMPTS, rem);
    lv_label_set_text(g_pinMsg, m);
  } else if (g_lockPainted) {
    g_lockPainted = false;
    g_lockSecs = -1;
    snprintf(m, sizeof(m), TRS("Pode tentar de novo (%d/%d)", "You can try again (%d/%d)"),
             g_pinAttempts, MAX_PIN_ATTEMPTS);
    lv_label_set_text(g_pinMsg, m);
    lv_obj_set_style_text_color(g_pinMsg, lv_color_hex(C_MUTED), 0);
  }
}

// Passo 4 do desbloqueio (item 23): zera tentativas e digitação, carrega os
// históricos de todos os slots salvos, sobe o servidor da sessão e o agendador.
static void session_open() {
  g_pinAttempts = 0; g_lockoutUntil = 0; save_attempts();
  mbedtls_platform_zeroize(g_pinEntry, sizeof(g_pinEntry));
  for (int s = 0; s < ACCT_MAX; s++) {
    if (!g_accts.used[s]) continue;
    hist_reset_ram(acct[s]);
    hist_load(s, acct[s]);
  }
  g_unlocked = true;
  web_start_session();
  scheduler_start();
  request_state(ST_MAIN);
}

static void pin_submit() {
  if (g_state == ST_SETUP_PIN) {
    if (!g_pinConfirming) {
      strlcpy(g_pinFirst, g_pinEntry, sizeof(g_pinFirst));
      g_pinConfirming = true;
      g_pinEntry[0] = 0;
      pin_update_dots();
      if (g_pinMsg) lv_label_set_text(g_pinMsg, TRS("Confirme o PIN", "Confirm the PIN"));
      return;
    }
    // confirmando
    if (strcmp(g_pinFirst, g_pinEntry) != 0) {
      g_pinConfirming = false;
      g_pinFirst[0] = 0; g_pinEntry[0] = 0;
      pin_update_dots();
      if (g_pinMsg) lv_label_set_text(g_pinMsg, TRS("Nao bateu. Defina de novo.", "Didn't match. Set it again."));
      return;
    }
    // PIN definido (item 23): chave da sessão, cifra o token validado e salva
    deriveSessionKey(g_pinEntry, g_sessionKey);
    char buf[TOKEN_BUF_SIZE];
    if (!token_store_take_pending(buf, sizeof(buf))) {
      // não há token validado (não deveria acontecer): volta ao token
      mbedtls_platform_zeroize(g_sessionKey, sizeof(g_sessionKey));
      g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0;
      Serial.println("[PIN] sem token validado; de volta ao token");
      request_state(ST_TOKEN);
      return;
    }
    EncryptedBlob blob;
    if (!encryptTokenWithKey(buf, g_sessionKey, blob)) {
      token_store_set_pending(buf);              // devolve à área de validação
      mbedtls_platform_zeroize(buf, sizeof(buf));
      mbedtls_platform_zeroize(g_sessionKey, sizeof(g_sessionKey));
      if (g_pinMsg) lv_label_set_text(g_pinMsg, TRS("Falha ao cifrar. Tente de novo.", "Encryption failed. Try again."));
      g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0; pin_update_dots();
      return;
    }
    int slot = g_tokenTargetSlot;
    accountSave(g_prefs, g_accts, slot, blob, g_pendingLabel);
    accountSetActive(g_prefs, g_accts, slot);
    g_pendingLabel[0] = 0;
    acct_bump_gen(slot);
    acct[slot].keyOk = true;
    token_store_set(slot, (uint16_t)acct[slot].gen, buf);
    mbedtls_platform_zeroize(buf, sizeof(buf));
    g_onboarding = false;
    g_pinConfirming = false;
    mbedtls_platform_zeroize(g_pinFirst, sizeof(g_pinFirst));
    Serial.printf("[PIN] token cifrado e salvo no slot %d\n", slot);
    session_open();
    return;
  }

  // ST_PIN: deriva a chave da sessão e abre todos os slots salvos (item 23)
  deriveSessionKey(g_pinEntry, g_sessionKey);
  int opened = 0, total = 0;
  for (int s = 0; s < ACCT_MAX; s++) {
    if (!g_accts.used[s]) continue;
    total++;
    acct[s].keyOk = false;
    EncryptedBlob b;
    if (!accountLoadBlob(g_prefs, s, b)) { Serial.printf("[slot %d] sem blob no NVS\n", s); continue; }
    if (b.len > TOKEN_MAX_LEN) { Serial.printf("[slot %d] token longo demais\n", s); continue; }
    char tok[TOKEN_BUF_SIZE];
    if (decryptTokenWithKey(b, g_sessionKey, tok, sizeof(tok))) {
      acct[s].keyOk = true;
      token_store_set(s, (uint16_t)acct[s].gen, tok);
      opened++;
    }
    mbedtls_platform_zeroize(tok, sizeof(tok));
  }
  if (opened > 0) {
    Serial.printf("[PIN] ok: %d/%d conta(s) abertas\n", opened, total);
    for (int s = 0; s < ACCT_MAX; s++)
      if (g_accts.used[s] && !acct[s].keyOk) Serial.printf("[slot %d] SEM CHAVE (nao decifrou)\n", s);
    session_open();
    return;
  }
  // nenhum slot abriu: PIN errado
  mbedtls_platform_zeroize(g_sessionKey, sizeof(g_sessionKey));
  {
    g_pinAttempts++; save_attempts();
    g_pinEntry[0] = 0; pin_update_dots();
    Serial.printf("[PIN] errado (%d/%d)\n", g_pinAttempts, MAX_PIN_ATTEMPTS);
    if (g_pinAttempts >= MAX_PIN_ATTEMPTS) {
      Serial.println("[PIN] limite estourado -> wipe");
      factory_reset();
      request_state(ST_WIFI);
      return;
    }
    int wait = LOCKOUT_BASE_SEC * (1 << (g_pinAttempts - 1));
    if (wait > 3600) wait = 3600;
    g_lockoutUntil = millis() + (uint32_t)wait * 1000;
    if (!g_lockoutUntil) g_lockoutUntil = 1;   // 0 e o sentinela de "sem bloqueio"
    g_lockSecs = -1;
    pin_lock_tick();                 // escreve ja; o loop() mantem a contagem
  }
}

static void pin_kb_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  if (pin_locked()) return;                  // travado
  lv_obj_t *bm = (lv_obj_t *)lv_event_get_target(e);
  uint32_t id = lv_buttonmatrix_get_selected_button(bm);
  const char *txt = lv_buttonmatrix_get_button_text(bm, id);
  if (!txt) return;
  int len = strlen(g_pinEntry);
  if (strcmp(txt, LV_SYMBOL_LEFT) == 0) {
    if (len > 0) g_pinEntry[len - 1] = 0;
    pin_update_dots();
  } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
    if (len == PIN_LEN) pin_submit();
  } else if (len < PIN_LEN) {
    g_pinEntry[len] = txt[0];
    g_pinEntry[len + 1] = 0;
    pin_update_dots();
    if (len + 1 == PIN_LEN) pin_submit();     // auto-submit ao completar
  }
}

static void ui_pin() {
  lv_obj_t *scr = lv_screen_active();
  const char *title = (g_state == ST_SETUP_PIN)
    ? (g_pinConfirming ? TRS("Confirme o PIN", "Confirm the PIN") : TRS("Defina um PIN", "Set a PIN"))
    : TRS("Digite o PIN", "Enter the PIN");
  lv_obj_t *t = mklabel(scr, title, &lv_font_montserrat_22, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);

  g_pinDots = mklabel(scr, "", &lv_font_montserrat_28, C_ACCENT);
  lv_obj_align(g_pinDots, LV_ALIGN_TOP_MID, 0, 48);
  pin_update_dots();

  const char *sub = (g_state == ST_SETUP_PIN)
    ? TRS("Voce vai digita-lo a cada boot.", "You'll type it on every boot.")
    : TRS("Necessario para desbloquear o token.", "Needed to unlock the token.");
  g_pinMsg = mklabel(scr, sub, &lv_font_montserrat_14, C_MUTED);
  g_lockPainted = false;                       // label novo: estado do aviso zera
  g_lockSecs = -1;
  lv_obj_align(g_pinMsg, LV_ALIGN_TOP_MID, 0, 86);

  lv_obj_t *bm = lv_buttonmatrix_create(scr);
  lv_buttonmatrix_set_map(bm, pin_map);
  lv_obj_set_size(bm, 280, 180);
  lv_obj_align(bm, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(bm, lv_color_hex(C_BG), 0);
  lv_obj_set_style_border_width(bm, 0, 0);
  lv_obj_set_style_text_font(bm, &lv_font_montserrat_24, 0);
  lv_obj_set_style_bg_color(bm, lv_color_hex(C_SURFACE2), LV_PART_ITEMS);
  lv_obj_set_style_text_color(bm, lv_color_hex(C_TEXT), LV_PART_ITEMS);
  lv_obj_add_event_cb(bm, pin_kb_cb, LV_EVENT_VALUE_CHANGED, NULL);

  pin_lock_tick();      // se ha bloqueio pendente, ja nasce com a contagem
}

// ============================================================
// Tela: WiFi (scan + teclado)
// ============================================================
static lv_obj_t *wifi_list = nullptr, *wifi_ta = nullptr, *wifi_kb = nullptr, *wifi_status = nullptr;
// Sessão: a tela espera a net_task ficar ociosa antes de mexer no rádio.
static bool g_wifiWaiting = false;
static char sel_ssid[33] = {0};
static void wifi_item_cb(lv_event_t *e);

static void wifi_populate() {
  lv_obj_clean(wifi_list);
  lv_label_set_text(wifi_status, TRS("Escaneando redes...", "Scanning networks..."));
  lv_refr_now(NULL);
  WiFiManager::NetworkInfo nets[12];
  int n = g_wifi.scanNetworks(nets, 12);
  for (int i = 0; i < n; i++) {
    lv_obj_t *b = lv_list_add_button(wifi_list, LV_SYMBOL_WIFI, nets[i].ssid);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_text_color(b, lv_color_hex(C_TEXT), 0);
    lv_obj_add_event_cb(b, wifi_item_cb, LV_EVENT_CLICKED, NULL);  // clique direto no botão
  }
  lv_label_set_text(wifi_status, n > 0 ? TRS("Toque na sua rede", "Tap your network")
                                       : TRS("Nenhuma rede. Toque em Reescanear.", "No networks. Tap Rescan."));
}
static void wifi_item_cb(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  const char *txt = lv_list_get_button_text(wifi_list, btn);
  if (!txt) return;
  strlcpy(sel_ssid, txt, sizeof(sel_ssid));
  lv_label_set_text_fmt(wifi_status, TRS("Senha de \"%s\":", "Password for \"%s\":"), sel_ssid);
  lv_obj_add_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
  lv_textarea_set_text(wifi_ta, "");
  lv_obj_clear_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(wifi_kb, wifi_ta);
}
static void wifi_kb_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char *pass = lv_textarea_get_text(wifi_ta);
    lv_label_set_text(wifi_status, TRS("Conectando...", "Connecting..."));
    lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);
    bool ok = g_wifi.connectTo(sel_ssid, pass, 15000);
    if (ok) { ensure_mdns(); request_state(g_onboarding ? ST_TOKEN : ST_MAIN); }
    else { lv_label_set_text(wifi_status, TRS("Falhou. Toque numa rede de novo.", "Failed. Tap a network again.")); lv_obj_clear_flag(wifi_list, LV_OBJ_FLAG_HIDDEN); }
  } else if (code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(wifi_status, TRS("Toque na sua rede", "Tap your network"));
  }
}
static void wifi_rescan_cb(lv_event_t *e) { (void)e; wifi_populate(); }

static void ui_wifi() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *title = mklabel(scr, TRS("Configurar WiFi", "Configure WiFi"), &lv_font_montserrat_20, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

  // Sessão: o agendador já está pausado (render_state); espera o job em
  // andamento terminar antes de escanear. O loop() reabre a tela quando
  // poller_idle() ficar verdadeiro.
  g_wifiWaiting = session_unlocked() && !poller_idle();
  if (g_wifiWaiting) {
    lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                         &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_MAIN);
    lv_obj_t *w = mklabel(scr, TRS("aguardando a rede...", "waiting for the network..."),
                          &lv_font_montserrat_16, C_MUTED);
    lv_obj_align(w, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_obj_t *rb = mkbtn(scr, TRS("Reescanear", "Rescan"), &lv_font_montserrat_14, C_SURFACE2, C_ACCENT);
  lv_obj_align(rb, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_add_event_cb(rb, wifi_rescan_cb, LV_EVENT_CLICKED, NULL);

  if (!g_onboarding && session_unlocked()) {
    lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                         &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -150, 8);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_MAIN);
  }

  wifi_status = mklabel(scr, "...", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 14, 44);

  wifi_list = lv_list_create(scr);
  lv_obj_set_size(wifi_list, 452, 246);
  lv_obj_align(wifi_list, LV_ALIGN_TOP_MID, 0, 68);
  lv_obj_set_style_bg_color(wifi_list, lv_color_hex(C_BG), 0);
  lv_obj_set_style_border_color(wifi_list, lv_color_hex(C_BORDER), 0);
  // clique é anexado por botão em wifi_populate()

  wifi_ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(wifi_ta, true);
  lv_textarea_set_password_mode(wifi_ta, true);
  lv_textarea_set_placeholder_text(wifi_ta, TRS("senha do WiFi", "WiFi password"));
  lv_obj_set_size(wifi_ta, 452, 44);
  lv_obj_align(wifi_ta, LV_ALIGN_TOP_MID, 0, 66);
  lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);

  wifi_kb = lv_keyboard_create(scr);
  lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(wifi_kb, wifi_kb_cb, LV_EVENT_ALL, NULL);

  wifi_populate();
}

// ============================================================
// Tela: token (onboarding). O servidor web fica em src/app/web_api.{h,cpp};
// o que ele muda na tela ou no PIN passa pelos callbacks abaixo.
// ============================================================
static lv_obj_t *g_tokMsg = nullptr;            // status na tela do device

static void web_token_msg_cb(const char *m) {
  if (g_tokMsg) lv_label_set_text(g_tokMsg, m);
}
static void web_tok_stats_cb() { update_tok_row(); }

static void anim_opa_cb(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, 0); }

// Clawd oficial (pixel-art) com "respiração" — substitui o antigo sol de raios.
static lv_obj_t *build_claude_mark(lv_obj_t *parent) {
  lv_obj_t *img = lv_image_create(parent);
  lv_image_set_src(img, &img_clawd_big);
  lv_anim_t a; lv_anim_init(&a);
  lv_anim_set_var(&a, img);
  lv_anim_set_exec_cb(&a, anim_opa_cb);
  lv_anim_set_values(&a, 140, 255);
  lv_anim_set_duration(&a, 900);
  lv_anim_set_playback_duration(&a, 900);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
  return img;
}

static void ui_token() {
  lv_obj_t *scr = lv_screen_active();

  if (session_unlocked()) {
    lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                         &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_MAIN);
  }

  lv_obj_t *mark = build_claude_mark(scr);
  lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 8);

  lv_obj_t *cap = mklabel(scr, TRS("Cole o token pelo navegador, em:",
                                   "Paste the token via browser, at:"), &lv_font_montserrat_16, C_MUTED);
  lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 108);

  String url = String("http://") + WiFi.localIP().toString();
  lv_obj_t *ip = mklabel(scr, url.c_str(), &lv_font_montserrat_28, C_ACCENT);
  lv_obj_align(ip, LV_ALIGN_TOP_MID, 0, 132);

  lv_obj_t *hint = mklabel(scr, TRS("abra esse endereco no PC/celular na MESMA rede WiFi",
                                    "open this address on a PC/phone on the SAME WiFi"),
                           &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 174);

  lv_obj_t *sp = lv_spinner_create(scr);
  lv_spinner_set_anim_params(sp, 1200, 70);
  lv_obj_set_size(sp, 36, 36);
  lv_obj_align(sp, LV_ALIGN_BOTTOM_MID, 0, -46);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(sp, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sp, 5, LV_PART_INDICATOR);

  g_tokMsg = mklabel(scr, TRS("aguardando o token...", "waiting for the token..."), &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(g_tokMsg, LV_ALIGN_BOTTOM_MID, 0, -14);

  // Item 6: a sessão já tem servidor (web_start_session); o onboarding sobe o
  // dele aqui, se ainda não houver. A validação é assíncrona (VALIDATE).
  web_token_begin();
  if (!session_unlocked() && !web_running()) web_start_onboarding();
}

// Resultado do VALIDATE (hook onValidate do poller; item 6, passos 4, 7 e 8).
static void on_validate_result(const NetResult &r) {
  if (r.cls == FC_DISCARDED || r.gen != web_token_seq()) {
    Serial.printf("[TOKEN] resultado ignorado (seq %u, atual %u, cls %s)\n", (unsigned)r.gen,
                  (unsigned)web_token_seq(), fetch_class_name(r.cls));
    return;
  }
  if (r.cls != FC_OK) {
    token_store_clear_pending();
    char reason[12];
    strlcpy(reason, fetch_class_name(r.cls), sizeof(reason));
    for (char *c = reason; *c; c++) *c = (char)tolower((unsigned char)*c);
    web_token_set_result(false, reason);
    Serial.printf("[TOKEN] recusado: %s (http %d)\n", fetch_class_name(r.cls), r.http);
    char m[64];
    snprintf(m, sizeof(m), TRS("token recusado (%s), tente de novo", "token rejected (%s), try again"),
             fetch_class_name(r.cls));
    web_token_msg_cb(m);
    return;
  }

  if (!session_unlocked()) {               // onboarding (passo 7)
    web_token_set_result(true, nullptr);
    g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0;
    Serial.println("[TOKEN] validado; defina o PIN");
    web_token_msg_cb(TRS("token OK! defina o PIN", "token OK! set the PIN"));
    if (g_state == ST_TOKEN) request_state(ST_SETUP_PIN);
    return;
  }

  // sessão desbloqueada: adicionar ou trocar conta (passo 8)
  int slot = g_tokenTargetSlot;
  if (slot < 0 || slot >= ACCT_MAX) { token_store_clear_pending(); web_token_set_result(false, "err"); return; }
  bool replacing = g_accts.used[slot];
  char buf[TOKEN_BUF_SIZE];
  if (!token_store_take_pending(buf, sizeof(buf))) { web_token_set_result(false, "err"); return; }
  EncryptedBlob blob;
  if (!encryptTokenWithKey(buf, g_sessionKey, blob)) {
    mbedtls_platform_zeroize(buf, sizeof(buf));
    g_pendingLabel[0] = 0;
    web_token_set_result(false, "crypto");
    web_token_msg_cb(TRS("falha ao cifrar, tente de novo", "encryption failed, try again"));
    return;
  }
  char lbl[ACCT_LBL_MAX];
  strlcpy(lbl, g_pendingLabel[0] ? g_pendingLabel : (replacing ? g_accts.label[slot] : ""), sizeof(lbl));
  accountSave(g_prefs, g_accts, slot, blob, lbl);
  accountSetActive(g_prefs, g_accts, slot);
  acct_bump_gen(slot);
  acct[slot].keyOk = true;
  token_store_set(slot, (uint16_t)acct[slot].gen, buf);
  mbedtls_platform_zeroize(buf, sizeof(buf));
  if (!replacing) { hist_reset_ram(acct[slot]); hist_load(slot, acct[slot]); }
  acct_reset_runtime(slot);
  g_pendingLabel[0] = 0;
  web_token_set_result(true, nullptr);
  Serial.printf("[ACCT] token %s no slot %d (%s)\n", replacing ? "trocado" : "adicionado",
                slot, g_accts.label[slot]);
  request_state(ST_MAIN);
  scheduler_request_refresh();
}

// ============================================================
// Dashboard — helpers visuais
// ============================================================
// label posicionado vazio (preenchido em refresh_ui_values/dash_tick)
static lv_obj_t *tlabel(lv_obj_t *p, const lv_font_t *f, uint32_t c, int x, int y) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(c), 0);
  lv_label_set_text(l, "");
  lv_obj_set_pos(l, x, y);
  return l;
}
static lv_obj_t *tstatic(lv_obj_t *p, const char *txt, const lv_font_t *f, uint32_t c, int x, int y) {
  lv_obj_t *l = mklabel(p, txt, f, c);
  lv_obj_set_pos(l, x, y);
  return l;
}
static void tile_setup(lv_obj_t *t) {
  lv_obj_set_style_bg_opa(t, 0, 0);
  lv_obj_set_style_border_width(t, 0, 0);
  lv_obj_set_style_pad_all(t, 0, 0);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
}
// card moderno: superfície arredondada SEM borda (estrutura por cor, não caixa)
static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_set_pos(c, x, y); lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_radius(c, 18, 0);
  lv_obj_set_style_pad_all(c, 14, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}
// chip com fundo tintado + texto na cor (mais leve que chip sólido)
static lv_obj_t *mkchip(lv_obj_t *p, int x, int y) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, LV_SIZE_CONTENT, 24);
  lv_obj_set_style_radius(o, 12, 0);
  lv_obj_set_style_pad_hor(o, 10, 0);
  lv_obj_set_style_pad_ver(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *l = lv_label_create(o);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_label_set_text(l, ""); lv_obj_center(l);
  return o;
}
static void set_chip(lv_obj_t *o, const char *txt, uint32_t col) {
  if (!o) return;
  lv_color_t c = lv_color_hex(col);
  lv_obj_set_style_bg_color(o, lv_color_mix(c, lv_color_hex(C_BG), 60), 0);
  lv_obj_t *l = lv_obj_get_child(o, 0);
  if (l) { lv_label_set_text(l, txt[0] ? txt : "--"); lv_obj_set_style_text_color(l, c, 0); }
}
// peça retangular arredondada do mascote
static lv_obj_t *rrect(lv_obj_t *p, int x, int y, int w, int h, int r, uint32_t col) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
// humor do modelo: sonda real (HTTP) + incidentes do status.claude.com
static int model_mood(int i) {
  bool inc = (i == 0) ? g_status.haikuUp : (i == 1) ? g_status.sonnetUp
           : (i == 2) ? g_status.opusUp  : g_status.fableUp;
  int c = g_models[i].pr.code;
  if (!inc) return 3;
  if (c == 0) return 0;
  if (c == 200) return 1;
  if (c == 429) return 2;
  if (c == 404) return 4;
  return 3;                              // rede / 5xx / auth
}

// adereço pixel que identifica cada modelo (flutuando sobre a cabeça)
static void build_accessory(lv_obj_t *c, int model) {
  switch (model) {
    case 0:                              // Haiku: raio
      rrect(c, 46, 0, 8, 7, 1, C_WARN);
      rrect(c, 41, 5, 8, 7, 1, C_WARN);
      rrect(c, 46, 10, 8, 7, 1, C_WARN);
      break;
    case 1:                              // Sonnet: nota musical
      rrect(c, 50, 0, 10, 4, 1, 0x7DD3FC);
      rrect(c, 50, 0, 4, 13, 1, 0x7DD3FC);
      rrect(c, 44, 10, 8, 7, 3, 0x7DD3FC);
      break;
    case 2:                              // Opus: coroa
      rrect(c, 30, 4, 7, 8, 1, C_WARN);
      rrect(c, 41, 1, 7, 11, 1, C_WARN);
      rrect(c, 52, 4, 7, 8, 1, C_WARN);
      rrect(c, 30, 12, 29, 6, 1, C_WARN);
      break;
    case 3:                              // Fable: faisca (estrela 4 pontas)
      rrect(c, 41, 0, 6, 17, 2, 0xC4B5FD);
      rrect(c, 36, 6, 16, 6, 2, 0xC4B5FD);
      break;
  }
}

// Mascote da pagina de modelos: Clawd oficial + humor + adereço.
static void build_model_mascot(lv_obj_t *parent, int cx, int i) {
  if (g_mascN >= NMODELS) return;
  int mood = model_mood(i);
  int baseY = 14;
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_pos(c, cx - 44, baseY); lv_obj_set_size(c, 88, 80);
  lv_obj_set_style_bg_opa(c, 0, 0); lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0); lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *img = lv_image_create(c);
  lv_image_set_src(img, &img_clawd_md);
  lv_obj_set_pos(img, 0, 20);

  build_accessory(c, i);

  const int ex[2] = {CLAWD_MD_EYE0_X, CLAWD_MD_EYE1_X};
  const int ey = CLAWD_MD_EYE0_Y + 20, ew = CLAWD_MD_EYE0_W, eh = CLAWD_MD_EYE0_H;

  Mascot &m = g_masc[g_mascN];
  m.cont = c; m.img = img; m.baseY = baseY; m.mood = mood;
  m.lid[0] = m.lid[1] = nullptr; m.drop = nullptr;

  if (mood == 1) {                       // ok: pálpebras escondidas p/ piscar
    for (int k = 0; k < 2; k++) {
      m.lid[k] = rrect(c, ex[k] - 1, ey - 1, ew + 2, eh + 2, 1, C_ACCENT);
      lv_obj_add_flag(m.lid[k], LV_OBJ_FLAG_HIDDEN);
    }
  } else if (mood == 2) {                // limitado: gota de suor
    m.drop = rrect(c, 70, 24, 6, 10, 3, 0x7DD3FC);
  } else if (mood == 3) {                // erro/incidente: cinza + olhos em X
    lv_obj_set_style_image_recolor(img, lv_color_hex(0x6A6A74), 0);
    lv_obj_set_style_image_recolor_opa(img, 190, 0);
    lv_obj_set_y(img, 24);               // caidinho
    for (int k = 0; k < 2; k++) {
      g_mXPts[i][k * 2][0]     = { (lv_value_precise_t)(ex[k] - 2), (lv_value_precise_t)(ey + 2) };
      g_mXPts[i][k * 2][1]     = { (lv_value_precise_t)(ex[k] + ew + 2), (lv_value_precise_t)(ey + eh + 6) };
      g_mXPts[i][k * 2 + 1][0] = { (lv_value_precise_t)(ex[k] + ew + 2), (lv_value_precise_t)(ey + 2) };
      g_mXPts[i][k * 2 + 1][1] = { (lv_value_precise_t)(ex[k] - 2), (lv_value_precise_t)(ey + eh + 6) };
      for (int l = 0; l < 2; l++) {
        lv_obj_t *ln = lv_line_create(c);
        lv_line_set_points(ln, g_mXPts[i][k * 2 + l], 2);
        lv_obj_set_style_line_width(ln, 3, 0);
        lv_obj_set_style_line_color(ln, lv_color_hex(C_BAD), 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
      }
    }
  } else if (mood == 4) {                // n/d p/ o plano: dormindo
    lv_obj_set_style_image_recolor(img, lv_color_hex(0x6A6A74), 0);
    lv_obj_set_style_image_recolor_opa(img, 170, 0);
    for (int k = 0; k < 2; k++)
      m.lid[k] = rrect(c, ex[k] - 1, ey + eh / 2, ew + 2, eh / 2 + 1, 1, 0x8A8A94);
    lv_obj_set_style_opa(c, 180, 0);
  } else {                               // nunca sondado: apagadinho
    lv_obj_set_style_opa(c, 140, 0);
  }
  g_mascN++;
}

static void model_chip(int i, char *out, size_t sz, uint32_t *col) {
  int c = g_models[i].pr.code;
  if (c == 0)             { strlcpy(out, "--", sz);        *col = C_MUTED; }
  else if (c == 200)      { snprintf(out, sz, "OK %.1fs", g_models[i].pr.ms / 1000.0f); *col = C_OK; }
  else if (c == 429)      { strlcpy(out, TRS("LIMITADO", "LIMITED"), sz); *col = C_WARN; }
  else if (c == 404)      { strlcpy(out, TRS("N/D", "N/A"), sz); *col = C_MUTED; }
  else if (c == 401 || c == 403) { strlcpy(out, "AUTH", sz); *col = C_BAD; }
  else if (c < 0)         { strlcpy(out, TRS("REDE", "NET"), sz); *col = C_BAD; }
  else                    { snprintf(out, sz, TRS("ERRO %d", "ERR %d"), c); *col = C_BAD; }
}

// ============================================================
// Builders dos 4 tiles
// ============================================================
// Tile 0 — AGORA (fase 2, item 24): uma coluna por slot salvo (1 a 4 colunas
// iguais) com a % da 5h, os resets, a semana e a linha de estado/projeção.
// Ainda no visual da v2.3; o screen_home da fase 3 substitui esta tela.
// Fontes por número de colunas (tabela do item 24).
static const lv_font_t *col_pct_font(int n) {
  return n <= 1 ? &lv_font_montserrat_48 : n == 2 ? &lv_font_montserrat_40
       : n == 3 ? &lv_font_montserrat_28 : &lv_font_montserrat_24;
}
static const lv_font_t *col_txt_font(int n) {
  return n <= 2 ? &lv_font_montserrat_14 : &lv_font_montserrat_12;
}
// Tocar no card de uma conta muda o foco (Technical Plan §3).
static void col_click_cb(lv_event_t *e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  if (slot == g_accts.active || slot < 0 || slot >= ACCT_MAX || !g_accts.used[slot]) return;
  accountSetActive(g_prefs, g_accts, slot);
  Serial.printf("[ACCT] foco -> slot %d (%s)\n", slot, g_accts.label[slot]);
  request_state(ST_MAIN);
}
static lv_obj_t *col_label(lv_obj_t *p, const lv_font_t *f, uint32_t c, lv_label_long_mode_t mode) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(c), 0);
  lv_obj_set_width(l, lv_pct(100));
  lv_label_set_long_mode(l, mode);
  lv_label_set_text(l, "");
  return l;
}
static void build_tile_agora(lv_obj_t *t) {
  int n = 0;
  for (int s = 0; s < ACCT_MAX; s++) if (g_accts.used[s]) n++;
  if (n == 0) return;
  const int GAP = 8, X0 = 8, W = 464, H = 212;
  int w = (W - GAP * (n - 1)) / n;
  const lv_font_t *fp = col_pct_font(n), *ft = col_txt_font(n);
  g_ui.colN = 0;
  for (int s = 0; s < ACCT_MAX; s++) {
    if (!g_accts.used[s]) continue;
    int i = g_ui.colN++;
    auto &c = g_ui.col[i];
    c.slot = s;
    c.card = card(t, X0 + i * (w + GAP), 4, w, H);
    lv_obj_set_style_pad_all(c.card, n >= 3 ? 8 : 12, 0);
    lv_obj_set_flex_flow(c.card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c.card, n >= 3 ? 3 : 5, 0);
    bool focus = (s == g_accts.active);
    if (focus && n > 1) {
      lv_obj_set_style_border_width(c.card, 2, 0);
      lv_obj_set_style_border_color(c.card, lv_color_hex(C_ACCENT), 0);
    }
    lv_obj_add_flag(c.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(c.card, col_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)s);

    c.name = col_label(c.card, ft, focus ? C_ACCENT : C_MUTED, LV_LABEL_LONG_DOT);
    if (g_accts.label[s][0]) lv_label_set_text(c.name, g_accts.label[s]);
    else lv_label_set_text_fmt(c.name, TRS("conta %d", "account %d"), s + 1);
    c.pct = col_label(c.card, fp, C_TEXT, LV_LABEL_LONG_CLIP);
    c.bar = lv_bar_create(c.card);
    lv_obj_set_size(c.bar, lv_pct(100), 6);
    lv_bar_set_range(c.bar, 0, 100);
    lv_obj_set_style_bg_color(c.bar, lv_color_hex(C_TRACK), LV_PART_MAIN);
    lv_obj_set_style_radius(c.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(c.bar, 3, LV_PART_INDICATOR);
    lv_obj_clear_flag(c.bar, LV_OBJ_FLAG_CLICKABLE);
    c.l5 = col_label(c.card, ft, C_MUTED, LV_LABEL_LONG_DOT);
    c.l7 = col_label(c.card, ft, C_MUTED, LV_LABEL_LONG_DOT);
    c.st = col_label(c.card, ft, C_MUTED, LV_LABEL_LONG_WRAP);
  }
  // tokens da sessão da conta em foco (bridge), no rodapé
  g_ui.agTok = tlabel(t, &lv_font_montserrat_12, C_MUTED, 8, 224);
  lv_obj_set_width(g_ui.agTok, 464);
  lv_obj_set_style_text_align(g_ui.agTok, LV_TEXT_ALIGN_RIGHT, 0);
}
// Tile 1 — MODELOS: Clawd oficial por modelo (humor animado) + sonda real.
// Incidentes nao aparecem mais aqui: viraram o dialog da tela inicial.
static void build_tile_models(lv_obj_t *t) {
  static const int CENTERS[NMODELS] = {60, 180, 300, 420};
  for (int i = 0; i < NMODELS; i++) {
    build_model_mascot(t, CENTERS[i], i);
    lv_obj_t *n = mklabel(t, g_models[i].name, &lv_font_montserrat_16,
                          model_mood(i) == 1 ? C_TEXT : C_MUTED);
    lv_obj_set_width(n, 104);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(n, CENTERS[i] - 52, 98);
    g_ui.mChip[i] = mkchip(t, 0, 122);
  }
  tstatic(t, TRS("sonda real na API \xE2\x80\xA2 1 modelo por ciclo",
                 "live API probe \xE2\x80\xA2 1 model per cycle"),
          &lv_font_montserrat_12, C_FAINT, 14, 170);
}
// Tile 2 — JANELA 5H: histórico + projeção pontilhada até esgotar.
#define TR_X0 12
#define TR_Y0 10
#define TR_W  440
#define TR_H  126
static int tr_x(uint32_t tt, uint32_t ws, uint32_t we) {
  if (we <= ws) return TR_X0;
  long long v = (long long)(tt - ws) * TR_W / (long long)(we - ws);
  if (v < 0) v = 0; if (v > TR_W) v = TR_W;
  return TR_X0 + (int)v;
}
static int tr_y(float p) {
  if (p < 0) p = 0; if (p > 100) p = 100;
  return TR_Y0 + TR_H - (int)(p * TR_H / 100.0f);
}
static void build_tile_trend(lv_obj_t *t) {
  tstatic(t, TRS("Janela de 5h", "5-hour window"), &lv_font_montserrat_16, C_TEXT, 14, 2);
  tstatic(t, TRS("uso real + projecao", "real usage + projection"), &lv_font_montserrat_12, C_FAINT, 320, 6);

  lv_obj_t *c = card(t, 8, 26, 464, 170);
  lv_obj_set_style_pad_all(c, 0, 0);

  // grade: 25/50/75%
  for (int i = 1; i <= 3; i++) {
    lv_obj_t *g = rrect(c, TR_X0, tr_y(i * 25.0f), TR_W, 1, 0, C_GRID);
    (void)g;
  }
  tstatic(c, "100", &lv_font_montserrat_12, C_FAINT, TR_X0 + TR_W - 24, TR_Y0 - 6);
  tstatic(c, "0",   &lv_font_montserrat_12, C_FAINT, TR_X0 + TR_W - 10, TR_Y0 + TR_H - 14);

  // histórico (linha sólida coral)
  g_ui.trHist = lv_line_create(c);
  lv_obj_set_pos(g_ui.trHist, 0, 0);
  lv_obj_set_style_line_width(g_ui.trHist, 3, 0);
  lv_obj_set_style_line_color(g_ui.trHist, lv_color_hex(C_ACCENT), 0);
  lv_obj_set_style_line_rounded(g_ui.trHist, true, 0);

  // projeção (pontilhada)
  g_ui.trProj = lv_line_create(c);
  lv_obj_set_pos(g_ui.trProj, 0, 0);
  lv_obj_set_style_line_width(g_ui.trProj, 2, 0);
  lv_obj_set_style_line_color(g_ui.trProj, lv_color_hex(C_ACCENT), 0);
  lv_obj_set_style_line_opa(g_ui.trProj, 170, 0);
  lv_obj_set_style_line_dash_width(g_ui.trProj, 6, 0);
  lv_obj_set_style_line_dash_gap(g_ui.trProj, 6, 0);

  // marcador do ponto atual
  g_ui.trDot = rrect(c, 0, 0, 8, 8, 4, C_TEXT);
  lv_obj_add_flag(g_ui.trDot, LV_OBJ_FLAG_HIDDEN);

  // horários do eixo X (início da janela / reset)
  g_ui.trT0 = tlabel(c, &lv_font_montserrat_12, C_FAINT, TR_X0, TR_Y0 + TR_H + 8);
  g_ui.trT1 = tlabel(c, &lv_font_montserrat_12, C_FAINT, TR_X0 + TR_W - 40, TR_Y0 + TR_H + 8);

  g_ui.trCap = tlabel(t, &lv_font_montserrat_16, C_MUTED, 14, 210);
  lv_obj_set_width(g_ui.trCap, 452);
  lv_label_set_long_mode(g_ui.trCap, LV_LABEL_LONG_WRAP);
}
// Tile 3 — RITMO: heatmap por hora com filtro de período.
static void heat_btn_style() {
  const char *names[4] = {TRS("Hoje", "Today"), "7d", "30d", TRS("Tudo", "All")};
  for (int i = 0; i < 4; i++) {
    if (!g_ui.heatBtn[i]) continue;
    bool on = (i == g_heatMode);
    lv_obj_set_style_bg_color(g_ui.heatBtn[i], lv_color_hex(on ? C_ACCENT : C_SURFACE2), 0);
    lv_obj_t *l = lv_obj_get_child(g_ui.heatBtn[i], 0);
    if (l) {
      lv_label_set_text(l, names[i]);
      lv_obj_set_style_text_color(l, lv_color_hex(on ? C_BG : C_MUTED), 0);
    }
  }
}
static void heat_btn_cb(lv_event_t *e) {
  int m = (int)(intptr_t)lv_event_get_user_data(e);
  if (m == g_heatMode) return;
  g_heatMode = m;
  g_prefs.putInt("heatm", m);
  heat_btn_style();
  heat_redraw();
}
static void build_tile_heat(lv_obj_t *t) {
  tstatic(t, TRS("Ritmo por hora", "Hourly rhythm"), &lv_font_montserrat_16, C_TEXT, 14, 6);
  for (int i = 0; i < 4; i++) {
    lv_obj_t *b = lv_button_create(t);
    lv_obj_set_size(b, 52, 30);
    lv_obj_set_pos(b, 246 + i * 56, 0);
    lv_obj_set_style_radius(b, 15, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_ext_click_area(b, 6);
    lv_obj_t *l = mklabel(b, "", &lv_font_montserrat_14, C_MUTED);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, heat_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    g_ui.heatBtn[i] = b;
  }
  heat_btn_style();
  for (int h = 0; h < 24; h++) {
    lv_obj_t *bar = lv_obj_create(t);
    lv_obj_set_size(bar, 13, 4);
    lv_obj_set_pos(bar, 18 + h * 18, 176);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    g_ui.heat[h] = bar;
  }
  int ticks[5] = {0, 6, 12, 18, 23};
  for (int i = 0; i < 5; i++) {
    int h = ticks[i]; char s[4]; snprintf(s, sizeof(s), "%dh", h);
    lv_obj_t *l = mklabel(t, s, &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_pos(l, 14 + h * 18, 186);
  }
  tstatic(t, TRS("quota da janela 5h queimada em cada hora local",
                 "5h-window quota burned per local hour"), &lv_font_montserrat_12, C_FAINT, 14, 214);
}

static void on_tile_changed(lv_event_t *e) {
  (void)e;
  if (!g_ui.tv) return;
  lv_obj_t *act = lv_tileview_get_tile_active(g_ui.tv);
  for (int i = 0; i < NTILES; i++) {
    if (!g_ui.dots[i]) continue;
    bool on = (g_ui.tile[i] == act);
    if (on) g_curTile = i;
    lv_obj_set_style_bg_color(g_ui.dots[i], lv_color_hex(on ? C_ACCENT : C_BORDER), 0);
    lv_obj_set_width(g_ui.dots[i], on ? 18 : 8);
  }
}

// ============================================================
// Atualização de valores
// ============================================================
static void update_tok_row() {
  if (!g_ui.agTok) return;
  const TokenStats &tok = acct[g_accts.active].tok;
  if (tok.atMs == 0 || millis() - tok.atMs > TOK_FRESH_MS) {
    lv_label_set_text(g_ui.agTok, "");
    return;
  }
  char a[16], b[16], s[96];
  fmt_tok(tok.tin, a, sizeof(a));
  fmt_tok(tok.tout, b, sizeof(b));
  snprintf(s, sizeof(s), TRS("tokens na janela: %s entrada \xE2\x80\xA2 %s saida",
                             "window tokens: %s in \xE2\x80\xA2 %s out"), a, b);
  lv_label_set_text(g_ui.agTok, s);
}

// Projeção curta da coluna (ASCII: as Montserrat de hoje não têm ✓ ✗ ·; o
// projection_text() da src/logic entra com a fonte da fase 3).
static void col_proj_text(const AccountState &a, bool narrow, char *out, size_t sz, uint32_t *col) {
  time_t now = time(nullptr);
  uint32_t we = a.usage.h5ResetEpoch;
  *col = C_MUTED;
  if (now < 1000000000L || we == 0) { strlcpy(out, "--", sz); return; }
  int n, head;
  const Sample *hist = hist_data(a, &n, &head);
  ProjResult pr = projection(hist, n, head, a.usage.h5, we, (uint32_t)now);
  if (pr.st == ProjResult::SEM_DADOS) {
    strlcpy(out, narrow ? TRS("Coletando...", "Collecting...")
                        : TRS("Coletando dados...", "Collecting data..."), sz);
  } else if (pr.st == ProjResult::ESGOTADA) {
    strlcpy(out, TRS("Esgotada", "Exhausted"), sz);
    *col = C_BAD;
  } else if (pr.st == ProjResult::ESGOTA) {
    char hm[12]; fmt_hm(pr.etaEpoch, hm, sizeof(hm));
    if (narrow) snprintf(out, sz, TRS("Esgota %s", "Out %s"), hm);
    else snprintf(out, sz, TRS("Esgota as %s (em %dh%02dm)", "Runs out %s (in %dh%02dm)"),
                  hm, pr.minutes / 60, pr.minutes % 60);
    *col = pr.minutes < 60 ? C_BAD : C_WARN;
  } else if (pr.rate > 0.02f) {
    snprintf(out, sz, TRS("Nao esgota (~%d%%)", "Won't run out (~%d%%)"), pr.pctAtReset);
    *col = C_OK;
  } else {
    strlcpy(out, TRS("Uso estavel", "Stable usage"), sz);
    *col = C_OK;
  }
}

// Valores de uma coluna, com o texto do card na precedência do Technical
// Plan §4: SEM CHAVE; falha sem dado ("! CLASSE", --%); falha com dado
// ("! CLASSE • idade", últimos valores); pendente ("consultando...", --%);
// senão a projeção. O esmaecimento da falha com dado entra na fase 3.
static void col_update(int i) {
  auto &c = g_ui.col[i];
  if (!c.card) return;
  const AccountState &a = acct[c.slot];
  bool narrow = g_ui.colN >= 3;
  char st[64], b[48], e[24];
  uint32_t stCol = C_MUTED;
  bool showData = true;
  FetchClass k = a.lastClass;
  bool failed = (k != FC_NONE && k != FC_OK);
  if (!a.keyOk) {
    strlcpy(st, TRS("SEM CHAVE", "NO KEY"), sizeof(st));
    stCol = C_BAD; showData = false;
  } else if (failed && a.usageAtEpoch == 0) {
    snprintf(st, sizeof(st), LV_SYMBOL_WARNING " %s", fetch_class_name(k));
    stCol = C_BAD; showData = false;
  } else if (failed) {
    uint32_t now = (uint32_t)time(nullptr);
    char age[12];
    age_text(now > a.usageAtEpoch ? now - a.usageAtEpoch : 0, age, sizeof(age));
    snprintf(st, sizeof(st), LV_SYMBOL_WARNING " %s \xE2\x80\xA2 %s", fetch_class_name(k), age);
    stCol = C_BAD;
  } else if (k == FC_NONE) {
    strlcpy(st, TRS("consultando...", "querying..."), sizeof(st));
    showData = false;
  } else {
    col_proj_text(a, narrow, st, sizeof(st), &stCol);
  }
  lv_label_set_text(c.st, st);
  lv_obj_set_style_text_color(c.st, lv_color_hex(stCol), 0);

  const UsageData &u = a.usage;
  if (!showData) {
    lv_label_set_text(c.pct, "--%");
    lv_obj_set_style_text_color(c.pct, lv_color_hex(C_FAINT), 0);
    lv_bar_set_value(c.bar, 0, LV_ANIM_OFF);
    lv_label_set_text(c.l5, narrow ? "5h --" : TRS("5h: --", "5h: --"));
    lv_label_set_text(c.l7, narrow ? "7d --" : TRS("semana: --", "week: --"));
    return;
  }
  snprintf(b, sizeof(b), "%d%%", (int)(u.h5 + 0.5f));
  lv_label_set_text(c.pct, b);
  lv_color_t gc = grad_color(u.h5);
  lv_obj_set_style_text_color(c.pct, gc, 0);
  lv_bar_set_value(c.bar, (int32_t)(u.h5 + 0.5f), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(c.bar, gc, LV_PART_INDICATOR);
  fmt_eta(u.h5ResetEpoch, e, sizeof(e));
  snprintf(b, sizeof(b), narrow ? "5h %s" : TRS("5h: reseta em %s", "5h: resets in %s"), e);
  lv_label_set_text(c.l5, b);
  fmt_eta(u.d7ResetEpoch, e, sizeof(e));
  snprintf(b, sizeof(b), narrow ? "7d %d%% \xE2\x80\xA2 %s" : TRS("semana: %d%% \xE2\x80\xA2 %s", "week: %d%% \xE2\x80\xA2 %s"),
           (int)(u.d7 + 0.5f), e);
  lv_label_set_text(c.l7, b);
}

// Contadores, idades e projeções (1s) — separado da chegada de resultados.
static void dash_tick() {
  if (g_state != ST_MAIN) return;
  for (int i = 0; i < g_ui.colN; i++) col_update(i);
  set_hdr_status();
}

// Tendência da janela 5h: histórico + projeção pontilhada até esgotar.
static void trend_redraw() {
  if (!g_ui.trHist) return;
  const AccountState &a = acct[g_accts.active];
  int histN, histHead;
  const Sample *hist = hist_data(a, &histN, &histHead);
  time_t now = time(nullptr);
  uint32_t we = a.usage.h5ResetEpoch;
  bool clockOk = (now > 1000000000L) && we != 0;

  if (!clockOk) {
    lv_line_set_points(g_ui.trHist, g_trPts, 0);
    lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
    lv_obj_add_flag(g_ui.trDot, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_ui.trCap, TRS("Aguardando dados da janela...", "Waiting for window data..."));
    lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_MUTED), 0);
    return;
  }
  uint32_t ws = we - 5 * 3600;

  // horários do eixo
  char t0[12], t1[12], b[112];
  fmt_hm(ws, t0, sizeof(t0)); fmt_hm(we, t1, sizeof(t1));
  lv_label_set_text(g_ui.trT0, t0);
  lv_label_set_text(g_ui.trT1, t1);

  // pontos do histórico dentro da janela
  int n = 0;
  for (int i = 0; i < histN && n < HIST_MAX; i++) {
    Sample s = hist[(histHead - histN + i + HIST_MAX * 2) % HIST_MAX];
    if (s.t == 0 || s.t < ws || s.t > (uint32_t)now) continue;
    g_trPts[n].x = tr_x(s.t, ws, we);
    g_trPts[n].y = tr_y(s.h5);
    n++;
  }
  // ponto atual (leitura mais recente)
  uint32_t nowClamped = ((uint32_t)now > we) ? we : (uint32_t)now;
  if (n < HIST_MAX) {
    g_trPts[n].x = tr_x(nowClamped, ws, we);
    g_trPts[n].y = tr_y(a.usage.h5);
    n++;
  }
  lv_line_set_points(g_ui.trHist, g_trPts, n);

  int cx = tr_x(nowClamped, ws, we), cy = tr_y(a.usage.h5);
  lv_obj_set_pos(g_ui.trDot, cx - 4, cy - 4);
  lv_obj_clear_flag(g_ui.trDot, LV_OBJ_FLAG_HIDDEN);

  // Estado, hora de esgotar e % no reset vêm de projection() (src/logic).
  // Os textos continuam os da v2.3: projection_text() usa glifos (✓ ✗ ·) que
  // as fontes Montserrat de hoje não têm; ela entra com a fonte nova (fase 3).
  ProjResult pr = projection(hist, histN, histHead, a.usage.h5, we, (uint32_t)now);

  if (pr.st == ProjResult::SEM_DADOS) {
    lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
    lv_label_set_text(g_ui.trCap, TRS("Coletando dados... (~alguns minutos)",
                                      "Collecting data... (~a few minutes)"));
    lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_MUTED), 0);
    return;
  }

  char e[32];
  if (pr.st == ProjResult::ESGOTADA) {
    lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
    fmt_eta(we, e, sizeof(e));
    snprintf(b, sizeof(b), TRS("Janela esgotada \xE2\x80\xA2 reseta em %s",
                               "Window exhausted \xE2\x80\xA2 resets in %s"), e);
    lv_label_set_text(g_ui.trCap, b);
    lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_BAD), 0);
  } else if (pr.st == ProjResult::ESGOTA) {
    g_trProjPts[0].x = cx; g_trProjPts[0].y = cy;
    g_trProjPts[1].x = tr_x(pr.etaEpoch, ws, we);
    g_trProjPts[1].y = tr_y(100);
    char hm[12]; fmt_hm(pr.etaEpoch, hm, sizeof(hm));
    uint32_t col = pr.minutes < 60 ? C_BAD : C_WARN;
    snprintf(b, sizeof(b), TRS("No ritmo atual, esgota as %s (em %dh%02dm)",
                               "At this pace, runs out at %s (in %dh%02dm)"),
             hm, pr.minutes / 60, pr.minutes % 60);
    lv_label_set_text(g_ui.trCap, b);
    lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(col), 0);
    lv_line_set_points(g_ui.trProj, g_trProjPts, 2);
  } else if (pr.rate > 0.02f) {
    // OK_ATE_RESET com consumo relevante: linha até o reset, na % prevista
    g_trProjPts[0].x = cx; g_trProjPts[0].y = cy;
    g_trProjPts[1].x = tr_x(we, ws, we);
    g_trProjPts[1].y = tr_y(pr.pctAtReset);
    snprintf(b, sizeof(b), TRS("No ritmo atual, NAO esgota antes do reset (~%d%%)",
                               "At this pace, does NOT run out before reset (~%d%%)"),
             pr.pctAtReset);
    lv_label_set_text(g_ui.trCap, b);
    lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_OK), 0);
    lv_line_set_points(g_ui.trProj, g_trProjPts, 2);
  } else {
    lv_line_set_points(g_ui.trProj, g_trProjPts, 0);
    lv_label_set_text(g_ui.trCap, TRS("Uso estavel \xE2\x80\xA2 sem risco no momento",
                                      "Stable usage \xE2\x80\xA2 no risk right now"));
    lv_obj_set_style_text_color(g_ui.trCap, lv_color_hex(C_OK), 0);
  }
}

static void heat_redraw() {
  if (!g_ui.heat[0]) return;
  float data[24];
  heat_mode_data(acct[g_accts.active], g_heatMode, (uint32_t)time(nullptr), g_tzOffset * 3600, data);
  float mx = 1.0f;
  for (int h = 0; h < 24; h++) if (data[h] > mx) mx = data[h];
  int curHour = -1; time_t now = time(nullptr);
  if (now > 1000000000L) { struct tm tv; localtime_r(&now, &tv); curHour = tv.tm_hour; }
  for (int h = 0; h < 24; h++) {
    if (!g_ui.heat[h]) continue;
    float r = data[h] / mx; if (r < 0) r = 0; if (r > 1) r = 1;
    int hgt = 4 + (int)(r * 114);
    lv_obj_set_size(g_ui.heat[h], 13, hgt);
    lv_obj_set_y(g_ui.heat[h], 176 - hgt);
    lv_obj_set_style_bg_color(g_ui.heat[h], lv_color_hex(h == curHour ? C_TEXT : C_ACCENT), 0);
    lv_obj_set_style_bg_opa(g_ui.heat[h], (lv_opa_t)(70 + (int)(r * 185)), 0);
  }
}

// ============================================================
// Momentos — animações de limiar (25/50/70/100% nas janelas 5h e semanal)
// Overlay em lv_layer_top: Clawd XL com "humor" por nível + contador de %
// + medidor acendendo. Dispara ao cruzar o limiar; toque dispensa.
// Animação 100% procedural (moment_tick no loop), sem lv_anim pendurado.
// ============================================================
struct MomentUI {
  lv_obj_t *scrim, *box, *img, *pct, *seg[NSEG];
  lv_obj_t *lid[2], *drop[2], *ring, *xline[4];
  int win, thr, fromPct;
  int boxY;
  uint32_t t0;
};
static MomentUI g_mo = {};
static uint32_t g_momentUntil = 0;
static lv_point_precise_t g_moXPts[4][2];      // olhos em X (KO)

static void moment_close() {
  if (!g_mo.scrim) return;
  lv_obj_delete(g_mo.scrim);
  memset(&g_mo, 0, sizeof(g_mo));
}
// Fechar pelo toque ou pelo tempo mostra o próximo momento guardado pelo
// agendador (moment_flush). moment_close() sozinho não chama: render_state()
// e show_moment() também o usam para limpar a tela.
static void moment_close_cb(lv_event_t *e) { (void)e; moment_close(); moment_flush(); }

// Hook do agendador (fase 2): só mostra na tela inicial, sem outro momento
// aberto e sem rebuild pendente (que fecharia o overlay).
static bool moment_hook(int slot, int win, int thr) {
  if (g_mo.scrim || g_state != ST_MAIN || g_dirty) return false;
  show_moment(slot, win, thr);
  return true;
}

static void show_moment(int slot, int win, int thr) {
  moment_close();
  g_mo.win = win; g_mo.thr = thr;
  g_mo.fromPct = (thr == 25) ? 0 : (thr == 50) ? 25 : (thr == 70) ? 50 : 70;
  g_mo.t0 = millis();
  g_momentUntil = g_mo.t0 + 4600;

  lv_obj_t *s = lv_obj_create(lv_layer_top());
  g_mo.scrim = s;
  lv_obj_set_pos(s, 0, 0); lv_obj_set_size(s, 480, 320);
  lv_obj_set_style_bg_color(s, lv_color_hex(0x0D0D10), 0);
  lv_obj_set_style_bg_opa(s, 248, 0);
  lv_obj_set_style_border_width(s, 0, 0);
  lv_obj_set_style_radius(s, 0, 0);
  lv_obj_set_style_pad_all(s, 0, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, moment_close_cb, LV_EVENT_CLICKED, NULL);

  // anel de alerta (só no 100%; pisca no tick)
  if (thr == 100) {
    g_mo.ring = lv_obj_create(s);
    lv_obj_set_pos(g_mo.ring, 4, 4); lv_obj_set_size(g_mo.ring, 472, 312);
    lv_obj_set_style_bg_opa(g_mo.ring, 0, 0);
    lv_obj_set_style_radius(g_mo.ring, 14, 0);
    lv_obj_set_style_border_width(g_mo.ring, 4, 0);
    lv_obj_set_style_border_color(g_mo.ring, lv_color_hex(C_BAD), 0);
    lv_obj_clear_flag(g_mo.ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(g_mo.ring, LV_OBJ_FLAG_CLICKABLE);
  }

  // caixa do Clawd (a caixa inteira anima: bounce / shake / queda de entrada)
  g_mo.boxY = 110;
  lv_obj_t *bx = lv_obj_create(s);
  g_mo.box = bx;
  lv_obj_set_pos(bx, 36, g_mo.boxY - 40);
  lv_obj_set_size(bx, 176, 116);
  lv_obj_set_style_bg_opa(bx, 0, 0);
  lv_obj_set_style_border_width(bx, 0, 0);
  lv_obj_set_style_pad_all(bx, 0, 0);
  lv_obj_clear_flag(bx, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(bx, LV_OBJ_FLAG_CLICKABLE);

  g_mo.img = lv_image_create(bx);
  lv_image_set_src(g_mo.img, &img_clawd_xl);
  lv_obj_set_pos(g_mo.img, 0, 0);

  // humores (sobre os buracos dos olhos do sprite)
  const int ex[2] = {CLAWD_XL_EYE0_X, CLAWD_XL_EYE1_X};
  const int ey = CLAWD_XL_EYE0_Y, ew = CLAWD_XL_EYE0_W, eh = CLAWD_XL_EYE0_H;
  if (thr == 50) {
    // focado: pálpebras cobrindo metade dos olhos + 1 gota de suor
    for (int i = 0; i < 2; i++)
      g_mo.lid[i] = rrect(bx, ex[i] - 1, ey - 1, ew + 2, eh / 2 + 2, 0, C_ACCENT);
    g_mo.drop[0] = rrect(bx, 150, 6, 8, 12, 4, 0x7DD3FC);
  } else if (thr == 70) {
    // preocupado: olhos arregalados + 2 gotas (a caixa treme no tick)
    for (int i = 0; i < 2; i++)
      g_mo.lid[i] = rrect(bx, ex[i] - 3, ey - 4, ew + 6, eh + 8, 2, 0x0D0D10);
    g_mo.drop[0] = rrect(bx, 150, 6, 8, 12, 4, 0x7DD3FC);
    g_mo.drop[1] = rrect(bx, 18, 12, 8, 12, 4, 0x7DD3FC);
  } else if (thr == 100) {
    // KO: corpo acinzentado + olhos em X
    lv_obj_set_style_image_recolor(g_mo.img, lv_color_hex(0x6A6A74), 0);
    lv_obj_set_style_image_recolor_opa(g_mo.img, 190, 0);
    for (int i = 0; i < 2; i++) {
      g_moXPts[i * 2][0]     = { (lv_value_precise_t)(ex[i] - 2), (lv_value_precise_t)(ey - 1) };
      g_moXPts[i * 2][1]     = { (lv_value_precise_t)(ex[i] + ew + 2), (lv_value_precise_t)(ey + eh + 1) };
      g_moXPts[i * 2 + 1][0] = { (lv_value_precise_t)(ex[i] + ew + 2), (lv_value_precise_t)(ey - 1) };
      g_moXPts[i * 2 + 1][1] = { (lv_value_precise_t)(ex[i] - 2), (lv_value_precise_t)(ey + eh + 1) };
      for (int k = 0; k < 2; k++) {
        lv_obj_t *ln = lv_line_create(bx);
        lv_line_set_points(ln, g_moXPts[i * 2 + k], 2);
        lv_obj_set_style_line_width(ln, 4, 0);
        lv_obj_set_style_line_color(ln, lv_color_hex(C_BAD), 0);
        lv_obj_set_style_line_rounded(ln, true, 0);
        g_mo.xline[i * 2 + k] = ln;
      }
    }
  }

  // coluna de texto à direita
  lv_obj_t *win_l = mklabel(s, win == 0 ? TRS("JANELA DE 5 HORAS", "5-HOUR WINDOW")
                                        : TRS("JANELA SEMANAL", "WEEKLY WINDOW"),
                            &lv_font_montserrat_20, C_MUTED);
  lv_obj_set_pos(win_l, 240, 42);
  g_mo.pct = tlabel(s, &lv_font_montserrat_48, C_OK, 240, 70);
  const char *MSG[4] = {
    TRS("Comecando \xE2\x80\xA2 ritmo tranquilo",       "Just starting \xE2\x80\xA2 easy pace"),
    TRS("Metade da janela usada",                       "Half the window used"),
    TRS("Atencao \xE2\x80\xA2 uso alto",                "Heads up \xE2\x80\xA2 heavy usage"),
    TRS("Limite atingido \xE2\x80\xA2 aguarde o reset", "Limit reached \xE2\x80\xA2 wait for the reset"),
  };
  int mi = (thr == 25) ? 0 : (thr == 50) ? 1 : (thr == 70) ? 2 : 3;
  lv_obj_t *msg = mklabel(s, MSG[mi], &lv_font_montserrat_16, C_TEXT);
  lv_obj_set_pos(msg, 240, 148);
  lv_obj_set_width(msg, 232);
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);

  for (int i = 0; i < NSEG; i++)
    g_mo.seg[i] = rrect(s, 240 + i * 11, 196, 8, 16, 2, C_TRACK);

  char e[32], b[48];
  const UsageData &u = acct[(slot >= 0 && slot < ACCT_MAX) ? slot : g_accts.active].usage;
  fmt_eta(win == 0 ? u.h5ResetEpoch : u.d7ResetEpoch, e, sizeof(e));
  snprintf(b, sizeof(b), TRS("reseta em %s", "resets in %s"), e);
  lv_obj_t *eta = mklabel(s, b, &lv_font_montserrat_14, C_FAINT);
  lv_obj_set_pos(eta, 240, 228);

  lv_obj_t *hint = mklabel(s, TRS("toque para fechar", "tap to close"), &lv_font_montserrat_12, C_FAINT);
  lv_obj_set_pos(hint, 352, 296);
}

// Anima o momento (chamado a cada frame do loop enquanto o overlay existe).
static void moment_tick() {
  if (!g_mo.scrim) return;
  uint32_t t = millis() - g_mo.t0;

  // entrada: Clawd cai de cima com acomodação; depois o humor manda
  int y = g_mo.boxY, x = 36;
  if (t < 450) {
    float p = t / 450.0f;
    y = g_mo.boxY - (int)((1.0f - p) * (1.0f - p) * 60.0f);
  } else if (g_mo.thr == 25 || g_mo.thr == 50) {
    y = g_mo.boxY + (int)(4.0f * sinf((t - 450) / 260.0f));           // bounce feliz
  } else if (g_mo.thr == 70) {
    x = 36 + (((t / 70) % 2) ? 2 : -2);                                // treme
  } else if (g_mo.thr == 100) {
    y = g_mo.boxY + 6;                                                 // caído
  }
  lv_obj_set_pos(g_mo.box, x, y);

  // contador de % (200ms..1100ms) + medidor acendendo em sequência
  float p = (t < 200) ? 0 : (t > 1100 ? 1.0f : (t - 200) / 900.0f);
  float v = g_mo.fromPct + (g_mo.thr - g_mo.fromPct) * p;
  char b[12]; snprintf(b, sizeof(b), "%d%%", (int)(v + 0.5f));
  lv_label_set_text(g_mo.pct, b);
  lv_obj_set_style_text_color(g_mo.pct, grad_color(v), 0);
  set_meter(g_mo.seg, v);

  // gotas de suor caindo em ciclo
  for (int i = 0; i < 2; i++) {
    if (!g_mo.drop[i]) continue;
    uint32_t c = (t + i * 450) % 900;
    lv_obj_set_y(g_mo.drop[i], (i ? 12 : 6) + (int)(c * 34 / 900));
    lv_obj_set_style_bg_opa(g_mo.drop[i], (lv_opa_t)(255 - c * 190 / 900), 0);
  }
  // anel vermelho piscando (100%)
  if (g_mo.ring)
    lv_obj_set_style_border_opa(g_mo.ring, ((t / 350) % 2) ? 190 : 30, 0);

  if (millis() > g_momentUntil) { moment_close(); moment_flush(); }
}

// ============================================================
// Dialog de incidente — status.claude.com reportando problema nao resolvido
// Modal em lv_layer_top: escurece a tela inicial, some no toque. So volta a
// aparecer depois que o incidente e resolvido e um novo surge (g_incAck).
// ============================================================
static lv_obj_t *g_incDlg = nullptr;   // overlay vivo (nullptr = fechado)
static bool g_incAck = false;          // usuario ja dispensou o incidente atual
static uint8_t g_incSev = 0xFF;        // severidade desenhada no dialog atual
static uint8_t g_incMask = 0xFF;       // modelos afetados desenhados no dialog

// Bitmask dos modelos citados pelos incidentes abertos (Haiku, Sonnet, Opus,
// Fable). Vem do mesmo scan de palavra-chave que decide se o dialog aparece.
static uint8_t affected_mask() {
  return (uint8_t)((!g_status.haikuUp)       | (!g_status.sonnetUp) << 1 |
                   (!g_status.opusUp)   << 2 | (!g_status.fableUp)  << 3);
}

// "Opus 4.6 \u2022 Sonnet 4.5". Os quatro afetados viram "Todos"; false = nenhum
// modelo citado, ai a linha some.
//
// A versao so entra com ate 2 modelos: com 3, "Haiku 4.5 \u2022 Sonnet 4.5 \u2022 Opus 4.6"
// passa dos 262 px da coluna e o LONG_DOT cortaria justamente o ultimo nome.
static bool affected_models(char *out, size_t sz) {
  static const char *NAMES[4] = {"Haiku", "Sonnet", "Opus", "Fable"};
  uint8_t mask = affected_mask();
  out[0] = 0;
  if (mask == 0x0F) { strlcpy(out, TRS("Todos", "All"), sz); return true; }

  int total = 0;
  for (int i = 0; i < 4; i++) if (mask & (1 << i)) total++;
  bool withVer = (total <= 2);

  int n = 0;
  for (int i = 0; i < 4; i++) {
    if (!(mask & (1 << i))) continue;
    if (n++) strlcat(out, " \xE2\x80\xA2 ", sz);
    strlcat(out, NAMES[i], sz);
    if (withVer && g_status.version[i][0]) {
      strlcat(out, " ", sz);
      strlcat(out, g_status.version[i], sz);
    }
  }
  return n > 0;
}

// Palavra do "impact" do Statuspage. Todo incidente alerta: quando nao ha
// gravidade utilizavel o chip diz isso na cara, em vez de sumir e deixar o
// modal parecendo generico.
static const char *severity_label(uint8_t sev) {
  switch (sev) {
    case SEV_CRITICAL: return TRS("CRITICO", "CRITICAL");
    case SEV_MAJOR:    return TRS("GRAVE", "MAJOR");
    case SEV_MINOR:    return TRS("LEVE", "MINOR");
    default:           return TRS("IMPACTO NAO DECLARADO", "IMPACT NOT DECLARED");
  }
}

// Cor do dialog por gravidade reportada no "impact" do status.claude.com.
static uint32_t severity_color(uint8_t sev) {
  return sev >= SEV_CRITICAL ? C_BAD
       : sev == SEV_MAJOR    ? C_SEV_MAJOR
                             : C_WARN;   // minor / desconhecido
}

static void incident_close() {
  if (!g_incDlg) return;
  lv_obj_delete(g_incDlg);
  g_incDlg = nullptr;
}
static void incident_dismiss_cb(lv_event_t *e) { (void)e; g_incAck = true; incident_close(); }

static void show_incident() {
  incident_close();
  g_incSev = g_status.severity;
  g_incMask = affected_mask();
  uint32_t sevCol = severity_color(g_incSev);

  lv_obj_t *s = lv_obj_create(lv_layer_top());
  g_incDlg = s;
  lv_obj_set_pos(s, 0, 0); lv_obj_set_size(s, 480, 320);
  lv_obj_set_style_bg_color(s, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(s, 210, 0);
  lv_obj_set_style_border_width(s, 0, 0);
  lv_obj_set_style_radius(s, 0, 0);
  lv_obj_set_style_pad_all(s, 0, 0);
  lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, incident_dismiss_cb, LV_EVENT_CLICKED, NULL);

  // Caixa 420x206 (pad 18 -> area util 384x170): Clawd "morto" na esquerda,
  // coluna com titulo + chip de gravidade + modelos afetados + link + botao.
  lv_obj_t *box = lv_obj_create(s);
  lv_obj_set_size(box, 420, 206);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_radius(box, 18, 0);
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(sevCol), 0);
  lv_obj_set_style_pad_all(box, 18, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dead = lv_image_create(box);
  lv_image_set_src(dead, &img_clawd_dead);
  lv_obj_align(dead, LV_ALIGN_LEFT_MID, 0, 0);

  const int TX = CLAWD_DEAD_W + 18;          // inicio da coluna de texto
  const int TW = 384 - TX;                   // largura util dessa coluna

  lv_obj_t *t = mklabel(box, TRS(LV_SYMBOL_WARNING "  INCIDENTE ATIVO  " LV_SYMBOL_WARNING,
                                 LV_SYMBOL_WARNING "  ACTIVE INCIDENT  " LV_SYMBOL_WARNING),
                        &lv_font_montserrat_20, sevCol);
  lv_obj_set_width(t, TW);
  lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
  // empilhamento sequencial: chip e modelos somem quando nao ha o que dizer,
  // e o que sobra sobe junto em vez de deixar buraco.
  int y = 4;
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, TX, y);
  y += 30;

  lv_obj_t *chip = mkchip(box, 0, 0);
  set_chip(chip, severity_label(g_incSev), sevCol);
  lv_obj_update_layout(chip);
  lv_obj_align(chip, LV_ALIGN_TOP_LEFT, TX + (TW - lv_obj_get_width(chip)) / 2, y);
  y += 32;

  char mods[80];
  if (affected_models(mods, sizeof(mods))) {
    lv_obj_t *ml = mklabel(box, mods, &lv_font_montserrat_16, C_TEXT);
    lv_obj_set_width(ml, TW);
    lv_label_set_long_mode(ml, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(ml, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ml, LV_ALIGN_TOP_LEFT, TX, y);
    y += 24;
  }

  lv_obj_t *m = mklabel(box, TRS("veja status.claude.com", "see status.claude.com"),
                        &lv_font_montserrat_12, C_MUTED);
  lv_obj_set_width(m, TW);
  lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(m, LV_ALIGN_TOP_LEFT, TX, y);

  lv_obj_t *ok = mkbtn(box, "OK", &lv_font_montserrat_16, C_SURFACE2, C_TEXT);
  lv_obj_set_size(ok, 140, 44);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_LEFT, TX + (TW - 140) / 2, 0);
  lv_obj_add_event_cb(ok, incident_dismiss_cb, LV_EVENT_CLICKED, NULL);
}

// Abre/fecha o dialog conforme a ultima leitura do status.claude.com.
static void incident_sync() {
  if (g_state != ST_MAIN) return;
  bool any = g_status.ok &&
             !(g_status.haikuUp && g_status.sonnetUp && g_status.opusUp && g_status.fableUp);
  if (!any) { g_incAck = false; g_incSev = g_incMask = 0xFF; incident_close(); return; }

  uint8_t sev = g_status.severity, mask = affected_mask();
  if (g_incDlg && (sev != g_incSev || mask != g_incMask)) incident_close();   // redesenha
  else if (g_incAck && g_incSev != 0xFF && sev > g_incSev) g_incAck = false;  // escalou
  if (!g_incAck && !g_incDlg) show_incident();
}

// Preenche todos os valores vindos do fetch (sem rebuild de tela).
static void refresh_ui_values() {
  if (g_state != ST_MAIN || !g_ui.tv) return;
  update_tok_row();          // as colunas do tile 0 são atualizadas em dash_tick()

  // Modelos: chips de sonda
  for (int i = 0; i < NMODELS; i++) {
    if (!g_ui.mChip[i]) continue;
    char txt[16]; uint32_t col;
    model_chip(i, txt, sizeof(txt), &col);
    set_chip(g_ui.mChip[i], txt, col);
    lv_obj_update_layout(g_ui.mChip[i]);
    static const int CENTERS[NMODELS] = {60, 180, 300, 420};
    lv_obj_set_x(g_ui.mChip[i], CENTERS[i] - lv_obj_get_width(g_ui.mChip[i]) / 2);
  }

  incident_sync();

  trend_redraw();
  heat_redraw();
  dash_tick();
}

// Conta salva cuja última tentativa falhou (ou que não decifrou).
static bool slot_failed(int s) {
  if (!g_accts.used[s]) return false;
  if (!acct[s].keyOk) return true;
  FetchClass c = acct[s].lastClass;
  return c != FC_NONE && c != FC_OK;
}

// Status do cabeçalho (fase 2, até sair na fase 3): "ok" ou "K com falha".
static void set_hdr_status() {
  if (!g_hdrStatus) return;
  int k = 0;
  for (int s = 0; s < ACCT_MAX; s++) if (slot_failed(s)) k++;
  char buf[40];
  if (k == 0) strlcpy(buf, "ok", sizeof(buf));
  else snprintf(buf, sizeof(buf), TRS("%d com falha", "%d failing"), k);
  lv_label_set_text(g_hdrStatus, buf);
  lv_obj_set_style_text_color(g_hdrStatus, lv_color_hex(k ? C_BAD : C_MUTED), 0);
}
// Botão de refresh: só pede; o ciclo roda na net_task.
static void refresh_cb(lv_event_t *e) { (void)e; scheduler_request_refresh(); }

static void ui_main() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);

  // Header: Clawd + logotipo (duplo toque em QUALQUER um = demo das animacoes),
  // botao de ATUALIZAR visivel no centro, engrenagem grande a direita.
  lv_obj_t *hIcon = lv_image_create(scr);
  lv_image_set_src(hIcon, &img_clawd_sm);
  lv_obj_set_pos(hIcon, 14, 8);
  lv_obj_t *hWord = lv_image_create(scr);
  lv_image_set_src(hWord, &img_wordmark);
  lv_obj_set_pos(hWord, 66, 8);

  lv_obj_t *logoSpot = lv_obj_create(scr);     // hotspot icone+nome (so demo)
  lv_obj_set_pos(logoSpot, 6, 2); lv_obj_set_size(logoSpot, 128, 40);
  lv_obj_set_style_bg_opa(logoSpot, 0, 0);
  lv_obj_set_style_border_width(logoSpot, 0, 0);
  lv_obj_clear_flag(logoSpot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(logoSpot, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(logoSpot, [](lv_event_t *e) {
    (void)e;
    static uint32_t lastClick = 0;             // duplo clique manual (9.2 nao tem nativo)
    static int di = 0;
    uint32_t now = millis();
    if (now - lastClick < 450) {
      static const int T[4] = {25, 50, 70, 100};
      show_moment(g_accts.active, (di / 4) % 2, T[di % 4]);
      di++;
      lastClick = 0;
    } else {
      lastClick = now;
    }
  }, LV_EVENT_CLICKED, NULL);

  // botao de atualizar no centro do header (acao explicita; a busca e bloqueante)
  lv_obj_t *ref = mkbtn(scr, LV_SYMBOL_REFRESH, &lv_font_montserrat_20, C_SURFACE2, C_ACCENT);
  lv_obj_set_size(ref, 56, 40);
  lv_obj_set_ext_click_area(ref, 10);
  lv_obj_align(ref, LV_ALIGN_TOP_MID, 0, 2);
  lv_obj_add_event_cb(ref, refresh_cb, LV_EVENT_CLICKED, NULL);

  g_hdrStatus = mklabel(scr, "", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(g_hdrStatus, LV_ALIGN_TOP_RIGHT, -92, 16);

  lv_obj_t *gear = mkbtn(scr, LV_SYMBOL_SETTINGS, &lv_font_montserrat_22, C_SURFACE2, C_TEXT);
  lv_obj_set_size(gear, 78, 40);
  lv_obj_set_ext_click_area(gear, 16);
  lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -6, 2);
  lv_obj_add_event_cb(gear, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_SETTINGS);

  // Telas (swipe horizontal)
  g_ui.tv = lv_tileview_create(scr);
  lv_obj_set_pos(g_ui.tv, 0, 46);
  lv_obj_set_size(g_ui.tv, 480, 250);
  lv_obj_set_style_bg_opa(g_ui.tv, 0, 0);
  lv_obj_set_style_border_width(g_ui.tv, 0, 0);
  lv_obj_set_scrollbar_mode(g_ui.tv, LV_SCROLLBAR_MODE_OFF);
  for (int i = 0; i < NTILES; i++) {
    g_ui.tile[i] = lv_tileview_add_tile(g_ui.tv, i, 0, LV_DIR_HOR);
    tile_setup(g_ui.tile[i]);
  }
  build_tile_agora(g_ui.tile[0]);
  build_tile_models(g_ui.tile[1]);
  build_tile_trend(g_ui.tile[2]);
  build_tile_heat(g_ui.tile[3]);
  lv_obj_add_event_cb(g_ui.tv, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

  // Dots (objetos; o ativo vira pílula)
  for (int i = 0; i < NTILES; i++) {
    g_ui.dots[i] = lv_obj_create(scr);
    lv_obj_set_size(g_ui.dots[i], 8, 8);
    lv_obj_set_style_radius(g_ui.dots[i], 4, 0);
    lv_obj_set_style_bg_color(g_ui.dots[i], lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(g_ui.dots[i], 0, 0);
    lv_obj_clear_flag(g_ui.dots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(g_ui.dots[i], LV_ALIGN_BOTTOM_MID, (int)((i - (NTILES - 1) / 2.0f) * 18), -4);
  }

  refresh_ui_values();
  on_tile_changed(NULL);
}

// ============================================================
// Tela: settings (lista rolável; linhas >=44px de toque)
// ============================================================
static bool g_wipeArmed = false;
static lv_obj_t *g_briLbl = nullptr, *g_wipeLbl = nullptr, *g_pollLbl = nullptr,
                *g_tzLbl = nullptr, *g_slideLbl = nullptr;
static const int POLL_OPTS[4] = {30, 60, 120, 300};
static const int TZ_OPTS[] = {-3, -4, -5, -6, -7, -8, -2, -1, 0, 1, 2, 3};
#define NTZ ((int)(sizeof(TZ_OPTS) / sizeof(TZ_OPTS[0])))

static int g_acctDelArmed = -1;

static void settings_action_cb(lv_event_t *e) {
  int act = (int)(intptr_t)lv_event_get_user_data(e);
  switch (act) {
    case 0: scheduler_request_refresh(); request_state(ST_MAIN); break;   // atualizar
    case 1: g_onboarding = false; request_state(ST_WIFI); break;
    case 2:                                            // trocar token
      g_tokenTargetSlot = g_accts.active;
      g_pendingLabel[0] = 0;
      request_state(ST_TOKEN);
      break;
    case 3:                                            // brilho
      g_briIdx = (g_briIdx + 1) % 3; g_prefs.putInt("bri", g_briIdx); apply_brightness();
      if (g_briLbl) {
        const char *n[3] = {TRS("baixo", "low"), TRS("medio", "medium"), TRS("alto", "high")};
        char m[40]; snprintf(m, sizeof(m), TRS(LV_SYMBOL_EYE_OPEN "  Brilho: %s",
                                               LV_SYMBOL_EYE_OPEN "  Brightness: %s"), n[g_briIdx]);
        lv_label_set_text(g_briLbl, m);
      }
      break;
    case 4:                                            // apagar tudo (2 toques)
      if (!g_wipeArmed) {
        g_wipeArmed = true;
        if (g_wipeLbl) lv_label_set_text(g_wipeLbl, TRS(LV_SYMBOL_TRASH "  Toque de novo p/ confirmar",
                                                        LV_SYMBOL_TRASH "  Tap again to confirm"));
      } else {
        g_wipeArmed = false;
        factory_reset();
        request_state(ST_WIFI);
      }
      break;
    case 5: request_state(ST_MAIN); break;             // voltar
    case 6: {                                          // intervalo de atualização
      int idx = 0;
      for (int i = 0; i < 4; i++) if (POLL_OPTS[i] == g_pollSec) idx = i;
      g_pollSec = POLL_OPTS[(idx + 1) % 4];
      g_prefs.putInt("poll", g_pollSec);
      if (g_pollLbl) {
        char m[40];
        if (g_pollSec < 60) snprintf(m, sizeof(m), TRS(LV_SYMBOL_LOOP "  Atualizar: %ds",
                                                       LV_SYMBOL_LOOP "  Refresh: %ds"), g_pollSec);
        else snprintf(m, sizeof(m), TRS(LV_SYMBOL_LOOP "  Atualizar: %dmin",
                                        LV_SYMBOL_LOOP "  Refresh: %dmin"), g_pollSec / 60);
        lv_label_set_text(g_pollLbl, m);
      }
      break;
    }
    case 7: {                                          // fuso horário (GMT)
      int idx = 0;
      for (int i = 0; i < NTZ; i++) if (TZ_OPTS[i] == g_tzOffset) idx = i;
      g_tzOffset = TZ_OPTS[(idx + 1) % NTZ];
      g_prefs.putInt("tz", g_tzOffset);
      scheduler_apply_tz();
      if (g_tzLbl) {
        char m[40];
        snprintf(m, sizeof(m), TRS(LV_SYMBOL_GPS "  Fuso: GMT%+d",
                                   LV_SYMBOL_GPS "  Timezone: GMT%+d"), g_tzOffset);
        lv_label_set_text(g_tzLbl, m);
      }
      break;
    }
    case 8: {                                          // slideshow: off -> 5 -> 10 -> 15 -> 30 -> off
      static const int SL[5] = {0, 5, 10, 15, 30};
      int idx = 0;
      for (int i = 0; i < 5; i++) if (SL[i] == g_slideSec) idx = i;
      g_slideSec = SL[(idx + 1) % 5];
      g_prefs.putInt("slide", g_slideSec);
      if (g_slideLbl) {
        char m[48];
        if (g_slideSec) snprintf(m, sizeof(m), LV_SYMBOL_PLAY "  Slideshow: %ds", g_slideSec);
        else            snprintf(m, sizeof(m), "%s", TRS(LV_SYMBOL_PLAY "  Slideshow: desligado",
                                                         LV_SYMBOL_PLAY "  Slideshow: off"));
        lv_label_set_text(g_slideLbl, m);
      }
      break;
    }
    case 9:                                            // idioma / language
      g_lang ^= 1;
      g_prefs.putInt("lang", g_lang);
      request_state(ST_SETTINGS);                      // redesenha tudo no novo idioma
      break;
    case 10: request_state(ST_ABOUT); break;           // sobre / about
    case 11: g_acctDelArmed = -1; request_state(ST_ACCOUNTS); break;
  }
}
static void add_setting_row(lv_obj_t *p, const char *txt, int act, uint32_t fg, lv_obj_t **out) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_size(b, 444, 44);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = mklabel(b, txt, &lv_font_montserrat_16, fg);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_add_event_cb(b, settings_action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)act);
  if (out) *out = l;
}
static void ui_settings() {
  lv_obj_t *scr = lv_screen_active();
  g_wipeArmed = false;
  lv_obj_t *title = mklabel(scr, TRS("Ajustes", "Settings"), &lv_font_montserrat_20, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                       &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
  lv_obj_set_size(bk, 100, 32);
  lv_obj_set_ext_click_area(bk, 6);
  lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
  lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_MAIN);

  // lista rolável
  lv_obj_t *lst = lv_obj_create(scr);
  lv_obj_set_pos(lst, 8, 44);
  lv_obj_set_size(lst, 464, 268);
  lv_obj_set_style_bg_opa(lst, 0, 0);
  lv_obj_set_style_border_width(lst, 0, 0);
  lv_obj_set_style_pad_all(lst, 0, 0);
  lv_obj_set_style_pad_row(lst, 8, 0);
  lv_obj_set_flex_flow(lst, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(lst, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(lst, LV_SCROLLBAR_MODE_AUTO);

  const char *n[3] = {TRS("baixo", "low"), TRS("medio", "medium"), TRS("alto", "high")};
  char bri[40]; snprintf(bri, sizeof(bri), TRS(LV_SYMBOL_EYE_OPEN "  Brilho: %s",
                                               LV_SYMBOL_EYE_OPEN "  Brightness: %s"), n[g_briIdx]);
  char pollTxt[40];
  if (g_pollSec < 60) snprintf(pollTxt, sizeof(pollTxt), TRS(LV_SYMBOL_LOOP "  Atualizar: %ds",
                                                             LV_SYMBOL_LOOP "  Refresh: %ds"), g_pollSec);
  else                snprintf(pollTxt, sizeof(pollTxt), TRS(LV_SYMBOL_LOOP "  Atualizar: %dmin",
                                                             LV_SYMBOL_LOOP "  Refresh: %dmin"), g_pollSec / 60);
  char tzTxt[40]; snprintf(tzTxt, sizeof(tzTxt), TRS(LV_SYMBOL_GPS "  Fuso: GMT%+d",
                                                     LV_SYMBOL_GPS "  Timezone: GMT%+d"), g_tzOffset);
  char slideTxt[48];
  if (g_slideSec) snprintf(slideTxt, sizeof(slideTxt), LV_SYMBOL_PLAY "  Slideshow: %ds", g_slideSec);
  else            snprintf(slideTxt, sizeof(slideTxt), "%s", TRS(LV_SYMBOL_PLAY "  Slideshow: desligado",
                                                                 LV_SYMBOL_PLAY "  Slideshow: off"));

  add_setting_row(lst, TRS(LV_SYMBOL_REFRESH "  Atualizar agora",
                           LV_SYMBOL_REFRESH "  Refresh now"),   0, C_TEXT, nullptr);
  add_setting_row(lst, pollTxt,                                  6, C_TEXT, &g_pollLbl);
  add_setting_row(lst, slideTxt,                                 8, C_TEXT, &g_slideLbl);
  add_setting_row(lst, TRS(LV_SYMBOL_LIST "  Idioma: Portugues",
                           LV_SYMBOL_LIST "  Language: English"), 9, C_TEXT, nullptr);
  add_setting_row(lst, tzTxt,                                    7, C_TEXT, &g_tzLbl);
  add_setting_row(lst, bri,                                      3, C_TEXT, &g_briLbl);
  add_setting_row(lst, TRS(LV_SYMBOL_WIFI "  Configurar WiFi",
                           LV_SYMBOL_WIFI "  Configure WiFi"),   1, C_TEXT, nullptr);
  char acctTxt[64];
  snprintf(acctTxt, sizeof(acctTxt), TRS(LV_SYMBOL_DIRECTORY "  Contas: %s (%d/%d)",
                                         LV_SYMBOL_DIRECTORY "  Accounts: %s (%d/%d)"),
           g_accts.label[g_accts.active], accountCount(g_accts), ACCT_MAX);
  add_setting_row(lst, acctTxt,                                 11, C_TEXT, nullptr);
  add_setting_row(lst, TRS(LV_SYMBOL_KEYBOARD "  Trocar token",
                           LV_SYMBOL_KEYBOARD "  Change token"), 2, C_TEXT, nullptr);
  add_setting_row(lst, TRS(LV_SYMBOL_FILE "  Sobre",
                           LV_SYMBOL_FILE "  About"),           10, C_TEXT, nullptr);
  add_setting_row(lst, TRS(LV_SYMBOL_TRASH "  Apagar tudo",
                           LV_SYMBOL_TRASH "  Erase everything"), 4, C_BAD, &g_wipeLbl);
}

// Trocar o foco não decifra nada (item 23): todas as contas já estão abertas.
static void acct_switch_cb(lv_event_t *e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  g_acctDelArmed = -1;
  if (slot == g_accts.active) return;
  accountSetActive(g_prefs, g_accts, slot);
  Serial.printf("[ACCT] foco -> slot %d (%s)\n", slot, g_accts.label[slot]);
  request_state(ST_MAIN);
}
// Remover (item 23): gen++ e token zerado; a fila não é esvaziada e a task não
// é esperada — um resultado atrasado deste slot é descartado pelo gen.
static void acct_del_cb(lv_event_t *e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  if (accountCount(g_accts) <= 1) return;
  if (g_acctDelArmed != slot) {
    g_acctDelArmed = slot;
    request_state(ST_ACCOUNTS);
    return;
  }
  g_acctDelArmed = -1;
  accountRemove(g_prefs, g_accts, slot);
  acct_bump_gen(slot);
  token_store_clear(slot, (uint16_t)acct[slot].gen);
  char pth[16]; hist_path(slot, pth, sizeof(pth));
  LittleFS.remove(pth);
  acct_clear(slot);
  Serial.printf("[ACCT] slot %d removido (gen %u)\n", slot, (unsigned)acct[slot].gen);
  request_state(ST_ACCOUNTS);
}
static void acct_add_cb(lv_event_t *e) {
  (void)e;
  int slot = accountFirstFree(g_accts);
  if (slot < 0) return;
  g_tokenTargetSlot = slot;
  g_pendingLabel[0] = 0;
  request_state(ST_TOKEN);
}
static int g_renameSlot = -1;
static lv_obj_t *g_nameTa = nullptr;

static void acct_edit_cb(lv_event_t *e) {
  g_renameSlot = (int)(intptr_t)lv_event_get_user_data(e);
  g_acctDelArmed = -1;
  request_state(ST_ACCT_NAME);
}
static void acct_name_kb_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    accountSetLabel(g_prefs, g_accts, g_renameSlot, lv_textarea_get_text(g_nameTa));
    request_state(ST_ACCOUNTS);
  } else if (code == LV_EVENT_CANCEL) {
    request_state(ST_ACCOUNTS);
  }
}

static void ui_account_name() {
  if (g_renameSlot < 0 || g_renameSlot >= ACCT_MAX || !g_accts.used[g_renameSlot]) {
    request_state(ST_ACCOUNTS);
    return;
  }
  lv_obj_t *scr = lv_screen_active();

  lv_obj_t *title = mklabel(scr, TRS("Renomear conta", "Rename account"),
                            &lv_font_montserrat_20, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                       &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
  lv_obj_set_size(bk, 100, 32);
  lv_obj_set_ext_click_area(bk, 6);
  lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
  lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_ACCOUNTS);

  g_nameTa = lv_textarea_create(scr);
  lv_textarea_set_one_line(g_nameTa, true);
  lv_textarea_set_max_length(g_nameTa, ACCT_LBL_MAX - 1);
  lv_textarea_set_text(g_nameTa, g_accts.label[g_renameSlot]);
  lv_textarea_set_placeholder_text(g_nameTa, TRS("rotulo (ex.: Pessoal, Trabalho)",
                                                 "label (e.g. Personal, Work)"));
  lv_obj_set_size(g_nameTa, 452, 44);
  lv_obj_align(g_nameTa, LV_ALIGN_TOP_MID, 0, 52);

  lv_obj_t *kb = lv_keyboard_create(scr);
  lv_keyboard_set_textarea(kb, g_nameTa);
  lv_obj_add_event_cb(kb, acct_name_kb_cb, LV_EVENT_ALL, NULL);
}

static void ui_accounts() {
  lv_obj_t *scr = lv_screen_active();

  lv_obj_t *title = mklabel(scr, TRS("Contas", "Accounts"), &lv_font_montserrat_20, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                       &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
  lv_obj_set_size(bk, 100, 32);
  lv_obj_set_ext_click_area(bk, 6);
  lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
  lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_SETTINGS);

  lv_obj_t *lst = lv_obj_create(scr);
  lv_obj_set_pos(lst, 8, 44);
  lv_obj_set_size(lst, 464, 240);
  lv_obj_set_style_bg_opa(lst, 0, 0);
  lv_obj_set_style_border_width(lst, 0, 0);
  lv_obj_set_style_pad_all(lst, 0, 0);
  lv_obj_set_style_pad_row(lst, 8, 0);
  lv_obj_set_flex_flow(lst, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(lst, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(lst, LV_SCROLLBAR_MODE_AUTO);

  bool canDelete = accountCount(g_accts) > 1;
  for (int i = 0; i < ACCT_MAX; i++) {
    if (!g_accts.used[i]) continue;
    bool active = (i == g_accts.active);

    lv_obj_t *row = lv_obj_create(lst);
    lv_obj_set_size(row, 444, 44);
    no_box(row);

    lv_obj_t *b = lv_button_create(row);
    lv_obj_set_size(b, canDelete ? 324 : 384, 44);
    lv_obj_set_pos(b, 0, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    char txt[40];
    snprintf(txt, sizeof(txt), "%s%s", g_accts.label[i],
             active ? TRS("  \xE2\x80\xA2  ativa", "  \xE2\x80\xA2  active") : "");
    lv_obj_t *l = mklabel(b, txt, &lv_font_montserrat_16, active ? C_ACCENT : C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(b, acct_switch_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *ed = lv_button_create(row);
    lv_obj_set_size(ed, 52, 44);
    lv_obj_set_pos(ed, canDelete ? 332 : 392, 0);
    lv_obj_set_style_bg_color(ed, lv_color_hex(C_SURFACE2), 0);
    lv_obj_set_style_radius(ed, 12, 0);
    lv_obj_set_style_shadow_width(ed, 0, 0);
    lv_obj_center(mklabel(ed, LV_SYMBOL_EDIT, &lv_font_montserrat_16, C_MUTED));
    lv_obj_add_event_cb(ed, acct_edit_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    if (canDelete) {
      bool armed = (g_acctDelArmed == i);
      lv_obj_t *d = lv_button_create(row);
      lv_obj_set_size(d, 52, 44);
      lv_obj_set_pos(d, 392, 0);
      lv_obj_set_style_bg_color(d, lv_color_hex(armed ? C_BAD : C_SURFACE2), 0);
      lv_obj_set_style_radius(d, 12, 0);
      lv_obj_set_style_shadow_width(d, 0, 0);
      lv_obj_center(mklabel(d, armed ? LV_SYMBOL_WARNING : LV_SYMBOL_TRASH,
                            &lv_font_montserrat_16, armed ? C_BG : C_MUTED));
      lv_obj_add_event_cb(d, acct_del_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }
  }

  if (accountFirstFree(g_accts) >= 0) {
    lv_obj_t *add = lv_button_create(lst);
    lv_obj_set_size(add, 444, 44);
    lv_obj_set_style_bg_color(add, lv_color_hex(C_SURFACE2), 0);
    lv_obj_set_style_radius(add, 12, 0);
    lv_obj_set_style_shadow_width(add, 0, 0);
    lv_obj_t *al = mklabel(add, TRS(LV_SYMBOL_PLUS "  Adicionar conta",
                                    LV_SYMBOL_PLUS "  Add account"),
                           &lv_font_montserrat_16, C_ACCENT);
    lv_obj_align(al, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(add, acct_add_cb, LV_EVENT_CLICKED, NULL);
  }

  lv_obj_t *hint = mklabel(scr, TRS("Todas as contas sao consultadas; a ativa tambem sonda os modelos.",
                                    "All accounts are polled; the active one also probes the models."),
                           &lv_font_montserrat_12, C_FAINT);
  lv_obj_set_width(hint, 452);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 14, -8);
}

// ============================================================
// Tela: sobre / about
// ============================================================
static void ui_about() {
  lv_obj_t *scr = lv_screen_active();

  lv_obj_t *bk = mkbtn(scr, TRS(LV_SYMBOL_LEFT " Voltar", LV_SYMBOL_LEFT " Back"),
                       &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
  lv_obj_set_size(bk, 100, 32);
  lv_obj_set_ext_click_area(bk, 6);
  lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 6);
  lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_SETTINGS);

  lv_obj_t *mark = build_claude_mark(scr);
  lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *t = mklabel(scr, "Claude Usage Stick", &lv_font_montserrat_22, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 94);

  char v[64];
  snprintf(v, sizeof(v), "v" FW_VERSION " \xE2\x80\xA2 ESP32-S3 \xE2\x80\xA2 LVGL 9.2");
  lv_obj_t *ver = mklabel(scr, v, &lv_font_montserrat_12, C_FAINT);
  lv_obj_align(ver, LV_ALIGN_TOP_MID, 0, 122);

  lv_obj_t *d = mklabel(scr, TRS("Medidor de uso do Claude Code em tempo real: "
                                 "janelas de 5h e semanal direto da API da Anthropic.",
                                 "Real-time Claude Code usage meter: "
                                 "5-hour and weekly windows straight from the Anthropic API."),
                        &lv_font_montserrat_14, C_MUTED);
  lv_obj_set_width(d, 420);
  lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);
  lv_obj_align(d, LV_ALIGN_TOP_MID, 0, 146);

  lv_obj_t *h = mklabel(scr, TRS("Tela: Guition JC4832W535 \xE2\x80\xA2 IPS 3.5\" 480x320 touch (AXS15231B)",
                                 "Display: Guition JC4832W535 \xE2\x80\xA2 3.5\" IPS 480x320 touch (AXS15231B)"),
                        &lv_font_montserrat_12, C_FAINT);
  lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 200);

  lv_obj_t *devCap = mklabel(scr, TRS("Desenvolvido por", "Developed by"), &lv_font_montserrat_12, C_FAINT);
  lv_obj_align(devCap, LV_ALIGN_TOP_MID, 0, 234);
  lv_obj_t *dev = mklabel(scr, "Benevid Felix", &lv_font_montserrat_18, C_TEXT);
  lv_obj_align(dev, LV_ALIGN_TOP_MID, 0, 254);
  lv_obj_t *mail = mklabel(scr, "benevid@gmail.com", &lv_font_montserrat_14, C_ACCENT);
  lv_obj_align(mail, LV_ALIGN_TOP_MID, 0, 282);
}

// ============================================================
// Navegação genérica
// ============================================================
static void nav_cb(lv_event_t *e) {
  State s = (State)(intptr_t)lv_event_get_user_data(e);
  request_state(s);
}

// ============================================================
// Render do estado atual
// ============================================================
static void render_state() {
  State prev = g_state;
  g_state = g_pending;
  // O servidor web não cai mais a cada tela (item 24): só o estado da
  // validação do token fecha junto com a tela de token.
  if (prev == ST_TOKEN && g_state != ST_TOKEN) web_token_reset();
  // Tela de WiFi da sessão: sem jobs enquanto ela mexe no rádio.
  if (prev == ST_WIFI && g_state != ST_WIFI) scheduler_resume();
  if (g_state == ST_WIFI && session_unlocked()) scheduler_pause();
  moment_close();                             // overlay vive em lv_layer_top
  lv_obj_clean(lv_layer_top());
  g_incDlg = nullptr;                         // dialog de incidente idem
  // invalida ponteiros vivos antes de destruir a tela antiga
  memset(&g_ui, 0, sizeof(g_ui));
  g_mascN = 0;
  g_pinDots = g_pinMsg = nullptr;
  g_tokMsg = nullptr;
  g_nameTa = nullptr;
  g_hdrStatus = nullptr;
  g_briLbl = g_wipeLbl = g_pollLbl = g_tzLbl = g_slideLbl = nullptr;

  lv_obj_clean(lv_screen_active());
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  switch (g_state) {
    case ST_PIN:
    case ST_SETUP_PIN: ui_pin(); break;
    case ST_WIFI:      ui_wifi(); break;
    case ST_TOKEN:     ui_token(); break;
    case ST_MAIN:      ui_main(); break;
    case ST_SETTINGS:  ui_settings(); break;
    case ST_ACCOUNTS:  ui_accounts(); break;
    case ST_ACCT_NAME: ui_account_name(); break;
    case ST_ABOUT:     ui_about(); break;
    default: break;
  }
  if (g_state == ST_MAIN) moment_flush();     // momento guardado durante outra tela
}

// ============================================================
// setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Claude Usage Stick (touch) ===");

  // Display
  Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
  Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
  gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);
  if (!gfx->begin(QSPI_FREQ)) { Serial.println("FATAL display"); while (1) delay(1000); }
  gfx->fillScreen(0x0000); gfx->flush();
  canvas_fb = gfx->getFramebuffer();

  // Backlight via PWM (brilho ajustável)
  ledcAttach(TFT_BL, 5000, 8);
  touch_dev.begin();

  // LVGL
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) { Serial.println("FATAL PSRAM"); while (1) delay(1000); }
  if (!acct_init()) { Serial.println("FATAL PSRAM (acct)"); while (1) delay(1000); }
  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, disp_flush_cb);
  lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_FULL);
#ifdef THEP_DEV
  lv_display_add_event_cb(disp, frame_evt_cb, LV_EVENT_REFR_START, NULL);
  lv_display_add_event_cb(disp, frame_evt_cb, LV_EVENT_REFR_READY, NULL);
  web_on_health_ui(health_ui_cb);
  Serial.println("[BOOT] build THEP_DEV: GET /health ativo");
#endif
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);

  load_persisted();
  apply_brightness();
  Serial.printf("[BOOT] fw %s reset_reason=%d\n", FW_VERSION, (int)esp_reset_reason());
  if (g_prefs.isKey("lastReset")) {             // motivo gravado por poller_check_stuck()
    String lr = g_prefs.getString("lastReset", "");
    Serial.printf("[BOOT] lastReset=%s\n", lr.c_str());
    g_prefs.remove("lastReset");
  }

  if (!LittleFS.begin(true)) Serial.println("LittleFS: falhou");
  else hist_migrate_legacy();       // os históricos carregam no desbloqueio (item 23)

  web_on_token_msg(web_token_msg_cb);
  web_on_tok_stats(web_tok_stats_cb);

  token_store_init();               // antes da net_task existir
  g_wifi.begin();
  poller_start();                   // VALIDATE já no onboarding (item 6)
  PollerHooks hooks = {incident_sync, on_validate_result, ensure_mdns};
  poller_set_hooks(hooks);
  scheduler_set_moment_hook(moment_hook);

  if (has_accounts()) {
    // Tenta WiFi cedo (em paralelo o usuário digita o PIN)
    g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS);
    request_state(ST_PIN);
  } else {
    g_onboarding = true;
    // Se já há WiFi salvo (reboot no meio do onboarding), pula direto p/ o token
    request_state(g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS) ? ST_TOKEN : ST_WIFI);
  }
}

// Chegou resultado da net_task com a tela inicial aberta: valores no lugar e,
// se algum mascote mudou de humor, só o tile de modelos é refeito (um rebuild
// da tela inteira fecharia o overlay de momento e voltaria ao tile 0).
static void ui_apply_results() {
  bool moodChanged = false;
  for (int i = 0; i < g_mascN; i++) if (g_masc[i].mood != model_mood(i)) moodChanged = true;
  if (moodChanged && g_ui.tile[1]) {
    lv_obj_clean(g_ui.tile[1]);
    g_mascN = 0;
    memset(g_ui.mChip, 0, sizeof(g_ui.mChip));
    build_tile_models(g_ui.tile[1]);
  }
  refresh_ui_values();
}

void loop() {
  // Ordem do item 2 (fase 2), em todos os estados, antes e depois do desbloqueio
  lv_timer_handler();
  web_api_poll();
  bool changed = poller_apply_results();
  scheduler_tick();
  poller_check_stuck();
  if (changed) g_uiDirty = true;

  if (g_dirty) {
    g_dirty = false;
    render_state();
  }
  if (g_uiDirty) {
    g_uiDirty = false;
    if (g_state == ST_MAIN) ui_apply_results();
  }

  // Tela de WiFi da sessão esperando a net_task terminar o job em andamento.
  if (g_state == ST_WIFI && g_wifiWaiting && poller_idle()) request_state(ST_WIFI);

  // Contagem do bloqueio por PIN errado. 250ms para o segundo virar sem atraso
  // visivel; o tick so reescreve o label quando o valor muda. So ST_PIN: o
  // setup de PIN nao tem tentativa nem bloqueio.
  if (g_state == ST_PIN) {
    static uint32_t lastLock = 0;
    if (millis() - lastLock > 250) { lastLock = millis(); pin_lock_tick(); }
  }

  // Atualização viva: contadores (1s), mascotes,
  // slideshow (5s, pausa 10s após qualquer toque)
  if (g_state == ST_MAIN) {
    uint32_t now = millis();
    static uint32_t lastTick = 0, lastBob = 0, blinkAt = 0;
    static bool blinkClosed = false;
    if (now - lastTick > 1000) { lastTick = now; dash_tick(); update_tok_row(); }
    if (now - lastBob > 80) {                       // animação por humor
      lastBob = now;
      float ph = now / 600.0f;
      for (int i = 0; i < g_mascN; i++) {
        if (!g_masc[i].cont) continue;
        if (g_masc[i].mood == 1)                    // ok: bob alegre
          lv_obj_set_y(g_masc[i].cont, g_masc[i].baseY + (int)(2.0f * sinf(ph + i * 0.9f) - 1.0f));
        else if (g_masc[i].mood == 2) {             // limitado: bob curto + suor
          lv_obj_set_y(g_masc[i].cont, g_masc[i].baseY + (int)(1.2f * sinf(ph * 0.6f + i)));
          if (g_masc[i].drop) {
            uint32_t cyc = (now + i * 300) % 900;
            lv_obj_set_y(g_masc[i].drop, 24 + (int)(cyc * 22 / 900));
            lv_obj_set_style_bg_opa(g_masc[i].drop, (lv_opa_t)(255 - cyc * 190 / 900), 0);
          }
        }
      }
    }
    uint32_t bp = blinkClosed ? 150 : 3000;
    if (now - blinkAt > bp) {                        // piscar (só quem está ok)
      blinkAt = now; blinkClosed = !blinkClosed;
      for (int i = 0; i < g_mascN; i++) {
        if (g_masc[i].mood != 1) continue;
        for (int k = 0; k < 2; k++) {
          if (!g_masc[i].lid[k]) continue;
          if (blinkClosed) lv_obj_clear_flag(g_masc[i].lid[k], LV_OBJ_FLAG_HIDDEN);
          else             lv_obj_add_flag(g_masc[i].lid[k], LV_OBJ_FLAG_HIDDEN);
        }
      }
    }
    if (g_slideSec > 0 && g_ui.tv && !g_mo.scrim && !g_incDlg &&
        now - g_lastTouchMs > 10000 && now - g_lastSlideMs > (uint32_t)g_slideSec * 1000) {
      g_lastSlideMs = now;
      int next = (g_curTile + 1) % NTILES;
      lv_tileview_set_tile_by_index(g_ui.tv, next, 0, LV_ANIM_ON);
    }

    // Momento de limiar aberto (o agendador decide quando abrir: moment_flush)
    if (g_mo.scrim) moment_tick();
  }

  delay(5);
}
