# THEP Usage Stick v3 — várias contas na tela inicial e UI no estilo Hermes

*   **Author:** caio
*   **Status:** Implemented (fase 4 em validação)
*   **Date:** 2026-11-08 (revisado em 2026-11-09)

--------------------------------------------------------------------------------

## Overview

O Claude Usage Stick é um firmware para ESP32-S3 com tela touch de 3,5" (480×320, LVGL 9.2). Ele mostra quanto do limite de uso do Claude Code (o *rate limit*) já foi gasto. Para isso, faz uma requisição mínima à API (`max_tokens: 1`) e lê os headers `anthropic-ratelimit-unified-*` da resposta. A v2.3 guarda até 4 contas, mas **consulta e mostra uma de cada vez**, a "conta ativa". Para ver outra, é preciso entrar em Ajustes → Contas e trocar. A interface tem dois idiomas, português e inglês.

A v3 faz duas mudanças, que podem ser entregues em fases:

1.  **Dados de várias contas:** todas as contas salvas são consultadas a cada intervalo, numa task de rede separada da interface. A tela inicial mostra todas lado a lado, com o layout mudando pela quantidade (1, 2, 3 ou 4).
2.  **Nova interface:** a linguagem visual do **Hermes Agent** com as cores da **THEP**.
    *   O Hermes Agent é um agente de terminal da Nous Research. A CLI dele abre com um banner com o nome em ASCII art (fonte figlet *ANSI Shadow*), um caduceu desenhado em pontos braille e painéis com o título embutido na borda.
    *   A THEP (Thep Inovação Sustentável) é a empresa do dono. O design system dela define a paleta e o logo, um pássaro colorido.

    A v3 traz:
    *   o logo "THEP" em ANSI Shadow;
    *   o pássaro da THEP em pontos braille;
    *   um painel com título na borda;
    *   uma barra de status no rodapé;
    *   fonte monoespaçada.

    Um menu no cabeçalho substitui o swipe. Um ponto de status colorido e pulsante substitui o texto "atualizado há XXs".

**Token bridge:** o `tools/token_bridge.py` é um script Python que roda no PC do dono. Ele soma os tokens consumidos nas sessões locais do Claude Code e envia o total ao aparelho via `POST /tokens`. A tela mostra esse total como "tokens na janela: 1.2M entrada • 88k saída". Neste documento:
*   **token OAuth** é a credencial `sk-ant-oat01-…`;
*   **contagem de tokens** é esse total de consumo.

