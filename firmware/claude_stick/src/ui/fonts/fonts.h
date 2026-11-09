#pragma once
// Fontes JetBrains Mono 2.304 da v3, geradas por tools/gen_fonts.sh
// (lv_font_conv 1.5.3, bpp 4, sem compressão). Não edite os .c à mão:
// rode o script de novo.
//
// As fontes de texto já saem com fallback = &lv_font_montserrat_14, gravado
// no const lv_font_t gerado (no LVGL 9 o campo não pode ser atribuído
// depois). A Montserrat 14 atende o LV_SYMBOL_REFRESH (↻) e os outros
// LV_SYMBOL_*. As numéricas não têm fallback.
//
// Licença: SIL OFL 1.1, ver LICENSES.md nesta pasta.

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Texto: ASCII, Latin-1, — • …, │ ─ ╭ ╮ ╰ ╯, █ ░, ⚠ ✓ ✗ ❯
LV_FONT_DECLARE(jbmono_11)       // Regular 11: secundário, rodapé, "chave:"
LV_FONT_DECLARE(jbmono_13)       // Regular 13: texto principal, menu
LV_FONT_DECLARE(jbmono_bold_13)  // Bold 13: rótulos de conta, títulos

// % de destaque por layout: só 0-9, % e -
LV_FONT_DECLARE(jbmono_bold_num_20)
LV_FONT_DECLARE(jbmono_bold_num_30)
LV_FONT_DECLARE(jbmono_bold_num_38)
LV_FONT_DECLARE(jbmono_bold_num_50)

#ifdef __cplusplus
}
#endif
