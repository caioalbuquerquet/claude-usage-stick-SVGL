#pragma once
// Segredos compartilhados entre o loop() e a net_task (v3, item 16 do plano).
//
// Tudo aqui fica sob um único mutex FreeRTOS (tokMx):
//   · tokens[ACCT_MAX][TOKEN_BUF_SIZE] + gen[ACCT_MAX]: token OAuth decifrado
//     de cada slot e a geração com que foi gravado;
//   · pending: token em validação (POST /token → job VALIDATE);
//   · reconnect: cópia das redes salvas para o job RECONNECT, para a task
//     não ler o NVS;
//   · join: rede escolhida na tela de WiFi (SSID + senha) para o job
//     WIFI_JOIN; o loop() grava no NVS se conectar.
//
// Quem escreve é o loop(); a net_task só copia. Toda cópia que sai daqui vai
// para um buffer do chamador, que deve zerá-lo (memset) ao terminar. Tokens
// e senhas nunca são logados.
//
// gen: uint16_t, como no NetJob (item 18). AccountState::gen é uint32_t; os
// dois lados truncam do mesmo jeito (gen & 0xFFFF), então a comparação vale.
#include <stdint.h>
#include <stddef.h>
#include "hw/wifi_manager.h"   // WiFiManager::SavedNet, MAX_SAVED_NETWORKS
#include "model/accounts.h"    // ACCT_MAX

#define TOKEN_MAX_LEN   199                   // maior token aceito (sem o NUL)
#define TOKEN_BUF_SIZE  (TOKEN_MAX_LEN + 1)   // 200, buffer de um token

// Cria o mutex. Chamar no setup(), antes de poller_start(). As outras funções
// também criam o mutex na primeira chamada, mas só é seguro se essa primeira
// chamada vier do loop() antes de a net_task existir.
void token_store_init();

// ── Tokens por slot ──────────────────────────────────────
// Grava o token do slot com a geração gen. Falso (e slot zerado, com gen
// gravado) se o slot é inválido, tok é nulo/vazio ou passa de TOKEN_MAX_LEN.
bool token_store_set(int slot, uint16_t gen, const char* tok);
// Zera o token (memset) e grava newGen. Ao remover uma conta:
// acct_bump_gen(slot) e depois token_store_clear(slot, acct[slot].gen).
void token_store_clear(int slot, uint16_t newGen);
// Zera todos os tokens e a cópia das redes (factory reset). Os gen ficam.
void token_store_clear_all();
// Copia o token para buf. Falso (buf zerado) se o slot é inválido, está vazio,
// o gen não bate ou buf é pequeno demais. Usada pela net_task: com falso, ela
// publica FC_DISCARDED sem fazer a requisição.
bool token_store_copy(int slot, uint16_t gen, char* buf, size_t bufLen);

// ── Token em validação (VALIDATE) ────────────────────────
// Falso se já há um token em validação, ou se tok é nulo/vazio/longo demais.
bool token_store_set_pending(const char* tok);
// Copia sem apagar (net_task). Falso (buf zerado) se não há token pendente.
bool token_store_copy_pending(char* buf, size_t bufLen);
// Copia e zera a área (loop(), ao cifrar no ST_SETUP_PIN). Falso se vazia.
bool token_store_take_pending(char* buf, size_t bufLen);
void token_store_clear_pending();

// ── Redes para o RECONNECT ───────────────────────────────
// loop(): grava a cópia (g_wifi.copySaved) antes de enfileirar o RECONNECT.
// n é limitado a [0, MAX_SAVED_NETWORKS]; o resto da área é zerado.
void token_store_set_reconnect(const WiFiManager::SavedNet* nets, int n);
// net_task: copia para out[MAX_SAVED_NETWORKS] e devolve quantas.
int  token_store_copy_reconnect(WiFiManager::SavedNet* out);

// ── Rede nova para o WIFI_JOIN ───────────────────────────
// loop(): grava antes de enfileirar (ssid até 32 bytes, pass até 64).
void token_store_set_join(const char* ssid, const char* pass);
// net_task (para conectar) e loop() (para gravar no NVS). Falso sem rede.
bool token_store_copy_join(WiFiManager::SavedNet* out);
void token_store_clear_join();
