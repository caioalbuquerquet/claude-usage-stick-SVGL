#pragma once
// Contagem dos resultados do ciclo de consulta (net/scheduler.cpp). Função
// pura, sem Arduino. Cada job leva o id do ciclo que o mandou (NetJob::cycle);
// só os resultados com o id do ciclo aberto contam. Id 0 = "fora de ciclo":
// o RECONNECT do boot (antes do desbloqueio) e o VALIDATE nunca fecham um
// ciclo.
#include <stdint.h>

struct CycleTracker {
    uint16_t open;      // id do ciclo aberto (0 = nenhum)
    int outstanding;    // resultados que faltam para fechar
    uint16_t last;      // último id entregue por cycle_next_id
};

// Próximo id (nunca 0, dá a volta em 65535). Pegar antes de montar os jobs.
uint16_t cycle_next_id(CycleTracker& t);
// Abre o ciclo id com n jobs enfileirados (n <= 0 ou id 0 não abre nada).
void cycle_begin(CycleTracker& t, uint16_t id, int n);
// Conta um resultado. Devolve true quando ele fecha o ciclo aberto; ids 0 ou
// diferentes do aberto não contam.
bool cycle_on_result(CycleTracker& t, uint16_t id);
// Esquece o ciclo aberto (os resultados dele que ainda chegarem não contam).
void cycle_cancel(CycleTracker& t);
bool cycle_running(const CycleTracker& t);
