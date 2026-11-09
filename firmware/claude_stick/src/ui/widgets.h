#pragma once
// Peças de interface compartilhadas pelas telas de src/ui/ (item 39).
//
// Duas famílias:
//   - os helpers que vieram do .ino da v2.3 (mklabel, no_box, mkbtn, tlabel,
//     tstatic, rrect, mkchip, set_chip, fmt_*), já com as cores do tema;
//   - as peças do visual Hermes (ui_*): texto posicionado pela linha de base,
//     botão com borda, lista rolável de linhas de 36 px, moldura das telas
//     fora do shell (PIN, WiFi e token do onboarding).
#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>
#include "ui/theme.h"
#include "model/account_state.h"   // TokenStats

// ---- Helpers da v2.3 ----
lv_obj_t* mklabel(lv_obj_t* p, const char* txt, const lv_font_t* font, uint32_t color);
void no_box(lv_obj_t* o);
// Botão com borda de 1 px (fundo bg, texto fg), label centralizado.
lv_obj_t* mkbtn(lv_obj_t* p, const char* txt, const lv_font_t* font, uint32_t bg, uint32_t fg);
// Label vazio posicionado (preenchido depois) e label fixo posicionado.
lv_obj_t* tlabel(lv_obj_t* p, const lv_font_t* f, uint32_t c, int x, int y);
lv_obj_t* tstatic(lv_obj_t* p, const char* txt, const lv_font_t* f, uint32_t c, int x, int y);
// Retângulo sólido arredondado, sem borda nem toque.
lv_obj_t* rrect(lv_obj_t* p, int x, int y, int w, int h, int r, uint32_t col);
// Chip com borda na cor do texto (ex.: gravidade do incidente).
lv_obj_t* mkchip(lv_obj_t* p, int x, int y);
void set_chip(lv_obj_t* o, const char* txt, uint32_t col);

// "1h 23m" / "2d 4h" / "agora" / "--" (relógio não sincronizado)
void fmt_eta(uint32_t epoch, char* out, int sz);
// "HH:MM" local ou "--:--"
void fmt_hm(uint32_t epoch, char* out, int sz);
// 1234 -> "1.2k", 2345678 -> "2.3M"
void fmt_tok(long long v, char* out, size_t sz);
// Linha da contagem de tokens do bridge ("tokens: 1.2k entrada · 3.4M saída").
// Sem LVGL. Devolve false (e out = "") quando não há contagem recente
// (TOK_FRESH_MS).
bool tok_text(const TokenStats& t, char* out, size_t sz);

// ---- Visual Hermes ----
// Distância do topo da linha à linha de base (as JetBrains Mono geradas têm
// altura de linha bem maior que o corpo do texto).
int ui_ascent(const lv_font_t* f);
int ui_text_w(const char* t, const lv_font_t* f);
// Label com a linha de base em `base` (topo = base - ascent), LONG_CLIP.
lv_obj_t* ui_label(lv_obj_t* p, const lv_font_t* f, uint32_t col, int x, int base,
                   const char* txt = "");
// Só invalidam quando o texto ou a cor mudam.
void ui_set_text(lv_obj_t* l, const char* t);
void ui_set_color(lv_obj_t* l, uint32_t col);

// Botão do visual Hermes: borda 1 px C_BORDER, raio 6, fundo C_SELECTED só
// quando pressionado, texto bold 13 na cor fg. Altura h, largura pelo texto
// (mínimo 84 px) se w = 0.
lv_obj_t* ui_button(lv_obj_t* p, const char* txt, uint32_t fg, int w = 0, int h = 32);
// Lista rolável do tamanho do painel (438 x 214), linhas empilhadas.
lv_obj_t* ui_list(lv_obj_t* p, int w, int h);
// Linha de 36 px da lista: ícone LV_SYMBOL_* (C_DIM, pode ser nullptr), texto
// em jbmono_13 na cor fg e um valor alinhado à direita (C_TITLE, pode ser
// nullptr). Toque: cb com user_data. *outVal recebe o label do valor (ou o do
// texto, se não houver valor).
lv_obj_t* ui_row(lv_obj_t* list, const char* icon, const char* text, uint32_t fg,
                 const char* value, lv_event_cb_t cb, void* user, lv_obj_t** outVal = nullptr);
// Textarea e teclado no tema (fonte mono, superfícies do menu).
void ui_style_textarea(lv_obj_t* ta);
void ui_style_keyboard(lv_obj_t* kb);

// Moldura das telas fora do shell: cabeçalho com o logo THEP e um painel com
// o título sobre a borda (mesma geometria do shell, sem rodapé). Monta em
// lv_screen_active() e devolve o container do conteúdo (438 x 234).
constexpr int FRAME_CONTENT_W = 438;
constexpr int FRAME_CONTENT_H = 234;
lv_obj_t* ui_frame(const char* title);

// Rótulo da conta do slot, ou "Conta N" (ACCT_DEFAULT_FMT) se vazio. Com
// maxChars > 0, cortado com "…" (fit_label, em code points); senão inteiro.
void account_label(int slot, int maxChars, char* out, size_t sz);

// /janela e /ritmo: título do painel = rótulo da conta em foco; com mais de
// uma conta, tocar nele passa para a próxima (intent_focus_next).
void ui_focus_title();

// Fonte do teclado do PIN: a bold numérica de 30 px com a Montserrat 14 de
// fallback, para os LV_SYMBOL_LEFT/OK (cópia em RAM da fonte gerada).
const lv_font_t* ui_font_keypad();
