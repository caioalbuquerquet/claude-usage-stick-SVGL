# Goldfish Review Report — Stage 5 (Initial Zoom Out Review)

**Alvo:** `/Users/caiotorres/Development/claude-usage-stick/docs/designs/thep-usage-stick-v3.md` (linhas 1–917)

## Status dos revisores

| Revisor | Status |
| --- | --- |
| `egm-zoom-out` | completed |
| `egm-inventive-catalyst` | completed |

**Resumo:** 0 🔴 Blocker · 11 🟡 Major · 8 🟢 Nit (já sem duplicatas).

Os dois revisores concordam que o documento resolve o problema certo: a dor de ver as duas contas ao mesmo tempo é resolvida na Fase 2, antes da parte visual. Nenhum achou Blocker. Quase todos os achados cortam trabalho ou risco sem perder nenhum objetivo. Os achados M1 a M5 deveriam ser resolvidos antes de começar a Fase 2 e a Fase 3.

### Tensões entre os revisores (decisão do autor)

- **Bridge por conta (M6 x M11).** O zoom-out quer tirar da Fase 2 o contrato do bridge com uma instância por conta. O inventive-catalyst quer usar esse mesmo bridge como sinal de "houve uso" para consultar menos. Se M11 for aceito, o bridge por conta deixa de ser só para a linha de tokens e pode justificar ficar mais cedo. Se não for, M6 vale como está.
- **Ícones e Montserrat 14 (M9).** O zoom-out sugere pegar o `↻` do `LV_SYMBOL_REFRESH` usando a Montserrat 14 como fallback. O inventive-catalyst sugere tirar a Montserrat 14 e embutir os glifos na fonte mono. As duas opções tiram as fontes DejaVu e Noto. A escolha é entre ficar com a Montserrat 14 ou com a mistura de glifos.

---

## Achados priorizados (Major → Nit)

### 🟡 M1. A regra de dado velho deixa amarela uma conta saudável e o azul nunca acende — Severidade: Major
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** §4 (linhas 219–220 e 231), §2 "Regras" (linhas 172–173), item 11 (linha 574)
* **Problema:** "dado velho" é `agora − usageAtEpoch > 2 × intervalo`. Só que o próximo ciclo começa em `fim + intervalo`, e um ciclo pode levar de 40 a 48 s (4 timeouts de 8 s mais `STATUS` e `PROBE`).
  * Exemplo com intervalo de 30 s: a conta 1 responde em t=2, o ciclo termina em ~48 s e o próximo começa em ~78 s. A idade dela chega a ~78 s, acima de 60 s, e a conta fica amarela por culpa das outras.
  * O azul é medido "a partir do fim do ciclo", mas o `AccountView` só tem o `lastTryEndEpoch` de cada conta, sem o horário de fim do ciclo. Com ciclos longos, o azul não acende.
* **Correção sugerida:**
  * Definir "dado velho" em ciclos: nenhum sucesso nos últimos 2 ciclos concluídos. Alternativa: `idade > intervalo + duração do último ciclo + margem`.
  * Passar `cycleEndEpoch` para o `dot_state` e calcular o azul a partir dele.
  * Incluir casos de ciclo longo na tabela de testes do item 13.

### 🟡 M2. O watchdog com `vTaskDelete` vaza RAM interna e dispara sem motivo — Severidade: Major
* **Revisores:** `egm-zoom-out`, `egm-inventive-catalyst`
* **Onde:** §4 "Watchdog" (linha 235), item 15 (linha 633), NFR de RAM interna (linha 422)
* **Problema:**
  * Matar a task no meio de uma sessão TLS não libera os 40–50 KB do mbedTLS e não fecha o socket. Depois de uma ou duas recriações, o requisito de ≥60 KB livres e um bloco de 45 KB deixa de ser cumprido.
  * O critério "não publicou nada em 3 × intervalo" também é atingido quando simplesmente não havia job. Exemplo: 1 conta em espera de AUTH, `/modelos` fechada e intervalo de 5 min. Nesse caso uma task saudável é recriada.
  * A regra de dado velho já avisa o usuário quando a task para.
* **Correção sugerida:** tirar a recriação via `vTaskDelete`. Se a recuperação for mesmo necessária:
  * detectar "job em voo há mais de `API_TIMEOUT_MS` + margem", usando um timestamp de início do job que a task publica;
  * recuperar com `esp_restart()`;
  * documentar que, depois do restart, o PIN é pedido de novo.

  Se não, só registrar no log. A constante `NET_WATCHDOG_MULT` sai ou muda de significado.

