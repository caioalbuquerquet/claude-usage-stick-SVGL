// net_task, filas e aplicação dos resultados (item 18). Não chama o LVGL
// (firmware/tests/run.sh confere): o que mexe na tela passa pelos hooks.
#include "net/poller.h"
#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include <string.h>
#include <esp_system.h>
#include <mbedtls/platform_util.h>
#include "freertos/queue.h"
#include "config.h"
#include "hw/wifi_manager.h"
#include "logic/classify.h"
#include "model/account_state.h"
#include "net/scheduler.h"
#include "net/token_store.h"

// ficam no .ino (regra das fases 0 a 2)
extern Preferences g_prefs;
extern int g_pollSec;
extern int g_tzOffset;

namespace {

QueueHandle_t s_jobQ = nullptr;
QueueHandle_t s_resQ = nullptr;
TaskHandle_t s_task = nullptr;
PollerHooks s_hooks = {nullptr, nullptr, nullptr};

// Escrito só pela task, lido pelo loop(). 32 bits: leitura atômica no Xtensa.
volatile uint32_t s_reqStartMs = 0;
// Jobs enfileirados e ainda não drenados da resQ. Só o loop() mexe.
int s_inflight = 0;

const char* job_name(JobType t) {
    switch (t) {
        case JOB_USAGE:     return "USAGE";
        case JOB_STATUS:    return "STATUS";
        case JOB_VALIDATE:  return "VALIDATE";
        case JOB_RECONNECT: return "RECONNECT";
    }
    return "?";
}

void mark_req_start() {
    uint32_t m = millis();
    s_reqStartMs = m ? m : 1;   // 0 é o sentinela de "sem requisição"
}

// Nome curto do modelo para o log ("Sonnet"), ou o id se não estiver na tabela.
const char* model_short(int8_t idx, const char* id) {
    if (idx >= 0 && idx < NMODELS) return g_models[idx].name;
    return id;
}

// ── execução de um job (na net_task) ──────────────────────
void run_usage(const NetJob& job, NetResult& r, char* tok, size_t tokLen) {
    if (!token_store_copy(job.slot, job.gen, tok, tokLen)) {
        r.cls = FC_DISCARDED;
        return;
    }
    const char* model = (job.modelIdx >= 0 && job.model[0]) ? job.model : PROBE_MODEL;
    UsageData u = {};
    int http = -1;
    uint16_t ms = 0;
    mark_req_start();
    bool hh = fetchUsage(tok, model, u, &http, &ms);
    FetchClass cls = classify_usage(http, hh);
    if (job.modelIdx >= 0) {
        r.probeHttp = http;
        r.probeMs = ms;
        bool retry = probe_needs_retry(hh, model, PROBE_MODEL);
        Serial.printf("[slot %d] PROBE %s http=%d ms=%u%s\n", job.slot,
                      model_short(job.modelIdx, model), http, (unsigned)ms,
                      retry ? " (sem headers: repete com PROBE_MODEL)" : "");
        if (retry) {
            memset(&u, 0, sizeof(u));
            mark_req_start();
            hh = fetchUsage(tok, PROBE_MODEL, u, &http, &ms);
            cls = classify_usage(http, hh);
        }
    }
    r.cls = cls;
    r.http = http;
    r.ms = ms;
    r.u = u;
}

void run_validate(const NetJob& job, NetResult& r, char* tok, size_t tokLen) {
    (void)job;
    if (!token_store_copy_pending(tok, tokLen)) {
        r.cls = FC_DISCARDED;
        return;
    }
    UsageData u = {};
    int http = -1;
    uint16_t ms = 0;
    mark_req_start();
    bool hh = fetchUsage(tok, PROBE_MODEL, u, &http, &ms);
    r.cls = classify_usage(http, hh);
    r.http = http;
    r.ms = ms;
    r.u = u;
}

void run_status(NetResult& r) {
    ModelStatus s;
    memset(&s, 0, sizeof(s));
    uint32_t t0 = millis();
    mark_req_start();
    bool ok = fetchModelStatus(s);
    uint32_t dt = millis() - t0;
    r.cls = ok ? FC_OK : FC_NET;
    r.http = 0;
    r.ms = dt > 65000 ? 65000 : (uint16_t)dt;
    r.s = s;
}

void run_reconnect(NetResult& r) {
    static WiFiManager::SavedNet nets[MAX_SAVED_NETWORKS];   // só a task usa
    uint32_t t0 = millis();
    mark_req_start();
    int n = token_store_copy_reconnect(nets);
    int idx = -1;
    bool ok = WiFiManager::connectList(nets, n, WIFI_CONNECT_TIMEOUT_MS, &idx);
    mbedtls_platform_zeroize(nets, sizeof(nets));
    uint32_t dt = millis() - t0;
    r.cls = ok ? FC_OK : FC_NOWIFI;
    r.http = 0;
    r.ms = dt > 65000 ? 65000 : (uint16_t)dt;
    r.netIdx = (int8_t)idx;
}

void log_result(const NetJob& job, const NetResult& r) {
    switch (job.type) {
        case JOB_USAGE:
        case JOB_VALIDATE:
            if (r.cls == FC_DISCARDED) {
                Serial.printf("[slot %d] %s cls=DISCARDED (gen %u sem token na tabela)\n",
                              job.slot, job_name(job.type), (unsigned)job.gen);
            } else if (r.cls == FC_OK) {
                Serial.printf("[slot %d] %s http=%d cls=%s ms=%u h5=%.1f d7=%.1f\n",
                              job.slot, job_name(job.type), r.http, fetch_class_name(r.cls),
                              (unsigned)r.ms, r.u.h5, r.u.d7);
            } else {
                Serial.printf("[slot %d] %s http=%d cls=%s ms=%u\n", job.slot,
                              job_name(job.type), r.http, fetch_class_name(r.cls), (unsigned)r.ms);
            }
            break;
        case JOB_STATUS:
            Serial.printf("[NET] STATUS cls=%s ms=%u\n", fetch_class_name(r.cls), (unsigned)r.ms);
            break;
        case JOB_RECONNECT:
            Serial.printf("[NET] RECONNECT cls=%s ms=%u net=%d\n", fetch_class_name(r.cls),
                          (unsigned)r.ms, r.netIdx);
            break;
    }
}

void net_task(void*) {
    NetJob job;
    for (;;) {
        if (xQueueReceive(s_jobQ, &job, portMAX_DELAY) != pdTRUE) continue;
        NetResult r;
        memset(&r, 0, sizeof(r));
        r.type = job.type;
        r.slot = job.slot;
        r.gen = job.gen;
        r.modelIdx = job.modelIdx;
        r.netIdx = -1;
        r.cls = FC_NONE;

        char tok[TOKEN_BUF_SIZE];
        switch (job.type) {
            case JOB_USAGE:     run_usage(job, r, tok, sizeof(tok)); break;
            case JOB_VALIDATE:  run_validate(job, r, tok, sizeof(tok)); break;
            case JOB_STATUS:    run_status(r); break;
            case JOB_RECONNECT: run_reconnect(r); break;
        }
        mbedtls_platform_zeroize(tok, sizeof(tok));   // o token nunca sai da task

        r.endEpoch = (uint32_t)time(nullptr);
        s_reqStartMs = 0;
        log_result(job, r);
        xQueueSend(s_resQ, &r, portMAX_DELAY);
    }
}

// ── aplicação (no loop()) ────────────────────────────────
void apply_one(const NetResult& r) {
    switch (r.type) {
        case JOB_USAGE: {
            if (r.cls == FC_DISCARDED || r.slot < 0 || r.slot >= ACCT_MAX) break;
            AccountState& a = acct[r.slot];
            if (r.gen != (uint16_t)a.gen) {
                Serial.printf("[slot %d] USAGE descartado (gen %u != %u)\n", r.slot,
                              (unsigned)r.gen, (unsigned)(uint16_t)a.gen);
                break;
            }
            if (r.modelIdx >= 0 && r.modelIdx < NMODELS) {
                g_models[r.modelIdx].pr.code = r.probeHttp;
                g_models[r.modelIdx].pr.ms = r.probeMs;
                g_models[r.modelIdx].atMs = millis();
            }
            ThrHit hit = {-1, 0};
            acct_apply_result(r, (uint32_t)g_pollSec, g_tzOffset * 3600, &hit);
            if (r.cls == FC_AUTH)
                Serial.printf("[slot %d] AUTH fails=%u -> pula %u ciclo(s)\n", r.slot,
                              (unsigned)a.fails, (unsigned)a.backoffCycles);
            if (hit.win >= 0) scheduler_offer_moment(r.slot, hit);
            break;
        }
        case JOB_STATUS:
            if (r.cls == FC_OK) {
                g_status = r.s;
                if (s_hooks.onStatus) s_hooks.onStatus();
            }
            break;
        case JOB_VALIDATE:
            if (s_hooks.onValidate) s_hooks.onValidate(r);
            break;
        case JOB_RECONNECT:
            if (r.cls == FC_OK) {
                if (r.netIdx > 0) g_wifi.promote(r.netIdx);
                if (s_hooks.onReconnected) s_hooks.onReconnected();
            }
            break;
    }
}

}  // namespace

