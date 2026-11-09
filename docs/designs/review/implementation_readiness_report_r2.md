# Goldfish Review Report — Estágio 9 (Teste de Prontidão para Implementação), rodada 2

**Alvo:** `/Users/caiotorres/Development/claude-usage-stick/docs/designs/thep-usage-stick-v3.md` (DetailedRange: linhas 618–1497, "## Detailed Implementation Plan"). As linhas 1–617 (spec) foram lidas só como referência.
**Modo:** REVIEW (re-auditoria depois das correções da rodada 1)
**Padrão:** `/Users/caiotorres/.claude/plugins/cache/egm/egm/0.1.1/resources/document_standards/implementation-readiness-standard.md`, usado por implementation-readiness e codebase-details. A lente style não usa padrão.
**Base da rodada 1:** `review/implementation_readiness_report.md` e `review/goldfish_review_report.md` (seção "Etapa 9 — rodada 1" e as decisões do dono sob CONTEST).

## Status dos revisores

| Revisor | Status | Achados brutos |
| --- | --- | --- |
| `egm-implementation-readiness` | completed | 2 🔴 · 10 🟡 · 13 🟢 |
| `egm-codebase-details` | completed | 0 🔴 · 6 🟡 · 12 🟢 |
| `egm-style` | completed | 0 🔴 · 4 🟡 · 17 🟢, mais 1 observação fora da lente |

Nenhum revisor falhou, e nenhum precisou de nova tentativa.

**Veredito:** **NÃO PRONTO.** Depois de juntar os achados repetidos, sobram **2 🔴 bloqueadores, 17 🟡 importantes e 34 🟢 menores**. Quase todos são **NOVOS** e vêm do texto acrescentado pelas correções da rodada 1. Há dois **REABERTOS**:
- o M10 abaixo (colchetes da barra, parte do CONTEST de M24): a evidência do CONTEST está errada, e o sintetizador conferiu isso no próprio mockup;
- o `st-2-5` (células contra blocos), resto do `st-1-30`.

O sintetizador também conferiu a observação do style que virou o M17.

Os ids seguem o formato `<revisor>-2-<n>`, com `ir` = implementation-readiness, `cd` = codebase-details e `st` = style. Quando um achado junta mais de um revisor, todos os ids aparecem na linha "Ids".

**Limitação:** o `lv_font_conv` 1.5.3 não está instalado. Por isso, as flags `--lv-fallback` e `--lv-include` não foram conferidas de novo. Elas seguem como resolvidas na rodada 1.

---

## Achados da rodada 1 conferidos como corrigidos

- **Bloqueadores:**
  - B1 (watchdog por requisição, `NET_REQ_STUCK_MS 90000`);
  - B3 (`setConnectTimeout`, conferido em `HTTPClient.h:216`/`HTTPClient.cpp:1109`; `setHandshakeTimeout` em segundos em `NetworkClientSecure.cpp:450-451`);
  - B4 (`status.cpp` Modify com o helper `net_client_setup`);
  - B5 (RGB565A8, `lv_color.h:144`, `lv_image_dsc.h:88-97`);
  - B6 (símbolos e intervalos do servidor web, com a ressalva do M15);
  - B7 (helpers e migração do histórico, todos nas linhas citadas);
  - B8 (`model_status_text` com tabela pt/en).
  - B2 também foi corrigido, mas o texto novo abriu os achados B1, B2, M2 e M3 desta rodada.
- **Importantes:**
  - corrigidos sem ressalva: M1–M9 e M11–M38;
  - M10 corrigido, mas abriu o M5;
  - M24 corrigido em parte, com a parte "colchetes" reaberta no M10.
- **Menores:**
  - os 22 da rodada 1 foram aplicados;
  - `st-1-30` corrigido só em parte (ver `st-2-5`);
  - as citações de `cd-1-33`, `cd-1-35`, `cd-1-36`, `cd-1-37` e `cd-1-38` conferem.
- **CONTEST de M26 (remover conta apaga `/histN.bin`):** mantido. A linha citada como evidência está um pouco errada, mas a conclusão não muda (ver `cd-2-8`).

---

## 🔴 Bloqueadores

