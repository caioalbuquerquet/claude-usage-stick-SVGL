#pragma once
// Momento de limiar (Technical Plan §5 e §6, item 40): overlay no
// lv_layer_top com o pássaro em braille, o rótulo da conta, a % contando e a
// barra de blocos acendendo. 25/50: o pássaro quica; 70: treme; 100: cinza
// com borda vermelha piscando. Toque ou tempo fecham.
#include <lvgl.h>

// Hook do agendador (scheduler_set_moment_hook, item 19): devolve false sem
// fazer nada se já há um momento aberto, fora do shell ou com troca de tela
// pendente; senão abre o overlay da conta `slot` e devolve true. win: 0 = 5h,
// 1 = semana; thr: 25/50/70/100.
bool show_moment(int slot, int win, int thr);
// Fecha sem chamar moment_flush() (limpeza de tela). O fechamento por toque
// e por tempo chama moment_flush() depois (próximo momento guardado).
void moment_close();
// Anima o momento aberto; chamar a cada passada do loop() (no-op se fechado).
void moment_tick();
// Raiz do overlay (nullptr = fechado), para o overlay_restack().
lv_obj_t* moment_obj();
