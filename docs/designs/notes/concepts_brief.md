# THEP Usage Stick v3 — nova UI no estilo Hermes, com várias contas

* **Author:** caio
* **Date:** 2026-11-08

## Overview

O Claude Usage Stick (ESP32-S3, LVGL 9.2, tela touch de 480×320) mostra o rate limit do Claude Code. Hoje ele monitora **uma conta por vez**. O dono usa **duas contas corporativas ao mesmo tempo** e perde tempo trocando entre elas na tela de contas.

A proposta é refazer a UI e ajustar a parte de dados:

- **Várias contas na tela inicial.** Todas as contas conectadas (1 a 4) aparecem juntas, e o layout muda pela quantidade:
  - 1 conta: pássaro em braille à esquerda e seções à direita;
  - 2 contas: duas colunas;
  - 3 contas: três colunas;
  - 4 contas: grade 2×2.
- **O que cada conta mostra:** a % da janela de 5h em destaque, uma barra de texto `[████░░]`, o reset da 5h, a % e o reset da semana, e a projeção resumida (`✓ não esgota · ~78%` / `✗ esgota 15:20 · 38m`).
- **Identidade visual do Hermes Agent, só com cores da THEP:**
  - logo "THEP" em figlet *ANSI Shadow* com 3 faixas (`#2fb5dd` / `#2f8fd6` / `#1272c2`);
  - pássaro da THEP em pontos braille;
  - painel com borda arredondada e título embutido na borda;
  - seções em negrito na cor de destaque e `chave:` apagado seguido do valor claro;
  - barra de status no rodapé com separadores `│`;
  - fonte monoespaçada.
- **Navegação.** O botão `❯ menu` no cabeçalho abre um menu de comandos (`/agora`, `/modelos`, `/janela`, `/ritmo`, `/contas`, `/ajustes`) e substitui o swipe. O swipe e o slideshow saem.
- **Ponto de status pulsante** no lugar de "atualizado há XXs":
  - vermelho: todas as contas falharam na última consulta;
  - amarelo: alguma conta falhou;
  - azul: todas atualizadas há menos de 10 s;
  - verde: ok;
  - prioridade: vermelho > amarelo > azul > verde.
- **Dados.** Todas as contas conectadas são consultadas no intervalo definido pelo dono (30 s a 5 min), com o HTTPS fora do loop da interface.

**Referência visual aprovada:** canvas "THEP Usage Stick — Estilos", linha "Estilo A · identidade Hermes, cores THEP". Tem os mockups de 1, 2, 3 e 4 contas e o menu aberto: https://claude.ai/artifact/JeUby4eTTN3HA5PsYaaimR. Os assets (logo ANSI Shadow e pássaro em braille) são gerados a partir do logo da THEP pelos scripts `hermesart.py` e `dotbird.py`, que hoje estão no scratchpad e precisam ir para `tools/`.

**Paleta THEP** (design system THEP, tema escuro; antes estava listada como lacuna):

| Papel | Token THEP | Valor |
|---|---|---|
| Fundo | `color-bg` (dark) | `#0c1222` |
| Fundo da barra de status | `ac-space-950` | `#070b18` |
| Superfície (menu) | `color-surface` (dark) | `#141e33` |
| Item selecionado | `color-surface-muted` (dark) | `#1a2740` |
| Borda do painel / 3ª faixa do logo | `thep-blue` | `#1272c2` |
| Título / 1ª faixa do logo | `thep-cyan` | `#2fb5dd` |
| Destaque de seção / 2ª faixa / ponto azul | `color-primary` (dark) | `#2f8fd6` |
| Texto | `color-text` (dark) | `#e2e8f0` |
| Texto apagado | `thep-neutral-400` | `#8a98a8` |
| Barra apagada / separador | `ac-space-500` | `#34457a` |
| Nível ok / ponto verde | `thep-green` | `#5caf4e` (texto: `ac-acerto-ink` `#8ed57f`) |
| Nível aviso | `color-warning` (dark) | `#f0a32a` |
| Ponto amarelo | — (fora da paleta) | `#f0c419` |
| Nível ruim / ponto vermelho | — (fora da paleta, pedido do dono) | `#f05252` (texto `#ff8a8a`) |

A THEP não tem vermelho. O "perigo" da marca é magenta/rosa (`#97035a` / `#f27fa2`). O dono pediu vermelho explicitamente, então o vermelho e o amarelo ficam como exceções declaradas à paleta.

## Analysis

Sete revisores Goldfish, em contexto limpo, analisaram o conceito: design, codebase-details, zoom-out, comprehension, implementation-readiness, structural e inventive-catalyst. Todos concluíram.

**Resumo:** o conceito é viável, mas a parte difícil não é o visual. O firmware inteiro parte de duas premissas: existe **uma conta ativa**, e o **fetch bloqueia dentro do `loop()`**. Antes de desenhar layouts, é preciso definir:

1. o modelo de concorrência;
2. o modelo de dados por conta;
3. o que passa a significar "conta ativa";
4. o pipeline de fontes e glifos, que hoje **não tem nem os acentos do português**.

As referências `arquivo:linha` abaixo são de `firmware/claude_stick/claude_stick.ino`, salvo indicação em contrário.

### 🔴 Bloqueios

#### B1. Não existe modelo de concorrência, e o LVGL não é thread-safe
*Lentes: design, codebase, readiness, structural, inventive, zoom-out*

**Situação hoje**
- `lv_conf.h:43` usa `LV_USE_OS LV_OS_NONE`, então o LVGL não aceita chamadas de outra thread.
- `bg_refresh()` (`claude_stick.ino:2708-2730`) chama `set_hdr_status`, `lv_refr_now` e `refresh_ui_values` no mesmo lugar do fetch.
- `wait_for_time()` (2670-2681) gira `lv_task_handler`.
- `handleTokenPost()` (738-766) faz `fetchUsage` dentro do handler web e escreve direto em `g_usage` e nos labels.
- `g_wifi.autoConnect(8000)` bloqueia com `delay` (`wifi_manager.h:28-47`).

**O que definir**
- Uma única task de rede, fixada no core 0, com stack de 12-16 KB. Ela **nunca** chama `lv_*` e nunca escreve no modelo global.
- A task publica o resultado (`{slot, gen, UsageData, fetchedAt, ok, httpCode}`) numa fila ou num buffer duplo protegido por mutex.
- O `loop()` é o **único dono** do modelo, do LVGL, do LittleFS e do NVS. Ele aplica os resultados, chama `hist_push`, `check_thresholds` e `save_history`, e redesenha.
- As consultas são **seriais**, com no máximo uma sessão TLS por vez. Cada sessão mbedTLS ocupa uns 40-50 KB de heap interno.
- Toda a rede passa por essa task: usage das N contas, probe de modelo, status.claude.com, validação de token do onboarding e reconexão do WiFi.
- A task pausa em `ST_WIFI`, `ST_TOKEN` e `ST_PIN`.
- `api.cpp` serve para um worker único, porque não tem estado estático mutável e o cliente é local. Mas `fetchUsage` escreve parcialmente na struct de saída. Por isso o fetch tem de ir sempre para uma struct local, publicada depois.

#### B2. Corrida entre um fetch em andamento e uma conta removida ou trocada
*Lentes: design, readiness*

**Problema**
- `acct_del_cb` (2382-2411), `finalize_pending_token` (1102) e `factory_reset` (351-370) reaproveitam ou zeram slots enquanto um fetch pode estar em voo.
- Um resultado atrasado pode recriar `/histN.bin` ou cair na conta nova do mesmo slot.

**O que definir**
- Um contador `gen` por slot, incrementado a cada inclusão, remoção ou troca de token.
- A task copia token e `gen` no início do job. O loop descarta resultado com `gen` velho.
- Reset de fábrica e remoção de conta param ou esvaziam a task antes de apagar.

#### B3. Todos os dados de conta são globais únicos
*Todas as lentes*

**Situação hoje**
- `g_usage` (77), `g_token` (101), `g_lastFetchOk`/`g_lastOkMs` (123-124) e `g_tok` (93) existem uma vez só.
- O mesmo vale para `g_hist`/`g_histN`/`g_histHead`/`g_hourBurn`/`g_lastH5`/`g_days` (140-152) e para `g_thrFired`/`g_thrPrev`/`g_thrBase`/`g_pendWin` (1697-1700).
- `hist_path()` (1002-1004) está amarrado a `g_accts.active`.
- `switch_account()` (1080-1100) descarrega uma conta e carrega outra.

**A projeção depende disso.** Mostrar "esgota 15:20" em cada conta exige o **histórico de todas as contas em RAM**, e hoje só o da ativa é carregado.

**O que definir**
- Um `AccountState[ACCT_MAX]` como fonte única de verdade. Cada registro guarda:
  - slot, rótulo e `gen`;
  - o último `UsageData` e o horário dele;
  - o resultado da última tentativa e a classe do erro;
  - `lastOkEpoch` e o número de falhas seguidas;
  - o anel de histórico e o heatmap;
  - o estado de limiar e o `TokenStats`.
- Isso custa uns 4,5-5,2 KB por conta, cerca de 20 KB para 4. Dá para colocar em PSRAM.
- Extrair a matemática de projeção de `trend_redraw()` (1542-1660, taxa em 1596-1609) para uma função pura `(estado da conta) → {estado, eta, minutos, pctFinal}`. A home e o `/janela` usam a mesma função. **Não escrever uma segunda projeção.**
- Parar de usar os buffers `static HistFileV2` de `save_history`/`load_history` (1017, 1035) fora do loop.

#### B4. Ninguém definiu o que vira a "conta ativa"
*Todas as lentes*