### 🟡 M3. "Esperar a task ficar ociosa" trava a interface que o G3 promete não travar — Severidade: Major
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** §1 (linha 145), item 19 (linha 675), item 36 (linha 836), Verification Plan, teste 6 (linha 911)
* **Problema:** o `gen` por slot já descarta resultados atrasados, e quem grava `/histN.bin` é o `loop()`. Mesmo assim, o `loop()` espera a task terminar. Isso leva até 8 s e, se a task travar, prende até o watchdog agir (90 s ou mais).
* **Correção sugerida:** ao remover uma conta, trocar o token ou usar "Apagar tudo":
  1. `acct_bump_gen`;
  2. `xQueueReset(jobQ)`;
  3. apagar o slot e seguir, sem esperar.

  O job em voo termina e o resultado é descartado pelo `gen`. Registrar na seção de segurança que uma requisição com o token removido pode continuar em voo por até 8 s.

### 🟡 M4. O ponto pulsa quase sempre, e o plano B (`PARTIAL`) não reduz o custo — Severidade: Major
* **Revisores:** `egm-zoom-out`, `egm-inventive-catalyst`
* **Onde:** §4, tabela do ponto (linhas 226–231, em especial a 229), §5 "Pulso do ponto" (linha 276), item 29 (linha 761), NFR "Quadros" (linha 421)
* **Problema:**
  * O verde está descrito como "Fixo, com respiração lenta", o que se contradiz e, na prática, é animação contínua.
  * O azul pulsa ~10 s a cada 30 s, e o amarelo e o vermelho também pulsam. O resultado é redesenhar a tela inteira a 8 Hz quase o tempo todo, no core 1, que também atende o toque e o `WebServer`.
  * O plano B não resolve. O `disp_flush_cb` (`claude_stick.ino:215-224`) ignora a área recebida, gira os 480×320 e envia o framebuffer inteiro, então mesmo em `PARTIAL` cada passo do pulso custa um quadro cheio.
* **Correção sugerida:**
  * Verde realmente fixo.
  * Azul como um único flash curto ao fim do ciclo.
  * Amarelo e vermelho pulsando a 2–4 Hz.
  * Trocar o plano B por baixar a frequência. Se o `PARTIAL` ficar no plano, especificar um flush que envie só o retângulo alterado e confirmar que o AXS15231B aceita janelas parciais por QSPI.

### 🟡 M5. O limite de RAM interna é tratado como fixo, mas pode ser removido — Severidade: Major
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** §1 "Uma conexão por vez" (linha 144), Alternativa E (linhas 375–379), NFR "RAM interna" (linha 422), Future Improvements (linha 444)
* **Problema:** o desenho serial e o pior caso de ~40 s partem de dois fatos:
  * o pool do LVGL ocupa 96 KB internos (`lv_conf.h:35-38`);
  * cada conta abre um TLS novo, e esse abre-e-fecha de blocos de 40–50 KB é a causa mais provável de fragmentação.
* **Correção sugerida:**
  * **(a)** Mover o pool do LVGL para a PSRAM, com `LV_STDLIB_CUSTOM` + `heap_caps_malloc(MALLOC_CAP_SPIRAM)` ou `LV_MEM_POOL_ALLOC`. O concepts brief já sugeria isso (I16).
  * **(b)** Trazer o `setReuse(true)` para a Fase 2: uma conexão com `api.anthropic.com` por ciclo, trocando só o `Authorization`, e reconectar uma vez se o servidor fechar.
  * Medir o impacto de render de (a) junto com M4.

### 🟡 M6. A mudança no bridge está no caminho crítico, mas serve a uma linha que o dono não vê — Severidade: Major
* **Revisor:** `egm-zoom-out`
* **Onde:** §8 "Contrato com o token bridge" (linhas 324–335), Fase 2 (linha 345), item 21 (linha 690)
* **Problema:** a linha de tokens só aparece no layout de 1 conta e em `/janela`. Com 2 contas, ela nunca aparece na tela inicial. Mesmo assim, a Fase 2, marcada como "G1, G3", inclui:
  * roteamento por rótulo;
  * `?account=` e o array `accounts[]` no `/window`;
  * troca do 409 pelo 404;
  * a opção `--config-dir`;
  * um `CLAUDE_CONFIG_DIR` por conta, com novo login em cada uma.
* **Correção sugerida:**
  * Levar esse contrato para a Fase 4 ou para uma fase opcional. Na Fase 2, manter o comportamento de hoje: os tokens vão para a conta em foco.
  * Cortar o `accounts[]`, que ninguém consome.
  * Registrar em "O que a mudança custa" que separar o `CLAUDE_CONFIG_DIR` muda o jeito de usar o Claude Code.
  * Ver também a tensão com M11.

