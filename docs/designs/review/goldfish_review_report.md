Status: resolved

# Goldfish Annotated Review Report — etapas 5, 6 e 7

**Target:** `docs/designs/thep-usage-stick-v3.md` (FeatureRange, linhas 1–448 na versão revisada, e itens do plano detalhado afetados pelas mesmas mudanças)

**Modo:** o dono pediu execução autônoma, o equivalente a "auto accept". Os achados `PROPOSE FIX` foram aplicados. Os que contradizem decisões do dono foram mantidos como `CONTEST`, citando onde a decisão está registrada. A versão anterior do documento está salva no scratchpad da sessão (`design_before_stage7.md`).

**Fontes dos achados:**
- `zoom_out_report.md`: etapa 5, M1–M11 e N1–N8;
- `comprehension_report.md`: etapa 6, C1–C19;
- `critic_report.md`: etapa 7, itens 1–52.

## Resumo

| Categoria | Quantidade |
|---|---|
| PROPOSE FIX (aplicado) | 71 |
| CONTEST (decisão do dono mantida) | 4 |
| DECLINE (Nit não aplicado) | 7 |
| ESCALATE | 0 |

## CONTEST — decisões do dono mantidas

### M4 (etapa 5) / item 4 parcial (etapa 7) — verde fixo, azul como flash
*   **Suggestion:** CONTEST.
*   **Justificativa:** o dono pediu o ponto pulsante em todas as cores ("coloque um dot pulsante na cor verde se estiver ok…", registrado no [brief](../notes/concepts_brief.md), Overview).
*   **Correção aplicada:** a contradição "fixo, com respiração" foi resolvida. Todas as cores pulsam com a mesma animação (4 degraus em 1,6 s, só a área do ponto). O risco de render entrou em Risks & Mitigations, com um spike na fase 3 e um plano B.

### M10 (etapa 5) — 3 contas na grade 2×2
*   **Suggestion:** CONTEST.
*   **Justificativa:** o mockup aprovado mostra 3 colunas para 3 contas (`review/context/mockup-home.dc.html`, ramo `is3`). O dono aprovou os mockups.

### M11 (etapa 5) — polling adaptativo
*   **Suggestion:** CONTEST.
*   **Justificativa:** o dono definiu "serão consultadas na frequência definida por mim" (brief, Key Decisions 2). O item continua em Future Improvements e agora aparece também nos Non-Goals.

### N3 (etapa 5) / §5 — fila de momentos
*   **Suggestion:** PROPOSE FIX, aplicado. Listado aqui porque contradiz o texto original do draft, não uma decisão do dono: a fila saiu, e cada ciclo mostra só o maior limiar.

## PROPOSE FIX — aplicados

