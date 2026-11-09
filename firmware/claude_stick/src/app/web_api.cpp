// Servidor HTTP local: onboarding do token (/, POST /token, GET /token/status)
// e, na sessão, também /info, /window e POST /tokens para o bridge e /health
// (THEP_DEV). Não chama o LVGL: o que mexe na tela passa pelos callbacks do .ino.
// Fase 2: o POST /token enfileira um VALIDATE na net_task (item 6).
#include "app/web_api.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include <esp_heap_caps.h>
#include "hw/wifi_manager.h"
#include "hw/frame_stats.h"
#include "config.h"
#include "net/api.h"
#include "net/poller.h"
#include "net/scheduler.h"
#include "net/token_store.h"
#include "model/accounts.h"
#include "model/account_state.h"

// ficam no .ino (item 2 do plano da v3)
extern WiFiManager g_wifi;
extern AccountSlots g_accts;
extern int g_tokenTargetSlot;
extern char g_pendingLabel[ACCT_LBL_MAX];
extern uint8_t g_lang;                  // idioma da tela (0 = pt, 1 = en)
#define TRS(pt, en) (g_lang ? (en) : (pt))

static void (*s_onTokenMsg)(const char *) = nullptr;
static void (*s_onTokStats)() = nullptr;
static void token_msg(const char *m) { if (s_onTokenMsg) s_onTokenMsg(m); }

static WebServer *g_web = nullptr;
static bool s_session = false;          // servidor atual é o da sessão

// Estado da validação do token (item 6, fase 2)
enum TokState : uint8_t { TS_IDLE, TS_PENDING, TS_OK, TS_REJECTED };
static TokState s_tokState = TS_IDLE;
static char s_tokReason[16] = {0};
static bool s_accepting = false;
static uint16_t s_seq = 0;

void stop_web() {
  if (g_web) { g_web->stop(); delete g_web; g_web = nullptr; }
  s_session = false;
}
bool web_running() { return g_web != nullptr; }

static bool g_mdnsUp = false;
void ensure_mdns() {
  if (g_mdnsUp || !g_wifi.isConnected()) return;
  if (MDNS.begin("claude-stick")) {
    MDNS.addService("http", "tcp", 80);
    g_mdnsUp = true;
    Serial.println("[MDNS] claude-stick.local");
  }
}

// ---- páginas HTML ----
#define WEB_CSS \
  ":root{--bg:#0F0F12;--card:#1A1A20;--bd:#30303A;--tx:#F2F0EC;--mut:#8C8C98;--cor:#D97757}" \
  "*{box-sizing:border-box}" \
  "body{margin:0;background:var(--bg);color:var(--tx);font-family:-apple-system,Segoe UI,Roboto,sans-serif;" \
  "display:flex;min-height:100vh;align-items:center;justify-content:center}" \
  ".card{background:var(--card);border:1px solid var(--bd);border-radius:16px;padding:26px;max-width:520px;width:92%}" \
  "h1{font-size:19px;margin:0 0 6px;display:flex;align-items:center;gap:10px}" \
  "p{color:var(--mut);font-size:14px;line-height:1.5;margin:6px 0 14px}" \
  "textarea{width:100%;background:var(--bg);color:var(--tx);border:1px solid var(--bd);border-radius:10px;" \
  "padding:12px;font-family:ui-monospace,monospace;font-size:13px;min-height:96px;resize:vertical}" \
  "input{width:100%;background:var(--bg);color:var(--tx);border:1px solid var(--bd);border-radius:10px;" \
  "padding:12px;font-size:14px;margin-bottom:10px}" \
  "button{margin-top:14px;width:100%;background:var(--cor);color:#1A1A20;border:0;border-radius:10px;" \
  "padding:14px;font-size:16px;font-weight:700;cursor:pointer}" \
  ".spark{width:26px;height:26px;flex:0 0 auto}code,a{color:var(--cor)}"

#define WEB_SPARK \
  "<svg class=spark viewBox='0 0 100 100'><g stroke='#D97757' stroke-width='12' stroke-linecap='round'>" \
  "<line x1=50 y1=9 x2=50 y2=91/><line x1=9 y1=50 x2=91 y2=50/>" \
  "<line x1=21 y1=21 x2=79 y2=79/><line x1=79 y1=21 x2=21 y2=79/>" \
  "<line x1=34 y1=11 x2=66 y2=89/><line x1=66 y1=11 x2=34 y2=89/></g></svg>"

