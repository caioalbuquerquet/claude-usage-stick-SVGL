// Estado por conta (v3). Fase 0: globais movidas de claude_stick.ino sem
// mudança de comportamento.
#include "model/account_state.h"

UsageData  g_usage = {};
TokenStats g_tok = {0, 0, 0, 0, 0};

uint8_t g_thrFired[2] = {0, 0};
float g_thrPrev[2] = {-1, -1};
bool g_thrBase = false;
