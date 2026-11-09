// Tema THEP (item 33). Ver theme.h.
#include "ui/theme.h"

lv_style_t st_panel;
lv_style_t st_panel_title;
lv_style_t st_section;
lv_style_t st_key;
lv_style_t st_value;
lv_style_t st_menu_row;
lv_style_t st_menu_row_sel;

uint32_t tone_color(Tone t) {
  switch (t) {
    case TONE_OK:   return C_OK_INK;
    case TONE_WARN: return C_WARN;
    case TONE_BAD:  return C_BAD_INK;
    case TONE_DIM:
    default:        return C_DIM;
  }
}

uint32_t level_color(Level l) {
  switch (l) {
    case LEVEL_WARN: return C_WARN;
    case LEVEL_BAD:  return C_BAD;
    case LEVEL_OK:
    default:         return C_OK;
  }
}

const lv_font_t* theme_font(FontRole r) {
#if THEME_HAS_JBMONO
  switch (r) {
    case FONT_11:      return &jbmono_11;
    case FONT_13:      return &jbmono_13;
    case FONT_BOLD_13: return &jbmono_bold_13;
    case FONT_NUM_20:  return &jbmono_bold_num_20;
    case FONT_NUM_30:  return &jbmono_bold_num_30;
    case FONT_NUM_38:  return &jbmono_bold_num_38;
    case FONT_NUM_50:  return &jbmono_bold_num_50;
  }
  return &jbmono_13;
#else
  // Sem as fontes geradas (item 30): tudo na Montserrat 14, a única que fica
  // no lv_conf.h (item 42).
  (void)r;
  return &lv_font_montserrat_14;
#endif
}

const lv_font_t* theme_font_num(LayoutKind k) {
  switch (k) {
    case LAYOUT_1: return theme_font(FONT_NUM_50);
    case LAYOUT_2: return theme_font(FONT_NUM_38);
    case LAYOUT_3: return theme_font(FONT_NUM_30);
    case LAYOUT_4:
    default:       return theme_font(FONT_NUM_20);
  }
}

void theme_init() {
  static bool done = false;
  if (done) return;
  done = true;

  lv_style_init(&st_panel);
  lv_style_set_bg_color(&st_panel, lv_color_hex(C_BG));
  lv_style_set_bg_opa(&st_panel, LV_OPA_COVER);
  lv_style_set_border_color(&st_panel, lv_color_hex(C_BORDER));
  lv_style_set_border_width(&st_panel, 1);
  lv_style_set_border_opa(&st_panel, LV_OPA_COVER);
  lv_style_set_radius(&st_panel, 8);
  lv_style_set_pad_top(&st_panel, 14);
  lv_style_set_pad_left(&st_panel, 12);
  lv_style_set_pad_right(&st_panel, 12);
  lv_style_set_pad_bottom(&st_panel, 8);
  lv_style_set_text_color(&st_panel, lv_color_hex(C_TEXT));

  lv_style_init(&st_panel_title);
  lv_style_set_bg_color(&st_panel_title, lv_color_hex(C_BG));
  lv_style_set_bg_opa(&st_panel_title, LV_OPA_COVER);
  lv_style_set_pad_left(&st_panel_title, 6);
  lv_style_set_pad_right(&st_panel_title, 6);
  lv_style_set_pad_top(&st_panel_title, 0);
  lv_style_set_pad_bottom(&st_panel_title, 0);
  lv_style_set_text_font(&st_panel_title, theme_font(FONT_BOLD_13));
  lv_style_set_text_color(&st_panel_title, lv_color_hex(C_TITLE));

  lv_style_init(&st_section);
  lv_style_set_text_font(&st_section, theme_font(FONT_BOLD_13));
  lv_style_set_text_color(&st_section, lv_color_hex(C_ACCENT));

  lv_style_init(&st_key);
  lv_style_set_text_font(&st_key, theme_font(FONT_11));
  lv_style_set_text_color(&st_key, lv_color_hex(C_DIM));

  lv_style_init(&st_value);
  lv_style_set_text_font(&st_value, theme_font(FONT_11));
  lv_style_set_text_color(&st_value, lv_color_hex(C_TEXT));

  lv_style_init(&st_menu_row);
  lv_style_set_height(&st_menu_row, 36);
  lv_style_set_radius(&st_menu_row, 4);
  lv_style_set_bg_opa(&st_menu_row, LV_OPA_TRANSP);
  lv_style_set_border_width(&st_menu_row, 0);
  lv_style_set_shadow_width(&st_menu_row, 0);
  lv_style_set_pad_left(&st_menu_row, 10);
  lv_style_set_pad_right(&st_menu_row, 10);
  lv_style_set_pad_top(&st_menu_row, 0);
  lv_style_set_pad_bottom(&st_menu_row, 0);
  lv_style_set_pad_column(&st_menu_row, 8);
  lv_style_set_text_font(&st_menu_row, theme_font(FONT_13));
  lv_style_set_text_color(&st_menu_row, lv_color_hex(C_TEXT));

  lv_style_init(&st_menu_row_sel);
  lv_style_set_bg_color(&st_menu_row_sel, lv_color_hex(C_SELECTED));
  lv_style_set_bg_opa(&st_menu_row_sel, LV_OPA_COVER);
  lv_style_set_text_color(&st_menu_row_sel, lv_color_hex(C_TITLE));
}
