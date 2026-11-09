#pragma once
// Idioma da interface e textos novos da v3 (item 32).
//
// g_lang é definida no .ino (0 = português, 1 = english; Ajustes -> NVS
// "lang"). TRS(pt, en) escolhe o literal no momento da chamada; tr(Txt) faz o
// mesmo para as entradas da tabela abaixo.
//
// Textos que NÃO estão aqui, porque são gerados em src/logic/ com `lang` como
// parâmetro (uma fonte só, testada no computador):
//   - projeção: "✓ não esgota · ~78%", "✗ esgota 15:20 · 38m", "✗ esgotada",
//     "— sem histórico"                      -> projection_text (projection.h)
//   - marca de estado: "ok" / "atenção" / "limite" -> status_mark (levels.h)
//   - estado dos modelos em /modelos        -> model_status_text (classify.h)
// Os nomes de classe (AUTH, RATE, ERR, SRV, NET, NOWIFI, NOCLOCK) e os comandos
// do menu (/agora, /modelos…) não são traduzidos.
#include <stdint.h>

extern uint8_t g_lang;

#ifndef TRS
#define TRS(pt, en) (g_lang ? (en) : (pt))
#endif

struct Txt {
  const char* pt;
  const char* en;
};

inline const char* tr(const Txt& t) { return g_lang ? t.en : t.pt; }

namespace T {

// ---- Card da tela inicial ----
constexpr Txt CARD_QUERYING  = {"consultando…", "querying…"};
constexpr Txt CARD_NO_KEY    = {"SEM CHAVE", "NO KEY"};
// Falha: "⚠ <classe> · <idade>" e, sem dado, "⚠ <classe>" (iguais nos dois idiomas).
constexpr Txt CARD_FAIL_FMT  = {"⚠ %s · %s", "⚠ %s · %s"};
constexpr Txt CARD_FAIL_ONLY = {"⚠ %s", "⚠ %s"};
// % da 5h do card pendente ou em falha sem dado.
constexpr Txt CARD_NO_PCT    = {"--%", "--%"};
// Linhas do card: "reset:" / "semana:" (layouts 1 e 2), "semana" (3), "sem" (4).
constexpr Txt CARD_RESET     = {"reset:", "reset:"};
constexpr Txt CARD_RESET_S   = {"reset", "reset"};     // layouts 3 e 4, sem ':'
constexpr Txt CARD_WEEK      = {"semana:", "week:"};
constexpr Txt CARD_WEEK_S    = {"semana", "week"};     // layout 3, sem ':'
constexpr Txt CARD_WEEK_XS   = {"sem", "wk"};          // layout 4
// Layout de 1 conta: títulos de seção, marca sob o pássaro e tokens do bridge.
constexpr Txt CARD_SEC_5H    = {"Janela 5h", "5h window"};
constexpr Txt CARD_SEC_WEEK  = {"Semana", "Week"};
constexpr Txt CARD_STATUS    = {"status: ", "status: "};
// %s = entrada, %s = saída (fmt_tok: "1.2k", "3.4M").
constexpr Txt CARD_TOK_FMT   = {"tokens: %s entrada · %s saída", "tokens: %s in · %s out"};
// Rótulo de um slot sem nome (%d = slot + 1).
constexpr Txt ACCT_DEFAULT_FMT = {"conta %d", "account %d"};

// ---- Shell ----
constexpr Txt WAITING_NET    = {"aguardando a rede…", "waiting for the network…"};
// Rodapé: N = slots salvos.
constexpr Txt FOOT_ACCT_ONE  = {"1 conta", "1 account"};
constexpr Txt FOOT_ACCT_FMT  = {"%d contas", "%d accounts"};

// ---- Menu (comandos com '/' não são traduzidos) ----
constexpr Txt MENU_REFRESH   = {"Atualizar agora", "Refresh now"};
constexpr Txt MENU_D_HOME    = {"uso das contas", "account usage"};       // /agora
constexpr Txt MENU_D_MODELS  = {"status por modelo", "per-model status"}; // /modelos
constexpr Txt MENU_D_TREND   = {"gráfico 5h", "5h chart"};                // /janela
constexpr Txt MENU_D_HEAT    = {"uso por hora", "usage by hour"};         // /ritmo
constexpr Txt MENU_D_ACCTS   = {"gerenciar contas", "manage accounts"};   // /contas
constexpr Txt MENU_D_SETTINGS= {"configurações", "settings"};             // /ajustes

// ---- /modelos ----
constexpr Txt MODELS_TITLE_FMT = {"Modelos · %s", "Models · %s"};          // %s = conta
constexpr Txt MODELS_NO_INC    = {"status.claude.com: sem incidentes",
                                  "status.claude.com: no incidents"};
// %s = gravidade (severity_label), depois " · <modelos>" se houver.
constexpr Txt MODELS_INC_FMT   = {"incidente %s", "incident %s"};
constexpr Txt MODELS_INC_MODELS_FMT = {"incidente %s · %s", "incident %s · %s"};

// ---- /contas ----
constexpr Txt ACCTS_HINT     = {"Tocar numa conta define o foco.", "Tap an account to focus it."};
constexpr Txt ACCTS_FOCUS    = {"•  foco", "•  focus"};

// ---- Página do token (web) ----
constexpr Txt TOK_VALIDATING = {"validando…", "validating…"};
constexpr Txt TOK_OK_SET_PIN = {"Token validado. Defina o PIN no aparelho.",
                                "Token validated. Set the PIN on the device."};
constexpr Txt TOK_OK_SAVED   = {"Conta salva.", "Account saved."};
// %s = reason
constexpr Txt TOK_REJECTED_FMT = {"A API recusou o token (%s). Confira e cole de novo.",
                                  "The API rejected the token (%s). Check it and paste again."};
constexpr Txt TOK_SCREEN_CLOSED = {"Abra a tela de token no aparelho e envie de novo.",
                                   "Open the token screen on the device and send again."};
constexpr Txt TOK_CRYPTO_FAIL = {"Falha ao cifrar no aparelho. Tente de novo.",
                                 "Encryption failed on the device. Try again."};
constexpr Txt TOK_BUSY       = {"Já existe uma validação em andamento.",
                                "A validation is already running."};
constexpr Txt TOK_NO_ANSWER  = {"O aparelho não respondeu. Tente de novo.",
                                "The device did not answer. Try again."};

}  // namespace T
