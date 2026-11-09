// /ajustes (item 39): lista rolável de linhas de 36 px no visual Hermes,
// "chave" à esquerda e o valor atual à direita em C_TITLE. Mesmas opções da
// v2.3, sem "Slideshow". "Atualizar agora" chama intent_refresh(); a troca de
// fuso chama scheduler_apply_tz() (item 19).
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>

#include "config.h"
#include "app/app_state.h"
#include "net/scheduler.h"
#include "model/accounts.h"
#include "ui/screens.h"
#include "ui/widgets.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"

extern Preferences g_prefs;
extern AccountSlots g_accts;
extern int g_pollSec;
extern int g_tzOffset;

namespace {

bool g_wipeArmed = false;
lv_obj_t* g_briLbl = nullptr;
lv_obj_t* g_wipeLbl = nullptr;
lv_obj_t* g_pollLbl = nullptr;
lv_obj_t* g_tzLbl = nullptr;

const int POLL_OPTS[4] = {30, 60, 120, 300};
const int TZ_OPTS[] = {-3, -4, -5, -6, -7, -8, -2, -1, 0, 1, 2, 3};
constexpr int NTZ = (int)(sizeof(TZ_OPTS) / sizeof(TZ_OPTS[0]));

enum Act : int {
  A_REFRESH, A_POLL, A_LANG, A_TZ, A_BRI, A_WIFI, A_ACCTS, A_TOKEN, A_ABOUT, A_WIPE
};

const char* bri_text() {
  const char* n[3] = {TRS("baixo", "low"), TRS("médio", "medium"), TRS("alto", "high")};
  return n[g_briIdx < 0 || g_briIdx > 2 ? 1 : g_briIdx];
}
void poll_text(char* b, size_t sz) {
  if (g_pollSec < 60) snprintf(b, sz, "%ds", g_pollSec);
  else snprintf(b, sz, "%dmin", g_pollSec / 60);
}
void tz_text(char* b, size_t sz) { snprintf(b, sz, "GMT%+d", g_tzOffset); }

void action_cb(lv_event_t* e) {
  int act = (int)(intptr_t)lv_event_get_user_data(e);
  char m[40];
  switch (act) {
    case A_REFRESH: intent_refresh(); break;
    case A_WIFI: g_onboarding = false; shell_show(ContentId::WIFI); break;
    case A_TOKEN:                                      // trocar token da conta em foco
      g_tokenTargetSlot = g_accts.active;
      g_pendingLabel[0] = 0;
      shell_show(ContentId::TOKEN);
      break;
    case A_BRI:
      g_briIdx = (g_briIdx + 1) % 3;
      g_prefs.putInt("bri", g_briIdx);
      apply_brightness();
      if (g_briLbl) lv_label_set_text(g_briLbl, bri_text());
      break;
    case A_WIPE:                                       // apagar tudo (2 toques)
      if (!g_wipeArmed) {
        g_wipeArmed = true;
        if (g_wipeLbl) lv_label_set_text(g_wipeLbl, TRS("toque de novo p/ confirmar", "tap again to confirm"));
      } else {
        g_wipeArmed = false;
        factory_reset();
        request_state(ST_WIFI);
      }
      break;
    case A_POLL: {
      int idx = 0;
      for (int i = 0; i < 4; i++) if (POLL_OPTS[i] == g_pollSec) idx = i;
      g_pollSec = POLL_OPTS[(idx + 1) % 4];
      g_prefs.putInt("poll", g_pollSec);
      poll_text(m, sizeof(m));
      if (g_pollLbl) lv_label_set_text(g_pollLbl, m);
      break;
    }
    case A_TZ: {
      int idx = 0;
      for (int i = 0; i < NTZ; i++) if (TZ_OPTS[i] == g_tzOffset) idx = i;
      g_tzOffset = TZ_OPTS[(idx + 1) % NTZ];
      g_prefs.putInt("tz", g_tzOffset);
      scheduler_apply_tz();
      tz_text(m, sizeof(m));
      if (g_tzLbl) lv_label_set_text(g_tzLbl, m);
      break;
    }
    case A_LANG:
      g_lang ^= 1;
      g_prefs.putInt("lang", g_lang);
      shell_show(ContentId::SETTINGS);                 // redesenha no novo idioma
      break;
    case A_ABOUT: shell_show(ContentId::ABOUT); break;
    case A_ACCTS: shell_show(ContentId::ACCOUNTS); break;
  }
}

void row(lv_obj_t* lst, const char* icon, const char* key, const char* val, int act,
         uint32_t fg = C_TEXT, lv_obj_t** out = nullptr) {
  ui_row(lst, icon, key, fg, val, action_cb, (void*)(intptr_t)act, out);
}

}  // namespace

void screen_settings_build(lv_obj_t* p) {
  g_wipeArmed = false;
  lv_obj_t* lst = ui_list(p, SHELL_CONTENT_W, SHELL_CONTENT_H);
  const char* more = "\xE2\x9D\xAF";   // ❯: abre outra tela
  char poll[16], tz[16], acc[ACCT_LBL_MAX + 16];
  poll_text(poll, sizeof(poll));
  tz_text(tz, sizeof(tz));
  snprintf(acc, sizeof(acc), "%d/%d", accountCount(g_accts), ACCT_MAX);

  row(lst, LV_SYMBOL_REFRESH, TRS("Atualizar agora", "Refresh now"), nullptr, A_REFRESH);
  row(lst, LV_SYMBOL_LOOP, TRS("Atualizar a cada", "Refresh every"), poll, A_POLL, C_TEXT, &g_pollLbl);
  row(lst, LV_SYMBOL_LIST, TRS("Idioma", "Language"), TRS("Português", "English"), A_LANG);
  row(lst, LV_SYMBOL_GPS, TRS("Fuso", "Timezone"), tz, A_TZ, C_TEXT, &g_tzLbl);
  row(lst, LV_SYMBOL_EYE_OPEN, TRS("Brilho", "Brightness"), bri_text(), A_BRI, C_TEXT, &g_briLbl);
  row(lst, LV_SYMBOL_WIFI, TRS("Configurar WiFi", "Configure WiFi"), more, A_WIFI);
  row(lst, LV_SYMBOL_DIRECTORY, TRS("Contas", "Accounts"), acc, A_ACCTS);
  row(lst, LV_SYMBOL_KEYBOARD, TRS("Trocar token", "Change token"), more, A_TOKEN);
  row(lst, LV_SYMBOL_FILE, TRS("Sobre", "About"), more, A_ABOUT);
  row(lst, LV_SYMBOL_TRASH, TRS("Apagar tudo", "Erase everything"), "", A_WIPE, C_BAD_INK, &g_wipeLbl);
  if (g_wipeLbl) lv_obj_set_style_text_color(g_wipeLbl, lv_color_hex(C_BAD_INK), 0);
}

void screen_settings_reset() {
  g_briLbl = g_wipeLbl = g_pollLbl = g_tzLbl = nullptr;
}
