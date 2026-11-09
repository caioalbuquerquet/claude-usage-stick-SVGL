// Momento de limiar (item 40). Ver overlay_moment.h.
//
// Animação toda procedural (moment_tick no loop), sem lv_anim pendurado: o
// pássaro cai de cima e se acomoda; depois quica (25/50), treme (70) ou fica
// caído e cinza (100, com a borda vermelha piscando). A % conta do limiar
// anterior até o atual e a barra de 18 blocos acende junto.
#include "ui/overlay_moment.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "app/app_state.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "net/scheduler.h"
#include "logic/levels.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"
#include "ui/widgets.h"
#include "ui/assets_thep.h"

extern AccountSlots g_accts;

namespace {

constexpr int NCELLS = 18;
constexpr int BIRD_X = 40, BIRD_Y = 98;
constexpr int COL_X = 196;
constexpr uint32_t DURATION_MS = 4600;

struct MomentUI {
  lv_obj_t *scrim, *bird, *pct, *ring, *bar;
  lv_span_t* barOn;
  lv_span_t* barOff;
  int win, thr, fromPct;
  int lastPct;
  uint32_t t0;
};
MomentUI g_mo = {};

void close_cb(lv_event_t* e) {
  (void)e;
  moment_close();
  moment_flush();
}

void set_bar(float v, uint32_t col) {
  char all[3 * NCELLS + 1];
  text_bar(v, NCELLS, all, sizeof(all));
  const char* off = strstr(all, "\xE2\x96\x91");   // primeiro "░"
  char on[sizeof(all)];
  size_t k = off ? (size_t)(off - all) : strlen(all);
  memcpy(on, all, k);
  on[k] = 0;
  lv_span_set_text(g_mo.barOn, on);
  lv_style_set_text_color(lv_span_get_style(g_mo.barOn), lv_color_hex(col));
  lv_span_set_text(g_mo.barOff, off ? off : "");
  lv_spangroup_refr_mode(g_mo.bar);
  lv_obj_invalidate(g_mo.bar);
}

}  // namespace

lv_obj_t* moment_obj() { return g_mo.scrim; }

void moment_close() {
  if (!g_mo.scrim) return;
  lv_obj_delete(g_mo.scrim);
  memset(&g_mo, 0, sizeof(g_mo));
}