| Achado | Onde | Correção |
|---|---|---|
| M1, C1, C2, crit 1, 2, 3 | G4, §4, item 11, testes | Novo estado `pendente`, que não conta como falha. "Falhou" passa a ser contado em ciclos (2 ciclos sem `OK`) e só vale com dado e relógio válidos. O azul usa `cycleEndEpoch`. `dot_state` ganha a assinatura nova |
| M2, crit 9 | §4, item 15 | Sem `vTaskDelete`. Um job travado há mais de 36 s leva a `esp_restart()` com o motivo gravado na NVS. A falta de jobs não dispara o reinício |
| M3 | §1, itens 19 e 36 | Remover conta = `gen++` + zerar token + `xQueueReset`, sem esperar a task |
| M5 | §1 | O pool do LVGL vai para a PSRAM |
| M6 | §8, §10, item 21 | O contrato do bridge vai para a fase 4. `accounts[]` é removido |
| M7 | §2, itens 15–17 | O job `PROBE` acaba. O `USAGE` da conta em foco faz o rodízio de modelos e serve de sonda |
| M8 | §10, itens 2 e 7 | A fase 0 só extrai o que as fases 1 e 2 mudam. As telas saem do `.ino` na fase 3 |
| M9, crit 12, C11 | §6, itens 25–27 | Só JetBrains Mono: Regular 11 e 13, Bold 13 e Bold numérica 20/30/38/50. Sem DejaVu e sem Noto. `↻` vem da Montserrat 14, e `⏲` saiu. O flash passa a ser estimativa declarada |
| N1 | §6, item 24 | O pássaro cinza do incidente sai por `image_recolor`, sem imagem separada |
| N2, crit 22 | §5, item 32 | A tabela da §5 é declarada provisória, e o item 32 a substitui. A linha das 4 contas foi alinhada |
| N4 | §9, item 6 | `/health` só na build `THEP_DEV` |
| N6 | item 42 | Removido (processo de escrita) |
| N7, crit 13 | §1, §7, itens 15, 17 e 19 | O token OAuth não passa pela fila: fica numa tabela sob mutex, com buffers de stack zerados. O resíduo no `HTTPClient` foi declarado. Gatilhos de zeragem corrigidos. A chave da sessão substitui o PIN em RAM |
| N8, crit 31 | §10, item 20 | A tela da fase 2 vira N colunas e mostra a classe do erro e a idade |
| C3, crit 8 | §2, item 16 | Sem WiFi, só o job `RECONNECT`, que percorre as redes salvas |
| C4 | Overview | Bridge definido. "Token OAuth" e "contagem de tokens" são distinguidos |
| C5, crit 17 | §6 | A tabela de paleta foi copiada para o documento |
| C6 | Overview | Hermes Agent e THEP apresentados |
| C7 | §10 | Tabela de fases alinhada com os itens |
| C8, crit 19 | §4, §5, §7 | `bloq` vira `limite`. O slot que não decifra vira `SEM CHAVE` e conta como falha. A tabela da marca de estado foi criada. Open Questions = "Nenhuma." |
| C9 | §5 | Tabela header → marca → cor |
| C10, crit 25, 26 | §2, Risks | PROBE agora vem junto com a consulta da conta em foco. Custo descrito |
| C12, crit 11 | §1, §9, item 6 | Contratos de `/health` e `/token/status` |
| C13 | §3, item 10 | Assinatura única de `projection`. `Sample` definido |
| C14 | §2, item 16 | Espera crescente de 2^(n−1) |
| C15, crit 4 | §4 | Animação do pulso definida |
| C16 | item 20 | Fase 2 com N colunas |
| C17, crit 21 | Problem | "Momento de limiar" definido (25/50/70/100%) e usado como nome único |
| C18 | Overview, refs | UI bilíngue mencionada. Jargão EGM retirado |
| C19 | Problem | Clawd apresentado na primeira menção |
| crit 5 | §4, Risks, item 29 | Plano B do pulso reescrito, com spike de gravação parcial |
| crit 6 | §5, CUJ 4, itens 3 e 36 | Token e WiFi viram conteúdos do shell depois do desbloqueio. O WiFi pausa o agendador |
| crit 7 | §2, §5, item 16 | Condição "sem relógio" (NTP) e prazo de 60 s. `ST_LOADING`/`ST_ERROR` saem do caminho pós-desbloqueio |
| crit 10 | §2, CUJ 1, Alt. E, item 17 | Timeouts de conexão, handshake e leitura. Pior caso de 90 s declarado. Idade = intervalo + ciclo |
| crit 14 | Alternatives | Cada rejeição começa por um trade-off técnico. Link para o registro das decisões |
| crit 15 | Risks & Mitigations | Nova seção com 9 riscos |
| crit 16 | Overview, G2, Additional Resources | Links para as cópias locais |
| crit 18 | G1, G4 | Recuos do Markdown corrigidos |
| crit 20 | §3 | "cálculo da taxa" |
| crit 23 | G4, CUJ 2, §4 | "classe do erro" como nome único |
| crit 24 | G5, §5 | O menu lista `Atualizar agora` e 6 telas. Sobre fica em `/ajustes`. Nomes internos mapeados |
| crit 27 | §4 | Classe `ERR` para outros 4xx. 401 com headers vira `AUTH` |
| crit 28, 35 | §5 | Citação corrigida para `render_state()`, `claude_stick.ino:2620-2635`, em formato de código |
| crit 29 | §8 | Compatibilidade "só para a conta em foco" |
| crit 30 | §7, Risks | Exposição na LAN registrada |
| crit 32 | todo o FeatureRange | Marcadores `*` |
| crit 33 | todo o FeatureRange | Links com texto |
| crit 34 | §5 | Nome exato da seção. Tabela marcada como provisória |
| crit 36 | Additional Resources, Future Improvements | Promovidos a H2 |
| crit 37 | NFR, §7 | Regras de tokens e de flash num só lugar, com referência no outro |
| crit 38 | §5, §6 | Subtítulos H4 |
| crit 39 | G7 | Significado de `~78%` e `38m` |
| crit 40 | Problem, §4 | Ressalvas defensivas cortadas |
| crit 41, 42, 43 | Problem | Reescritas |
| crit 44 | Open Questions, §10, NFR | "draft", "Restyle" e "home de 4 contas" trocados por termos em português |
| crit 46, 47 | §6, §7 | Sujeito e "Hoje" corrigidos |
| crit 48 | Alternatives A, B, J | Opiniões atribuídas ao dono. "Foto do caduceu" explicada |
| crit 49 | Alt. D | Pior caso de 15 s |
| crit 50 | Alt. H | "0,5 em vez de 0,6 em" |
| crit 51 | Additional Resources | Detalhe de processo cortado |

## DECLINE — Nits não aplicados

| Achado | Motivo |
|---|---|
| N5 (etapa 5) | Os detalhes de struct na §1 e na §3 ajudam a ler a arquitetura. Os itens 15 e 18 trazem a versão exata |
| crit 45 | "Mesclados" deixou de ser necessário: a fonte agora é uma família só |
| crit 52 | Duplicata do item 25, sem achado novo |
| M5 (parte `setReuse` na fase 2) | Continua em Future Improvements: trocar o `Authorization` numa conexão reaproveitada ainda não foi testado com a API |
| C18 (parte "resumir as regras de incidente da v2.3") | Regra herdada sem mudança, descrita no README atual |
| crit 1 (parte "pôr pendente acima do amarelo") | Resolvido de outro jeito: pendente não conta como falha, e a prioridade das cores não muda |
| M9 (parte "dois tamanhos de texto") | Ficaram 3 fontes de texto (11, 13 e Bold 13), porque os mockups aprovados usam negrito nos rótulos |