// JS da página: envia o formulário com fetch e consulta /token/status a cada
// 1 s, por no máximo 30 s (item 6, passo 6). S = 1 no servidor da sessão.
#define WEB_JS \
  "<script>(function(){var f=document.forms[0],m=document.getElementById('msg')," \
  "b=f.querySelector('button'),S=document.body.getAttribute('data-s')=='1';" \
  "function show(t){m.textContent=t}function en(){b.disabled=false}" \
  "var NR='O aparelho não respondeu. Tente de novo.';" \
  "function fin(j){if(j.state=='ok'){show(S?'Conta salva.':'Token validado. Defina o PIN no aparelho.');return}" \
  "if(j.state=='rejected'){var r=j.reason;show(r=='closed'?'Abra a tela de token no aparelho e envie de novo.':" \
  "r=='crypto'?'Falha ao cifrar no aparelho. Tente de novo.':r=='short'?'Token vazio ou muito curto.':" \
  "r=='long'?'Token longo demais.':'A API recusou o token ('+r+'). Confira e cole de novo.');en();return}" \
  "show(NR);en()}" \
  "function poll(t0){if(Date.now()-t0>30000){show(NR);en();return}" \
  "setTimeout(function(){fetch('/token/status').then(function(r){return r.json()}).then(function(j){" \
  "if(j.state=='pending')poll(t0);else fin(j)}).catch(function(){poll(t0)})},1000)}" \
  "f.onsubmit=function(e){e.preventDefault();b.disabled=true;show('enviando…');" \
  "fetch('/token',{method:'POST',body:new URLSearchParams(new FormData(f))}).then(function(r){" \
  "if(r.status==409){show('Já existe uma validação em andamento.');en();return}" \
  "return r.json().then(function(j){if(j.state=='pending'){show('validando…');poll(Date.now())}else fin(j)})" \
  "}).catch(function(){show(NR);en()})}})()</script>"

static String web_form() {
  String h = F("<!doctype html><html lang=pt><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Claude Usage Stick</title><style>" WEB_CSS "</style></head>");
  h += s_session ? F("<body data-s=1>") : F("<body data-s=0>");
  h += F("<div class=card>"
         "<h1>" WEB_SPARK " Claude Usage Stick</h1>"
         "<p>Cole o seu token OAuth do Claude (<code>sk-ant-oat01-...</code>) e toque em <b>Salvar</b>. "
         "O gadget vai <b>validar</b> o token");
  h += s_session ? F(" e salvar a conta.</p>") : F(" e pedir um PIN na tela.</p>");
  h += F("<form method=POST action='/token'>"
         "<input name=label maxlength=16 placeholder='rotulo da conta (ex.: Pessoal, Trabalho)' autocomplete=off>"
         "<textarea name=token placeholder='sk-ant-oat01-...' autocomplete=off autofocus></textarea>"
         "<button type=submit>Salvar e validar</button></form><p id=msg></p></div>" WEB_JS "</body></html>");
  return h;
}

static void handleRoot()     { g_web->send(200, "text/html; charset=utf-8", web_form()); }
static void handleNotFound() { g_web->sendHeader("Location", "/"); g_web->send(302, "text/plain", ""); }

static void send_rejected(int code, const char *reason) {
  char b[64];
  snprintf(b, sizeof(b), "{\"state\":\"rejected\",\"reason\":\"%s\"}", reason);
  g_web->send(code, "application/json", b);
}

// Zera o buffer de um String (o token). O WebServer ainda guarda o argumento
// até o fim da requisição (risco residual, como na v2.3).
static void wipe(String &s) {
  if (s.length()) memset((void *)s.c_str(), 0, s.length());
  s = "";
}

// POST /token (item 6, passo 2): enfileira VALIDATE e responde 202.
static void handleTokenPost() {
  if (!s_accepting) { send_rejected(403, "closed"); return; }
  String t = g_web->arg("token");
  t.trim();
  if (t.length() < 8) { wipe(t); send_rejected(400, "short"); return; }
  if (t.length() > TOKEN_MAX_LEN) { wipe(t); send_rejected(400, "long"); return; }
  if (s_tokState == TS_PENDING) {
    wipe(t);
    g_web->send(409, "application/json", "{\"state\":\"pending\"}");
    return;
  }
  String lb = g_web->arg("label");
  lb.trim();
  strlcpy(g_pendingLabel, lb.c_str(), ACCT_LBL_MAX);

  token_store_clear_pending();                 // sobra de validação anterior
  bool stored = token_store_set_pending(t.c_str());
  wipe(t);
  if (!stored) { send_rejected(400, "short"); return; }

  s_seq++;
  NetJob j = {};
  j.type = JOB_VALIDATE;
  j.slot = (int8_t)g_tokenTargetSlot;
  j.gen = s_seq;
  j.modelIdx = -1;
  strlcpy(j.model, PROBE_MODEL, sizeof(j.model));
  if (!poller_enqueue(j)) {
    token_store_clear_pending();
    send_rejected(503, "busy");
    return;
  }
  s_tokState = TS_PENDING;
  s_tokReason[0] = 0;
  Serial.printf("[WEB] POST /token -> VALIDATE slot %d seq %u\n", g_tokenTargetSlot, (unsigned)s_seq);
  token_msg(TRS("validando token...", "validating token..."));
  g_web->send(202, "application/json", "{\"state\":\"pending\"}");
}

