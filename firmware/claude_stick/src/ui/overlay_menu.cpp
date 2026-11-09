// Menu modal (item 35). Ver overlay_menu.h.
#include "ui/overlay_menu.h"
#include <stdio.h>
#include <lvgl.h>
#include "config.h"
#include "app/app_state.h"
#include "ui/shell.h"
#include "ui/theme.h"
#include "ui/i18n.h"

namespace {

constexpr int MENU_W = 262;
constexpr int ROW_H = 36;
constexpr int N_ROWS = 7;
constexpr int MENU_H = N_ROWS * ROW_H + 2 * 4 + 2 * 1;   // 262
constexpr int MENU_X = SCREEN_WIDTH - 8 - MENU_W;        // 210
constexpr int MENU_Y = 50;

struct Item {
    const char* cmd;     // nullptr = "↻ Atualizar agora" (traduzido)
    const Txt* desc;
    ContentId target;
};
const Item ITEMS[N_ROWS] = {
    {nullptr,    nullptr,             ContentId::HOME},
    {"/agora",   &T::MENU_D_HOME,     ContentId::HOME},
    {"/modelos", &T::MENU_D_MODELS,   ContentId::MODELS},
    {"/janela",  &T::MENU_D_TREND,    ContentId::TREND},
    {"/ritmo",   &T::MENU_D_HEAT,     ContentId::HEAT},
    {"/contas",  &T::MENU_D_ACCTS,    ContentId::ACCOUNTS},
    {"/ajustes", &T::MENU_D_SETTINGS, ContentId::SETTINGS},
};

lv_obj_t* s_scrim = nullptr;   // raiz do menu (tela inteira, transparente)
lv_obj_t* s_dead = nullptr;    // escondido por um evento do menu; apagado no gc

// Linha do menu que corresponde ao conteúdo aberto (as subtelas marcam a
// linha de onde se chega a elas).
int current_row(ContentId c) {
    switch (c) {
        case ContentId::HOME:         return 1;
        case ContentId::MODELS:       return 2;
        case ContentId::TREND:        return 3;
        case ContentId::HEAT:         return 4;
        case ContentId::ACCOUNTS:
        case ContentId::ACCOUNT_NAME:
        case ContentId::TOKEN:        return 5;
        case ContentId::SETTINGS:
        case ContentId::ABOUT:
        case ContentId::WIFI:
        default:                      return 6;
    }
}

// Num evento do próprio menu não dá para apagá-lo: esconde e deixa para o gc.
void hide_from_event() {
    if (!s_scrim) return;
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_HIDDEN);
    if (s_dead) lv_obj_delete(s_dead);
    s_dead = s_scrim;
    s_scrim = nullptr;
    shell_set_menu_active(false);
}

void scrim_cb(lv_event_t* e) {
    if (lv_event_get_target(e) != lv_event_get_current_target(e)) return;
    hide_from_event();
}

void row_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    hide_from_event();
    if (i == 0) intent_refresh();
    else if (i > 0 && i < N_ROWS) intent_open(ITEMS[i].target);
}

lv_obj_t* label(lv_obj_t* p, const char* txt, const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(p);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

void build_row(lv_obj_t* box, int i, bool sel) {
    lv_obj_t* r = lv_obj_create(box);
    lv_obj_remove_style_all(r);
    lv_obj_add_style(r, &st_menu_row, 0);
    if (sel) lv_obj_add_style(r, &st_menu_row_sel, 0);
    lv_obj_set_style_bg_color(r, lv_color_hex(C_SELECTED), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_width(r, lv_pct(100));
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(r, row_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

    lv_obj_t* mark = label(r, sel ? "\xE2\x9D\xAF" : "", theme_font(FONT_13), C_TITLE);   // ❯
    lv_obj_set_width(mark, 10);

    const Item& it = ITEMS[i];
    if (!it.cmd) {
        char b[48];
        snprintf(b, sizeof(b), LV_SYMBOL_REFRESH " %s", tr(T::MENU_REFRESH));
        label(r, b, theme_font(FONT_BOLD_13), C_TEXT);
        return;
    }
    lv_obj_t* cmd = label(r, it.cmd, theme_font(FONT_BOLD_13), sel ? C_TITLE : C_TEXT);
    lv_obj_set_width(cmd, 86);
    label(r, tr(*it.desc), theme_font(FONT_11), C_DIM);
}

}  // namespace

void overlay_menu_open() {
    if (s_scrim || !shell_is_up()) return;
    s_scrim = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_scrim);
    lv_obj_set_pos(s_scrim, 0, 0);
    lv_obj_set_size(s_scrim, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_remove_flag(s_scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_scrim, scrim_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* box = lv_obj_create(s_scrim);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, MENU_X, MENU_Y);
    lv_obj_set_size(box, MENU_W, MENU_H);
    lv_obj_set_style_bg_color(box, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 6, 0);
    lv_obj_set_style_pad_all(box, 4, 0);
    lv_obj_set_style_pad_row(box, 0, 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);   // toque entre linhas não fecha

    int cur = current_row(app_content());
    for (int i = 0; i < N_ROWS; i++) build_row(box, i, i == cur);

    shell_set_menu_active(true);
    overlay_restack();   // momento e incidente continuam por cima
}

// Não chama o gc: pode rodar dentro do evento de uma linha do menu escondido
// (row_cb -> intent_open), e o s_dead é o pai de quem está despachando o
// evento. O s_dead só sai no overlay_menu_gc(), no loop().
void overlay_menu_close() {
    if (!s_scrim) return;
    lv_obj_delete(s_scrim);
    s_scrim = nullptr;
    shell_set_menu_active(false);
}

void overlay_menu_toggle() {
    if (s_scrim) overlay_menu_close();
    else overlay_menu_open();
}

bool overlay_menu_is_open() { return s_scrim != nullptr; }

void overlay_menu_gc() {
    if (!s_dead) return;
    lv_obj_delete(s_dead);
    s_dead = nullptr;
}