## Etapa 8 — achados aplicados

Fonte: `intermediate_zoom_out_report.md`.

| Achado | Correção |
|---|---|
| `xQueueReset` apagava os jobs de todas as contas | Removido. A tabela de tokens guarda o `gen` de cada slot. A task confere o `gen` e devolve `DISCARDED` sem fazer a requisição quando o slot foi removido |
| Rodízio de modelos deixava a conta amarela por falha de um modelo | Isolamento da sonda: uma resposta sem headers com o modelo do rodízio vai só para `/modelos`, e a task repete a consulta com `PROBE_MODEL` |
| Regra de 2 ciclos nunca acionada | Removida, junto com o `cyclesSinceOk`. "Em falha" voltou à definição original do dono (última consulta falhou). O estado `pendente` continua |
| Pool do LVGL na PSRAM sem item e com mecanismo errado | `LV_MEM_POOL_INCLUDE`/`LV_MEM_POOL_ALLOC` com `heap_caps_malloc(SPIRAM)` no item 38, na fase 2 |
| Nits | Item 21 movido para a fase 4; item 20 aponta para o `ui_main()` no `.ino`; variante cinza removida do item 22; `FC_ERR`, `FC_NOCLOCK` e `FC_DISCARDED` no enum; "grep no CI" virou `run.sh`; sem relógio aparece como `NOCLOCK`, e não `NOWIFI` |
| Excessos pequenos (zeragem de buffers, espera crescente, mudança de pastas do item 8) | DECLINE: custo baixo e pedidos em etapas anteriores |

## Etapa 9 — rodada 1

Fonte: `implementation_readiness_report.md` (8 🔴, 38 🟡, 22 🟢). Modo: execução autônoma ("auto accept"). A versão anterior do documento está no scratchpad da sessão (`design_before_stage9.md`).

O Detailed Implementation Plan foi reescrito e renumerado na ordem do documento (itens 1–47). As mudanças de comportamento foram levadas também à spec (§1, §2, §3, §4, §5, §6, §10 e Risks). Todas as APIs citadas foram conferidas no core `esp32:esp32` 3.3.8, no LVGL 9.2.2 (`~/Documents/Arduino/libraries/lvgl`) e no `lv_font_conv` 1.5.3.

### Resumo

| Categoria | Quantidade |
|---|---|
| PROPOSE FIX (aplicado) | 8 🔴 + 38 🟡 + 22 🟢 = 68, mais 1 achado novo da própria resolução |
| CONTEST (parte de um achado) | 2 |
| DECLINE | 0 |
| ESCALATE | 0 |

### Mapa de numeração (antigo → novo)

| Antigo | Novo | Antigo | Novo |
|---|---|---|---|
| 1, 2 | 1, 2 | 22–24 | 26–28 |
| 3 (app_state) | 25 (fase 3) | 25–27 | 29–31 |
| 4 | 3 | — | 32 (`i18n.h`, novo) |
| — | 4 (`fetch_class.h`, novo) | 28–33 | 33–38 |
| 5 | 5 | 7 + 34 + 36 | 39 (lista única de telas) |
| 6 | 6 (agora em `src/app/`) | 35 | 40 |
| 8, 9 | 7, 8 | 37 | 41 |
| 10 | 9 | 38 (fase 3) | 42 |
| 11 | 10 | 39 | 43 |
| — | 11 (`classify`, novo) | — | 44 (`.gitignore`, novo) |
| 12–14 | 12–14 | 21 | 45 |
| 38 (pool) | 15 (fase 2, com `lv_pool.{h,c}`) | 40, 41 | 46, 47 |
| — | 16 (`token_store`), 17 (`wifi_manager.h`) | 15–19 | 18–23 (status.cpp = 21, novo) |
| 20 | 24 | | |

### PROPOSE FIX — aplicados