### B1. `used`, `label` e `keyOk` do `AccountState` nunca são preenchidos — NOVO
- **Ids:** `ir-2-1`
- **Onde:** item 3, item 6 passo 8, item 19 (pseudo-código, l. 986), item 23 (desbloqueio e `ST_SETUP_PIN`), item 39 (`screen_accounts`, renomear).
- **Problema:**
  - O registro nasce zerado (`heap_caps_calloc`).
  - O desbloqueio só grava `keyOk`; ninguém grava `used` nem `label`. O agendador pula as contas com `if not used or not keyOk: continue`, então, lido ao pé da letra, nenhuma conta é consultada.
  - Adicionar conta (passo 8) e o `ST_SETUP_PIN` não gravam `keyOk = true`.
  - Renomear não atualiza `acct[slot].label`.
  - `AccountSlots` e `AccountState` viram duas fontes da verdade, sem regra de sincronia.
- **Correção:**
  - Escolher uma de duas saídas:
    1. tirar `used`/`label` do `AccountState` e ler `g_accts.used[slot]`/`g_accts.label[slot]`;
    2. criar `acct_sync_from_slots(slot)`, chamada depois de `accountsLoad`, `accountSave`, `accountSetLabel` e `accountRemove`.
  - Nos dois casos, acrescentar `acct[slot].keyOk = true` ao passo 8 do item 6 e ao passo 4 do `ST_SETUP_PIN`.

### B2. O filtro de `gen` do item 18 descarta o `VALIDATE` e lê `acct[-1]` — NOVO
- **Ids:** `ir-2-2`
- **Onde:** item 18, primeira linha da tabela ("qualquer | `FC_DISCARDED`, ou `gen` diferente do atual"), contra o item 6, passo 2.
- **Problema:**
  - O `VALIDATE` sai com `gen = 0`. Como o filtro vale para qualquer tipo, trocar o token de um slot com `gen ≥ 1` descarta o resultado, e a página fica em `pending` até estourar os 30 s.
  - `STATUS` e `RECONNECT` não têm slot, então a comparação com `acct[r.slot].gen` lê fora do vetor.
- **Correção:**
  - Restringir a linha a "`USAGE`: `FC_DISCARDED` ou `r.gen != acct[r.slot].gen`".
  - Declarar `slot = -1` e `gen = 0` para `STATUS` e `RECONNECT`, e dizer que `VALIDATE`, `STATUS` e `RECONNECT` não passam pelo filtro.
  - Dar uma linha própria ao `VALIDATE` com `FC_DISCARDED`: vira `rejected`/`reason:"discarded"`, ou é ignorado. Escolher um.

---

## 🟡 Importantes

### M1. Agendador e poller antes do desbloqueio — NOVO
- **Ids:** `ir-2-3`
- **Onde:** item 2 (ordem do `loop()`, l. 641–647), item 19, item 24 (l. 1093).
- **Problema:**
  - `scheduler_tick()` roda sem condição, e `nextCycleMs` nasce 0. No onboarding e no `ST_PIN`, o agendador manda `STATUS` ou `RECONNECT`, e o `RECONNECT` disputa o rádio com o `connectTo()` do onboarding.
  - Já `poller_apply_results()` só roda "depois do desbloqueio". Só que o `VALIDATE` do onboarding precisa ser aplicado antes disso.
- **Correção:**
  - Criar a flag `started` em `scheduler.cpp`: `scheduler_start()` a liga, e `scheduler_tick()` sai na hora se ela estiver desligada.
  - `poller_apply_results()` e `poller_check_stuck()` rodam em todos os estados.

### M2. Corrida no onboarding e cancelamento do token — NOVO
- **Ids:** `ir-2-4`, `cd-2-11`, `st-2-11`
- **Onde:** item 6, passos 5–7; item 24 (l. 1092). Código atual: `render_state` → `stop_web()` em ino:2622, e `ui_token()` → `stop_web()` em ino:838.
- **Problema:**
  - Quando chega `ok`, o `loop()` vai para `ST_SETUP_PIN`, e `render_state()` derruba o servidor. A página perde o `ok` e, depois de 30 s, mostra "O aparelho não respondeu".
  - O JS não diz o que fazer quando lê `idle` depois do envio.
  - Não está definido o que acontece se o usuário fecha o conteúdo de token com um `VALIDATE` em andamento.
  - `web_token_reset()` aparece sem assinatura e sem dono.
  - Para reaproveitar o servidor da sessão, o `stop_web()` dentro de `ui_token()` (:838) também precisa sair.
