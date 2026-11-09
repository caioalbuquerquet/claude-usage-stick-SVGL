// /janela (item 39): gráfico da janela de 5h da conta em foco no visual
// Hermes. Histórico em C_TITLE, projeção tracejada, grade em C_UNLIT. A
// legenda é projection_text (maxChars = 60), a mesma da tela inicial (G7).
// A linha de projeção só aparece em ESGOTA (até etaEpoch, a 100%) e em
// OK_ATE_RESET com rate > 0.02 (até resetEpoch, a pctAtReset), como na v2.3.
// Título do painel = rótulo da conta em foco; tocar nele passa para a próxima.
#include <Arduino.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "model/history.h"
#include "logic/projection.h"
#include "logic/levels.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"

extern AccountSlots g_accts;
extern int g_tzOffset;

namespace {

// Área do gráfico dentro do conteúdo (438 x 214).
constexpr int TR_X0 = 30;
constexpr int TR_Y0 = 30;
constexpr int TR_W = SHELL_CONTENT_W - TR_X0 - 4;   // 404
constexpr int TR_H = 110;

struct TrendUI {
  lv_obj_t *pct, *hist, *proj, *dot, *t0, *t1, *cap, *tok;
};
TrendUI s = {};
lv_point_precise_t s_pts[HIST_MAX];
lv_point_precise_t s_projPts[2];
lv_point_precise_t s_grid[3][2];

int tr_x(uint32_t tt, uint32_t ws, uint32_t we) {
  if (we <= ws) return TR_X0;
  long long v = (long long)(tt - ws) * TR_W / (long long)(we - ws);
  if (v < 0) v = 0;
  if (v > TR_W) v = TR_W;
  return TR_X0 + (int)v;
}
int tr_y(float p) {
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return TR_Y0 + TR_H - (int)(p * TR_H / 100.0f);
}

}  // namespace

void screen_trend_build(lv_obj_t* p) {
  ui_focus_title();
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);

  ui_label(p, fb, C_ACCENT, 0, 14, TRS("Janela 5h", "5h window"));
  s.pct = ui_label(p, fb, C_DIM, 0, 14);
  lv_obj_align(s.pct, LV_ALIGN_TOP_RIGHT, 0, 14 - ui_ascent(fb));

  // eixo e grade: 0 e 100 sólidos, 25/50/75 tracejados
  rrect(p, TR_X0, tr_y(0), TR_W, 1, 0, C_UNLIT);
  rrect(p, TR_X0, tr_y(100), TR_W, 1, 0, C_UNLIT);
  for (int i = 0; i < 3; i++) {
    int y = tr_y((i + 1) * 25.0f);
    s_grid[i][0] = {(lv_value_precise_t)TR_X0, (lv_value_precise_t)y};
    s_grid[i][1] = {(lv_value_precise_t)(TR_X0 + TR_W), (lv_value_precise_t)y};
    lv_obj_t* g = lv_line_create(p);
    lv_line_set_points(g, s_grid[i], 2);
    lv_obj_set_style_line_width(g, 1, 0);
    lv_obj_set_style_line_color(g, lv_color_hex(C_UNLIT), 0);
    lv_obj_set_style_line_dash_width(g, 2, 0);
    lv_obj_set_style_line_dash_gap(g, 4, 0);
  }
  ui_label(p, f11, C_DIM, 0, tr_y(100) + 4, "100");
  ui_label(p, f11, C_DIM, 0, tr_y(50) + 4, " 50");
  ui_label(p, f11, C_DIM, 0, tr_y(0) + 4, "  0");

  s.hist = lv_line_create(p);
  lv_obj_set_style_line_width(s.hist, 2, 0);
  lv_obj_set_style_line_color(s.hist, lv_color_hex(C_TITLE), 0);
  lv_obj_set_style_line_rounded(s.hist, true, 0);

  s.proj = lv_line_create(p);
  lv_obj_set_style_line_width(s.proj, 2, 0);
  lv_obj_set_style_line_color(s.proj, lv_color_hex(C_DIM), 0);
  lv_obj_set_style_line_dash_width(s.proj, 5, 0);
  lv_obj_set_style_line_dash_gap(s.proj, 4, 0);

  s.dot = rrect(p, 0, 0, 7, 7, 4, C_TEXT);
  lv_obj_add_flag(s.dot, LV_OBJ_FLAG_HIDDEN);

  // horários do eixo X (início da janela / reset)
  const int axis = tr_y(0) + 16;
  s.t0 = ui_label(p, f11, C_DIM, TR_X0, axis);
  s.t1 = ui_label(p, f11, C_DIM, 0, axis);
  lv_obj_align(s.t1, LV_ALIGN_TOP_RIGHT, -4, axis - ui_ascent(f11));

  s.cap = ui_label(p, f11, C_DIM, 0, 186);
  lv_obj_set_width(s.cap, SHELL_CONTENT_W);
  s.tok = ui_label(p, f11, C_DIM, 0, 204);
  lv_obj_set_width(s.tok, SHELL_CONTENT_W);
  screen_trend_refresh();
}

