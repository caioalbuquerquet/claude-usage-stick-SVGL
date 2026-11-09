// Agendador de ciclos (item 19). Sem LVGL: o momento de limiar vai por hook.
#include "net/scheduler.h"
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <string.h>
#include <esp_heap_caps.h>
#include "config.h"
#include "hw/wifi_manager.h"
#include "model/accounts.h"
#include "net/poller.h"
#include "net/token_store.h"
#include "hw/frame_stats.h"
#include "logic/cycle.h"

// ficam no .ino (regra das fases 0 a 2)
extern int g_pollSec;
extern int g_tzOffset;
extern AccountSlots g_accts;

namespace {

bool s_started = false;
bool s_paused = false;
bool s_timeInit = false;
bool s_clockLogged = false;
CycleTracker s_cyc = {};        // ciclo aberto e resultados que faltam
bool s_reconnecting = false;     // um RECONNECT (do boot ou de um ciclo) está em voo
uint32_t s_cycleStartMs = 0;
uint32_t s_nextCycleMs = 0;
uint32_t s_cycleEndEpoch = 0;
bool s_firstCycleRunning = false;
uint32_t s_probeIdx = 0;
uint32_t s_lastStatusMs = 0;
bool s_statusEver = false;
uint32_t s_noClockSinceMs = 0;
uint32_t s_lastCycleMs = 0;
uint32_t s_cyclesOver = 0;
uint32_t s_cycleNo = 0;

struct Moment { int slot, win, thr; };
Moment s_best = {-1, 0, 0};
bool (*s_momentHook)(int, int, int) = nullptr;

uint32_t interval_ms() { return (uint32_t)g_pollSec * 1000UL; }

// millis() vira em ~49 dias: compara por delta assinado.
bool due(uint32_t at) { return (int32_t)(millis() - at) >= 0; }

bool clock_ok() { return time(nullptr) > 1000000000L; }

void ensure_time() {
    if (s_timeInit || !WiFi.isConnected()) return;
    scheduler_apply_tz();
    s_timeInit = true;
    Serial.println("[NTP] sync iniciado");
}

void set_class_all(FetchClass c) {
    for (int s = 0; s < ACCT_MAX; s++)
        if (g_accts.used[s] && acct[s].keyOk) acct[s].lastClass = c;
}

void begin_cycle(uint16_t id, int n, const char* what) {
    cycle_begin(s_cyc, id, n);
    s_cycleStartMs = millis();
    s_cycleNo++;
    Serial.printf("[CYC] #%u start n=%d %s\n", (unsigned)s_cycleNo, n, what);
}

void end_cycle(const NetResult& r) {
    uint32_t dur = millis() - s_cycleStartMs;
    s_lastCycleMs = dur;
    if (dur > interval_ms()) s_cyclesOver++;
    s_cycleEndEpoch = (uint32_t)time(nullptr);
    s_firstCycleRunning = false;

    char cls[96] = {0};
    for (int s = 0; s < ACCT_MAX; s++) {
        if (!g_accts.used[s]) continue;
        char one[24];
        snprintf(one, sizeof(one), "%s%d:%s", cls[0] ? " " : "", s,
                 acct[s].keyOk ? fetch_class_name(acct[s].lastClass) : "SEMCHAVE");
        strlcat(cls, one, sizeof(cls));
    }
    Serial.printf("[CYC] #%u end %ums classes=%s over=%u\n", (unsigned)s_cycleNo,
                  (unsigned)dur, cls[0] ? cls : "-", (unsigned)s_cyclesOver);
    // Tempo de quadro desde o último log (spike da fase 3, item 34), em ms
    // com uma casa: frame = layout + render + flush; flush = rotação + envio
    // (rot_max e send_max, cada um o maior do seu, não somam flush_max);
    // rows_max = linhas nativas enviadas no maior quadro (480 = tela cheia).
    FrameWindow fw = frame_window_take();
    Serial.printf("[HEAP] int_free=%u int_big=%u int_min=%u psram_free=%u net_hwm=%u "
                  "frame_max=%u.%ums flush_max=%u.%ums rot_max=%u.%ums send_max=%u.%ums "
                  "rows_max=%u frames=%u disp=%s\n",
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                  (unsigned)poller_stack_hwm(),
                  (unsigned)(fw.frameMaxUs / 1000), (unsigned)(fw.frameMaxUs / 100 % 10),
                  (unsigned)(fw.flushMaxUs / 1000), (unsigned)(fw.flushMaxUs / 100 % 10),
                  (unsigned)(fw.rotateMaxUs / 1000), (unsigned)(fw.rotateMaxUs / 100 % 10),
                  (unsigned)(fw.sendMaxUs / 1000), (unsigned)(fw.sendMaxUs / 100 % 10),
                  (unsigned)fw.rowsMax,
                  (unsigned)fw.frames, frame_mode());

    moment_flush();
    s_nextCycleMs = millis() + interval_ms();
    if (r.type == JOB_RECONNECT && r.cls == FC_OK) s_nextCycleMs = millis();   // consulta já
}

// Copia as redes salvas para o token_store e enfileira um RECONNECT com o id
// de ciclo `cycle` (0 = fora de ciclo). Único lugar que monta esse job.
bool enqueue_reconnect(uint16_t cycle) {
    WiFiManager::SavedNet nets[MAX_SAVED_NETWORKS];
    int n = g_wifi.copySaved(nets);
    token_store_set_reconnect(nets, n);
    memset(nets, 0, sizeof(nets));
    NetJob j = {};
    j.type = JOB_RECONNECT;
    j.slot = -1;
    j.modelIdx = -1;
    j.cycle = cycle;
    if (!poller_enqueue(j)) return false;
    s_reconnecting = true;
    return true;
}

}  // namespace

