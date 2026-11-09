// /modelos (item 38): lista em texto no estilo Hermes, uma linha por modelo
// (nome em 9 colunas + model_status_text na cor do tom) e a linha do
// status.claude.com. Substitui os mascotes Clawd da v2.3.
//
// A sonda de cada modelo vem do USAGE da conta em foco (rodízio no
// agendador), por isso a seção mostra o rótulo dela; tocar na seção passa
// para a próxima conta (Technical Plan §3).
#include <Arduino.h>
#include <string.h>
#include <stdio.h>

#include "ui/screens.h"
#include "ui/theme.h"
#include "ui/i18n.h"
#include "app/app_state.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "logic/classify.h"
#include "logic/levels.h"

extern AccountSlots g_accts;

namespace {

lv_obj_t* s_status[NMODELS];
lv_obj_t* s_inc = nullptr;
bool s_built = false;

int ascent(const lv_font_t* f) { return f->line_height - f->base_line; }

lv_obj_t* mk_label(lv_obj_t* p, const lv_font_t* f, uint32_t col, int x, int base,
                   const char* txt = "") {
  lv_obj_t* l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_label_set_text(l, txt);
  lv_obj_set_pos(l, x, base - ascent(f));
  return l;
}

void lbl_set(lv_obj_t* l, const char* t, uint32_t col) {
  if (!l) return;
  if (strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
  lv_color_t c = lv_color_hex(col);
  if (!lv_color_eq(lv_obj_get_style_text_color(l, LV_PART_MAIN), c))
    lv_obj_set_style_text_color(l, c, 0);
}

void section_click_cb(lv_event_t* e) {
  (void)e;
  intent_focus_next();
}

void update() {
  if (!s_built) return;
  for (int i = 0; i < NMODELS; i++) {
    char t[24];
    Tone tone = TONE_DIM;
    model_status_text(g_models[i].pr.code, g_models[i].pr.ms, g_lang, t, sizeof(t), &tone);
    lbl_set(s_status[i], t, tone_color(tone));
  }
  // status.claude.com: mesma regra do dialog de incidente (incident_sync)
  char t[96], who[64];
  bool inc = g_status.ok &&
             !(g_status.haikuUp && g_status.sonnetUp && g_status.opusUp && g_status.fableUp);
  if (!g_status.ok) {
    lbl_set(s_inc, "status.claude.com: \xE2\x80\x94", C_DIM);
  } else if (!inc) {
    lbl_set(s_inc, tr(T::MODELS_NO_INC), C_DIM);
  } else {
    const char* sev = severity_label(g_status.severity);
    if (affected_models(who, sizeof(who)))
      snprintf(t, sizeof(t), tr(T::MODELS_INC_MODELS_FMT), sev, who);
    else
      snprintf(t, sizeof(t), tr(T::MODELS_INC_FMT), sev);
    char fit[96];
    fit_label(t, 66, fit, sizeof(fit));   // 438 px / 6,6 px da jbmono_11
    lbl_set(s_inc, fit, g_status.severity >= SEV_MAJOR ? C_BAD_INK : C_WARN);
  }
}

}  // namespace

void screen_models_build(lv_obj_t* parent) {
  memset(s_status, 0, sizeof(s_status));
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* f13 = theme_font(FONT_13);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);

  // "Modelos · <conta em foco>"
  int a = g_accts.active;
  char lbl[ACCT_LBL_MAX + 16], t[64];
  if (a >= 0 && a < ACCT_MAX && g_accts.label[a][0]) snprintf(lbl, sizeof(lbl), "%s", g_accts.label[a]);
  else snprintf(lbl, sizeof(lbl), tr(T::ACCT_DEFAULT_FMT), a + 1);
  snprintf(t, sizeof(t), tr(T::MODELS_TITLE_FMT), lbl);
  lv_obj_t* sec = mk_label(parent, fb, C_ACCENT, 0, 14, t);
  if (accountCount(g_accts) > 1) {
    lv_obj_add_flag(sec, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(sec, 8);
    lv_obj_add_event_cb(sec, section_click_cb, LV_EVENT_CLICKED, nullptr);
  }

  // uma linha por modelo: nome em 9 colunas + estado
  const int statusX = lv_text_get_width("123456789", 9, f13, 0);
  for (int i = 0; i < NMODELS; i++) {
    int base = 44 + i * 24;
    mk_label(parent, f13, C_TEXT, 0, base, g_models[i].name);
    s_status[i] = mk_label(parent, f13, C_DIM, statusX, base);
  }
  s_inc = mk_label(parent, f11, C_DIM, 0, 44 + NMODELS * 24 + 16);
  s_built = true;
  update();
}

void screen_models_refresh() { update(); }

void screen_models_reset() {
  s_built = false;
  memset(s_status, 0, sizeof(s_status));
  s_inc = nullptr;
}