Os mockups aprovados ficam no canvas [THEP Usage Stick — Estilos](https://claude.ai/artifact/JeUby4eTTN3HA5PsYaaimR), na linha "Estilo A · identidade Hermes, cores THEP". O canvas pede login, então há uma cópia local em [mockup-home.dc.html](./review/context/mockup-home.dc.html), com os 4 layouts e o menu.

## The Problem

O dono do aparelho trabalha com **duas contas corporativas do Claude ao mesmo tempo**. Com a v2.3, para saber quanto resta em cada uma, ele precisa:
1.  abrir Ajustes;
2.  entrar em Contas;
3.  tocar na outra conta;
4.  esperar a nova consulta.

Faz isso várias vezes por dia. Além do tempo perdido:

*   **Só uma conta fica visível.** A outra pode estar perto do limite sem nenhum aviso. A projeção e os *momentos de limiar* só funcionam para a conta ativa. *Momento de limiar* é o overlay animado de tela cheia que aparece quando a janela de 5h ou a semana cruza 25%, 50%, 70% ou 100%.
*   **Trocar de conta recarrega o histórico da outra**, e as duas nunca aparecem lado a lado para responder "qual conta eu uso agora?".
*   **A navegação por swipe esconde as telas.** Os pontinhos de paginação não dizem o que existe em cada tela. Um menu com nomes deixa isso explícito e libera as bordas laterais, hoje reservadas ao swipe.
*   **"Atualizado há 24s" ocupa espaço e não mostra se alguma conta está falhando.** Com várias contas, uma idade única não descreve o estado.
*   **A identidade visual é a do projeto original**, com o coral do Claude, a fonte Montserrat e o *Clawd* (o mascote em pixel art da v2.3). O dono quer a identidade da THEP sobre a linguagem de terminal do Hermes Agent.

Os custos aceitos pelo dono (mais requisições à API, até 4 tokens OAuth em RAM e a concorrência entre rede e tela) estão em [Risks & Mitigations](#risks--mitigations).

## Goals

*   **G1 — Todas as contas visíveis de uma vez.** A tela inicial mostra cada slot salvo (1 a 4), depois do desbloqueio por PIN, com:
    *   % da janela de 5h, em destaque;
    *   barra de texto;
    *   reset da 5h;
    *   % e reset da semana;
    *   projeção da 5h;
    *   marca de estado da API (`ok` / `atenção` / `limite`).

    Nenhuma ação é necessária para ver outra conta.
*   **G2 — Layout pela quantidade de contas.** Há layouts específicos para 1, 2, 3 e 4 contas, conforme os [mockups](./review/context/mockup-home.dc.html). A quantidade é a de **slots salvos**: um slot com falha, ou que não decifrou, continua ocupando a coluna dele, e o layout não muda a cada queda de rede.
*   **G3 — Todas as contas atualizadas a cada intervalo, sem travar a tela.**
    *   Cada conta é consultada uma vez por ciclo, e um novo ciclo começa um intervalo depois do fim do anterior (30 s, 1, 2 ou 5 min, escolhidos pelo dono).
    *   A interface responde ao toque durante as consultas: nenhuma chamada HTTPS roda na thread que desenha a tela.
*   **G4 — Estado das consultas em um olhar.** Um ponto colorido e pulsante no cabeçalho resume o estado de todas as contas. As regras completas estão em [§4](#4-falhas-e-o-ponto-de-status). A prioridade é:
    *   vermelho: todas as contas em falha;
    *   amarelo: ao menos uma conta em falha;
    *   azul: ciclo terminado há menos de 10 s com todas as contas OK, ou primeiro ciclo após o desbloqueio ainda em andamento;
    *   verde: demais casos.

    "Em falha" quer dizer que a última consulta da conta falhou. Uma conta nova, ainda sem resposta, não conta como falha.

    O card de uma conta em falha mostra a classe do erro e a idade dos dados (`⚠ AUTH · 4m`), ou só a classe quando a conta ainda não tem nenhum dado (`⚠ AUTH`).
*   **G5 — Navegação por menu.**
    *   O botão `❯ menu` no cabeçalho abre a lista: `Atualizar agora`, `/agora` (tela inicial), `/modelos`, `/janela` (gráfico da 5h), `/ritmo` (uso por hora), `/contas` e `/ajustes`.
    *   A tela Sobre fica dentro de `/ajustes`, como na v2.3.
    *   O swipe entre telas e o slideshow deixam de existir.
    *   Todas as telas da v2.3 continuam acessíveis.
*   **G6 — Identidade Hermes × THEP:**
    *   logo ANSI Shadow em 3 faixas de cor;
    *   pássaro em braille;
    *   painel com título na borda;
    *   barra de status;
    *   fonte monoespaçada **com acentos do português**;
    *   somente as cores da [paleta da §6](#6-identidade-visual-tema-fontes-e-imagens), que vêm do design system THEP, com duas exceções declaradas: vermelho `#f05252` e amarelo `#f0c419`.
*   **G7 — Projeção curta.** A projeção aparece como:
    *   `✓ não esgota · ~78%`, onde 78% é o uso previsto no momento do reset;
    *   `✗ esgota 15:20 · 38m`, com a hora em que esgota e o tempo até lá.

    Uma única função calcula a projeção, usada pela tela inicial e por `/janela`.
*   **G8 — Atualização e volta para a v2.3 sem perder dados.** Gravar a v3 por cima de uma v2.3 mantém as contas, o PIN e o histórico. Gravar a v2.3 de volta também.

## Non-Goals

*   **Mais de 4 contas.** O limite `ACCT_MAX = 4` continua.
*   **Mudar a tabela de partições** (`partitions.csv`) **ou o formato dos arquivos de histórico** (`/hist{0..3}.bin`). Qualquer uma das duas mudanças apaga dados no próximo flash.
*   **Renovar tokens OAuth.** Os tokens `sk-ant-oat01` continuam fixos. Uma conta que recebe 401 passa a mostrar o erro e um caminho para colar um token novo.
*   **Tema claro.** A identidade é só no tema escuro.
*   **Ler dados novos da API.** A v3 usa os mesmos headers `unified-*` que a v2.3 já lê.
*   **Mudar o flasher web** (usagestick.autom.my).
*   **Polling adaptativo.** As contas são consultadas no intervalo fixo escolhido pelo dono (decisão dele). O status global e a sonda por modelo feita junto com a consulta da conta em foco ficam dentro do escopo.

## Open Questions

*   Nenhuma.

## Critical User Journeys (CUJs)

### CUJ 1: Ver as duas contas sem tocar na tela

*   **Given**: o aparelho tem 2 contas salvas, foi desbloqueado com o PIN e está com WiFi e relógio sincronizado.
*   **When**: o dono olha para a tela inicial.
*   **Then**: aparecem duas colunas, uma por conta, cada uma com a % da 5h, os resets, a semana, a projeção e a marca de estado. Os valores têm, no máximo, um intervalo mais a duração de um ciclo de idade, e o ponto do cabeçalho está verde ou azul.

### CUJ 2: Perceber que uma conta parou de atualizar

*   **Given**: 2 contas, e o token OAuth da Conta 2 foi revogado.
*   **When**: o próximo ciclo de consultas termina.
*   **Then**:
    *   o ponto fica amarelo;
    *   a coluna da Conta 2 continua no lugar, com os últimos valores esmaecidos e `⚠ AUTH · 4m` no lugar da projeção;
    *   a Conta 1 continua atualizando;
    *   o token revogado não é consultado de novo a cada ciclo: ele entra em espera crescente.

### CUJ 3: Abrir outra tela pelo menu

*   **Given**: a tela inicial está aberta.
*   **When**: o dono toca em `❯ menu` e escolhe `/janela`.
*   **Then**: abre o gráfico da 5h da conta em foco, com o rótulo da conta no título do painel. O cabeçalho, o ponto de status e o rodapé continuam os mesmos, e as consultas seguem rodando.

### CUJ 4: Adicionar uma conta

*   **Given**: 2 contas salvas, sessão desbloqueada.
*   **When**: o dono abre `/contas`, toca em `+ Adicionar conta` e cola o token OAuth no formulário web.
*   **Then**:
    *   a tela de token aparece **dentro do painel**, e as consultas das outras contas continuam;
    *   depois que o token é validado, a tela inicial passa ao layout de 3 colunas;
    *   a conta nova aparece como `consultando…` até a primeira resposta, sem mudar a cor do ponto;
    *   as outras duas mantêm os dados.

### CUJ 5: Voltar para a v2.3

*   **Given**: o aparelho roda a v3 com 2 contas e histórico de vários dias.
*   **When**: o dono grava a v2.3 de novo (só a partição de app).
*   **Then**: a v2.3 inicia, pede o PIN, mostra a conta em foco como ativa e encontra o histórico. Nenhum dado é perdido.

## Technical Plan (Architecture)

O plano tem três camadas, nesta ordem de dependência:

1.  **Rede:** uma task de rede que consulta as contas.
2.  **Modelo:** o estado de cada conta, como fonte única.
3.  **Interface:** um shell fixo (cabeçalho, ponto e rodapé) com o conteúdo trocado pelo menu.

O visual novo é a última camada. Por isso a entrega pode ser feita em fases, e a dor principal (G1) é resolvida antes da identidade (G6).

### 1. Concorrência: quem pode fazer o quê

Hoje o LVGL roda sem suporte a threads (`lv_conf.h:43`, `LV_USE_OS LV_OS_NONE`). Vários pontos fazem HTTPS e desenham na mesma passada:
*   `bg_refresh()` chama `set_hdr_status()` e `lv_refr_now()` em volta do fetch;
*   `handleTokenPost()` valida o token dentro do handler web e escreve nos labels.

A regra da v3 é:

*   **Task de rede** (`net_task`, fixada no core 0, stack de 14 KB):
    *   É a única que abre conexões HTTPS e a única que mexe no rádio WiFi (reconexão).
    *   **Nunca** chama `lv_*` e nunca escreve no modelo.
    *   Nunca acessa LittleFS ou NVS.
*   **`loop()`** (core 1). É o **único dono** do modelo (`AccountState[]`), do LVGL, do LittleFS e do NVS.
*   **Comunicação entre os dois:** duas filas FreeRTOS.
    *   `jobQ` (loop → net): pedidos `{tipo, slot, gen, modelo}`, **sem o token OAuth**.
    *   `resQ` (net → loop): resultados `{tipo, slot, gen, UsageData | ModelStatus, httpCode, classe, fimEpoch}`.
*   **Tokens OAuth:** ficam numa tabela `tokens[ACCT_MAX][200]` protegida por um mutex.
    *   A task copia o token do slot para um buffer na própria stack só durante o job e zera esse buffer (`memset`) no fim.
    *   O header `Authorization` é montado num buffer de stack que também é zerado.
    *   O `HTTPClient::addHeader` copia o valor para um `String` no heap, que é liberado no `end()` sem ser zerado (risco residual, ver [Risks](#risks--mitigations)).
*   **Uma conexão por vez.** Cada sessão TLS ocupa cerca de 40–50 KB de RAM interna. O pool do LVGL, que hoje usa 96 KB de RAM interna (`LV_MEM_SIZE`), passa para a PSRAM, alocado com `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` através das macros `LV_MEM_POOL_INCLUDE`/`LV_MEM_POOL_ALLOC` do `lv_conf.h`.
*   **Contador `gen` por slot.**
    *   É incrementado ao adicionar, remover ou trocar o token da conta.
    *   O `loop()` descarta qualquer resultado `USAGE` com `gen` diferente do atual, o que impede que uma resposta atrasada caia numa conta removida ou num slot reaproveitado.
    *   Remover uma conta faz `gen++` e zera o token na tabela. A tabela guarda também o `gen` de cada slot. Ao tirar um job da fila, a task compara o `gen` do job com o da tabela. Se forem diferentes, ou se o slot estiver vazio, ela não faz a requisição e devolve um resultado com a classe `FC_DISCARDED`, que encerra aquele item do ciclo. A fila não é esvaziada, então os jobs das outras contas seguem normalmente. O `loop()` **não espera** a task: um job da conta removida que já esteja em andamento termina em, no máximo, o timeout de um job, e o resultado é descartado pelo `gen`.

```text
            core 0                                   core 1
  ┌──────────────────────────┐   jobQ    ┌────────────────────────────────┐
  │ net_task                 │◀──────────│ loop()                         │
  │  · fetchUsage(slot,      │           │  · scheduler (monta jobs)      │
  │      modelo)             │   resQ    │  · aplica resultado no modelo  │
  │  · fetchModelStatus()    │──────────▶│  · hist_push / save (LittleFS) │
  │  · validateToken()       │           │  · thresholds, dot_state       │
  │  · WiFi reconnect        │           │  · LVGL: shell + tela + overlay│
  └────────────┬─────────────┘           │  · WebServer.handleClient()    │
               │ lê sob mutex            └────────────────────────────────┘
        tokens[ACCT_MAX][200]
```

O servidor web (`WebServer`) continua no `loop()`. O POST do token deixa de validar dentro do handler: ele enfileira um job `VALIDATE` e responde `202` com `{"state":"pending"}`. A página consulta `GET /token/status` a cada 1 s, por no máximo 30 s, até receber `{"state":"ok"}` ou `{"state":"rejected","reason":…}`. O fluxo completo, inclusive o onboarding e a cifra com a chave da sessão, está no item 6 do Detailed Implementation Plan. A `net_task` é criada no `setup()`, porque o `VALIDATE` já é usado no onboarding, antes do desbloqueio.

### 2. Agendamento das consultas

O agendador fica no `loop()` e monta um **ciclo** por intervalo:

1.  **Pré-condições.** Se não há WiFi, nenhum job de consulta é enviado: só um job `RECONNECT`, que percorre as redes salvas (até 3, como o `autoConnect()` da v2.3, com 8 s por rede). Antes de enviar o job, o `loop()` copia a lista para uma área protegida por mutex; a task lê essa cópia e não grava nada no NVS. Quem promove a rede que conectou é o `loop()`, quando o resultado chega. As contas ficam com a classe `NOWIFI`, contada como uma falha só, não N. Se o relógio ainda não sincronizou por NTP, nenhum job de consulta é enviado e as contas continuam `pendente`. Sem NTP, o TLS falha, como mostrou o commit e80448f. Depois de 60 s sem relógio, contados a partir do primeiro tick com WiFi conectado, as contas passam para `NOCLOCK`, que aparece no card como `⚠ NOCLOCK`. O agendador inicia o NTP (`configTime`) assim que há WiFi.
2.  **Consultas.** Um job `USAGE` para cada slot decifrado, em ordem de slot, **exceto** os que estão em espera.
    *   A consulta da **conta em foco** também serve de sonda por modelo: em vez do modelo fixo (`PROBE_MODEL`), ela usa o próximo modelo do rodízio (Haiku → Sonnet → Opus → Fable). Assim a mesma requisição devolve os headers de uso e o código HTTP e a latência daquele modelo.
    *   Isso substitui o job `PROBE` separado da v2.3. Os headers `unified-*` são da conta, então são os mesmos para qualquer modelo.
    *   **Isolamento da sonda:** se a resposta com o modelo do rodízio vier **sem** headers `unified-*` (modelo indisponível, 404 ou 5xx daquele modelo), o código HTTP vai só para a lista de `/modelos`. A task repete na hora a consulta da conta com `PROBE_MODEL`, e só esse segundo resultado define a classe da conta. Se o modelo do rodízio já é o `PROBE_MODEL`, a consulta não é repetida. Uma falha de um modelo nunca deixa a conta amarela.
3.  **Status.** Um job `STATUS` (status.claude.com) a cada `STATUS_POLL_SEC` (300 s), **global** e não por conta. Isso coloca em uso a constante que hoje está definida e não é usada (ver o [drift report](./notes/2026-11-08_DRIFT_REPORT.md)).

**Regras:**
*   **Sem sobreposição.** O próximo ciclo começa em `fim do ciclo + intervalo`, não em horário fixo.
*   **Timeouts por requisição:** conexão de 5 s (`HTTPClient::setConnectTimeout`), handshake TLS de 5 s (`setHandshakeTimeout`) e leitura de 8 s (`HTTPClient::setTimeout`). Na v2.3 só existe a leitura, de 15 s, e o handshake fica no padrão de 120 s do arduino-esp32. As APIs foram conferidas no core `esp32:esp32` 3.3.8, a versão fixada no plano.
    *   A leitura de 8 s limita cada espera por dados, e não a requisição inteira (`HTTPClient.cpp:1287`).
    *   O DNS não tem timeout configurável no core (`Network.hostByName` → `lwip_getaddrinfo`). O lwIP desiste depois de 4 tentativas por servidor com espera crescente a partir de 1 s (`DNS_MAX_RETRIES`, `DNS_TMR_INTERVAL`), cerca de 7 s por servidor, até 3 servidores: ~21 s, estimado. Depois da primeira consulta, o nome fica no cache do lwIP.
    *   Pior caso de **uma requisição**: ~21 + 5 + 5 + 8 = **~39 s**.
    *   Um ciclo típico com 4 contas leva de 6 a 10 s.
    *   O pior caso de um ciclo tem 6 requisições: 4 `USAGE`, 1 reenvio da sonda com `PROBE_MODEL` e 1 `STATUS`. Com DNS lento, 6 × ~39 s ≈ **4 min**. Com o DNS em cache, 6 × 18 s = **108 s**.
    *   Como não há sobreposição, o próximo ciclo só começa depois.
*   **Espera crescente para AUTH.**
    *   A n-ésima falha AUTH seguida faz a conta pular 2^(n−1) ciclos (1, 2, 4…), até um total de 30 min.
    *   A espera zera quando o token é trocado.
*   **"Atualizar agora".** Durante um ciclo não tem efeito, porque o ciclo em andamento já atende ao pedido. Fora de um ciclo, inicia um ciclo na hora.

### 3. Modelo por conta

Um `AccountState acct[ACCT_MAX]` substitui as variáveis globais únicas: `g_usage`, `g_lastFetchOk`/`g_lastOkMs`, `g_tok`, `g_hist*`, `g_hourBurn`, `g_days` e `g_thr*`. Cada registro tem:

| Campo | Conteúdo |
|---|---|
| `gen`, `keyOk` | Geração e se o token decifrou. Slot salvo e rótulo **não** são copiados: a fonte única é o `AccountSlots` da v2.3 (`g_accts.used[slot]`, `g_accts.label[slot]`) |
| `usage`, `usageAtEpoch` | Último `UsageData` bom e quando chegou (0 = nenhum ainda) |
| `lastTryEndEpoch`, `lastClass`, `lastHttp` | Resultado da última tentativa |
| `fails`, `backoffCycles` | Falhas `AUTH` seguidas e espera atual |
| `lastSaveEpoch`, `lastSavedReset` | Quando o histórico foi gravado e o reset da 5h naquele momento (regra de gravação) |
| `hist[HIST_MAX]`, `histN/Head`, `hourBurn[24]`, `lastH5`, `days[31]`, `dayN` | Mesmo conteúdo do `HistFileV2` de hoje |
| `thr` | Estado de limiar (`fired/prev/base`) |
| `tok` | `TokenStats` (contagem de tokens recebida do bridge) |

O token OAuth decifrado **não** fica no `AccountState`, e sim na tabela protegida por mutex da §1. O registro ocupa cerca de 5 KB por conta, 20 KB no total, e é alocado em **PSRAM**. No desbloqueio, os 4 históricos são carregados de `/hist{0..3}.bin`, sem mudar o formato (G8).

**Conta em foco.** `focus` é um índice de visualização e não tem efeito na rede, exceto que a consulta dessa conta faz o rodízio de modelos (§2).
*   É salvo na chave NVS `acct`, a mesma da v2.3. Num rollback, a conta em foco volta como "ativa".
*   Tocar no card de uma conta na tela inicial muda o foco.
*   `/modelos`, `/janela` e `/ritmo` mostram a conta em foco, com o rótulo no título do painel. Tocar no rótulo passa para a próxima conta.
*   Incidentes do status.claude.com são globais.

**Projeção.** O cálculo da taxa de consumo sai de `trend_redraw()` (`claude_stick.ino:1542-1660`) e vira uma função pura (a assinatura exata está no Detailed Implementation Plan, item 9).
*   **Entrada:** o histórico de amostras `{epoch, %5h, %7d}` da conta, a % atual, o horário do reset e a hora atual.
*   **Saída:** `{estado, etaEpoch, minutos, pctNoReset}`, com estado `OK_ATE_RESET | ESGOTA | ESGOTADA | SEM_DADOS`.
*   A tela inicial e `/janela` usam a mesma função. O gráfico só desenha o que ela devolve.

### 4. Falhas e o ponto de status

**Classes de resultado** de cada `USAGE`:

| Resposta | Classe | Conta como falha? |
|---|---|---|
| 200 com headers `unified-*` | `OK` | Não |
| 429 **com** headers | `OK` (dado válido: conta no limite) | Não |
| 401 / 403 | `AUTH` | Sim, com espera crescente |
| 429 sem headers | `RATE` | Sim |
| Outros 4xx (400, 404, 413…) | `ERR` | Sim |
| Qualquer outro código (3xx, 200 sem headers) | `ERR` | Sim |
| 5xx | `SRV` | Sim |
| Código ≤ 0, timeout, DNS, TLS | `NET` | Sim |
| Relógio sem NTP por mais de 60 s | `NOCLOCK` | Sim |
| Sem WiFi | `NOWIFI` | Sim |

Só o `USAGE` afeta o ponto. O `STATUS` não afeta. O resultado de sonda de modelo que vem junto com a consulta da conta em foco também não. Hoje um 401 que vem com headers `unified-*` é lido como OK (`api.cpp:63`). Na v3 ele passa a ser `AUTH`.

**Estado de cada conta para o ponto:**
*   **`pendente`:** nenhuma tentativa concluída desde o desbloqueio ou desde a última mudança de `gen` (conta nova ou token trocado). **Não** conta como falha.
*   **`falhou`:** a última tentativa concluída é falha. Uma conta em espera crescente continua `falhou`, porque a última tentativa dela foi `AUTH`. Uma task travada não precisa de regra própria: ela leva ao reinício descrito abaixo.
*   **`ok`:** os demais casos.
*   **Slot que não decifrou** (`keyOk = false`): mostra `SEM CHAVE` no card e conta como `falhou`. Ele nunca é consultado, então nunca fica `pendente`.

**`dot_state(contas[], agora, cycleEndEpoch, primeiroCiclo)`** é uma função pura. Ela recebe o horário de fim do último ciclo e devolve, nesta prioridade:

| Condição | Cor |
|---|---|
| Há ao menos uma conta não pendente, e todas as não pendentes estão em `falhou` (inclui sem WiFi) | Vermelho `#f05252` |
| Ao menos uma conta em `falhou` | Amarelo `#f0c419` |
| Primeiro ciclo após o desbloqueio em andamento, ou `agora − cycleEndEpoch < 10 s` com todas as contas não pendentes `ok` | Azul `#2f8fd6` |
| Demais casos | Verde `#5caf4e` |

Sem nenhuma conta não pendente (por exemplo, logo depois do desbloqueio), o ponto nunca fica vermelho. O azul é medido a partir do fim do ciclo, para todas as contas acenderem juntas. Com intervalo de 30 s, ele fica aceso por ~10 s a cada ciclo, por decisão do dono.

**Pulso:** todas as cores pulsam (decisão do dono), com a mesma animação.
*   São 4 degraus de opacidade (100%, 70%, 40%, 70%) num ciclo de 1,6 s, ou seja, 2,5 trocas por segundo.
*   A troca é feita por um `lv_timer` e só invalida a área do ponto. Com o render atual em tela cheia, o envio ao painel continua sendo da tela inteira; o spike da fase 3 (item 34) mede esse custo.
*   O custo de render está em [Risks & Mitigations](#risks--mitigations).

**Texto do card**, nesta precedência:
1.  slot que não decifrou: `SEM CHAVE`;
2.  em falha e sem nenhum dado ainda (por exemplo, a primeira consulta depois de trocar o token voltou `AUTH`): `⚠ <classe>` no lugar da projeção e `--%` no lugar da 5h;
3.  em falha com dado anterior: mantém os últimos valores esmaecidos (texto `#8a98a8`) e troca a linha de projeção por `⚠ <classe> · <idade dos dados>`, por exemplo `⚠ AUTH · 4m`. A idade sai em `Ns` abaixo de 1 min, `Nm` abaixo de 1 h e `Nh` a partir daí;
4.  `pendente`: `consultando…`, com `--%`.

**Proteção contra task travada:**
*   A task grava o horário de início de **cada requisição HTTPS**, inclusive o reenvio da sonda com `PROBE_MODEL`, e o `loop()` lê esse horário. Um job com duas requisições não soma os dois tempos.
*   Se uma requisição passar de 90 s sem terminar, o motivo é gravado na NVS (`lastReset`) e o aparelho reinicia com `esp_restart()`. O pior caso de uma requisição que respeita os timeouts é ~39 s (§2), então o limite tem mais de 2× de folga, mesmo com DNS lento. Um `RECONNECT` leva no máximo 3 × 8 s = 24 s.
*   O estado está salvo em flash, e o PIN é pedido de novo. A task **não** é recriada: um `vTaskDelete` no meio do TLS vazaria o contexto mbedTLS (~40–50 KB de RAM interna) e o socket.
*   Falta de jobs (sem WiFi, sem relógio, todas as contas em espera) não dispara a reinicialização.
*   No boot seguinte, o motivo é impresso no serial e a chave `lastReset` é apagada.

### 5. Interface: shell fixo, conteúdo trocado, overlays

Hoje cada troca de estado apaga a tela inteira e chama `stop_web()` em `render_state()` (`claude_stick.ino:2620-2635`), e o auto-refresh só roda em `ST_MAIN`. Na v3, depois do desbloqueio, a tela é um **shell** fixo.

#### Shell e conteúdos

*   **O shell é criado uma vez:**
    *   cabeçalho (50 px): logo, ponto e `❯ menu`;
    *   painel com borda e título;
    *   rodapé de status (20 px).
*   **Conteúdos trocados dentro do painel**, com o rótulo do menu entre parênteses:
    *   `home` (`/agora`);
    *   `models` (`/modelos`);
    *   `trend` (`/janela`);
    *   `heat` (`/ritmo`);
    *   `accounts` (`/contas`);
    *   `settings` (`/ajustes`);
    *   `about` (dentro de `/ajustes`);
    *   `token` (adicionar ou trocar token, a partir de `/contas` ou `/ajustes`);
    *   `wifi` (a partir de `/ajustes`).
    
    Trocar o conteúdo não para as consultas nem o servidor web.
*   **Exceção do `wifi`:** escanear e conectar mexe no rádio. Ao abrir esse conteúdo, o agendador para de enviar jobs, e a tela mostra "aguardando a rede…" até a task terminar o job em andamento (em geral poucos segundos; no pior caso, o de um job com reenvio da sonda, 2 × ~39 s). Só então a tela de WiFi assume o rádio. Ao sair, o agendador volta.
*   **Telas fora do shell:** PIN, definição de PIN, e o WiFi e o token do **onboarding**. Elas acontecem antes do desbloqueio, quando não há sessão. Os estados `ST_LOADING` e `ST_ERROR` deixam de existir: o shell abre direto depois do desbloqueio, com os cards em `consultando…`, e uma falha de consulta aparece no card, nunca numa tela de erro.
*   **Menu:** é um modal no `lv_layer_top`, sobre o shell.
    *   Tem 262 px de largura e fica ancorado abaixo do botão. São 7 linhas de 36 px: 7 × 36 px + 8 px de padding + 2 px de borda = 262 px de altura, que cabem nos 270 px abaixo do cabeçalho.
    *   Tocar fora ou escolher um item fecha o menu.
    *   O primeiro item é `Atualizar agora`.
*   **Voltar:** o botão de menu é sempre visível. Não há volta automática para a tela inicial.
*   **Ordem dos overlays**, de baixo para cima: menu, momento de limiar, incidente. O incidente aparece sobre qualquer conteúdo do shell (hoje só em `ST_MAIN`).
*   **Removidos:**
    *   o tileview, os pontinhos de paginação e o slideshow;
    *   o badge `@label`;
    *   o hotspot de demo com duplo toque;
    *   a `refBar`;
    *   o texto "atualizado há".
    
    A chave NVS `slide` deixa de ser lida, mas **não é apagada** (G8).

#### Layouts da tela inicial

São quatro layouts, um por quantidade de slots. A tabela abaixo é um resumo provisório. A versão final, com fontes e limites de caracteres, fica no [Detailed Implementation Plan](#detailed-implementation-plan) (item 37), validada pelo gerador de mockups com a fonte real.

| Contas | Disposição | Por conta |
|---|---|---|
| 1 | Pássaro em braille + seções | 5h grande (50 px), barra de 18 blocos, reset, semana com barra, reset, projeção, contagem de tokens |
| 2 | 2 colunas | 5h (38 px), barra de 14 blocos, reset, semana + barra de 8 blocos, reset, projeção |
| 3 | 3 colunas | 5h (30 px), barra de 9 blocos, reset, semana, reset, projeção |
| 4 | Grade 2×2 | Rótulo + 5h (20 px) na mesma linha, barra de 14 blocos, semana + reset da 5h, projeção |

Toda barra de blocos fica entre colchetes `[` `]` em `#8a98a8`, como nos mockups. Rótulos com mais caracteres que o espaço são cortados com `…`. A ordem das contas é a ordem dos slots. As contas se distinguem pelo rótulo, não pela cor.

#### Marca de estado da API

O header `anthropic-ratelimit-unified-status` vira uma marca ao lado do rótulo:

| Valor do header | Marca | Cor do texto |
|---|---|---|
| `allowed` | `ok` | `#8ed57f` |
| `allowed_warning` | `atenção` | `#f0a32a` |
| `rejected`, `rate_limited`, `exceeded` | `limite` | `#ff8a8a` |
| outro valor contendo `warning` | `atenção` | `#f0a32a` |
| outro valor desconhecido | o próprio valor, cortado em 8 caracteres | `#8a98a8` |
| ausente (sem dado ainda) | — | — |

As regras de `rejected`/`rate_limited`/`exceeded` e de `warning` são as mesmas de `status_color`/`overall_label` da v2.3 (`claude_stick.ino:1137-1149`).

No layout de 4 contas não há espaço para a marca, então ela vira a cor da % da 5h quando o valor é `atenção` ou `limite`.

#### Momentos de limiar

*   O estado de limiar é por conta.
*   Em cada ciclo, aparece no máximo um momento, o de maior limiar entre as contas que cruzaram algum, com o rótulo da conta no overlay.
*   Os outros cruzamentos daquele ciclo não aparecem. Eles continuam marcados como disparados, para não repetirem.

### 6. Identidade visual: tema, fontes e imagens

#### Paleta

Os valores vêm do design system THEP, tema escuro. A cópia local está em [thep-design-system-tokens.json](./review/context/thep-design-system-tokens.json).

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
| Nível ok / ponto verde | `thep-green` | `#5caf4e` |
| Texto ok | `ac-acerto-ink` | `#8ed57f` |
| Nível de aviso | `color-warning` (dark) | `#f0a32a` |
| Ponto amarelo | **exceção** (fora da paleta) | `#f0c419` |
| Nível alto / ponto vermelho | **exceção** (fora da paleta) | `#f05252` |
| Texto de erro | **exceção** (fora da paleta) | `#ff8a8a` |

#### Níveis de uso

São degraus fixos, no lugar do gradiente contínuo atual (`grad_color`):
*   verde (`#5caf4e`) abaixo de 50%;
*   aviso (`#f0a32a`) de 50% a menos de 80%;
*   vermelho (`#f05252`) a partir de 80%.

Valem para a 5h e para a semana.

#### Tema

Um único módulo `theme` guarda os papéis de cor acima, os estilos `lv_style_t` compartilhados e as fontes. As cores fixas espalhadas hoje (`claude_stick.ino:40-53` e o CSS da página web) passam a usar esses papéis.

#### Imagens

O logo THEP em ANSI Shadow e o pássaro em braille viram imagens LVGL prontas, geradas no computador por script a partir do logo THEP, como já é feito com `logo_assets.h`:
*   logo do cabeçalho, ~104×38;
*   pássaro grande, ~124×124, usado no layout de 1 conta, nos momentos de limiar e, recolorido em cinza com `image_recolor` + `image_recolor_opa`, no alerta de incidente;
*   pássaro pequeno, 13×13, no rodapé.

#### Fonte

**JetBrains Mono 2.304** (SIL OFL 1.1, sem nome reservado), a mesma dos mockups aprovados, convertida com `lv_font_conv` em bpp 4.

| Fonte gerada | Uso | Glifos |
|---|---|---|
| Regular 11 | Texto secundário, rodapé, `chave:` | ASCII, Latin-1 (acentos e `·`), `— •`, box drawing usado (`│ ─ ╭ ╮ ╰ ╯`), `█ ░`, `❯ ✓ ✗ ⚠ …` |
| Regular 13 | Texto principal, menu | mesmos |
| Bold 13 | Rótulos de conta, títulos de seção e de painel | mesmos |
| Bold 20 / 30 / 38 / 50 | % de destaque por layout | só dígitos, `%` e `-` (para o `--%` do card sem dados) |

*   **Ícone de refresh:** o `↻` vem do `LV_SYMBOL_REFRESH` da Montserrat 14, que continua no firmware como `fallback` das fontes de texto. Ela também atende os ícones `LV_SYMBOL_*` das telas de WiFi, ajustes e outras.
*   **Ícone de relógio:** o rodapé mostra só a hora (`14:32`), sem ícone.
*   **Uma família só:** não é preciso mesclar outras fontes.
*   **Flash estimado:** cerca de 40 KB, extrapolando do que foi medido nos tamanhos 10–13 Regular com box drawing completo (12–17 KB cada) e reduzido (~6,5–8 KB cada). O valor real é medido na fase 3.
*   **Licença:** o texto da licença acompanha os arquivos gerados.

#### Clawd removido

Por decisão do dono, os mascotes saem da v3, junto com `logo_assets.h` (pixel art do Clawd e do logo Claude Code) e `clawd_dead.h`. As 6 imagens ARGB8888 desses arquivos somam ~190 KB de flash, mais do que as fontes novas.
*   **`/modelos`:** vira uma lista em texto no estilo Hermes, uma linha por modelo (`Opus     ok · 0.9s`), com a cor da classe.
*   **Momentos de limiar:** usam o pássaro em braille, com a % contando e a barra de blocos.
*   **Alerta de incidente:** usa o pássaro em braille em cinza.

#### Orçamento de flash

A fase 0 registra o tamanho do app da v2.3. A v3 precisa ficar abaixo de 2,8 MB, num slot de 3 MB.

### 7. Segurança

*   A v3 mantém a chave única para todos os slots (SHA-256 em 10 mil rodadas, com o MAC como salt).
*   No desbloqueio, a chave é derivada **uma vez** e todos os slots são decifrados para a tabela `tokens[]` (§1).
*   A chave derivada fica em RAM durante a sessão, para cifrar contas adicionadas depois, no lugar do PIN que a v2.3 mantém em RAM (`claude_stick.ino:105-107`). Quem lê a RAM já tem os tokens decifrados de qualquer forma.
*   **Desbloqueio parcial:** um slot que não decifra fica `SEM CHAVE`, sem impedir os outros. Se nenhum slot decifrar, conta como PIN errado, com a mesma regra de tentativas e espera de hoje.
*   **Zeragem:** a chave e os tokens são zerados (`memset`) ao remover a conta (só aquele token), ao trocar o token e no "Apagar tudo". O apagamento depois de 10 PINs errados acontece antes de qualquer token ser decifrado.
*   **Logs:** passam a dizer `[slot N]`, e o token nunca é logado.
*   **Exposição na LAN:** o contrato novo do bridge (§8) passa a aceitar contagens para qualquer conta pelo rótulo, sem autenticação, como o `POST /tokens` atual já aceita para a conta ativa. Alguém na mesma rede pode falsificar a contagem de tokens exibida, mas não a % de uso, que vem da API.

### 8. Contrato com o token bridge (fase 4)

O bridge só alimenta a contagem de tokens. Ela aparece no layout de 1 conta e em `/janela`. Por isso o contrato novo fica na fase 4 e não bloqueia a tela de várias contas.

*   **`GET /window`:** mantém os campos atuais, que passam a ser os da conta em foco. Ganha o parâmetro `?account=<rótulo>`, que devolve os campos dessa conta, e 404 se o rótulo não existe.
*   **`POST /tokens`:** com `account`, é aplicado ao slot com esse rótulo, comparado de forma exata (`strcmp`, diferenciando maiúsculas). Sem `account`, vai para a conta em foco. Rótulo desconhecido devolve **404**. O 409 `account_mismatch` da v2.3 deixa de existir.
*   **Compatibilidade:** o bridge atual continua funcionando **só para a conta em foco**. Para as outras ele espera para sempre, e trata o 404 como erro genérico. As outras contas exigem o bridge novo.
*   **`tools/token_bridge.py`:**
    *   ganha `--config-dir`, que lê `<dir>/projects` em vez de `~/.claude/projects`;
    *   passa a pedir `/window?account=`;
    *   trata o 404;
    *   corrige o glob para percorrer subpastas, como apontou o drift report.
*   **Duas contas no mesmo PC:** o dono usa as duas contas na mesma máquina. Por isso cada conta roda o Claude Code com o próprio `CLAUDE_CONFIG_DIR` (por exemplo, `alias claude-a='CLAUDE_CONFIG_DIR=~/.claude-a claude'`) e tem o próprio bridge, com `--config-dir ~/.claude-a --account <rótulo>`. Isso muda o jeito de abrir o Claude Code no dia a dia. O README documenta essa configuração.

### 9. Observabilidade

*   **Logs de ciclo** no serial: início e fim do ciclo, duração, contas consultadas, classe de cada resultado e o motivo do último reinício (`lastReset`).
*   **`GET /health`**, só na build de desenvolvimento (`THEP_DEV`), para medir os NFRs no aparelho. Devolve JSON sem tokens nem rótulos:
    *   `uptime_s`;
    *   `heap_internal_free` e `heap_internal_largest`;
    *   `psram_free`;
    *   `lv_mem_used_pct`;
    *   `net_stack_hwm`;
    *   `last_cycle_ms` e `cycles_over_interval`;
    *   `frame_ms_max`;
    *   `slots: [{slot, class, age_s, backoff}]`.

### 10. Fases de entrega

Cada fase compila, é gravada e é usável sozinha:

| Fase | Entrega | Resolve |
|---|---|---|
| 0 | Linha de base: tag `v2.3`, tamanho do app e heap anotados. Só extrai do `.ino` o que as fases 1 e 2 mudam (estado das contas, histórico e servidor web), sem mudança visual. Cada tela muda de arquivo quando for redesenhada | Base para o resto |
| 1 | Funções puras (`projection`, `dot_state`, níveis, barra, layout) com testes no computador. `AccountState[]` substitui as globais, ainda com uma conta na tela | Testabilidade |
| 2 | Pool do LVGL na PSRAM + `net_task` + agendador + `gen` + tabela de tokens OAuth + desbloqueio de todos os slots. Todas as contas consultadas. Tela inicial guiada pela tabela de layouts, ainda no visual atual, com a classe do erro e a idade no card | **G1, G3** |
| 3 | Tema, fontes e imagens. Shell + menu no lugar do tileview. Ponto de status. Os 4 layouts. `/modelos` em texto. Novo visual das demais telas. Momentos de limiar por conta | **G2, G4, G5, G6, G7** |
| 4 | Contrato novo do bridge. Teste de longa duração, ensaio de rollback, README e mockups | **G8** |

## Alternatives Considered

As decisões do dono citadas abaixo foram tomadas sobre os mockups do canvas [THEP Usage Stick — Estilos](https://claude.ai/artifact/JeUby4eTTN3HA5PsYaaimR) (cópia local: [mockup-canvas.json](./review/context/mockup-canvas.json), [mockup-home.dc.html](./review/context/mockup-home.dc.html)) e registradas no [brief do brainstorm](./notes/concepts_brief.md).

### Alternative A: Estilo B · Painel (cartões azul-marinho, fonte Sora)

*   **Pros**: cartões arredondados com números grandes. É o estilo que exige menos trabalho no LVGL: uma fonte proporcional e nenhum glifo especial.
*   **Cons**: a fonte proporcional não alinha em colunas as barras de blocos nem os valores `chave: valor`, que são a base da linguagem de terminal do Hermes. Na avaliação do dono, também ficou um visual de dashboard genérico.
*   **Reason for Rejection**: Rejected by author: perde o alinhamento em colunas que define o estilo Hermes. O dono escolheu o estilo A entre os três mockups.

### Alternative B: Estilo C · Matriz LED (números em Doto, barras de pontos com brilho)

*   **Pros**: é o mais próximo da foto de referência do dono, um caduceu do Hermes desenhado em pontos numa tela.
*   **Cons**: depende de brilho (`text-shadow`) nos números e nas barras. O LVGL não faz isso nativamente, e em sombra de software custaria redesenhar a tela inteira a cada quadro. Também precisaria de duas fontes extras (Doto e Space Mono).
*   **Reason for Rejection**: Rejected by author: o custo de render do brilho no ESP32 e as duas fontes extras. O dono escolheu o estilo A.

### Alternative C: Manter só a conta ativa consultada, com as outras paradas (modelo da v2.3)

*   **Pros**: zero requisições extras, o que importa em conta corporativa, onde cada consulta é visível para os admins. Menor uso de cota, um token OAuth em claro na RAM e nenhuma concorrência nova.
*   **Cons**: não resolve o problema. O dono continua trocando de conta várias vezes por dia, e a conta fora da tela pode chegar ao limite sem aviso.
*   **Reason for Rejection**: Rejected by author: não atende o G1. O dono usa as duas contas corporativas ao mesmo tempo e aceita o tráfego extra.

### Alternative D: Revezar as contas dentro do `loop()`, uma por tick, sem task de rede

*   **Pros**: mudança mínima, sem filas, `gen` ou dono único. As consultas ficam espalhadas em `intervalo / N`.
*   **Cons**: a interface continua congelando a cada consulta, por ~1–2 s e até 15 s em timeout, como na v2.3. Com 4 contas, isso acontece 4 vezes por intervalo, e o toque durante a consulta se perde. O G3 não é cumprido.
*   **Reason for Rejection**: Rejected by author: não cumpre o G3. Fica registrada como plano de contingência da fase 2 (ver [Risks](#risks--mitigations)).

### Alternative E: Uma task por conta, com consultas em paralelo

*   **Pros**: um ciclo de 4 contas termina no tempo de uma consulta.
*   **Cons**: cada sessão TLS ocupa cerca de 40–50 KB de RAM **interna**. Quatro sessões simultâneas, mais 4 stacks de 14 KB, não cabem com folga mesmo com o pool do LVGL na PSRAM, e a falha seria silenciosa (alocações voltando NULL).
*   **Reason for Rejection**: Rejected by author: a RAM interna não comporta. A task serial fecha um ciclo típico em 6–10 s, bem dentro do intervalo mínimo de 30 s.

### Alternative F: Menu como estado de tela cheia (`ST_MENU`), no lugar de um modal

*   **Pros**: encaixa na máquina de estados atual sem criar o conceito de shell.
*   **Cons**: cada troca de estado apaga a tela e chama `stop_web()`. Abrir o menu derrubaria o servidor do bridge e pararia o auto-refresh, que hoje só roda em `ST_MAIN`.
*   **Reason for Rejection**: Rejected by author: interromperia as consultas e o servidor a cada navegação. O shell fixo com o menu modal mantém os dois rodando.

### Alternative G: Ponto de status com 3 cores e pulso só durante a consulta

*   **Pros**: o significado fica mais simples (pulso = consultando), com menos estados e menos redesenho.
*   **Cons**: perde a distinção entre "recém-atualizado" (azul) e "ok".
*   **Reason for Rejection**: Rejected by author: sem o azul não dá para ver de relance que o ciclo acabou de confirmar todas as contas. O custo do pulso fica limitado (2,5 trocas por segundo, só na área do ponto).

### Alternative H: Iosevka como fonte única

*   **Pros**: é a única fonte que cobre todos os glifos sozinha, incluindo `↻` e `⏲`, com uma licença só (OFL).
*   **Cons**: tem largura de 0,5 em vez de 0,6 em, e `✓ ✗ ↻ ⏲` saem com largura dupla, o que quebra o alinhamento em colunas. Também foge do visual dos mockups aprovados, feitos em JetBrains Mono.
*   **Reason for Rejection**: Rejected by author: os glifos de largura dupla quebram as colunas. Com a JetBrains Mono, o `↻` vem da Montserrat 14 que já está no firmware, e o `⏲` sai do rodapé.

### Alternative I: Desenhar o logo e o pássaro em tempo real com fontes de box drawing e braille

*   **Pros**: o logo poderia ser regenerado em qualquer tamanho sem imagens.
*   **Cons**: exige o bloco braille inteiro (256 glifos) em cada tamanho e as 3 faixas de cor por linha de texto. O alinhamento varia com a métrica da fonte.
*   **Reason for Rejection**: Rejected by author: 256 glifos braille por tamanho custam mais flash que 3 imagens, e o desenho depende da métrica da fonte.

### Alternative J: Cor de perigo da marca (magenta/rosa) para o nível alto e o ponto de erro

*   **Pros**: fica 100% dentro da paleta THEP.
*   **Cons**: o perigo da marca (`#97035a` / `#f27fa2`) se parece com as cores decorativas do próprio pássaro. Na avaliação do dono, não é lido como alerta.
*   **Reason for Rejection**: Rejected by author: o alerta precisa se distinguir das cores da marca. Vermelho e amarelo ficam como exceções declaradas (§6).

## Risks & Mitigations

| Risco | Gatilho | Impacto | Mitigação | Onde |
|---|---|---|---|---|
| A `net_task` traz instabilidade (travamentos, corrida com o LVGL) | Fase 2 | Atraso na entrega do G1 | Regra de dono único, grep no `firmware/tests/run.sh` contra `lv_` em `src/net/`, teste de 1 h. Contingência: Alternativa D (revezar no `loop()`) | §1, fase 2 |
| Falta de RAM interna ou fragmentação durante o TLS | 4 contas + LVGL | Alocações falham sem aviso | Pool do LVGL na PSRAM, uma conexão por vez, NFR de 60 KB livres / bloco de 45 KB medido pelo `/health` | §1, §9 |
| O pulso redesenha a tela inteira | O flush atual (`claude_stick.ino:215-224`) rotaciona e envia os 480×320 a cada quadro | Quadro acima de 40 ms, toque lento | Pulso de 2,5 trocas/s invalidando só o ponto. Na fase 3, um spike mede o quadro no modo de render atual (tela cheia) e testa o modo parcial com gravação só da área no painel QSPI. Se nenhum ficar abaixo de 40 ms: menos degraus (1 troca/s) | §4 |
| App passa de 2,8 MB | Fontes e imagens novas | Não cabe no slot OTA | Remoção do Clawd (~190 KB), box drawing reduzido, `build.sh` falha acima do limite | §6 |
| Cota e visibilidade | Cada conta consultada a cada intervalo | ~480 requisições/h com 4 contas a 30 s, visíveis aos admins. Cada uma custa `max_tokens: 1` e poucos tokens de entrada | Aceito pelo dono. Status global e sonda feita junto com a consulta da conta em foco, sem requisição extra | §2 |
| Tokens OAuth em claro | Sessão desbloqueada | Até 4 tokens na RAM; cópia residual no `HTTPClient` até o `end()` | Tabela sob mutex, buffers de stack zerados, zeragem ao remover, trocar ou apagar tudo | §1, §7 |
| Resultado atrasado de conta removida | Remover ou trocar token durante um ciclo | Dado na conta errada | Contador `gen` por slot | §1 |
| Task travada | Requisição que não respeita os timeouts | Dados param de atualizar | Reinício com `esp_restart()` quando uma requisição HTTPS passa de 90 s (o pior caso que respeita os timeouts é ~39 s), com o motivo gravado na NVS | §4 |
| Falsificação de contagem na LAN | Alguém na mesma rede | Contagem de tokens errada na tela | Aceito (já existe na v2.3); a % de uso vem da API | §7 |

--------------------------------------------------------------------------------

## Non-Functional Requirements

*   **Segurança e privacidade:**
    *   Regras de tokens OAuth em [§7](#7-segurança).
    *   Nenhum token em log, em `/health` ou em `/window`.
*   **Desempenho**, medido pelo `/health` (§9):
    *   **Toque:** responde em até 100 ms durante um ciclo de consultas.
    *   **Quadros:** o tempo de quadro com o pulso ligado fica abaixo de 40 ms.
    *   **RAM interna:** pelo menos 60 KB livres, e um bloco contíguo de 45 KB ou mais, durante um handshake TLS com a tela inicial de 4 contas.
    *   **Flash:** app abaixo de 2,8 MB (§6).
    *   **Gravações:** cada conta grava o histórico no máximo a cada 5 min, e também no reset da janela de 5h. Com 4 contas a 30 s, são 48 gravações por hora, contra 480 se gravasse a cada consulta.
*   **Rollout e rollback:**
    *   O `FW_VERSION` passa a `3.0`.
    *   As partições e o formato `HistFileV2` não mudam, e nenhuma chave NVS é apagada (`slide` e `acct` continuam).
    *   Rollback = gravar só a partição de app da v2.3, nunca `erase_flash`.
    *   A tag `v2.3` é criada antes da fase 0.

--------------------------------------------------------------------------------

## Additional Resources

*   [Brief do brainstorm](./notes/concepts_brief.md).
*   [Drift report dos READMEs](./notes/2026-11-08_DRIFT_REPORT.md).
*   **Mockups aprovados:** [canvas no claude.ai](https://claude.ai/artifact/JeUby4eTTN3HA5PsYaaimR), com cópias locais em [mockup-home.dc.html](./review/context/mockup-home.dc.html) e [mockup-canvas.json](./review/context/mockup-canvas.json).
*   **Design system THEP:** [artifact no claude.ai](https://claude.ai/artifact/RQgM1nHRZn5UqPRrHWdy3a), com cópias locais em [README](./review/context/thep-design-system-README.md) e [tokens](./review/context/thep-design-system-tokens.json).
*   **Referência visual do Hermes Agent:** [repositório](https://github.com/NousResearch/hermes-agent).
    *   [`hermes_cli/banner.py`](./review/context/hermes-banner.py.txt): `HERMES_AGENT_LOGO` em ANSI Shadow, `HERMES_CADUCEUS` em braille e `Panel` com título na borda.
    *   [`hermes_cli/skin_engine.py`](./review/context/hermes-skin_engine.py.txt): papéis de cor `banner_title/accent/dim/text/border`.

## Future Improvements

*   **Polling adaptativo por conta:** espaçar até 5 min as contas paradas e voltar ao mínimo as que passam de 80%. Hoje está fora do escopo por decisão do dono.
*   **Reaproveitar a conexão TLS entre contas** (`setReuse(true)`, trocando só o `Authorization`). Encurta o ciclo.
*   **Tema claro** com os papéis `light` do design system THEP.

--------------------------------------------------------------------------------

## Detailed Implementation Plan

Todos os caminhos são relativos à raiz do repositório. Os arquivos novos de firmware ficam em `firmware/claude_stick/src/`, que o arduino-cli compila de forma recursiva; a raiz do sketch fica só com o `.ino` e os arquivos de configuração. Os itens estão numerados na ordem do documento, que segue as fases do Technical Plan. Cada fase termina com o firmware compilado e gravável.

**Versões fixadas:** core `esp32:esp32` **3.3.8** (a versão instalada, contra a qual as APIs deste plano foram conferidas), LVGL 9.2.2, GFX Library for Arduino 1.6.5 e `lv_font_conv` 1.5.3. O README, o `build.sh:12` e o `firmware/REFERENCIA-HARDWARE-LVGL.md` ainda citam 3.3.11 e são corrigidos nos itens 46 e 47.

**Regra de dependências entre módulos:** `src/logic/*` só inclui headers C/C++ padrão e outros headers de `src/logic/`, para compilar no computador (item 13). Por isso, nenhuma função de `src/logic/` lê constantes do `config.h`: os valores entram como parâmetro (por exemplo, `freshSec` no item 10 e `probeModel` no item 11). `src/net/*` nunca chama `lv_*` nem funções de `src/ui/` ou `src/app/`: o que precisa da interface passa por hooks registrados pelo `.ino` (fase 2) ou pelo `app_state` (fase 3), descritos nos itens 18 e 19. Nas fases 0 a 2, os módulos extraídos do `.ino` podem declarar `extern` das globais movidas e das globais que ficam no `.ino`, listadas em cada item; as globais que ficam no `.ino` e são lidas por um módulo perdem o `static`. A partir da fase 3, as telas só leem o modelo por funções e chamam funções de intenção (item 25), sem `extern` de variável mutável.

### Fase 0 — Linha de base e divisão do `.ino` (sem mudança visual)

#### 1. Tag `v2.3` no git (Create)

*   **Type / Scope**: Controle de versão.
*   **Proposed Changes**:
    *   Criar a tag anotada `v2.3` no commit atual da `main` e arquivar o `.bin` compilado (anexo na release do GitHub).
    *   Anotar a saída "Sketch uses … bytes" e "Global variables use …" do `./build.sh`, o número de warnings da compilação e o heap livre na tela principal. Esses números vão para o PR da fase 0 como linha de base.
    *   A v2.3 não imprime o heap. Para medir, usar um build local temporário, que não entra no commit, com `Serial.printf("[HEAP] int=%u big=%u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL), heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL))` dentro de `dash_tick()` (ino:1515), e anotar o menor valor depois de 5 min em `ST_MAIN`.

#### 2. `firmware/claude_stick/claude_stick.ino` (Modify)

*   **Type / Scope**: Sketch principal (2.906 linhas hoje).
*   **Proposed Changes**:
    *   Na fase 0, só saem do `.ino` o estado das contas (item 3), o histórico (item 5), o servidor web (item 6) e os arquivos já separados (item 7). Tudo é movido **sem mudar comportamento**. As globais movidas perdem o `static` e passam a ser declaradas `extern` nos headers dos itens 3, 5 e 6. Na fase 0, ficam no `.ino`, mas perdem o `static` porque os módulos extraídos as leem: `g_accts` (ino:97; lida por `hist_path` e pelos handlers web), `g_tzOffset` (ino:133; lida por `day_key`), `g_pendingToken` (ino:102) e `g_pendingLabel` (ino:110). Na fase 2 entra na lista `g_tokenTargetSlot` (ino:109), que o `POST /token` põe no `VALIDATE` (item 6). `g_prefs` e `g_wifi` já são globais sem `static` (ino:63-64).
    *   A máquina de estados (item 25) e as telas (item 39) saem na fase 3. Ao fim da fase 3, o `.ino` fica só com `setup()`, `loop()`, os includes e a criação do display, do touch e do LVGL.
    *   A partir da fase 2, o `loop()` faz, nesta ordem, **em todos os estados** (antes e depois do desbloqueio):
        1. `lv_timer_handler()`;
        2. `web_api_poll()` (item 6);
        3. `bool changed = poller_apply_results()` (item 18). Roda também no onboarding, porque o resultado do `VALIDATE` precisa ser aplicado antes de existir sessão;
        4. `scheduler_tick()` (item 19), que sai na hora enquanto `scheduler_start()` não foi chamado;
        5. `poller_check_stuck()` (item 18);
        6. o tick de interface: na fase 2, `if (changed) g_uiDirty = true` e o código de tick que já existe no `loop()`; na fase 3, `app_state_tick(changed)` (item 25).

#### 3. `firmware/claude_stick/src/model/account_state.{h,cpp}` (Create)

*   **Type / Scope**: Modelo de dados por conta.
*   **Proposed Changes**:
    *   **Fase 0:** recebe as globais de limiar (`g_thrFired`, `g_thrPrev`, `g_thrBase`, ino:1698-1700), a contagem de tokens (`TokenStats`, `g_tok` e `TOK_FRESH_MS`, ino:92-94) e `g_usage` (ino:77), com o mesmo comportamento. O header declara `extern` dessas globais até a fase 1. `g_pendWin`/`g_pendThr` (ino:1697) são estado de tela e ficam no `.ino` até a fase 2 (item 24).
    *   **Fase 1:** define `struct AccountState` com os campos da tabela do Technical Plan §3 e `AccountState* acct`, alocado em PSRAM com `heap_caps_calloc(ACCT_MAX, sizeof(AccountState), MALLOC_CAP_SPIRAM)`. O registro **não** tem `used` nem `label`: quem precisa deles lê `g_accts.used[slot]` e `g_accts.label[slot]`, que `accountSave`, `accountSetLabel` e `accountRemove` já mantêm. Assim não há duas fontes da verdade nem regra de sincronia.
    *   **Fase 1, limiares:** `THR` (ino:1686), `struct ThrHit { int8_t win; uint8_t thr; }` e `ThrHit check_thresholds(AccountState&)` vêm para cá, porque a função só mexe no estado `thr` da conta e não usa LVGL. Ela devolve `{win, thr}` do cruzamento, ou `{-1, 0}`. Se as duas janelas cruzam na mesma consulta, vale o maior limiar; em empate, a de 5h. Na fase 1, o `.ino` chama `ThrHit h = check_thresholds(acct[g_accts.active])` onde hoje chama `check_thresholds()` e, se `h.win >= 0`, grava `g_pendWin = h.win; g_pendThr = h.thr`.
    *   **Fase 2, modelos e status:** `NMODELS`, `struct ModelInfo` e `g_models[]` (ino:81-88) e `g_status` (ino:78) vêm para cá, com `extern` no header (regra das fases 0 a 2). `g_probeIdx` (ino:89) sai, porque o rodízio passa ao agendador (`probeIdx`, item 19). `ProbeResult` continua em `api.h`, mesmo com `probeModel` removido (item 20). Na fase 3, as telas leem por `const ModelInfo& models_get(int i)` e `const ModelStatus& status_get()`.
    *   Funções:
        *   `acct_init()`;
        *   `acct_apply_result(slot, gen, const NetResult&)` (fase 2, quando `NetResult` passa a existir no item 18), que descarta um resultado `USAGE` com `gen` velho e aplica as regras do item 18;
        *   `acct_bump_gen(slot)`;
        *   `acct_clear(slot)`, que zera o registro com `memset`, **preservando o `gen`** (guarda, zera e restaura). Assim, um slot reaproveitado depois de remover continua com `gen` maior que o de qualquer resultado atrasado da conta removida (§1). O token OAuth não fica no `AccountState`: quem o zera é `token_store_clear` (item 16);
        *   `acct_reset_runtime(slot)`, que volta a conta a `pendente` (`usage`, `usageAtEpoch`, `lastClass = FC_NONE`, `fails`, `backoffCycles` e `thr` zerados) sem mexer no histórico.
    *   **Conta em foco:** não há função nova. O foco é `AccountSlots.active`, lido por `accountsLoad` (`accounts.cpp:71-75`), gravado na chave NVS `acct` por `accountSetActive` (`accounts.cpp:124-128`) e movido por `accountRemove` quando a conta em foco é removida (`accounts.cpp:116-121`).
    *   `FetchClass` vem de `src/logic/fetch_class.h` (item 4).

#### 4. `firmware/claude_stick/src/logic/fetch_class.h` (Create)

*   **Type / Scope**: Tipo compartilhado entre lógica pura, rede e modelo.
*   **Proposed Changes**:
    *   `enum FetchClass : uint8_t { FC_NONE, FC_OK, FC_AUTH, FC_RATE, FC_ERR, FC_SRV, FC_NET, FC_NOWIFI, FC_NOCLOCK, FC_DISCARDED };`
    *   Só inclui `<stdint.h>`, para que `dot_state.h` e `classify.h` compilem no computador. `account_state.h` inclui este header, e não o contrário.
    *   `FC_DISCARDED` nunca entra no `AccountState`.
    *   `inline const char* fetch_class_name(FetchClass c)`: devolve o nome sem o prefixo `FC_` (`"NONE"`, `"OK"`, `"AUTH"`, `"RATE"`, `"ERR"`, `"SRV"`, `"NET"`, `"NOWIFI"`, `"NOCLOCK"`, `"DISCARDED"`). É usada no card, nos logs (item 18) e, em minúsculas, no `reason` de `/token/status` (item 6).
    *   Na tela, a classe aparece sem o prefixo `FC_` (`AUTH`, `NET`…), igual em português e inglês.

#### 5. `firmware/claude_stick/src/model/history.{h,cpp}` (Create)

*   **Type / Scope**: Histórico, heatmap e persistência em LittleFS.
*   **Proposed Changes**:
    *   **Fase 0:** recebe os tipos `HIST_MAX`/`Sample` (ino:140-141) e `NDAYS`/`DayHeat` (ino:149-150), as globais `g_hist`, `g_histN`, `g_histHead`, `g_hourBurn`, `g_lastH5`, `g_days` e `g_dayN` (ino:142-152) e, de `claude_stick.ino:930-1078`, as funções `hist_push` (:933), `hist_idx` (:941), `day_key` (:944), `day_slot` (:951), `accumulate_heat` (:965), `heat_mode_data` (:980), `HistFileV1/V2` (:997-1001), `hist_path` (:1002), `reset_history_ram` (:1005), `save_history` (:1013), `load_history` (:1027) e `copy_file` (:1063). A migração `/hist.bin → /hist0.bin` de `setup()` (ino:2770-2782) vira `hist_migrate_legacy()`, chamada no mesmo ponto do `setup()`. `history.cpp` declara `extern` de `g_accts` (`hist_path`) e `g_tzOffset` (`day_key`), que ficam no `.ino` (item 2).
    *   **Fase 1**, assinaturas por conta (o estado fica no `AccountState`; `Sample` e `HIST_MAX` passam a vir de `src/logic/sample.h`, item 9). Os chamadores do `.ino` passam `acct[g_accts.active]`, e o `load_history()` do `setup()` (ino:2783) vira `hist_load(g_accts.active, acct[g_accts.active])`:
        *   `void hist_push(AccountState&, float h5, float d7, uint32_t now)`;
        *   `void accumulate_heat(AccountState&, float h5, uint32_t now, int32_t tzOffsetSec)`;
        *   `void heat_mode_data(const AccountState&, int mode, uint32_t now, int32_t tzOffsetSec, float out[24])`;
        *   `void hist_path(int slot, char* out, size_t)`, que deixa de ler `g_accts.active`;
        *   `void hist_load(int slot, AccountState&)` e `void hist_save(int slot, const AccountState&)`;
        *   `void hist_reset_ram(AccountState&)`;
        *   `const Sample* hist_data(const AccountState&, int* n, int* head)`.
    *   O formato `HistFileV2` continua **byte a byte igual** (G8).
    *   **Fase 2**, regra de gravação: `bool hist_save_due(const AccountState&, uint32_t now)` devolve verdadeiro quando `now - lastSaveEpoch >= HIST_SAVE_MIN_SEC` ou `usage.h5ResetEpoch != lastSavedReset`. Depois de gravar, `hist_save` atualiza `lastSaveEpoch` e `lastSavedReset`, que são campos novos do `AccountState` (Technical Plan §3).
    *   **Fase 2:** a chamada de `hist_load` sai do `setup()`. Os históricos de todos os slots salvos são carregados no passo 4 do desbloqueio (item 23).
    *   **Remover conta** apaga `/histN.bin`, como a v2.3 faz em `acct_del_cb` (ino:2393-2394). Nada grava o arquivo depois, porque os resultados atrasados são descartados pelo `gen`. **Trocar o token** mantém o histórico do slot. **"Apagar tudo"** apaga os 4 arquivos, como hoje (`factory_reset`, ino:354-357).

#### 6. `firmware/claude_stick/src/app/web_api.{h,cpp}` (Create)

*   **Type / Scope**: Servidor HTTP local (onboarding, `/window`, `/tokens`, `/token/status`, `/health`). Fica em `src/app/`, e não em `src/net/`, porque conversa com o modelo e com a tela.
*   **Proposed Changes**:
    *   **Fase 0**, recebe do `.ino` (intervalos 646-835 e 876-882, menos o que fica no `.ino`):
        *   `g_web`, `g_tokenGot`, `stop_web` (:653), `g_mdnsUp` e `ensure_mdns` (:656);
        *   `WEB_CSS` e `WEB_SPARK` (:683-703), `web_form` (:705) e `web_result` (:718);
        *   os handlers `handleRoot`, `handleNotFound`, `handleTokenPost`, `handleWindow`, `handleTokensPost` e `handleInfo` (:735-825), com `jll` e `jstr` (:769-783);
        *   `start_data_web` (:826-835) e o trecho de `ui_token` que cria o servidor do onboarding (:876-882), que vira `web_start_onboarding()`.
    *   **Ficam no `.ino`**, embora estejam dentro do intervalo, porque usam LVGL: `g_tokMsg` (:651), `anim_opa_cb` (:665) e `build_claude_mark` (:668-680), usados por `ui_token`, `ui_loading` e `ui_about`.
    *   **Interface com o resto na fase 0:** os handlers leem `g_usage`, `g_accts`, `g_tok`, `g_pendingToken` e `g_pendingLabel` por `extern`. O `loop()` lê `g_tokenGot` por `extern` (`web_api.h`), no lugar do `if (g_web)` de ino:2803. O que mexe na tela ou no estado do PIN passa por callbacks registrados pelo `.ino`, para que `web_api.cpp` não chame `lv_*` nem precise de `extern` de `g_pinEntry`/`g_pinFirst`/`g_pinConfirming` (ino:111-113):
        *   `web_on_token_msg(void (*)(const char*))`, que troca o texto de `g_tokMsg` (hoje `lv_label_set_text` + `lv_refr_now`, ino:745-763);
        *   `web_on_tok_stats(void (*)())`, que chama `update_tok_row()` (ino:814);
        *   `web_on_token_ok(void (*)())`, chamado por `handleTokenPost` no lugar de ino:757; o `.ino` registra uma função que faz `g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0;`.
    *   `void web_api_poll()` chama `g_web->handleClient()` quando há servidor. `ensure_mdns()` é declarada em `web_api.h`.
    *   **Fase 1:** o `web_api.cpp` muda junto com o item 14, porque `g_usage` e `g_tok` deixam de existir:
        *   `handleWindow` lê `acct[g_accts.active].usage`;
        *   `handleTokensPost` grava em `acct[g_accts.active].tok`, ainda com a regra do 409 da v2.3;
        *   `handleTokenPost` guarda o `UsageData` validado em `g_validatedUsage` (nova global de `web_api.cpp`, com `extern` em `web_api.h`), no lugar de `g_usage = tmp` (ino:756). `finalize_pending_token` copia esse valor para `acct[g_tokenTargetSlot].usage` antes do `hist_push` (ino:1128). A global sai na fase 2, com o fluxo assíncrono.
    *   **Fase 2 — servidores e rotas.** Depois da fase 2, `stop_web()` só é chamado dentro de `web_start_onboarding()` e `web_start_session()` (antes de criar o servidor novo) e por `factory_reset`. `render_state()` e `ui_token()` deixam de chamá-lo (item 24).

        | Servidor | Quando nasce | Rotas |
        |---|---|---|
        | Onboarding (`web_start_onboarding()`) | Ao abrir `ST_TOKEN` antes do desbloqueio, se ainda não existir. Continua vivo em `ST_SETUP_PIN`, para a página ler o `ok` | `/` → `handleRoot` (formulário), `POST /token`, `GET /token/status`, `onNotFound` → `handleNotFound` (redirect 302 para `/`, como hoje) |
        | Sessão (`web_start_session()`) | No passo 4 do desbloqueio (item 23), no lugar do servidor do onboarding. Fica ligado a sessão inteira | `/` → `handleRoot` (formulário), `/info` → `handleInfo`, `GET /window`, `POST /tokens`, `POST /token`, `GET /token/status`, `/health` (só `THEP_DEV`), `onNotFound` → 404 `{"error":"not_found"}` (como o servidor de dados de hoje, ino:833) |

    *   **Fase 2 — estado da validação**, todo dentro de `web_api.cpp`: `state` (`idle`, `pending`, `ok`, `rejected`), `reason`, `accepting` e `seq` (contador `uint16_t` da validação atual). Funções novas em `web_api.h`:
        *   `void web_token_begin()`: chamada quando a tela de token abre (`ST_TOKEN`; na fase 3, o conteúdo `TOKEN`). Liga `accepting`;
        *   `void web_token_reset()`: chamada quando a tela de token fecha, por qualquer caminho. Desliga `accepting`. Se o estado for `pending`, também zera o token em validação (`token_store_clear_pending()`), faz `seq++` (o resultado do `VALIDATE` em voo passa a ser ignorado) e volta a `idle`. Se for `ok` ou `rejected`, o estado fica como está, para a página ainda conseguir lê-lo;
        *   `uint16_t web_token_seq()`;
        *   `void web_token_set_result(bool ok, const char* reason)`: grava `ok`, ou `rejected` com o `reason`.
    *   **Fase 2 — fluxo do token, onboarding e sessão:**
        1.  `poller_start()` (item 18) é chamado em `setup()`, logo depois de `g_wifi.begin()`. O `VALIDATE` precisa da `net_task` já no onboarding. Antes do desbloqueio, o agendador não roda (item 19), e o único job possível é o `VALIDATE`.
        2.  `POST /token` (form `label`, `token`):
            *   `accepting` desligado (tela de token fechada) → 403 `{"state":"rejected","reason":"closed"}`;
            *   token com menos de 8 caracteres → 400 `{"state":"rejected","reason":"short"}`;
            *   token com mais de 199 caracteres (o limite de `tokens[ACCT_MAX][200]`) → 400 `{"state":"rejected","reason":"long"}`;
            *   já existe uma validação em andamento (`state == pending`) → 409 `{"state":"pending"}`;
            *   caso contrário: guarda o rótulo em `g_pendingLabel`, copia o token com `token_store_set_pending(tok)` (item 16), faz `seq++`, enfileira `VALIDATE` com `slot = g_tokenTargetSlot` e `gen = seq`, passa o estado a `pending` e responde **202** `{"state":"pending"}`. O `gen` do `VALIDATE` é o `seq` da validação, e não o `gen` da conta: o `VALIDATE` lê o token em validação, e não a tabela.
        3.  `GET /token/status` devolve `{"state":"idle|pending|ok|rejected","reason":"…"}`. `reason` só aparece em `rejected`, com `closed`, `short`, `long`, `crypto` ou a classe em minúsculas (`auth`, `rate`, `err`, `srv`, `net`).
        4.  **Resultado do `VALIDATE`**, tratado pelo hook `onValidate` (item 18), registrado pelo `.ino` na fase 2 e pelo `app_state` na fase 3:
            *   `r.gen != web_token_seq()` ou `FC_DISCARDED` → ignorado. O `FC_DISCARDED` só acontece quando o token em validação já foi zerado por `web_token_reset()`;
            *   qualquer classe diferente de `FC_OK` → `token_store_clear_pending()` e `web_token_set_result(false, <classe em minúsculas>)`. Um 401 com headers, que a v2.3 aceitava, passa a ser `rejected` (`auth`);
            *   `FC_OK` → passo 7 ou 8. O `UsageData` do `VALIDATE` não entra no modelo nem no histórico: a conta fica `pendente` até o primeiro `USAGE` (CUJ 4).
        5.  O estado volta a `idle` quando chega um novo `POST /token` aceito, ou quando a tela de token fecha com uma validação em andamento (`web_token_reset()`).
        6.  **JS da página** (dentro de `web_form()`): intercepta o submit, envia o formulário com `fetch('/token', {method:'POST', body:new URLSearchParams(new FormData(form))})` e depois consulta `GET /token/status` a cada 1 s, por no máximo 30 s. Textos mostrados (pt/en no item 32):
            *   `validando…` enquanto o estado é `pending`;
            *   em `ok`: "Token validado. Defina o PIN no aparelho." no onboarding e "Conta salva." na sessão;
            *   em `rejected`: com `closed`, "Abra a tela de token no aparelho e envie de novo."; com `crypto`, "Falha ao cifrar no aparelho. Tente de novo."; nos outros casos, "A API recusou o token (<reason>). Confira e cole de novo.";
            *   em 409: "Já existe uma validação em andamento.";
            *   `idle` lido depois do envio (a tela fechou no meio da validação), ou 30 s sem estado final: "O aparelho não respondeu. Tente de novo."
        7.  **`ok` no onboarding** (`!session_unlocked()`): `web_token_set_result(true, nullptr)` e, se a tela ainda for `ST_TOKEN`, vai para `ST_SETUP_PIN`, como hoje (ino:2807, onde a checagem `g_onboarding || !g_sessionPin[0]` vira `!session_unlocked()`). O servidor do onboarding continua vivo, e o token validado continua na área de validação até o `ST_SETUP_PIN` (item 23).
        8.  **`ok` numa sessão desbloqueada** (adicionar ou trocar conta), no lugar de `finalize_pending_token()` (ino:1102-1132). `slot = g_tokenTargetSlot`, que é o primeiro slot livre ao adicionar (`acct_add_cb`, ino:2416) ou o slot da conta ao trocar (Ajustes, ino:2223):
            1.  `replacing = g_accts.used[slot]`, lido antes de qualquer gravação (como ino:1109);
            2.  `token_store_take_pending(buf)` copia o token em validação para um buffer de stack e zera a área;
            3.  `encryptTokenWithKey(buf, g_sessionKey, blob)` (item 23). Se falhar: `memset(buf)`, `g_pendingLabel[0] = 0`, `web_token_set_result(false, "crypto")` e fim, sem gravar nada (como ino:1104-1107);
            4.  `accountSave(g_prefs, g_accts, slot, blob, rótulo)`, onde o rótulo é `g_pendingLabel` ou, ao trocar sem rótulo novo, o rótulo atual do slot (ino:1112-1113);
            5.  `accountSetActive(g_prefs, g_accts, slot)`: a conta adicionada ou trocada ganha o foco, como na v2.3 (ino:1115-1116);
            6.  `acct_bump_gen(slot)`, `acct[slot].keyOk = true`;
            7.  `token_store_set(slot, acct[slot].gen, buf)` e `memset(buf, 0, …)`;
            8.  se `!replacing` (adicionar): `hist_reset_ram(acct[slot])` e `hist_load(slot, acct[slot])`; ao trocar, o histórico fica como está;
            9.  `acct_reset_runtime(slot)`: a conta fica `pendente` (`consultando…`);
            10. `g_pendingLabel[0] = 0` e `web_token_set_result(true, nullptr)`;
            11. navega para a tela inicial: `request_state(ST_MAIN)` na fase 2, `intent_open(HOME)` na fase 3. Ao adicionar, a tela inicial é recriada com o novo layout;
            12. `scheduler_request_refresh()` (item 19): se não houver ciclo em andamento, um ciclo começa na hora. Senão, a conta entra no próximo ciclo.
    *   **Fase 2 — `GET /health`**, só com `-DTHEP_DEV`, com os campos do Technical Plan §9. Fonte de cada métrica:

        | Campo | Fonte | Módulo dono |
        |---|---|---|
        | `uptime_s` | `millis() / 1000` | `web_api.cpp` |
        | `heap_internal_free` / `heap_internal_largest` | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` / `heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)` | `web_api.cpp` |
        | `psram_free` | `heap_caps_get_free_size(MALLOC_CAP_SPIRAM)` | `web_api.cpp` |
        | `lv_mem_used_pct` | `lv_mem_monitor(&m); m.used_pct` (lido no `loop()`, que é dono do LVGL) | `web_api.cpp` |
        | `net_stack_hwm` | `uxTaskGetStackHighWaterMark(poller_task_handle())`, em bytes no ESP-IDF | `poller.cpp` |
        | `last_cycle_ms` / `cycles_over_interval` | `scheduler_last_cycle_ms()` / `scheduler_cycles_over_interval()` (ciclos mais longos que o intervalo) | `scheduler.cpp` |
        | `frame_ms_max` | callbacks de `LV_EVENT_REFR_START` e `LV_EVENT_REFR_READY` no display (`lv_display_add_event_cb`), que medem a passada inteira, inclusive o flush | `.ino` (fase 2), `shell.cpp` (fase 3) |
        | `slots` | `acct[]`: `{slot, class, age_s, backoff}` | `web_api.cpp` |

    *   **Fase 4:**
        *   `/window` ganha `?account=<rótulo>`: devolve os campos dessa conta, 404 se o rótulo não existe e, para uma conta sem dados ainda, os campos numéricos em 0 com `"pending":true`;
        *   `/tokens` passa a rotear pelo rótulo: com `account`, vai para o slot cujo rótulo é igual (`strcmp`, diferenciando maiúsculas, ao contrário do `makeUnique` de `accounts.cpp:35`); sem `account`, vai para a conta em foco; rótulo desconhecido devolve 404. O 409 `account_mismatch` (ino:798-805) sai.
    *   O CSS das páginas troca o coral pelas cores THEP na fase 3.

#### 7. `firmware/claude_stick/src/net/api.{h,cpp}`, `status.{h,cpp}`, `certs.{h,cpp}`, `src/sec/crypto.{h,cpp}`, `src/model/accounts.{h,cpp}`, `src/hw/wifi_manager.h`, `src/hw/touch.h` (Move)

*   **Type / Scope**: Arquivos já separados hoje.
*   **Proposed Changes**:
    *   São movidos da raiz do sketch para `src/`, sem mudança de conteúdo na fase 0, e os includes são ajustados.
    *   `wifi_manager.h` e `touch.h` vão para `src/hw/`.

#### 8. `firmware/claude_stick/build.sh` (Modify)

*   **Type / Scope**: Script de build.
*   **Proposed Changes**:
    *   Acrescentar `-I${SKETCH_DIR}/src` ao `LVFLAGS`.
    *   Passar as flags também em `compiler.S.extra_flags`, como já faz `firmware/bringup/build.sh:48`.
    *   Compilar com `--output-dir "$SKETCH_DIR/build"`, que o `.gitignore` já ignora (`firmware/*/build/`).
    *   Novos comandos `dev` e `upload-dev`: iguais a `build` e `upload`, com `-DTHEP_DEV` acrescentado ao `LVFLAGS`. A mensagem de uso passa a listar `build | upload | dev | upload-dev | monitor`.
    *   **Checagem de tamanho:** depois de compilar, `size=$(wc -c < "$SKETCH_DIR/build/claude_stick.ino.bin")`, imprime o valor e sai com erro se passar de `2936012` bytes (2,8 MB).

### Fase 1 — Lógica pura e testes no computador

#### 9. `firmware/claude_stick/src/logic/sample.h`, `projection.{h,cpp}` (Create)

*   **Type / Scope**: Função pura, sem Arduino nem LVGL.
*   **Proposed Changes**:
    *   `sample.h` recebe `struct Sample { uint32_t t; uint8_t h5; uint8_t d7; }` (`claude_stick.ino:141`) e `HIST_MAX 160`.
    *   `ProjResult projection(const Sample* hist, int n, int head, float pctNow, uint32_t resetEpoch, uint32_t now)`, onde `hist` é o ring buffer, `n` o número de amostras e `head` a próxima posição de escrita. A amostra `i` (0 = mais antiga) fica em `(head - n + i + 2*HIST_MAX) % HIST_MAX`, como em `hist_idx` (ino:941).
    *   `ProjResult { enum {SEM_DADOS, OK_ATE_RESET, ESGOTA, ESGOTADA} st; uint32_t etaEpoch; int minutes; int pctAtReset; float rate; }`. `rate` é a taxa da regra 4 (%/min, 0 quando não calculada); `/janela` usa esse campo para decidir se desenha a linha de projeção (item 39).
    *   Regras, avaliadas nesta ordem (`ws = resetEpoch - 5*3600`):
        1.  `now < 1e9` ou `resetEpoch == 0` → `SEM_DADOS`;
        2.  `pctNow >= 99.5` → `ESGOTADA`, com `etaEpoch = resetEpoch` e `minutes = (resetEpoch - now) / 60`;
        3.  `k` = número de amostras com `t != 0` e `ws <= t <= now`, mais 1 (o ponto atual, como em ino:1569-1587). Se `k < 3` → `SEM_DADOS`;
        4.  taxa como em ino:1596-1609: a primeira amostra com `t >= ws` e `t >= now - 2700`; se ela existe e `now > t + 300`, `rate = (pctNow - h5) / ((now - t) / 60)`, senão `rate = 0`;
        5.  se `rate > 0.02` (ino:1622) e `eta = now + (100 - pctNow) / rate * 60 <= resetEpoch` → `ESGOTA`, com `etaEpoch = eta` e `minutes = (eta - now) / 60`;
        6.  senão → `OK_ATE_RESET`, com `etaEpoch = resetEpoch`, `minutes = (resetEpoch - now) / 60` e `pctAtReset = round(pctNow + max(rate, 0) * minutes)`, limitado a 0..100.
    *   `void projection_text(const ProjResult&, uint8_t lang, int32_t tzOffsetSec, int maxChars, char* out, size_t)`:
        *   a hora vem de `gmtime_r(etaEpoch + tzOffsetSec)`, e não de `localtime`, para o teste não depender do fuso da máquina. O firmware passa `g_tzOffset * 3600`; os testes passam 0;
        *   tempo até esgotar: `"%dm"` abaixo de 60 minutos e `"%dh%02dm"` a partir de 60;
        *   se o texto passar de `maxChars` code points, o trecho ` · <tempo>` sai (layout de 3 contas);
        *   textos exatos na tabela do item 32: `✓ não esgota · ~78%`, `✗ esgota 15:20 · 38m`, `✗ esgotada`, `— sem histórico`.

#### 10. `firmware/claude_stick/src/logic/dot_state.{h,cpp}` (Create)

*   **Type / Scope**: Função pura.
*   **Proposed Changes**:
    *   `DotState dot_state(const AccountView* v, int n, uint32_t now, uint32_t cycleEndEpoch, bool firstCycleRunning, uint32_t freshSec)`. O firmware passa `DOT_FRESH_SEC` (item 22) em `freshSec`; os testes passam 10. `v` tem uma entrada por slot salvo (`g_accts.used[slot]`), em ordem de slot.
    *   `AccountView { bool keyOk; FetchClass lastClass; }`. Em todo o plano, `pendente` é o estado da §4, e `pending` só aparece como nome de campo ou variável.
    *   Derivados por conta:
        *   `pending = keyOk && lastClass == FC_NONE`. Um slot `SEM CHAVE` nunca é pendente, porque nunca vai ser consultado;
        *   `failed = !keyOk || lastClass ∈ {FC_AUTH, FC_RATE, FC_ERR, FC_SRV, FC_NET, FC_NOWIFI, FC_NOCLOCK}`.
    *   Regras, nesta ordem:
        1.  `np` = contas com `!pending`; `nf` = contas com `!pending && failed`;
        2.  `DOT_RED` se `np > 0 && nf == np`;
        3.  `DOT_YELLOW` se `nf > 0`;
        4.  `DOT_BLUE` se `firstCycleRunning || (cycleEndEpoch != 0 && now - cycleEndEpoch < freshSec)`;
        5.  `DOT_GREEN` nos outros casos.
    *   Com `n = 0` ou todas as contas `pendente`, nunca há vermelho: o ponto é azul durante o primeiro ciclo e verde depois.

#### 11. `firmware/claude_stick/src/logic/classify.{h,cpp}` (Create)

*   **Type / Scope**: Funções puras de classificação.
*   **Proposed Changes**:
    *   `FetchClass classify_usage(int http, bool hasUnifiedHeaders)`, nesta ordem:
        1.  `http <= 0` → `FC_NET`. O `fetchUsage` só devolve valores negativos em erro de transporte (item 20); o 0 fica reservado para "nunca sondado" em `/modelos` (item 38) e, se aparecer aqui, também é `FC_NET`;
        2.  `http == 401 || http == 403` → `FC_AUTH`, com ou sem headers;
        3.  `hasUnifiedHeaders && (http == 200 || http == 429)` → `FC_OK`;
        4.  `http == 429` → `FC_RATE`;
        5.  `500 <= http <= 599` → `FC_SRV`;
        6.  qualquer outro código (outros 4xx, 3xx, 200 sem headers) → `FC_ERR`.
    *   `bool probe_needs_retry(bool hasUnifiedHeaders, const char* model, const char* probeModel)`: verdadeiro só se a resposta veio sem headers e `strcmp(model, probeModel) != 0`. O firmware passa `PROBE_MODEL` (`config.h:51`) em `probeModel`.
    *   `void model_status_text(int http, uint16_t ms, uint8_t lang, char* out, size_t, Tone* tone)`, que substitui `model_chip()` (ino:1313-1322) com os textos da tabela do item 38. `Tone { TONE_DIM, TONE_OK, TONE_WARN, TONE_BAD }` é declarado em `classify.h` e vira cor no item 33.

#### 12. `firmware/claude_stick/src/logic/levels.{h,cpp}` (Create)

*   **Type / Scope**: Funções puras.
*   **Proposed Changes**:
    *   `Level level_for(float pct)`: verde abaixo de 50, aviso de 50 a menos de 80 e vermelho a partir de 80.
    *   `void text_bar(float pct, int cells, char* out, size_t)`: `filled = (int)(pct * cells / 100 + 0.5f)`, limitado a 0..`cells`; escreve `filled` vezes `█` e o resto `░`, em UTF-8.
    *   `LayoutKind layout_for_count(int n)`: `LAYOUT_1..LAYOUT_4` para n de 1 a 4; n ≤ 0 → `LAYOUT_1`; n > 4 → `LAYOUT_4`.
    *   `void fit_label(const char* in, int maxChars, char* out, size_t)`: conta **code points** UTF-8. Se `in` tem mais que `maxChars`, corta e põe `…`, que conta como 1 dentro do limite.
    *   `void age_text(uint32_t ageSec, char* out, size_t)`: idade dos dados no card em falha. `"%us"` abaixo de 60 s, `"%um"` (minutos inteiros) abaixo de 3600 s e `"%uh"` (horas inteiras) a partir daí. Exemplos: 59 → `59s`, 60 → `1m`, 3599 → `59m`, 3600 → `1h`.
    *   `const char* status_mark(const char* unifiedStatus, uint8_t lang, char* buf, size_t, Tone* tone)`: marca de estado da API, no lugar de `overall_label`/`status_color` (ino:1137-1149), com a tabela "Marca de estado da API" do Technical Plan §5. Vazio ou `nullptr` → `""` e `TONE_DIM`; `allowed` → `ok`/`TONE_OK`; `rejected`, `rate_limited` ou `exceeded` → `limite`/`limit` e `TONE_BAD`; contém `warning` → `atenção`/`warning` e `TONE_WARN`; outro valor → o próprio valor cortado com `fit_label(…, 8, …)` em `buf`, com `TONE_DIM`. `Tone` vem de `classify.h`.

#### 13. `firmware/tests/` (Create)

*   **Type / Scope**: Testes no computador (g++, sem framework externo).
*   **Proposed Changes**:
    *   **`firmware/tests/run.sh`:**
        *   compila `firmware/claude_stick/src/logic/*.cpp` com os testes usando `g++ -std=c++17 -Wall -Wextra -Werror -I firmware/claude_stick/src` e roda;
        *   roda `grep -rn 'lv_' firmware/claude_stick/src/net/` e falha se achar alguma linha;
        *   sai com código diferente de zero em qualquer falha.
    *   **`firmware/tests/test_main.cpp`:** um `CHECK(expr)` mínimo que conta falhas.
    *   **`firmware/tests/test_projection.cpp`:**
        *   0, 1 e 2 amostras na janela (`SEM_DADOS`, porque `k < 3`);
        *   1 amostra com `pctNow = 99.6` → `ESGOTADA` (a regra 2 vem antes da 3);
        *   `pctNow` 99.4 e 99.5; `resetEpoch == 0`; `now < 1e9`;
        *   ring buffer com `head` no meio (amostras que dão a volta);
        *   primeira amostra com menos de 300 s → taxa 0 → `OK_ATE_RESET`;
        *   `rate` igual a 0.02 (não esgota) e logo acima;
        *   taxa que esgota antes do reset → `etaEpoch` e `minutes` certos;
        *   `pctAtReset` limitado a 100;
        *   textos exatos em pt e en, com `tzOffsetSec = 0`, 38 minutos (`38m`), 65 minutos (`1h05m`) e corte por `maxChars = 20`.
    *   **`firmware/tests/test_dot_state.cpp`:**
        *   uma linha por regra do item 10, inclusive empates de prioridade (amarelo vence azul);
        *   todas as contas `pendente` (azul no primeiro ciclo, verde depois);
        *   `n = 0`;
        *   uma `pendente` + uma em falha → vermelho;
        *   slot `SEM CHAVE` contando como falha, e nunca como `pendente`;
        *   `FC_NOWIFI` e `FC_NOCLOCK` em todas → vermelho;
        *   azul expirando em `freshSec = 10` (9 s azul, 10 s verde);
        *   `firstCycleRunning`.
    *   **`firmware/tests/test_classify.cpp`:**
        *   um caso por linha da tabela de classes da §4: 200 com headers, 429 com e sem headers, 401 e 403 com e sem headers (sempre `FC_AUTH`), 400, 404, 413, 500, 503, `-1`, `-11`, 200 sem headers, 302;
        *   `probe_needs_retry` com e sem headers e com `model` igual a `probeModel`;
        *   `model_status_text` para cada linha da tabela do item 38, em pt e en.
    *   **`firmware/tests/test_levels.cpp`:**
        *   `level_for` em 49.9, 50, 79.9 e 80;
        *   `text_bar` com 0%, 100%, 50% em 9 blocos (5 acesos) e 105% (limitado);
        *   `layout_for_count` de 0 a 5;
        *   `fit_label` com 16 caracteres ASCII, com acentos e com limite exato;
        *   `age_text` em 0, 59, 60, 3599 e 3600 s;
        *   `status_mark` com vazio, `allowed`, `allowed_warning`, `rejected`, `rate_limited`, `exceeded` e um valor desconhecido com mais de 8 caracteres, em pt e en.
    *   A detecção de `NOCLOCK` (prazo de 60 s) e a espera crescente dependem do relógio e do agendador. Elas são verificadas no aparelho (E2E 5 e 6), não nos testes de host. A classe `FC_NOCLOCK` em si entra no `test_dot_state.cpp`.

#### 14. `firmware/claude_stick/claude_stick.ino`, `src/model/account_state.cpp`, `src/model/history.cpp` e `src/app/web_api.cpp` (Modify)

*   **Type / Scope**: Troca das globais pelo modelo.
*   **Proposed Changes**:
    *   `AccountState[]` passa a ser a fonte única. As globais `g_usage`, `g_tok`, `g_hist*`, `g_hourBurn`, `g_lastH5`, `g_days`, `g_dayN` e `g_thr*` saem, junto com os `extern` temporários dos itens 3 e 5. `g_lastFetchOk`, `g_lastOkMs` e `g_pendWin`/`g_pendThr` ainda ficam no `.ino` nesta fase e saem na fase 2 (item 24).
    *   Cada leitor das globais removidas muda nesta mesma fase, para a fase compilar:
        *   as telas e os fluxos do `.ino` (`refresh_ui_values`, `dash_tick`, `update_tok_row`, `trend_redraw`, `heat_redraw`, `do_refresh`, `bg_refresh`, `finalize_pending_token`, `switch_account`, `acct_del_cb`, `factory_reset`, `ui_error` e os botões Voltar de ino:618, 845 e 2324) leem e gravam `acct[g_accts.active]`;
        *   `history.cpp` passa às assinaturas por conta do item 5;
        *   `check_thresholds` passa à versão por conta do item 3;
        *   `web_api.cpp` muda como descrito no item 6, "Fase 1".
    *   A tela continua mostrando só a conta em foco, igual à v2.3.
    *   `trend_redraw()` (ino:1542) passa a usar `projection()` e `projection_text()`.

### Fase 2 — Task de rede e todas as contas na tela inicial

#### 15. `firmware/claude_stick/lv_conf.h` (Modify) e `firmware/claude_stick/src/hw/lv_pool.{h,c}` (Create) — pool do LVGL na PSRAM

*   **Type / Scope**: Memória do LVGL. Vem antes da `net_task` (item 18), para liberar RAM interna para o TLS.
*   **Proposed Changes**:
    *   `lv_pool.h` declara `void* thep_lv_pool_alloc(size_t size);` com `extern "C"` quando compilado como C++.
    *   `lv_pool.c`: `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`. Se voltar `NULL`, imprime `FATAL lv pool PSRAM` com `ets_printf` e chama `abort()`. É o mesmo tratamento do buffer de tela, que já para o aparelho sem PSRAM (ino:2757).
    *   `lv_conf.h`:
        *   `#define LV_MEM_POOL_INCLUDE "hw/lv_pool.h"`;
        *   `#define LV_MEM_POOL_ALLOC(size) thep_lv_pool_alloc(size)`.

        As duas macros são lidas por `lv_mem_core_builtin.c:20-21,78-79` quando `LV_MEM_ADR == 0`, que é o padrão (`lv_conf_internal.h:191-211`). O include resolve pelo `-I${SKETCH_DIR}/src` do item 8.
    *   Medir o heap interno livre antes e depois e anotar no PR.

#### 16. `firmware/claude_stick/src/net/token_store.{h,cpp}` (Create)

*   **Type / Scope**: Segredos compartilhados entre o `loop()` e a `net_task`, sob o mutex `tokMx`.
*   **Proposed Changes**:
    *   Estado interno: `tokens[ACCT_MAX][200]`, `gen[ACCT_MAX]`, `pendingToken[200]`, `reconnNets[MAX_SAVED_NETWORKS]` + `reconnN` (cópia das redes salvas para o `RECONNECT`) e o mutex `tokMx` (`xSemaphoreCreateMutex`).
    *   Funções (todas pegam `tokMx`):
        *   `void token_store_set(int slot, uint16_t gen, const char* tok)`;
        *   `void token_store_clear(int slot, uint16_t newGen)`: zera o token com `memset` e grava `newGen`. Sequência ao remover: `acct_bump_gen(slot)` e depois `token_store_clear(slot, acct[slot].gen)`;
        *   `void token_store_clear_all()`;
        *   `bool token_store_copy(int slot, uint16_t gen, char* buf, size_t)`: devolve `false` se o `gen` não bate ou o slot está vazio. Nesse caso a task publica `cls = FC_DISCARDED` sem fazer a requisição;
        *   `bool token_store_set_pending(const char* tok)` (falso se já há um token em validação), `bool token_store_copy_pending(char* buf, size_t)` (usado pela task), `bool token_store_take_pending(char* buf, size_t)` (copia e zera; usado pelo `loop()`) e `void token_store_clear_pending()`;
        *   `void token_store_set_reconnect(const WiFiManager::SavedNet* nets, int n)` e `int token_store_copy_reconnect(WiFiManager::SavedNet* out)`.

#### 17. `firmware/claude_stick/src/hw/wifi_manager.h` (Modify)

*   **Type / Scope**: WiFi sem NVS para a `net_task`.
*   **Proposed Changes**:
    *   `struct SavedNet` passa a ser público.
    *   `int copySaved(SavedNet out[MAX_SAVED_NETWORKS])`: copia `_nets`, devolve `_count`. Chamada no `loop()`.
    *   `static bool connectList(const SavedNet* nets, int n, int timeoutMs, int* winnerIdx)`: tenta cada rede como o `autoConnect()` atual (wifi_manager.h:28-47), mas **sem** `_promote` e sem `_saveAll`, ou seja, sem NVS. É a única função de WiFi chamada pela `net_task`. Timeout de `WIFI_CONNECT_TIMEOUT_MS` (8 s) por rede, até 3 × 8 s = 24 s.
    *   `void promote(int idx)`: wrapper público de `_promote`, chamado pelo `loop()` quando o resultado do `RECONNECT` chega.
    *   **Chamadas de `autoConnect()`:**
        *   `pin_submit` (ino:476): removida. Depois do desbloqueio, quem reconecta é o agendador, com `RECONNECT`;
        *   `bg_refresh` (ino:2709): removida junto com `bg_refresh` (item 24);
        *   `setup()` (ino:2790 e :2795): mantidas. Elas rodam antes de existir qualquer job, no contexto do `loop()`, que é dono do NVS.
    *   O WiFi do onboarding e a tela de WiFi da sessão continuam chamando `connectTo()` no `loop()`. No onboarding, o agendador ainda não roda. Na sessão, a tela espera `poller_idle()` (item 18) antes de mexer no rádio.

#### 18. `firmware/claude_stick/src/net/poller.{h,cpp}` (Create)

*   **Type / Scope**: Task FreeRTOS, filas e aplicação dos resultados no `loop()`.
*   **Proposed Changes**:
    *   `void poller_start()` cria a `net_task` com `xTaskCreatePinnedToCore` (core `NET_TASK_CORE`, stack `NET_TASK_STACK`, prioridade 1), uma `jobQ` (8 itens) e uma `resQ` (8 itens). É chamada em `setup()` (item 6, passo 1).
    *   `TaskHandle_t poller_task_handle()`, para o `/health`.
    *   **Job, sem token:** `struct NetJob { JobType type; int8_t slot; uint16_t gen; int8_t modelIdx; char model[48]; }`, com `JobType { JOB_USAGE, JOB_STATUS, JOB_VALIDATE, JOB_RECONNECT }`. `modelIdx` é o índice em `g_models[]` (item 3) quando o job é a sonda da conta em foco, e −1 nos outros casos.
        *   `USAGE`: `slot` e `gen` da conta.
        *   `VALIDATE`: `slot = g_tokenTargetSlot` (só para o log) e `gen = web_token_seq()` (item 6). Lê o token com `token_store_copy_pending`.
        *   `STATUS` e `RECONNECT`: `slot = -1`, `gen = 0`. O `RECONNECT` lê as redes com `token_store_copy_reconnect`; o `loop()` grava a cópia (`copySaved` + `token_store_set_reconnect`) antes de enfileirar.
    *   **Resultado:** `struct NetResult { JobType type; int8_t slot; uint16_t gen; FetchClass cls; int http; uint16_t ms; uint32_t endEpoch; int8_t modelIdx; int probeHttp; uint16_t probeMs; int8_t netIdx; union { UsageData u; ModelStatus s; }; }`.
        *   `cls`, `http` e `ms` são da consulta que define a classe da conta.
        *   `probeHttp`/`probeMs` são **sempre** preenchidos quando `modelIdx >= 0`: com a tentativa do modelo do rodízio quando houve reenvio, ou com a própria consulta (`http`/`ms`) quando não houve. Com `modelIdx < 0`, ficam em 0.
        *   `netIdx` é o índice da rede que conectou no `RECONNECT` (−1 se nenhuma), e −1 nos outros tipos.
    *   **Execução de um job**, por tipo:
        *   **`USAGE`:**
            1.  `token_store_copy(slot, gen, buf, …)`. Se falhar, publica `FC_DISCARDED` e passa ao próximo job;
            2.  `reqStartMs = millis()` e `fetchUsage(buf, model, u, &http, &ms)` (item 20), em structs **locais**;
            3.  `cls = classify_usage(http, hasHeaders)` (item 11);
            4.  se `modelIdx >= 0`: `probeHttp = http`, `probeMs = ms`. Se, além disso, `probe_needs_retry(hasHeaders, model, PROBE_MODEL)`, faz `reqStartMs = millis()` de novo e repete com `PROBE_MODEL`; só esse segundo resultado define `cls`, `http`, `ms` e `u`. Se o modelo do rodízio já é o `PROBE_MODEL`, não repete.
        *   **`VALIDATE`:** igual ao `USAGE`, com `token_store_copy_pending(buf, …)` no passo 1, sempre com `PROBE_MODEL` e sem o passo 4.
        *   **`STATUS`:** sem token. `reqStartMs = millis()`, `ok = fetchModelStatus(s)` (item 21), `cls = ok ? FC_OK : FC_NET`, `http = 0`. O `fetchModelStatus` só devolve `bool` (`status.cpp:17`), e o `STATUS` não afeta o ponto, então não há classe mais fina.
        *   **`RECONNECT`:** sem token. `reqStartMs = millis()`, `n = token_store_copy_reconnect(nets)`, `ok = WiFiManager::connectList(nets, n, WIFI_CONNECT_TIMEOUT_MS, &netIdx)` (item 17), `cls = ok ? FC_OK : FC_NOWIFI`, `http = 0`.
        *   **Fim, em todos os tipos:** `endEpoch = time(nullptr)`, `reqStartMs = 0`, publica uma cópia na `resQ` com `xQueueSend(resQ, &r, portMAX_DELAY)`, zera o buffer do token com `memset` (quando houve) e loga `[slot N] <tipo> http=%d cls=%s ms=%u`, com `fetch_class_name(cls)` (item 4). O token nunca é logado.
    *   **Hooks** (declarados em `poller.h`), registrados pelo `.ino` na fase 2 e pelo `app_state` na fase 3, para que `src/net` não chame interface:

        ```cpp
        struct PollerHooks {
            void (*onStatus)();                       // depois de gravar g_status
            void (*onValidate)(const NetResult& r);   // item 6, passo 4
            void (*onReconnected)();                  // depois de um RECONNECT com FC_OK
        };
        void poller_set_hooks(const PollerHooks& h);
        ```

        | Hook | Fase 2 (`.ino`) | Fase 3 (`app_state`) |
        |---|---|---|
        | `onStatus` | `incident_sync()` (ino:2030) | `incident_sync()` de `overlay_incident.h` (item 40) |
        | `onValidate` | `on_validate_result(r)`, função nova do `.ino` com o item 6, passos 4, 7 e 8 | a mesma função, movida para `app_state.cpp` |
        | `onReconnected` | `ensure_mdns()` (item 6) | `ensure_mdns()` |

    *   **`bool poller_apply_results()`** roda no `loop()` em todos os estados (item 2) e drena a `resQ`. Devolve verdadeiro se aplicou algum resultado; o `loop()` usa esse valor para marcar a interface como suja. Cada resultado é passado a `scheduler_on_result` (item 19), inclusive `FC_DISCARDED`. Regras por tipo e classe:

        | Tipo | Classe | Efeito |
        |---|---|---|
        | `USAGE` | `FC_DISCARDED`, ou `r.gen != acct[r.slot].gen` | Nenhum efeito no modelo |
        | `USAGE` | `FC_OK` (200 ou 429 com headers) | `usage = r.u`, `usageAtEpoch = endEpoch`, `lastClass`, `lastHttp`, `lastTryEndEpoch`, `fails = 0`, `backoffCycles = 0`. Depois: `hist_push`, `accumulate_heat`, `hist_save_due` → `hist_save`, `check_thresholds` (item 3) → `scheduler_offer_moment` |
        | `USAGE` | `FC_AUTH` | `lastClass`, `lastHttp`, `lastTryEndEpoch`; `fails++`; `backoffCycles = min(2^(fails−1), ceil(AUTH_BACKOFF_MAX_SEC / intervalo))`. `usage` e histórico não mudam |
        | `USAGE` | outra falha (`RATE`, `ERR`, `SRV`, `NET`) | `lastClass`, `lastHttp`, `lastTryEndEpoch`; `fails = 0`; `backoffCycles = 0`. `usage` e histórico não mudam |
        | `USAGE` com `modelIdx >= 0` | qualquer, com `gen` válido | `g_models[modelIdx].pr = {probeHttp, probeMs}`, `atMs = millis()` |
        | `STATUS` | `FC_OK` | `g_status = r.s` e `hooks.onStatus()` |
        | `STATUS` | `FC_NET` | Mantém o último `g_status`, como a v2.3 (`status.cpp:35-38`) |
        | `VALIDATE` | qualquer, inclusive `FC_DISCARDED` | `hooks.onValidate(r)`. O filtro é o do item 6, passo 4 (`r.gen` contra `web_token_seq()`), e não o `gen` da conta |
        | `RECONNECT` | `FC_OK` | `g_wifi.promote(r.netIdx)`, `hooks.onReconnected()` e próximo ciclo antecipado (item 19) |
        | `RECONNECT` | `FC_NOWIFI` | Nenhum efeito (as contas já estão em `FC_NOWIFI`) |

        Só a primeira linha tem filtro de `gen`. `STATUS`, `RECONNECT` e `VALIDATE` nunca indexam `acct[]` com `r.slot`.
    *   **`bool poller_idle()`**: verdadeiro quando a `jobQ` está vazia e não há job em andamento.
    *   **Proteção contra requisição travada** (`void poller_check_stuck()`, no `loop()`):
        *   `uint32_t poller_req_started_ms()` devolve `reqStartMs` (0 sem requisição em andamento). A task grava o valor **antes de cada requisição HTTPS, inclusive o reenvio com `PROBE_MODEL`**, e no início de um `RECONNECT`.
        *   Se `reqStartMs != 0` e `millis() - reqStartMs > NET_REQ_STUCK_MS` (90 s), o `loop()` grava `lastReset="net_stuck"` na NVS e chama `esp_restart()`. A task **não** é recriada.
        *   **Margem:** o pior caso de uma requisição que respeita os timeouts é de ~39 s: DNS ~21 s (estimado a partir das constantes do lwIP; o E2E 2 mede), conexão 5 s, handshake 5 s e leitura 8 s (a conta está no Technical Plan §2). Um `RECONNECT` leva no máximo 24 s. O limite de 90 s fica com mais de 2× de folga.
        *   Fila vazia não é requisição travada.

#### 19. `firmware/claude_stick/src/net/scheduler.{h,cpp}` (Create)

*   **Type / Scope**: Agendador de ciclos (roda no `loop()`).
*   **Proposed Changes**:
    *   Recebe `apply_tz()` e `ensure_time()` (ino:2659-2665), com `g_timeInit`. `apply_tz()` vira `void scheduler_apply_tz()`, pública, chamada pela troca de fuso em Ajustes (hoje ino:2268) e por `ensure_time()`. `scheduler.cpp` declara `extern` de `g_pollSec`, `g_tzOffset`, `g_accts`, `g_wifi` e `g_models` (itens 2 e 3). `wait_for_time()`, `do_refresh()`, `bg_refresh()` e `probe_next_model()` (ino:2670-2732) são apagados.
    *   Estado interno: `started`, `cycleOutstanding`, `cycleStartMs`, `nextCycleMs`, `cycleEndEpoch`, `firstCycleRunning`, `probeIdx`, `lastStatusMs`, `noClockSinceMs`, `paused`, `bestMoment {slot, win, thr}` (`slot = -1` quando vazio).
    *   API: `void scheduler_start()` (no desbloqueio), `void scheduler_tick()`, `void scheduler_on_result(const NetResult&)`, `void scheduler_request_refresh()`, `void scheduler_pause()`, `void scheduler_resume()`, `void scheduler_offer_moment(int slot, ThrHit hit)`, `void moment_flush()`, `void scheduler_set_moment_hook(bool (*show)(int slot, int win, int thr))`, `void scheduler_apply_tz()`, `uint32_t scheduler_last_cycle_ms()`, `uint32_t scheduler_cycles_over_interval()`, `uint32_t scheduler_cycle_end_epoch()`, `bool scheduler_first_cycle_running()`.
    *   Pseudo-código:

        ```text
        scheduler_start():
            started = true                                       // antes disto, scheduler_tick() não faz nada
            firstCycleRunning = true; nextCycleMs = millis()      // primeiro ciclo já
            log "[CYC] unlock"

        scheduler_tick():
            if not started or paused or cycleOutstanding > 0: return    // onboarding e ST_PIN: sem jobs
            if WiFi.isConnected() and not g_timeInit: ensure_time()
            if millis() < nextCycleMs: return
            if not WiFi.isConnected():
                noClockSinceMs = 0
                para cada slot com g_accts.used e keyOk: acct[slot].lastClass = FC_NOWIFI   // só lastClass
                copySaved + token_store_set_reconnect; envia RECONNECT(slot=-1, gen=0)
                cycleOutstanding = 1; cycleStartMs = millis(); log "[CYC] start n=0 reconnect"
                return
            if time(nullptr) < 1e9:                                  // sem NTP
                if noClockSinceMs == 0: noClockSinceMs = millis()    // 1º tick com WiFi e sem relógio
                if millis() - noClockSinceMs > NTP_GRACE_SEC*1000:
                    para cada slot com g_accts.used e keyOk: acct[slot].lastClass = FC_NOCLOCK
                return                                               // reavalia no próximo tick
            noClockSinceMs = 0
            // monta o ciclo
            n = 0
            para slot em 0..ACCT_MAX-1, em ordem:
                if not g_accts.used[slot] or not acct[slot].keyOk: continue   // SEM CHAVE não é consultado
                if acct[slot].backoffCycles > 0: acct[slot].backoffCycles--; log "[CYC] slot %d backoff %d"; continue
                job = USAGE(slot, acct[slot].gen, PROBE_MODEL, modelIdx=-1)
                if slot == g_accts.active:
                    job.model = g_models[probeIdx % NMODELS].id; job.modelIdx = probeIdx % NMODELS
                    probeIdx++                                       // só avança quando a sonda é enviada
                envia job; n++
            if millis() - lastStatusMs >= STATUS_POLL_SEC*1000 or lastStatusMs == 0:
                envia STATUS(slot=-1, gen=0); lastStatusMs = millis(); n++
            if n == 0: nextCycleMs = millis() + intervalo; return    // tudo em espera: sem ciclo
            cycleOutstanding = n; cycleStartMs = millis(); log "[CYC] start n=%d"

        scheduler_on_result(r):                                      // chamado para todo resultado, inclusive DISCARDED
            if r.type == VALIDATE: return                            // fora do ciclo
            cycleOutstanding--
            if cycleOutstanding > 0: return
            dur = millis() - cycleStartMs
            if dur > intervalo: cyclesOverInterval++
            cycleEndEpoch = time(nullptr); firstCycleRunning = false
            log "[CYC] end %ums classes=<classe de cada slot>"
            moment_flush()                                           // mostra bestMoment, se houver
            nextCycleMs = millis() + intervalo
            if r.type == RECONNECT and r.cls == FC_OK: nextCycleMs = millis()   // volta a consultar já

        moment_flush():
            if bestMoment.slot < 0 or not momentHook: return
            if momentHook(bestMoment.slot, bestMoment.win, bestMoment.thr): bestMoment.slot = -1
            // false = já há um overlay de momento aberto (ou, na fase 2, a tela não é ST_MAIN):
            // o momento fica guardado até a próxima chamada de moment_flush()

        scheduler_request_refresh():                                 // "Atualizar agora"
            if cycleOutstanding == 0: nextCycleMs = millis()         // durante um ciclo: sem efeito
        ```

    *   **Espera crescente:** `fails` conta só `FC_AUTH` consecutivos; qualquer outra classe zera (item 18). `fails` e `backoffCycles` voltam a zero quando o token é trocado (`acct_reset_runtime`, item 6, passo 8).
    *   **Conta em foco em espera ou `SEM CHAVE`:** não há sonda naquele ciclo, e `probeIdx` não avança.
    *   **Momentos de limiar:** `scheduler_offer_moment(slot, hit)` guarda `bestMoment` e só o troca por um `thr` estritamente maior. Como os resultados chegam em ordem de slot, o empate fica com o slot menor. Dentro de uma conta, `check_thresholds` já devolve o maior limiar e, em empate, a janela de 5h (item 3). `moment_flush()` mostra o momento guardado no fim do ciclo ou, se já houver um overlay de momento aberto, assim que ele fechar: os dois fechamentos de fim de momento, o toque (`moment_close_cb`, ino:1727) e o tempo esgotado em `moment_tick` (ino:1879), chamam `moment_flush()` depois de `moment_close()`. A própria `moment_close()` (ino:1722) não chama, porque `render_state()` (ino:2623) e `show_moment()` (ino:1730) também a usam para limpar a tela.
    *   **Hook do momento:** na fase 2, o `.ino` registra uma função que devolve `false` se `g_mo.scrim` estiver aberto ou se `g_state != ST_MAIN` e, senão, chama o `show_moment(win, thr)` atual (ino:1729, ainda sem rótulo) e devolve `true`. `render_state()` chama `moment_flush()` no fim, depois de montar `ST_MAIN`. O bloco `g_pendWin` do `loop()` (ino:2898-2901) sai, junto com `g_pendWin`/`g_pendThr` (ino:1697). Na fase 3, o hook é `show_moment(slot, win, thr)` do item 40, que só recusa quando há um momento aberto.
    *   **`scheduler_pause()`/`scheduler_resume()`:** param e retomam o envio de jobs. Usados pela tela de WiFi da sessão (item 24 na fase 2, item 39 na fase 3).
    *   **`lastReset`:** no `setup()`, o motivo gravado pelo item 18 é impresso (`[BOOT] lastReset=%s`) e a chave é apagada.

#### 20. `firmware/claude_stick/src/net/api.{h,cpp}` (Modify)

*   **Type / Scope**: Cliente HTTPS.
*   **Proposed Changes**:
    *   **Helper comum de timeouts**, declarado em `api.h` e usado também por `status.cpp` (item 21):

        ```cpp
        // Chamado depois de https.begin(client, url).
        void net_client_setup(WiFiClientSecure& client, HTTPClient& https) {
            client.setCACert(CA_BUNDLE);
            client.setHandshakeTimeout(NET_HANDSHAKE_S);   // segundos (NetworkClientSecure.cpp:450-451)
            https.setConnectTimeout(NET_CONNECT_MS);       // HTTPClient.h:216; repassado a cada connect (HTTPClient.cpp:1109)
            https.setTimeout(API_TIMEOUT_MS);              // espera máxima por dados (HTTPClient.cpp:1287)
        }
        ```

    *   `client.setConnectionTimeout()` não é usado: o `HTTPClient::connect` sobrescreve o valor do client com o próprio `_connectTimeout` (`HTTPClient.cpp:1109`, `NetworkClientSecure.cpp:129-131`).
    *   **Assinatura nova:** `bool fetchUsage(const char* token, const char* modelId, UsageData& out, int* httpOut, uint16_t* msOut)`.
        *   Devolve `hasUnifiedHeaders`.
        *   `out` é preenchido sempre que houver headers, inclusive em 401/403/429.
        *   `*httpOut` é sempre definido: o código HTTP, ou um valor **negativo** em erro de transporte (os `HTTPC_ERROR_*` de `HTTPClient.h:47-57`; uma falha de `https.begin` vira −1, como `probeModel` já faz em `api.cpp:98`). O 0 nunca é devolvido, porque em `/modelos` ele quer dizer "nunca sondado" (item 38). `*msOut` traz a latência da requisição.
        *   A classificação sai daqui e vai para `classify_usage` (item 11). O ramo `auth_failed` de `api.cpp:63-64` sai.
    *   **Token:** no máximo 199 caracteres. O limite é garantido pelo `POST /token` (item 6) e pelo desbloqueio, que recusa `blob.len > 199` antes de decifrar, como o `decryptToken` já faz com um buffer de 200 (`crypto.cpp:60`). Nesse caso o slot fica `SEM CHAVE`, e o desbloqueio loga `[slot N] token longo demais` (item 23).
    *   **Header `Authorization`:** montado num `char[220]` de stack ("Bearer " + 199 + NUL = 207 bytes), zerado depois do `https.end()`. O `addHeader(const String&, const String&, …)` (`HTTPClient.h:238`) copia o valor para um `String` no heap, que o `HTTPClient` libera sem zerar. Zerar o `char[220]` não apaga essa cópia: ela é o risco residual declarado no Technical Plan §1 e em Risks & Mitigations.
    *   **`probeModel`:** é removido, porque a sonda passa a vir do `USAGE` da conta em foco.
    *   **Logs:** `api.cpp` deixa de logar por conta própria; o log com `[slot N]` fica no poller (item 18).

#### 21. `firmware/claude_stick/src/net/status.cpp` (Modify)

*   **Type / Scope**: Cliente do status.claude.com.
*   **Proposed Changes**:
    *   `fetchModelStatus` passa a chamar `net_client_setup(client, https)` (item 20) no lugar de `client.setCACert` + `https.setTimeout` (`status.cpp:19-28`). Hoje o handshake fica no padrão de 120 s (`NetworkClientSecure.cpp:41`), o que passaria do limite de requisição travada.
    *   O resto não muda.

#### 22. `firmware/claude_stick/config.h` (Modify)

*   **Type / Scope**: Constantes.
*   **Proposed Changes**:
    *   `FW_VERSION "3.0"`.
    *   `API_TIMEOUT_MS 8000`.
    *   Novas: `NET_CONNECT_MS 5000`, `NET_HANDSHAKE_S 5`, `NET_REQ_STUCK_MS 90000`, `NET_TASK_STACK 14336`, `NET_TASK_CORE 0`, `HIST_SAVE_MIN_SEC 300`, `DOT_FRESH_SEC 10`, `AUTH_BACKOFF_MAX_SEC 1800`, `NTP_GRACE_SEC 60`.
    *   `STATUS_POLL_SEC` passa a ser usado (item 19).

#### 23. `firmware/claude_stick/src/sec/crypto.{h,cpp}` e desbloqueio no `.ino` (Modify)

*   **Type / Scope**: Segurança e desbloqueio.
*   **Proposed Changes**:
    *   Funções novas em `crypto.h`, todas envolvendo o `deriveKey` existente (`crypto.cpp:10`):
        *   `void deriveSessionKey(const char* pin, uint8_t key[32])`: lê o MAC com `esp_efuse_mac_get_default(mac)` e chama `deriveKey(pin, mac, 6, key)`. É o mesmo salt que `encryptToken` grava em cada blob (`crypto.cpp:34`);
        *   `bool encryptTokenWithKey(const char* plaintext, const uint8_t key[32], EncryptedBlob& blob)`: igual a `encryptToken`, com `blob.salt` = MAC e sem derivar a chave;
        *   `bool decryptTokenWithKey(const EncryptedBlob& blob, const uint8_t key[32], char* out, size_t max)`.
    *   **Dono:** na fase 2, `uint8_t g_sessionKey[32]` e `bool session_unlocked()` ficam no `.ino`, com o fluxo de PIN. Na fase 3, vão para `app_state.cpp` (item 25): `g_sessionKey` fica `static` lá, e `session_unlocked()` é declarada em `app_state.h`.
    *   **Desbloqueio (`ST_PIN`, no lugar de ino:470-478):**
        1.  `deriveSessionKey(pin, g_sessionKey)`;
        2.  para cada slot com `g_accts.used[slot]`: `accountLoadBlob`; se `blob.len > 199`, loga `[slot N] token longo demais` e o slot não abre; senão, `decryptTokenWithKey(blob, g_sessionKey, tok, sizeof(tok))`. Se abrir: `acct[slot].keyOk = true` e `token_store_set(slot, acct[slot].gen, tok)`. Se não: `acct[slot].keyOk = false` (`SEM CHAVE`). O buffer `tok` é zerado a cada slot. Não há ramo com `decryptToken(blob, pin, …)`: todo blob gravado pela v2.3 usa o MAC como salt (`encryptToken`, `crypto.cpp:34`), então um blob com outro salt simplesmente não abre;
        3.  se nenhum slot abriu (um token longo demais conta como slot que não abriu, como na v2.3): `memset(g_sessionKey)` e a mesma regra de tentativas e espera de hoje (ino:479-494);
        4.  se ao menos um abriu, como hoje em ino:472-474: `g_pinAttempts = 0`, `g_lockoutUntil = 0`, `save_attempts()` e `memset(g_pinEntry)`. Depois, carrega os históricos (`hist_load` de cada slot salvo, item 5), chama `web_start_session()` (item 6) e `scheduler_start()` (item 19) e vai para `ST_MAIN` (fase 2) ou para o shell com `HOME` (fase 3). Não chama mais `autoConnect()` nem vai para `ST_WIFI` sem rede: sem WiFi, o agendador manda `RECONNECT` e os cards mostram `NOWIFI`.
    *   **Definição de PIN (`ST_SETUP_PIN`, no lugar de ino:449-466):**
        1.  `deriveSessionKey(g_pinEntry, g_sessionKey)`;
        2.  `token_store_take_pending(buf)` e `encryptTokenWithKey(buf, g_sessionKey, blob)`. Se a cifra falhar: devolve o token à área de validação (`token_store_set_pending(buf)`), `memset(buf)`, `memset(g_sessionKey)`, mostra "Falha ao cifrar. Tente de novo." e zera a digitação do PIN, como hoje (ino:449-452);
        3.  `accountSave(g_prefs, g_accts, g_tokenTargetSlot, blob, g_pendingLabel)` + `accountSetActive`, como hoje, e `g_pendingLabel[0] = 0`;
        4.  `acct_bump_gen(slot)`, `acct[slot].keyOk = true`, `token_store_set(slot, acct[slot].gen, buf)` e `memset(buf)`;
        5.  `g_onboarding = false`, `g_pinConfirming = false` e `memset(g_pinFirst)`, como hoje (ino:460-463);
        6.  segue o passo 4 do desbloqueio, que zera as tentativas e `g_pinEntry`.
    *   `g_sessionPin`, `g_token` e `g_blob` saem, e `g_pendingToken` também: a área de validação do `token_store` (item 16) o substitui. `g_hasToken` passa a ser `accountCount(g_accts) > 0`, e `load_persisted()` (ino:325) deixa de carregar o blob da conta ativa. `session_unlocked()` devolve verdadeiro depois do passo 4.
    *   `switch_account()` (ino:1080) sai: trocar o foco não decifra nada.
    *   **Remover conta** (`acct_del_cb`, ino:2382):
        1.  a guarda `if (accountCount(g_accts) <= 1) return;` (ino:2384) e a confirmação com dois toques (`g_acctDelArmed`) continuam;
        2.  `accountRemove`, `acct_bump_gen(slot)`, `token_store_clear(slot, acct[slot].gen)`, apagar `/histN.bin` (item 5) e `acct_clear(slot)`, que preserva o `gen` (item 3). A fila não é esvaziada e a task não é esperada;
        3.  volta a `ST_ACCOUNTS` (fase 2) ou fica em `/contas` (fase 3). A tela inicial é recriada com o novo layout na próxima vez que abrir (fase 2) ou na hora, porque o número de slots mudou (fase 3, item 36).
    *   **`factory_reset`** (ino:351) zera também os tokens e a chave da sessão: `token_store_clear_all()`, `token_store_clear_pending()`, `memset(g_sessionKey, 0, 32)` e `stop_web()`.

#### 24. `firmware/claude_stick/claude_stick.ino` — fase 2 (Modify)

*   **Type / Scope**: Convivência da máquina de estados antiga com o agendador, e tela inicial de N colunas (ainda no visual da v2.3).
*   **Proposed Changes**:
    *   **Depois do desbloqueio**, o aparelho vai direto para `ST_MAIN`. `ST_LOADING` e `ST_ERROR` saem do `enum State` (ino:69), junto com `ui_loading` (:910), `ui_error` (:897-908), `ui_message` (:888-896, que só `ui_error` usa), `do_refresh` (a única entrada em `ST_ERROR`, ino:2702), a nova tentativa a cada 15 s (ino:2839-2843) e o `if (g_state == ST_LOADING)` do `loop()` (ino:2815-2819). Destino de cada chamada que hoje vai para `ST_LOADING`:

        | Linha do `.ino` | Onde | Destino na fase 2 |
        |---|---|---|
        | :466 | `pin_submit`, fim do `ST_SETUP_PIN` | `ST_MAIN`, pelo passo 6 da definição de PIN (item 23) |
        | :477 | `pin_submit`, desbloqueio | `ST_MAIN`, pelo passo 4 do desbloqueio (item 23) |
        | :594 | `wifi_kb_cb`, fora do onboarding | `request_state(ST_MAIN)`; a saída de `ST_WIFI` já chama `scheduler_resume()` (abaixo) |
        | :907 | `ui_error`, toque para tentar de novo | sai junto com `ui_error` |
        | :1098 | `switch_account`, chamado por `acct_switch_cb` (:2380) | `acct_switch_cb` faz `accountSetActive(g_prefs, g_accts, slot)` e `request_state(ST_MAIN)`; `switch_account` sai (item 23) |
        | :2220 | Ajustes → Atualizar agora | `scheduler_request_refresh()` e `request_state(ST_MAIN)` |
        | :2406 | `acct_del_cb`, conta em foco removida | sequência de remover do item 23 e `request_state(ST_ACCOUNTS)` |

    *   **Globais e código de atualização que saem na fase 2:** `bg_refresh`, `wait_for_time`, `probe_next_model` (item 19), `g_wantRefresh`, `g_refreshing`, `g_lastFetchOk`, `g_lastOkMs` e `g_lastPollMs` (ino:121-131), o bloco de poll automático do `loop()` (ino:2822-2827), a atualização da `refBar` no `loop()` (ino:2851-2860) e o widget dela (ino:2162-2171). `refresh_cb` (:2092) passa a chamar `scheduler_request_refresh()`. `set_hdr_status()` (:2077) passa a mostrar `ok` em `C_MUTED`, ou `K com falha` em `C_BAD` (K contado como no rodapé, item 34), até sair na fase 3. Os testes de `g_refreshing` no slideshow (:2890) somem com a variável.
    *   **Servidor web:** `render_state()` deixa de chamar `stop_web()` (:2622), e `ui_token()` também (:838). `ui_main`, `ui_settings`, `ui_accounts` e `ui_about` deixam de chamar `start_data_web()` (ino:2098, 2315, 2472, 2566). `ui_token()` chama `web_token_begin()` e, só antes do desbloqueio, `web_start_onboarding()` se ainda não houver servidor (item 6). `render_state()`, ao sair de `ST_TOKEN`, chama `web_token_reset()`. O bloco `if (g_state == ST_TOKEN && g_tokenGot)` do `loop()` (ino:2805-2809) sai, junto com `g_tokenGot`: o resultado chega pelo hook `onValidate` (item 18).
    *   **Hooks:** no `setup()`, logo depois de `poller_start()`, o `.ino` chama `poller_set_hooks({incident_sync, on_validate_result, ensure_mdns})` e `scheduler_set_moment_hook(...)` (itens 18 e 19).
    *   `poller_apply_results()`, `poller_check_stuck()` e `scheduler_tick()` rodam em todos os estados, inclusive antes do desbloqueio (item 2). O agendador só começa a mandar jobs depois de `scheduler_start()`.
    *   `g_uiDirty` (global nova do `.ino`, marcada pelo `loop()` quando `poller_apply_results()` devolve verdadeiro) só chama `refresh_ui_values()` em `ST_MAIN`. Nos outros estados, a flag é limpa e os valores aparecem quando a tela é aberta.
    *   Os botões Voltar de `ui_wifi` (:618), `ui_token` (:845) e `ui_settings` (:2324) passam a ir sempre para `ST_MAIN`, porque deixa de existir estado pós-desbloqueio sem dados.
    *   **WiFi depois do desbloqueio** (Ajustes → WiFi, ino:2221): ao entrar em `ST_WIFI`, chama `scheduler_pause()` e mostra `aguardando a rede…` até `poller_idle()`. Só então `ui_wifi()` monta a lista. Ao sair, chama `scheduler_resume()`.
    *   **`ui_main()` e o tile Agora:** uma coluna por slot salvo (1 a 4 colunas iguais), com a % da 5h, os resets, a semana e o `projection_text` de cada conta. Fontes Montserrat já habilitadas em `lv_conf.h:54-63`:

        | Colunas | % da 5h | Texto |
        |---|---|---|
        | 1 | `lv_font_montserrat_48` | `lv_font_montserrat_14` |
        | 2 | `lv_font_montserrat_40` | `lv_font_montserrat_14` |
        | 3 | `lv_font_montserrat_28` | `lv_font_montserrat_12` |
        | 4 | `lv_font_montserrat_24` | `lv_font_montserrat_12` |

    *   Texto do card na precedência do Technical Plan §4: `SEM CHAVE`; em falha sem dado, `<classe>` com `--%`; em falha com dado, `<classe> · <idade>` (`age_text`, item 12), como na CUJ 2 (o esmaecimento entra na fase 3); `pendente`, `consultando...`. As Montserrat do firmware não têm `⚠`, `…` nem acentos, então a fase 2 usa `LV_SYMBOL_WARNING` antes da classe e reticências ASCII, como os textos atuais (`"Coletando dados..."`, ino:1588).
    *   Some o badge `@label`. O tileview continua até a fase 3, quando `screen_home.cpp` (item 36) substitui esta tela.

### Fase 3 — Identidade, shell, menu e ponto

#### 25. `firmware/claude_stick/src/app/app_state.{h,cpp}` (Create)

*   **Type / Scope**: Máquina de estados, navegação e funções de intenção.
*   **Proposed Changes**:
    *   Recebe do `.ino` o `enum State` (ino:67), renomeado para `AppState`, e `request_state()`, `render_state()`, `nav_cb()`, o fluxo de PIN e onboarding (com `g_sessionKey`, `session_unlocked()` e `on_validate_result`, itens 6 e 23), o registro dos hooks (itens 18 e 19), `load_persisted()` (ino:325, sem ler a chave `slide`, que não é apagada), `save_attempts()` (:346) e `apply_brightness()` (:347).
    *   Estados:
        *   **Fora do shell, só antes do desbloqueio:** `ST_BOOT`, `ST_PIN`, `ST_SETUP_PIN`, `ST_WIFI` e `ST_TOKEN`. Depois do desbloqueio, WiFi e token são conteúdos do shell (`WIFI`, `TOKEN`).
        *   **Dentro do shell:** `ST_SHELL`, com `ContentId { HOME, MODELS, TREND, HEAT, ACCOUNTS, ACCOUNT_NAME, SETTINGS, ABOUT, TOKEN, WIFI }`.
        *   `ST_LOADING` e `ST_ERROR` já saíram na fase 2 (item 24). `ST_MAIN`, `ST_SETTINGS`, `ST_ACCOUNTS`, `ST_ACCT_NAME` e `ST_ABOUT` viram conteúdos.
    *   `void shell_show(ContentId)` troca só o conteúdo do painel. Ao abrir `TOKEN`, chama `web_token_begin()`; ao sair de `TOKEN` para qualquer outro conteúdo, `web_token_reset()` (item 6). `render_state()` faz o mesmo com `ST_TOKEN` antes do desbloqueio, como na fase 2 (item 24).
    *   Funções de intenção, declaradas em `app_state.h`:
        *   `void intent_refresh()`: chama `scheduler_request_refresh()`;
        *   `void intent_focus(int slot)`: `accountSetActive(g_prefs, g_accts, slot)` e atualiza a tela;
        *   `void intent_focus_next()`: próxima conta salva em ordem de slot, voltando à primeira;
        *   `void intent_open(ContentId)`: fecha o menu e chama `shell_show`.
    *   `void app_state_tick(bool dataChanged)`, chamado a cada passada do `loop()` com o valor devolvido por `poller_apply_results()`. Quando `dataChanged` é verdadeiro, chama a função de atualização do conteúdo aberto (`screen_home_refresh()` no `HOME`, item 36; as outras telas releem o modelo ao abrir). Além disso, faz uma reavaliação por segundo: `dot_state` → `shell_set_dot`, relógio e contadores do rodapé, idade dos cards em falha e expiração do azul. Também mantém a contagem do bloqueio de PIN (hoje ino:2833-2836).

#### 26. `tools/gen_hermes_assets.py` (Create)

*   **Type / Scope**: Gerador de assets (Python 3 + Pillow).
*   **Proposed Changes**:
    *   Script novo. O script usado nos mockups não está no repositório. As regras abaixo definem o resultado; o `gen_mockups.py` (item 43) é a conferência visual.
    *   **Logo do cabeçalho (`img_thep_logo_hdr`, 104×38):**
        *   texto em ANSI Shadow, 6 linhas × 33 colunas:

            ```text
            ████████╗██╗  ██╗███████╗██████╗ 
            ╚══██╔══╝██║  ██║██╔════╝██╔══██╗
               ██║   ███████║█████╗  ██████╔╝
               ██║   ██╔══██║██╔══╝  ██╔═══╝ 
               ██║   ██║  ██║███████╗██║     
               ╚═╝   ╚═╝  ╚═╝╚══════╝╚═╝     
            ```

        *   desenho por células, sem fonte, em 8× (832×304), reduzido no fim para 104×38 com `Image.LANCZOS`. Como 104/33 e 38/6 não são inteiros, as bordas das células são arredondadas na escala 8×: a coluna `c` vai de `round(c*832/33)` a `round((c+1)*832/33)` e a linha `r` de `round(r*304/6)` a `round((r+1)*304/6)`. Assim as células cobrem a imagem sem sobra nem buraco, com larguras de 25 ou 26 px em 8×;
        *   `█` preenche a célula inteira. `═ ║ ╔ ╗ ╚ ╝` viram traços de 1 px na escala final (8 px em 8×), centrados na célula, com os cantos ligando os dois traços;
        *   faixas de cor por linha: linhas 0–1 `#2fb5dd`, 2–3 `#2f8fd6`, 4–5 `#1272c2`.
    *   **Pássaro em braille:**
        *   entrada: `assets/brand/thep-bird.png` (item 27);
        *   `img_bird_braille_124` (124×124): grade de 40×40 pontos com passo de 3 px, centrada (120 px + 2 px de margem de cada lado). A imagem de entrada é reduzida para 40×40 com `Image.BOX`; um ponto acende quando o alfa é ≥ 128; a cor do ponto é a média RGB dos pixels do bloco de origem; cada ponto é um círculo de 2 px de diâmetro;
        *   `img_bird_braille_13` (13×13): grade de 6×6 pontos com passo de 2 px e pontos de 2×2 px, ocupando de (0, 0) a (11, 11). A sobra de 1 px fica à direita e embaixo, transparente, para não haver meio pixel. Mesmas regras de alfa e cor.
    *   **Saída:** `firmware/claude_stick/src/ui/assets_thep.c` com as definições e `firmware/claude_stick/src/ui/assets_thep.h` com `LV_IMAGE_DECLARE(...)` (`lv_image.h:302`) de cada imagem. As imagens não ficam em header, para não serem copiadas em cada `.cpp` que as inclui (hoje são `static const` em `logo_assets.h:5`).
    *   **Formato** `LV_COLOR_FORMAT_RGB565A8` (`lv_color.h:144`):
        *   cabeçalho `{ LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565A8, 0, w, h, w*2, 0 }` (`magic, cf, flags, w, h, stride, reserved_2`, `lv_image_dsc.h:88-97`);
        *   dados: `w·h` pixels RGB565 little-endian, seguidos de `w·h` bytes de alfa; `data_size = w·h·3`;
        *   do `gen_logo_assets.py` antigo, que gera ARGB8888 (`gen_logo_assets.py:58-75`), só a estrutura de `to_c()` é reaproveitada.
    *   Uso: `python3 tools/gen_hermes_assets.py`.

#### 27. `assets/brand/thep-bird.png` (Create)

*   **Type / Scope**: Asset de marca.
*   **Proposed Changes**: o logo do pássaro da THEP (PNG 1024×1024 com transparência, fornecido pelo dono). É a fonte de verdade para o braille.

#### 28. `firmware/claude_stick/src/ui/assets_thep.{h,c}` (Create, gerados)

*   **Type / Scope**: Imagens LVGL embutidas.
*   **Proposed Changes**:
    *   `img_thep_logo_hdr` (104×38): cabeçalho.
    *   `img_bird_braille_124` (124×124, ~46 KB): layout de 1 conta, momentos de limiar e alerta de incidente. No incidente, o cinza sai de `lv_obj_set_style_image_recolor(img, lv_color_hex(0x6A6A74), 0)` junto com `lv_obj_set_style_image_recolor_opa(img, 190, 0)`, os mesmos valores de hoje (ino:1285-1286). Sem a opacidade, o recolor não aparece (o padrão é 0).
    *   `img_bird_braille_13` (13×13): rodapé.

#### 29. `tools/gen_fonts.sh` (Create)

*   **Type / Scope**: Gerador de fontes.
*   **Proposed Changes**:
    *   Baixa a JetBrains Mono 2.304 (Regular e Bold) para `tools/.cache/fonts/`, em versão fixa, e confere o sha256.
    *   **Checagem de glifos** (feita pelo script, antes de gerar): para cada code point fora de ASCII e Latin-1 das faixas abaixo, confere com `fontTools` se a TTF tem o glifo. Falta qualquer um → sai com erro. A exceção é o `⚠` (U+26A0): se faltar, só avisa, e o card em falha passa a mostrar `<classe> · <idade>`, sem o ícone.
    *   Comando por fonte:

        ```text
        npx lv_font_conv@1.5.3 --bpp 4 --no-compress --format lvgl --size <N> \
          --font tools/.cache/fonts/JetBrainsMono-<Regular|Bold>.ttf -r <faixas> \
          --lv-font-name <nome> --lv-include lvgl.h [--lv-fallback lv_font_montserrat_14] \
          -o firmware/claude_stick/src/ui/fonts/<nome>.c
        ```

        *   `--lv-include lvgl.h` faz o arquivo gerado incluir `lvgl.h`, e não `lvgl/lvgl.h`, sem precisar de `LV_LVGL_H_INCLUDE_SIMPLE` no `build.sh`;
        *   `--lv-fallback lv_font_montserrat_14` só nas fontes de texto. O campo `fallback` é gravado no `const lv_font_t` gerado e não pode ser atribuído depois (`lv_font.h:108`). As duas opções existem no `lv_font_conv` 1.5.3.
    *   Faixas das fontes de texto: `0x20-0x7E,0xA0-0xFF,0x2014,0x2022,0x2026,0x2500,0x2502,0x256D-0x2570,0x2588,0x2591,0x26A0,0x2713,0x2717,0x276F`.
    *   Faixas das fontes numéricas: `0x25,0x2D,0x30-0x39` (`%`, `-` para o `--%` do card pendente e os dígitos).
    *   Imprime o tamanho do `.o` de cada fonte, compilado com o `xtensa-esp32s3-elf-gcc` do Arduino15, para registrar o flash real.

#### 30. `firmware/claude_stick/src/ui/fonts/jbmono_11.c`, `jbmono_13.c`, `jbmono_bold_13.c`, `jbmono_bold_num_{20,30,38,50}.c`, `fonts.h` (Create, gerados)

*   **Type / Scope**: Fontes LVGL.
*   **Proposed Changes**:
    *   Arquivos `.c` gerados pelo item 29. As três fontes de texto já saem com `fallback = &lv_font_montserrat_14`.
    *   `fonts.h` (escrito à mão) só declara as fontes com `LV_FONT_DECLARE(...)` (`lv_font.h:171`).

#### 31. `firmware/claude_stick/src/ui/fonts/LICENSES.md` (Create)

*   **Type / Scope**: Licenças.
*   **Proposed Changes**: o texto da SIL OFL 1.1 e o aviso de copyright da JetBrains Mono.

#### 32. `firmware/claude_stick/src/ui/i18n.h` (Create)

*   **Type / Scope**: Idioma da interface e tabela de textos novos.
*   **Proposed Changes**:
    *   Recebe `TRS(pt, en)` (ino:57) e o acesso a `g_lang`.
    *   Os textos de projeção e de `/modelos` são gerados em `src/logic/` (itens 9 e 11), que recebem `lang` como parâmetro. Os outros ficam como `TRS(...)` nas telas.
    *   Tabela de todos os textos novos da v3:

        | Onde | Português | English |
        |---|---|---|
        | Projeção, não esgota | `✓ não esgota · ~78%` | `✓ ok to reset · ~78%` |
        | Projeção, esgota | `✗ esgota 15:20 · 38m` | `✗ runs out 15:20 · 38m` |
        | Projeção, esgotada | `✗ esgotada` | `✗ exhausted` |
        | Projeção, sem dados | `— sem histórico` | `— no history` |
        | Card pendente | `consultando…` | `querying…` |
        | Card que não decifrou | `SEM CHAVE` | `NO KEY` |
        | Card em falha | `⚠ AUTH · 4m` | `⚠ AUTH · 4m` |
        | Card em falha sem dado | `⚠ AUTH` | `⚠ AUTH` |
        | Card pendente ou em falha sem dado, % | `--%` | `--%` |
        | Marca de estado | `ok` / `atenção` / `limite` | `ok` / `warning` / `limit` |
        | Linhas do card | `reset:` / `semana:` / `sem` | `reset:` / `week:` / `wk` |
        | WiFi da sessão | `aguardando a rede…` | `waiting for the network…` |
        | Rodapé | `1 conta` / `N contas` | `1 account` / `N accounts` |
        | Menu | `Atualizar agora` | `Refresh now` |
        | Menu, descrições | `uso das contas`, `status por modelo`, `gráfico 5h`, `uso por hora`, `gerenciar contas`, `configurações` | `account usage`, `per-model status`, `5h chart`, `usage by hour`, `manage accounts`, `settings` |
        | `/modelos`, título | `Modelos · <conta>` | `Models · <account>` |
        | `/modelos`, status | `status.claude.com: sem incidentes` | `status.claude.com: no incidents` |
        | `/modelos`, incidente | `incidente <gravidade> · <modelos>` | `incident <severity> · <models>` |
        | `/contas`, dica | `Tocar numa conta define o foco.` | `Tap an account to focus it.` |
        | `/contas`, marcador | `•  foco` | `•  focus` |
        | Página do token | `validando…` | `validating…` |
        | Página do token, ok | `Token validado. Defina o PIN no aparelho.` / `Conta salva.` | `Token validated. Set the PIN on the device.` / `Account saved.` |
        | Página do token, recusado | `A API recusou o token (<reason>). Confira e cole de novo.` | `The API rejected the token (<reason>). Check it and paste again.` |
        | Página do token, tela fechada | `Abra a tela de token no aparelho e envie de novo.` | `Open the token screen on the device and send again.` |
        | Página do token, falha de cifra | `Falha ao cifrar no aparelho. Tente de novo.` | `Encryption failed on the device. Try again.` |
        | Página do token, ocupado | `Já existe uma validação em andamento.` | `A validation is already running.` |
        | Página do token, sem resposta | `O aparelho não respondeu. Tente de novo.` | `The device did not answer. Try again.` |

    *   Os nomes de classe (`AUTH`, `RATE`, `ERR`, `SRV`, `NET`, `NOWIFI`, `NOCLOCK`) e os comandos do menu (`/agora`…) não são traduzidos.

#### 33. `firmware/claude_stick/src/ui/theme.{h,cpp}` (Create)

*   **Type / Scope**: Tema.
*   **Proposed Changes**:
    *   Constantes de papel com os valores da tabela "Paleta" do Technical Plan §6: `C_BG`, `C_STATUSBAR`, `C_SURFACE`, `C_SELECTED`, `C_BORDER`, `C_TITLE`, `C_ACCENT`, `C_TEXT`, `C_DIM`, `C_UNLIT`, `C_OK`, `C_OK_INK`, `C_WARN`, `C_DOT_YELLOW`, `C_BAD`, `C_BAD_INK`.
    *   `uint32_t tone_color(Tone)`: `TONE_DIM` → `C_DIM`, `TONE_OK` → `C_OK_INK`, `TONE_WARN` → `C_WARN`, `TONE_BAD` → `C_BAD_INK`, que são as cores da tabela "Marca de estado da API" (§5). `uint32_t level_color(Level)`: verde → `C_OK`, aviso → `C_WARN`, vermelho → `C_BAD`.
    *   Estilos `lv_style_t` compartilhados: painel, título de painel, seção, `chave:`, valor, linha de menu e linha de menu selecionada.
    *   `theme_init()` é chamada no `setup()`.
    *   As cores fixas de `claude_stick.ino:40-53` e as funções `pct_color`/`grad_color` (ino:263-276) saem.

#### 34. `firmware/claude_stick/src/ui/shell.{h,cpp}` (Create)

*   **Type / Scope**: Shell fixo. Valores copiados do [mockup-home.dc.html](./review/context/mockup-home.dc.html), que passa a ser a referência de geometria.
*   **Proposed Changes**:
    *   `shell_create()`:
        *   **cabeçalho** de 50 px, padding 6/8/4/10 px (cima/direita/baixo/esquerda), gap de 10 px: `img_thep_logo_hdr`, o ponto (10 px, círculo) e o botão `❯ menu` (38 px de altura, largura mínima de 84 px, borda de 1 px `C_BORDER`, raio 6, texto `jbmono_bold_13` em `C_TITLE`, `ext_click_area` de 6 px);
        *   **painel** com margem de 6 px em cima e embaixo e 8 px dos lados, borda de 1 px `C_BORDER`, raio 8 e padding 14/12/8 px (cima/lados/baixo). Área interna: 438 × 214 px. O título fica sobre a borda (8 px acima dela e 14 px da direita), com padding de 6 px dos lados, fundo `C_BG` e `jbmono_bold_13` em `C_TITLE`. O mockup usa 11 px em negrito; o plano usa a Bold 13, que já existe, para não gerar mais uma fonte;
        *   **rodapé** de 20 px, fundo `C_STATUSBAR`, padding de 10 px dos lados, texto `jbmono_11` em `C_TEXT` e separadores `│` em `C_UNLIT`.
    *   **Rodapé:** `<pássaro 13px> thep │ N contas · K ✗ │ ↻ <intervalo> │ HH:MM`.
        *   N = slots salvos; K = contas no estado `falhou` (estado `falhou` da §4; `failed` no item 10). O trecho ` · K ✗` some quando K = 0;
        *   `<intervalo>` é o intervalo configurado (`30s`, `1min`, `2min` ou `5min`). O `↻` é o `LV_SYMBOL_REFRESH` (U+F021, `lv_symbol_def.h:80`), desenhado pela Montserrat 14 através do `fallback` da `jbmono_11`, e não o U+21BB;
        *   o "N/4" do mockup é substituído por "N contas · K ✗", porque "2/4" era ambíguo.
    *   **Ponto:** `shell_set_dot(DotState)`, com um `lv_timer` de 400 ms que alterna 4 degraus de opacidade (100/70/40/70%), um ciclo de 1,6 s. Só o objeto do ponto é invalidado.
    *   **Spike da fase 3**, antes de dar este item por concluído:
        1.  medir o `frame_ms_max` com o pulso ligado no modo atual (`LV_DISPLAY_RENDER_MODE_FULL`, buffer de tela cheia, ino:2755-2760). Nesse modo, o `disp_flush_cb` rotaciona e envia os 480×320 pixels a cada passada (ino:215-224), então invalidar só o ponto não reduz o envio;
        2.  testar `LV_DISPLAY_RENDER_MODE_PARTIAL` com um buffer de 1/10 da tela e um flush que envia só a área ao AXS15231B;
        3.  registrar no PR o modo e o `frame_ms_max` de cada teste.

        Se o passo 1 ficar abaixo de 40 ms, nada muda na fase 3. Se não, e o passo 2 resolver, o display passa ao modo parcial. Se nenhum resolver, o pulso cai para 1 troca por segundo.
    *   **Ordem dos overlays** no `lv_layer_top`, de baixo para cima: menu, momento, incidente. `overlay_restack()` é chamada depois de criar qualquer overlay e faz `lv_obj_move_to_index(moment, -1)` e depois `lv_obj_move_to_index(incident, -1)` para os que existirem.
    *   **Conteúdo:** `shell_content()` devolve o container que as telas preenchem, e `shell_set_title(const char*)` muda o título.

#### 35. `firmware/claude_stick/src/ui/overlay_menu.{h,cpp}` (Create)

*   **Type / Scope**: Menu modal.
*   **Proposed Changes**:
    *   Fica no `lv_layer_top`, a 50 px do topo e 8 px da direita, com 262 px de largura, fundo `C_SURFACE`, borda de 1 px `C_BORDER`, raio 6 e padding de 4 px.
    *   7 linhas de **36 px**: 7 × 36 + 8 de padding + 2 de borda = 262 px de altura, que cabem nos 270 px abaixo do cabeçalho. Cada linha tem a marca `❯` (10 px), o comando (86 px, `jbmono_bold_13`) e a descrição (`jbmono_11`, `C_DIM`).
    *   Itens, em ordem, com as descrições do mockup (`mockup-home.dc.html:168`):

        | Comando | Descrição (pt) |
        |---|---|
        | `↻ Atualizar agora` (`↻` = `LV_SYMBOL_REFRESH`, U+F021, pela Montserrat 14 do `fallback`; não é U+21BB) | sem descrição |
        | `/agora` | uso das contas |
        | `/modelos` | status por modelo |
        | `/janela` | gráfico 5h |
        | `/ritmo` | uso por hora |
        | `/contas` | gerenciar contas |
        | `/ajustes` | configurações |

    *   A tela atual fica marcada com `❯`, comando em `C_TITLE` e fundo `C_SELECTED`.
    *   `Atualizar agora` é traduzido (item 32). Os comandos com `/` não são traduzidos; as descrições são.
    *   Tocar fora ou escolher um item fecha o menu.

#### 36. `firmware/claude_stick/src/ui/screen_home.cpp` (Create)

*   **Type / Scope**: Tela inicial com os 4 layouts. Substitui `ui_main()` e `build_tile_agora` (ino:1339, 2094).
*   **Proposed Changes**:
    *   Usa `layout_for_count(n)` e a tabela de campos (item 37).
    *   Grade dentro da área interna do painel (438 × 214 px):

        | Contas | Grade | Largura de cada card |
        |---|---|---|
        | 1 | coluna esquerda de 150 px (pássaro) + coluna direita, gap de 16 px | 272 px (coluna direita) |
        | 2 | 2 colunas, gap de 18 px | 210 px |
        | 3 | 3 colunas, gap de 12 px | 138 px |
        | 4 | 2 × 2, gap de 18 px entre colunas e 8 px entre linhas | 210 × 103 px |

    *   O "espaço disponível" que o `gen_mockups.py` confere (item 43) é a largura do card nesta tabela.
    *   Cada card:
        *   rótulo de `g_accts.label[slot]` com `fit_label`;
        *   marca de estado de `status_mark(usage.statusOverall, …)` (item 12), na cor de `tone_color` (item 33), conforme a tabela "Marca de estado da API" do Technical Plan §5;
        *   % da 5h na fonte numérica do layout, na cor de `level_color`;
        *   barra de blocos: `[` em `C_DIM`, os blocos acesos de `text_bar` na cor do nível, os `░` em `C_UNLIT` e `]` em `C_DIM`, como no mockup (`mockup-home.dc.html:45, 50, 68, 72, 91, 109`). `text_bar` (item 12) só gera os blocos; a barra inteira é um `lv_spangroup` com 4 spans de cores diferentes (`lv_spangroup_create`/`lv_spangroup_new_span`, `lv_span.h:58,65`; `LV_USE_SPAN` vem ligado por padrão, `lv_conf_internal.h:2135`);
        *   linhas `reset:`, `semana:` e projeção. A projeção usa `C_OK_INK` (não esgota), `C_BAD_INK` (esgota, esgotada) ou `C_DIM` (sem histórico).
    *   **Texto do card**, na precedência do Technical Plan §4:
        1.  `!keyOk` → `SEM CHAVE` no lugar da projeção e `--%`;
        2.  em falha (item 10) e `usageAtEpoch == 0` → `⚠ <classe>` (`fetch_class_name`, item 4) no lugar da projeção e `--%`;
        3.  em falha com dado → valores em `C_DIM` e `⚠ <classe> · <idade>`, com a idade de `age_text(now - usageAtEpoch, …)` (item 12), por exemplo `⚠ AUTH · 4m`;
        4.  `pendente` → `consultando…` e `--%`;
        5.  `ok` → valores normais e `projection_text`.
    *   O card inteiro é clicável e chama `intent_focus(slot)`, com um contorno de 1 px `C_ACCENT` na conta em foco.
    *   **Layout de 1 conta:** pássaro em braille na coluna esquerda e linha da contagem de tokens do bridge.
    *   **Atualização:** `void screen_home_refresh()` (declarada em `screens.h`, item 39) reescreve só os valores e é chamada por `app_state_tick(true)` (item 25). Ela substitui `refresh_ui_values()` (ino:2043). A tela só é recriada quando o número de slots salvos muda, o que `screen_home_refresh()` confere a cada chamada.

#### 37. Tabela de campos por layout (no `screen_home.cpp`, como `static const`)

*   **Type / Scope**: Dados de layout.
*   **Proposed Changes**:

    | Campo | 1 conta | 2 contas | 3 contas | 4 contas |
    |---|---|---|---|---|
    | Rótulo (máx. caracteres) | 16, `jbmono_bold_13` | 16, `jbmono_bold_13` | 11, `jbmono_bold_13` | 14, `jbmono_bold_13` |
    | Marca de estado | sob o pássaro | à direita do rótulo | à direita do rótulo | — (cor da %) |
    | % 5h | `jbmono_bold_num_50` | `jbmono_bold_num_38` | `jbmono_bold_num_30` | `jbmono_bold_num_20`, na linha do rótulo |
    | Barra 5h (blocos, entre `[` `]`) | 18, `jbmono_13` | 14, `jbmono_13` | 9, `jbmono_11` | 14, `jbmono_11` |
    | Reset 5h | `reset: 1h12m · 16:40` | `reset: 1h12m · 16:40` | `1h12m · 16:40` | `reset 16:40`, na linha da semana |
    | Semana | seção + % em `jbmono_bold_13` + barra de 10 blocos em `jbmono_11` | `semana: 38%` + barra de 8 blocos em `jbmono_11` | `semana 38%` | `sem 38%` |
    | Reset semana | `reset: qui 09:00` | `reset: qui 09:00` | `reset qui 09:00` | — |
    | Projeção (`maxChars`) | 41 | 31 | 20 | 31 |
    | Contagem de tokens | sim | — | — | — |

    *   O `maxChars` da projeção é a largura do card (item 36) dividida pela largura de um caractere de `jbmono_11` (0,6 em = 6,6 px), arredondada para baixo: 272 ÷ 6,6 = 41; 210 ÷ 6,6 = 31; 138 ÷ 6,6 = 20. No layout de 3 contas, um texto maior que 20 caracteres perde o trecho ` · <tempo>` (item 9).
    *   **Fonte da barra:** o mockup usa 12 px nos layouts 1 e 2 e 11 px nos 3 e 4. O plano usa `jbmono_13` e `jbmono_11`, que já existem, para não gerar mais uma fonte. Com os 2 colchetes, a barra da 5h mede 20 × 7,8 = 156 px (layout 1), 16 × 7,8 ≈ 125 px (layout 2), 11 × 6,6 ≈ 73 px (layout 3) e 16 × 6,6 ≈ 106 px (layout 4), todas dentro da largura do card.
    *   A tabela é validada pelo `tools/gen_mockups.py` (item 43) antes do código LVGL: se um campo não couber com a fonte real, o `gen_mockups.py` falha, e a linha correspondente da tabela é ajustada no PR. Nas barras, a largura medida inclui os 2 colchetes.
    *   Texto secundário (`reset:`, `semana:`, projeção) usa `jbmono_11`. Esta tabela substitui o resumo provisório do Technical Plan §5.

#### 38. `firmware/claude_stick/src/ui/screen_models.cpp` (Create)

*   **Type / Scope**: `/modelos` em texto. Substitui `build_tile_models` (ino:1354) e os mascotes.
*   **Proposed Changes**:
    *   Seção `Modelos · <conta em foco>` e uma linha por modelo: nome alinhado em 9 colunas e o texto de `model_status_text` (item 11), por exemplo `Haiku    ok · 0.9s`.
    *   `model_chip()` (ino:1313-1322) é substituída por `model_status_text`, com esta tabela:

        | Código HTTP | Português | English | Tom |
        |---|---|---|---|
        | 0 (nunca sondado) | `—` | `—` | `TONE_DIM` |
        | 200 | `ok · 0.9s` | `ok · 0.9s` | `TONE_OK` |
        | 429 | `limitado` | `limited` | `TONE_WARN` |
        | 404 | `n/d` | `n/a` | `TONE_DIM` |
        | 401, 403 | `AUTH` | `AUTH` | `TONE_BAD` |
        | < 0 | `NET` | `NET` | `TONE_BAD` |
        | outros | `erro 503` | `error 503` | `TONE_BAD` |

    *   `NET` substitui o `REDE` de hoje, para usar o mesmo nome da classe da §4 e do card.
    *   Abaixo, uma linha `status.claude.com: sem incidentes` ou, com incidente aberto (`status_get()`, item 3), `incidente <gravidade> · <modelos>` (texto no item 32). `<gravidade>` vem de `severity_label` e `<modelos>` de `affected_models`, as funções da v2.3 (ino:1930 e 1904), que vão para `overlay_incident.cpp` (item 40) e são declaradas em `overlay_incident.h`. Sem modelo citado, o trecho ` · <modelos>` some, como no dialog de hoje.

#### 39. Telas movidas do `.ino` para `firmware/claude_stick/src/ui/` (Create, movidas + novo visual)

*   **Type / Scope**: Interface LVGL, um arquivo por tela. Cada uma sai do `.ino` com o visual novo (tema, fonte mono, dentro do painel quando for conteúdo do shell).
*   **Proposed Changes**:

    | Arquivo | Origem no `.ino` | Mudanças |
    |---|---|---|
    | `screens.h` | — | Declara a função de montagem de cada conteúdo (`screen_home_build(lv_obj_t*)`…), `screen_home_refresh()` (item 36) e as telas fora do shell |
    | `widgets.{h,cpp}` | `mklabel` (:240), `no_box` (:247), `mkbtn` (:254), `card` (:1172), `rrect` (:1205), `tlabel` (:1152), `tstatic` (:1160), `mkchip` (:1183), `set_chip` (:1197), `fmt_eta` (:289), `fmt_clock` (:301), `fmt_hm` (:307), `fmt_tok` (:314) | Sem mudança de comportamento. `update_tok_row` (:1500) vira `tok_text(const TokenStats&, char*, size_t)`, sem LVGL, usada pelo layout de 1 conta e por `/janela` |
    | `screen_pin.cpp` | `ui_pin` e teclado (:375-553) | Novo visual |
    | `screen_wifi.cpp` | `ui_wifi` (:556-644) | Na sessão, conteúdo `WIFI`: `scheduler_pause()`, `aguardando a rede…` até `poller_idle()`, e `scheduler_resume()` ao sair. No onboarding, continua em tela cheia |
    | `screen_token.cpp` | `ui_token` (:837-875), sem o trecho do servidor (item 6) | Na sessão, conteúdo `TOKEN`. Troca o logo Claude Code pelo `img_thep_logo_hdr` |
    | `screen_settings.cpp` | `ui_settings`, callbacks e as globais `g_wipeArmed`, `POLL_OPTS`, `TZ_OPTS` e `NTZ` (:2208-2213, 2217-2375) | Perde "Slideshow" e `g_slideLbl`. "Atualizar agora" chama `intent_refresh()`. A troca de fuso chama `scheduler_apply_tz()` (item 19) |
    | `screen_accounts.cpp` | `g_acctDelArmed` (:2215), `ui_accounts`, `ui_account_name` e callbacks (:2376-2563) | Tocar numa conta chama `intent_focus(slot)`. O marcador `•  ativa` (:2512) vira `•  foco`. A dica "So a conta ativa e consultada na API (as outras ficam dormentes)." (:2553) vira "Tocar numa conta define o foco.". Renomear continua com `accountSetLabel`, que já atualiza `g_accts.label`, a única fonte do rótulo (item 3). Remover segue o item 23 |
    | `screen_about.cpp` | `ui_about` (:2564-2611) | Troca o nome da placa e o logo Claude Code pelo logo THEP |
    | `screen_trend.cpp` | `build_tile_trend` (:1370-1427), `tr_x`/`tr_y`, `trend_redraw` (:1542-1660) | `/janela`. Usa `projection()`. A legenda passa a ser `projection_text` com `maxChars = 60`, no lugar das legendas longas da v2.3 ("Coletando dados…", "No ritmo atual…", "Uso estavel…", ino:1588-1658), que saem; assim a tela inicial e `/janela` dizem a mesma coisa (G7). A linha de projeção é desenhada só em `ESGOTA` (até `etaEpoch`, a 100%) e em `OK_ATE_RESET` com `rate > 0.02` (até `resetEpoch`, a `pctAtReset`), como na v2.3 (ino:1621); nos outros casos não há linha. Título do painel = rótulo da conta em foco; tocar nele chama `intent_focus_next()`. Mostra a linha da contagem de tokens (`tok_text`) |
    | `screen_heat.cpp` | `build_tile_heat` (:1428-1483), `heat_redraw` (:1662-1679) | `/ritmo`. Mesmo título e toque de `/janela` |

    *   **Saem** na fase 3, sem destino: `anim_opa_cb` (:665), `build_claude_mark` (:668), `tile_setup` (:1165), `set_meter` (:277), `build_win_card` (:1329), `set_ag_proj` (:1535), `status_color`/`overall_label` (:1137-1149, substituídas por `status_mark`, item 12), `on_tile_changed` (:1484), `dash_tick` (:1515), `set_hdr_status` (:2077), `refresh_cb` (:2092), `refresh_ui_values` (:2043, substituída por `screen_home_refresh`, item 36) e o slideshow.
    *   **Vão para `app_state.cpp`** (item 25): `load_persisted` (:325, sem ler a chave `slide`, que não é apagada), `save_attempts` (:346) e `apply_brightness` (:347).
    *   A `refBar` já saiu na fase 2 (item 24).

#### 40. `firmware/claude_stick/src/ui/overlay_moment.{h,cpp}`, `overlay_incident.{h,cpp}` (Create, movidos + novo visual)

*   **Type / Scope**: Overlays.
*   **Proposed Changes**:
    *   **Momento** (origem: `MomentUI`, `g_mo`, `show_moment`, `moment_close`, `moment_tick`, o que resta de ino:1684-1880 depois que `THR`, `g_thr*` e `check_thresholds` foram para o item 3 na fase 1):
        *   `bool show_moment(int slot, int win, int thr)` é o hook do agendador (item 19): devolve `false` sem fazer nada se já houver um momento aberto; senão, abre o overlay e devolve `true`. O fechamento por toque e por tempo chama `moment_flush()` depois de `moment_close()` (item 19);
        *   `show_moment` usa `img_bird_braille_124`, com tremor no 70% e cinza com borda vermelha piscando no 100%. Mostra o rótulo da conta, a % contando e a barra de blocos.
    *   **Incidente** (origem: ino:1882-2042, com as globais `g_incDlg`, `g_incAck`, `g_incSev` e `g_incMask` de 1887-1890): `incident_sync()`, `severity_label()` e `affected_models()` são declaradas em `overlay_incident.h`; usa `img_bird_braille_124` recolorido em cinza (item 28) e o chip de severidade, com as mesmas regras de dispensa e escalada da v2.3. Passa a aparecer sobre qualquer conteúdo do shell.
    *   Os dois chamam `overlay_restack()` (item 34).

#### 41. `firmware/claude_stick/logo_assets.h`, `firmware/claude_stick/clawd_dead.h`, `tools/gen_logo_assets.py`, `tools/gen_dead_clawd.py` (Delete)

*   **Type / Scope**: Assets do Clawd e do logo Claude Code.
*   **Proposed Changes**: removidos (decisão do dono). Saem junto `model_mood` (:1216), `build_accessory` (:1229), `build_model_mascot` (:1255) e a animação dos mascotes no `loop()` (ino:2861-2889). A recriação dos mascotes quando o estado do modelo muda (ino:2722-2730) já saiu na fase 2, dentro de `bg_refresh`. As entradas desses scripts saem no item 46: `assets/brand/claudecode-text.svg` e `clawd-dead.png` são apagados; `claudecode-color.svg` fica, porque `tools/gen_banners.py` ainda o usa (item 46).

#### 42. `firmware/claude_stick/lv_conf.h` (Modify, fase 3)

*   **Type / Scope**: Configuração do LVGL.
*   **Proposed Changes**:
    *   `LV_USE_TILEVIEW 0`.
    *   Mantém `LV_FONT_MONTSERRAT_14` (fallback de símbolos) e desliga as Montserrat que deixarem de ser usadas (12, 16–48), depois de conferir com grep.
    *   Log só na build de desenvolvimento:

        ```c
        #ifdef THEP_DEV
          #define LV_USE_LOG 1
          #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
          #define LV_LOG_PRINTF 0      /* a saída vai pelo callback abaixo */
        #endif
        ```

        No `setup()`, com `THEP_DEV`, `lv_log_register_print_cb(cb)` (`lv_log.h:72`) registra um callback que escreve na `Serial`.

#### 43. `tools/gen_mockups.py` (Modify)

*   **Type / Scope**: Gerador de mockups do README.
*   **Proposed Changes**:
    *   Passa a usar as **mesmas fontes** do firmware, lidas de `tools/.cache/fonts/` (baixadas pelo `gen_fonts.sh`), no lugar de `/System/Library/Fonts/HelveticaNeue.ttc` (`gen_mockups.py:26`), e os assets de `gen_hermes_assets.py`.
    *   Gera `assets/mock-home-{1,2,3,4}.png`, `assets/mock-menu.png`, `assets/mock-modelos.png`, `assets/mock-janela5h.png`, `assets/mock-ritmo.png`, `assets/mock-incidente.png` e `assets/mock-momento.png`.
    *   Para cada texto da tabela do item 37, mede a largura com a fonte real e **falha** se passar da largura do card (item 36). As barras são medidas com os 2 colchetes.
    *   Deixa de gerar `assets/mock-agora.png` e `assets/mock-contas.png`. Os arquivos são apagados no item 46, junto com o README que os usa.

#### 44. `.gitignore` (Modify)

*   **Type / Scope**: Arquivos ignorados.
*   **Proposed Changes**: acrescentar `tools/.cache/`, onde ficam as TTF baixadas.

### Fase 4 — Bridge, documentação e fechamento

#### 45. `tools/token_bridge.py` (Modify)

*   **Type / Scope**: Bridge de tokens (Python, só stdlib). Fica na fase 4 e não bloqueia as fases 2 e 3.
*   **Proposed Changes**:
    *   Novo `--config-dir DIR` (padrão `~/.claude`). A global `PROJECTS` (`token_bridge.py:33`) sai, e `collect(start, projects_dir)` recebe o diretório `DIR/projects` como parâmetro.
    *   O glob vira recursivo (`glob.glob(os.path.join(projects_dir, "**", "*.jsonl"), recursive=True)`), corrigindo o item do drift report (`token_bridge.py:49`).
    *   `push()` passa a chamar `GET /window?account=<rótulo>` quando há `--account`, com o rótulo codificado por `urllib.parse.quote(account, safe="")`.
    *   O `urlopen` do `GET /window` entra num `try`: um `HTTPError` 404 vira a mensagem "rótulo não existe no gadget" e o push é pulado.
    *   O ramo "conta diferente" (`token_bridge.py:87-90`) **fica**, de propósito: contra um aparelho v2.3, que ignora `?account=` e devolve a conta ativa, ele evita enviar a contagem para a conta errada.
    *   No `POST /tokens`, um 404 vira a mesma mensagem. O 409 continua tratado, para compatibilidade com a v2.3.

#### 46. `README.md`, `README.pt-BR.md` (Modify); `assets/mock-agora.png`, `assets/mock-contas.png`, `assets/brand/claudecode-text.svg`, `assets/brand/clawd-dead.png` (Delete)

*   **Type / Scope**: Documentação e assets antigos.
*   **Proposed Changes**:
    *   Reescrever "Screens" para a v3: tela inicial por quantidade de contas, menu, ponto de status com a tabela de cores, `/modelos` em texto. As imagens passam a ser as do item 43.
    *   Trocar o logo do topo (`README.md:3`, `README.pt-BR.md:3`, hoje `claudecode-color.svg`) pelo logo THEP.
    *   **Banners não mudam:** `assets/brand/claudecode-color.svg` fica no repositório, porque `tools/gen_banners.py:53` o passa ao `rsvg-convert`. Os banners (`assets/banner-{steps,web}{,-pt}.png`, `README.md:36, 44`) anunciam o gravador web e usam a paleta do site (`gen_banners.py:28-33`), que está fora do escopo (Non-Goals). `gen_banners.py` e os 4 PNGs ficam como estão.
    *   Trocar "Only the active account is polled" por "every account is polled", com o custo (requisições por hora, visíveis aos admins).
    *   Seção de segurança: até 4 tokens em RAM.
    *   Token bridge: `--config-dir`, aliases por conta e `?account=`.
    *   Atualizar o layout do repositório (sem `gen_logo_assets.py`, com `gen_hermes_assets.py`, `gen_fonts.sh` e `firmware/tests/`) e o badge `firmware-v3.0`.
    *   Pré-requisitos: core `esp32:esp32` **3.3.8** (`README.md:384`, `README.pt-BR.md:387`).
    *   Fechar os itens do drift report: ordem dos ajustes, `STATUS_POLL_SEC`, cor do chip, partições OTA e layout incompleto.
    *   Seção "Rollback para a v2.3".
    *   Apagar os 4 assets listados no título, que deixam de ser referenciados (`claudecode-text.svg` e `clawd-dead.png` só eram usados por `gen_logo_assets.py`, `gen_dead_clawd.py` e pelo `gen_mockups.py` antigo).

#### 47. `firmware/README.md`, `firmware/REFERENCIA-HARDWARE-LVGL.md`, `firmware/claude_stick/build.sh` (Modify)

*   **Type / Scope**: Documentação do firmware.
*   **Proposed Changes**:
    *   `firmware/README.md`: descrever os módulos de `src/`, a regra de concorrência (`net_task` × `loop()`), `tools/gen_fonts.sh`, `tools/gen_hermes_assets.py`, `firmware/tests/run.sh` e os comandos `dev`/`upload-dev`.
    *   `firmware/README.md`, **dependências de ferramenta**:
        *   Python 3 com Pillow (`gen_hermes_assets.py`, `gen_mockups.py`) e `fontTools` (checagem de glifos do `gen_fonts.sh`);
        *   Node.js com `npx` (`lv_font_conv@1.5.3`);
        *   `g++` com C++17 (`firmware/tests/run.sh`);
        *   para o tamanho das fontes no `gen_fonts.sh`: `~/Library/Arduino15/packages/esp32/tools/esp-x32/2601/bin/xtensa-esp32s3-elf-gcc` (a toolchain instalada com o core 3.3.8), com `-c -Os -DLV_CONF_INCLUDE_SIMPLE -I firmware/claude_stick -I ~/Documents/Arduino/libraries/lvgl`.
    *   `firmware/REFERENCIA-HARDWARE-LVGL.md`: core 3.3.8 (linhas 52 e 185) e a nota de imagens (linha 249), que passa a citar `gen_hermes_assets.py` e RGB565A8.
    *   `build.sh`: comentário de pré-requisitos (linha 12) com o core 3.3.8.

### Verification Plan

*   **Compilation/Build**:
    *   `cd firmware/claude_stick && ./build.sh` sem warnings novos em relação ao número registrado na fase 0 (item 1).
    *   App abaixo de 2,8 MB; o próprio `build.sh` falha se passar (item 8).
    *   `./build.sh dev` também compila.
*   **Unit Tests**:
    *   `firmware/tests/run.sh` (g++ no computador) cobrindo exatamente a lista do item 13: `projection` e `projection_text`; `dot_state`; `classify_usage`, `probe_needs_retry` e `model_status_text`; `level_for`, `text_bar`, `layout_for_count`, `fit_label`, `age_text` e `status_mark`.
    *   Não há CI. A partir da fase 1, quem abre o PR roda `firmware/tests/run.sh` e cola a saída no PR.
*   **Static Analysis & Linting**:
    *   `g++ -std=c++17 -Wall -Wextra -Werror` nos testes (as mesmas flags do item 13).
    *   `python3 -m py_compile tools/*.py`.
    *   `firmware/tests/run.sh` também roda um grep que falha se aparecer `lv_` em `src/net/`, o que bloqueia chamadas diretas ao LVGL no código de rede.
*   **Test Coverage Requirement**: todas as regras do item 10, todas as linhas da tabela de classes da §4 (item 11) e todos os casos de borda de `projection` listados no item 13.
*   **Mockups**: `python3 tools/gen_mockups.py` sem estouro de largura nos 4 layouts.
*   **E2E/Integration no aparelho**, cada um registrado no PR da fase:
    1.  Gravar a v3 por cima de uma v2.3 com 2 contas reais. PIN, contas e histórico continuam.
    2.  Teste de 1 hora, com build `THEP_DEV`, intervalo de 30 s e 4 slots. São 2 contas reais, com cada token repetido em 2 slots; isso dobra as chamadas contra cada conta durante o teste. Pelo serial e pelo `/health`, registrar:
        *   heap interno mínimo e maior bloco livre;
        *   `lv_mem_used_pct`;
        *   high-water da stack da `net_task`;
        *   duração de cada ciclo e `frame_ms_max`.

        Critérios: nenhum reset, heap interno acima de 60 KB, bloco de 45 KB ou mais e quadro abaixo de 40 ms.
    3.  Com 2 contas, tocar no menu durante um ciclo de consultas: responde em até 100 ms.
    4.  Com 2 contas, tirar o WiFi: o ponto fica vermelho, os cards mostram `⚠ NOWIFI` e o log mostra os jobs `RECONNECT`. Ao voltar o WiFi, o ponto passa a azul no fim do ciclo seguinte e depois a verde.
    5.  Com 2 contas, WiFi conectado e sem internet (roteador sem WAN), desbloquear: os cards ficam `consultando…` e o ponto fica azul (primeiro ciclo) por até 60 s. Depois de 60 s, os cards mostram `⚠ NOCLOCK` e o ponto fica vermelho. Ao ligar a WAN, o NTP sincroniza, o primeiro ciclo roda e o ponto passa a azul e depois a verde.
    6.  Com 2 contas, revogar o token OAuth de uma delas (CUJ 2). Um token inválido colado pela página é recusado no `VALIDATE` e nunca é salvo, então o teste usa um token que já está salvo. Ponto amarelo, card com `⚠ AUTH · <idade>` e espera crescente visível no log (`[CYC] slot N backoff …`). Repetir logo depois de trocar o token por outro que também foi revogado: o card mostra `⚠ AUTH` com `--%`.
    7.  Com 2 contas, remover uma conta no meio de um ciclo: o `/histN.bin` dela não é recriado e o layout muda de 2 para 1. Logo em seguida, ainda no mesmo ciclo, adicionar uma conta no mesmo slot: o resultado atrasado da conta removida é descartado (`gen` preservado por `acct_clear`, item 3), e a conta nova aparece como `consultando…` até a primeira resposta dela.
    8.  Bridge com dois `--config-dir` e `--account` diferentes: cada conta recebe a própria contagem de tokens, e um rótulo errado devolve 404.
    9.  Gravar a v2.3 de volta (só o app): a v2.3 abre com PIN, a conta em foco vira a ativa e o histórico continua.

--------------------------------------------------------------------------------

## Implementation notes

Desvios do plano registrados durante a implementação e a validação na placa (2026-11-09). O resto do documento continua descrevendo o plano aprovado; onde houver conflito, vale esta seção.

*   **Core `esp32:esp32` 3.3.8.** A v3 foi compilada e validada nessa versão. README, `build.sh`, `flash.sh` e `firmware/REFERENCIA-HARDWARE-LVGL.md` foram corrigidos (itens 46 e 47).
*   **Resultado do spike do display (item 34).** O AXS15231B em QSPI **ignora a janela de linhas (RASET)**: o modo parcial com a faixa exata desenhou deslocado. O padrão ficou `DISP_BANDS=1` + `DISP_ROT_FAST=1` (`config.h`): render `PARTIAL`, rotação de 270° em blocos 16×16 (`src/logic/rot270`) e envio de linhas nativas inteiras sempre a partir da linha 0 (o prefixo `0..r1` que cobre o que mudou). Medido: pior quadro 44–46 ms (`rows_max` 459, na troca do relógio do rodapé), rotação < 1 ms, envio ~37 ms; o modo FULL anterior dava ~96–109 ms. O pulso ficou no plano: 4 degraus a cada 400 ms (ciclo de 1,6 s). O NFR de 40 ms é atendido, exceto nos quadros que tocam a borda esquerda da paisagem. Detalhes em `firmware/REFERENCIA-HARDWARE-LVGL.md` §4.
*   **Layout de 2 contas com texto secundário em 13 px** (era 11 px no item 37), com as linhas distribuídas pela altura do card; a projeção cabe em 26 caracteres. Pedido do dono.
*   **Rodapé sem o pássaro de 13 px e sem "thep":** começa direto na contagem de contas. Pedido do dono.
*   **Layout de 3 contas com rótulo de até 10 caracteres** (era 11), para caber ao lado da marca de estado de 8 (conferido pelo `tools/gen_mockups.py`).
*   **Novo estado de projeção `JANELA_PASSOU`.** Quando o reset da 5h já passou (`resetEpoch <= now`) e a próxima leitura ainda não chegou, `projection()` devolve `JANELA_PASSOU` (regra 1b, entre as regras 1 e 2), o card mostra `— janela reiniciou` e o reset vira `passou · HH:MM` (`reset_text`), em vez de uma contagem parada.
*   **Memória medida:** heap interno livre ~166 KB durante os ciclos, maior bloco ~98–106 KB, pool do LVGL na PSRAM. App ~1,64 MB depois da remoção do Clawd e das Montserrat não usadas (v2.3: ~2,05 MB).
*   **Rollback:** os binários da v2.3 ficam guardados localmente em `backups/` (no `.gitignore`); o rollback grava só a partição de app (README, "Rollback to v2.3").

--------------------------------------------------------------------------------

*Drafted with EGM*
