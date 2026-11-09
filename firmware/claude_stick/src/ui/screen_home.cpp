// /agora (item 36): um card por slot salvo, nos 4 layouts do item 37 e do
// mockup-home.dc.html. Texto do card na precedência do Technical Plan §4.
//
// Posições por linha de base (como no CSS do mockup): cada texto fica com o
// topo em `base - ascent` da fonte, porque a altura de linha das JetBrains
// Mono geradas (11 -> 18 px, 13 -> 21 px) é bem maior que o corpo do texto.
//
// Os valores são reescritos no refresh (resultado da rede) e a cada segundo
// (contagens e idade). Só invalidam o objeto quando o texto ou a cor mudam
// (no modo FULL cada invalidação custa um quadro inteiro).
#include <Arduino.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "ui/screens.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/widgets.h"
#include "ui/i18n.h"
#include "ui/assets_thep.h"
#include "app/app_state.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "model/history.h"
#include "logic/projection.h"
#include "logic/levels.h"
#include "logic/reset_text.h"

extern AccountSlots g_accts;
extern int g_tzOffset;

namespace {

// ---- Tabela de campos por layout (item 37) ----
struct LayoutSpec {
  int16_t cardW, cardH;     // largura do card (item 36) e altura
  uint8_t labelMax;         // rótulo, em caracteres (fit_label)
  uint8_t bar5Cells;        // barra da 5h, sem contar os colchetes
  FontRole bar5Font;
  uint8_t bar7Cells;        // barra da semana (0 = sem barra)
  uint8_t projMax;          // maxChars da projeção = cardW / 6,6 px
  ResetFmt r5Fmt;
  const Txt* r5Key;         // nullptr = só o valor ("1h12m · 16:40", layout 3)
  const Txt* wkKey;         // nullptr = seção "Semana" (layout 1)
  const Txt* r7Key;         // nullptr = sem reset da semana (layout 4)
};

const LayoutSpec SPEC[4] = {
  // 1 conta: coluna direita de 272 px ao lado do pássaro
  {272, 214, 16, 18, FONT_13, 10, 41, RESET_IN_AT, &T::CARD_RESET, nullptr, &T::CARD_RESET},
  // 2 colunas
  // texto secundário em 13 px (teste pedido pelo dono): ~7,8 px por caractere,
  // então a projeção cabe em 26 caracteres nos 210 px do card
  {210, 214, 16, 14, FONT_13, 8, 26, RESET_IN_AT, &T::CARD_RESET, &T::CARD_WEEK, &T::CARD_RESET},
  // 3 colunas: rótulo de 10 para caber com a marca de 8 (tools/gen_mockups.py)
  {138, 214, 10, 9, FONT_11, 0, 20, RESET_IN_AT, nullptr, &T::CARD_WEEK_S, &T::CARD_RESET_S},
  // 2 x 2
  {210, 103, 14, 14, FONT_11, 0, 31, RESET_AT, &T::CARD_RESET_S, &T::CARD_WEEK_XS, nullptr},
};

// Posição dos cards por layout (x, y), a partir do item 36.
const int16_t POS2[2][2] = {{0, 0}, {228, 0}};
const int16_t POS3[3][2] = {{0, 0}, {150, 0}, {300, 0}};
const int16_t POS4[4][2] = {{0, 0}, {228, 0}, {0, 111}, {228, 111}};
constexpr int L1_COL_X = 166;   // coluna direita do layout 1 (150 + gap 16)

// ---- Grupo de spans de uma cor por trecho (barra, "chave: valor") ----
struct Spans {
  lv_obj_t* g;
  lv_span_t* sp[4];
  uint32_t h[4];      // hash de texto + cor de cada trecho (0 = ainda vazio)
  uint8_t n;
  bool dirty;
};

struct Card {
  int slot;
  lv_obj_t* box;
  lv_obj_t* label;
  Spans mark;         // layout 1: "status: " + marca; 2 e 3: só a marca
  lv_obj_t* key5;     // layout 4: "5h" ao lado da %
  lv_obj_t* pct;
  Spans bar5;
  Spans r5;           // chave + valor
  lv_obj_t* wkKey;    // layouts 2 a 4
  lv_obj_t* wkPct;
  Spans bar7;         // layouts 1 e 2
  Spans r7;           // layouts 1 a 3
  lv_obj_t* proj;
  lv_obj_t* tok;      // layout 1
};

Card s_card[ACCT_MAX];
int s_n = 0;
LayoutKind s_layout = LAYOUT_1;
uint8_t s_mask = 0;        // slots salvos quando a tela foi montada
bool s_built = false;
int16_t s_wkKeyW = 0;      // largura da chave da semana (a % vem logo depois)
int16_t s_key5W = 0;       // layout 4: largura de "5h "

uint8_t used_mask() {
  uint8_t m = 0;
  for (int s = 0; s < ACCT_MAX; s++) if (g_accts.used[s]) m |= (uint8_t)(1u << s);
  return m;
}

// Alinhado à direita do pai (o LVGL refaz o x quando a largura muda).
void align_right(lv_obj_t* o, const lv_font_t* f, int base) {
  lv_obj_align(o, LV_ALIGN_TOP_RIGHT, 0, base - ui_ascent(f));
}

uint32_t hash_of(const char* s, uint32_t col) {
  uint32_t h = 2166136261u;
  for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
  h ^= col; h *= 16777619u;
  return h | 1u;
}

void spans_make(Spans& s, lv_obj_t* p, int n, const lv_font_t* f, int x, int base) {
  memset(&s, 0, sizeof(s));
  s.g = lv_spangroup_create(p);
  lv_obj_remove_flag(s.g, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_text_font(s.g, f, 0);
  lv_spangroup_set_mode(s.g, LV_SPAN_MODE_EXPAND);
  lv_obj_set_pos(s.g, x, base - ui_ascent(f));
  s.n = (uint8_t)(n > 4 ? 4 : n);
  for (int i = 0; i < s.n; i++) s.sp[i] = lv_spangroup_new_span(s.g);
}
void spans_set(Spans& s, int i, const char* t, uint32_t col) {
  if (!s.g || i >= s.n) return;
  uint32_t h = hash_of(t, col);
  if (s.h[i] == h) return;
  s.h[i] = h;
  lv_span_set_text(s.sp[i], t);
  lv_style_set_text_color(lv_span_get_style(s.sp[i]), lv_color_hex(col));
  s.dirty = true;
}
void spans_commit(Spans& s) {
  if (!s.g || !s.dirty) return;
  s.dirty = false;
  lv_spangroup_refr_mode(s.g);
  lv_obj_invalidate(s.g);
}

// "[", blocos acesos na cor do nível, "░" em C_UNLIT, "]" (mockup).
void bar_make(Spans& s, lv_obj_t* p, const lv_font_t* f, int x, int base) {
  spans_make(s, p, 4, f, x, base);
  spans_set(s, 0, "[", C_DIM);
  spans_set(s, 3, "]", C_DIM);
}
void bar_set(Spans& s, float pct, int cells, uint32_t col) {
  char all[3 * 20 + 1], on[sizeof(all)];
  text_bar(pct, cells, all, sizeof(all));
  const char* off = text_bar_split(all, on, sizeof(on));
  spans_set(s, 1, on, col);
  spans_set(s, 2, off, C_UNLIT);
  spans_commit(s);
}
void kv_set(Spans& s, const char* key, const char* val, uint32_t col) {
  spans_set(s, 0, key, C_DIM);
  spans_set(s, 1, val, col);
  spans_commit(s);
}

void key_text(const Txt* k, char* out, size_t sz) {
  if (k) snprintf(out, sz, "%s ", tr(*k));
  else out[0] = 0;
}

void card_click_cb(lv_event_t* e) {
  intent_focus((int)(intptr_t)lv_event_get_user_data(e));
}

lv_obj_t* card_box(lv_obj_t* p, int slot, int x, int y, int w, int h, bool focus) {
  lv_obj_t* b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, card_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)slot);
  if (focus) {   // contorno de 1 px fora do card (não come o conteúdo)
    lv_obj_set_style_radius(b, 4, 0);
    lv_obj_set_style_outline_width(b, 1, 0);
    lv_obj_set_style_outline_color(b, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_outline_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_pad(b, 3, 0);
  }
  return b;
}

// ---- Montagem por layout ----
void build_l1(Card& c, lv_obj_t* box) {
  const LayoutSpec& S = SPEC[LAYOUT_1];
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* f13 = theme_font(FONT_13);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);
  const lv_font_t* fn = theme_font_num(LAYOUT_1);
  char t[48];

