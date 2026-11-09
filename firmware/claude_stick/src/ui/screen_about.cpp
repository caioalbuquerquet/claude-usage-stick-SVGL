// /ajustes · sobre (item 39): logo THEP no lugar do logo Claude Code e do
// Clawd, nome do produto e linhas "chave: valor" no visual Hermes.
#include <stdio.h>
#include "config.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"
#include "ui/assets_thep.h"

namespace {

// "chave: valor" com a chave em C_DIM, numa coluna de 84 px.
void kv(lv_obj_t* p, int base, const char* key, const char* val, uint32_t col = C_TEXT) {
  const lv_font_t* f11 = theme_font(FONT_11);
  ui_label(p, f11, C_DIM, 0, base, key);
  lv_obj_t* v = ui_label(p, f11, col, 84, base, val);
  lv_obj_set_width(v, SHELL_CONTENT_W - 84);
}

}  // namespace

void screen_about_build(lv_obj_t* p) {
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);

  lv_obj_t* logo = lv_image_create(p);
  lv_image_set_src(logo, &img_thep_logo_hdr);
  lv_obj_set_pos(logo, 0, 0);
  lv_obj_t* bird = lv_image_create(p);
  lv_image_set_src(bird, &img_bird_braille_124);
  lv_obj_align(bird, LV_ALIGN_TOP_RIGHT, 0, 0);

  ui_label(p, fb, C_TITLE, 0, 62, DEVICE_NAME);
  lv_obj_t* d = mklabel(p, TRS("Medidor de uso do Claude Code em tempo real: janelas "
                               "de 5h e semanal, direto da API da Anthropic.",
                               "Real-time Claude Code usage meter: 5-hour and weekly "
                               "windows, straight from the Anthropic API."), f11, C_DIM);
  lv_obj_set_pos(d, 0, 72);
  lv_obj_set_width(d, SHELL_CONTENT_W - 140);
  lv_label_set_long_mode(d, LV_LABEL_LONG_WRAP);

  kv(p, 140, TRS("versão:", "version:"), "v" FW_VERSION " \xC2\xB7 ESP32-S3 \xC2\xB7 LVGL 9.2");
  kv(p, 157, TRS("tela:", "display:"), "JC4832W535 \xC2\xB7 IPS 3.5\" 480x320 (AXS15231B)");
  kv(p, 174, TRS("autor:", "author:"), "Benevid Felix");
  kv(p, 191, TRS("contato:", "contact:"), "benevid@gmail.com", C_TITLE);
  kv(p, 208, TRS("fonte:", "font:"), "JetBrains Mono (SIL OFL 1.1)");
}