**Onde ela ainda é usada**
- `/window` (784-794) e `/tokens`, que devolve 409 para outra conta (798-805, `token_bridge.py:87-111`).
- O probe de modelos, que usa `g_token` (2684-2689).
- Os momentos de limiar.
- `/janela`, `/ritmo` e `/modelos`.
- A tela de contas ("toque para trocar").
- O texto em 2553: "Só a conta ativa é consultada…".

**Proposta convergente:** trocar "ativa" por **"conta em foco"**. É só um ponteiro de visualização e não tem efeito de rede.
- Para escolher a conta em foco, o usuário toca no card dela na home. As telas de detalhe também têm um seletor. Isso transforma a troca de conta que sobra em um toque.
- O foco continua salvo na chave NVS `acct`, o que mantém compatibilidade com a v2.3 num rollback.
- **Telas por conta:** `/modelos`, `/janela` e `/ritmo`. Elas sempre mostram o rótulo da conta no cabeçalho.
- **Telas globais:** incidentes.
- O probe de modelo roda só na conta em foco, e só enquanto `/modelos` está aberto ou uma vez por ciclo.

#### B5. As fontes não têm os glifos que o visual exige
*Todas as lentes*

**Situação hoje**
- Só existe Montserrat 12-48 (`lv_conf.h:54-64`), que é proporcional.
- Faltam braille (U+2800-28FF), box drawing (`│ ═ ║ ╔ ╗ ╭ ╮`), blocos (`█ ░`), `❯ ✓ ✗ ↻ ⏲` e `·`.
- **Faltam também os acentos do português.** "não" não aparece na tela. O código atual evita acentos de propósito ("NAO esgota").

**O que definir**
- O **logo THEP em figlet e o pássaro em braille como imagens prontas**, gerados por script como em `tools/gen_logo_assets.py` e `logo_assets.h`. Assim as 3 faixas de cor saem de graça e a altura fica fixa.
- **Uma fonte monoespaçada recortada** com `lv_font_conv`, num script versionado (`tools/gen_fonts.sh`). Os intervalos:
  - 0x20-0x7E e 0xA0-0xFF;
  - U+2500-257F e U+2580-259F;
  - U+276F, U+2713, U+2717, U+21BB, U+23F2 e U+00B7.
- Uma fonte grande só com dígitos e `%`.
- Ligar `lv_font_t.fallback` para a Montserrat, ou embutir o FontAwesome. Sem isso, todos os `LV_SYMBOL_*` atuais viram quadrados.
- Conferir glifo por glifo. `⏲` e `❯` faltam em muitas fontes mono.
- Atalho possível: usar `LV_SYMBOL_REFRESH`, `LV_SYMBOL_OK` e `LV_SYMBOL_CLOSE` no lugar de `↻ ✓ ✗`.
- Registrar a licença da fonte e o custo em flash (dezenas de KB por tamanho).

#### B6. O polling e o servidor web só funcionam na tela principal
*Lentes: codebase, readiness, design*

**Problema**
- O auto-poll só roda quando `g_state == ST_MAIN` (2823-2827).
- `render_state()` (2620-2625) chama `stop_web()` e limpa `lv_layer_top` a cada troca de estado.
- Se `/modelos`, `/janela` e `/ritmo` virarem estados próprios, os dados envelhecem e o bridge sai do ar enquanto essas telas estão abertas.

**O que definir**
- Um **shell persistente**: cabeçalho, ponto de status e rodapé são criados uma vez, e só o container de conteúdo é trocado.
- O polling fica ligado a "sessão desbloqueada + WiFi", não à tela.
- O WebServer e o mDNS seguem a mesma regra.

---

### 🟡 Importantes

#### Escopo e faseamento

**I1. Fazer tudo de uma vez torna os bugs impossíveis de rastrear.** *(zoom-out, structural, readiness)*
- Não há testes automatizados no aparelho.
- Se a task nova, o modelo multi-conta e uma UI de 2900 linhas entrarem juntos, um travamento pode vir de corrida, de LVGL fora da thread ou do layout, e não dá para saber qual.

Sugestão de fases, cada uma compilável e gravável:

| Fase | Conteúdo | Resolve a dor do dono? |
|---|---|---|
| 0 | Linha de base: tag `v2.3`, `.bin` arquivado, tamanho do sketch e heap anotados. Divisão do `.ino` em módulos **sem mudança visual** | Não |
| 1 | Lógica pura com testes no host (projeção, nível, barra, ponto, layout). Modelo `AccountState[]` | Não |
| 2 | Task de rede com N contas, contrato novo do bridge e home de 2 colunas no estilo atual | **Sim** |
| 3 | Pipeline de fontes e paleta, menu no lugar do tileview, slideshow removido | Não (identidade) |
| 4 | Layouts 1, 3 e 4, restyle das outras telas, teste de longa duração, ensaio de rollback, README | Não |

O zoom-out propôs uma alternativa ainda menor para a fase 2: **round-robin no loop**, uma conta por tick, espaçadas de `intervalo/N`, sem task. Cada travada continua em ~1-2 s, como hoje. Ela fica registrada como opção de custo, porque o dono já decidiu tirar o HTTPS do loop.

