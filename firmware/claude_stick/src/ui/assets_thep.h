// GERADO por tools/gen_hermes_assets.py — não editar na mão.
#pragma once
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// logo THEP do cabeçalho (104x38, RGB565A8)
LV_IMAGE_DECLARE(img_thep_logo_hdr);
// pássaro em braille: 1 conta, limiar e incidente (recolor cinza) (124x124, RGB565A8)
LV_IMAGE_DECLARE(img_bird_braille_124);
// pássaro em braille do rodapé (13x13, RGB565A8)
LV_IMAGE_DECLARE(img_bird_braille_13);
// logo THEP da tela de abertura, só o alfa: as faixas são pintadas no firmware (264x96, A1)
LV_IMAGE_DECLARE(img_thep_logo_splash);
// pássaro em braille da tela de abertura (56x56, RGB565A8)
LV_IMAGE_DECLARE(img_bird_braille_56);

// Faixas de img_thep_logo_splash, de cima para baixo (RGB888).
#define THEP_LOGO_SPLASH_BAND_H 32
#define THEP_LOGO_SPLASH_BANDS { 0x2fb5dd, 0x2f8fd6, 0x1272c2 }

#ifdef __cplusplus
}
#endif
