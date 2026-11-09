# Goldfish Review Report — Estágio 9 (Teste de Prontidão para Implementação), rodada 1

**Alvo:** `/Users/caiotorres/Development/claude-usage-stick/docs/designs/thep-usage-stick-v3.md` (DetailedRange: linhas 612–1114, "## Detailed Implementation Plan")
**Modo:** REVIEW
**Padrão:** `/Users/caiotorres/.claude/plugins/cache/egm/egm/0.1.1/resources/document_standards/implementation-readiness-standard.md` (implementation-readiness e codebase-details); style usou os padrões da própria persona.

## Status dos revisores

| Revisor | Status | Achados brutos |
| --- | --- | --- |
| `egm-implementation-readiness` | completed | 2 🔴 · 21 🟡 · 9 🟢 |
| `egm-codebase-details` | completed | 7 🔴 · 24 🟡 · 7 🟢 |
| `egm-style` | completed | 0 🔴 · 17 🟡 · 15 🟢 |

Nenhum revisor falhou. Nenhum precisou de nova tentativa.

**Veredito:** **NÃO PRONTO.** Depois de juntar os achados repetidos, sobraram **8 🔴 bloqueadores, 38 🟡 importantes e 22 🟢 menores**.

Os ids seguem o formato `<revisor>-1-<n>`. As abreviações são `ir` = implementation-readiness, `cd` = codebase-details e `st` = style. Quando um achado junta mais de um revisor, todos os ids aparecem na linha "Ids".

**Observação de ambiente:** o revisor de codebase encontrou instalado só o core **esp32 3.3.8**. O plano (item 17) e o `build.sh:12` citam 3.3.11. Todas as conferências de API do core foram feitas contra a 3.3.8. As de LVGL foram feitas contra a cópia 9.2.2 em `~/Documents/Arduino/libraries/lvgl`.

---

## 🔴 Bloqueadores

### B1. O watchdog de job travado reinicia o aparelho num caso normal
- **Ids:** `ir-1-1`, `cd-1-3`
- **Onde:** item 15 (linhas 803–804 e 817), item 18 (linha 850). Na spec: §2 (l. 210) e §4 (l. 298).
- **Problema:** o job da conta em foco pode fazer duas requisições: o modelo da rotação e depois o reenvio com `PROBE_MODEL`. Cada uma pode levar 5 + 5 + 8 = 18 s, então o job todo chega a 36 s. Esse é exatamente o valor de `NET_JOB_STUCK_MS`. O DNS e o `https.begin()` ficam fora desses tempos. Além disso, o `setTimeout` do HTTPClient limita cada espera por dados, não a requisição inteira (`HTTPClient.cpp:1287`). A conta de "pior caso = 5 × 18 = 90 s" também esquece o reenvio.
- **Por que importa:** numa rede lenta, um aparelho saudável chama `esp_restart()` e pede o PIN de novo, talvez em loop.
- **Correção:** medir o watchdog **por requisição**, assim: "`poller_job_started_ms()` é zerado antes de cada requisição HTTPS, inclusive o reenvio". A alternativa é subir para `NET_JOB_STUCK_MS 60000` e acertar §4, Riscos e item 18. Em qualquer caso, corrigir o pior caso para "4 USAGE + 1 reenvio + 1 STATUS = 6 × 18 s = 108 s".

### B2. Fluxo do token, onboarding e cifra com a chave da sessão não estão desenhados de ponta a ponta
- **Ids:** `ir-1-2`, `cd-1-19`
- **Onde:** item 6 (l. 676–677), item 15 (l. 794), item 19 (l. 857–863), item 36 (l. 1021).
- **Problema:**
  1. Nenhum passo diz quando `poller_start()` é chamado. O `VALIDATE` precisa da `net_task`, e o onboarding acontece antes do desbloqueio.
  2. O formulário web hoje é um POST que espera uma página de resultado (`web_result`, `claude_stick.ino:718`). O plano diz que "a página consulta `GET /token/status`", mas não define:
     - o JS da página;
     - o corpo da resposta 202;
     - o intervalo de consulta;
     - o texto mostrado no caso `rejected`;
     - quando o estado volta a idle;
     - o que acontece com dois POSTs simultâneos.
  3. Falta definir os valores de `slot` e `gen` do `VALIDATE` ao adicionar e ao trocar uma conta. Falta dizer quais classes contam como `ok`.
  4. Nada substitui `finalize_pending_token()` (ino:1102). Falta a sequência: cifrar, `accountSave`, `acct_bump_gen`, `token_store_set`, `hist_load`, reset do `AccountState`, recriar o layout.
  5. O caminho `ST_SETUP_PIN` (ino:430–467, que cifra em :449–457) também precisa derivar a chave da sessão. O plano não o menciona. Também não menciona a checagem `!g_sessionPin[0]` em ino:2807.
  6. Não existe `encryptTokenWithKey`. Hoje `encryptToken` recebe o PIN (`crypto.h:16`). `deriveSessionKey` não diz qual salt usa: `deriveKey` recebe salt, e cada blob guarda o seu (`crypto.cpp:34,63`).
- **Correção:** criar no item 6 uma sublista numerada "Fluxo do token, onboarding e sessão" com:
  - poller iniciado em `setup()`, logo após `g_wifi.begin()`;
  - 202 com corpo `{"state":"pending"}`;
  - JS que consulta `GET /token/status` a cada 1 s, por no máximo 30 s;
  - corpo `{"state":"idle|pending|ok|rejected","reason":"auth|net|…"}`;
  - `ok` = `FC_OK`, todo o resto = `rejected`;
  - a sequência de adicionar/trocar acima;
  - o caminho `ST_SETUP_PIN`.

  No item 19, acrescentar `bool encryptTokenWithKey(const char*, const uint8_t key[32], EncryptedBlob&)`. Definir que o salt é o MAC de `esp_efuse_mac_get_default()`, como no `encryptToken` atual, e que `deriveSessionKey` só envolve o `deriveKey` existente (`crypto.cpp:10`).

