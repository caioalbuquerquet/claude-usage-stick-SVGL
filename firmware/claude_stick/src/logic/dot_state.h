#pragma once
// Cor do ponto de status (Technical Plan §4). Função pura, sem Arduino nem LVGL.
#include <stdint.h>
#include "logic/fetch_class.h"

enum DotState : uint8_t { DOT_GREEN, DOT_BLUE, DOT_YELLOW, DOT_RED };

// Uma entrada por slot salvo (g_accts.used[slot]), em ordem de slot.
struct AccountView {
    bool keyOk;            // false = SEM CHAVE (falhou, nunca pendente)
    FetchClass lastClass;  // FC_NONE = pendente (se keyOk)
};

// Prioridade: vermelho (todas as não pendentes falharam, e há ao menos uma),
// amarelo (alguma falhou), azul (primeiro ciclo ou now - cycleEndEpoch <
// freshSec), verde. O firmware passa DOT_FRESH_SEC em freshSec.
DotState dot_state(const AccountView* v, int n, uint32_t now, uint32_t cycleEndEpoch,
                   bool firstCycleRunning, uint32_t freshSec);