void poller_start() {
    if (s_task) return;
    s_jobQ = xQueueCreate(8, sizeof(NetJob));
    s_resQ = xQueueCreate(8, sizeof(NetResult));
    if (!s_jobQ || !s_resQ) {
        Serial.println("[NET] FATAL: filas da net_task");
        return;
    }
    BaseType_t ok = xTaskCreatePinnedToCore(net_task, "net_task", NET_TASK_STACK, nullptr, 1,
                                            &s_task, NET_TASK_CORE);
    if (ok != pdPASS) {
        s_task = nullptr;
        Serial.println("[NET] FATAL: xTaskCreatePinnedToCore");
        return;
    }
    Serial.printf("[NET] net_task no core %d, stack %d\n", NET_TASK_CORE, NET_TASK_STACK);
}

void poller_set_hooks(const PollerHooks& h) { s_hooks = h; }
TaskHandle_t poller_task_handle() { return s_task; }
uint32_t poller_stack_hwm() { return s_task ? (uint32_t)uxTaskGetStackHighWaterMark(s_task) : 0; }

bool poller_enqueue(const NetJob& job) {
    if (!s_jobQ || !s_task) return false;
    if (xQueueSend(s_jobQ, &job, 0) != pdTRUE) {
        Serial.printf("[NET] jobQ cheia: %s descartado\n", job_name(job.type));
        return false;
    }
    s_inflight++;
    return true;
}

bool poller_apply_results() {
    if (!s_resQ) return false;
    static NetResult r;   // ~250 bytes: fora da stack do loop()
    bool any = false;
    while (xQueueReceive(s_resQ, &r, 0) == pdTRUE) {
        if (s_inflight > 0) s_inflight--;
        any = true;
        apply_one(r);
        scheduler_on_result(r);
    }
    return any;
}

bool poller_idle() { return s_inflight == 0; }

uint32_t poller_req_started_ms() { return s_reqStartMs; }

void poller_check_stuck() {
    uint32_t st = s_reqStartMs;
    if (st == 0) return;
    uint32_t el = millis() - st;
    if (el <= NET_REQ_STUCK_MS) return;
    Serial.printf("[NET] requisicao travada ha %ums -> lastReset=net_stuck, esp_restart()\n",
                  (unsigned)el);
    g_prefs.putString("lastReset", "net_stuck");
    Serial.flush();
    delay(100);
    esp_restart();
}