### B3. `client.setConnectionTimeout()` não tem efeito
- **Ids:** `cd-1-1`, `ir-1-26`, `st-1-23`
- **Onde:** item 17 (l. 837 e 842).
- **Problema:** `HTTPClient::connect` chama `_client->connect(host, port, _connectTimeout)` (`HTTPClient.cpp:1109`). Essa chamada sobrescreve o valor configurado no client (`NetworkClientSecure.cpp:130`). O padrão é 5000 ms (`HTTPClient.h:293`). As outras duas chamadas estão certas:
  - `setHandshakeTimeout`, em segundos, com padrão de 120 s;
  - `HTTPClient::setTimeout(uint16_t)`.

  O texto "Os nomes exatos são conferidos no core…" deixa a decisão para quem implementar. Há mais duas lacunas no item: `fetchUsage` não recebe o slot para o log `[slot N]`, e tokens com mais de 199 caracteres caem em `SEM CHAVE` sem aviso.
- **Correção:** trocar por `https.setConnectTimeout(NET_CONNECT_MS)` (`HTTPClient.h:216`) e remover a frase "conferir". Registrar o log no poller e anotar o limite de 199 caracteres. Fixar uma única versão do core (3.3.8 instalada contra 3.3.11 citada).

### B4. `status.cpp` fica sem timeouts e pode disparar o reboot
- **Ids:** `cd-1-2`
- **Onde:** item 15 (l. 817) e item 17.
- **Problema:** `fetchModelStatus` só chama `https.setTimeout(API_TIMEOUT_MS)` (`status.cpp:28`). O handshake fica com o padrão de 120 s. Um TLS lento até status.claude.com passa dos 36 s e reinicia o aparelho.
- **Correção:** incluir `firmware/claude_stick/src/net/status.cpp` (Modify) na fase 2, com os mesmos três timeouts. O melhor é usar um helper comum, por exemplo `net_client_setup(WiFiClientSecure&, HTTPClient&)` em `api.cpp`.

### B5. "RGB565A8 no formato do `gen_logo_assets.py` atual" está errado
- **Ids:** `cd-1-4`
- **Onde:** item 22 (l. 883).
- **Problema:** `gen_logo_assets.py:58-75` gera `LV_COLOR_FORMAT_ARGB8888` (bytes B,G,R,A, stride `w*4`), igual a todo o `logo_assets.h`. Além disso, o item 37 apaga esse script.
- **Correção:** descrever o layout no próprio item:
  - cabeçalho `{LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565A8, 0, w, h, w*2, 0}`;
  - dados = w·h pixels RGB565 little-endian, seguidos de w·h bytes de alfa.

  O enum existe (`lv_color.h:144`). Do script antigo, reaproveitar só a estrutura de `to_c()`.

### B6. `start_web` não existe e o intervalo citado deixa parte do servidor de fora
- **Ids:** `cd-1-5`
- **Onde:** item 6 (l. 674).
- **Problema:** os símbolos reais são `stop_web` (:653), `ensure_mdns` (:656) e `start_data_web` (:826-835). O servidor do onboarding (`/`, `/token`) é criado dentro de `ui_token()` em :877-881, fora do intervalo 650-835. O plano também não nomeia `WEB_CSS` e `WEB_SPARK` (:683-703).
- **Correção:** listar:
  - `start_data_web`;
  - o trecho de `ui_token` em 877-881;
  - `web_form` e `web_result`;
  - os handlers `handleRoot`, `handleNotFound`, `handleTokenPost`, `handleWindow`, `handleTokensPost` e `handleInfo`;
  - `jll` e `jstr`;
  - `WEB_CSS` e `WEB_SPARK`.

  Intervalo correto: 646-835 + 876-882.

### B7. O intervalo de histórico não inclui a migração nem os helpers
- **Ids:** `cd-1-6`, `ir-1-24`, `st-1-8` (parte)
- **Onde:** item 5 (l. 666), item 15.
- **Problema:** a migração `/hist.bin → /hist0.bin` fica em `setup()`, ino:2770-2782, fora do intervalo 992-1078. Nenhum módulo recebe `hist_push` (:933), `hist_idx` (:941), `day_key`, `day_slot`, `accumulate_heat` (:965), `heat_mode_data`, `hist_path` (:1002, que lê `g_accts.active`) e `reset_history_ram`. Mesmo assim, o item 15 chama `hist_push` e `accumulate_heat` por slot.
- **Correção:** mover 930-1078 + 2770-2782 para `history.cpp`, com assinaturas por slot:
  - `hist_push(AccountState&, float, float, uint32_t now)`;
  - `accumulate_heat(AccountState&, float)`;
  - `hist_path(int slot, …)`.

  A migração vira `hist_migrate_legacy()`, chamada em `setup()`.

### B8. O texto de `/modelos` atribui a `model_chip()` coisas que ela não faz
- **Ids:** `cd-1-7`
- **Onde:** item 33 (l. 995).
- **Problema:** `model_chip` (ino:1313-1322) já gera `AUTH` e `REDE`, além de `"OK %.1fs"`, `LIMITADO`, `N/D`, `ERRO %d` e `--`. Os exemplos do plano usam outros textos: `ok   · 0.9s`, `limitado`, `erro 503`, `—`.
- **Correção:** dar uma tabela com o texto v3 exato por código HTTP, em pt e en, incluindo 404 → `N/D`. Dizer se `model_chip` será reescrita ou substituída. Ver também M34, que trata `NET` contra `REDE`.

---

## 🟡 Importantes

### M1. A `net_task` deveria ser dona do rádio e nunca tocar NVS, mas o RECONNECT não funciona assim
- **Ids:** `ir-1-3`, `cd-1-16`, `cd-1-17`
- **Onde:** item 15 (l. 793–795), item 16 (l. 827). Na spec: §1 (l. 169–171).
- **Problema:**
  - `autoConnect()` chama `_promote()` e `_saveAll()`, que gravam NVS (`wifi_manager.h:40,171-176`).
  - `NetJob` não tem campo para a lista de redes (3 × (33 + 65) bytes).
  - `_nets` é privado e não há getter de senha.
  - Continuam chamando `autoConnect` no loop: `pin_submit` (:476), `setup()` (:2790, :2795) e `bg_refresh` (:2709).
  - `wait_for_time` bloqueia o loop por até 10 s (:2670-2681).