- **Correção:**
  - Manter o servidor do onboarding vivo de `ST_TOKEN` até `ST_SETUP_PIN`. A alternativa é atrasar a troca de tela até a página ler `ok`, com limite de 3 s.
  - JS: ler `idle` depois do envio conta como "O aparelho não respondeu".
  - Declarar `web_token_reset()` em `web_api.h`. Ela é chamada quando a tela de token fecha, executa `token_store_clear_pending()` e marca o `VALIDATE` em voo para ser ignorado.
  - Remover o `stop_web()` de ino:838 junto com o de `render_state`.

### M3. O passo 8 do item 6 e as rotas da sessão estão incompletos — NOVO
- **Ids:** `ir-2-5`, `st-2-4`, `cd-2-5`, `cd-2-14`
- **Onde:** item 6, passos 2 e 8, e a linha 708 ("`slot = g_tokenTargetSlot` (só para o log)"); item 6, fase 2 (l. 701).
- **Problema:**
  - A l. 708 diz que o slot serve "só para o log", mas o passo 8 usa `slot` em `accountSave`, `acct_bump_gen` e `token_store_set`.
  - Não há regra para distinguir adicionar de trocar. Na v2.3, isso é `g_accts.used[slot]` lido antes do `accountSave` (ino:1109).
  - Não está dito se a conta nova ganha o foco, como na v2.3 (`accountSetActive`, ino:1115-1116), nem se o uso validado entra no histórico (`hist_push`/`save_history`, ino:1128).
  - Faltam:
    - a navegação depois de `ok`;
    - zerar `g_pendingLabel`;
    - o caminho de falha do `encryptTokenWithKey` (ino:1104-1107);
    - a resposta do POST quando a tela está fechada.
  - O servidor único da sessão não escolhe o handler de `/` nem o de 404. Hoje o servidor de dados usa `/` → `handleInfo` e 404 JSON (ino:830, 833), e o do onboarding usa `/` → `handleRoot` e redirect 302 (ino:878, 880).
- **Correção:**
  - Tirar "(só para o log)" e fixar `slot = g_tokenTargetSlot` (o primeiro livre ao adicionar, o da conta ao trocar).
  - `replacing = g_accts.used[slot]`, lido antes do `accountSave`.
  - Dizer se o foco muda e se o resultado do `VALIDATE` entra no histórico.
  - Depois de `ok`: `intent_open(HOME)` na fase 3, ou `ST_MAIN` na fase 2.
  - Falha de cifra: `rejected`/`reason:"crypto"`.
  - POST com a tela fechada: 403 `{"state":"rejected","reason":"closed"}`.
  - Rotas: `/` → `handleRoot` (formulário), `/info` → `handleInfo`, `onNotFound` → 404 JSON.

### M4. `acct_clear` zera o `gen` e reabre a corrida do slot reaproveitado — NOVO
- **Ids:** `ir-2-6`
- **Onde:** item 3 (`acct_clear` com `memset`) e item 23 (sequência de remover).
- **Problema:**
  - A sequência de remover é `acct_bump_gen` → `token_store_clear` → `acct_clear`, e o `memset` de `acct_clear` volta `gen` a 0.
  - Ao adicionar de novo no mesmo slot, o `gen` vira 1. Uma resposta atrasada da conta removida, que também tinha `gen = 1`, passa no filtro.
  - Isso viola a §1.
- **Correção:**
  - `acct_clear` preserva o `gen`: guarda, faz o `memset` e restaura.
  - Criar um teste ou passo de E2E: remover e adicionar no mesmo slot durante um ciclo.

### M5. A fase 2 remove `ST_LOADING` sem destino para quem chama esse estado — NOVO (ligado a ir-1-12)
- **Ids:** `ir-2-7`
- **Onde:** itens 23 e 24.
- **Problema:**
  - Estas chamadas a `request_state(ST_LOADING)` ficam sem substituto:

    | Linha do `.ino` | Onde |
    |---|---|
    | :594 | `wifi_kb_cb` |
    | :907 | `ui_error` |
    | :1098 | `switch_account`, chamado por `acct_switch_cb` (:2380) |
    | :2220 | Ajustes → atualizar |
    | :2406 | `acct_del_cb` |
    | :466 e :477 | `pin_submit` |

  - `apply_tz()` (:2268) também vai para o `scheduler.cpp` sem estar na API.
