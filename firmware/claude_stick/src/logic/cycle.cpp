#include "logic/cycle.h"

uint16_t cycle_next_id(CycleTracker& t) {
    t.last++;
    if (t.last == 0) t.last = 1;
    return t.last;
}

void cycle_begin(CycleTracker& t, uint16_t id, int n) {
    if (id == 0 || n <= 0) return;
    t.open = id;
    t.outstanding = n;
}

bool cycle_on_result(CycleTracker& t, uint16_t id) {
    if (id == 0 || t.open == 0 || id != t.open || t.outstanding <= 0) return false;
    if (--t.outstanding > 0) return false;
    t.open = 0;
    return true;
}

void cycle_cancel(CycleTracker& t) {
    t.open = 0;
    t.outstanding = 0;
}

bool cycle_running(const CycleTracker& t) { return t.open != 0 && t.outstanding > 0; }