- **Correção:**
  - Incluir `src/hw/wifi_manager.h` (Modify) com `copySaved(SavedNet out[3])` e `connectList(const SavedNet*, int n, int timeoutMs, int* winnerIdx)`, sem acesso a NVS.
  - O loop aplica `_promote` quando o resultado chega.
  - Criar o buffer `reconnBuf` protegido por `tokMx`.
  - Listar cada chamada que será removida.
  - Dizer que o WiFi do onboarding continua no loop, com o poller parado.
  - Timeout de 8 s por rede (3 × 8 = 24 s).

### M2. Ninguém configura o NTP na v3
- **Ids:** `ir-1-4`, `cd-1-18`
- **Onde:** item 16 (l. 828).
- **Problema:** hoje `ensure_time()`/`apply_tz()` (ino:2659-2665) só rodam a partir de `bg_refresh`/`wait_for_time`. Na v3 nada chama `configTime` depois de um RECONNECT, e todas as contas acabam em `NOCLOCK`. O plano também não diz quando começa a contagem de 60 s do NOCLOCK.
- **Correção:**
  - "`scheduler_tick()` chama `ensure_time()` quando `WiFi.isConnected() && !g_timeInit`."
  - `wait_for_time()` e `do_refresh()` são apagados.
  - A contagem de 60 s começa no primeiro tick com WiFi ligado e `time(nullptr) < 1e9`.

### M3. `dot_state` fica vermelho quando todas as contas estão pendentes
- **Ids:** `ir-1-5`
- **Onde:** item 11 (l. 740–743), testes (l. 765–772).
- **Problema:** a regra "todas as não pendentes em falhou" é verdadeira quando não há nenhuma conta não pendente. A implementação literal mostra vermelho no desbloqueio.
- **Correção:** escrever as regras em ordem:
  1. `nf` = número de contas `!pending && (falha || !keyOk)`; `np` = número de contas `!pending`.
  2. RED se `np > 0 && nf == np`.
  3. YELLOW se `nf > 0`.
  4. BLUE se `firstCycleRunning || (cycleEndEpoch != 0 && now - cycleEndEpoch < DOT_FRESH_SEC)`.
  5. GREEN nos outros casos.

  Acrescentar testes para: todas pendentes, n = 0, e uma pendente + uma falhou.

### M4. O teste E2E 5 contradiz a regra de "sem WiFi"
- **Ids:** `ir-1-6`
- **Onde:** Verification Plan (l. 1106) contra item 16 (l. 827).
- **Problema:** sem WiFi, o scheduler marca `FC_NOWIFI`, e o ponto fica vermelho (E2E 4). O E2E 5 espera azul.
- **Correção:** reescrever o E2E 5 como "WiFi conectado, mas sem internet/NTP: `consultando…` e ponto azul por até 60 s, depois `⚠ NOCLOCK` e vermelho". A outra saída é definir que `FC_NOWIFI` só vale depois que o primeiro RECONNECT falha.

### M5. `projection()` não reproduz o código que diz extrair "sem mudança"
- **Ids:** `ir-1-7`, `cd-1-8`
- **Onde:** item 10 (l. 724–729).
- **Problema:**
  - O histórico é um ring buffer lido com `hist_idx()`, e a assinatura não recebe `head`.
  - Hoje `n` conta as amostras dentro da janela **mais** o ponto atual (:1569-1587), então "<3" significa 2 amostras de histórico.
  - A taxa só vale se `rate > 0.02f` (:1622) e se a primeira amostra tiver mais de 300 s (:1605). Isso fica fora das linhas citadas.
  - A ordem das regras não está definida. Exemplo: com 1 amostra e 99,6 %, o resultado é SEM_DADOS ou ESGOTADA?
  - `minutes`/`etaEpoch` de `ESGOTADA` e `OK_ATE_RESET` não estão definidos.
- **Correção:** assinatura `projection(const Sample*, int n, int head, …)`. Ordem:
  1. `now < 1e9 || resetEpoch == 0` → SEM_DADOS.
  2. `pctNow >= 99.5` → ESGOTADA, com `etaEpoch = resetEpoch`.
  3. Amostras na janela + 1 < 3 → SEM_DADOS.
  4. Calcular a taxa como em 1596–1609.
  5. Se `rate > 0.02` e a ETA for ≤ reset → ESGOTA. Senão, OK_ATE_RESET, com `pctAtReset` arredondado e limitado a 0..100.

  Acrescentar casos de teste no item 13.

### M6. Textos novos só em português e projeção dependente do fuso da máquina
- **Ids:** `ir-1-8`, `cd-1-9`
- **Onde:** item 10 (l. 730–734), itens 29–31, 33 e 36.
- **Problema:**
  - A UI é bilíngue (`TRS`, ino:57, com `g_lang`), mas o plano não dá o texto em inglês de: projeção, `consultando…`, `SEM CHAVE`, `aguardando a rede…`, rodapé, menu, `/modelos` e classes.
  - "15:20" depende de `localtime`, então o teste de host muda conforme o fuso da máquina.
  - Não há formato para 60 minutos ou mais.
- **Correção:**
  - `projection_text(const ProjResult&, uint8_t lang, int32_t tzOffsetSec, char*, size_t)`, usando `gmtime_r(eta + tz)`. Os testes passam 0.
  - Formato `"%dm"` abaixo de 60 e `"%dh%02dm"` a partir de 60.
  - Uma tabela pt/en com todos os textos novos, no item 30 ou 31.

### M7. `FetchClass` fica num header que o teste de host não compila
- **Ids:** `ir-1-9`, `cd-1-21`
- **Onde:** item 4 (l. 660), item 11 (l. 741), item 13 (l. 758).
- **Problema:** `dot_state.h` precisa de `FetchClass`, que está em `src/model/account_state.h`. Esse header puxa `api.h`, que puxa Arduino.
- **Correção:** criar `src/logic/fetch_class.h` sem includes. Regra: `src/logic/*` só inclui headers C padrão e outros headers de `src/logic`.

