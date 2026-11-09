// /ritmo (item 39): quota da 5h queimada em cada hora local, da conta em
// foco, com o filtro de período (hoje / 7d / 30d / tudo, NVS "heatm"). Barras
// em C_ACCENT com a hora atual em C_TITLE. Mesmo título e toque de /janela.
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "model/history.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"

extern Preferences g_prefs;
extern AccountSlots g_accts;
extern int g_tzOffset;

namespace {

constexpr int BASE_Y = 168;      // base das barras
constexpr int BAR_W = 12, STEP = 18;
constexpr int X0 = (SHELL_CONTENT_W - 23 * STEP - BAR_W) / 2;   // 24 barras centradas
constexpr int MAX_H = 120;

lv_obj_t* s_bar[24];
lv_obj_t* s_btn[4];
bool s_built = false;

const char* mode_name(int i) {
  switch (i) {
    case 0: return TRS("hoje", "today");
    case 1: return "7d";
    case 2: return "30d";
    default: return TRS("tudo", "all");
  }
}

void btn_style() {
  for (int i = 0; i < 4; i++) {
    if (!s_btn[i]) continue;
    bool on = (i == g_heatMode);
    lv_obj_set_style_bg_opa(s_btn[i], on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_btn[i], lv_color_hex(on ? C_TITLE : C_BORDER), 0);
    lv_obj_t* l = lv_obj_get_child(s_btn[i], 0);
    if (l) lv_obj_set_style_text_color(l, lv_color_hex(on ? C_TITLE : C_DIM), 0);
  }
}

void btn_cb(lv_event_t* e) {
  int m = (int)(intptr_t)lv_event_get_user_data(e);
  if (m == g_heatMode) return;
  g_heatMode = m;
  g_prefs.putInt("heatm", m);
  btn_style();
  screen_heat_refresh();
}

}  // namespace

void screen_heat_build(lv_obj_t* p) {
  ui_focus_title();
  memset(s_bar, 0, sizeof(s_bar));
  memset(s_btn, 0, sizeof(s_btn));
  const lv_font_t* f11 = theme_font(FONT_11);

  ui_label(p, theme_font(FONT_BOLD_13), C_ACCENT, 0, 16, TRS("Ritmo por hora", "Hourly rhythm"));
  // filtro de período: 4 botões com borda, o ativo em C_TITLE
  for (int i = 3; i >= 0; i--) {
    lv_obj_t* b = ui_button(p, mode_name(i), C_DIM, 52, 26);
    lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -(3 - i) * 58, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(b, 0), f11, 0);
    lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    s_btn[i] = b;
  }
  btn_style();

  for (int h = 0; h < 24; h++) {
    s_bar[h] = rrect(p, X0 + h * STEP, BASE_Y - 2, BAR_W, 2, 2, C_ACCENT);
  }
  rrect(p, X0 - 2, BASE_Y + 1, 23 * STEP + BAR_W + 4, 1, 0, C_UNLIT);
  const int ticks[5] = {0, 6, 12, 18, 23};
  for (int i = 0; i < 5; i++) {
    char t[6];
    snprintf(t, sizeof(t), "%dh", ticks[i]);
    ui_label(p, f11, C_DIM, X0 + ticks[i] * STEP, BASE_Y + 16, t);
  }
  ui_label(p, f11, C_DIM, 0, 208, TRS("quota da janela 5h queimada em cada hora local",
                                      "5h-window quota burned per local hour"));
  s_built = true;
  screen_heat_refresh();
}

void screen_heat_refresh() {
  if (!s_built) return;
  float data[24];
  const uint32_t now = (uint32_t)time(nullptr);
  heat_mode_data(acct[g_accts.active], g_heatMode, now, g_tzOffset * 3600, data);
  float mx = 1.0f;
  for (int h = 0; h < 24; h++) if (data[h] > mx) mx = data[h];
  int curHour = -1;
  if (now > 1000000000UL) {
    time_t t = (time_t)now;
    struct tm tv;
    localtime_r(&t, &tv);
    curHour = tv.tm_hour;
  }
  for (int h = 0; h < 24; h++) {
    if (!s_bar[h]) continue;
    float r = data[h] / mx;
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    int hgt = 2 + (int)(r * (MAX_H - 2));
    lv_obj_set_height(s_bar[h], hgt);
    lv_obj_set_y(s_bar[h], BASE_Y - hgt);
    lv_obj_set_style_bg_color(s_bar[h], lv_color_hex(h == curHour ? C_TITLE : C_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_bar[h], (lv_opa_t)(90 + (int)(r * 165)), 0);
  }
}

void screen_heat_reset() {
  s_built = false;
  memset(s_bar, 0, sizeof(s_bar));
  memset(s_btn, 0, sizeof(s_btn));
}