### 🟡 M7. `PROBE` e `USAGE` são a mesma requisição e são pagos duas vezes — Severidade: Major
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** §2 "Agendamento das consultas", passo 4 (linha 169), item 16 (linha 643), The Problem "Cota e visibilidade" (linha 38)
* **Problema:** `fetchUsage` e `probeModel` (`api.cpp:26-120`) fazem o mesmo `POST /v1/messages` com `max_tokens:1`. Só mudam o modelo e a coleta de headers. Com `/modelos` aberta, a conta em foco gera duas requisições por ciclo.
* **Correção sugerida:**
  * Criar um único tipo de job, "mensagem para o modelo X", que sempre coleta os headers `unified-*`. O `USAGE` da conta em foco passa pelo modelo da vez no rodízio, e o job `PROBE` deixa de existir.
  * Se a resposta vier sem headers, repetir com o `PROBE_MODEL`.
  * Separar "modelo falhou" de "conta falhou", para que um 503 do Opus não deixe o ponto amarelo.
  * Medir antes se o consumo de cota muda conforme o modelo.

### 🟡 M8. A Fase 0 divide o `.ino` inteiro em cerca de 20 arquivos antes de entregar valor — Severidade: Major
* **Revisor:** `egm-zoom-out`
* **Onde:** Fase 0, itens 2–8 (linhas 453–542), tabela de fases (linha 343)
* **Problema:** mover 2.906 linhas de uma vez, sem testes no aparelho, é um refactor arriscado que fica antes da dor principal. As fases seguintes só precisam de:
  * `logic/`, para os testes no computador;
  * `account_state` e `history`;
  * `poller` e `scheduler`.
* **Correção sugerida:** reduzir a Fase 0 à tag, à linha de base e a essas extrações. Cada tela vai para o seu arquivo quando for repintada (Fase 3, item 36). Ver também N8.

### 🟡 M9. O pipeline de fontes é maior que o necessário — Severidade: Major
* **Revisores:** `egm-zoom-out`, `egm-inventive-catalyst`
* **Onde:** §6 (linhas 291–309, "Símbolos do LVGL" na 307), itens 25–27 (até a linha 736), item 32 (linha 801), item 38 (linha 849)
* **Problema:**
  * São quatro tamanhos de texto (10, 11, 12 e 13 px), com 1 px de diferença entre eles.
  * DejaVu e Noto entram só para `↻` e `⏲`, com duas licenças, downloads com sha256 e junção de glifos.
  * A Montserrat 14 como fallback de fontes de 10–13 px deixa os ícones maiores e com outra linha de base.
  * A tabela do item 32 pede "jbmono_13 **bold**", mas o plano só gera a JetBrains Mono Regular.
* **Correção sugerida:**
  * Usar dois tamanhos de texto (por exemplo 11 e 13).
  * Tirar o `⏲`, que não acrescenta nada ao lado do `HH:MM`.
  * Para o `↻`, ou usar o `LV_SYMBOL_REFRESH` da Montserrat 14, ou embutir na fonte mono os glifos FontAwesome em uso e desligar a Montserrat 14 (ver "Tensões").
  * Corrigir o "bold" na linha 801 para Regular ou para a cor de destaque.

### 🟡 M10. Quatro layouts para um dono que tem 2 contas — Severidade: Major (depende do dono)
* **Revisor:** `egm-zoom-out`
* **Onde:** G2 (linha 50), tabela da §5 (linhas 265–270), itens 32 e 39
* **Problema:** cada layout custa uma fonte numérica (20, 30, 38 e 50 px), uma coluna da tabela de campos, um mockup e um teste. O layout de 3 colunas é o mais apertado: rótulo de 11 caracteres e projeção cortada em 20. O dono pediu para guardar até 4 contas, não 4 disposições diferentes.
* **Correção sugerida:** com 3 contas, usar a grade 2×2 com a quarta célula vazia ou com "+ adicionar". Isso tira um layout, a `jbmono_num_30` e uma coluna. Precisa de confirmação do dono, porque os mockups aprovados mostram as 4 disposições.

### 🟡 M11. 480 requisições/h aceitas como custo fixo, sendo que o bridge já sinaliza o uso — Severidade: Major
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** Non-Goals (linha 77), Future Improvements (linha 443), §8 (linhas 326–334)
* **Problema:** a % só muda quando a conta é usada ou no reset. O bridge por conta já faz `POST /tokens` exatamente quando aquela conta é usada no PC.
* **Correção sugerida:** o intervalo do usuário vira um teto. A conta só é consultada no ciclo se:
  * recebeu `POST /tokens` desde a última consulta; ou
  * passou do horário de reset; ou
  * a % mudou nas últimas 2 consultas; ou
  * passaram 5 min.

  Sem bridge rodando, vale o comportamento atual. Nesse caso, a regra do azul passa a considerar só as contas consultadas no ciclo. Custo: o uso feito fora daquele PC só aparece no teto de 5 min.