void scheduler_apply_tz() { configTime(g_tzOffset * 3600, 0, NTP_SERVER_1, NTP_SERVER_2); }

void scheduler_start() {
    s_started = true;
    s_paused = false;
    s_firstCycleRunning = true;
    s_nextCycleMs = millis();
    Serial.println("[CYC] unlock");
}

bool scheduler_request_reconnect() {
    if (g_wifi.getSavedCount() <= 0 || s_reconnecting) return false;
    return enqueue_reconnect(0);
}

void scheduler_stop() {
    s_started = false;
    cycle_cancel(s_cyc);                 // resultados em voo não contam mais
    s_firstCycleRunning = false;
    s_best.slot = -1;
}

void scheduler_tick() {
    // NTP já no onboarding: o VALIDATE também precisa de relógio para o TLS.
    if (!s_timeInit && WiFi.isConnected()) ensure_time();
    if (!s_clockLogged && clock_ok()) {
        s_clockLogged = true;
        Serial.printf("[NTP] relogio pronto (uptime %us)\n", (unsigned)(millis() / 1000));
    }
    if (!s_started || s_paused || cycle_running(s_cyc)) return;
    // RECONNECT do boot ainda em voo: o resultado decide (FC_OK consulta já).
    if (s_reconnecting) return;
    if (!due(s_nextCycleMs)) return;

    const uint16_t id = cycle_next_id(s_cyc);
    if (!WiFi.isConnected()) {
        s_noClockSinceMs = 0;
        set_class_all(FC_NOWIFI);
        if (enqueue_reconnect(id)) {
            begin_cycle(id, 1, "reconnect (sem WiFi)");
        } else {
            s_nextCycleMs = millis() + interval_ms();
        }
        return;
    }

    if (!clock_ok()) {
        if (s_noClockSinceMs == 0) {
            s_noClockSinceMs = millis();
            if (!s_noClockSinceMs) s_noClockSinceMs = 1;
            Serial.println("[CYC] aguardando relogio (NTP)");
        }
        if (millis() - s_noClockSinceMs > (uint32_t)NTP_GRACE_SEC * 1000UL) {
            bool first = false;
            for (int s = 0; s < ACCT_MAX; s++)
                if (g_accts.used[s] && acct[s].keyOk && acct[s].lastClass != FC_NOCLOCK) first = true;
            if (first) Serial.printf("[CYC] sem relogio ha %ds -> NOCLOCK\n", NTP_GRACE_SEC);
            set_class_all(FC_NOCLOCK);
        }
        return;
    }
    s_noClockSinceMs = 0;

    int n = 0, nUsage = 0;
    char probeTxt[40] = "sonda=-";
    for (int s = 0; s < ACCT_MAX; s++) {
        if (!g_accts.used[s] || !acct[s].keyOk) continue;   // SEM CHAVE não é consultado
        if (acct[s].backoffCycles > 0) {
            acct[s].backoffCycles--;
            Serial.printf("[CYC] slot %d backoff %u\n", s, (unsigned)acct[s].backoffCycles);
            continue;
        }
        NetJob j = {};
        j.type = JOB_USAGE;
        j.slot = (int8_t)s;
        j.gen = (uint16_t)acct[s].gen;
        j.modelIdx = -1;
        j.cycle = id;
        strlcpy(j.model, PROBE_MODEL, sizeof(j.model));
        bool probe = (s == g_accts.active);
        if (probe) {
            int mi = (int)(s_probeIdx % NMODELS);
            j.modelIdx = (int8_t)mi;
            strlcpy(j.model, g_models[mi].id, sizeof(j.model));
        }
        if (!poller_enqueue(j)) continue;
        if (probe) {
            snprintf(probeTxt, sizeof(probeTxt), "sonda=%s@slot%d", g_models[j.modelIdx].name, s);
            s_probeIdx++;                       // só avança quando a sonda é enviada
        }
        n++;
        nUsage++;
    }
    bool status = false;
    if (!s_statusEver || millis() - s_lastStatusMs >= (uint32_t)STATUS_POLL_SEC * 1000UL) {
        NetJob j = {};
        j.type = JOB_STATUS;
        j.slot = -1;
        j.modelIdx = -1;
        j.cycle = id;
        if (poller_enqueue(j)) {
            s_lastStatusMs = millis();
            s_statusEver = true;
            status = true;
            n++;
        }
    }
    if (n == 0) {                                // tudo em espera: sem ciclo
        s_nextCycleMs = millis() + interval_ms();
        return;
    }
    char what[80];
    snprintf(what, sizeof(what), "(usage=%d status=%d) %s", nUsage, status ? 1 : 0, probeTxt);
    begin_cycle(id, n, what);
}