### M8. Classificador sem função pura, listas de teste divergentes e flags diferentes
- **Ids:** `ir-1-10`, `ir-1-22`, `st-1-16`, `cd-1-34`
- **Onde:** item 13 (l. 754–776), item 17 (l. 839), Verification Plan (l. 1087, 1090, 1093).
- **Problema:**
  - O plano exige testar "todas as linhas das tabelas de verdade (ponto e classes de falha)", mas a classificação não está numa função pura nem tem arquivo de teste.
  - O item 13 compila com `-Wall -Werror`; a verificação pede `-Wall -Wextra -Werror`.
  - A verificação cita testes de `NOCLOCK` e de `layout_for_count` que o item 13 não lista.
  - "`FC_DISCARDED` sem efeito" não dá para testar em `dot_state`.
  - A regra de arredondamento de `text_bar` não está definida.
  - `fit_label` não diz se conta bytes ou code points, nem se o `…` entra na conta.
  - O retorno de `fetchUsage` e o conteúdo de `out` quando vem 401/429 com headers não estão definidos.
- **Correção:**
  - `src/logic/classify.{h,cpp}` com `FetchClass classify_usage(int http, bool hasUnifiedHeaders)`, mais `test_classify.cpp` com um caso por linha da §4, incluindo 401 com headers → `FC_AUTH`.
  - Usar `-Wall -Wextra -Werror` nos dois lugares.
  - Alinhar as duas listas de teste.
  - `text_bar`: `filled = (int)(pct*cells/100 + 0.5)`, limitado a 0..cells.
  - `fit_label` conta code points UTF-8, com o `…` incluído no limite.
  - Item 17: "`fetchUsage` retorna `hasUnifiedHeaders`; `out` é preenchido sempre que houver headers; `*httpOut` é sempre definido (≤0 em erro de transporte)."

### M9. A extração da fase 0 se contradiz e não tem interfaces
- **Ids:** `ir-1-11`, `cd-1-22`
- **Onde:** itens 2 (l. 629), 3 (l. 641–642), 4 (l. 652), 6 e 7 (l. 701).
- **Problema:**
  - O item 2 não lista o item 3 na fase 0, mas o item 3 move `render_state`, que chama funções `static ui_*` que só saem do `.ino` na fase 3.
  - Os handlers web usam globais do `.ino` e LVGL: `g_usage`, `g_accts`, `g_tok`, `g_pendingToken`, `g_tokMsg`, `lv_refr_now`, `update_tok_row` (:739-815).
  - O item 7 proíbe `extern` de variáveis mutáveis, mas não oferece accessors.
  - "Tokens atuais" é ambíguo (ver M32).
- **Correção:** escolher uma única fase para o item 3 (sugestão: fase 3). Dar a API dos headers da fase 0, por exemplo:
  - `history.h`: `hist_push`, `accumulate_heat`, `hist_save`, `hist_load`, `const Sample* hist_data(int* n, int* head)`;
  - `web_api.h`: callbacks `on_token_msg(const char*)` e `on_tok_stats()`.

  A alternativa é permitir `extern` temporário de forma explícita.

### M10. A fase 2 não diz como a máquina de estados antiga convive com o scheduler
- **Ids:** `ir-1-12`, `st-1-14`
- **Onde:** itens 6, 15, 16, 20 e 38 (l. 682, 1029–1038).
- **Problema:**
  - Na fase 2, `render_state()` ainda chama `stop_web()`, e `ui_token()` cria o próprio servidor (ino:837-883).
  - A regra "servidor ligado durante toda a sessão" (l. 682) não tem fase.
  - Não está dito se `scheduler_tick`/`poller_apply_results` rodam em `ST_SETTINGS`/`ST_ACCOUNTS`.
  - Não está dito o que "UI suja" faz quando os objetos do dashboard não existem.
  - `ST_LOADING`, `ST_ERROR` e `do_refresh` continuam existindo.
  - O pool de PSRAM, exigido "na fase 2", está escrito no item 38, sob o título da Fase 3.
  - A home de 1–4 colunas do item 20 não dá fontes nem campos.
- **Correção:**
  - Separar do item 38 um novo item da Fase 2, antes do item 15: "`lv_conf.h`: pool do LVGL na PSRAM".
  - No item 20, definir:
    - a fase 2 remove `ST_LOADING`/`ST_ERROR` depois do desbloqueio;
    - a fase 2 deixa de chamar `stop_web()` depois do desbloqueio;
    - `ui_token` reaproveita o servidor da sessão;
    - `g_uiDirty` só chama `refresh_ui_values()` em `ST_MAIN`;
    - fontes por número de colunas.

### M11. Backoff e ciclo do scheduler incompletos
- **Ids:** `ir-1-13`
- **Onde:** item 16 (l. 823–831).
- **Problema:** o plano não diz:
  - onde `backoffCycles` diminui;
  - se `fails` conta só `AUTH` consecutivos;
  - o efeito de `NOWIFI`/`NOCLOCK`;
  - como o fim do ciclo é detectado (inclusive com DISCARDED, reenvio e STATUS);
  - quando `firstCycleRunning` liga e desliga;
  - quem é dono de `g_probeIdx`;
  - o que acontece com a rotação quando a conta em foco está em backoff ou em `SEM CHAVE`.
- **Correção:** pseudo-código no item 16:
  - `cycleOutstanding` sobe a cada job enviado e desce a cada resultado, inclusive DISCARDED. Quando chega a 0: `cycleEndEpoch = now` e `firstCycleRunning = false`.
  - `firstCycleRunning = true` no desbloqueio.
  - Ao montar o ciclo, se `backoffCycles > 0`, diminui e pula a conta.
  - `fails` = `FC_AUTH` consecutivos; qualquer outra classe zera.
  - `NOWIFI`/`NOCLOCK` só alteram `lastClass`.
  - `probeIdx` mora em `scheduler.cpp` e só avança quando um probe é enviado.

