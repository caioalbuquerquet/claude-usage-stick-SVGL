// Servidor HTTP local: onboarding do token (/, POST /token) e dados para o
// bridge de tokens (/, /window, POST /tokens). Não chama lv_*: o que mexe na
// tela ou no estado do PIN passa pelos callbacks registrados pelo .ino.
// Fase 1: lê e grava acct[g_accts.active] (a conta em foco).
#include "app/web_api.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include "hw/wifi_manager.h"
#include "config.h"
#include "net/api.h"
#include "model/accounts.h"
#include "model/account_state.h"

// ficam no .ino (item 2 do plano da v3)
extern WiFiManager g_wifi;
extern AccountSlots g_accts;
extern char g_pendingToken[200];
extern char g_pendingLabel[ACCT_LBL_MAX];
extern uint8_t g_lang;                  // idioma da tela (0 = pt, 1 = en)
#define TRS(pt, en) (g_lang ? (en) : (pt))

static void (*s_onTokenMsg)(const char *) = nullptr;
static void (*s_onTokStats)() = nullptr;
static void (*s_onTokenOk)() = nullptr;
static void token_msg(const char *m) { if (s_onTokenMsg) s_onTokenMsg(m); }

static WebServer *g_web = nullptr;
volatile bool g_tokenGot = false;
UsageData g_validatedUsage = {};

void stop_web() { if (g_web) { g_web->stop(); delete g_web; g_web = nullptr; } }

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

static String web_form() {
  String h = F("<!doctype html><html lang=pt><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Claude Usage Stick</title><style>" WEB_CSS "</style></head><body><div class=card>"
               "<h1>" WEB_SPARK " Claude Usage Stick</h1>"
               "<p>Cole o seu token OAuth do Claude (<code>sk-ant-oat01-...</code>) e toque em <b>Salvar</b>. "
               "O gadget vai <b>validar</b> o token e pedir um PIN na tela.</p>"
               "<form method=POST action='/token'>"
               "<input name=label maxlength=16 placeholder='rotulo da conta (ex.: Pessoal, Trabalho)' autocomplete=off>"
               "<textarea name=token placeholder='sk-ant-oat01-...' autocomplete=off autofocus></textarea>"
               "<button type=submit>Salvar e validar</button></form></div></body></html>");
  return h;
}
static String web_result(bool ok, const String &msg) {
  String h = F("<!doctype html><html lang=pt><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Claude Usage Stick</title><style>" WEB_CSS "</style></head><body><div class=card>");
  if (ok) {
    h += F("<h1>" WEB_SPARK " Token validado</h1>"
           "<p>Token aceito pela API. Agora <b>defina um PIN de 4 dígitos</b> na tela do gadget para finalizar. "
           "Pode fechar esta página.</p>");
  } else {
    h += F("<h1>" WEB_SPARK " Token recusado</h1><p>");
    h += msg;
    h += F("</p><p><a href='/'>Voltar e tentar de novo</a></p>");
  }
  h += F("</div></body></html>");
  return h;
}

static void handleRoot()     { g_web->send(200, "text/html; charset=utf-8", web_form()); }
static void handleNotFound() { g_web->sendHeader("Location", "/"); g_web->send(302, "text/plain", ""); }

static void handleTokenPost() {
  String lb = g_web->arg("label");
  lb.trim();
  strlcpy(g_pendingLabel, lb.c_str(), sizeof(g_pendingLabel));
  String t = g_web->arg("token");
  t.trim();
  if (t.length() < 8) {
    token_msg(TRS("token vazio", "empty token"));
    g_web->send(200, "text/html; charset=utf-8", web_result(false, "Token vazio ou muito curto."));
    return;
  }
  // feedback no device antes da chamada bloqueante
  token_msg(TRS("validando token...", "validating token..."));

  UsageData tmp = {};
  int http; uint16_t ms;
  bool ok = fetchUsage(t.c_str(), PROBE_MODEL, tmp, &http, &ms);
  if (ok) {
    strlcpy(g_pendingToken, t.c_str(), sizeof(g_pendingToken));
    g_validatedUsage = tmp;                     // .ino copia p/ acct[slot] ao salvar
    if (s_onTokenOk) s_onTokenOk();             // .ino zera o estado do PIN
    g_tokenGot = true;                          // loop -> ST_SETUP_PIN
    token_msg(TRS("token OK! defina o PIN", "token OK! set the PIN"));
    g_web->send(200, "text/html; charset=utf-8", web_result(true, ""));
  } else {
    String m = String("A API recusou o token (") + tmp.error + "). Confira e cole de novo.";
    token_msg(TRS("token recusado, tente de novo", "token rejected, try again"));
    g_web->send(200, "text/html; charset=utf-8", web_result(false, m));
  }
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
               "<code>POST /tokens</code> (bridge de tokens por sessao — ver tools/token_bridge.py).</p>"
               "</div></body></html>");
  g_web->send(200, "text/html; charset=utf-8", h);
}
void start_data_web() {
  stop_web();
  ensure_mdns();
  g_web = new WebServer(80);
  g_web->on("/", HTTP_GET, handleInfo);
  g_web->on("/window", HTTP_GET, handleWindow);
  g_web->on("/tokens", HTTP_POST, handleTokensPost);
  g_web->onNotFound([]() { g_web->send(404, "application/json", "{\"error\":\"not_found\"}"); });
  g_web->begin();
}

// Servidor do onboarding (formulario do token). Era o fim de ui_token() no .ino.
void web_start_onboarding() {
  String url = String("http://") + WiFi.localIP().toString();
  // sobe o servidor web (formulario do token)
  g_web = new WebServer(80);
  g_web->on("/", HTTP_GET, handleRoot);
  g_web->on("/token", HTTP_POST, handleTokenPost);
  g_web->onNotFound(handleNotFound);
  g_web->begin();
  Serial.printf("[WEB] servidor em %s\n", url.c_str());
}

void web_api_poll() {
  if (g_web) g_web->handleClient();
}

void web_on_token_msg(void (*cb)(const char *)) { s_onTokenMsg = cb; }
void web_on_tok_stats(void (*cb)()) { s_onTokStats = cb; }
void web_on_token_ok(void (*cb)()) { s_onTokenOk = cb; }