| Achado | Onde | Correção |
|---|---|---|
| B1 | §2, §4, Risks, itens 18 e 22 | Watchdog **por requisição**: `reqStartMs` é gravado antes de cada HTTPS, inclusive o reenvio da sonda. `NET_REQ_STUCK_MS 90000`. Pior caso de uma requisição ~39 s (DNS ~21 s estimado a partir de `DNS_MAX_RETRIES 4`/`DNS_TMR_INTERVAL 1000` do lwIP e `CONFIG_LWIP_DNS_MAX_SERVERS 3`, + 5 + 5 + 8). Pior caso do ciclo: 6 requisições, ~4 min com DNS lento e 108 s com DNS em cache |
| B2 | §1, itens 6, 16, 23 | Sublista "fluxo do token, onboarding e sessão": `poller_start()` no `setup()`, 202 `{"state":"pending"}`, JS com consulta a cada 1 s por até 30 s, corpo de `/token/status`, `ok` = `FC_OK`, 409 para POST simultâneo, sequência de adicionar/trocar no lugar de `finalize_pending_token`, caminho `ST_SETUP_PIN`, `!g_sessionPin[0]` → `!session_unlocked()`. `deriveSessionKey`/`encryptTokenWithKey`/`decryptTokenWithKey` envolvem `deriveKey` com o MAC como salt |
| B3 | itens 20 e 22, §2 | `https.setConnectTimeout(NET_CONNECT_MS)` (`HTTPClient.h:216`); frase "conferir" removida; log `[slot N]` no poller; limite de 199 caracteres no POST e no desbloqueio; core fixado em 3.3.8 |
| B4 | item 21 | `status.cpp` (Modify) usa o helper `net_client_setup` do item 20 |
| B5 | item 26 | Layout RGB565A8 descrito (cabeçalho, RGB565 LE + plano alfa, `data_size = w·h·3`), conferido em `lv_image_dsc.h:88-97` |
| B6 | item 6 | Símbolos reais e intervalos 646-835 + 876-882 |
| B7 | item 5 | 930-1078 + migração 2770-2782 (`hist_migrate_legacy`), todos os helpers e assinaturas por conta |
| B8 | itens 11 e 38 | `model_chip` substituída por `model_status_text`, com tabela pt/en por código HTTP |
| M1 | itens 16, 17 | `copySaved`, `connectList` estático sem NVS, `promote` no `loop()`, cópia das redes no `token_store` sob `tokMx`, lista de chamadas de `autoConnect` removidas/mantidas |
| M2 | item 19, §2 | `ensure_time()` no `scheduler_tick`; `wait_for_time`/`do_refresh`/`bg_refresh` apagados; prazo do NOCLOCK conta do primeiro tick com WiFi |
| M3 | item 10, §4 | Regras em ordem com `np > 0`; slot `SEM CHAVE` nunca é pendente (o achado não cobria isso: com a regra literal, ele nunca contaria como falha) |
| M4 | Verification E2E 5 | Reescrito para "WiFi sem internet" |
| M5 | item 9 | Assinatura com `head`, regras em ordem, `minutes`/`etaEpoch` de cada estado, testes no item 13 |
| M6 | itens 9 e 32 | `projection_text` com `lang`, `tzOffsetSec` e `maxChars`; formato `%dh%02dm`; tabela pt/en de todos os textos novos |
| M7 | item 4 | `src/logic/fetch_class.h` e regra de includes de `src/logic` |
| M8 | itens 11–13, 20, Verification | `classify.{h,cpp}` + `test_classify.cpp`; `-Wall -Wextra -Werror` nos dois lugares; listas alinhadas; regras de `text_bar` e `fit_label`; contrato de `fetchUsage` |
| M9 | itens 2, 3, 5, 6, 25 | `app_state` só na fase 3; `extern` temporário explícito nas fases 0–2; callbacks de tela no `web_api`; API do `history.h` |
| M10 | itens 15 e 24 | Pool em item próprio da fase 2; convivência com a máquina de estados antiga; fontes da home da fase 2 |
| M11 | item 19 | Pseudo-código do ciclo, backoff, `firstCycleRunning`, `probeIdx`, NOWIFI/NOCLOCK |
| M12 | itens 18, 19, 40 | Tabela por tipo e classe; `check_thresholds` devolve `{win, thr}`; `bestMoment` e `moment_flush` |
| M13 | itens 2, 18, 19, 25, 39, 40 | Lista única de telas (sem `screen_main.cpp`, com `screens.h`); dono e assinatura de cada função |
| M14 | item 14 | Título com `claude_stick.ino` |
| M15 | §6, itens 29 e 36 | `—`, `•` e `-` nas faixas; `--%`; checagem de glifos pelo script com `fontTools`; `0xB7` repetido removido |
| M16 | item 29 | `--lv-fallback lv_font_montserrat_14` (`lib/cli.js:282` do 1.5.3) e comando completo |
| M17 | item 29 | `--lv-include lvgl.h` (`lib/cli.js:266`) |
| M18 | itens 26 e 28 | `assets_thep.c` + `assets_thep.h` com `LV_IMAGE_DECLARE` |
| M19 | item 28, §6 | `image_recolor` + `image_recolor_opa(190)` com `0x6A6A74` |
| M20 | item 34, Risks, §4 | Spike mede o modo FULL e testa o PARTIAL; "se passar, nada muda" |
| M21 | item 6 | Tabela de métricas do `/health` com API e módulo dono (`LV_EVENT_REFR_START/READY`, `lv_mem_monitor`, `uxTaskGetStackHighWaterMark`…) |
| M22 | itens 8 e 42 | `dev`/`upload-dev`, `--output-dir build`, tamanho por `wc -c`, `lv_log_register_print_cb` |
| M23 | item 26 | Texto ANSI Shadow, faixas por linha, desenho por células, regras do braille e tamanhos |
| M24 | itens 34–36, §5 | Geometria copiada do mockup (painel, cabeçalho, grade por layout, larguras de card), cores da projeção, `app_state_tick` a cada 1 s, `overlay_restack` com `lv_obj_move_to_index` (a API v9; `lv_obj_move_foreground` só existe no mapa v8) |
| M25 | item 19 | Logs `[CYC] start/end/backoff` e `[BOOT] lastReset` |
| M26 | item 5, §3 | `lastSaveEpoch`/`lastSavedReset` e regra de gravação. Ver CONTEST parcial abaixo |
| M27 | item 3 | Foco = `AccountSlots.active` + `accountSetActive`, sem função nova |
| M28 | item 6, Verification | `web_api` em `src/app/`; texto do grep reescrito |
| M29 | item 45 | Ramo "conta diferente" mantido de forma explícita (protege contra aparelho v2.3); `quote`; 404 do GET tratado; `collect(start, projects_dir)` |
| M30 | itens 29, 43, 44 | `tools/.cache/fonts/` e `.gitignore` |
| M31 | todo o plano | Renumerado na ordem do documento; linhas em branco duplas removidas |
| M32 | itens 3, 36, 37, 39, E2E 8 | "contagem de tokens" |
| M33 | item 10 | `pendente` no texto, `pending` só como campo |
| M34 | §1, itens 4, 18, 38 | `FC_DISCARDED`; classe sem `FC_` na tela; `NET` no lugar de `REDE` |
| M35 | item 25 | Lista de estados fora do shell; `ST_LOADING` removido; `State` → `AppState` |
| M36 | item 16 | `token_store_clear(slot, newGen)` e sequência |
| M37 | item 35 | Descrições do mockup; `Atualizar agora` sem descrição |
| M38 | item 34 | Definição de N e K; intervalo como exemplo |
| Nits (22) | vários | Todos aplicados: `acct_clear` (ir-1-25), sem reenvio quando o modelo é o `PROBE_MODEL` (ir-1-27), chave `slide` não lida (ir-1-28), `/window` de conta sem dados (ir-1-32), textos reais de `/contas` (cd-1-33), assets e `REFERENCIA-HARDWARE-LVGL.md` em itens (cd-1-35), remoção dos PNGs e SVGs adiada para junto do README na fase 4 (cd-1-36), `NULL` do pool → `abort()` com log via `lv_pool.c` (cd-1-37), core 3.3.8 (cd-1-38) e os de estilo (st-1-18 a st-1-32) |
| Achado novo | §5, item 35 | O menu com 7 linhas de 44 px (316 px) não cabia nos 270 px abaixo do cabeçalho. Linhas de 36 px (7 × 36 + 8 = 260 px). A spec dizia "na largura toda"; o plano já usava 262 px, agora igual nos dois |

