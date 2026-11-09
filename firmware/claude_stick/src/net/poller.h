#pragma once
// Task de rede (net_task) e filas loop() <-> net (Technical Plan §1, item 18).
//
// A net_task, fixada no core NET_TASK_CORE, é a única que abre HTTPS e a única
// que mexe no rádio depois do desbloqueio (RECONNECT). Ela nunca chama o LVGL,
// nunca escreve no modelo e nunca acessa LittleFS ou NVS: só lê os tokens com
// token_store_copy* e publica um NetResult na resQ.
//
// O loop() é o dono de todo o resto: enfileira jobs (scheduler.cpp e
// web_api.cpp) e aplica os resultados com poller_apply_results().
#include <stdint.h>
#include "logic/fetch_class.h"
#include "net/api.h"
#include "net/status.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

enum JobType : uint8_t { JOB_USAGE, JOB_STATUS, JOB_VALIDATE, JOB_RECONNECT };

// Job, sem token. gen é uint16_t (token_store); AccountState::gen (uint32_t)
// entra truncado (gen & 0xFFFF) dos dois lados.
//   USAGE:     slot e gen da conta; model = modelo da consulta; modelIdx =
//              índice em g_models[] quando é a sonda da conta em foco, -1 senão.
//   VALIDATE:  slot = g_tokenTargetSlot (só log), gen = web_token_seq().
//   STATUS, RECONNECT: slot = -1, gen = 0.
struct NetJob {
    JobType type;
    int8_t slot;
    uint16_t gen;
    int8_t modelIdx;
    char model[48];
};

// Resultado. cls/http/ms são da consulta que define a classe da conta.
// probeHttp/probeMs: sempre preenchidos quando modelIdx >= 0 (a tentativa com
// o modelo do rodízio); 0 quando modelIdx < 0. netIdx: rede que conectou no
// RECONNECT (-1 se nenhuma, e nos outros tipos).
struct NetResult {
    JobType type;
    int8_t slot;
    uint16_t gen;
    FetchClass cls;
    int http;
    uint16_t ms;
    uint32_t endEpoch;
    int8_t modelIdx;
    int probeHttp;
    uint16_t probeMs;
    int8_t netIdx;
    union {
        UsageData u;     // USAGE, VALIDATE
        ModelStatus s;   // STATUS
    };
};

// Hooks registrados pelo .ino (fase 2) / app_state (fase 3), para src/net não
// chamar interface. Todos rodam no loop(), dentro de poller_apply_results().
struct PollerHooks {
    void (*onStatus)();                       // depois de gravar g_status
    void (*onValidate)(const NetResult& r);   // resultado de um VALIDATE
    void (*onReconnected)();                  // depois de um RECONNECT com FC_OK
};

// Cria jobQ/resQ (8 itens cada) e a net_task. Chamar no setup(), depois de
// token_store_init() e de g_wifi.begin().
void poller_start();
void poller_set_hooks(const PollerHooks& h);
TaskHandle_t poller_task_handle();
// Menor folga de stack já vista na net_task, em bytes (0 sem task).
uint32_t poller_stack_hwm();

// Enfileira um job (loop()). Falso se a fila está cheia ou o poller não
// começou; nesse caso o job não conta como em andamento.
bool poller_enqueue(const NetJob& job);

// Drena a resQ e aplica cada resultado (tabela do item 18). Chama
// scheduler_on_result para todo resultado, inclusive FC_DISCARDED. Devolve
// verdadeiro se aplicou algum resultado (o loop() marca a interface suja).
bool poller_apply_results();

// Verdadeiro quando não há job na fila nem em andamento (contado no loop():
// enfileirado e ainda não drenado da resQ).
bool poller_idle();

// millis() do início da requisição HTTPS em andamento (0 = nenhuma). A task
// grava antes de cada requisição, inclusive o reenvio com PROBE_MODEL, e no
// início de um RECONNECT.
uint32_t poller_req_started_ms();
// Se uma requisição passa de NET_REQ_STUCK_MS: grava lastReset="net_stuck"
// na NVS e chama esp_restart(). Chamar no loop(). A task não é recriada.
void poller_check_stuck();