  // coluna esquerda (150 px): pássaro, rótulo e marca de estado
  lv_obj_t* bird = lv_image_create(box);
  lv_image_set_src(bird, &img_bird_braille_124);
  lv_obj_remove_flag(bird, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(bird, (150 - 124) / 2, 18);
  account_label(c.slot, S.labelMax, t, sizeof(t));
  c.label = ui_label(box, fb, C_ACCENT, 0, 160, t);
  lv_obj_set_width(c.label, 150);
  lv_obj_set_style_text_align(c.label, LV_TEXT_ALIGN_CENTER, 0);
  spans_make(c.mark, box, 2, f11, 0, 178);
  lv_spangroup_set_mode(c.mark.g, LV_SPAN_MODE_FIXED);
  lv_spangroup_set_align(c.mark.g, LV_TEXT_ALIGN_CENTER);
  lv_obj_set_size(c.mark.g, 150, f11->line_height);

  // coluna direita: seções Janela 5h e Semana
  const int X = L1_COL_X;
  ui_label(box, fb, C_ACCENT, X, 12, tr(T::CARD_SEC_5H));
  c.pct = ui_label(box, fn, C_DIM, X, 56);
  bar_make(c.bar5, box, f13, X, 76);
  spans_make(c.r5, box, 2, f11, X, 93);
  ui_label(box, fb, C_ACCENT, X, 116, tr(T::CARD_SEC_WEEK));
  c.wkPct = ui_label(box, fb, C_DIM, X, 134);
  bar_make(c.bar7, box, f11, X + 44, 134);
  spans_make(c.r7, box, 2, f11, X, 151);
  c.proj = ui_label(box, f11, C_DIM, X, 172);
  c.tok = ui_label(box, f11, C_DIM, X, 190);
}

// Layouts 2 e 3: colunas com rótulo + marca, "5h", %, barra, reset, semana,
// reset da semana e projeção.
void build_cols(Card& c, lv_obj_t* box, LayoutKind L) {
  const LayoutSpec& S = SPEC[L];
  const bool two = (L == LAYOUT_2);
  // Layout 2: texto secundário em 13 px e linhas mais espaçadas, ocupando a
  // altura do card (teste pedido pelo dono). Layout 3 continua em 11 px, que é
  // o que cabe em colunas de 138 px.
  const lv_font_t* ft = theme_font(two ? FONT_13 : FONT_11);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);
  const lv_font_t* fn = theme_font_num(L);
  const lv_font_t* fbar = theme_font(S.bar5Font);
  char t[48];