// GET /token/status (item 6, passo 3).
static void handleTokenStatus() {
  static const char *NAMES[4] = {"idle", "pending", "ok", "rejected"};
  char b[80];
  if (s_tokState == TS_REJECTED)
    snprintf(b, sizeof(b), "{\"state\":\"rejected\",\"reason\":\"%s\"}", s_tokReason);
  else
    snprintf(b, sizeof(b), "{\"state\":\"%s\"}", NAMES[s_tokState]);
  g_web->send(200, "application/json", b);
}

void web_token_begin() { s_accepting = true; }

void web_token_reset() {
  s_accepting = false;
  if (s_tokState == TS_PENDING) {
    token_store_clear_pending();
    s_seq++;
    s_tokState = TS_IDLE;
    Serial.println("[WEB] tela de token fechou: validacao em andamento cancelada");
  }
}

uint16_t web_token_seq() { return s_seq; }

void web_token_set_result(bool ok, const char *reason) {
  s_tokState = ok ? TS_OK : TS_REJECTED;
  strlcpy(s_tokReason, (!ok && reason) ? reason : "", sizeof(s_tokReason));
}

// ---- Endpoints de dados (bridge de tokens; ver tools/token_bridge.py) ----
static long long jll(const String &s, const char *key) {
  String k = String("\"") + key + "\"";
  int i = s.indexOf(k); if (i < 0) return 0;
  i = s.indexOf(':', i + k.length() - 1); if (i < 0) return 0;
  return atoll(s.c_str() + i + 1);
}
static bool jstr(const String &s, const char *key, char *out, size_t sz) {
  String k = String("\"") + key + "\"";
  int i = s.indexOf(k); if (i < 0) return false;
  i = s.indexOf(':', i + k.length()); if (i < 0) return false;
  i = s.indexOf('"', i); if (i < 0) return false;
  int e = s.indexOf('"', i + 1); if (e < 0) return false;
  strlcpy(out, s.substring(i + 1, e).c_str(), sz);
  return true;
}
static void handleWindow() {
  const UsageData &u = acct[g_accts.active].usage;
  char b[256];
  snprintf(b, sizeof(b),
           "{\"now\":%lu,\"h5_reset\":%lu,\"d7_reset\":%lu,\"h5_util\":%.4f,\"d7_util\":%.4f,"
           "\"account\":\"%s\",\"slot\":%d}",
           (unsigned long)time(nullptr),
           (unsigned long)u.h5ResetEpoch, (unsigned long)u.d7ResetEpoch,
           u.h5 / 100.0f, u.d7 / 100.0f,
           g_accts.label[g_accts.active], g_accts.active);
  g_web->send(200, "application/json", b);
}
static void handleTokensPost() {
  String body = g_web->arg("plain");
  char lbl[ACCT_LBL_MAX];
  if (jstr(body, "account", lbl, sizeof(lbl)) && lbl[0] &&
      strcmp(lbl, g_accts.label[g_accts.active]) != 0) {
    char r[96];
    snprintf(r, sizeof(r), "{\"error\":\"account_mismatch\",\"active\":\"%s\"}",
             g_accts.label[g_accts.active]);
    g_web->send(409, "application/json", r);
    return;
  }
  TokenStats &tok = acct[g_accts.active].tok;
  tok.tin      = jll(body, "in");
  tok.tout     = jll(body, "out");
  tok.cache    = jll(body, "cache");
  tok.sessions = (int)jll(body, "sessions");
  tok.atMs     = millis();
  Serial.printf("[TOK] in=%lld out=%lld cache=%lld sess=%d\n",
                tok.tin, tok.tout, tok.cache, tok.sessions);
  g_web->send(200, "application/json", "{\"ok\":true}");
  if (s_onTokStats) s_onTokStats();
}
static void handleInfo() {
  String h = F("<!doctype html><html lang=pt><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Claude Usage Stick</title><style>" WEB_CSS "</style></head><body><div class=card>"
               "<h1>" WEB_SPARK " Claude Usage Stick</h1>"
               "<p>Device online. Endpoints: <code>GET /window</code> (janela atual) e "
               "<code>POST /tokens</code> (bridge de tokens por sessao — ver tools/token_bridge.py). "
               "Para adicionar ou trocar uma conta, abra a tela de token no aparelho e use <a href='/'>/</a>.</p>"
               "</div></body></html>");
  g_web->send(200, "text/html; charset=utf-8", h);
}
#ifdef THEP_DEV
static void (*s_healthUi)(WebHealthUi *) = nullptr;
void web_on_health_ui(void (*cb)(WebHealthUi *)) { s_healthUi = cb; }

