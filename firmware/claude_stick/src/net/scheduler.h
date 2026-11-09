#pragma once
// Agendador de ciclos de consulta (Technical Plan §2, item 19). Roda no
// loop(): monta os jobs de cada ciclo e os entrega à net_task (poller.h).
// Também é dono do NTP (configTime) e do fuso.
#include <stdint.h>
#include "model/account_state.h"   // ThrHit

struct NetResult;

// Começa a mandar jobs (desbloqueio). Antes disto, scheduler_tick() só cuida
// do NTP. O primeiro ciclo começa no próximo tick.
void scheduler_start();
// Para de mandar jobs (factory reset). Os resultados em voo ainda chegam.
void scheduler_stop();
// No loop(), em todos os estados.
void scheduler_tick();
// poller_apply_results() chama para todo resultado, inclusive FC_DISCARDED.
void scheduler_on_result(const NetResult& r);
// "Atualizar agora": fora de um ciclo, começa um na hora; durante, nada.
void scheduler_request_refresh();
// Tela de WiFi da sessão: para/retoma o envio de jobs.
void scheduler_pause();
void scheduler_resume();

// Momentos de limiar: guarda o maior limiar do ciclo (empate: slot menor) e
// mostra no fim do ciclo, ou quando o overlay aberto fechar.
void scheduler_offer_moment(int slot, ThrHit hit);
void moment_flush();
// show devolve false se não pode mostrar agora (já há momento aberto, ou a
// tela não é a inicial); o momento fica guardado até o próximo flush.
void scheduler_set_moment_hook(bool (*show)(int slot, int win, int thr));

// configTime com o fuso de Ajustes (g_tzOffset).
void scheduler_apply_tz();

uint32_t scheduler_last_cycle_ms();
uint32_t scheduler_cycles_over_interval();
uint32_t scheduler_cycle_end_epoch();
bool scheduler_first_cycle_running();
bool scheduler_cycle_running();