### M12. `poller_apply_results` sem regras por classe, e limiares sem estrutura
- **Ids:** `ir-1-14`, `cd-1-30`, `st-1-9`
- **Onde:** item 15 (l. 810–816), item 35 (l. 1010).
- **Problema:**
  - Não está dito que `hist_push`/`accumulate_heat`/`check_thresholds` só rodam com `FC_OK` (inclusive 429 com headers).
  - Não estão listados os campos alterados numa falha.
  - Não está dito como o probe chega a `g_models[]`, como STATUS chega a `g_status`/`incident_sync`, nem como VALIDATE chega a `/token/status`.
  - O item 15 chama `check_thresholds(slot)` por resultado, mas o item 35 junta os cruzamentos do ciclo. Não há mecanismo, momento nem regra de desempate.
  - Hoje `check_thresholds()` usa globais (:1697-1720).
- **Correção:**
  - Escrever um ramo para cada `JobType` e classe.
  - `check_thresholds(slot)` só registra e retorna `{win, thr}`, com estado `thr` dentro de `AccountState`.
  - O scheduler guarda `bestMoment {slot, win, thr}`, trocado só por `thr` estritamente maior. Desempate: slot menor, depois 5h antes de semana.
  - `moment_flush()` mostra o momento no fim do ciclo.

### M13. A lista de arquivos e de funções se contradiz
- **Ids:** `ir-1-15`, `st-1-6`, `st-1-8`, `cd-1-24`, `cd-1-25`
- **Onde:** item 7 (l. 689–701), itens 15, 16, 20, 31, 33, 34 e 36 (l. 1020).
- **Problema:**
  - O item 7 cria `screen_main.cpp`, e os itens 20 e 31 o "substituem" por `screen_home.cpp`.
  - `screen_trend.cpp` e `screen_heat.cpp` estão como Modify, mas ninguém os cria. Hoje são `build_tile_trend`/`build_tile_heat`, em ino:1384 e 1449.
  - O item 36 marca como Modify arquivos que o item 7 cria na mesma fase.
  - Faltam os headers das telas.
  - Estas funções não têm dono nem assinatura: `scheduler_pause/resume`, `poller_idle`, `web_api_poll`, `app_state_tick`, `intent_focus/open/refresh`.
- **Correção:**
  - Uma lista única dos arquivos criados na fase 3, sem `screen_main.cpp`, com `src/ui/screens.h`.
  - Itens 34/36 passam a "Create (movido do `.ino` + restyle)", com as linhas de origem: 1370-1495, `trend_redraw` 1542-1660, `heat_redraw` 1662-1679.
  - Cada função entra no header do item dono: `intent_*` em `app_state.h`, pause/resume no item 16, `poller_idle` no item 15.

### M14. O item 14 aponta o arquivo errado
- **Ids:** `st-1-7`, `cd-1-31`
- **Onde:** item 14 (l. 778, 784).
- **Problema:** o título fala em `account_state.cpp`, mas as mudanças acontecem no `.ino`. Por exemplo, `trend_redraw` está em ino:1542.
- **Correção:** título "`claude_stick.ino` e `src/model/account_state.cpp` (Modify)".

### M15. Faltam glifos nas fontes, e a checagem de glifos é vaga
- **Ids:** `ir-1-16`, `cd-1-10`, `st-1-13`, `st-1-28`
- **Onde:** item 25 (l. 906–909), item 26.
- **Problema:**
  - Faltam U+2014 `—` (usado nas l. 734 e 994 e na §5) e U+2022 `•`. A Montserrat de fallback também não tem esses glifos.
  - A fonte numérica não tem placeholder para card pendente.
  - Só o `⚠` é checado; `✓`, `✗` e `❯` ficam de fora.
  - "glifo por glifo" não diz se a checagem é do script ou de uma pessoa.
  - O fallback "sem ícone" muda o formato do card sem dar a string.
  - `0xB7` repete um código que já está em `0xA0-0xFF`.
- **Correção:**
  - Acrescentar `0x2014,0x2022` aos textos e `0x2D` aos números.
  - Card pendente mostra `--%`.
  - O script confere todo code point fora de ASCII/Latin-1 e sai com erro se faltar algum. Se faltar o `⚠`, só avisa, e a linha vira `<classe> · <idade>`.
  - Remover o `0xB7` repetido.

### M16. `fonts.h` não pode atribuir `fallback`
- **Ids:** `cd-1-11`
- **Onde:** item 26 (l. 917), item 25.
- **Problema:** `lv_font_conv` gera `const lv_font_t` com `.fallback = NULL` (`lv_font.h:108`), então o campo não pode ser atribuído depois.
- **Correção:** usar `--lv-fallback lv_font_montserrat_14` no item 25 (conferir que a versão 1.5.3 aceita) e dar o comando completo: `--format lvgl --size N --font <ttf> -r <faixas> --lv-font-name … -o …`.

### M17. As fontes geradas não compilam sem uma flag
- **Ids:** `cd-1-12`
- **Onde:** itens 9 e 25.
- **Problema:** a saída do `lv_font_conv` inclui `"lvgl/lvgl.h"`, a menos que `LV_LVGL_H_INCLUDE_SIMPLE` esteja definido. O `LVFLAGS` (`build.sh:24`) só define `LV_CONF_INCLUDE_SIMPLE`.
- **Correção:** acrescentar `-DLV_LVGL_H_INCLUDE_SIMPLE` no item 9, ou usar `--lv-include lvgl.h`.

### M18. Imagens em header são duplicadas na flash
- **Ids:** `cd-1-13`
- **Onde:** itens 22 e 24 (l. 897).
- **Problema:** hoje as imagens são arrays `static const` em header (`logo_assets.h:5,299`). O `img_bird_braille_124` (cerca de 46 KB) seria incluído em 3 arquivos `.cpp` e copiado 3 vezes.
- **Correção:** o gerador escreve `assets_thep.c` com as definições e `assets_thep.h` com `LV_IMAGE_DECLARE`/`extern`.

### M19. Recolor sem opacidade não aparece
- **Ids:** `cd-1-14`
- **Onde:** item 24 (l. 897).
- **Problema:** o código atual usa `image_recolor` junto com `image_recolor_opa(…, 190)` (ino:1285-1286). A opacidade padrão é 0, então só o recolor não muda nada.
- **Correção:** nomear as duas chamadas e dar a cor e a opacidade (hoje 0x6A6A74 a 190).