**I2. Dividir o `.ino` antes do trabalho visual.** *(structural, design)*
- Módulos sugeridos:
  - `theme`, `widgets`, `model`, `history`, `poller`, `app_state`, `web_api` e `overlays`;
  - uma tela por arquivo.
- O arduino-cli só compila a raiz do sketch e `src/`, então as subpastas têm de ficar dentro de `src/`.
- As telas só leem o modelo e mandam "intenções". Nada de `extern` de global mutável entre módulos.

#### Rede, agendamento e cota

**I3. Um ciclo completo pode passar do intervalo mínimo.** *(design, codebase, readiness)*
- Cada fetch faz um handshake TLS novo, de 1-2 s.
- O pior caso é 4 × 15 s (`API_TIMEOUT_MS`, `config.h:48`), mais o probe e o status: **75-90 s, contra um mínimo de 30 s**.

**O que definir**
- A task é sequencial e não reentrante. O próximo ciclo começa em "fim do ciclo + intervalo".
- Timeout de cerca de 8 s por conta.
- Backoff exponencial para 401/403. Um token revogado não pode ser martelado a cada 30 s.
- "Atualizar agora" durante um ciclo é absorvido pelo ciclo em andamento, não enfileirado de novo.
- Opção: reaproveitar a conexão (`setReuse(true)`) e trocar só o `Authorization`.

**I4. Cada consulta gasta a própria cota que está medindo.** *(inventive, design)*
- Cada fetch é um `POST /v1/messages` de verdade (`max_tokens:1`, `api.cpp:45-47`). Com 4 contas a cada 30 s são umas 960 inferências por hora.
- O dono aceitou o tráfego, mas talvez não o consumo de cota.
- Há ainda o User-Agent fixo e o mesmo IP para todas as contas.

**Sugestões**
- Status.claude.com **uma vez, global**, a cada `STATUS_POLL_SEC`. Isso resolve a constante morta do drift report.
- Probes só na conta em foco.
- Polling adaptativo opcional: conta parada espaça até 5 min, conta acima de 80% volta ao mínimo, mais jitter.

**I5. Definir o que significa "falhou" e "conectada".** *(comprehension, codebase, inventive)*
- Para cada caso, dizer se conta como falha:
  - 200 com headers;
  - **429 com headers**, que é dado e não falha, e o parser já aceita;
  - 429 sem headers;
  - 401/403;
  - 5xx;
  - timeout ou DNS;
  - sem WiFi.
- Se 429 contar como falha, uma conta esgotada pinta o ponto de amarelo ou vermelho, e o usuário confunde "acabou a cota" com "sem rede".
- Só o fetch de usage deve mexer no ponto. Status e probe não.
- **"Conectada"** = slot salvo que decifrou no desbloqueio. Uma conta com falha **mantém a coluna** e mostra estado de erro. Assim o layout depende só do número de slots e não muda a cada queda de rede.
- Verificar o WiFi primeiro, para uma queda aparecer como um estado só, e não como N falhas.

#### Segurança e bridge

**I6. N tokens decifrados em RAM.** *(todas)*
- Hoje só um token fica em claro (`g_token[200]`).
- `deriveKey` faz 10 mil rodadas e o salt é o MAC, então a chave é **a mesma** para todos os slots (`crypto.cpp:20-34`).

**O que definir**
- Derivar a chave uma vez no `pin_submit` (471) e decifrar todos os slots em `g_tokens[ACCT_MAX][200]`.
- Um slot que não decifra aparece como "bloqueada" na coluna, sem derrubar o desbloqueio.
- Zerar os tokens no bloqueio, na remoção de conta e no reset de fábrica.
- Atualizar a seção de segurança do README.
- **Não existe refresh de OAuth.** São tokens `sk-ant-oat01` fixos. Para 401, definir só a apresentação e o caminho para colar um token novo.

**I7. O contrato do bridge precisa mudar sem quebrar o antigo.** *(design, codebase, readiness)*
- **`/window`:**
  - mantém os campos atuais, que passam a ser os da conta em foco;
  - ganha um array `accounts`;
  - ganha o parâmetro `?account=<rótulo>`.
- **`/tokens`:** é roteado pelo rótulo para o slot. Rótulo desconhecido devolve 404 em vez de 409.
- **Bridge:** um processo por conta.
- **Ponto de produto importante:** duas contas corporativas no mesmo PC dividem `~/.claude/projects`. O bridge não consegue separar os tokens por conta sem um `CLAUDE_CONFIG_DIR` diferente para cada uma.
- Decidir se a linha "tokens na janela" continua existindo. A home nova não a menciona.

#### Visual e layout

**I8. O espaço do 2x2 provavelmente não comporta tudo.** *(todas)*
- **Altura disponível:**
  - Com o figlet de 6 linhas (~40-60 px), o rodapé (~18 px) e a borda do painel, cada célula fica com uns **229×110-125 px**.
  - Empilhar rótulo, % grande, barra, reset 5h, linha 7d e projeção dá ~110 px **sem nenhum espaçamento**.
