#pragma once
// Telas de src/ui/screen_*.cpp (item 39). Os conteúdos do shell montam dentro
// de shell_content() (438 x 214) e são registrados no app_state pelo .ino; as
// telas fora do shell (PIN, WiFi e token do onboarding) montam em
// lv_screen_active() com a moldura ui_frame() (widgets.h).
//
// Cada tela guarda ponteiros de objetos; screen_*_reset() os esquece e é
// chamada no app_on_ui_reset do .ino, antes de qualquer limpeza.
#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>
#include "model/accounts.h"   // ACCT_LBL_MAX

// ---- /agora (item 36): um card por slot salvo, nos 4 layouts do item 37 ----
void screen_home_build(lv_obj_t* parent);
// Chegou resultado da net_task: reescreve só os valores. Se o conjunto de
// slots salvos mudou, pede a recriação do conteúdo (shell_show(HOME)).
void screen_home_refresh();
// Uma vez por segundo: contagens, idade dos cards em falha.
void screen_home_tick();
void screen_home_reset();

// ---- /modelos (item 38): lista em texto, uma linha por modelo ----
void screen_models_build(lv_obj_t* parent);
void screen_models_refresh();
void screen_models_reset();

// ---- /janela: gráfico da 5h da conta em foco + projection_text ----
void screen_trend_build(lv_obj_t* parent);
void screen_trend_refresh();
void screen_trend_reset();

// ---- /ritmo: uso por hora da conta em foco ----
void screen_heat_build(lv_obj_t* parent);
void screen_heat_refresh();
void screen_heat_reset();

// ---- /ajustes e /ajustes · sobre ----
void screen_settings_build(lv_obj_t* parent);
void screen_settings_reset();
void screen_about_build(lv_obj_t* parent);

// ---- /contas e /contas · nome ----
void screen_accounts_build(lv_obj_t* parent);
void screen_account_name_build(lv_obj_t* parent);
void screen_accounts_reset();

// ---- PIN e definição de PIN (tela cheia, ST_PIN / ST_SETUP_PIN) ----
// Só desenha: as tentativas, o bloqueio e a chave da sessão ficam no .ino,
// que recebe cada tecla em onKey ("0".."9", LV_SYMBOL_LEFT, LV_SYMBOL_OK).
void screen_pin_build(const char* title, const char* sub, void (*onKey)(const char* key));
bool screen_pin_up();
void screen_pin_set_dots(int filled);
void screen_pin_set_msg(const char* msg);
void screen_pin_set_msg_bad(bool bad);   // C_BAD_INK ou C_DIM
void screen_pin_reset();

// ---- WiFi: tela cheia no onboarding (ST_WIFI) ou conteúdo WIFI na sessão ----
void screen_wifi_full();                 // ST_WIFI
void screen_wifi_build(lv_obj_t* parent);
// Conteúdo WIFI / ST_WIFI: reabre quando a net_task terminou o job em
// andamento (até lá a tela só mostra "aguardando a rede…").
void screen_wifi_tick();
void screen_wifi_full_tick();
// Resultado do WIFI_JOIN (hook do poller, no loop()); a rede já foi gravada.
void screen_wifi_on_join(bool ok);
void screen_wifi_reset();

// ---- Token: tela cheia no onboarding (ST_TOKEN) ou conteúdo TOKEN ----
void screen_token_full();                // ST_TOKEN
void screen_token_build(lv_obj_t* parent);
// Mensagem de status do token na tela (callback do web_api e do VALIDATE).
void screen_token_msg(const char* m);
void screen_token_reset();

// ---- Fornecidos pelo .ino (sessão, NVS e hardware) ----
bool session_unlocked();
void factory_reset();
void apply_brightness();
extern int g_briIdx;                       // 0..2 (BRI_LEVELS)
extern int g_heatMode;                     // 0=hoje 1=7d 2=30d 3=tudo
extern bool g_onboarding;
extern int g_tokenTargetSlot;
extern char g_pendingLabel[ACCT_LBL_MAX];
