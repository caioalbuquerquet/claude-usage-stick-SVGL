// Token (item 39): tela cheia no onboarding (ST_TOKEN, moldura THEP) ou
// conteúdo TOKEN do shell na sessão. O servidor web fica em
// src/app/web_api; a validação é assíncrona (VALIDATE) e o resultado chega
// pelo .ino (on_validate_result), que escreve aqui com screen_token_msg().
// Pássaro em braille na coluna esquerda (no lugar do Clawd da v2.3).
#include <Arduino.h>
#include <WiFi.h>
#include <stdio.h>

#include "config.h"
#include "app/web_api.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"
#include "ui/assets_thep.h"

namespace {

lv_obj_t* s_msg = nullptr;

void build(lv_obj_t* p, int h) {
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* f13 = theme_font(FONT_13);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);
  constexpr int X = 166, W = SHELL_CONTENT_W - X;   // coluna direita: 272 px

  lv_obj_t* bird = lv_image_create(p);
  lv_image_set_src(bird, &img_bird_braille_124);
  lv_obj_set_pos(bird, (150 - 124) / 2, (h - 124) / 2);

  ui_label(p, fb, C_ACCENT, X, 16, TRS("Token da API", "API token"));
  lv_obj_t* cap = mklabel(p, TRS("Cole o token pelo navegador, em:",
                                 "Paste the token via browser, at:"), f13, C_TEXT);
  lv_obj_set_pos(cap, X, 30);
  lv_obj_set_width(cap, W);
  lv_label_set_long_mode(cap, LV_LABEL_LONG_WRAP);

  char url[40];
  snprintf(url, sizeof(url), "http://%s", WiFi.localIP().toString().c_str());
  ui_label(p, fb, C_TITLE, X, 72, url);

  lv_obj_t* hint = mklabel(p, TRS("abra esse endereço no PC/celular, na MESMA rede WiFi",
                                  "open this address on a PC/phone, on the SAME WiFi"), f11, C_DIM);
  lv_obj_set_pos(hint, X, 84);
  lv_obj_set_width(hint, W);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);

  lv_obj_t* sp = lv_spinner_create(p);
  lv_spinner_set_anim_params(sp, 1200, 70);
  lv_obj_set_size(sp, 22, 22);
  lv_obj_set_pos(sp, X, h - 46);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_UNLIT), LV_PART_MAIN);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_TITLE), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(sp, 3, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sp, 3, LV_PART_INDICATOR);

  s_msg = mklabel(p, TRS("aguardando o token…", "waiting for the token…"), f11, C_DIM);
  lv_obj_set_pos(s_msg, X + 32, h - 46);
  lv_obj_set_width(s_msg, W - 32);
  lv_label_set_long_mode(s_msg, LV_LABEL_LONG_WRAP);

  // Item 6: a sessão já tem servidor (web_start_session); o onboarding sobe o
  // dele aqui, se ainda não houver. web_token_begin() é chamado pelo
  // app_state ao abrir ST_TOKEN ou TOKEN.
  if (!session_unlocked() && !web_running()) web_start_onboarding();
}

}  // namespace

void screen_token_full() {
  lv_obj_t* p = ui_frame("token");
  build(p, FRAME_CONTENT_H);
}

void screen_token_build(lv_obj_t* parent) { build(parent, SHELL_CONTENT_H); }

void screen_token_msg(const char* m) {
  if (s_msg) lv_label_set_text(s_msg, m);
}

void screen_token_reset() { s_msg = nullptr; }