### CONTEST — partes de achados

| Achado | Parte contestada | Evidência |
|---|---|---|
| M26 | "Remover uma conta não apaga nem salva nada (G8)" | A v2.3 apaga `/histN.bin` ao remover a conta (`claude_stick.ino:2391-2392`, `acct_del_cb`; **nota da rodada 2:** as linhas certas são 2393-2394, e a conclusão não muda), e o E2E 7 testa justamente que o arquivo não é recriado. O G8 trata de atualizar e voltar de versão, não de remover conta. O plano mantém o comportamento da v2.3 (item 5) |
| M24 | "colchetes em `C_DIM`" e "gaps 18/12/18" para o layout de 1 conta | O mockup não tem colchetes na barra (`bar()` em `mockup-home.dc.html:154` só gera `█`/`░`), então os blocos apagados usam `C_UNLIT`. O gap do layout de 1 conta no mockup é 16 px (`grid-template-columns: 150px … gap: 16px`), e foi esse o valor usado. A cor do rodapé no mockup (`#cbd5e1`) não está na paleta; o plano usa `C_TEXT`. **Nota da rodada 2:** a parte "colchetes" deste CONTEST estava errada. `bar()` só gera os blocos, mas o template envolve cada barra com `[` e `]` em `#8a98a8` (`mockup-home.dc.html:45, 50, 68, 72, 91, 109`). Os colchetes foram restaurados na rodada 2 (M10); as partes do gap e do rodapé continuam valendo |

### Não resolvido

Nenhum. Uma ressalva: o tempo de DNS do pior caso (~21 s) é uma estimativa feita a partir das constantes do lwIP, porque o `dns.c` não vem em fonte no pacote do core. O E2E 2 mede a duração real dos ciclos.

## Etapa 9 — rodada 2

Fonte: `implementation_readiness_report_r2.md` (2 🔴, 17 🟡, 34 🟢). Modo: execução autônoma ("auto accept"). Esta é a última passada de resolução, sem nova auditoria. A versão anterior do documento está no scratchpad da sessão (`design_before_stage9_r2.md`).

A numeração dos itens 1–47 não mudou. As mudanças de comportamento foram levadas também à spec (G4, §1, §3, §4, §5, §8 e Risks). Os símbolos e as linhas citadas foram conferidos no código (`claude_stick.ino`, `accounts.cpp`, `crypto.cpp`, `api.{h,cpp}`, `status.cpp`, `wifi_manager.h`, `config.h`, `tools/gen_banners.py`), no core `esp32:esp32` 3.3.8 (`HTTPClient.h:47-57, 238`) e no LVGL 9.2.2 (`lv_symbol_def.h:80, 152`, `lv_span.h:58, 65`, `lv_conf_internal.h:2135`). No fim, o **Status** do documento passou a `Approved`.

