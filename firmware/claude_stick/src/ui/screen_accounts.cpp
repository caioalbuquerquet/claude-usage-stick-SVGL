// /contas e /contas · nome (item 39). Uma linha por conta salva: rótulo
// (C_ACCENT na conta em foco, com "•  foco"), botões de editar e remover com
// borda. Tocar numa conta chama intent_focus(slot). Renomear usa
// accountSetLabel (g_accts.label é a única fonte do rótulo). Remover segue o
// item 23: gen++ e token zerado; um resultado atrasado do slot é descartado.
#include <Arduino.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <stdio.h>

#include "config.h"
#include "app/app_state.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "model/history.h"
#include "net/token_store.h"
#include "logic/levels.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"

extern Preferences g_prefs;
extern AccountSlots g_accts;

namespace {

int s_delArmed = -1;
bool s_keepArm = false;      // a próxima troca de conteúdo mantém s_delArmed
int s_renameSlot = -1;
lv_obj_t* s_nameTa = nullptr;

constexpr int BTN_W = 40, BTN_H = 30, GAP = 6;

void switch_cb(lv_event_t* e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  s_delArmed = -1;
  intent_focus(slot);                  // refaz /contas com o novo foco
}

void del_cb(lv_event_t* e) {
  int slot = (int)(intptr_t)lv_event_get_user_data(e);
  if (accountCount(g_accts) <= 1) return;
  if (s_delArmed != slot) {
    s_delArmed = slot;
    s_keepArm = true;                  // o rebuild de /contas mostra o botão armado
    shell_show(ContentId::ACCOUNTS);
    return;
  }
  s_delArmed = -1;
  accountRemove(g_prefs, g_accts, slot);
  acct_bump_gen(slot);
  token_store_clear(slot, (uint16_t)acct[slot].gen);
  char pth[16];
  hist_path(slot, pth, sizeof(pth));
  LittleFS.remove(pth);
  acct_clear(slot);
  Serial.printf("[ACCT] slot %d removido (gen %u)\n", slot, (unsigned)acct[slot].gen);
  shell_show(ContentId::ACCOUNTS);
}

void add_cb(lv_event_t* e) {
  (void)e;
  int slot = accountFirstFree(g_accts);
  if (slot < 0) return;
  g_tokenTargetSlot = slot;
  g_pendingLabel[0] = 0;
  shell_show(ContentId::TOKEN);
}

void edit_cb(lv_event_t* e) {
  s_renameSlot = (int)(intptr_t)lv_event_get_user_data(e);
  s_delArmed = -1;
  shell_show(ContentId::ACCOUNT_NAME);
}

void name_kb_cb(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    accountSetLabel(g_prefs, g_accts, s_renameSlot, lv_textarea_get_text(s_nameTa));
    shell_show(ContentId::ACCOUNTS);
  } else if (code == LV_EVENT_CANCEL) {
    shell_show(ContentId::ACCOUNTS);
  }
}

lv_obj_t* icon_btn(lv_obj_t* row, const char* sym, uint32_t fg, int x, lv_event_cb_t cb, int slot,
                   bool danger = false) {
  lv_obj_t* b = ui_button(row, sym, fg, BTN_W, BTN_H);
  lv_obj_set_pos(b, x, (36 - BTN_H) / 2);
  if (danger) {
    lv_obj_set_style_bg_color(b, lv_color_hex(C_BAD), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(C_BAD), 0);
  }
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void*)(intptr_t)slot);
  return b;
}

}  // namespace