### M20. O spike do pulso olha para o lugar errado
- **Ids:** `cd-1-15`, `ir-1-31`
- **Onde:** item 29 (l. 942–943).
- **Problema:** o display usa `LV_DISPLAY_RENDER_MODE_FULL` com buffer de tela cheia (ino:2755-2760), e `disp_flush_cb` copia os 480×320 pixels toda vez. Invalidar só o ponto não economiza nada. O spike também não diz o que acontece se passar.
- **Correção:** o spike testa também `RENDER_MODE_PARTIAL` com buffer menor e registra o modo escolhido em cada resultado. Acrescentar "se passar, nada muda na fase 3". A decisão do pulso em si continua valendo.

### M21. As métricas do `/health` não têm implementação
- **Ids:** `ir-1-17`
- **Onde:** item 6 (l. 678). Na spec: §9, E2E 2.
- **Problema:** não está dito como medir `frame_ms_max`, `cycles_over_interval`, `net_stack_hwm`, `lv_mem_used_pct` e `heap_internal_largest`.
- **Correção:** sublista no item 6 com a API de cada métrica e o módulo dono:
  - `LV_EVENT_RENDER_START`/`RENDER_READY` para o tempo de frame;
  - `uxTaskGetStackHighWaterMark(poller_task_handle())`;
  - `lv_mem_monitor`;
  - `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)`;
  - `scheduler_last_cycle_ms()`.

  Tudo atrás de `THEP_DEV`.

### M22. Falta o comando `build.sh dev`, a checagem de tamanho e a saída do log
- **Ids:** `ir-1-18`, `cd-1-26`, `st-1-15`
- **Onde:** item 9 (l. 714–716), item 38 (l. 1040). Verificação: l. 1097.
- **Problema:**
  - `build.sh` só aceita `build|upload|monitor` (`build.sh:29-55`).
  - "Imprimir tamanho" não diz de onde vem o número.
  - `LV_USE_LOG 1` sem `LV_LOG_PRINTF` nem callback não imprime nada.
- **Correção:**
  - Os comandos `dev` e `upload-dev` acrescentam `-DTHEP_DEV`.
  - Compilar com `--output-dir build`. Tamanho via `stat -f%z build/claude_stick.ino.bin`; falha acima de 2936012 bytes.
  - O item 38 registra `lv_log_register_print_cb`, que escreve na `Serial`.

### M23. O algoritmo e as entradas do gerador de assets não estão definidos
- **Ids:** `ir-1-19`, `cd-1-28`, `st-1-26`
- **Onde:** item 22 (l. 877–885).
- **Problema:**
  - O "script usado nos mockups" não está no repositório.
  - Faltam:
    - o texto ANSI Shadow de THEP;
    - o mapeamento das faixas de cor para as linhas;
    - a espessura do "traço grosso";
    - a conversão para braille (grade, limiar de alfa, cor, raio, passo);
    - os tamanhos exatos;
    - a fonte TTF usada.
- **Correção:**
  - Colar as 6 linhas ANSI Shadow no item.
  - Faixas: linhas 0–1 `#2fb5dd`, 2–3 `#2f8fd6`, 4–5 `#1272c2`.
  - Braille:
    - redimensionar para (W/cellW × 2) × (H/cellH × 4);
    - ponto aceso se alfa ≥ 128;
    - cor = média do bloco;
    - diâmetro e passo em px.
  - Tamanhos: 104×38, 124×124, 13×13.
  - Espessura do traço em px.

### M24. Geometria e cores da home não estão definidas
- **Ids:** `ir-1-20`
- **Onde:** itens 29, 31 e 32.
- **Problema:** faltam:
  - larguras, gaps, paddings e posição do título;
  - a definição de "espaço disponível", que o item 39 usa para falhar;
  - cores da projeção e colchetes da barra;
  - um tick de 1 s para o relógio do rodapé e para a expiração do azul;
  - a ordem z dos overlays.
- **Correção:**
  - Tornar `mockup-home.dc.html` normativo e copiar os valores para o item 32: margens 6/8, padding 14/12/8, gaps 18/12/18, coluna esquerda de 150 px, gap de linha de 8 px.
  - Projeção em `C_OK_INK`/`C_BAD_INK`; colchetes em `C_DIM`.
  - `app_state_tick()` reavalia a cada 1 s.
  - Overlays com `lv_obj_move_foreground` na ordem menu < momento < incidente.

### M25. Os logs de ciclo da §9 não têm item
- **Ids:** `ir-1-21`
- **Onde:** spec §9 (l. 472). Os testes E2E 4 e 6 dependem desses logs.
- **Correção:** no item 16, logs na serial `[CYC] start n=%d`, `[CYC] end %ums classes=…` e `[CYC] slot %d backoff %d`. No `setup()`, imprimir e limpar a chave NVS `lastReset`.

### M26. Não há regra de quando salvar o histórico
- **Ids:** `ir-1-23`
- **Onde:** item 5 (l. 668).
- **Problema:** "se a janela de 5h resetou" não tem regra de detecção. `lastSaveEpoch` não está no `AccountState`. Remover, trocar o token e factory reset não têm comportamento definido.
- **Correção:** acrescentar `lastSaveEpoch` e `lastSavedReset` ao `AccountState`. Salvar quando `now - lastSaveEpoch >= HIST_SAVE_MIN_SEC` ou quando `h5ResetEpoch != lastSavedReset`. Remover uma conta não apaga nem salva nada (G8).

### M27. `acct_focus()`/`acct_set_focus()` reimplementam código que já existe
- **Ids:** `cd-1-20`
- **Onde:** item 4 (l. 659).
- **Problema:** `AccountSlots.active`, `accountsLoad` (`accounts.cpp:71-75`) e `accountSetActive` (:124-128) já leem e gravam a chave NVS `acct`. `accountRemove` já move o foco (:116-121).
- **Correção:** fazer as funções novas delegarem para as existentes, ou citar as existentes direto.