- **Largura:**
  - Em 3 colunas, cada célula tem ~150-160 px.
  - Um rótulo de 16 caracteres (`ACCT_LBL_MAX` 17) precisa de corte com reticências.

**O que definir**
- Uma tabela por layout (1/2/3/4) × campo, com fonte, visibilidade, máximo de caracteres e largura da barra em células.
- Uma ordem de corte. Exemplo de 2x2 em 3 linhas:
  - `73% [████░░] ↺14:20`
  - `7d 42% ↺sex`
  - `✗ 15:20·38m`
- Cabeçalho que se adapta: logo cheio com 1-2 contas, versão compacta (~20-24 px) com 3-4.
- Rótulo da conta como título embutido na borda do painel. Ordem pelo índice do slot, estável. As contas se distinguem pelo texto, não pela cor.
- **Validar os 4 layouts no `gen_mockups.py` antes do código LVGL**, usando as mesmas fontes do firmware. Hoje ele usa HelveticaNeue (`gen_mockups.py:26`), que esconde glifos ausentes.

**I9. Falta uma tabela de tokens da paleta THEP.** *(todas)*
> **Atualização:** a paleta foi lida do design system THEP e está na tabela do Overview. Verde e âmbar existem. Vermelho e amarelo são exceções pedidas pelo dono.
- **Papéis a mapear:** fundo, superfície, borda, texto, texto apagado, nível ok/aviso/ruim, os 4 estados do ponto, severidade de incidente, as faixas do logo e o Clawd.
- **O que muda no código:**
  - O código passa a referenciar só papéis, com `lv_style_t` compartilhado em `theme.cpp`.
  - As cores fixas somem de 40-53 e do CSS web (684, 699-703), que ainda usam o coral Claude.
- **Os limiares mudam.** Hoje são `pct_color` 70/90 (263-267) e um gradiente contínuo `grad_color` (269-274). O conceito pede degraus em 50/80. Definir os limites exatos (50 é verde ou âmbar?) e se o 7d usa as mesmas cores.
- **Se a THEP não tiver verde, âmbar e vermelho**, o nível vai para forma: um glifo ✓ ! ✗ ao lado do número, o preenchimento da barra ou a luminância dos azuis.

**I10. O ponto de status precisa de uma tabela de verdade.** *(todas)*
- **Estados que faltam:**
  - boot, antes da primeira consulta;
  - ciclo em andamento, que substitui o "atualizando…";
  - WiFi caído;
  - dado velho sem nenhuma tentativa, com a task travada.
- **O azul "todas há <10 s" é ambíguo:**
  - Com intervalo de 30 s, ele fica aceso um terço do tempo e vira ruído.
  - É a cor da marca, então pode ser lido como enfeite.
  - Com consultas escalonadas, pode nunca acender.
- **Proposta:**
  - O ponto é uma função pura `dot_state(contas[], agora, wifi)`.
  - Uma conta conta como falha se `agora - lastOk > 2 × intervalo`, mesmo sem tentativa.
  - O azul é medido a partir do fim do ciclo.
  - Watchdog que reinicia a task se nada for publicado em 3 × intervalo.
  - O zoom-out sugere 3 cores, com pulso só durante a consulta. Fica como opção de simplificação, já que o dono definiu 4.

**I11. O card com falha não tem aparência definida.** *(comprehension, codebase, inventive)*
- O amarelo não diz *qual* conta falhou.
- O "atualizado há" saiu do cabeçalho, então números velhos sem idade enganam.
- **Proposta:** os valores antigos ficam esmaecidos, com uma etiqueta curta (`AUTH`, `REDE`, `429`, `ERRO 5xx`) e a idade (`⚠ 4m`). O detalhe completo fica em `/contas`.

**I12. Navegação, overlays e refresh manual.** *(comprehension, structural, design)*
- **`/agora` × home:** a home nova já é o "agora". Decidir entre `/agora` = voltar à home ou `/agora` = detalhe de uma conta.
- **Menu:** os revisores divergem.
  - O design propõe um modal no `lv_layer_top` sobre o shell persistente.
  - O structural propõe um estado `ST_MENU` de tela cheia.
  - O shell persistente (B6) favorece o modal.
- **Voltar:** sempre para o menu ou sempre para a home? Há volta automática à home depois de N segundos sem toque? O slideshow saiu, então nada mais volta sozinho.
- **Ordem de empilhamento dos overlays:** momento, incidente e menu. Decidir também se o incidente aparece por cima de `/janela`. Hoje `incident_sync` só roda em `ST_MAIN` (2031).
- **Refresh manual:** o botão do cabeçalho e a engrenagem somem. Opções: item no menu, toque longo no ponto ou ação em `/agora`.
- **Alvos de toque:** `❯ menu` com `lv_obj_set_ext_click_area` para ter pelo menos 44 px. Linhas do menu com 48 px ou mais na largura toda. O card inteiro é área de toque.