// Linhas de 36 px: conta + editar (40) + remover (40), gap de 6.
void screen_accounts_build(lv_obj_t* p) {
  lv_obj_t* lst = ui_list(p, SHELL_CONTENT_W, SHELL_CONTENT_H);
  const int rowW = SHELL_CONTENT_W - 6;   // ui_list reserva 6 px da rolagem
  const bool canDelete = accountCount(g_accts) > 1;
  const int nBtn = canDelete ? 2 : 1;
  const int acctW = rowW - nBtn * (BTN_W + GAP);

  for (int i = 0; i < ACCT_MAX; i++) {
    if (!g_accts.used[i]) continue;
    const bool focus = (i == g_accts.active);

    lv_obj_t* row = lv_obj_create(lst);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, rowW, 36);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);

    char lbl[ACCT_LBL_MAX + 16], fit[ACCT_LBL_MAX + 16];
    if (g_accts.label[i][0]) snprintf(lbl, sizeof(lbl), "%s", g_accts.label[i]);
    else snprintf(lbl, sizeof(lbl), tr(T::ACCT_DEFAULT_FMT), i + 1);
    fit_label(lbl, 22, fit, sizeof(fit));
    lv_obj_t* a = lv_obj_create(row);
    lv_obj_remove_style_all(a);
    lv_obj_set_pos(a, 0, 0);
    lv_obj_set_size(a, acctW, 36);
    lv_obj_set_style_radius(a, 4, 0);
    lv_obj_set_style_bg_color(a, lv_color_hex(C_SELECTED), 0);
    lv_obj_set_style_bg_opa(a, focus ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_pad_hor(a, 8, 0);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(a, switch_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    lv_obj_t* mark = mklabel(a, focus ? "\xE2\x9D\xAF" : "", theme_font(FONT_BOLD_13), C_TITLE);   // ❯
    lv_obj_align(mark, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t* l = mklabel(a, fit, theme_font(FONT_BOLD_13), focus ? C_ACCENT : C_TEXT);
    lv_obj_align(l, LV_ALIGN_LEFT_MID, 16, 0);
    if (focus) {
      lv_obj_t* f = mklabel(a, tr(T::ACCTS_FOCUS), theme_font(FONT_11), C_DIM);
      lv_obj_align_to(f, l, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    }

    icon_btn(row, LV_SYMBOL_EDIT, C_DIM, acctW + GAP, edit_cb, i);
    if (canDelete) {
      bool armed = (s_delArmed == i);
      icon_btn(row, armed ? LV_SYMBOL_WARNING : LV_SYMBOL_TRASH, armed ? C_BG : C_DIM,
               acctW + GAP + BTN_W + GAP, del_cb, i, armed);
    }
  }

  if (accountFirstFree(g_accts) >= 0)
    ui_row(lst, LV_SYMBOL_PLUS, TRS("Adicionar conta", "Add account"), C_TITLE, nullptr, add_cb, nullptr);

  lv_obj_t* hint = mklabel(lst, tr(T::ACCTS_HINT), theme_font(FONT_11), C_DIM);
  lv_obj_set_width(hint, rowW);
  lv_obj_set_style_pad_top(hint, 6, 0);
  lv_obj_set_style_pad_left(hint, 8, 0);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
}

// Conteúdo ACCOUNT_NAME: campo no painel, teclado na camada flutuante.
void screen_account_name_build(lv_obj_t* p) {
  if (s_renameSlot < 0 || s_renameSlot >= ACCT_MAX || !g_accts.used[s_renameSlot]) {
    shell_show(ContentId::ACCOUNTS);
    return;
  }
  ui_label(p, theme_font(FONT_BOLD_13), C_ACCENT, 0, 14, TRS("Renomear conta", "Rename account"));

  s_nameTa = lv_textarea_create(p);
  lv_textarea_set_one_line(s_nameTa, true);
  lv_textarea_set_max_length(s_nameTa, ACCT_LBL_MAX - 1);
  lv_textarea_set_text(s_nameTa, g_accts.label[s_renameSlot]);
  lv_textarea_set_placeholder_text(s_nameTa, TRS("rótulo (ex.: Pessoal, Trabalho)",
                                                 "label (e.g. Personal, Work)"));
  ui_style_textarea(s_nameTa);
  lv_obj_set_size(s_nameTa, SHELL_CONTENT_W, 40);
  lv_obj_set_pos(s_nameTa, 0, 24);

  lv_obj_t* kb = lv_keyboard_create(shell_float());
  ui_style_keyboard(kb);
  lv_keyboard_set_textarea(kb, s_nameTa);
  lv_obj_add_event_cb(kb, name_kb_cb, LV_EVENT_ALL, NULL);
}

void screen_accounts_reset() {
  if (!s_keepArm) s_delArmed = -1;   // sair de /contas desarma o remover
  s_keepArm = false;
  s_nameTa = nullptr;
}