- **Correção:**
  - Tabela no item 24 com o destino de cada chamada:

    | Linha | Destino |
    |---|---|
    | :594 | `scheduler_resume()` + `ST_MAIN` |
    | :2220 | `scheduler_request_refresh()` |
    | :2380 | `accountSetActive` + `ST_MAIN` |
    | :2406 | sequência de remover + `ST_ACCOUNTS` |
    | :907 | só antes do desbloqueio |
    | :466/:477 | `ST_MAIN` |

  - Acrescentar `void scheduler_apply_tz()` ao item 19.

### M6. `poller`/`scheduler` (`src/net/`) chamam UI e app sem mecanismo; `ThrHit` só existe na fase 3 — NOVO
- **Ids:** `cd-2-1`, `ir-2-8`
- **Onde:** item 14 (fase 1), itens 18 e 19 (fase 2, l. 935–942, 1006, 1016), item 40 (fase 3, l. 1385).
- **Problema:**
  - `poller_apply_results()` chama:
    - `incident_sync()`, que é `static` em ino:2030 e mexe em LVGL;
    - `check_thresholds`, que é `static` em ino:1705 e vai para `src/ui/overlay_moment.cpp` na fase 3;
    - o fluxo dos passos 4–8 do item 6, que usa `g_sessionKey` e `g_prefs` e recria a tela.
  - `scheduler.cpp` chama `moment_flush()` → `show_moment()`, que também é LVGL. `moment_flush()` não tem header.
  - `ThrHit` e `check_thresholds(AccountState&)` só são definidos no item 40, mas o item 18 já os usa na fase 2, e o item 14 apaga `g_thr*` na fase 1.
  - Não está descrito como o novo mecanismo convive com o `g_pendWin`/`show_moment` atual (ino:2898).
  - Do jeito que está, a regra "`src/net` nunca chama `lv_*`" (l. 624) só vale para chamadas diretas.
- **Correção:**
  - Hooks registrados pelo `.ino` (depois pelo `app_state`): `poller_on_status`, `poller_on_validate(const NetResult&)` e `scheduler_on_moment(int slot, int win, int thr)`. A alternativa é mover `poller_apply_results` para `src/app/`.
  - `struct ThrHit` e `check_thresholds(AccountState&)` vão para `src/model/account_state` já na fase 1.
  - `void moment_flush()` vai para `scheduler.h`, e `moment_close()` chama `moment_flush()`.
  - Na fase 2, `moment_flush` chama o `show_moment` atual, e o bloco `g_pendWin` sai do `loop()`.

### M7. `src/logic` depende de constantes do `config.h` — NOVO
- **Ids:** `ir-2-9`
- **Onde:** item 10 (`DOT_FRESH_SEC`), item 11 (`probe_needs_retry` contra `PROBE_MODEL`), item 13 (`-I firmware/claude_stick/src`), regra da l. 624.
- **Problema:** o `config.h` fica fora de `src/` e fora da regra de includes de `src/logic`, então o `run.sh` não compila.
- **Correção:**
  - Passar os valores como parâmetro: `probe_needs_retry(bool, const char* model, const char* probeModel)` e `dot_state(..., uint32_t freshSec)`.
  - A alternativa é criar `src/logic/logic_config.h`, incluído pelo `config.h`.

### M8. Card em falha sem dado anterior; formato da idade — NOVO
- **Ids:** `ir-2-10`
- **Onde:** itens 24 e 36; E2E 6.
- **Problema:**
  - Depois de trocar o token, `usageAtEpoch = 0`. Se o primeiro resultado for `AUTH`, as regras "falha → `⚠ <classe> · <idade>`" e "sem dados → `consultando…`" colidem, e a idade fica indefinida. É exatamente o caso do E2E 6.
  - O formato da idade a partir de 60 min não está definido.