**I13. Momentos de limiar com várias contas.** *(design, comprehension, inventive)*
- **Problema:** hoje existe um único slot pendente (`g_pendWin`/`g_pendThr`, 1697). Quando várias contas cruzam um limiar no mesmo ciclo, só a última aparece, e sem rótulo.
- **O que definir:**
  - estado de limiar por conta;
  - uma fila `{slot, janela, limiar}`;
  - o rótulo no overlay;
  - no máximo um momento por ciclo, ou um aviso agrupado ("2 contas passaram de 80%").

**I14. O que acontece com o resto da home e das outras telas.** *(comprehension, readiness)*
- **Itens da home atual sem destino:** o chip `OK / ATENÇÃO / BLOQUEADO` (que vem de `unified-status: rejected`), a linha do bridge e as contagens grandes. Sem o chip, uma conta bloqueada parece igual a uma em 95%.
- **Telas a listar:** quais ganham o visual novo (PIN, WiFi, token, ajustes, contas, sobre, incidente, momentos) e o que acontece com os mascotes Clawd. Se saírem, `logo_assets.h` e `clawd_dead.h` liberam flash.
- **Projeção:** dar uma frase de fórmula para cada campo. No "~78%", o 78% é o uso previsto no reset? No "38m", é o tempo até esgotar? Definir os textos de fallback ("— sem histórico", "esgotado", relógio sem NTP) e deixar claro que só a janela 5h tem projeção.

#### Memória, render e plataforma

**I15. O pulso redesenha a tela inteira.** *(design, inventive, readiness)*
- `LV_DISPLAY_RENDER_MODE_FULL` (2760) mais a rotação pixel a pixel no flush (215-224) fazem qualquer animação contínua ocupar o core 1 e disputar a PSRAM com o TLS.
- **Proposta:** pulso em 3-4 degraus de opacidade por segundo, num `lv_timer` de 8-10 Hz. Medir o tempo de quadro. Fazer o mesmo com o balanço dos mascotes (2861). `anim_opa_cb` (665) pode ser reaproveitado.

**I16. O heap interno vai ficar apertado.** *(design, codebase, readiness)*
- O pool do LVGL (`LV_MEM_SIZE` 96 KB, `lv_conf.h:38`) disputa DRAM interna com o mbedTLS e com a stack da task.
- Estouro do pool é silencioso: os objetos simplesmente voltam NULL.
- **Proposta:**
  - Medir `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)`, o maior bloco livre e `lv_mem_monitor` com a home de 4 contas durante um handshake.
  - Se faltar, mover o pool e os históricos para PSRAM.
  - Ligar `LV_USE_LOG` em WARN durante o desenvolvimento.

**I17. Partições congeladas e tamanho do binário medido.** *(readiness)* O próprio readiness classificou este item como bloqueio. Aqui ele aparece como regra de proteção, porque é fácil de cumprir.
- `partitions.csv` fica fora do escopo. Redimensionar desloca o LittleFS e apaga histórico e tokens.
- Anotar o "Sketch uses…" da v2.3 e fixar um orçamento, por exemplo app abaixo de 2,8 MB num slot de 3 MB.

**I18. Rollback e compatibilidade de dados.** *(readiness)*
- Parar de ler a chave NVS `slide`, mas **não removê-la**.
- Manter `/hist{0..3}.bin` exatamente em HistFileV2 (998-1001), com `HIST_MAX` 160 e `NDAYS` 31. Coisas novas vão em arquivo separado.
- Chaves NVS novas com nomes novos, de até 15 caracteres.
- Documentar o rollback: gravar só o app, nunca `erase_flash`. Subir o `FW_VERSION`.

**I19. Testes sem hardware e critérios de aceitação.** *(readiness)*
- **Lógica pura** compilada com `g++` no host:
  - nível de cor, com limites 49,9/50/79,9/80;
  - barra de texto;
  - strings de projeção, com 99,5 → esgotado, menos de 3 amostras, sem NTP e `reset == 0`;
  - `dot_state`;
  - `layout_for_count`;
  - rodapé.
- **No hardware:**
  - compilar sem warnings novos;
  - gravar por cima de uma v2.3 com 2 contas reais;
  - teste de 1 hora a 30 s com 4 slots, registrando heap e high-water da stack;
  - tirar o WiFi, invalidar um token e remover uma conta no meio do ciclo;
  - regravar a v2.3 e conferir que os dados continuam.
- **Contas de teste:** o dono só tem 2. Repetir o mesmo token em vários slots multiplica as chamadas contra uma única conta. Isso precisa constar do plano.

**I20. Observabilidade.** *(codebase)*
- Os logs `[API]` e `[PROBE]` (`api.cpp:49-51, 85`, 118) não dizem a conta. Prefixar com `[slot N/rótulo]`, sem nunca logar o token.
- `GET /health` em JSON, sem tokens, com:
  - o estado de cada slot;
  - heap interno e PSRAM;
  - `lv_mem_monitor`;
  - high-water da stack da task;
  - duração do último ciclo comparada ao intervalo;
  - uptime.
