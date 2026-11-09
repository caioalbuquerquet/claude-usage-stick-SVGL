#pragma once
// Estado da aplicação, navegação entre conteúdos do shell e intenções
// (item 25). Fora do shell, só antes do desbloqueio: PIN, definição de PIN,
// WiFi e token do onboarding (telas cheias). Depois do desbloqueio a tela é o
// shell (ui/shell.h) e só o conteúdo do painel troca.
//
// As telas se registram aqui (app_register_screen / app_register_content);
// app_state não conhece o código delas. Toda troca de tela ou de conteúdo é
// adiada para app_state_tick(), no loop(): um evento de um objeto pode pedir
// a troca sem apagar o próprio objeto no meio do evento.
#include <stdint.h>
#include <lvgl.h>

enum AppState : uint8_t {
  ST_BOOT,
  ST_PIN,
  ST_SETUP_PIN,
  ST_WIFI,       // onboarding (na sessão o WiFi é o conteúdo WIFI)
  ST_TOKEN,      // onboarding (na sessão o token é o conteúdo TOKEN)
  ST_SHELL,
  ST_COUNT
};

enum class ContentId : uint8_t {
  HOME,          // /agora
  MODELS,        // /modelos
  TREND,         // /janela
  HEAT,          // /ritmo
  ACCOUNTS,      // /contas
  ACCOUNT_NAME,  // renomear (a partir de /contas)
  SETTINGS,      // /ajustes
  ABOUT,         // a partir de /ajustes
  TOKEN,         // adicionar/trocar token (a partir de /contas ou /ajustes)
  WIFI,          // a partir de /ajustes
  COUNT
};

// Tela cheia fora do shell. build monta em lv_screen_active() (já limpa);
// tick roda a cada passada do loop() enquanto a tela está aberta.
struct ScreenOps {
  void (*build)();
  void (*tick)();
};

// Conteúdo do shell. Todos opcionais.
//   build(parent): monta dentro de parent (= shell_content(), 438 x 214). O
//                  título padrão já está posto; build pode trocá-lo com
//                  shell_set_title() e dar ação a ele com shell_set_title_action();
//   refresh():     chegou resultado da net_task (app_state_tick(true));
//   tick():        uma vez por segundo, com o conteúdo aberto.
struct ContentOps {
  void (*build)(lv_obj_t* parent);
  void (*refresh)();
  void (*tick)();
};

void app_register_screen(AppState s, const ScreenOps& ops);
void app_register_content(ContentId c, const ContentOps& ops);
// Chamado antes de limpar qualquer tela ou conteúdo, para quem guarda
// ponteiros de objetos zerá-los. full = true quando a tela inteira e o
// lv_layer_top vão ser limpos (troca de AppState), false numa troca de
// conteúdo (os overlays continuam).
void app_on_ui_reset(void (*cb)(bool full));

// Pede uma troca de estado (aplicada no próximo app_state_tick). Pedir
// ST_SHELL fora do shell cria o shell e abre o conteúdo pendente (HOME, se
// nenhum foi pedido); dentro do shell, não faz nada.
void request_state(AppState s);
AppState app_state();
// Há troca de estado ou de conteúdo pendente.
bool app_state_pending();

// Troca só o conteúdo do painel (adiado). Pedir o conteúdo aberto refaz o
// conteúdo. Ao abrir TOKEN chama web_token_begin(); ao sair, web_token_reset().
// Ao abrir WIFI pausa o agendador; ao sair, retoma.
void shell_show(ContentId c);
ContentId app_content();
// Comando do menu de cada conteúdo ("/agora"…); as subtelas devolvem o
// comando de onde se chega a elas (ABOUT -> "/ajustes").
const char* content_command(ContentId c);

// ---- Intenções ----
void intent_refresh();          // scheduler_request_refresh()
void intent_focus(int slot);    // conta em foco + refaz o conteúdo aberto
void intent_focus_next();       // próxima conta salva, em ordem de slot
void intent_open(ContentId c);  // fecha o menu e chama shell_show

// Contas salvas e contas no estado "falhou" (Technical Plan §4; SEM CHAVE conta).
int app_account_count();
int app_failed_count();
bool app_slot_failed(int slot);

// No loop(), a cada passada, com o valor de poller_apply_results(). Aplica a
// troca pendente, chama refresh() do conteúdo quando dataChanged e, uma vez
// por segundo, reavalia o ponto (dot_state), o rodapé e o tick() do conteúdo.
void app_state_tick(bool dataChanged);