  // linhas de base (y) de cada linha do card
  const int yLabel = two ? 14 : 12;
  const int y5h    = two ? 36 : 28;
  const int yPct   = two ? 76 : 56;
  const int yBar5  = two ? 98 : 73;
  const int yR5    = two ? 118 : 89;
  const int yWk    = two ? 146 : 112;
  const int yR7    = two ? 166 : 128;
  const int yProj  = two ? 196 : 148;

  account_label(c.slot, S.labelMax, t, sizeof(t));
  c.label = ui_label(box, fb, C_ACCENT, 0, yLabel, t);
  spans_make(c.mark, box, 1, ft, 0, yLabel);
  align_right(c.mark.g, ft, yLabel);
  ui_label(box, ft, C_DIM, 0, y5h, "5h");
  c.pct = ui_label(box, fn, C_DIM, 0, yPct);
  bar_make(c.bar5, box, fbar, 0, yBar5);
  spans_make(c.r5, box, 2, ft, 0, yR5);
  key_text(S.wkKey, t, sizeof(t));
  c.wkKey = ui_label(box, ft, C_DIM, 0, yWk, t);
  s_wkKeyW = (int16_t)ui_text_w(t, ft);
  c.wkPct = ui_label(box, fb, C_DIM, s_wkKeyW, yWk);
  if (S.bar7Cells) {
    bar_make(c.bar7, box, ft, 0, yWk);
    align_right(c.bar7.g, ft, yWk);
  }
  spans_make(c.r7, box, 2, ft, 0, yR7);
  c.proj = ui_label(box, ft, C_DIM, 0, yProj);
}

// Layout 4 (2 x 2, 210 x 103): rótulo e "5h NN%" na mesma linha, barra,
// semana + reset da 5h, projeção.
void build_l4(Card& c, lv_obj_t* box) {
  const LayoutSpec& S = SPEC[LAYOUT_4];
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);
  const lv_font_t* fn = theme_font_num(LAYOUT_4);
  char t[48];

  account_label(c.slot, S.labelMax, t, sizeof(t));
  c.label = ui_label(box, fb, C_ACCENT, 0, 18, t);
  c.pct = ui_label(box, fn, C_DIM, 0, 18);
  align_right(c.pct, fn, 18);
  c.key5 = ui_label(box, f11, C_DIM, 0, 18, "5h ");
  s_key5W = (int16_t)ui_text_w("5h ", f11);
  bar_make(c.bar5, box, theme_font(S.bar5Font), 0, 40);
  key_text(S.wkKey, t, sizeof(t));
  c.wkKey = ui_label(box, f11, C_DIM, 0, 60, t);
  s_wkKeyW = (int16_t)ui_text_w(t, f11);
  c.wkPct = ui_label(box, fb, C_DIM, s_wkKeyW, 60);
  spans_make(c.r5, box, 2, f11, 0, 60);
  align_right(c.r5.g, f11, 60);
  c.proj = ui_label(box, f11, C_DIM, 0, 82);
}

