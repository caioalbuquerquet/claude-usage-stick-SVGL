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
 * Sem botao fisico: navegacao 100% touch. Depois do desbloqueio a tela e o
 * shell fixo (src/ui/shell) e o menu (src/ui/overlay_menu) troca o conteudo.
 * As telas ficam em src/ui/screen_* e os overlays (momento de limiar e
 * incidente) em src/ui/overlay_*; aqui ficam o hardware, o PIN e a sessao.
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
#include "app/app_state.h"
#include "ui/shell.h"
#include "ui/overlay_menu.h"
#include "ui/screens.h"
#include "ui/screen_splash.h"
#include "hw/frame_stats.h"
#include "logic/rot270.h"
#include "ui/i18n.h"
#include "ui/theme.h"
#include "ui/overlay_moment.h"
#include "ui/overlay_incident.h"
#include <mbedtls/platform_util.h>

// ---- Idioma (0 = portugues, 1 = english; Ajustes -> NVS "lang") ----
uint8_t g_lang = 0;                          // lido por web_api.cpp (TRS); TRS vem de ui/i18n.h

// ---- Hardware ----
Arduino_Canvas *gfx = nullptr;
static uint16_t *canvas_fb = nullptr;
AXS15231B_Touch touch_dev(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR, TOUCH_ROTATION);
WiFiManager g_wifi;
Preferences g_prefs;

// ---- Estado da aplicação: src/app/app_state (AppState, ContentId, intenções) ----

// ---- Dados: g_status e g_models[] ficam em src/model/account_state ----

// ---- Uso, tokens por sessao, limiares e historico por conta: acct[] (src/model/account_state.h).
// Fase 1: a tela mostra so a conta em foco, acct[g_accts.active]. ----

// ---- Token / segurança ----
// Fase 2 (item 23): os tokens decifrados ficam em src/net/token_store (sob
// mutex, lidos pela net_task). Aqui fica só a chave da sessão, derivada uma
// vez no desbloqueio e usada para cifrar/decifrar todos os slots; zerada no
// factory reset. Nada disso vai para o NVS.
AccountSlots g_accts;               // lida por web_api.cpp
bool g_onboarding = false;                   // primeiro setup em andamento (lido por screen_settings)
static uint8_t g_sessionKey[32] = {0};
static bool g_unlocked = false;
bool session_unlocked() { return g_unlocked; }
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

// ---- Brilho ----
static const uint8_t BRI_LEVELS[3] = {60, 160, 255};
int g_briIdx = 1;                            // lido por screen_settings

int g_pollSec = DEFAULT_POLL_SEC;         // intervalo de atualização (config, NVS); lido pelo agendador
int g_tzOffset = -3;                      // fuso GMT (horas), config NVS
int g_heatMode = 3;                       // 0=hoje 1=7d 2=30d 3=tudo (config, NVS; screen_heat)

// ---- Histórico / heatmap: src/model/history.{h,cpp} ----

