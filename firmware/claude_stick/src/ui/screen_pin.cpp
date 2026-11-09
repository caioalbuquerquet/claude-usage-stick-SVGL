// PIN e definição de PIN (item 39): tela cheia antes do desbloqueio, na
// moldura THEP. Esquerda: título, 4 casas do PIN e a mensagem; direita: o
// teclado. A lógica (tentativas, bloqueio, chave da sessão) fica no .ino, que
// recebe cada tecla e chama screen_pin_set_dots / screen_pin_set_msg.
#include <string.h>
#include "config.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/theme.h"

namespace {

const char* const KEYMAP[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    LV_SYMBOL_LEFT, "0", LV_SYMBOL_OK, ""};

constexpr int KP_W = 252;
constexpr int COL_W = FRAME_CONTENT_W - KP_W - 16;   // coluna de texto: 170 px
constexpr int CELL = 24, CELL_GAP = 10;

lv_obj_t* s_cell[PIN_LEN];
lv_obj_t* s_msg = nullptr;
void (*s_onKey)(const char*) = nullptr;
bool s_up = false;

void kb_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t* bm = (lv_obj_t*)lv_event_get_target(e);
  uint32_t id = lv_buttonmatrix_get_selected_button(bm);
  const char* txt = lv_buttonmatrix_get_button_text(bm, id);
  if (txt && s_onKey) s_onKey(txt);
}

void cell_fill(lv_obj_t* c, bool on) {
  lv_obj_set_style_bg_opa(c, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(on ? C_TITLE : C_BORDER), 0);
}

}  // namespace

void screen_pin_build(const char* title, const char* sub, void (*onKey)(const char*)) {
  s_onKey = onKey;
  lv_obj_t* p = ui_frame("THEP Usage v" FW_VERSION);
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);

  // título do estado ("Digite o PIN", "Defina um PIN"…)
  lv_obj_t* sec = ui_label(p, fb, C_ACCENT, 0, 16, title);
  lv_obj_set_width(sec, COL_W);

  for (int i = 0; i < PIN_LEN; i++) {
    lv_obj_t* c = lv_obj_create(p);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, i * (CELL + CELL_GAP), 36);
    lv_obj_set_size(c, CELL, CELL);
    lv_obj_set_style_radius(c, 4, 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_bg_color(c, lv_color_hex(C_TITLE), 0);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);
    cell_fill(c, false);
    s_cell[i] = c;
  }

  s_msg = mklabel(p, sub, f11, C_DIM);
  lv_obj_set_pos(s_msg, 0, 74);
  lv_obj_set_width(s_msg, COL_W);
  lv_label_set_long_mode(s_msg, LV_LABEL_LONG_WRAP);

  lv_obj_t* bm = lv_buttonmatrix_create(p);
  lv_buttonmatrix_set_map(bm, KEYMAP);
  lv_obj_set_size(bm, KP_W, FRAME_CONTENT_H);
  lv_obj_align(bm, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(bm, 0, 0);
  lv_obj_set_style_pad_all(bm, 0, 0);
  lv_obj_set_style_pad_gap(bm, 6, 0);
  lv_obj_set_style_text_font(bm, ui_font_keypad(), LV_PART_ITEMS);
  lv_obj_set_style_text_color(bm, lv_color_hex(C_TEXT), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(bm, lv_color_hex(C_SURFACE), LV_PART_ITEMS);
  lv_obj_set_style_bg_color(bm, lv_color_hex(C_SELECTED), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(bm, lv_color_hex(C_TITLE), (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_border_width(bm, 1, LV_PART_ITEMS);
  lv_obj_set_style_border_color(bm, lv_color_hex(C_BORDER), LV_PART_ITEMS);
  lv_obj_set_style_radius(bm, 6, LV_PART_ITEMS);
  lv_obj_set_style_shadow_width(bm, 0, LV_PART_ITEMS);
  lv_obj_add_event_cb(bm, kb_cb, LV_EVENT_VALUE_CHANGED, NULL);
  s_up = true;
}

bool screen_pin_up() { return s_up && s_msg; }

void screen_pin_set_dots(int filled) {
  if (!s_up) return;
  for (int i = 0; i < PIN_LEN; i++)
    if (s_cell[i]) cell_fill(s_cell[i], i < filled);
}

void screen_pin_set_msg(const char* msg) {
  if (s_msg) lv_label_set_text(s_msg, msg);
}

void screen_pin_set_msg_bad(bool bad) {
  if (s_msg) lv_obj_set_style_text_color(s_msg, lv_color_hex(bad ? C_BAD_INK : C_DIM), 0);
}

void screen_pin_reset() {
  s_up = false;
  s_msg = nullptr;
  memset(s_cell, 0, sizeof(s_cell));
}
