// Testes de dot_state (item 10 / Technical Plan §4).
#include "check.h"
#include "logic/dot_state.h"

static const uint32_t T0 = 1700000000u;  // fim do último ciclo
static const uint32_t FRESH = 10;
// Bem depois do fim do ciclo: azul já expirou.
static const uint32_t LATE = T0 + 1000;

static AccountView ok()       { return AccountView{true, FC_OK}; }
static AccountView pend()     { return AccountView{true, FC_NONE}; }
static AccountView fail(FetchClass c) { return AccountView{true, c}; }
static AccountView nokey()    { return AccountView{false, FC_NONE}; }

TEST(dot_rule_red_all_failed) {
    AccountView v[] = {fail(FC_AUTH), fail(FC_NET)};
    CHECK_EQ(dot_state(v, 2, LATE, T0, false, FRESH), DOT_RED);
    // Vermelho vence azul (primeiro ciclo e janela fresca).
    CHECK_EQ(dot_state(v, 2, T0 + 1, T0, true, FRESH), DOT_RED);
}

TEST(dot_rule_red_each_failure_class) {
    const FetchClass cls[] = {FC_AUTH, FC_RATE, FC_ERR, FC_SRV, FC_NET, FC_NOWIFI, FC_NOCLOCK};
    for (FetchClass c : cls) {
        AccountView v[] = {fail(c)};
        CHECK_EQ(dot_state(v, 1, LATE, T0, false, FRESH), DOT_RED);
    }
}

TEST(dot_rule_red_nowifi_and_noclock_everywhere) {
    AccountView w[] = {fail(FC_NOWIFI), fail(FC_NOWIFI), fail(FC_NOWIFI)};
    CHECK_EQ(dot_state(w, 3, LATE, T0, false, FRESH), DOT_RED);
    AccountView c[] = {fail(FC_NOCLOCK), fail(FC_NOCLOCK)};
    CHECK_EQ(dot_state(c, 2, LATE, T0, false, FRESH), DOT_RED);
}

TEST(dot_rule_yellow_some_failed) {
    AccountView v[] = {ok(), fail(FC_SRV)};
    CHECK_EQ(dot_state(v, 2, LATE, T0, false, FRESH), DOT_YELLOW);
    // Amarelo vence azul (empate de prioridade).
    CHECK_EQ(dot_state(v, 2, T0 + 1, T0, false, FRESH), DOT_YELLOW);
    CHECK_EQ(dot_state(v, 2, LATE, T0, true, FRESH), DOT_YELLOW);
}

TEST(dot_rule_blue_fresh_window) {
    AccountView v[] = {ok(), ok()};
    CHECK_EQ(dot_state(v, 2, T0, T0, false, FRESH), DOT_BLUE);
    CHECK_EQ(dot_state(v, 2, T0 + 9, T0, false, FRESH), DOT_BLUE);
    CHECK_EQ(dot_state(v, 2, T0 + 10, T0, false, FRESH), DOT_GREEN);
}

TEST(dot_rule_blue_first_cycle) {
    AccountView v[] = {ok()};
    CHECK_EQ(dot_state(v, 1, LATE, T0, true, FRESH), DOT_BLUE);
    // Sem ciclo concluído ainda (cycleEndEpoch == 0).
    CHECK_EQ(dot_state(v, 1, T0, 0, true, FRESH), DOT_BLUE);
}

TEST(dot_rule_green_otherwise) {
    AccountView v[] = {ok(), ok()};
    CHECK_EQ(dot_state(v, 2, LATE, T0, false, FRESH), DOT_GREEN);
    // cycleEndEpoch == 0 nunca dá azul por janela.
    CHECK_EQ(dot_state(v, 2, 5, 0, false, FRESH), DOT_GREEN);
}

TEST(dot_all_pending_never_red) {
    AccountView v[] = {pend(), pend(), pend()};
    CHECK_EQ(dot_state(v, 3, T0, 0, true, FRESH), DOT_BLUE);    // desbloqueio
    CHECK_EQ(dot_state(v, 3, LATE, T0, false, FRESH), DOT_GREEN);
}

TEST(dot_zero_accounts) {
    CHECK_EQ(dot_state(nullptr, 0, T0, 0, true, FRESH), DOT_BLUE);
    CHECK_EQ(dot_state(nullptr, 0, LATE, T0, false, FRESH), DOT_GREEN);
    AccountView v[] = {fail(FC_NET)};
    CHECK_EQ(dot_state(v, 0, LATE, T0, false, FRESH), DOT_GREEN);
}

TEST(dot_pending_never_counts_as_failure) {
    AccountView a[] = {pend(), ok()};
    CHECK_EQ(dot_state(a, 2, LATE, T0, false, FRESH), DOT_GREEN);
    // Uma pendente + uma em falha → vermelho (a pendente não conta).
    AccountView b[] = {pend(), fail(FC_AUTH)};
    CHECK_EQ(dot_state(b, 2, LATE, T0, false, FRESH), DOT_RED);
    AccountView c[] = {pend(), ok(), fail(FC_AUTH)};
    CHECK_EQ(dot_state(c, 3, LATE, T0, false, FRESH), DOT_YELLOW);
}

TEST(dot_no_key_is_failure_never_pending) {
    // SEM CHAVE sozinho: falhou (vermelho), mesmo com lastClass == FC_NONE.
    AccountView a[] = {nokey()};
    CHECK_EQ(dot_state(a, 1, LATE, T0, false, FRESH), DOT_RED);
    CHECK_EQ(dot_state(a, 1, T0, 0, true, FRESH), DOT_RED);
    // SEM CHAVE + pendente: não é "todas pendentes", então vermelho.
    AccountView b[] = {nokey(), pend()};
    CHECK_EQ(dot_state(b, 2, LATE, T0, false, FRESH), DOT_RED);
    // SEM CHAVE + ok → amarelo.
    AccountView c[] = {ok(), nokey()};
    CHECK_EQ(dot_state(c, 2, LATE, T0, false, FRESH), DOT_YELLOW);
    // keyOk=false vence qualquer lastClass, inclusive FC_OK.
    AccountView d[] = {AccountView{false, FC_OK}};
    CHECK_EQ(dot_state(d, 1, LATE, T0, false, FRESH), DOT_RED);
}

TEST(dot_discarded_has_no_effect) {
    // FC_DISCARDED nunca entra no AccountState; se aparecer, não conta como
    // falha (a conta é tratada como não pendente e ok).
    AccountView a[] = {fail(FC_DISCARDED), ok()};
    CHECK_EQ(dot_state(a, 2, LATE, T0, false, FRESH), DOT_GREEN);
    AccountView b[] = {fail(FC_DISCARDED), fail(FC_NET)};
    CHECK_EQ(dot_state(b, 2, LATE, T0, false, FRESH), DOT_YELLOW);
}