// ============================================================
// Pipeline de display/touch (validado no bring-up)
// ============================================================
// Spike da fase 3 (item 34): o tempo de cada flush vai para hw/frame_stats,
// separado em rotação (rot) e envio (send). Flags em config.h:
//   DISP_ROT_FAST=1  rotação em blocos (src/logic/rot270) no lugar da pixel a pixel;
//   DISP_BANDS=1     PARTIAL + envio só do prefixo de linhas nativas tocado;
//   DISP_BANDS=2     idem, mas com a janela de linhas (RASET) de verdade: só
//                    experimento, para confirmar que o painel ignora o RASET.
#define DISP_NW SCREEN_HEIGHT   // largura nativa (320) = altura da paisagem
#if DISP_BANDS
// Modo de faixas. O LVGL entrega só as áreas invalidadas; cada uma é girada
// para o framebuffer do Canvas (a imagem inteira da tela, em PSRAM, que
// guarda o que não mudou). No fim do quadro vai ao painel um bloco de linhas
// nativas INTEIRAS (CASET sempre 0..319).
//
// Mapeamento (detalhe em src/logic/rot270.h): o pixel de paisagem (lx, ly)
// vai para a linha nativa r = 479 - lx, coluna ly. Uma área de paisagem
// [x1..x2] x [y1..y2] toca então as linhas nativas [479-x2 .. 479-x1],
// qualquer que seja o y; a borda direita da paisagem é a linha nativa 0. Ex.:
// o ponto do cabeçalho (x ~368..378) são as linhas nativas ~101..111.
//
// Por que um prefixo [0 .. r1] e não a faixa [r0 .. r1]: o PARTIAL anterior
// mandava exatamente a faixa (CASET 0..319 + RASET r0..r1) e o conteúdo saiu
// deslocado na horizontal. Na paisagem, deslocamento horizontal = linha
// nativa errada. A explicação mais provável: o AXS15231B em QSPI ignora o
// RASET (não o CASET) e o RAMWR recomeça na linha 0 (a faixa caiu na borda
// direita da paisagem). DISP_BANDS=2 repete aquele envio para confirmar na
// placa. É o que o driver da Espressif (esp_lcd_axs15231b) assume: ele nem
// manda RASET em QSPI: usa RAMWR (0x2C) quando y_start == 0 e RAMWRC (0x3C,
// continuar) nas faixas seguintes, que precisam vir em ordem. Então a única
// faixa endereçável é a que começa na linha 0: mandamos as linhas 0..r1, com
// r1 = 479 - (menor x1 do quadro). O RASET (0, r1) que o Arduino_GFX manda
// é inofensivo: começa em 0, valha ou não. O ganho depende de quão à direita
// está o que mudou (o ponto do cabeçalho: 112 de 480 linhas).
static Arduino_GFX *g_panel = nullptr;      // saída do Canvas (o AXS15231B)
static int16_t g_bandR0 = SCREEN_WIDTH, g_bandR1 = -1;   // linhas nativas tocadas no quadro
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t t0 = micros();
  const int w = lv_area_get_width(area);
  const int stride = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565) / 2;
#if DISP_ROT_FAST
  rot270_fast((const uint16_t *)px_map, stride, area->x1, area->y1, area->x2, area->y2,
              canvas_fb, SCREEN_WIDTH, DISP_NW);
#else
  rot270_naive((const uint16_t *)px_map, stride, area->x1, area->y1, area->x2, area->y2,
               canvas_fb, SCREEN_WIDTH, DISP_NW);
#endif
  int r0, r1;
  rot270_rows(area->x1, area->x2, SCREEN_WIDTH, &r0, &r1);
  if (r0 < g_bandR0) g_bandR0 = r0;
  if (r1 > g_bandR1) g_bandR1 = r1;
  uint32_t t1 = micros();
  frame_rotate_add(t1 - t0);
  if (lv_display_flush_is_last(disp) && g_bandR1 >= g_bandR0) {
#if DISP_BANDS == 2
    const int16_t y0 = g_bandR0;            // experimento: faixa exata via RASET
#else
    const int16_t y0 = 0;                   // prefixo: o RAMWR começa na linha 0
#endif
    const int16_t rows = g_bandR1 - y0 + 1;
    g_panel->draw16bitRGBBitmap(0, y0, canvas_fb + y0 * DISP_NW, DISP_NW, rows);
    g_bandR0 = SCREEN_WIDTH; g_bandR1 = -1;
    uint32_t t2 = micros();
    frame_send_add(t2 - t1, rows);
    t1 = t2;
  }
  frame_flush_add(t1 - t0);
  lv_display_flush_ready(disp);
}
#else
// Modo FULL (validado no bring-up): a tela inteira é girada e enviada.
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  (void)area;
  uint32_t t0 = micros();
  uint16_t *src = (uint16_t *)px_map;
#if DISP_ROT_FAST
  rot270_fast(src, SCREEN_WIDTH, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1,
              canvas_fb, SCREEN_WIDTH, DISP_NW);
#else
  for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {
    uint16_t *src_row = src + ly * SCREEN_WIDTH;
    for (int lx = 0; lx < SCREEN_WIDTH; lx++)
      canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];
  }