void scheduler_on_result(const NetResult& r) {
    if (r.type == JOB_RECONNECT) {
        s_reconnecting = false;
        // RECONNECT do boot (fora de ciclo) que conectou: se já desbloqueou,
        // o primeiro ciclo de consulta começa já (antes disso o
        // scheduler_start() já agenda para o próximo tick).
        if (r.cycle == 0 && r.cls == FC_OK) s_nextCycleMs = millis();
    }
    // Só conta o que o ciclo aberto mandou: VALIDATE e o RECONNECT do boot
    // vêm com cycle 0, e um resultado de ciclo cancelado tem outro id.
    if (cycle_on_result(s_cyc, r.cycle)) end_cycle(r);
}

void scheduler_request_refresh() {
    if (!cycle_running(s_cyc)) {
        s_nextCycleMs = millis();
        Serial.println("[CYC] atualizar agora");
    }
}

void scheduler_pause() {
    if (!s_paused) Serial.println("[CYC] pausado (tela de WiFi)");
    s_paused = true;
}
void scheduler_resume() {
    if (s_paused) Serial.println("[CYC] retomado");
    s_paused = false;
}

void scheduler_offer_moment(int slot, ThrHit hit) {
    if (hit.win < 0) return;
    if (s_best.slot < 0 || hit.thr > s_best.thr) s_best = Moment{slot, hit.win, hit.thr};
}

void moment_flush() {
    if (s_best.slot < 0 || !s_momentHook) return;
    if (s_momentHook(s_best.slot, s_best.win, s_best.thr)) {
        Serial.printf("[MOMENT] slot %d janela %s %d%%\n", s_best.slot,
                      s_best.win == 0 ? "5h" : "7d", s_best.thr);
        s_best.slot = -1;
    }
}

void scheduler_set_moment_hook(bool (*show)(int slot, int win, int thr)) { s_momentHook = show; }

uint32_t scheduler_last_cycle_ms() { return s_lastCycleMs; }
uint32_t scheduler_cycles_over_interval() { return s_cyclesOver; }
uint32_t scheduler_cycle_end_epoch() { return s_cycleEndEpoch; }
bool scheduler_first_cycle_running() { return s_firstCycleRunning; }
bool scheduler_cycle_running() { return cycle_running(s_cyc); }
