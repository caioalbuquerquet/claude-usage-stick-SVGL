// Shell fixo da sessão (item 34). Ver shell.h.
#include "ui/shell.h"
#include <string.h>
#include <stdio.h>
#include "config.h"
#include "ui/theme.h"
#include "ui/i18n.h"
#include "ui/assets_thep.h"
#include "ui/overlay_menu.h"

namespace {

// ---- Geometria (mockup-home.dc.html; item 34) ----
constexpr int HDR_H = 50;
constexpr int FOOT_H = 20;
constexpr int PANEL_X = 8;
constexpr int PANEL_Y = HDR_H + 6;                                  // 56
constexpr int PANEL_W = SCREEN_WIDTH - 2 * PANEL_X;                 // 464
constexpr int PANEL_H = SCREEN_HEIGHT - HDR_H - FOOT_H - 2 * 6;     // 238
constexpr int TITLE_RIGHT = PANEL_X + 1 + 14;                       // da borda direita da tela

// Pulso: 100/70/40/70% (Technical Plan §4).
const lv_opa_t PULSE[4] = {LV_OPA_COVER, 178, 102, 178};

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_panel = nullptr;
lv_obj_t* s_content = nullptr;
lv_obj_t* s_float = nullptr;
lv_obj_t* s_title = nullptr;
lv_obj_t* s_dot = nullptr;
lv_obj_t* s_menuBtn = nullptr;
lv_obj_t* s_footAcct = nullptr;
lv_obj_t* s_footPoll = nullptr;
lv_obj_t* s_footClock = nullptr;
lv_timer_t* s_pulse = nullptr;
uint8_t s_pulseStep = 0;
int s_dotState = -1;
void (*s_titleCb)() = nullptr;

lv_obj_t* (*s_getMoment)() = nullptr;
lv_obj_t* (*s_getIncident)() = nullptr;

void bare(lv_obj_t* o) {
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* label(lv_obj_t* p, const char* txt, const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(p);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

// Só invalida quando o texto muda (o rodapé é reavaliado a cada segundo e,
// no modo FULL, cada invalidação custa um quadro inteiro).
void set_text_if(lv_obj_t* l, const char* t) {
    if (!l) return;
    if (strcmp(lv_label_get_text(l), t) == 0) return;
    lv_label_set_text(l, t);
}

uint32_t dot_color(DotState d) {
    switch (d) {
        case DOT_RED:    return C_BAD;
        case DOT_YELLOW: return C_DOT_YELLOW;
        case DOT_BLUE:   return C_ACCENT;
        case DOT_GREEN:
        default:         return C_OK;
    }
}

// Só o objeto do ponto é invalidado (opacidade local do próprio ponto).
void pulse_cb(lv_timer_t* t) {
    (void)t;
    if (!s_dot) return;
    s_pulseStep = (uint8_t)((s_pulseStep + 1) & 3);
    lv_obj_set_style_bg_opa(s_dot, PULSE[s_pulseStep], 0);
}

void menu_btn_cb(lv_event_t* e) {
    (void)e;
    overlay_menu_toggle();
}

void title_cb(lv_event_t* e) {
    (void)e;
    if (s_titleCb) s_titleCb();
}

lv_obj_t* footer_sep(lv_obj_t* p) {
    lv_obj_t* l = label(p, "\xE2\x94\x82", theme_font(FONT_13), C_UNLIT);   // │
    lv_obj_set_style_pad_hor(l, 2, 0);
    return l;
}

void build_header() {
    lv_obj_t* h = lv_obj_create(s_scr);
    bare(h);
    lv_obj_set_pos(h, 0, 0);
    lv_obj_set_size(h, SCREEN_WIDTH, HDR_H);
    lv_obj_set_style_pad_top(h, 6, 0);
    lv_obj_set_style_pad_right(h, 8, 0);
    lv_obj_set_style_pad_bottom(h, 4, 0);
    lv_obj_set_style_pad_left(h, 10, 0);
    lv_obj_set_style_pad_column(h, 10, 0);
    lv_obj_set_flex_flow(h, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(h, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(h, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* logo = lv_image_create(h);
    lv_image_set_src(logo, &img_thep_logo_hdr);

    lv_obj_t* spacer = lv_obj_create(h);
    bare(spacer);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_remove_flag(spacer, LV_OBJ_FLAG_CLICKABLE);

    s_dot = lv_obj_create(h);
    bare(s_dot);
    lv_obj_set_size(s_dot, 10, 10);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_dot, LV_OBJ_FLAG_CLICKABLE);
    s_dotState = -1;

    s_menuBtn = lv_obj_create(h);
    bare(s_menuBtn);
    lv_obj_set_size(s_menuBtn, LV_SIZE_CONTENT, 38);
    lv_obj_set_style_min_width(s_menuBtn, 84, 0);
    lv_obj_set_style_pad_hor(s_menuBtn, 12, 0);
    lv_obj_set_style_pad_column(s_menuBtn, 8, 0);
    lv_obj_set_style_border_width(s_menuBtn, 1, 0);
    lv_obj_set_style_border_color(s_menuBtn, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_radius(s_menuBtn, 6, 0);
    lv_obj_set_style_bg_color(s_menuBtn, lv_color_hex(C_SELECTED), 0);
    lv_obj_set_style_bg_opa(s_menuBtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(s_menuBtn, LV_OPA_COVER, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(s_menuBtn, LV_OPA_COVER, LV_STATE_CHECKED);
    lv_obj_set_flex_flow(s_menuBtn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_menuBtn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_menuBtn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_menuBtn, 6);
    lv_obj_add_event_cb(s_menuBtn, menu_btn_cb, LV_EVENT_CLICKED, nullptr);
    label(s_menuBtn, "\xE2\x9D\xAF", theme_font(FONT_BOLD_13), C_DIM);       // ❯
    label(s_menuBtn, "menu", theme_font(FONT_BOLD_13), C_TITLE);
}

void build_panel() {
    s_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_panel);
    lv_obj_add_style(s_panel, &st_panel, 0);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(s_panel, PANEL_X, PANEL_Y);
    lv_obj_set_size(s_panel, PANEL_W, PANEL_H);
    s_content = nullptr;
    shell_content_reset();

    // Título sobre a borda superior, centrado na linha da borda.
    s_title = lv_label_create(s_scr);
    lv_obj_add_style(s_title, &st_panel_title, 0);
    lv_label_set_text(s_title, "");
    int lh = lv_font_get_line_height(theme_font(FONT_BOLD_13));
    lv_obj_align(s_title, LV_ALIGN_TOP_RIGHT, -TITLE_RIGHT, PANEL_Y - lh / 2);
    lv_obj_set_ext_click_area(s_title, 8);
    lv_obj_add_event_cb(s_title, title_cb, LV_EVENT_CLICKED, nullptr);
    s_titleCb = nullptr;
}

void build_footer() {
    lv_obj_t* f = lv_obj_create(s_scr);
    bare(f);
    lv_obj_set_pos(f, 0, SCREEN_HEIGHT - FOOT_H);
    lv_obj_set_size(f, SCREEN_WIDTH, FOOT_H);
    lv_obj_set_style_bg_color(f, lv_color_hex(C_STATUSBAR), 0);
    lv_obj_set_style_bg_opa(f, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(f, 10, 0);
    lv_obj_set_style_pad_column(f, 5, 0);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(f, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(f, LV_OBJ_FLAG_CLICKABLE);

    // Rodapé em 13 px (pedido do dono). Pássaro e "thep" saíram: começa direto
    // na contagem de contas.
    const lv_font_t* f13 = theme_font(FONT_13);
    s_footAcct = label(f, "", f13, C_TEXT);
    footer_sep(f);
    label(f, LV_SYMBOL_REFRESH, f13, C_DIM);          // ↻ pela Montserrat 14 (fallback)
    s_footPoll = label(f, "", f13, C_TEXT);
    // O relógio fica na borda DIREITA: com a tela girada 270°, a borda direita da
    // paisagem são as primeiras linhas nativas, e o envio em faixas sempre começa
    // na linha 0. Na esquerda, a troca de minuto mandava ~459 das 480 linhas;
    // na direita manda só ~50.
    lv_obj_t* spacer = lv_obj_create(f);
    bare(spacer);
    lv_obj_set_height(spacer, 1);
    lv_obj_set_flex_grow(spacer, 1);
    s_footClock = label(f, "--:--", f13, C_TEXT);
}

}  // namespace

void shell_create() {
    if (s_scr) shell_destroy();
    s_scr = lv_screen_active();
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    build_header();
    build_panel();
    build_footer();
    s_pulseStep = 0;
    s_pulse = lv_timer_create(pulse_cb, DOT_PULSE_STEP_MS, nullptr);
}

void shell_destroy() {
    if (s_pulse) lv_timer_delete(s_pulse);
    s_pulse = nullptr;
    s_scr = s_panel = s_content = s_float = s_title = s_dot = s_menuBtn = nullptr;
    s_footAcct = s_footPoll = s_footClock = nullptr;
    s_titleCb = nullptr;
    s_dotState = -1;
}

bool shell_is_up() { return s_scr != nullptr; }

lv_obj_t* shell_content() { return s_content; }

lv_obj_t* shell_content_reset() {
    if (!s_panel) return nullptr;
    if (s_float) { lv_obj_delete(s_float); s_float = nullptr; }
    if (s_content) lv_obj_delete(s_content);
    s_content = lv_obj_create(s_panel);
    bare(s_content);
    lv_obj_set_size(s_content, SHELL_CONTENT_W, SHELL_CONTENT_H);
    lv_obj_remove_flag(s_content, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(s_content, lv_color_hex(C_TEXT), 0);
    shell_set_title_action(nullptr);
    return s_content;
}

lv_obj_t* shell_float() {
    if (!s_scr) return nullptr;
    if (!s_float) {
        s_float = lv_obj_create(s_scr);
        bare(s_float);
        lv_obj_set_pos(s_float, 0, 0);
        lv_obj_set_size(s_float, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_remove_flag(s_float, LV_OBJ_FLAG_CLICKABLE);
    }
    return s_float;
}

void shell_set_title(const char* t) { set_text_if(s_title, t ? t : ""); }

void shell_set_title_action(void (*cb)()) {
    s_titleCb = cb;
    if (!s_title) return;
    if (cb) lv_obj_add_flag(s_title, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_remove_flag(s_title, LV_OBJ_FLAG_CLICKABLE);
}

void shell_set_dot(DotState d) {
    if (!s_dot || s_dotState == (int)d) return;
    s_dotState = (int)d;
    lv_obj_set_style_bg_color(s_dot, lv_color_hex(dot_color(d)), 0);
}

void shell_set_footer(int nAccounts, int nFailed, int pollSec) {
    char b[48];
    if (nAccounts == 1) snprintf(b, sizeof(b), "%s", tr(T::FOOT_ACCT_ONE));
    else snprintf(b, sizeof(b), tr(T::FOOT_ACCT_FMT), nAccounts);
    if (nFailed > 0) {
        char k[24];
        snprintf(k, sizeof(k), " \xC2\xB7 %d \xE2\x9C\x97", nFailed);   // " · K ✗"
        strlcat(b, k, sizeof(b));
    }
    set_text_if(s_footAcct, b);
    if (pollSec < 60) snprintf(b, sizeof(b), "%ds", pollSec);
    else snprintf(b, sizeof(b), "%dmin", pollSec / 60);
    set_text_if(s_footPoll, b);
}

void shell_set_clock(const char* hhmm) { set_text_if(s_footClock, hhmm); }

void shell_set_menu_active(bool open) {
    if (!s_menuBtn) return;
    if (open) lv_obj_add_state(s_menuBtn, LV_STATE_CHECKED);
    else lv_obj_remove_state(s_menuBtn, LV_STATE_CHECKED);
}

void shell_set_overlay_getters(lv_obj_t* (*moment)(), lv_obj_t* (*incident)()) {
    s_getMoment = moment;
    s_getIncident = incident;
}

void overlay_restack() {
    lv_obj_t* m = s_getMoment ? s_getMoment() : nullptr;
    lv_obj_t* i = s_getIncident ? s_getIncident() : nullptr;
    if (m) lv_obj_move_to_index(m, -1);
    if (i) lv_obj_move_to_index(i, -1);
}