#endif
  uint32_t t1 = micros();
  gfx->flush();
  uint32_t t2 = micros();
  frame_rotate_add(t1 - t0);
  frame_send_add(t2 - t1, SCREEN_WIDTH);
  frame_flush_add(t2 - t0);
  lv_display_flush_ready(disp);
}
#endif
// Tempo de quadro (layout + render + flush), medido pelos eventos de refresh.
static void frame_evt_cb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_REFR_START) frame_refr_start();
  else frame_refr_end();
}
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t x, y;
  if (touch_dev.touched()) {
    touch_dev.readData(&x, &y);
    data->point.x = x; data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

#ifdef THEP_DEV
// Log do LVGL (item 42, só THEP_DEV): avisos e erros vão para a Serial.
static void lv_log_cb(lv_log_level_t level, const char *buf) {
  (void)level;
  Serial.print(buf);
}

// GET /health (item 6, só THEP_DEV): uso do pool do LVGL, lido aqui porque o
// loop() é o dono do LVGL. O tempo de quadro vem de hw/frame_stats.
static void health_ui_cb(WebHealthUi *o) {
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  o->lvMemUsedPct = m.used_pct;
}
#endif

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
  // A chave "slide" (slideshow da v2.3) não é mais lida nem apagada (G8).
  g_heatMode = g_prefs.getInt("heatm", 3);
  if (g_heatMode < 0 || g_heatMode > 3) g_heatMode = 3;
  g_lang = g_prefs.getInt("lang", 0) ? 1 : 0;
}
static void save_attempts() { g_prefs.putInt("pinatt", g_pinAttempts); }
void apply_brightness() { ledcWrite(TFT_BL, BRI_LEVELS[g_briIdx]); }