- **Correção:**
  - Ordem de precedência:
    1. `!keyOk` → `SEM CHAVE`;
    2. `failed && usageAtEpoch == 0` → `⚠ <classe>` + `--%`;
    3. `failed` → `⚠ <classe> · <idade>`;
    4. `pendente` → `consultando…`.
  - Criar `age_text()` em `levels`, com `%ds`, `%dm` e `%dh` e testes.

### M9. A execução do job só cobre `USAGE`/`VALIDATE`, e `NetResult` não tem o campo `ms` — NOVO
- **Ids:** `ir-2-11`, `st-2-2`, `cd-2-9`, `cd-2-10`
- **Onde:** item 18, "Execução de um job", `NetResult` (l. 919–922) e a linha `modelIdx >= 0` da tabela (l. 938).
- **Problema:**
  - `fetchModelStatus` devolve só `bool` (`status.cpp:17`). Sem o código HTTP, a regra de classe do `STATUS` fica em aberto.
  - O passo 1 (copiar o token) não se aplica a `STATUS` nem a `RECONNECT`, e o `RECONNECT` não aparece nos passos.
  - A l. 938 usa `{probeHttp ou http, probeMs ou ms}`, mas `ms` não existe, e o "ou" não tem critério.
- **Correção:**
  - Escrever os passos por `JobType`:
    - `STATUS`: pula o passo 1; `cls = ok ? FC_OK : FC_ERR` (ou `FC_NET`, escolher um); `http = 0`;
    - `RECONNECT`: `connectList(...)`, com `winnerIdx` num campo próprio (ver `st-2-15`).
  - A task sempre preenche `probeHttp`/`probeMs` quando `modelIdx >= 0`, e a tabela usa só esses dois campos.