void screen_trend_refresh() {
  if (!s.hist) return;
  const AccountState& a = acct[g_accts.active];
  int histN, histHead;
  const Sample* hist = hist_data(a, &histN, &histHead);
  const uint32_t now = (uint32_t)time(nullptr);
  const uint32_t we = a.usage.h5ResetEpoch;
  const bool clockOk = now > 1000000000UL && we != 0;
  const bool hasData = a.keyOk && a.usageAtEpoch != 0;
  char b[96], t0[12], t1[12];

  // % da 5h no canto, na cor do nível
  if (hasData) {
    snprintf(b, sizeof(b), "%d%%", (int)(a.usage.h5 + 0.5f));
    ui_set_text(s.pct, b);
    ui_set_color(s.pct, level_color(level_for(a.usage.h5)));
  } else {
    ui_set_text(s.pct, "--%");
    ui_set_color(s.pct, C_DIM);
  }

  // tokens do bridge
  char tk[64];
  tok_text(a.tok, tk, sizeof(tk));
  ui_set_text(s.tok, tk);

  ProjResult pr = projection(hist, histN, histHead, a.usage.h5, we, now);
  projection_text(pr, g_lang, (int32_t)g_tzOffset * 3600, 60, b, sizeof(b));
  ui_set_text(s.cap, b);
  uint32_t pc = C_DIM;
  if (pr.st == ProjResult::OK_ATE_RESET) pc = C_OK_INK;
  else if (pr.st == ProjResult::ESGOTA || pr.st == ProjResult::ESGOTADA) pc = C_BAD_INK;
  ui_set_color(s.cap, pc);

  if (!clockOk || !hasData) {
    lv_line_set_points(s.hist, s_pts, 0);
    lv_line_set_points(s.proj, s_projPts, 0);
    lv_obj_add_flag(s.dot, LV_OBJ_FLAG_HIDDEN);
    ui_set_text(s.t0, "--:--");
    ui_set_text(s.t1, "--:--");
    return;
  }
  const uint32_t ws = we - 5 * 3600;
  fmt_hm(ws, t0, sizeof(t0));
  fmt_hm(we, t1, sizeof(t1));
  ui_set_text(s.t0, t0);
  ui_set_text(s.t1, t1);

  // histórico dentro da janela + leitura mais recente
  int n = 0;
  for (int i = 0; i < histN && n < HIST_MAX; i++) {
    Sample sm = hist[(histHead - histN + i + HIST_MAX * 2) % HIST_MAX];
    if (sm.t == 0 || sm.t < ws || sm.t > now) continue;
    s_pts[n].x = tr_x(sm.t, ws, we);
    s_pts[n].y = tr_y(sm.h5);
    n++;
  }
  const uint32_t nowC = now > we ? we : now;
  if (n < HIST_MAX) {
    s_pts[n].x = tr_x(nowC, ws, we);
    s_pts[n].y = tr_y(a.usage.h5);
    n++;
  }
  lv_line_set_points(s.hist, s_pts, n);

  const int cx = tr_x(nowC, ws, we), cy = tr_y(a.usage.h5);
  lv_obj_set_pos(s.dot, cx - 3, cy - 3);
  lv_obj_clear_flag(s.dot, LV_OBJ_FLAG_HIDDEN);

  if (pr.st == ProjResult::ESGOTA) {
    s_projPts[0] = {(lv_value_precise_t)cx, (lv_value_precise_t)cy};
    s_projPts[1] = {(lv_value_precise_t)tr_x(pr.etaEpoch, ws, we), (lv_value_precise_t)tr_y(100)};
    lv_obj_set_style_line_color(s.proj, lv_color_hex(C_BAD), 0);
    lv_line_set_points(s.proj, s_projPts, 2);
  } else if (pr.st == ProjResult::OK_ATE_RESET && pr.rate > 0.02f) {
    s_projPts[0] = {(lv_value_precise_t)cx, (lv_value_precise_t)cy};
    s_projPts[1] = {(lv_value_precise_t)tr_x(we, ws, we), (lv_value_precise_t)tr_y(pr.pctAtReset)};
    lv_obj_set_style_line_color(s.proj, lv_color_hex(C_OK), 0);
    lv_line_set_points(s.proj, s_projPts, 2);
  } else {
    lv_line_set_points(s.proj, s_projPts, 0);
  }
}

void screen_trend_reset() {
  memset(&s, 0, sizeof(s));
}