void factory_reset() {
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
// A tela (src/ui/screen_pin.cpp) só desenha; tentativas, bloqueio e chave da
// sessão ficam aqui.
static void pin_update_dots() { screen_pin_set_dots((int)strlen(g_pinEntry)); }

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
  if (!screen_pin_up()) return;
  char m[64];
  if (pin_locked()) {
    if (!g_lockPainted) {
      g_lockPainted = true;
      screen_pin_set_msg_bad(true);
    }
    int rem = (int)((g_lockoutUntil - millis() + 999) / 1000);
    if (rem == g_lockSecs) return;      // so reescreve quando o segundo vira
    g_lockSecs = rem;
    snprintf(m, sizeof(m), TRS("PIN errado (%d/%d). Aguarde %ds", "Wrong PIN (%d/%d). Wait %ds"),
             g_pinAttempts, MAX_PIN_ATTEMPTS, rem);
    screen_pin_set_msg(m);
  } else if (g_lockPainted) {
    g_lockPainted = false;
    g_lockSecs = -1;
    snprintf(m, sizeof(m), TRS("Pode tentar de novo (%d/%d)", "You can try again (%d/%d)"),
             g_pinAttempts, MAX_PIN_ATTEMPTS);
    screen_pin_set_msg(m);
    screen_pin_set_msg_bad(false);
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
  request_state(ST_SHELL);           // shell + /agora
}

static void pin_submit() {
  if (app_state() == ST_SETUP_PIN) {
    if (!g_pinConfirming) {
      strlcpy(g_pinFirst, g_pinEntry, sizeof(g_pinFirst));
      g_pinConfirming = true;
      g_pinEntry[0] = 0;
      pin_update_dots();
      screen_pin_set_msg(TRS("Confirme o PIN", "Confirm the PIN"));
      return;
    }
    // confirmando
    if (strcmp(g_pinFirst, g_pinEntry) != 0) {
      g_pinConfirming = false;
      g_pinFirst[0] = 0; g_pinEntry[0] = 0;
      pin_update_dots();
      screen_pin_set_msg(TRS("Não bateu. Defina de novo.", "Didn't match. Set it again."));
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
      screen_pin_set_msg(TRS("Falha ao cifrar. Tente de novo.", "Encryption failed. Try again."));
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

// Tecla do teclado do PIN (screen_pin repassa o texto do botão).
static void pin_key(const char *txt) {
  if (pin_locked()) return;                  // travado
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

// ST_PIN e ST_SETUP_PIN: tela de src/ui/screen_pin.cpp.
static void pin_screen_build() {
  const char *title = (app_state() == ST_SETUP_PIN)
    ? (g_pinConfirming ? TRS("Confirme o PIN", "Confirm the PIN") : TRS("Defina um PIN", "Set a PIN"))
    : TRS("Digite o PIN", "Enter the PIN");
  const char *sub = (app_state() == ST_SETUP_PIN)
    ? TRS("Você vai digitá-lo a cada boot.", "You'll type it on every boot.")
    : TRS("Necessário para desbloquear o token.", "Needed to unlock the token.");
  g_lockPainted = false;                       // label novo: estado do aviso zera
  g_lockSecs = -1;
  screen_pin_build(title, sub, pin_key);
  pin_update_dots();
  pin_lock_tick();      // se ha bloqueio pendente, ja nasce com a contagem
}

// ============================================================
// Token. A tela fica em src/ui/screen_token.cpp e o servidor web em
// src/app/web_api.{h,cpp}; o que ele muda na tela ou no PIN passa pelos
// callbacks abaixo.
// ============================================================
static void web_token_msg_cb(const char *m) { screen_token_msg(m); }
// Tokens do bridge: a linha fica no layout de 1 conta de /agora e em /janela.
static void web_tok_stats_cb() {
  if (app_state() != ST_SHELL) return;
  if (app_content() == ContentId::HOME) screen_home_refresh();
  else if (app_content() == ContentId::TREND) screen_trend_refresh();
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
    if (app_state() == ST_TOKEN) request_state(ST_SETUP_PIN);
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
  shell_show(ContentId::HOME);
  scheduler_request_refresh();
}

// ============================================================
// Registro no app_state (item 25): telas cheias antes do desbloqueio e
// conteúdos do shell, todos em src/ui/. O app_state chama ui_reset() antes
// de limpar.
// ============================================================
// /agora: o incidente e o momento guardado durante outra tela aparecem ao
// abrir e a cada resultado.
static void home_build(lv_obj_t *p) {
  screen_home_build(p);
  incident_sync();
  moment_flush();
}
static void home_refresh() {
  screen_home_refresh();
  incident_sync();
}

static void ui_reset(bool full) {
  screen_splash_reset();
  screen_home_reset();
  screen_models_reset();
  screen_trend_reset();
  screen_heat_reset();
  screen_settings_reset();
  screen_accounts_reset();
  screen_pin_reset();
  screen_wifi_reset();
  screen_token_reset();
  if (full) {
    moment_close();                           // overlay vive em lv_layer_top
    incident_forget();                        // o app_state limpa o lv_layer_top
  }
}

// Contagem do bloqueio por PIN errado. 250ms para o segundo virar sem atraso
// visivel; o tick so reescreve o label quando o valor muda. So ST_PIN: o
// setup de PIN nao tem tentativa nem bloqueio.
static void pin_screen_tick() {
  static uint32_t lastLock = 0;
  if (millis() - lastLock > 250) { lastLock = millis(); pin_lock_tick(); }
}

static void register_ui() {
  app_on_ui_reset(ui_reset);
  shell_set_overlay_getters(moment_obj, incident_obj);
  app_register_screen(ST_SPLASH,    {screen_splash_build, nullptr});
  app_register_screen(ST_PIN,       {pin_screen_build, pin_screen_tick});
  app_register_screen(ST_SETUP_PIN, {pin_screen_build, nullptr});
  app_register_screen(ST_WIFI,      {screen_wifi_full, nullptr});
  app_register_screen(ST_TOKEN,     {screen_token_full, nullptr});

  app_register_content(ContentId::HOME,         {home_build, home_refresh, screen_home_tick});
  app_register_content(ContentId::MODELS,       {screen_models_build, screen_models_refresh, nullptr});
  app_register_content(ContentId::TREND,        {screen_trend_build, screen_trend_refresh, nullptr});
  app_register_content(ContentId::HEAT,         {screen_heat_build, screen_heat_refresh, nullptr});
  app_register_content(ContentId::ACCOUNTS,     {screen_accounts_build, nullptr, nullptr});
  app_register_content(ContentId::ACCOUNT_NAME, {screen_account_name_build, nullptr, nullptr});
  app_register_content(ContentId::SETTINGS,     {screen_settings_build, nullptr, nullptr});
  app_register_content(ContentId::ABOUT,        {screen_about_build, nullptr, nullptr});
  app_register_content(ContentId::TOKEN,        {screen_token_build, nullptr, nullptr});
  app_register_content(ContentId::WIFI,         {screen_wifi_build, nullptr, screen_wifi_tick});
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
  theme_init();
#ifdef THEP_DEV
  lv_log_register_print_cb(lv_log_cb);
#endif
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  // Buffer de tela cheia nos dois modos: no de faixas cada área invalidada é
  // desenhada inteira de uma vez (sem fatiar e redesenhar por pedaço).
  uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
#if DISP_BANDS
  g_panel = g;
  const lv_display_render_mode_t renderMode = LV_DISPLAY_RENDER_MODE_PARTIAL;
#else
  const lv_display_render_mode_t renderMode = LV_DISPLAY_RENDER_MODE_FULL;
#endif
#if DISP_BANDS == 2
  frame_set_mode(DISP_ROT_FAST ? "bands-raset+rotfast" : "bands-raset");
#elif DISP_BANDS
  frame_set_mode(DISP_ROT_FAST ? "bands+rotfast" : "bands");
#else
  frame_set_mode(DISP_ROT_FAST ? "full+rotfast" : "full");
#endif
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) { Serial.println("FATAL PSRAM"); while (1) delay(1000); }
  if (!acct_init()) { Serial.println("FATAL PSRAM (acct)"); while (1) delay(1000); }
  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, disp_flush_cb);
  lv_display_set_buffers(disp, buf, NULL, bufSize, renderMode);
  lv_display_add_event_cb(disp, frame_evt_cb, LV_EVENT_REFR_START, NULL);
  lv_display_add_event_cb(disp, frame_evt_cb, LV_EVENT_REFR_READY, NULL);
  Serial.printf("[BOOT] display %s, buffer %u bytes\n", frame_mode(), (unsigned)bufSize);
#ifdef THEP_DEV
  web_on_health_ui(health_ui_cb);
  Serial.println("[BOOT] build THEP_DEV: GET /health ativo");
#endif
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);

  load_persisted();
  register_ui();
  // Abertura parada já no primeiro quadro (sem tela preta enquanto o boot
  // carrega LittleFS, NVS e o WiFi); a animação de 4 s começa no fim do setup().
  request_state(ST_SPLASH);
  app_state_tick(false);
  lv_refr_now(NULL);
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
  scheduler_set_moment_hook(show_moment);

  if (has_accounts()) {
    // WiFi em segundo plano: um RECONNECT na net_task (connectList, sem NVS)
    // percorre as redes salvas enquanto a abertura anima e o usuário digita o
    // PIN. Assim a animação começa já, sem esperar o autoConnect (até 8 s).
    // O resultado chega por poller_apply_results(); antes do desbloqueio o
    // agendador o ignora (não há ciclo aberto).
    WiFiManager::SavedNet nets[MAX_SAVED_NETWORKS];
    int n = g_wifi.copySaved(nets);
    token_store_set_reconnect(nets, n);
    memset(nets, 0, sizeof(nets));
    if (n > 0) {
      NetJob j = {};
      j.type = JOB_RECONNECT;
      j.slot = -1;
      j.modelIdx = -1;
      poller_enqueue(j);
    }
    screen_splash_start(ST_PIN);
  } else {
    // Onboarding: a próxima tela depende de o WiFi conectar, então este
    // autoConnect continua bloqueante (a abertura fica parada até ele).
    g_onboarding = true;
    // Se já há WiFi salvo (reboot no meio do onboarding), pula direto p/ o token
    screen_splash_start(g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS) ? ST_TOKEN : ST_WIFI);
  }
}

void loop() {
  // Ordem do item 2 (fase 2), em todos os estados, antes e depois do desbloqueio
  lv_timer_handler();
  web_api_poll();
  bool changed = poller_apply_results();
  scheduler_tick();
  poller_check_stuck();
  // Trocas de tela/conteúdo pendentes, refresh do conteúdo aberto, ponto e
  // rodapé (1s), tick da tela cheia (ex.: contagem do bloqueio do PIN).
  app_state_tick(changed);

  // Momento de limiar aberto (o agendador decide quando abrir: moment_flush)
  if (app_state() == ST_SHELL) moment_tick();

  delay(5);
}