bool show_moment(int slot, int win, int thr) {
  if (g_mo.scrim || app_state() != ST_SHELL || app_state_pending()) return false;
  if (slot < 0 || slot >= ACCT_MAX) slot = g_accts.active;

  g_mo.win = win;
  g_mo.thr = thr;
  g_mo.fromPct = (thr == 25) ? 0 : (thr == 50) ? 25 : (thr == 70) ? 50 : 70;
  g_mo.lastPct = -1;
  g_mo.t0 = millis();

  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* f13 = theme_font(FONT_13);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);

  lv_obj_t* s = lv_obj_create(lv_layer_top());
  g_mo.scrim = s;
  lv_obj_remove_style_all(s);
  lv_obj_set_pos(s, 0, 0);
  lv_obj_set_size(s, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(s, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(s, 248, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, close_cb, LV_EVENT_CLICKED, NULL);

  // moldura: C_BORDER; no 100%, vermelha piscando no tick
  g_mo.ring = lv_obj_create(s);
  lv_obj_remove_style_all(g_mo.ring);
  lv_obj_set_pos(g_mo.ring, 6, 6);
  lv_obj_set_size(g_mo.ring, SCREEN_WIDTH - 12, SCREEN_HEIGHT - 12);
  lv_obj_set_style_radius(g_mo.ring, 8, 0);
  lv_obj_set_style_border_width(g_mo.ring, thr == 100 ? 3 : 1, 0);
  lv_obj_set_style_border_color(g_mo.ring, lv_color_hex(thr == 100 ? C_BAD : C_BORDER), 0);
  lv_obj_set_style_border_opa(g_mo.ring, LV_OPA_COVER, 0);
  lv_obj_remove_flag(g_mo.ring, LV_OBJ_FLAG_CLICKABLE);

  // pássaro em braille (cinza no 100%)
  g_mo.bird = lv_image_create(s);
  lv_image_set_src(g_mo.bird, &img_bird_braille_124);
  lv_obj_set_pos(g_mo.bird, BIRD_X, BIRD_Y - 60);
  lv_obj_remove_flag(g_mo.bird, LV_OBJ_FLAG_CLICKABLE);
  if (thr == 100) {
    lv_obj_set_style_image_recolor(g_mo.bird, lv_color_hex(C_DIM), 0);
    lv_obj_set_style_image_recolor_opa(g_mo.bird, 220, 0);
  }

  // coluna de texto: conta, janela, % contando, barra, mensagem, reset
  char lbl[ACCT_LBL_MAX + 16], fit[ACCT_LBL_MAX + 16];
  if (g_accts.label[slot][0]) snprintf(lbl, sizeof(lbl), "%s", g_accts.label[slot]);
  else snprintf(lbl, sizeof(lbl), tr(T::ACCT_DEFAULT_FMT), slot + 1);
  fit_label(lbl, 32, fit, sizeof(fit));
  ui_label(s, fb, C_ACCENT, COL_X, 64, fit);
  ui_label(s, f13, C_DIM, COL_X, 84, win == 0 ? TRS("Janela 5h", "5h window")
                                              : TRS("Semana", "Week"));
  g_mo.pct = ui_label(s, theme_font(FONT_NUM_50), C_OK, COL_X, 140, "0%");

  g_mo.bar = lv_spangroup_create(s);
  lv_obj_remove_flag(g_mo.bar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_text_font(g_mo.bar, f13, 0);
  lv_spangroup_set_mode(g_mo.bar, LV_SPAN_MODE_EXPAND);
  lv_obj_set_pos(g_mo.bar, COL_X, 162 - ui_ascent(f13));
  lv_span_t* sp = lv_spangroup_new_span(g_mo.bar);
  lv_span_set_text(sp, "[");
  lv_style_set_text_color(lv_span_get_style(sp), lv_color_hex(C_DIM));
  g_mo.barOn = lv_spangroup_new_span(g_mo.bar);
  g_mo.barOff = lv_spangroup_new_span(g_mo.bar);
  lv_style_set_text_color(lv_span_get_style(g_mo.barOff), lv_color_hex(C_UNLIT));
  sp = lv_spangroup_new_span(g_mo.bar);
  lv_span_set_text(sp, "]");
  lv_style_set_text_color(lv_span_get_style(sp), lv_color_hex(C_DIM));
  set_bar(0, C_OK);

  const char* MSG[4] = {
      TRS("Começando · ritmo tranquilo", "Just starting · easy pace"),
      TRS("Metade da janela usada", "Half the window used"),
      TRS("Atenção · uso alto", "Heads up · heavy usage"),
      TRS("Limite atingido · aguarde o reset", "Limit reached · wait for the reset"),
  };
  int mi = (thr == 25) ? 0 : (thr == 50) ? 1 : (thr == 70) ? 2 : 3;
  lv_obj_t* msg = mklabel(s, MSG[mi], f13, thr == 100 ? C_BAD_INK : C_TEXT);
  lv_obj_set_pos(msg, COL_X, 178);
  lv_obj_set_width(msg, SCREEN_WIDTH - COL_X - 20);
  lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);

  char e[32], b[48];
  const UsageData& u = acct[slot].usage;
  fmt_eta(win == 0 ? u.h5ResetEpoch : u.d7ResetEpoch, e, sizeof(e));
  snprintf(b, sizeof(b), TRS("reset em %s", "resets in %s"), e);
  ui_label(s, f11, C_DIM, COL_X, 236, b);

  lv_obj_t* hint = ui_label(s, f11, C_DIM, 0, 300, TRS("toque para fechar", "tap to close"));
  lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -20, 300 - ui_ascent(f11));
  overlay_restack();                          // incidente continua por cima
  return true;
}

void moment_tick() {
  if (!g_mo.scrim) return;
  uint32_t t = millis() - g_mo.t0;

  // entrada: cai de cima com acomodação; depois o humor manda
  int y = BIRD_Y, x = BIRD_X;
  if (t < 450) {
    float p = t / 450.0f;
    y = BIRD_Y - (int)((1.0f - p) * (1.0f - p) * 60.0f);
  } else if (g_mo.thr == 25 || g_mo.thr == 50) {
    y = BIRD_Y + (int)(4.0f * sinf((t - 450) / 260.0f));     // quica
  } else if (g_mo.thr == 70) {
    x = BIRD_X + (((t / 70) % 2) ? 2 : -2);                    // treme
  } else if (g_mo.thr == 100) {
    y = BIRD_Y + 6;                                            // caído
  }
  if (lv_obj_get_x(g_mo.bird) != x || lv_obj_get_y(g_mo.bird) != y) lv_obj_set_pos(g_mo.bird, x, y);

  // % contando (200..1100 ms) e barra acendendo junto
  float p = (t < 200) ? 0 : (t > 1100 ? 1.0f : (t - 200) / 900.0f);
  float v = g_mo.fromPct + (g_mo.thr - g_mo.fromPct) * p;
  int iv = (int)(v + 0.5f);
  if (iv != g_mo.lastPct) {
    g_mo.lastPct = iv;
    char b[12];
    snprintf(b, sizeof(b), "%d%%", iv);
    uint32_t col = level_color(level_for(v));
    lv_label_set_text(g_mo.pct, b);
    lv_obj_set_style_text_color(g_mo.pct, lv_color_hex(col), 0);
    set_bar(v, col);
  }

  // borda vermelha piscando (100%)
  if (g_mo.thr == 100) {
    lv_opa_t o = ((t / 350) % 2) ? 190 : 40;
    if (lv_obj_get_style_border_opa(g_mo.ring, LV_PART_MAIN) != o)
      lv_obj_set_style_border_opa(g_mo.ring, o, 0);
  }

  if (t > DURATION_MS) {
    moment_close();
    moment_flush();
  }
}
