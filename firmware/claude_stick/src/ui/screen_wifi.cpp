// WiFi (item 39): scan + senha. Tela cheia no onboarding (ST_WIFI, moldura
// THEP) ou conteúdo WIFI do shell na sessão. Na sessão o agendador já está
// pausado (app_state, ao abrir WIFI): a tela mostra "aguardando a rede…" até
// a net_task terminar o job em andamento e só então mexe no rádio.
#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "hw/wifi_manager.h"
#include "net/poller.h"
#include "app/web_api.h"
#include "app/app_state.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"

namespace {

constexpr int MAX_NETS = 12;

lv_obj_t* s_list = nullptr;
lv_obj_t* s_ta = nullptr;
lv_obj_t* s_kb = nullptr;
lv_obj_t* s_status = nullptr;
bool s_waiting = false;
char s_sel[33] = {0};
char s_ssid[MAX_NETS][33];

void item_cb(lv_event_t* e);

void populate() {
  lv_obj_clean(s_list);
  lv_label_set_text(s_status, TRS("escaneando redes…", "scanning networks…"));
  lv_refr_now(NULL);
  WiFiManager::NetworkInfo nets[MAX_NETS];
  int n = g_wifi.scanNetworks(nets, MAX_NETS);
  for (int i = 0; i < n; i++) {
    strlcpy(s_ssid[i], nets[i].ssid, sizeof(s_ssid[i]));
    char rssi[12];
    snprintf(rssi, sizeof(rssi), "%d dBm", nets[i].rssi);
    lv_obj_t* val = nullptr;
    ui_row(s_list, LV_SYMBOL_WIFI, s_ssid[i], C_TEXT, rssi, item_cb, (void*)(intptr_t)i, &val);
    if (val) {   // força do sinal em texto apagado
      lv_obj_set_style_text_font(val, theme_font(FONT_11), 0);
      lv_obj_set_style_text_color(val, lv_color_hex(C_DIM), 0);
    }
  }
  lv_label_set_text(s_status, n > 0 ? TRS("toque na sua rede", "tap your network")
                                    : TRS("nenhuma rede · toque em reescanear",
                                          "no networks · tap rescan"));
}

void item_cb(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= MAX_NETS) return;
  strlcpy(s_sel, s_ssid[i], sizeof(s_sel));
  char m[64];
  snprintf(m, sizeof(m), TRS("senha de \"%s\":", "password for \"%s\":"), s_sel);
  lv_label_set_text(s_status, m);
  lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
  lv_textarea_set_text(s_ta, "");
  lv_obj_clear_flag(s_ta, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(s_kb, s_ta);
}

void kb_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char* pass = lv_textarea_get_text(s_ta);
    lv_label_set_text(s_status, TRS("conectando…", "connecting…"));
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ta, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);
    bool ok = g_wifi.connectTo(s_sel, pass, 15000);
    if (ok) {
      ensure_mdns();
      if (session_unlocked()) shell_show(ContentId::HOME);
      else request_state(ST_TOKEN);
    } else {
      lv_label_set_text(s_status, TRS("falhou · toque numa rede de novo", "failed · tap a network again"));
      lv_obj_clear_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    }
  } else if (code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_status, TRS("toque na sua rede", "tap your network"));
  }
}

void rescan_cb(lv_event_t* e) {
  (void)e;
  populate();
}

// p: container do conteúdo (shell: 438 x 214; moldura: 438 x 234).
void build(lv_obj_t* p, int h, bool inShell) {
  const lv_font_t* f11 = theme_font(FONT_11);

  s_waiting = inShell && !poller_idle();
  if (s_waiting) {
    lv_obj_t* w = mklabel(p, tr(T::WAITING_NET), theme_font(FONT_13), C_DIM);
    lv_obj_align(w, LV_ALIGN_CENTER, 0, 0);
    return;
  }

  lv_obj_t* rb = ui_button(p, TRS("reescanear", "rescan"), C_TITLE, 0, 28);
  lv_obj_align(rb, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(rb, rescan_cb, LV_EVENT_CLICKED, NULL);

  s_status = ui_label(p, f11, C_DIM, 0, 18, "…");
  lv_obj_set_width(s_status, SHELL_CONTENT_W - 110);

  s_list = ui_list(p, SHELL_CONTENT_W, h - 36);
  lv_obj_set_y(s_list, 36);

  s_ta = lv_textarea_create(p);
  lv_textarea_set_one_line(s_ta, true);
  lv_textarea_set_password_mode(s_ta, true);
  lv_textarea_set_placeholder_text(s_ta, TRS("senha do WiFi", "WiFi password"));
  ui_style_textarea(s_ta);
  lv_obj_set_size(s_ta, SHELL_CONTENT_W, 40);
  lv_obj_set_pos(s_ta, 0, 34);
  lv_obj_add_flag(s_ta, LV_OBJ_FLAG_HIDDEN);

  s_kb = lv_keyboard_create(inShell ? shell_float() : lv_screen_active());
  ui_style_keyboard(s_kb);
  lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(s_kb, kb_cb, LV_EVENT_ALL, NULL);

  populate();
}

}  // namespace

void screen_wifi_full() {
  lv_obj_t* p = ui_frame(TRS("configurar WiFi", "configure WiFi"));
  build(p, FRAME_CONTENT_H, false);
}

void screen_wifi_build(lv_obj_t* parent) {
  build(parent, SHELL_CONTENT_H, session_unlocked() && shell_is_up());
}

void screen_wifi_tick() {
  if (s_waiting && poller_idle()) {
    s_waiting = false;
    shell_show(ContentId::WIFI);
  }
}

void screen_wifi_reset() {
  s_list = s_ta = s_kb = s_status = nullptr;
  s_waiting = false;
}