// ---- Valores ----
enum CardMode : uint8_t { M_NOKEY, M_FAIL_NODATA, M_FAIL, M_PENDING, M_OK };

void card_update(Card& c, uint32_t now) {
  if (!c.box) return;
  const LayoutKind L = s_layout;
  const LayoutSpec& S = SPEC[L];
  const AccountState& a = acct[c.slot];
  const UsageData& u = a.usage;
  const int32_t tz = (int32_t)g_tzOffset * 3600;
  const bool clockOk = now >= 1000000000UL;
  const bool hasData = a.keyOk && a.usageAtEpoch != 0;
  const FetchClass k = a.lastClass;

  CardMode m;
  if (!a.keyOk) m = M_NOKEY;
  else if (app_slot_failed(c.slot)) m = hasData ? M_FAIL : M_FAIL_NODATA;
  else if (k == FC_NONE) m = M_PENDING;
  else m = M_OK;
  const bool show = hasData && (m == M_FAIL || m == M_OK);
  const bool dim = (m == M_FAIL);                       // últimos valores esmaecidos
  const bool passed5 = show && reset_passed(u.h5ResetEpoch, now);
  const uint32_t valCol = (show && !dim) ? C_TEXT : C_DIM;
  char t[64], v[48];

  // marca de estado da API
  Tone tone = TONE_DIM;
  char mb[16];
  const char* mk = show ? status_mark(u.statusOverall, g_lang, mb, sizeof(mb), &tone) : "";
  const uint32_t markCol = dim ? C_DIM : tone_color(tone);
  if (L == LAYOUT_1) {
    spans_set(c.mark, 0, tr(T::CARD_STATUS), C_DIM);
    spans_set(c.mark, 1, mk[0] ? mk : "\xE2\x80\x94", mk[0] ? markCol : C_DIM);
    spans_commit(c.mark);
  } else if (c.mark.g) {
    spans_set(c.mark, 0, mk, markCol);
    spans_commit(c.mark);
  }

  // % e barra da 5h (layout 4: a marca atenção/limite vira a cor da %)
  uint32_t c5 = C_DIM;
  if (show && !dim && !passed5) {
    c5 = level_color(level_for(u.h5));
    if (L == LAYOUT_4 && (tone == TONE_WARN || tone == TONE_BAD)) c5 = tone_color(tone);
  }
  if (show) snprintf(t, sizeof(t), "%d%%", (int)(u.h5 + 0.5f));
  else snprintf(t, sizeof(t), "%s", tr(T::CARD_NO_PCT));
  ui_set_text(c.pct, t);
  ui_set_color(c.pct, c5);
  if (c.key5) {
    const lv_font_t* fn = theme_font_num(LAYOUT_4);
    int x = S.cardW - ui_text_w(t, fn) - s_key5W;
    if (lv_obj_get_x(c.key5) != x) lv_obj_set_x(c.key5, x);
  }
  bar_set(c.bar5, show ? u.h5 : 0.0f, S.bar5Cells, c5);

  // reset da 5h
  char key[24];
  key_text(S.r5Key, key, sizeof(key));
  if (show) reset_text(u.h5ResetEpoch, now, tz, g_lang, S.r5Fmt, v, sizeof(v));
  else snprintf(v, sizeof(v), "--");
  kv_set(c.r5, key, v, valCol);

  // semana
  uint32_t c7 = (show && !dim) ? level_color(level_for(u.d7)) : C_DIM;
  if (show) snprintf(t, sizeof(t), "%d%%", (int)(u.d7 + 0.5f));
  else snprintf(t, sizeof(t), "%s", tr(T::CARD_NO_PCT));
  ui_set_text(c.wkPct, t);
  ui_set_color(c.wkPct, c7);
  if (S.bar7Cells) bar_set(c.bar7, show ? u.d7 : 0.0f, S.bar7Cells, c7);
  if (S.r7Key) {
    key_text(S.r7Key, key, sizeof(key));
    if (show) reset_text(u.d7ResetEpoch, now, tz, g_lang, RESET_WEEKDAY_AT, v, sizeof(v));
    else snprintf(v, sizeof(v), "--");
    kv_set(c.r7, key, v, valCol);
  }

  // linha de projeção / estado (Technical Plan §4)
  uint32_t pc = C_DIM;
  switch (m) {
    case M_NOKEY:
      snprintf(t, sizeof(t), "%s", tr(T::CARD_NO_KEY));
      pc = C_BAD_INK;
      break;
    case M_FAIL_NODATA:
      snprintf(t, sizeof(t), tr(T::CARD_FAIL_ONLY), fetch_class_name(k));
      pc = C_BAD_INK;
      break;
    case M_FAIL: {
      char age[12];
      age_text(clockOk && now > a.usageAtEpoch ? now - a.usageAtEpoch : 0, age, sizeof(age));
      snprintf(t, sizeof(t), tr(T::CARD_FAIL_FMT), fetch_class_name(k), age);
      pc = C_BAD_INK;
      break;
    }
    case M_PENDING:
      snprintf(t, sizeof(t), "%s", tr(T::CARD_QUERYING));
      break;
    case M_OK: {
      int n, head;
      const Sample* hist = hist_data(a, &n, &head);
      ProjResult pr = projection(hist, n, head, u.h5, u.h5ResetEpoch, now);
      projection_text(pr, g_lang, tz, S.projMax, t, sizeof(t));
      if (pr.st == ProjResult::OK_ATE_RESET) pc = C_OK_INK;
      else if (pr.st == ProjResult::ESGOTA || pr.st == ProjResult::ESGOTADA) pc = C_BAD_INK;
      break;
    }
  }
  fit_label(t, S.projMax, v, sizeof(v));
  ui_set_text(c.proj, v);
  ui_set_color(c.proj, pc);

  // tokens do bridge (layout 1)
  if (c.tok) {
    if (tok_text(a.tok, t, sizeof(t))) {
      fit_label(t, S.projMax, v, sizeof(v));
      ui_set_text(c.tok, v);
    } else {
      ui_set_text(c.tok, "");
    }
  }
}

