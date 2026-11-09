// Estado da aplicação e navegação (item 25). Ver app_state.h.
#include "app/app_state.h"
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "app/web_api.h"
#include "logic/dot_state.h"
#include "model/accounts.h"
#include "model/account_state.h"
#include "net/scheduler.h"
#include "ui/i18n.h"
#include "ui/overlay_menu.h"
#include "ui/shell.h"
#include "ui/theme.h"

// ficam no .ino
extern AccountSlots g_accts;
extern Preferences g_prefs;
extern int g_pollSec;

namespace {

constexpr int NCONTENT = (int)ContentId::COUNT;

ScreenOps s_screens[ST_COUNT] = {};
ContentOps s_contents[NCONTENT] = {};
void (*s_uiReset)(bool) = nullptr;

AppState s_state = ST_BOOT;
AppState s_pendingState = ST_BOOT;
bool s_stateDirty = false;

ContentId s_content = ContentId::HOME;
ContentId s_pendingContent = ContentId::HOME;
bool s_contentDirty = false;
bool s_contentBuilt = false;    // há um conteúdo montado no shell

uint32_t s_lastSecMs = 0;

const char* const COMMANDS[NCONTENT] = {
    "/agora", "/modelos", "/janela", "/ritmo", "/contas",
    "/contas", "/ajustes", "/ajustes", "/contas", "/ajustes",
};

const ContentOps& ops_of(ContentId c) { return s_contents[(int)c]; }

// Saída do conteúdo aberto: efeitos de TOKEN e WIFI.
void leave_content(ContentId next) {
    if (!s_contentBuilt) return;
    if (s_content == ContentId::TOKEN && next != ContentId::TOKEN) web_token_reset();
    if (s_content == ContentId::WIFI && next != ContentId::WIFI) scheduler_resume();
}

// Limpa a tela inteira e o lv_layer_top (troca de AppState).
void full_clean() {
    if (s_uiReset) s_uiReset(true);
    overlay_menu_close();
    lv_obj_clean(lv_layer_top());
    shell_destroy();
    s_contentBuilt = false;
    lv_obj_t* scr = lv_screen_active();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
}

void status_update();

void apply_content() {
    s_contentDirty = false;
    ContentId next = s_pendingContent;
    bool same = s_contentBuilt && next == s_content;
    leave_content(next);
    if (s_uiReset) s_uiReset(false);
    lv_obj_t* parent = shell_content_reset();
    if (!parent) return;
    s_content = next;
    s_contentBuilt = true;
    if (!same) {
        if (next == ContentId::TOKEN) web_token_begin();
        if (next == ContentId::WIFI) scheduler_pause();
    }
    shell_set_title(content_command(next));
    const ContentOps& o = ops_of(next);
    if (o.build) o.build(parent);
}

void render_state() {
    AppState prev = s_state;
    s_state = s_pendingState;
    if (s_state == ST_SHELL) {
        if (prev == ST_SHELL && shell_is_up()) return;
        if (prev == ST_TOKEN) web_token_reset();
        full_clean();
        shell_create();
        s_contentDirty = true;          // abre o conteúdo pendente (HOME por padrão)
        status_update();
        return;
    }
    // Fora do shell. O servidor web não cai mais a cada tela (item 24): só o
    // estado da validação do token fecha junto com a tela de token.
    if (prev == ST_SHELL) {
        leave_content(ContentId::COUNT);
        s_pendingContent = ContentId::HOME;   // a próxima sessão abre em /agora
    }
    if (prev == ST_TOKEN && s_state != ST_TOKEN) web_token_reset();
    full_clean();
    if (s_state == ST_TOKEN) web_token_begin();
    const ScreenOps& o = s_screens[s_state];
    if (o.build) o.build();
}

void clock_text(char* out, size_t sz) {
    time_t now = time(nullptr);
    if (now < 1000000000L) { strlcpy(out, "--:--", sz); return; }
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(out, sz, "%H:%M", &tmv);
}

// Ponto, rodapé e relógio (uma vez por segundo e a cada resultado).
void status_update() {
    if (!shell_is_up()) return;
    AccountView v[ACCT_MAX];
    int n = 0, k = 0;
    for (int s = 0; s < ACCT_MAX; s++) {
        if (!g_accts.used[s]) continue;
        v[n].keyOk = acct[s].keyOk;
        v[n].lastClass = acct[s].lastClass;
        n++;
        if (app_slot_failed(s)) k++;
    }
    DotState d = dot_state(v, n, (uint32_t)time(nullptr), scheduler_cycle_end_epoch(),
                           scheduler_first_cycle_running(), DOT_FRESH_SEC);
    shell_set_dot(d);
    shell_set_footer(n, k, g_pollSec);
    char hm[8];
    clock_text(hm, sizeof(hm));
    shell_set_clock(hm);
}

}  // namespace

void app_register_screen(AppState s, const ScreenOps& ops) {
    if (s < ST_COUNT) s_screens[s] = ops;
}
void app_register_content(ContentId c, const ContentOps& ops) {
    if ((int)c < NCONTENT) s_contents[(int)c] = ops;
}
void app_on_ui_reset(void (*cb)(bool full)) { s_uiReset = cb; }

void request_state(AppState s) {
    s_pendingState = s;
    s_stateDirty = true;
}
AppState app_state() { return s_state; }
bool app_state_pending() { return s_stateDirty || s_contentDirty; }

void shell_show(ContentId c) {
    if ((int)c >= NCONTENT) return;
    s_pendingContent = c;
    s_contentDirty = true;
}
ContentId app_content() { return s_content; }

const char* content_command(ContentId c) {
    return (int)c < NCONTENT ? COMMANDS[(int)c] : "";
}

void intent_refresh() { scheduler_request_refresh(); }

void intent_focus(int slot) {
    if (slot < 0 || slot >= ACCT_MAX || !g_accts.used[slot]) return;
    if (slot == g_accts.active) return;
    accountSetActive(g_prefs, g_accts, slot);
    Serial.printf("[ACCT] foco -> slot %d (%s)\n", slot, g_accts.label[slot]);
    if (s_state == ST_SHELL) shell_show(s_content);
}

void intent_focus_next() {
    for (int i = 1; i <= ACCT_MAX; i++) {
        int s = (g_accts.active + i) % ACCT_MAX;
        if (g_accts.used[s]) { intent_focus(s); return; }
    }
}

void intent_open(ContentId c) {
    overlay_menu_close();
    shell_show(c);
}

int app_account_count() { return accountCount(g_accts); }

bool app_slot_failed(int s) {
    if (s < 0 || s >= ACCT_MAX || !g_accts.used[s]) return false;
    if (!acct[s].keyOk) return true;
    FetchClass c = acct[s].lastClass;
    return c != FC_NONE && c != FC_OK;
}

int app_failed_count() {
    int k = 0;
    for (int s = 0; s < ACCT_MAX; s++) if (app_slot_failed(s)) k++;
    return k;
}

void app_state_tick(bool dataChanged) {
    overlay_menu_gc();
    if (s_stateDirty) {
        s_stateDirty = false;
        render_state();
    }
    if (s_state != ST_SHELL) {
        const ScreenOps& o = s_screens[s_state];
        if (o.tick) o.tick();
        return;
    }
    if (s_contentDirty) apply_content();

    const ContentOps& o = ops_of(s_content);
    if (dataChanged && o.refresh) o.refresh();
    uint32_t now = millis();
    if (dataChanged || now - s_lastSecMs >= 1000) {
        bool second = now - s_lastSecMs >= 1000;
        if (second) s_lastSecMs = now;
        status_update();
        if (second && o.tick) o.tick();
    }
}