### Resumo

| Categoria | Quantidade |
|---|---|
| PROPOSE FIX (aplicado) | 2 🔴 + 17 🟡 + 34 🟢 = 53 |
| CONTEST | 0 |
| DECLINE | 0 |
| ESCALATE | 0 |

Correção de um registro anterior: o CONTEST de M24 da rodada 1 (parte "colchetes") estava errado. A tabela da rodada 1 ganhou uma nota, e os colchetes voltaram à spec (§5) e ao plano (itens 36, 37 e 43). A linha citada no CONTEST de M26 também ganhou nota (2393-2394, ver `cd-2-8`).

### PROPOSE FIX — bloqueadores e importantes

| Achado | Onde | Correção |
|---|---|---|
| B1 (`ir-2-1`) | §3, itens 3, 6, 10, 19, 23, 36, 39 | Saída 1: `used` e `label` saem do `AccountState`; a fonte única é `g_accts.used`/`g_accts.label`, que `accountSave`, `accountSetLabel` e `accountRemove` já mantêm. `acct[slot].keyOk = true` no passo 8 do item 6 e no passo 4 do `ST_SETUP_PIN`. O pseudo-código do agendador lê `g_accts.used[slot]` e `acct[slot].keyOk` |
| B2 (`ir-2-2`) | itens 6 e 18 | O filtro de `gen` vale só para `USAGE` (`r.gen != acct[r.slot].gen`). `STATUS` e `RECONNECT` levam `slot = -1`, `gen = 0` e nunca indexam `acct[]`. O `VALIDATE` tem filtro próprio: `gen = web_token_seq()`, e o resultado é ignorado se o `seq` mudou ou se veio `FC_DISCARDED` |
| M1 (`ir-2-3`) | itens 2, 6, 19, 24 | Flag `started` no agendador: `scheduler_tick()` sai na hora antes de `scheduler_start()`. `poller_apply_results()` e `poller_check_stuck()` rodam em todos os estados, inclusive no onboarding |
| M2 (`ir-2-4`, `cd-2-11`, `st-2-11`) | itens 6, 24, 25 | O servidor do onboarding vive de `ST_TOKEN` até o desbloqueio; `stop_web()` sai de `render_state` (:2622) e de `ui_token` (:838). `web_token_begin()`/`web_token_reset()`/`web_token_seq()`/`web_token_set_result()` declaradas em `web_api.h`. Fechar a tela com validação em andamento zera o token, faz `seq++` e volta a `idle`; `ok`/`rejected` ficam legíveis. O JS trata `idle` depois do envio como "O aparelho não respondeu" |
| M3 (`ir-2-5`, `st-2-4`, `cd-2-5`, `cd-2-14`) | item 6 | `slot = g_tokenTargetSlot` sem "(só para o log)"; `replacing = g_accts.used[slot]` antes de gravar; a conta ganha o foco (`accountSetActive`, como ino:1115-1116); o `UsageData` do `VALIDATE` não entra no modelo nem no histórico (CUJ 4); falha de cifra → `rejected`/`crypto`; `g_pendingLabel[0] = 0`; navegação para `ST_MAIN`/`intent_open(HOME)`; POST com a tela fechada → 403 `closed`. Tabela de rotas dos dois servidores (`/` → `handleRoot`, `/info` → `handleInfo`, 404 JSON na sessão, 302 no onboarding) |
| M4 (`ir-2-6`) | item 3, item 23, E2E 7 | `acct_clear` preserva o `gen`. O E2E 7 ganhou "remover e adicionar no mesmo slot durante um ciclo" |
| M5 (`ir-2-7`) | itens 19, 24 | Tabela no item 24 com o destino das 7 chamadas a `ST_LOADING` (:466, :477, :594, :907, :1098/:2380, :2220, :2406). `ST_ERROR` também sai, porque a única entrada é `do_refresh` (ino:2702); com ele saem `ui_error`, `ui_message` e `ui_loading`. `scheduler_apply_tz()` entrou na API do item 19 (troca de fuso, :2268) |
| M6 (`cd-2-1`, `ir-2-8`) | itens 3, 18, 19, 24, 25, 40 | `PollerHooks {onStatus, onValidate, onReconnected}` e `scheduler_set_moment_hook`, registrados pelo `.ino` na fase 2 e pelo `app_state` na fase 3. `ThrHit` e `check_thresholds(AccountState&)` vão para `account_state` já na fase 1. `moment_flush()` em `scheduler.h`, chamado no fim do ciclo, ao entrar em `ST_MAIN` e nos dois fechamentos de momento (:1727 e :1879); o bloco `g_pendWin` do `loop()` (:2898-2901) sai na fase 2. `poller_apply_results()` devolve `bool`, e quem marca a interface como suja é o `loop()`/`app_state_tick(bool)` |
| M7 (`ir-2-9`) | regra de dependências, itens 10, 11, 13 | Saída por parâmetro: `dot_state(..., uint32_t freshSec)` e `probe_needs_retry(bool, const char* model, const char* probeModel)`. A regra de `src/logic` diz que nenhuma função lê o `config.h` |
| M8 (`ir-2-10`) | G4, §4, itens 12, 13, 24, 32, 36, E2E 6 | Precedência do card: `SEM CHAVE` → falha sem dado (`⚠ <classe>`, `--%`) → falha com dado (`⚠ <classe> · <idade>`) → `pendente`. `age_text()` em `levels`, com `s`/`m`/`h` e testes. O E2E 6 passou a revogar um token já salvo, porque um token inválido colado pela página nunca é salvo |
| M9 (`ir-2-11`, `st-2-2`, `cd-2-9`, `cd-2-10`) | item 18 | Execução por `JobType`. `STATUS`: `cls = ok ? FC_OK : FC_NET`, `http = 0`. `RECONNECT`: `connectList` com `netIdx`. `NetResult` ganhou `ms` e `netIdx`; `probeHttp`/`probeMs` são sempre preenchidos com `modelIdx >= 0`, e a tabela usa só eles |
| M10 (`ir-2-12`) — REABERTO | §5, itens 36, 37, 43 | Saída 1: `[` `]` em `C_DIM` em volta de toda barra, desenhados com um `lv_spangroup` de 4 spans; a checagem do `gen_mockups.py` mede a barra com os 2 colchetes. Ver a nota no CONTEST de M24 da rodada 1 |
| M11 (`st-2-1`, `ir-2-13`, `cd-2-16`) | item 37 | `maxChars` do layout de 1 conta: 41 (272 ÷ 6,6) |
| M12 (`st-2-3`) | itens 11, 20 | `fetchUsage` devolve código negativo em erro de transporte (`HTTPC_ERROR_*`, `HTTPClient.h:47-57`; falha de `begin` → −1) e nunca 0, que fica reservado para "nunca sondado" |
| M13 (`cd-2-2`) | itens 41, 46 | `assets/brand/claudecode-color.svg` fica, porque `gen_banners.py:53` o usa. Os banners anunciam o gravador web, com a paleta do site, e não mudam (Non-Goals). Saem só `claudecode-text.svg` e `clawd-dead.png` |
| M14 (`cd-2-3`) | itens 12, 24, 36, 39, §5 | Destino de cada função: `tlabel`, `tstatic`, `mkchip`, `set_chip` → `widgets.cpp`; `update_tok_row` → `tok_text`; `refresh_ui_values` → `screen_home_refresh`; `load_persisted`, `save_attempts`, `apply_brightness` → `app_state.cpp`; `tile_setup`, `set_meter`, `build_win_card`, `set_ag_proj`, `status_color`/`overall_label` saem; a `refBar` sai na fase 2. `status_mark()` reescreve `overall_label`/`status_color`, com `rate_limited`/`exceeded` → `limite` e valor desconhecido → o próprio valor em 8 caracteres, cinza (spec §5 atualizada) |
| M15 (`cd-2-4`, `ir-2-18`) | itens 2, 6 | `g_tokMsg` (:651), `anim_opa_cb` (:665) e `build_claude_mark` (:668-680) ficam no `.ino`. As globais do PIN (:111-113) não viram `extern`: o `.ino` registra `web_on_token_ok()`. O item 2 lista as globais do `.ino` que perdem o `static` na fase 0 (`g_accts`, `g_tzOffset`, `g_pendingToken`, `g_pendingLabel`) |
| M16 (`cd-2-6`) | §8, item 6 | Sem `account` → conta em foco; o 409 `account_mismatch` (ino:798-805) sai; comparação com `strcmp` |
| M17 (`st-2-22`) | item 14, item 6 "Fase 1" | `web_api.cpp` e `history.cpp` entram no título do item 14. Os handlers passam a ler e gravar `acct[g_accts.active]`; o `UsageData` validado vai para `g_validatedUsage` até a fase 2 |

