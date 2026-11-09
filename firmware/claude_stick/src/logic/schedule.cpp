#include "logic/schedule.h"

uint8_t auth_backoff_cycles(uint8_t fails, uint32_t intervalSec, uint32_t maxSec) {
    if (fails == 0) return 0;
    if (intervalSec == 0) intervalSec = 1;
    uint32_t cap = (maxSec + intervalSec - 1) / intervalSec;   // ceil
    if (cap == 0) cap = 1;                                      // pula ao menos 1
    if (cap > 255) cap = 255;
    uint32_t n = (fails - 1u >= 31u) ? 0x80000000u : (1u << (fails - 1u));
    return (uint8_t)(n < cap ? n : cap);
}

bool hist_save_is_due(uint32_t now, uint32_t lastSaveEpoch,
                      uint32_t h5ResetNow, uint32_t lastSavedReset, uint32_t minSec) {
    if (lastSaveEpoch == 0) return true;
    if (h5ResetNow != lastSavedReset) return true;
    if (now < lastSaveEpoch) return true;
    return now - lastSaveEpoch >= minSec;
}

bool hist_file_ok(int n, int head, int cap, int dayN, int maxDays) {
    if (cap <= 0) return false;
    if (n < 0 || n > cap) return false;
    if (head < 0 || head >= cap) return false;
    if (maxDays >= 0 && (dayN < 0 || dayN > maxDays)) return false;
    return true;
}