- Logar início e fim de cada ciclo e contar os ciclos que estouram o intervalo.
- Documentar como extrair o coredump (a partição já existe, `partitions.csv:8`).

**I21. O rodapé é ambíguo.** *(comprehension, inventive, readiness)*
- "2/4 contas" pode ser "2 ok de 4" ou "2 salvas de 4 possíveis". Sugestão: `2 contas · 1 ✗`.
- "↻ 2min" pode ser o intervalo ou uma contagem regressiva. A `refBar` (2162-2171) sai, então dizer se a contagem some de propósito.
- "⏲ 14:32" pode ser o relógio ou a última atualização.

---

### 🟢 Detalhes

- **Sobras a apagar de uma vez:**
  - tileview: `NTILES`, `tv`/`tile[]`/`dots[]` (162-165), `g_curTile` (180), `on_tile_changed` (1484-1495) e 2173-2202;
  - slideshow: `g_slideSec`/`g_lastTouchMs`/`g_lastSlideMs` (134-137), o caso 8 dos ajustes (2277-2291), a linha 2348-2356, `g_slideLbl` e o bloco do loop (2890-2895);
  - cabeçalho: `set_hdr_status` e o badge `@label` (2145-2152);
  - o hotspot de demo com duplo toque (2109-2128) e `LV_USE_TILEVIEW`;
  - o rebuild por humor dos mascotes (2719-2728).
- **Escrita no flash:** `save_history` grava ~4,5 KB por conta a cada poll, umas 480 gravações por hora com 4 contas. Gravar só contas alteradas, a cada ~5 min ou no reset da janela.
- **Bilíngue:** todas as strings novas em `TRS(pt, en)` (57). Decidir se `/agora`, `/ritmo` e os demais são traduzidos no modo EN.
- **Reaproveitar sem mudança:** `fmt_eta`/`fmt_hm`/`fmt_clock` (289-312), `mklabel`/`mkbtn`/`card`, os overlays de momento e de incidente, e `build_tile_models`/`trend`/`heat` com outro pai.
- **Documentação:** README, README.pt-BR e mockups ainda descrevem swipe, slideshow e "atualizado há". Fechar os itens do drift report no mesmo ciclo (ordem dos ajustes, `STATUS_POLL_SEC`, cor do chip).
- **Referência visual:** linkar capturas do Hermes. Informar a fonte e a licença do pássaro em braille e o tamanho dele em células.

---

### Contradições e resoluções inventivas

| Contradição | Princípio | Resolução |
|---|---|---|
| Dados frescos de N contas × UI sempre responsiva | Extração | Task serial produz snapshots, a UI só lê. "A UI nunca espera a rede." |
| Frescor × cota consumida pela própria medição | Dinâmica, inversão | Intervalo do usuário vira teto. Conta parada espaça, conta perto do limite acelera. Probe e status sob demanda. 429 com headers conta como dado. |
| Mais informação por conta × menos espaço no 2x2 | Segmentação por prioridade | Layout como tabela de dados: cada quantidade de contas funde as linhas de menor prioridade. Cabeçalho compacto com 3-4 contas. |
| Estética de terminal × fontes disponíveis | Ação prévia | Logo e pássaro como bitmap. Uma única fonte mono recortada para o texto dinâmico. |
| Estética de terminal × alvo de toque | Extração | Área de toque separada do glifo (`ext_click_area` ≥ 44 px). |
| Ponto pulsante × CPU e significado | Dinâmica, amortecimento prévio | Só estados que pedem atenção pulsam, em degraus. O detalhe e a idade ficam no card. O ponto ocupa o lugar da `refBar`. |
| Níveis verde/âmbar/vermelho × paleta só THEP | Segmentação do canal | Se faltar cor semântica, o nível vai para forma (glifo, barra, luminância). |
| Um token em RAM × N contas | Ação prévia | Derivar a chave uma vez e decifrar todas no desbloqueio, com zeragem obrigatória no bloqueio. |
| Uma "ativa" no código × todas na tela | Assimetria | A "ativa" vira "em foco", sem efeito de rede, escolhida por toque no card. |

## Goals vs. Non-Goals

* **In-Scope Goals:**
  * A tela inicial mostra todas as contas conectadas (1–4), com layouts para 1, 2, 3 e 4 contas.
  * Todas as contas conectadas são consultadas a cada intervalo, por uma task de rede separada, sem travar a UI.
  * A identidade visual Hermes × THEP é aplicada à tela inicial e depois às demais telas: logo ANSI Shadow, pássaro em braille, painel com título na borda, barra de status e fonte monoespaçada com acentos.
  * O menu no cabeçalho substitui o tileview e o slideshow, e dá acesso a modelos, gráfico 5h, ritmo por hora, contas e ajustes.
  * O ponto de status pulsante, com 4 cores e prioridade, substitui "atualizado há XXs".
  * As projeções são resumidas e calculadas por uma função pura compartilhada entre a home e `/janela`.
  * Rollback seguro para a v2.3: mesmas partições, mesmos arquivos de histórico e chaves NVS antigas preservadas.
