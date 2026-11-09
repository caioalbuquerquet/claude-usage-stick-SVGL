#pragma once
// Tema THEP (Technical Plan §6, item 33): papéis de cor, estilos LVGL
// compartilhados e fontes. Uma fonte só para as cores da interface v3.
#include <stdint.h>
#include <lvgl.h>
#include "logic/classify.h"   // Tone
#include "logic/levels.h"     // Level, LayoutKind

// Fontes JetBrains Mono geradas (item 30). Enquanto src/ui/fonts/fonts.h não
// existir, theme_font() devolve Montserrat do mesmo tamanho aproximado e
// THEME_HAS_JBMONO fica 0.
#if __has_include("ui/fonts/fonts.h")
#include "ui/fonts/fonts.h"
#define THEME_HAS_JBMONO 1
#else
#define THEME_HAS_JBMONO 0
#endif

// ---- Paleta (tabela "Paleta", §6; valores RGB 0xRRGGBB) ----
constexpr uint32_t C_BG         = 0x0c1222;  // color-bg (dark)
constexpr uint32_t C_STATUSBAR  = 0x070b18;  // ac-space-950
constexpr uint32_t C_SURFACE    = 0x141e33;  // color-surface (dark): menu
constexpr uint32_t C_SELECTED   = 0x1a2740;  // color-surface-muted (dark)
constexpr uint32_t C_BORDER     = 0x1272c2;  // thep-blue: borda do painel, 3ª faixa
constexpr uint32_t C_TITLE      = 0x2fb5dd;  // thep-cyan: título, 1ª faixa
constexpr uint32_t C_ACCENT     = 0x2f8fd6;  // color-primary (dark): seção, ponto azul
constexpr uint32_t C_TEXT       = 0xe2e8f0;  // color-text (dark)
constexpr uint32_t C_DIM        = 0x8a98a8;  // thep-neutral-400: texto apagado, [ ]
constexpr uint32_t C_UNLIT      = 0x34457a;  // ac-space-500: barra apagada, separador
constexpr uint32_t C_OK         = 0x5caf4e;  // thep-green: nível ok, ponto verde
constexpr uint32_t C_OK_INK     = 0x8ed57f;  // ac-acerto-ink: texto ok
constexpr uint32_t C_WARN       = 0xf0a32a;  // color-warning (dark): nível de aviso
constexpr uint32_t C_DOT_YELLOW = 0xf0c419;  // exceção: ponto amarelo
constexpr uint32_t C_BAD        = 0xf05252;  // exceção: nível alto, ponto vermelho
constexpr uint32_t C_BAD_INK    = 0xff8a8a;  // exceção: texto de erro

// Cor de um tom (tabela "Marca de estado da API", §5):
// DIM -> C_DIM, OK -> C_OK_INK, WARN -> C_WARN, BAD -> C_BAD_INK.
uint32_t tone_color(Tone t);
// Cor de um nível de uso (§6): OK -> C_OK, WARN -> C_WARN, BAD -> C_BAD.
uint32_t level_color(Level l);

// ---- Fontes ----
enum FontRole : uint8_t {
  FONT_11,        // jbmono_11: texto secundário, rodapé, "chave:"
  FONT_13,        // jbmono_13: texto principal, menu
  FONT_BOLD_13,   // jbmono_bold_13: rótulos, títulos de seção e de painel
  FONT_NUM_20,    // jbmono_bold_num_20: % no layout de 4 contas
  FONT_NUM_30,    // jbmono_bold_num_30: layout de 3
  FONT_NUM_38,    // jbmono_bold_num_38: layout de 2
  FONT_NUM_50,    // jbmono_bold_num_50: layout de 1
};
const lv_font_t* theme_font(FontRole r);
// Fonte numérica da % da 5h por layout (item 37).
const lv_font_t* theme_font_num(LayoutKind k);

// ---- Estilos compartilhados (prontos depois de theme_init()) ----
extern lv_style_t st_panel;          // painel: borda 1 C_BORDER, raio 8, pad 14/12/8
extern lv_style_t st_panel_title;    // título sobre a borda: fundo C_BG, pad 0/6, bold 13 C_TITLE
extern lv_style_t st_section;        // título de seção: bold 13 C_ACCENT
extern lv_style_t st_key;            // "chave:": 11 C_DIM
extern lv_style_t st_value;          // valor: 11 C_TEXT
extern lv_style_t st_menu_row;       // linha do menu: 36 px, raio 4, pad 0/10, 13 C_TEXT
extern lv_style_t st_menu_row_sel;   // por cima de st_menu_row: fundo C_SELECTED, texto C_TITLE

// Inicializa os estilos. Chamar uma vez no setup(), depois de lv_init().
void theme_init();
