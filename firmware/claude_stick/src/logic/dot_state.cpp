#include "logic/dot_state.h"

static bool is_failure_class(FetchClass c) {
    switch (c) {
        case FC_AUTH: case FC_RATE: case FC_ERR: case FC_SRV:
        case FC_NET: case FC_NOWIFI: case FC_NOCLOCK:
            return true;
        default:
            return false;  // FC_NONE, FC_OK; FC_DISCARDED nunca entra no estado
    }
}

DotState dot_state(const AccountView* v, int n, uint32_t now, uint32_t cycleEndEpoch,
                   bool firstCycleRunning, uint32_t freshSec) {
    int np = 0, nf = 0;
    for (int i = 0; v && i < n; i++) {
        bool pending = v[i].keyOk && v[i].lastClass == FC_NONE;
        if (pending) continue;
        bool failed = !v[i].keyOk || is_failure_class(v[i].lastClass);
        np++;
        if (failed) nf++;
    }
    if (np > 0 && nf == np) return DOT_RED;
    if (nf > 0) return DOT_YELLOW;
    if (firstCycleRunning || (cycleEndEpoch != 0 && now - cycleEndEpoch < freshSec)) return DOT_BLUE;
    return DOT_GREEN;
}