### M28. O grep de `lv_` em `src/net/` falha já na fase 0 e promete mais do que prova
- **Ids:** `cd-1-23`, `st-1-17`
- **Onde:** Verification Plan (l. 1092), item 6.
- **Problema:** `web_api.cpp` fica em `src/net/` e chama `lv_label_set_text`/`lv_refr_now` (:745-763) e `update_tok_row` (:814). Além disso, o grep só pega chamadas diretas, e o texto diz "garantindo que a rede nunca toca o LVGL".
- **Correção:** mover `web_api` para `src/app/`, ou usar callbacks desde a fase 0. Reescrever o texto como "…um grep que falha se aparecer `lv_` em `src/net/`, o que bloqueia chamadas diretas ao LVGL no código de rede".

### M29. As mudanças no bridge estão incompletas
- **Ids:** `cd-1-27`, `ir-1-29`
- **Onde:** item 21 (l. 1058–1061).
- **Problema:** com `?account=`, o ramo de "conta diferente" (`token_bridge.py:87-90`) perde o sentido. Labels com espaço precisam de `urllib.parse.quote`. O 404 do GET acontece fora do `try` (só o POST trata `HTTPError`, :104-111). `PROJECTS` (:33) é global.
- **Correção:**
  - Remover ou manter o ramo de forma explícita.
  - Codificar o label na URL.
  - Tratar o 404 do GET.
  - `collect(start, projects_dir)` recebe o diretório como parâmetro.

### M30. Não se sabe onde ficam os TTF baixados
- **Ids:** `cd-1-29`
- **Onde:** item 39 (l. 1046), item 25.
- **Problema:** o `gen_mockups.py` lê "os TTF baixados pelo `gen_fonts.sh`", mas nenhum item diz onde ficam. Hoje o script usa `/System/Library/Fonts/HelveticaNeue.ttc` (:26).
- **Correção:** fixar o caminho `tools/.cache/fonts/` e incluir a entrada no `.gitignore` na lista de arquivos.

### M31. Numeração dos itens fora de ordem
- **Ids:** `st-1-1`, `ir-1-30`
- **Onde:** l. 873–877, 1051–1064.
- **Problema:** a Fase 3 começa no item 22. O item 21 aparece na Fase 4, depois do item 39. A linha 614 diz "a ordem segue as fases".
- **Correção:** renumerar na ordem do documento. Se quiser manter os números, anotar sob a Fase 4: "o item 21 foi movido para esta fase e mantém o número". Remover as linhas em branco duplas em 1052–1053 e 1062–1063.

### M32. "Tokens" significa duas coisas diferentes
- **Ids:** `st-1-2`
- **Onde:** l. 652, 968, 986, 1004, 1109.
- **Problema:** a spec separa **token OAuth** de **contagem de tokens** (l. 29–31). A linha 652 ("tokens atuais") pode ser lida como se o token OAuth fosse para o `account_state`, o que a §3 proíbe.
- **Correção:**
  - 652: "contagem de tokens (`g_tok`)".
  - 968 e 1004: "linha da contagem de tokens".
  - 986: "Contagem de tokens".
  - 1109: "a própria contagem de tokens".

### M33. `pending` e `pendente` usados para o mesmo estado
- **Ids:** `st-1-3`
- **Onde:** l. 742, 768, 828, 1087.
- **Correção:** no item 11: "`pending` = estado `pendente` da §4". Daí em diante, `pendente` no texto e `pending` só para o campo.

### M34. Nomes de classe misturam `FC_*`, forma curta e tradução
- **Ids:** `st-1-4`
- **Onde:** l. 800, 827, 839, 995, 1105.
- **Problema:** a linha 800 diz "publica `DISCARDED`", mas o enum é `FC_DISCARDED`. A linha 995 usa `REDE`, enquanto a §4 e o card usam `NET`.
- **Correção:**
  - 800: "`cls = FC_DISCARDED`".
  - Depois da l. 660: "na tela a classe aparece sem `FC_`".
  - Decidir entre `NET` e `REDE` em `/modelos`. Ver também B8.

### M35. O item 3 se contradiz sobre quais estados ficam fora do shell
- **Ids:** `st-1-5`, `cd-1-32`
- **Onde:** l. 642–646.
- **Problema:** a l. 644 inclui BOOT e ERROR fora do shell; a l. 646 não. `ST_LOADING` não aparece em nenhuma das duas. O código chama o enum de `State` (ino:67), não de `AppState`.
- **Correção:** "Fora do shell: `ST_BOOT`, `ST_PIN`, `ST_SETUP_PIN`, `ST_WIFI`, `ST_TOKEN` e `ST_ERROR` (só antes do desbloqueio). `ST_LOADING` é removido. `State` é renomeado para `AppState`."

### M36. `token_store_clear(slot)` troca o `gen`, mas não recebe o `gen`
- **Ids:** `st-1-10`
- **Onde:** l. 798 e 864.
- **Correção:** assinatura `token_store_clear(slot, newGen)`. Sequência: `acct_bump_gen(slot)` e depois `token_store_clear(slot, acct[slot].gen)`.

### M37. As descrições do menu são citadas, mas não definidas
- **Ids:** `st-1-11`
- **Onde:** l. 952.
- **Correção:** listar as descrições do mockup (`mockup-home.dc.html:168`): `/agora` uso das contas, `/modelos` status por modelo, `/janela` gráfico 5h, `/ritmo` uso por hora, `/contas` gerenciar contas, `/ajustes` configurações. Os comandos não são traduzidos. Dar também a descrição de `↻ Atualizar agora`, ou escrever "sem descrição".

### M38. O que K conta no rodapé não está definido
- **Ids:** `st-1-12`
- **Onde:** l. 940–941.
- **Correção:** "`<pássaro 13px> thep │ N contas · K ✗ │ ↻ <intervalo> │ HH:MM`, onde N = slots salvos e K = contas em `falhou`. O trecho `· K ✗` some quando K = 0." Deixar claro que `2min` é um exemplo.

---

## 🟢 Menores