### PROPOSE FIX — menores (todos aplicados)

| Achado | Correção |
|---|---|
| `st-2-5` (REABERTO) | "50% em 9 blocos (5 acesos)" no item 13 |
| `ir-2-14` | Barra da 5h em `jbmono_13` nos layouts 1–2 e `jbmono_11` nos 3–4; barra da semana em `jbmono_11` (item 37) |
| `ir-2-15` | `lastH5` e `dayN` na tabela da §3 |
| `ir-2-16` | Donos: `g_sessionKey`/`session_unlocked()` (item 23, depois `app_state`), `web_token_set_result` (item 6), `incident_sync` (hooks do item 18, item 40), `refresh_ui_values` (item 36), `g_models[]`/`g_status` (item 3, `models_get`/`status_get` na fase 3), `fetch_class_name` (item 4) |
| `ir-2-17` | Build local temporário com `heap_caps_get_free_size` no `dash_tick` (item 1) |
| `ir-2-19` | Desbloqueio: `g_pinAttempts = 0`, `g_lockoutUntil = 0`, `save_attempts()`; checagem `blob.len > 199`, que conta como slot que não abriu. `ST_SETUP_PIN`: `g_onboarding = false`, `memset(g_pinFirst)`, caminho de falha da cifra |
| `ir-2-20` | Remover mantém a guarda `accountCount <= 1` (:2384) e recria a tela inicial com o novo layout |
| `ir-2-21` | Bordas de célula arredondadas na escala 8×; pássaro de 13 px com a sobra de 1 px à direita e embaixo |
| `ir-2-22` | `/modelos`: `incidente <gravidade> · <modelos>`, com `severity_label`/`affected_models`; texto no item 32 |
| `ir-2-23` | `↻` = `LV_SYMBOL_REFRESH` (U+F021, `lv_symbol_def.h:80`) pela Montserrat 14 do `fallback` (itens 34 e 35) |
| `ir-2-24` | `/janela` usa `projection_text` como legenda; a linha de projeção só em `ESGOTA` e em `OK_ATE_RESET` com `rate > 0.02` (campo `rate` novo em `ProjResult`) |
| `ir-2-25` | Hook `onReconnected` → `ensure_mdns()` |
| `cd-2-7` | `crypto.cpp:60` |
| `cd-2-8` | ino:2393-2394 (item 5 e nota no CONTEST de M26) |
| `cd-2-12` | `load_history()` do `setup()` (:2783) vira `hist_load` da conta ativa na fase 1 e sai na fase 2 |
| `cd-2-13` | `HIST_MAX`/`Sample` (:140-141) e `NDAYS`/`DayHeat` (:149-150) listados no item 5 |
| `cd-2-15` | Item 20: a cópia em `String` do `addHeader` (`HTTPClient.h:238`) é o risco residual; spec §1 reescrita |
| `cd-2-17` | Dependências de ferramenta no item 47 (Pillow, `fontTools`, Node/`npx`, `g++`, caminho do `xtensa-esp32s3-elf-gcc` com os `-I`) |
| `cd-2-18` | Intervalos: momento 1684-1880, incidente 1882-2042 (globais 1887-1890), `screen_pin` 375-553, `screen_settings` a partir de 2208, `screen_accounts` com 2215 |
| `st-2-6` | "token em validação" no lugar de "token pendente" |
| `st-2-7` | Resolvido de outro jeito: `ST_ERROR` saiu (M5), e o item 25 diz "Fora do shell, só antes do desbloqueio; depois, WiFi e token são conteúdos do shell" |
| `st-2-8` | "Card em falha" e "Card pendente" |
| `st-2-9` | "(estado `falhou` da §4; `failed` no item 10)" |
| `st-2-10` | "tela de token (`ST_TOKEN`; na fase 3, o conteúdo `TOKEN`)" |
| `st-2-12` | "itens 46 e 47" |
| `st-2-13` | "A detecção de `NOCLOCK` (prazo de 60 s)…" |
| `st-2-14` | Log `[slot N] <tipo> http=%d cls=%s ms=%u` |
| `st-2-15` | Campo `int8_t netIdx` |
| `st-2-16` | Ramo `decryptToken(blob, pin)` removido: todo blob usa o MAC (`crypto.cpp:34`) |
| `st-2-17` | "As regras definem o resultado; o `gen_mockups.py` é a conferência visual" |
| `st-2-18` | "(estimado a partir das constantes do lwIP; o E2E 2 mede)" |
| `st-2-19` | "7 × 36 + 8 + 2 = 262 px" na §5 e no item 35 |
| `st-2-20` | "rotaciona e envia" em Risks e no item 34 |
| `st-2-21` | Tabela do item 37 recuada |