### M10. Colchetes da barra — REABERTO (ir-1-20 / M24, parte do CONTEST)
- **Ids:** `ir-2-12` (o sintetizador conferiu)
- **Onde:** item 36, l. 1313 ("sem colchetes"), e item 37.
- **Problema:** a evidência do CONTEST ("o mockup não tem colchetes, porque `bar()` só gera `█`/`░`") está errada. A função `bar()` só gera os blocos, mas o template envolve cada barra com `<span style="color: #8a98a8">[</span>…<span style="color: #8a98a8">]</span>`. Isso aparece em `mockup-home.dc.html:45, 50, 68, 72, 91 e 109`. O plano diverge do mockup que ele próprio declara normativo (item 34). A outra parte do CONTEST (gap de 16 px e `C_TEXT` no rodapé) não foi contestada e continua valendo.
- **Correção:** escolher uma das duas saídas.
  1. Restaurar `[`/`]` em `C_DIM` (#8a98a8) e somar 2 caracteres à largura das barras na checagem do `gen_mockups.py`.
  2. Registrar "sem colchetes" como decisão explícita do dono, sem citar o mockup como evidência.

### M11. `maxChars` do layout de 1 conta diverge da própria regra — NOVO
- **Ids:** `st-2-1`, `ir-2-13`, `cd-2-16`
- **Onde:** item 37, l. 1334 contra a regra da l. 1337 e o card de 272 px (item 36, l. 1303).
- **Problema:** 272 ÷ 6,6 dá 41, mas a tabela diz 32. Os layouts 2, 3 e 4 batem com a regra.
- **Correção:** trocar 32 por 41, ou explicar por que o layout de 1 conta usa uma largura menor.

### M12. O código HTTP 0 tem dois significados — NOVO
- **Ids:** `st-2-3`
- **Onde:** item 20 (l. 1040), item 11 (l. 803), item 38 (l. 1348).
- **Problema:** um erro de transporte é "`*httpOut ≤ 0`", e `classify_usage` trata `≤ 0` como `FC_NET`. Mas a tabela de `model_status_text` usa 0 para "nunca sondado" (`—`). Uma falha que devolva 0 aparece como `—` em `/modelos` e como `NET` no card.
- **Correção:** fixar "erro de transporte → `*httpOut < 0`" nos itens 20 e 11, ou criar uma sentinela própria para "nunca sondado".

### M13. O item 46 apaga um SVG que `tools/gen_banners.py` ainda usa — NOVO
- **Ids:** `cd-2-2`
- **Onde:** item 46 (l. 1440) e a lista de arquivos.
- **Problema:**
  - `gen_banners.py:53` passa `assets/brand/claudecode-color.svg` para o `rsvg-convert`, e a l. 32 usa a paleta coral.
  - Os `assets/banner-{steps,web}{,-pt}.png` aparecem no README, e nenhum item os cita.
  - Depois da fase 4, o script quebra, e o `py_compile` não pega isso.
- **Correção:** incluir no item 46 `tools/gen_banners.py` (Modify, logo e paleta THEP) e os 4 PNGs (regenerados). A alternativa é manter o SVG e registrar que os banners não mudam.

### M14. Várias funções do `.ino` ficam sem destino, contra a promessa do item 2 — NOVO
- **Ids:** `cd-2-3`
- **Onde:** item 2 (l. 640), item 39 (l. 1367, 1378), item 36.
- **Problema:**
  - Nenhum item move nem remove estas funções:

    | Grupo | Funções (linha no `.ino`) |
    |---|---|
    | Montagem de telas | `tlabel` (1152), `tstatic` (1160), `tile_setup` (1165), `mkchip` (1183), `set_chip` (1197), `set_meter` (277), `build_win_card` (1329) |
    | Status da API | `status_color`/`overall_label` (1137-1149) |
    | Valores na tela | `set_ag_proj` (1535), `update_tok_row` (1500), `refresh_ui_values` (2043) |
    | Ajustes e brilho | `load_persisted` (325), `save_attempts` (346), `apply_brightness` (347) |
    | Barra de atualização | `g_ui.refBar` no `loop()` (2851-2860) |

  - Há reuso perdido. `overall_label`/`status_color` já tratam `rate_limited` e `exceeded` (ino:1139). A "marca de estado" do item 36 só define `allowed`, `allowed_warning` e `rejected`.
- **Correção:**
  - Acrescentar uma linha de destino para cada função no item 39: `widgets.cpp`, `app_state.cpp` ou "sai".
  - A marca de estado reescreve `overall_label`/`status_color` e define o que fazer com valores desconhecidos.

### M15. A extração do `web_api` na fase 0 está incompleta — NOVO (a partir da correção de B6/M9)
- **Ids:** `cd-2-4`, `ir-2-18`
- **Onde:** item 6 (l. 694–699).
- **Problema:**
  - O intervalo 646-835 inclui `anim_opa_cb` (:665) e `build_claude_mark` (:668-680). As duas usam LVGL e são chamadas por `ui_token`, `ui_loading` e `ui_about`.
  - A lista de `extern` deixa de fora `g_pinConfirming`, `g_pinFirst` e `g_pinEntry` (ino:111-113), que `handleTokenPost` escreve (ino:757). A fase 0 não compila.
- **Correção:**
  - 665-680 fica no `.ino`.
  - Acrescentar os três `extern`. A alternativa é um callback `web_on_token_ok()` que zera o estado do PIN no `.ino`.

### M16. `POST /tokens` sem `account` na v3 não está definido — NOVO
- **Ids:** `cd-2-6`
- **Onde:** item 6, fase 4 (l. 739), e item 45.
- **Problema:** a v2.3 aplica o push à conta ativa quando não há `account` e responde 409 `account_mismatch` quando o rótulo não confere (ino:798-805). O plano não diz:
  - o que fazer quando o campo falta;
  - se o 409 continua existindo;
  - se a comparação do rótulo é `strcmp` ou sem diferenciar maiúsculas, como em `makeUnique`, `accounts.cpp:35`.
- **Correção:** "sem `account` → conta em foco; o 409 deixa de existir; comparação com `strcmp`".

### M17. A fase 1 remove `g_usage`/`g_tok`, mas o `web_api` continua lendo essas globais — NOVO
- **Ids:** `st-2-22` (observação fora da lente de style; o sintetizador conferiu)
- **Onde:** item 14 (l. 864) contra o item 6 (l. 699).
- **Problema:** a l. 864 apaga `g_usage`, `g_tok` e os `extern` temporários na fase 1. Os handlers do item 6 continuam lendo `g_usage` e `g_tok` por `extern` (l. 699), e nenhum item da fase 1 muda o `web_api.cpp`. A fase 1 não compila.
- **Correção:** acrescentar `src/app/web_api.cpp` (Modify) ao item 14. Os handlers passam a ler `acct[]`, com `/window` e `/info` usando a conta em foco até a fase 4.

---

## 🟢 Menores

| Id(s) | Onde | Problema | Correção |
| --- | --- | --- | --- |
| `st-2-5` — **REABERTO** (st-1-30) | l. 855 contra l. 1313, 1330, 1332 | Fala em "9 células" num lugar e "blocos" em outro. | "50% em 9 blocos (5 acesos)". O parâmetro `cells` pode ficar com esse nome. |
| `ir-2-14` | itens 36–37 | A fonte da barra de blocos não está definida. O mockup usa 12 px nos layouts 1–2 e 11 px nos 3–4. | Fixar a fonte por layout. |
| `ir-2-15` | §3 / item 3 | Faltam `lastH5` e `dayN`, usados por `accumulate_heat` e `heat_mode_data`. | Acrescentar à tabela de campos. |
| `ir-2-16` | vários | Faltam dono e header de `session_unlocked()`, `g_sessionKey`, `web_token_set_result(FetchClass)`, `incident_sync()`, `refresh_ui_values()` e `g_models[]` (fase 3), e falta uma `fetch_class_name(FetchClass)`. | Declarar cada um no header do item dono. |
| `ir-2-17` | item 1 | A v2.3 não imprime o heap no serial, então a linha de base "lida pelo serial" não sai sem mudar código. | Dizer como medir (por exemplo, um build temporário com `heap_caps_get_free_size` no `dash_tick`). |
| `ir-2-19` | item 23, desbloqueio | Faltam: `g_pinAttempts = 0`/`save_attempts()`/`g_lockoutUntil = 0` (ino:472); `g_onboarding = false` e `memset(g_pinFirst)` no `ST_SETUP_PIN`; checar `blob.len > 199` antes de decifrar; decidir se um token longo conta como PIN errado. | Listar esses passos. |
| `ir-2-20` | item 23, remover | Falta recriar a home com o novo layout (o E2E 7 exige) e manter a guarda `accountCount <= 1` (ino:2384). | Acrescentar os dois. |
| `ir-2-21` | item 26 | A célula tem largura fracionária (104/33 px; a margem de 0,5 px no ícone de 13 px). | Fixar o arredondamento e as margens. |
| `ir-2-22` | item 38 | O texto do "incidente atual" em `/modelos` não está definido em pt nem em en. | Dar o formato e incluir na tabela do item 32. |
| `ir-2-23` | item 35 | Não está dito que o `↻` é `LV_SYMBOL_REFRESH`, que vem do fallback da Montserrat, e não U+21BB. | Escrever isso no item. |
| `ir-2-24` | item 39, `screen_trend` | As legendas de `/janela` (ino:1589-1658) não estão na tabela do item 32. Também não está dito se a linha de projeção aparece em `OK_ATE_RESET` com `rate ≤ 0.02`. | Acrescentar as legendas e a regra. |
| `ir-2-25` | itens 18–19 | Falta chamar `ensure_mdns()` depois de um `RECONNECT` bem-sucedido. | Acrescentar a chamada. |
| `cd-2-7` | item 20, l. 1042 | A recusa de `blob.len > 199` está em `crypto.cpp:60`, não em `:58`. | Corrigir a linha. |
| `cd-2-8` | item 5, l. 688 | O arquivo é apagado em ino:2393-2394, não em 2391-2392. O mesmo erro está na evidência do CONTEST de M26, mas a conclusão dele não muda. | Corrigir a linha. |
| `cd-2-12` | itens 5 e 23 | Não está dito o que acontece com o `load_history()` do `setup()` (ino:2783). | "sai; os históricos são carregados no passo 4 do desbloqueio". |
| `cd-2-13` | item 5, fase 0 | `Sample`/`HIST_MAX` (ino:140-141) e `DayHeat`/`NDAYS` (ino:149-150) ficam fora do intervalo 930-1078. | Listar esses tipos no `history.h`. |
| `cd-2-15` | item 20, l. 1043 | Zerar o `char[220]` não apaga o token: `addHeader(const String&, const String&)` (`HTTPClient.h:238`) copia o valor para um `String` no heap, que é liberado sem ser zerado. | Reescrever a promessa de segurança ou registrar essa limitação. |
| `cd-2-17` | itens 26, 29, 47 | Faltam as dependências de ferramenta: Pillow, `fontTools`, Node/`npx` e o caminho do `xtensa-esp32s3-elf-gcc` com os `-I` do LVGL. | Acrescentar ao item 47. |
| `cd-2-18` | itens 39 e 40 | Intervalos imprecisos (ver a tabela abaixo). | Ajustar os intervalos. |
| `st-2-6` | l. 786 contra l. 707, 710, 715, 893, 917 | "pendente" também é usado para o token em validação. | "token em validação", ou restringir a definição. |
| `st-2-7` | l. 1116 | Não fica claro a que se refere "(só antes do desbloqueio)". | "Fora do shell, só antes do desbloqueio: …; depois, WiFi e token são conteúdos do shell". |
| `st-2-8` | l. 1218, 1315 | Usa "Card com falha" e "Card sem dados". | Trocar por "Card em falha" e "Card pendente". |
| `st-2-9` | l. 1259 | Diz "estado `falhou` (item 10)", mas o item 10 usa `failed`. | "(estado `falhou` da §4; `failed` no item 10)". |
| `st-2-10` | l. 704, 711 | Fala em "conteúdo de token" na fase 2, mas "conteúdo" só existe a partir da fase 3. | "tela de token (`ST_TOKEN`; na fase 3, o conteúdo `TOKEN`)". |
| `st-2-12` | l. 622 | Diz que os READMEs são corrigidos "no item 47", mas a raiz é corrigida no item 46. | "itens 46 e 47". |
| `st-2-13` | l. 858 contra l. 846 | "`NOCLOCK` não entra nos testes de host", mas `FC_NOCLOCK` é testado em `dot_state`. | "A detecção de `NOCLOCK` (prazo de 60 s)…". |
| `st-2-14` | l. 929 | O log fixo `[slot N] USAGE …` vale para todos os tipos de job. | `[slot N] <tipo> http=%d cls=%s ms=%u`. |
| `st-2-15` | l. 922 | No `RECONNECT`, o campo `http` leva o índice da rede. | Criar o campo `int8_t netIdx`. |
| `st-2-16` | l. 1073 | "(na prática todos usam o MAC)" não tem evidência nem decisão. | Citar `crypto.cpp:34` e dizer quando o ramo `decryptToken(blob, pin)` roda; se nunca roda, remover. |
| `st-2-17` | l. 1131 | "As regras bastam para refazer o resultado" não tem como ser verificado, porque o script original não existe. | "As regras definem o resultado; o `gen_mockups.py` é a conferência visual". |
| `st-2-18` | l. 950 | "DNS ~21 s" não diz que é estimativa. | "(estimado a partir das constantes do lwIP; o E2E 2 mede)". |
| `st-2-19` | l. 1277 | "7 × 36 + 8 = 260 px" ignora a borda de 1 px dos dois lados. | "7 × 36 + 8 + 2 = 262 px". |
| `st-2-20` | l. 1264 (e spec l. 571) | "gira e envia" é ambíguo. | "rotaciona e envia". |
| `st-2-21` | item 37, l. 1325–1337 | A tabela e o parágrafo estão sem recuo. | Recuar 4 espaços, como nos outros itens. |

Intervalos de `cd-2-18`:

| Item | Intervalo citado | Problema | Correção |
| --- | --- | --- | --- |
| 40, momento | 1684-1893 | Inclui as globais do incidente (1887-1890) | Separar 1887-1890 para o incidente |
| 39, `screen_pin` | 375-555 | Inclui as globais de WiFi (554-555) | Terminar em 553 |
| 39, `screen_settings` | 2217-2375 | Perde `g_wipeArmed`, `POLL_OPTS` e `TZ_OPTS` (2208-2213) | Começar em 2208 |
| 39, `screen_accounts` | — | Precisa de `g_acctDelArmed` (2215) | Incluir 2215 |

---

## Resumo de contagem (depois de juntar os repetidos)

| Severidade | Total | NOVO | REABERTO |
| --- | --- | --- | --- |
| 🔴 Bloqueador | 2 | 2 | 0 |
| 🟡 Importante | 17 | 16 | 1 (M10) |
| 🟢 Menor | 34 | 33 | 1 (`st-2-5`) |
