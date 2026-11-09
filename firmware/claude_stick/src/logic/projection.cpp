#include "logic/projection.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "logic/levels.h"   // utf8_len, fit_label

// Minutos inteiros de `from` até `to`; 0 se `to` já passou.
static int minutes_until(uint32_t to, uint32_t from) {
    return to > from ? (int)((to - from) / 60) : 0;
}

ProjResult projection(const Sample* hist, int n, int head, float pctNow,
                      uint32_t resetEpoch, uint32_t now) {
    ProjResult r = {ProjResult::SEM_DADOS, 0, 0, 0, 0.0f};

    // 1. relógio sem NTP ou janela desconhecida
    if (now < 1000000000UL || resetEpoch == 0) return r;
    const uint32_t ws = resetEpoch - 5 * 3600;

    // 2. já esgotada (antes da contagem de amostras)
    if (pctNow >= 99.5f) {
        r.st = ProjResult::ESGOTADA;
        r.etaEpoch = resetEpoch;
        r.minutes = minutes_until(resetEpoch, now);
        r.pctAtReset = 100;
        return r;
    }

    if (!hist || n < 0) n = 0;
    if (n > HIST_MAX) n = HIST_MAX;
    auto at = [&](int i) -> const Sample& {
        return hist[(head - n + i + 2 * HIST_MAX) % HIST_MAX];
    };

    // 3. pontos na janela + o ponto atual (trend_redraw)
    int k = 0;
    for (int i = 0; i < n; i++) {
        const Sample& s = at(i);
        if (s.t == 0 || s.t < ws || s.t > now) continue;
        k++;
    }
    k++;
    if (k < 3) return r;

    // 4. taxa: primeira amostra da janela dentro dos últimos 45 min
    float rate = 0;
    {
        Sample first = {0, 0, 0};
        for (int i = 0; i < n; i++) {
            const Sample& s = at(i);
            if (s.t == 0 || s.t < ws) continue;
            if (s.t >= now - 2700) { first = s; break; }
        }
        if (first.t != 0 && now > first.t + 300) {
            float dt = (now - first.t) / 60.0f;
            rate = (pctNow - first.h5) / dt;
        }
    }
    r.rate = rate;

    // 5. esgota antes do reset
    if (rate > 0.02f) {
        float minsLeft = (100.0f - pctNow) / rate;
        uint32_t eta = now + (uint32_t)(minsLeft * 60);
        if (eta <= resetEpoch) {
            r.st = ProjResult::ESGOTA;
            r.etaEpoch = eta;
            r.minutes = minutes_until(eta, now);
            return r;
        }
    }

    // 6. chega ao reset sem esgotar
    r.st = ProjResult::OK_ATE_RESET;
    r.etaEpoch = resetEpoch;
    r.minutes = minutes_until(resetEpoch, now);
    float end = pctNow + (rate > 0 ? rate : 0) * r.minutes;
    if (end < 0) end = 0;
    if (end > 100) end = 100;
    r.pctAtReset = (int)(end + 0.5f);
    return r;
}

void projection_text(const ProjResult& r, uint8_t lang, int32_t tzOffsetSec,
                     int maxChars, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    char b[64];
    switch (r.st) {
        case ProjResult::OK_ATE_RESET:
            snprintf(b, sizeof(b), lang ? "\xE2\x9C\x93 ok to reset \xC2\xB7 ~%d%%"
                                        : "\xE2\x9C\x93 n\xC3\xA3o esgota \xC2\xB7 ~%d%%",
                     r.pctAtReset);
            break;
        case ProjResult::ESGOTA: {
            time_t t = (time_t)((int64_t)r.etaEpoch + tzOffsetSec);
            struct tm tm;
            memset(&tm, 0, sizeof(tm));
            gmtime_r(&t, &tm);
            char dur[16];
            if (r.minutes < 60) snprintf(dur, sizeof(dur), "%dm", r.minutes);
            else snprintf(dur, sizeof(dur), "%dh%02dm", r.minutes / 60, r.minutes % 60);
            snprintf(b, sizeof(b), lang ? "\xE2\x9C\x97 runs out %02d:%02d"
                                        : "\xE2\x9C\x97 esgota %02d:%02d",
                     tm.tm_hour, tm.tm_min);
            // " · <tempo>" sai quando não cabe (layout de 3 contas)
            char full[64];
            snprintf(full, sizeof(full), "%s \xC2\xB7 %s", b, dur);
            if (utf8_len(full) <= maxChars) strcpy(b, full);
            break;
        }
        case ProjResult::ESGOTADA:
            snprintf(b, sizeof(b), "%s", lang ? "\xE2\x9C\x97 exhausted" : "\xE2\x9C\x97 esgotada");
            break;
        case ProjResult::SEM_DADOS:
        default:
            snprintf(b, sizeof(b), "%s", lang ? "\xE2\x80\x94 no history"
                                              : "\xE2\x80\x94 sem hist\xC3\xB3rico");
            break;
    }
    // Garantia final: nunca passa de maxChars nem de outSize.
    fit_label(b, maxChars, out, outSize);
}