void update_all() {
  if (!s_built) return;
  if (used_mask() != s_mask) {   // conta adicionada ou removida: monta de novo
    s_built = false;
    shell_show(ContentId::HOME);
    return;
  }
  uint32_t now = (uint32_t)time(nullptr);
  for (int i = 0; i < s_n; i++) card_update(s_card[i], now);
}

}  // namespace

void screen_home_build(lv_obj_t* parent) {
  memset(s_card, 0, sizeof(s_card));
  s_n = 0;
  s_mask = used_mask();
  int na = accountCount(g_accts);
  s_layout = layout_for_count(na);

  // título: "THEP Usage v3.0 · N contas" (mockup)
  char n[24], t[48];
  if (na == 1) snprintf(n, sizeof(n), "%s", tr(T::FOOT_ACCT_ONE));
  else snprintf(n, sizeof(n), tr(T::FOOT_ACCT_FMT), na);
  snprintf(t, sizeof(t), "THEP Usage v" FW_VERSION " \xC2\xB7 %s", n);
  shell_set_title(t);

  // o contorno da conta em foco fica 3 px fora do card
  lv_obj_add_flag(parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  for (int s = 0; s < ACCT_MAX && s_n < 4; s++) {
    if (!g_accts.used[s]) continue;
    int i = s_n++;
    Card& c = s_card[i];
    c.slot = s;
    bool focus = na > 1 && s == g_accts.active;
    switch (s_layout) {
      case LAYOUT_1:
        c.box = card_box(parent, s, 0, 0, SHELL_CONTENT_W, SHELL_CONTENT_H, false);
        build_l1(c, c.box);
        break;
      case LAYOUT_2:
        c.box = card_box(parent, s, POS2[i][0], POS2[i][1], SPEC[1].cardW, SPEC[1].cardH, focus);
        build_cols(c, c.box, LAYOUT_2);
        break;
      case LAYOUT_3:
        c.box = card_box(parent, s, POS3[i][0], POS3[i][1], SPEC[2].cardW, SPEC[2].cardH, focus);
        build_cols(c, c.box, LAYOUT_3);
        break;
      case LAYOUT_4:
        c.box = card_box(parent, s, POS4[i][0], POS4[i][1], SPEC[3].cardW, SPEC[3].cardH, focus);
        build_l4(c, c.box);
        break;
    }
  }
  s_built = true;
  update_all();
}

void screen_home_refresh() { update_all(); }
void screen_home_tick() { update_all(); }

void screen_home_reset() {
  s_built = false;
  s_n = 0;
  memset(s_card, 0, sizeof(s_card));
}