| Id(s) | Onde | Problema | Correção |
| --- | --- | --- | --- |
| `ir-1-25` | item 4, l. 658 | `acct_clear` "zera o token", mas o token não está no `AccountState`. | "zera o registro; o token é zerado por `token_store_clear`". |
| `ir-1-27` | item 15, l. 804 | Se o modelo da rotação for o próprio `PROBE_MODEL`, o reenvio repete a mesma requisição. | "não repete se `model == PROBE_MODEL`". |
| `ir-1-28` | item 36, l. 1017 | `load_persisted()` continua lendo a chave `slide` (ino:339). | Parar de ler a chave, sem apagá-la. |
| `ir-1-32` | item 6 | Não está definido o que `/window?account=` devolve para conta sem dados. | "campos 0 e `\"pending\":true`". |
| `cd-1-33`, `st-1-31` (parte) | item 36, l. 1018 | "toque para trocar" não existe no código. O texto real está em ino:2553, e o marcador "•  ativa" em :2512. | Citar os textos reais. Escrever "Tocar numa conta define o foco". |
| `cd-1-35` | item 37, l. 1024 | `assets/brand/claudecode-*.svg`, `clawd-dead.png` e `REFERENCIA-HARDWARE-LVGL.md:249` citam arquivos apagados. | Incluir esses arquivos num item. |
| `cd-1-36` | itens 39/40 | `mock-agora.png`/`mock-contas.png` são apagados na fase 3, mas os READMEs (`README.md:17,75,194`, `README.pt-BR.md:17,76,198`) só são atualizados na fase 4. | Atualizar os READMEs junto, ou adiar a remoção. |
| `cd-1-37` | item 38, l. 1035–1036 | Se `heap_caps_malloc` retornar NULL, o valor vai direto para `lv_tlsf_create_with_pool`. | Dizer o que acontece (abort com log ou fallback). |
| `cd-1-38` | item 17, `build.sh:12` | O plano cita core 3.3.11; está instalado 3.3.8. | Fixar uma versão (ver B3). |
| `st-1-18` | l. 614 | "compilando e gravável" mistura gerúndio e adjetivo. | "compilado e gravável". |
| `st-1-19` | l. 794 | "sob o mesmo mutex" aparece antes de o `tokMx` ser apresentado. | "…em `token_store`, sob o mutex `tokMx`". |
| `st-1-20` | l. 817 | "Se passar de" e "A falta de jobs" não têm sujeito claro. | "Se o job atual estiver em andamento há mais de `NET_JOB_STUCK_MS`… Fila vazia não conta como job travado." |
| `st-1-21` | l. 825 | "por slot decifrado fora de espera" é difícil de entender. | "um `USAGE` para cada slot decifrado que não esteja em espera crescente". |
| `st-1-22` | l. 831 | "Zera ao trocar o token" não diz o que zera. | "`n` e `backoffCycles` voltam a zero quando o token é trocado". |
| `st-1-24` | l. 865 | `factory_reset` também zera os tokens, mas o texto não diz. | "zera os tokens (`token_store_clear_all()`) e a chave da sessão (`memset(g_sessionKey)`)". |
| `st-1-25` | l. 872 | "o mínimo exigido pela CUJ 2" é um qualificador vago. | Apagar, ou "como na CUJ 2 (sem o esmaecimento, que entra na fase 3)". |
| `st-1-27` | l. 897, 1010 | O nome canônico é "momentos de limiar" (C17), e a frase está fora de ordem. | Usar o termo e reordenar a frase do `img_bird_braille_124`. |
| `st-1-29` | l. 912 | As fontes numéricas são Bold, mas o nome não mostra o peso. | `jbmono_bold_num_*`, ou "(Bold)" no título. |
| `st-1-30` | itens 32–33 (l. 981, 985, 988, 994) | "células" contra "blocos"; "completa (até 20)" se contradiz; "o mockup aponta" deveria ser "o `gen_mockups.py` falha"; espaçamento de `ok · 0.9s` diferente da spec l. 438. | Uniformizar os termos e os exemplos. |
| `st-1-31` | l. 943, 1003, 1027 | `intent_focus(next)` não está definido; "Antes de fechar o shell" é ambíguo; "rebuild por mudança de humor" é jargão. | "próxima conta salva em ordem de slot, voltando à primeira"; "Antes de dar o item 29 por concluído"; "a recriação dos mascotes quando o estado do modelo muda". |
| `st-1-32` | Verification Plan (l. 1084–1108) e l. 1037, 1102 | Falta registrar o número base de warnings; `levels` sem a lista de funções; "Roda em todo PR" sem dizer quem roda; resultado esperado após 60 s ausente; pré-condição "com 2 contas" ausente; linhas só com espaços e continuação com 7 espaços. | Completar cada ponto. Indentar a continuação com 4 espaços. |

---

## Citações confirmadas pelo revisor de codebase (resumo)

**`claude_stick.ino` (2906 linhas):**
- `Sample` na l. 141, `HIST_MAX` 160;
- taxa em 1596-1609, dentro de `trend_redraw` 1542-1660;
- cores em 40-53, `pct_color`/`grad_color`;
- `HistFileV1/V2`, `save_history` e `load_history` dentro de 992-1078;
- `factory_reset` :351, `pin_submit` :430, `g_sessionPin` :105-108;
- `render_state`/`stop_web` em 2620-2635;
- `lv_obj_set_ext_click_area` já em uso.

**Resto do firmware:**
- `api.cpp:63`, que trata 401 com headers como OK;
- em `config.h`: `FW_VERSION "2.3"`, `API_TIMEOUT_MS 15000`, `STATUS_POLL_SEC 300` e `PROBE_MODEL`;
- `ACCT_MAX` 4;
- limite de 2936012 bytes, compatível com a partição 0x300000.

**Ferramentas:**
- `token_bridge.py:49`: o glob não é recursivo.

**LVGL 9.2.2:**
- `LV_MEM_POOL_INCLUDE`/`LV_MEM_POOL_ALLOC`, `LV_USE_TILEVIEW`, `LV_USE_LOG`;
- `LV_SYMBOL_REFRESH`;
- `image_recolor`/`_opa`, `lv_mem_monitor`, `lv_font_t.fallback`, `LV_COLOR_FORMAT_RGB565A8`.

**Core esp32 3.3.8:**
- `setHandshakeTimeout` recebe segundos;
- `HTTPClient::setTimeout(uint16_t)`;
- `HTTPClient::setConnectTimeout`.
