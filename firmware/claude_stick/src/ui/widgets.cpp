// Peças de interface compartilhadas (item 39). Ver widgets.h.
#include "ui/widgets.h"
#include <Arduino.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "ui/i18n.h"
#include "ui/shell.h"
#include "ui/assets_thep.h"
#include "app/app_state.h"
#include "model/accounts.h"

extern AccountSlots g_accts;

// ---- Helpers da v2.3 ----
lv_obj_t* mklabel(lv_obj_t* p, const char* txt, const lv_font_t* font, uint32_t color) {
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}

void no_box(lv_obj_t* o) {
  lv_obj_set_style_bg_opa(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* mkbtn(lv_obj_t* p, const char* txt, const lv_font_t* font, uint32_t bg, uint32_t fg) {
  lv_obj_t* b = lv_button_create(p);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_SELECTED), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_center(mklabel(b, txt, font, fg));
  return b;
}

lv_obj_t* tlabel(lv_obj_t* p, const lv_font_t* f, uint32_t c, int x, int y) {
  lv_obj_t* l = mklabel(p, "", f, c);
  lv_obj_set_pos(l, x, y);
  return l;
}

lv_obj_t* tstatic(lv_obj_t* p, const char* txt, const lv_font_t* f, uint32_t c, int x, int y) {
  lv_obj_t* l = mklabel(p, txt, f, c);
  lv_obj_set_pos(l, x, y);
  return l;
}

lv_obj_t* rrect(lv_obj_t* p, int x, int y, int w, int h, int r, uint32_t col) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

lv_obj_t* mkchip(lv_obj_t* p, int x, int y) {
  lv_obj_t* o = lv_obj_create(p);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, LV_SIZE_CONTENT, 22);
  lv_obj_set_style_radius(o, 4, 0);
  lv_obj_set_style_pad_hor(o, 8, 0);
  lv_obj_set_style_border_width(o, 1, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* l = lv_label_create(o);
  lv_obj_set_style_text_font(l, theme_font(FONT_BOLD_13), 0);
  lv_label_set_text(l, "");
  lv_obj_center(l);
  return o;
}

void set_chip(lv_obj_t* o, const char* txt, uint32_t col) {
  if (!o) return;
  lv_color_t c = lv_color_hex(col);
  lv_obj_set_style_border_color(o, c, 0);
  lv_obj_set_style_bg_color(o, lv_color_mix(c, lv_color_hex(C_SURFACE), 40), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_t* l = lv_obj_get_child(o, 0);
  if (l) {
    lv_label_set_text(l, txt[0] ? txt : "--");
    lv_obj_set_style_text_color(l, c, 0);
  }
}

void fmt_eta(uint32_t epoch, char* out, int sz) {
  time_t now = time(nullptr);
  if (now < 1000000000L || epoch == 0) { snprintf(out, sz, "--"); return; }
  long d = (long)epoch - (long)now;
  if (d <= 0) { snprintf(out, sz, "%s", TRS("agora", "now")); return; }
  int days = d / 86400; d %= 86400;
  int hrs = d / 3600;   d %= 3600;
  int mins = d / 60;
  if (days > 0)     snprintf(out, sz, "%dd %dh", days, hrs);
  else if (hrs > 0) snprintf(out, sz, "%dh %02dm", hrs, mins);
  else              snprintf(out, sz, "%dm", mins);
}

void fmt_hm(uint32_t epoch, char* out, int sz) {
  if (epoch == 0 || time(nullptr) < 1000000000L) { strlcpy(out, "--:--", sz); return; }
  time_t t = (time_t)epoch;
  struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(out, sz, "%H:%M", &tmv);
}

void fmt_tok(long long v, char* out, size_t sz) {
  if (v >= 100000000LL)     snprintf(out, sz, "%lldM", v / 1000000LL);
  else if (v >= 1000000LL)  snprintf(out, sz, "%.1fM", v / 1e6);
  else if (v >= 10000LL)    snprintf(out, sz, "%lldk", v / 1000LL);
  else if (v >= 1000LL)     snprintf(out, sz, "%.1fk", v / 1e3);
  else                      snprintf(out, sz, "%lld", v);
}

bool tok_text(const TokenStats& t, char* out, size_t sz) {
  if (sz) out[0] = 0;
  if (t.atMs == 0 || millis() - t.atMs > TOK_FRESH_MS) return false;
  char in[24], o[24];
  fmt_tok(t.tin, in, sizeof(in));
  fmt_tok(t.tout, o, sizeof(o));
  snprintf(out, sz, tr(T::CARD_TOK_FMT), in, o);
  return true;
}

// ---- Visual Hermes ----
int ui_ascent(const lv_font_t* f) { return f->line_height - f->base_line; }

int ui_text_w(const char* t, const lv_font_t* f) {
  return lv_text_get_width(t, (uint32_t)strlen(t), f, 0);
}

lv_obj_t* ui_label(lv_obj_t* p, const lv_font_t* f, uint32_t col, int x, int base, const char* txt) {
  lv_obj_t* l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
  lv_label_set_text(l, txt);
  lv_obj_set_pos(l, x, base - ui_ascent(f));
  return l;
}

void ui_set_text(lv_obj_t* l, const char* t) {
  if (l && strcmp(lv_label_get_text(l), t) != 0) lv_label_set_text(l, t);
}

void ui_set_color(lv_obj_t* l, uint32_t col) {
  if (!l) return;
  lv_color_t c = lv_color_hex(col);
  if (!lv_color_eq(lv_obj_get_style_text_color(l, LV_PART_MAIN), c))
    lv_obj_set_style_text_color(l, c, 0);
}

lv_obj_t* ui_button(lv_obj_t* p, const char* txt, uint32_t fg, int w, int h) {
  lv_obj_t* b = lv_obj_create(p);
  lv_obj_remove_style_all(b);
  if (w > 0) lv_obj_set_size(b, w, h);
  else {
    lv_obj_set_size(b, LV_SIZE_CONTENT, h);
    lv_obj_set_style_min_width(b, 84, 0);
    lv_obj_set_style_pad_hor(b, 12, 0);
  }
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_radius(b, 6, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_SELECTED), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(b, 4);
  lv_obj_t* l = mklabel(b, txt, theme_font(FONT_BOLD_13), fg);
  lv_obj_center(l);
  return b;
}

lv_obj_t* ui_list(lv_obj_t* p, int w, int h) {
  lv_obj_t* lst = lv_obj_create(p);
  lv_obj_remove_style_all(lst);
  lv_obj_set_pos(lst, 0, 0);
  lv_obj_set_size(lst, w, h);
  lv_obj_set_style_pad_row(lst, 2, 0);
  lv_obj_set_style_pad_right(lst, 6, 0);       // espaço da barra de rolagem
  lv_obj_set_flex_flow(lst, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_scroll_dir(lst, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(lst, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_style_width(lst, 3, LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_color(lst, lv_color_hex(C_UNLIT), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(lst, LV_OPA_COVER, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(lst, 2, LV_PART_SCROLLBAR);
  return lst;
}

lv_obj_t* ui_row(lv_obj_t* list, const char* icon, const char* text, uint32_t fg,
                 const char* value, lv_event_cb_t cb, void* user, lv_obj_t** outVal) {
  lv_obj_t* r = lv_obj_create(list);
  lv_obj_remove_style_all(r);
  lv_obj_set_size(r, lv_pct(100), 36);
  lv_obj_set_style_radius(r, 4, 0);
  lv_obj_set_style_bg_color(r, lv_color_hex(C_SELECTED), 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_TRANSP, 0);
  lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
  lv_obj_set_style_pad_hor(r, 8, 0);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(r, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  if (cb) {
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, user);
  } else {
    lv_obj_remove_flag(r, LV_OBJ_FLAG_CLICKABLE);
  }
  const lv_font_t* f13 = theme_font(FONT_13);
  int x = 0;
  if (icon) {
    lv_obj_t* il = mklabel(r, icon, f13, C_DIM);   // LV_SYMBOL_* pela Montserrat 14 (fallback)
    lv_obj_align(il, LV_ALIGN_LEFT_MID, 0, 0);
    x = 24;
  }
  lv_obj_t* tl = mklabel(r, text, f13, fg);
  lv_obj_align(tl, LV_ALIGN_LEFT_MID, x, 0);
  lv_obj_t* vl = nullptr;
  if (value) {
    vl = mklabel(r, value, theme_font(FONT_BOLD_13), C_TITLE);
    lv_obj_align(vl, LV_ALIGN_RIGHT_MID, 0, 0);
  }
  if (outVal) *outVal = vl ? vl : tl;
  return r;
}

void ui_style_textarea(lv_obj_t* ta) {
  lv_obj_set_style_text_font(ta, theme_font(FONT_13), 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(C_TEXT), 0);
  lv_obj_set_style_bg_color(ta, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ta, 1, 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_color(ta, lv_color_hex(C_TITLE), LV_STATE_FOCUSED);
  lv_obj_set_style_radius(ta, 6, 0);
  lv_obj_set_style_text_color(ta, lv_color_hex(C_DIM), LV_PART_TEXTAREA_PLACEHOLDER);
  lv_obj_set_style_bg_color(ta, lv_color_hex(C_TITLE), LV_PART_CURSOR);
  lv_obj_set_style_border_color(ta, lv_color_hex(C_TITLE), LV_PART_CURSOR);
}

void ui_style_keyboard(lv_obj_t* kb) {
  lv_obj_set_style_bg_color(kb, lv_color_hex(C_STATUSBAR), 0);
  lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(kb, 1, 0);
  lv_obj_set_style_border_side(kb, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_color(kb, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_pad_all(kb, 4, 0);
  lv_obj_set_style_pad_gap(kb, 4, 0);
  lv_obj_set_style_text_font(kb, theme_font(FONT_13), LV_PART_ITEMS);
  lv_obj_set_style_text_color(kb, lv_color_hex(C_TEXT), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, lv_color_hex(C_SURFACE), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(kb, lv_color_hex(C_SELECTED), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(kb, lv_color_hex(C_SELECTED), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_color(kb, lv_color_hex(C_TITLE), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(kb, lv_color_hex(C_UNLIT), LV_PART_ITEMS);
  lv_obj_set_style_radius(kb, 4, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
}

lv_obj_t* ui_frame(const char* title) {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

  // cabeçalho de 50 px: só o logo (mesmo lugar do shell)
  lv_obj_t* logo = lv_image_create(scr);
  lv_image_set_src(logo, &img_thep_logo_hdr);
  lv_obj_set_pos(logo, 10, 6);

  // painel: x 8, y 56, 464 x 258 (margem de 6 px embaixo, sem rodapé)
  constexpr int PANEL_Y = 56;
  lv_obj_t* panel = lv_obj_create(scr);
  lv_obj_remove_style_all(panel);
  lv_obj_add_style(panel, &st_panel, 0);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(panel, 8, PANEL_Y);
  lv_obj_set_size(panel, SCREEN_WIDTH - 16, SCREEN_HEIGHT - PANEL_Y - 6);

  lv_obj_t* content = lv_obj_create(panel);
  lv_obj_remove_style_all(content);
  lv_obj_set_size(content, FRAME_CONTENT_W, FRAME_CONTENT_H);
  lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(content, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* t = lv_label_create(scr);
  lv_obj_add_style(t, &st_panel_title, 0);
  lv_label_set_text(t, title ? title : "");
  int lh = lv_font_get_line_height(theme_font(FONT_BOLD_13));
  lv_obj_align(t, LV_ALIGN_TOP_RIGHT, -(8 + 1 + 14), PANEL_Y - lh / 2);
  return content;
}

void ui_focus_title() {
  int a = g_accts.active;
  char t[ACCT_LBL_MAX + 16];
  if (a >= 0 && a < ACCT_MAX && g_accts.label[a][0]) snprintf(t, sizeof(t), "%s", g_accts.label[a]);
  else snprintf(t, sizeof(t), tr(T::ACCT_DEFAULT_FMT), a + 1);
  shell_set_title(t);
  if (accountCount(g_accts) > 1) shell_set_title_action(intent_focus_next);
}

const lv_font_t* ui_font_keypad() {
  static lv_font_t f;
  static bool init = false;
  if (!init) {
    init = true;
    f = *theme_font(FONT_NUM_30);
    f.fallback = &lv_font_montserrat_14;
  }
  return &f;
}
