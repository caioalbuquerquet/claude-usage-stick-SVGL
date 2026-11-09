#pragma once
// Shell fixo da sessão (Technical Plan §5, item 34): cabeçalho de 50 px (logo,
// ponto de status, "❯ menu"), painel com borda e título sobre a borda, e
// rodapé de 20 px. Criado uma vez depois do desbloqueio; só o conteúdo do
// painel é trocado (app_state.h, shell_show).
//
// Geometria (mockup-home.dc.html): painel em (8, 56) com 464 x 238; área
// interna do conteúdo 438 x 214 (SHELL_CONTENT_W x SHELL_CONTENT_H).
#include <stdint.h>
#include <lvgl.h>
#include "logic/dot_state.h"

constexpr int SHELL_CONTENT_W = 438;
constexpr int SHELL_CONTENT_H = 214;

// Monta o shell em lv_screen_active() (a tela já deve estar limpa).
void shell_create();
// Para o pulso e esquece os ponteiros. Os objetos morrem com a limpeza da
// tela, feita por quem chama (app_state).
void shell_destroy();
bool shell_is_up();

// Container do conteúdo atual (438 x 214, sem padding, sem rolagem). As telas
// o preenchem; não guardar entre trocas de conteúdo.
lv_obj_t* shell_content();
// Uso do app_state: apaga o conteúdo atual (e a camada flutuante) e devolve
// um container novo e vazio. Também tira a ação do título.
lv_obj_t* shell_content_reset();
// Camada transparente sobre a tela inteira, acima do painel e do rodapé, para
// o que não cabe no painel (ex.: teclado de 480 px). Não recebe toque; só os
// filhos recebem. Criada sob demanda e apagada na troca de conteúdo.
lv_obj_t* shell_float();

void shell_set_title(const char* t);
// Toque no título chama cb (nullptr = sem ação). Ex.: /janela e /ritmo usam
// intent_focus_next().
void shell_set_title_action(void (*cb)());

// Cor do ponto (o pulso roda sozinho, num lv_timer de DOT_PULSE_STEP_MS).
void shell_set_dot(DotState d);
// Rodapé: "<pássaro> thep │ N contas · K ✗ │ ↻ <intervalo> │ HH:MM".
// Só reescreve o que mudou.
void shell_set_footer(int nAccounts, int nFailed, int pollSec);
void shell_set_clock(const char* hhmm);

// Botão do menu com fundo C_SELECTED enquanto o menu está aberto.
void shell_set_menu_active(bool open);

// Ordem dos overlays no lv_layer_top, de baixo para cima: menu, momento,
// incidente. Os getters devolvem o objeto raiz de cada overlay (nullptr se
// fechado); overlay_restack() leva momento e depois incidente para o topo.
void shell_set_overlay_getters(lv_obj_t* (*moment)(), lv_obj_t* (*incident)());
void overlay_restack();
