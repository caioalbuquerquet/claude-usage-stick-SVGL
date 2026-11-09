// Alerta de incidente (item 40). Ver overlay_incident.h.
//
// Caixa 420 x 206 em C_SURFACE com a borda na cor da gravidade; pássaro em
// braille em cinza (image_recolor) à esquerda; à direita, título, chip de
// gravidade, modelos afetados, "veja status.claude.com" e o botão OK.
#include "ui/overlay_incident.h"
#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "app/app_state.h"
#include "model/account_state.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"
#include "ui/widgets.h"
#include "net/status.h"
#include "ui/assets_thep.h"

namespace {

lv_obj_t* g_incDlg = nullptr;   // overlay vivo (nullptr = fechado)
bool g_incAck = false;          // usuário já dispensou o incidente atual
uint8_t g_incSev = 0xFF;        // gravidade desenhada no overlay atual
uint8_t g_incMask = 0xFF;       // modelos afetados desenhados no overlay

// Bitmask dos modelos citados pelos incidentes abertos (Haiku, Sonnet, Opus,
// Fable). Vem do mesmo scan de palavra-chave que decide se o overlay aparece.
uint8_t affected_mask() {
  return (uint8_t)((!g_status.haikuUp) | (!g_status.sonnetUp) << 1 |
                   (!g_status.opusUp) << 2 | (!g_status.fableUp) << 3);
}

// Cor por gravidade: crítico C_BAD, grave C_BAD_INK, leve/desconhecido C_WARN.
uint32_t severity_color(uint8_t sev) {
  return sev >= SEV_CRITICAL ? C_BAD : sev == SEV_MAJOR ? C_BAD_INK : C_WARN;
}

void close_dlg() {
  if (!g_incDlg) return;
  lv_obj_delete(g_incDlg);
  g_incDlg = nullptr;
}

void dismiss_cb(lv_event_t* e) {
  (void)e;
  g_incAck = true;
  close_dlg();
}

void show() {
  close_dlg();
  g_incSev = g_status.severity;
  g_incMask = affected_mask();
  const uint32_t sevCol = severity_color(g_incSev);
  const lv_font_t* f11 = theme_font(FONT_11);
  const lv_font_t* f13 = theme_font(FONT_13);
  const lv_font_t* fb = theme_font(FONT_BOLD_13);

  lv_obj_t* s = lv_obj_create(lv_layer_top());
  g_incDlg = s;
  lv_obj_remove_style_all(s);
  lv_obj_set_pos(s, 0, 0);
  lv_obj_set_size(s, SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(s, lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(s, 210, 0);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s, dismiss_cb, LV_EVENT_CLICKED, NULL);

  // caixa 420 x 206, pad 16 -> área útil 386 x 172
  lv_obj_t* box = lv_obj_create(s);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, 420, 206);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(box, 8, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(sevCol), 0);
  lv_obj_set_style_pad_all(box, 16, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* bird = lv_image_create(box);
  lv_image_set_src(bird, &img_bird_braille_124);
  lv_obj_set_style_image_recolor(bird, lv_color_hex(C_DIM), 0);
  lv_obj_set_style_image_recolor_opa(bird, 230, 0);
  lv_obj_align(bird, LV_ALIGN_LEFT_MID, 0, 0);

  constexpr int TX = 124 + 16;          // início da coluna de texto
  constexpr int TW = 386 - TX;          // largura útil dessa coluna (246)

  // empilhamento sequencial: os modelos somem quando não há o que dizer
  int base = 16;
  lv_obj_t* t = ui_label(box, fb, sevCol, TX, base,
                         TRS("\xE2\x9A\xA0 INCIDENTE ATIVO", "\xE2\x9A\xA0 ACTIVE INCIDENT"));
  lv_obj_set_width(t, TW);

  lv_obj_t* chip = mkchip(box, TX, base + 10);
  set_chip(chip, severity_label(g_incSev), sevCol);
  base += 10 + 22 + 24;

  char mods[80];
  if (affected_models(mods, sizeof(mods))) {
    lv_obj_t* ml = ui_label(box, f13, C_TEXT, TX, base, mods);
    lv_obj_set_width(ml, TW);
    lv_label_set_long_mode(ml, LV_LABEL_LONG_DOT);
    base += 20;
  }
  ui_label(box, f11, C_DIM, TX, base, TRS("veja status.claude.com", "see status.claude.com"));

  lv_obj_t* ok = ui_button(box, "OK", C_TITLE, 120, 34);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_LEFT, TX, 0);
  lv_obj_add_event_cb(ok, dismiss_cb, LV_EVENT_CLICKED, NULL);
  overlay_restack();
}

}  // namespace

// "Opus 4.6 • Sonnet 4.5". Os quatro afetados viram "Todos"; false = nenhum
// modelo citado, aí a linha some. A versão só entra com até 2 modelos: com 3,
// o LONG_DOT cortaria justamente o último nome.
bool affected_models(char* out, size_t sz) {
  static const char* NAMES[4] = {"Haiku", "Sonnet", "Opus", "Fable"};
  uint8_t mask = affected_mask();
  out[0] = 0;
  if (mask == 0x0F) { strlcpy(out, TRS("Todos", "All"), sz); return true; }

  int total = 0;
  for (int i = 0; i < 4; i++) if (mask & (1 << i)) total++;
  bool withVer = (total <= 2);

  int n = 0;
  for (int i = 0; i < 4; i++) {
    if (!(mask & (1 << i))) continue;
    if (n++) strlcat(out, " \xE2\x80\xA2 ", sz);
    strlcat(out, NAMES[i], sz);
    if (withVer && g_status.version[i][0]) {
      strlcat(out, " ", sz);
      strlcat(out, g_status.version[i], sz);
    }
  }
  return n > 0;
}

// Todo incidente alerta: sem gravidade utilizável o chip diz isso, em vez de
// sumir e deixar o alerta parecendo genérico.
const char* severity_label(uint8_t sev) {
  switch (sev) {
    case SEV_CRITICAL: return TRS("CRÍTICO", "CRITICAL");
    case SEV_MAJOR:    return TRS("GRAVE", "MAJOR");
    case SEV_MINOR:    return TRS("LEVE", "MINOR");
    default:           return TRS("IMPACTO NÃO DECLARADO", "IMPACT NOT DECLARED");
  }
}

lv_obj_t* incident_obj() { return g_incDlg; }

void incident_forget() { g_incDlg = nullptr; }

void incident_sync() {
  if (app_state() != ST_SHELL) return;
  bool any = g_status.ok &&
             !(g_status.haikuUp && g_status.sonnetUp && g_status.opusUp && g_status.fableUp);
  if (!any) { g_incAck = false; g_incSev = g_incMask = 0xFF; close_dlg(); return; }

  uint8_t sev = g_status.severity, mask = affected_mask();
  if (g_incDlg && (sev != g_incSev || mask != g_incMask)) close_dlg();         // redesenha
  else if (g_incAck && g_incSev != 0xFF && sev > g_incSev) g_incAck = false;   // escalou
  if (!g_incAck && !g_incDlg) show();
}
