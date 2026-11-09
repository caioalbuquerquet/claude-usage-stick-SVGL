#pragma once
// Menu modal no lv_layer_top (Technical Plan §5, item 35): 262 x 262 px,
// ancorado a 50 px do topo e 8 px da direita, 7 linhas de 36 px:
// "↻ Atualizar agora", /agora, /modelos, /janela, /ritmo, /contas, /ajustes.
// A tela atual fica marcada com ❯. Tocar fora ou escolher um item fecha.

// Abre se fechado, fecha se aberto (botão "❯ menu" do shell).
void overlay_menu_toggle();
void overlay_menu_open();
// Fecha o menu aberto já. Pode ser chamada de um evento do próprio menu (que
// antes o esconde): o menu escondido só é apagado por overlay_menu_gc().
void overlay_menu_close();
bool overlay_menu_is_open();
// Apaga o menu escondido por um evento dele. Chamar só no loop(), fora de
// qualquer evento do LVGL (app_state_tick, e antes de limpar o lv_layer_top).
void overlay_menu_gc();