// GET /health (só THEP_DEV): métricas do Technical Plan §9, sem token nem rótulo.
static void handleHealth() {
  WebHealthUi ui = {-1};
  if (s_healthUi) s_healthUi(&ui);
  char b[256];
  String j;
  j.reserve(512);
  snprintf(b, sizeof(b),
           "{\"uptime_s\":%lu,\"heap_internal_free\":%u,\"heap_internal_largest\":%u,"
           "\"psram_free\":%u,\"lv_mem_used_pct\":%d,\"net_stack_hwm\":%u,",
           (unsigned long)(millis() / 1000),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
           (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           ui.lvMemUsedPct, (unsigned)poller_stack_hwm());
  j += b;
  snprintf(b, sizeof(b), "\"last_cycle_ms\":%u,\"cycles_over_interval\":%u,\"frame_ms_max\":%u,"
           "\"flush_ms_max\":%u,\"disp_mode\":\"%s\",\"slots\":[",
           (unsigned)scheduler_last_cycle_ms(), (unsigned)scheduler_cycles_over_interval(),
           (unsigned)frame_ms_max(), (unsigned)frame_flush_ms_max(), frame_mode());
  j += b;
  uint32_t now = (uint32_t)time(nullptr);
  bool first = true;
  for (int s = 0; s < ACCT_MAX; s++) {
    if (!g_accts.used[s]) continue;
    const AccountState &a = acct[s];
    long age = (a.usageAtEpoch && now >= a.usageAtEpoch) ? (long)(now - a.usageAtEpoch) : -1;
    snprintf(b, sizeof(b), "%s{\"slot\":%d,\"class\":\"%s\",\"age_s\":%ld,\"backoff\":%u}",
             first ? "" : ",", s, a.keyOk ? fetch_class_name(a.lastClass) : "NOKEY", age,
             (unsigned)a.backoffCycles);
    j += b;
    first = false;
  }
  j += "]}";
  g_web->send(200, "application/json", j);
}
#endif

// Servidor da sessão (item 6, fase 2): sobe no desbloqueio e fica a sessão toda.
void web_start_session() {
  stop_web();
  ensure_mdns();
  g_web = new WebServer(80);
  s_session = true;
  g_web->on("/", HTTP_GET, handleRoot);
  g_web->on("/info", HTTP_GET, handleInfo);
  g_web->on("/window", HTTP_GET, handleWindow);
  g_web->on("/tokens", HTTP_POST, handleTokensPost);
  g_web->on("/token", HTTP_POST, handleTokenPost);
  g_web->on("/token/status", HTTP_GET, handleTokenStatus);
#ifdef THEP_DEV
  g_web->on("/health", HTTP_GET, handleHealth);
#endif
  g_web->onNotFound([]() { g_web->send(404, "application/json", "{\"error\":\"not_found\"}"); });
  g_web->begin();
  Serial.printf("[WEB] servidor da sessao em http://%s\n", WiFi.localIP().toString().c_str());
}

// Servidor do onboarding (formulario do token). Fica vivo em ST_SETUP_PIN.
void web_start_onboarding() {
  stop_web();
  g_web = new WebServer(80);
  s_session = false;
  g_web->on("/", HTTP_GET, handleRoot);
  g_web->on("/token", HTTP_POST, handleTokenPost);
  g_web->on("/token/status", HTTP_GET, handleTokenStatus);
  g_web->onNotFound(handleNotFound);
  g_web->begin();
  Serial.printf("[WEB] servidor do onboarding em http://%s\n", WiFi.localIP().toString().c_str());
}

void web_api_poll() {
  if (g_web) g_web->handleClient();
}

void web_on_token_msg(void (*cb)(const char *)) { s_onTokenMsg = cb; }
void web_on_tok_stats(void (*cb)()) { s_onTokStats = cb; }
