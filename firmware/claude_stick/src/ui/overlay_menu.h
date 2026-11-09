#pragma once
// Menu modal no lv_layer_top (Technical Plan §5, item 35): 262 x 262 px,
// ancorado a 50 px do topo e 8 px da direita, 7 linhas de 36 px:
// "↻ Atualizar agora", /agora, /modelos, /janela, /ritmo, /contas, /ajustes.
// A tela atual fica marcada com ❯. Tocar fora ou escolher um item fecha.

// Abre se fechado, fecha se aberto (botão "❯ menu" do shell).
void overlay_menu_toggle();
void overlay_menu_open();
// Fecha já (chamar fora de um evento do próprio menu; o menu, nos seus
// eventos, só esconde e deixa a remoção para overlay_menu_gc()).
void overlay_menu_close();
bool overlay_menu_is_open();
// Apaga o menu escondido por um evento dele. Chamar no loop() (app_state_tick).
void overlay_menu_gc();
