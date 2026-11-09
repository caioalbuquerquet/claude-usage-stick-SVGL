#pragma once
// Tela de abertura (ST_SPLASH): pássaro em braille 56x56, logo THEP 264x96
// com uma faixa de brilho que o atravessa uma vez e três pontos de carga.
// Aparece no boot, antes do PIN (ou do onboarding), por SPLASH_MS; depois a
// troca de estado limpa a tela e screen_splash_reset() devolve a memória.
//
// O logo é um buffer RGB565A8 na PSRAM (fora do pool do LVGL), montado a
// partir do alfa em 1 bit de img_thep_logo_splash com as 3 faixas pintadas;
// o brilho é misturado nesse buffer só nas colunas que a faixa tocou, e só
// essas colunas são invalidadas. Nenhum quadro pré-renderizado no flash.
#include <stdint.h>
#include "app/app_state.h"

constexpr uint32_t SPLASH_MS = 4000;        // duração da animação
constexpr uint32_t SPLASH_FRAME_MS = 100;   // um quadro a cada 100 ms (~10 fps)

// ScreenOps.build de ST_SPLASH: monta a tela parada (faixa fora do logo,
// pontos apagados). O setup() a desenha logo depois do display, para o boot
// não ficar com a tela preta enquanto carrega NVS, LittleFS e o WiFi.
void screen_splash_build();
// Começa a animação de SPLASH_MS; no fim loga quadros e frame_max e pede
// request_state(next). Chamado no fim do setup(), quando nada mais bloqueia o
// loop(). Sem a tela montada, pede next na hora.
void screen_splash_start(AppState next);
// Esquece os objetos, apaga o timer e libera o buffer do logo (ui_reset).
void screen_splash_reset();