### Conferências feitas na resolução (além dos achados)

| Ponto | Correção |
|---|---|
| `moment_close()` é chamada por `render_state()` (:2623) e por `show_moment()` (:1730) | `moment_flush()` não fica dentro de `moment_close()`, e sim nos dois fechamentos de fim de momento (:1727, :1879) e no fim do `render_state()` de `ST_MAIN`; senão o momento abriria no meio da limpeza da tela |
| As Montserrat da fase 2 não têm `⚠`, `…` nem acentos | O card da fase 2 usa `LV_SYMBOL_WARNING` (`lv_symbol_def.h:152`) e reticências ASCII |
| `g_lastFetchOk`, `g_lastOkMs`, `g_refreshing`, `g_lastPollMs`, `g_wantRefresh` (ino:121-131) só fazem sentido com `bg_refresh` | Saem na fase 2 (item 24), com a `refBar` e o poll do `loop()`; `set_hdr_status` mostra `ok`/`K com falha` até a fase 3 |
| `g_pendingToken` (:102) e `g_tokenTargetSlot` (:109) | `g_pendingToken` sai na fase 2 (área de validação do `token_store`); `g_tokenTargetSlot` perde o `static` na fase 2 |

### Não resolvido

Nenhum achado ficou aberto. Duas ressalvas, as mesmas da rodada anterior: o tempo de DNS do pior caso (~21 s) continua estimado, e o E2E 2 mede a duração real; as flags `--lv-fallback`/`--lv-include` do `lv_font_conv` 1.5.3 não foram conferidas de novo nesta rodada, porque a ferramenta não está instalada.
