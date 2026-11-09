#include "logic/reset_text.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static bool clock_known(uint32_t resetEpoch, uint32_t now) {
    return now >= 1000000000UL && resetEpoch != 0;
}

bool reset_passed(uint32_t resetEpoch, uint32_t now) {
    return clock_known(resetEpoch, now) && resetEpoch <= now;
}

void reset_text(uint32_t resetEpoch, uint32_t now, int32_t tzOffsetSec, uint8_t lang,
                ResetFmt fmt, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    if (!clock_known(resetEpoch, now)) {
        snprintf(out, outSize, "--");
        return;
    }
    time_t t = (time_t)((int64_t)resetEpoch + tzOffsetSec);
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    gmtime_r(&t, &tm);
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d", tm.tm_hour % 24, tm.tm_min % 60);

    switch (fmt) {
        case RESET_AT:
            snprintf(out, outSize, "%s", hm);
            return;
        case RESET_WEEKDAY_AT: {
            static const char* PT[7] = {"dom", "seg", "ter", "qua", "qui", "sex", "s\xC3\xA1\x62"};
            static const char* EN[7] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
            int wd = (tm.tm_wday >= 0 && tm.tm_wday < 7) ? tm.tm_wday : 0;
            snprintf(out, outSize, "%s %s", lang ? EN[wd] : PT[wd], hm);
            return;
        }
        case RESET_IN_AT:
        default:
            break;
    }
    if (resetEpoch <= now) {
        snprintf(out, outSize, "%s \xC2\xB7 %s", lang ? "passed" : "passou", hm);
        return;
    }
    uint32_t mins = (resetEpoch - now) / 60;
    if (mins >= 24 * 60)
        snprintf(out, outSize, "%ud%02uh \xC2\xB7 %s", (unsigned)(mins / 1440),
                 (unsigned)(mins % 1440 / 60), hm);
    else
        snprintf(out, outSize, "%uh%02um \xC2\xB7 %s", (unsigned)(mins / 60),
                 (unsigned)(mins % 60), hm);
}