---

### 🟢 N1. A variante cinza do pássaro como imagem separada — Severidade: Nit
* **Revisor:** `egm-zoom-out`
* **Onde:** item 24 (linha 717), §6 (linha 313)
* **Correção sugerida:** deixar o pássaro cinza com `lv_obj_set_style_image_recolor`, como o firmware já faz (`claude_stick.ino:1285`, `1302`, `1792`), e não gerar a `img_bird_braille_gray_96`. Conferir também se um braille de 13×13 px é legível.

### 🟢 N2. A tabela de campos tem duas fontes da verdade, e elas já divergem — Severidade: Nit
* **Revisor:** `egm-zoom-out`
* **Onde:** §5 (linhas 265–270), item 32 (linhas 794–806), item 39 (linha 858)
* **Problema:** no layout de 4 contas, a §5 mostra "semana + reset", e o item 32 mostra "—".
* **Correção sugerida:** na §5, só remeter para o item 32. Fazer o `gen_mockups.py` ler a mesma tabela, ou usá-lo como conferência pontual, e não como trava permanente.

### 🟢 N3. A fila de momentos de limiar entre ciclos — Severidade: Nit
* **Revisor:** `egm-zoom-out`
* **Onde:** §5 (linha 274), item 35
* **Correção sugerida:** mostrar só o momento de maior limiar do ciclo e descartar os outros, para não exibir um momento já desatualizado.

### 🟢 N4. `GET /health` com muitas métricas — Severidade: Nit
* **Revisor:** `egm-zoom-out`
* **Onde:** item 6 (linha 515), NFR (linha 418), verificação (linha 901)
* **Correção sugerida:** deixar o endpoint só na build `THEP_DEV`, ou usar o serial para o teste de longa duração.

### 🟢 N5. Detalhe de código no Technical Plan — Severidade: Nit
* **Revisor:** `egm-zoom-out`
* **Onde:** §1 (linhas 140–141), tabela da §3 (linhas 181–190)
* **Correção sugerida:** no Technical Plan, dizer só "o job leva uma cópia do token e o `gen`". Os campos de struct, o `token[200]` e os tamanhos de stack ficam nos itens 15 e 18.

### 🟢 N6. Etapas do processo de escrita misturadas ao plano — Severidade: Nit
* **Revisor:** `egm-zoom-out`
* **Onde:** item 42 (linhas 880–883)
* **Correção sugerida:** tirar o item. "Step 6" e o `design_draft.md` são passos de quem escreveu o documento, e as notas já estão em `docs/designs/notes/`.

### 🟢 N7. Tokens copiados na `jobQ` continuam no buffer da fila — Severidade: Nit
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** §1 (linhas 140–143), item 15 (linhas 623–625), §7 "Segurança" (linha 320)
* **Problema:** a FreeRTOS copia o item para o armazenamento da fila. O `memset` na task só zera a cópia local.
* **Correção sugerida:** criar a fila com `xQueueCreateStatic` e dar `memset` no armazenamento logo depois do `xQueueReset`, no bloqueio, na remoção e no "Apagar tudo". Também reduzir a `jobQ` ao máximo real de jobs de um ciclo.

### 🟢 N8. A home de 2 colunas da Fase 2 é jogada fora na Fase 3 — Severidade: Nit
* **Revisor:** `egm-inventive-catalyst`
* **Onde:** item 20 (linhas 677–682), §9 (linha 345), item 31 (linha 773)
* **Correção sugerida:** na Fase 2, criar já o `screen_home.cpp` guiado pela tabela de campos e pelo `layout_for_count`, usando as fontes atuais. A Fase 3 troca só o tema e as fontes. Com isso a tabela de campos precisa existir antes da Fase 2. Combina com M8.

---

## O que está certo e deve ficar (consenso)

- Task serial em vez de uma task por conta (Alternativa E).
- Menu como modal sobre um shell fixo (Alternativa F), que elimina o `stop_web()` chamado a cada troca de estado.
- `gen` por slot contra a corrida entre consulta e remoção.
- 429 com headers contado como dado.
- Funções puras com testes no computador, e projeção numa função única.
- Rollback sem mudar partições nem formato.
- Job `VALIDATE` assíncrono.