* **Out-of-Scope Non-Goals:**
  * Mudar `partitions.csv` ou o formato de `/hist{0..3}.bin`.
  * Mais de 4 contas.
  * Refresh de token OAuth: os tokens `sk-ant-oat01` continuam fixos, e uma conta com 401 só ganha apresentação e um caminho para colar um token novo.
  * Hospedar ou mudar o flasher web.
  * Novos tipos de dado da API além dos headers `unified-*` já lidos.
  * Tema claro.

## Alternatives Weighed

* **Estilo B · Painel** (cartões azul-marinho, Sora) e **Estilo C · Matriz LED** (números em Doto, barras de LEDs): o dono os descartou ao escolher o A e pediu para levá-lo ao máximo da identidade Hermes. O C também tinha brilho em tempo real, caro no ESP32.
* **Só a conta ativa consultada, as outras dormentes** (decisão da v2.3): rejeitada pelo dono. Ele usa as duas contas corporativas ao mesmo tempo e aceita o tráfego extra de API.
* **Navegação por swipe com pontinhos de paginação, mais slideshow**: rejeitada pelo dono, que quer um menu no cabeçalho.
* **Guardar só 2 contas**: rejeitada pelo dono. Ficam as 4, e a home se adapta à quantidade.
* **Round-robin no `loop()` sem task**, uma conta por tick (proposta do zoom-out): fica registrada como opção de menor custo para uma fase intermediária. A direção é tirar o HTTPS do loop.
* **Ponto de 3 cores com pulso só durante a consulta** (proposta do zoom-out): rejeitada. O dono definiu 4 cores.
* **Fontes inteiras (braille, box drawing) para desenhar o logo e o pássaro em tempo real**: rejeitada, por custo de flash e por falta de glifos. Logo e pássaro viram imagens prontas.
* **Cor de perigo da marca (magenta/rosa) para o nível ruim e o ponto de erro**: rejeitada pelo dono, que pediu vermelho.

## Key Decisions & Staging for Next Steps

**Já decididas pelo dono:**
1. Estilo A, no máximo da identidade Hermes, e somente com cores da THEP (com as exceções de vermelho e amarelo acima). Mockups aprovados.
2. De 1 a 4 contas na home, layout pela quantidade, e todas consultadas no intervalo definido.
3. Menu no cabeçalho no lugar do swipe. Todas as telas existentes continuam e passam a ser acessadas pelo menu.
4. Ponto de status: vermelho > amarelo > azul (menos de 10 s) > verde. "Erro" quer dizer que a última consulta da conta falhou.
5. Projeções resumidas: `✓ não esgota · ~78%` / `✗ esgota 15:20 · 38m`.

6. **Em 2026-11-08 o dono aprovou todas as propostas dos Goldfish abaixo.** O draft parte delas como direção e detalha cada uma: fases, task serial com `gen` por slot, conta em foco, taxonomia de falhas, fonte mono recortada, tabela de campos, contrato do bridge com `CLAUDE_CONFIG_DIR` por conta, agendamento com status global e probe na conta em foco, menu sobre um shell persistente.

**A detalhar no `/egm-draft`** (levantadas pelos Goldfish):
1. **Fases (I1):** aceitar ou não a divisão em fases, começando pela divisão do `.ino` sem mudança visual.
2. **Concorrência (B1, B2):** task serial no core 0, `loop()` como dono único do LVGL e do modelo, e `gen` por slot.
3. **Conta em foco (B4):** o que substitui a "conta ativa" e quais telas são por conta (`/modelos`, `/janela`, `/ritmo`) e quais são globais (incidentes).
4. **Falhas e ponto (I5, I10, I11):** o que conta como falha (o 429 com headers é dado?), a tabela de verdade do ponto (boot, consulta em andamento, sem WiFi, dado velho) e a aparência de um card com erro.
5. **Fontes (B5):** fonte mono com acentos e licença, glifos `█ ░ ❯ ✓ ✗ ↻ ⏲`, e fallback para os `LV_SYMBOL_*`.
6. **Campos por layout (I8):** tabela validada com a fonte real em `gen_mockups.py`.
7. **Bridge (I7):** contrato novo de `/window` e `/tokens`, e como separar duas contas no mesmo PC (`CLAUDE_CONFIG_DIR`).
8. **Agendamento e cota (I3, I4):** timeout por conta, backoff em 401/403, status.claude.com global e probe só na conta em foco.
9. **Navegação (I12, I13):** menu como modal sobre um shell persistente ou como `ST_MENU`, regra de voltar, refresh manual e fila de momentos de limiar por conta.
10. **Itens da home atual (I14):** destino do chip OK/ATENÇÃO/BLOQUEADO, das contagens do bridge e dos mascotes Clawd.
