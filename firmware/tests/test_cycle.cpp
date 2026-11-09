// Testes de src/logic/cycle (contagem do ciclo do agendador por id).
#include "check.h"
#include "logic/cycle.h"

TEST(cycle_fecha_so_com_o_id_aberto) {
    CycleTracker t = {};
    uint16_t id = cycle_next_id(t);
    CHECK(id != 0);
    cycle_begin(t, id, 2);
    CHECK(cycle_running(t));
    CHECK(!cycle_on_result(t, 0));        // RECONNECT do boot / VALIDATE
    CHECK(!cycle_on_result(t, id + 1));   // ciclo velho
    CHECK(!cycle_on_result(t, id));
    CHECK(cycle_running(t));
    CHECK(cycle_on_result(t, id));
    CHECK(!cycle_running(t));
    CHECK(!cycle_on_result(t, id));       // sobra depois de fechar
}

// Cenário do achado 1: o RECONNECT do boot (id 0) chega durante o RECONNECT
// do agendador e depois durante o ciclo de USAGE: nenhum dos dois fecha cedo.
TEST(cycle_reconnect_do_boot_nao_conta) {
    CycleTracker t = {};
    uint16_t rc = cycle_next_id(t);
    cycle_begin(t, rc, 1);
    CHECK(!cycle_on_result(t, 0));        // boot chega primeiro: não fecha
    CHECK(cycle_running(t));
    CHECK(cycle_on_result(t, rc));        // o do agendador fecha
    uint16_t us = cycle_next_id(t);
    CHECK(us != rc);
    cycle_begin(t, us, 3);
    CHECK(!cycle_on_result(t, rc));       // resultado atrasado de outro ciclo
    CHECK(!cycle_on_result(t, us));
    CHECK(!cycle_on_result(t, us));
    CHECK(cycle_on_result(t, us));
}

TEST(cycle_id_nunca_zero_e_cancelar) {
    CycleTracker t = {};
    t.last = 0xFFFF;
    CHECK_EQ(cycle_next_id(t), 1);
    cycle_begin(t, 0, 3);                 // id 0 não abre
    CHECK(!cycle_running(t));
    cycle_begin(t, 5, 0);                 // sem jobs não abre
    CHECK(!cycle_running(t));
    cycle_begin(t, 5, 2);
    cycle_cancel(t);
    CHECK(!cycle_running(t));
    CHECK(!cycle_on_result(t, 5));
}
