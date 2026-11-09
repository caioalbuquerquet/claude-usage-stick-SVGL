# Goldfish Review Report: Etapa 8 (Zoom Out intermediário)

**Alvo:** `/Users/caiotorres/Development/claude-usage-stick/docs/designs/thep-usage-stick-v3.md` (arquivo inteiro)
**Padrão:** `zoom-out-standard.md`
**Base de comparação:** `review/zoom_out_report.md` (etapa 5) e `review/goldfish_review_report.md`

## Status dos revisores

| Revisor | Status |
| --- | --- |
| `egm-zoom-out` | completed |

**Resumo:** 🔴 0 · 🟡 4 · 🟢 10

**Veredito:** nenhum bloqueador. A maior parte das mudanças deixou o design mais simples. Os ganhos foram:

- o `PROBE` passou para dentro do `USAGE`;
- o bridge foi para a fase 4;
- o documento usa uma família de fonte só;
- o `/health` existe só na build `THEP_DEV`.

Sobraram três tipos de problema:

- duas mitigações criaram defeitos novos: a `xQueueReset` e o rodízio de modelos na consulta da conta;
- metade da regra de "falhou" nunca é acionada;
- alguns trechos antigos contradizem as correções.

---

## Achados por prioridade (Bloqueadores -> Maiores -> Menores)

### 🟡 1. Remover uma conta esvazia a fila inteira e pode travar o ciclo (Maior)
* **Revisor:** `egm-zoom-out`
* **Onde:** §1, linha 184; item 19, linha 853
* **Problema:**
  * A `xQueueReset(jobQ)` apaga os jobs de todas as contas do ciclo, inclusive um `VALIDATE` que esteja na fila.
  * O documento não diz como o agendador encerra um ciclo cujos resultados nunca vão chegar.
  * Também não diz o que acontece com a página de token, que pode ficar em `pending` para sempre.
  * Pela linha 298, a falta de jobs não provoca reinício. Então o ciclo pode nunca terminar.
* **Correção:**
  * Tirar a `xQueueReset`. O `gen` já descarta resultados atrasados e o `token_store_clear(slot)` já esvazia o token.
  * Antes de executar um job, a task confere o token. Se o slot estiver vazio, ela devolve na hora um resultado "descartado", sem abrir conexão.
  * O ciclo continua sendo contado pelos resultados que chegam.

### 🟡 2. O rodízio de modelos pode deixar amarela uma conta saudável (Maior)
* **Revisor:** `egm-zoom-out`
* **Onde:** §2, linhas 207–209; §4, linha 265; item 15, linha 793; item 16, linha 814
* **Problema:**
  * A consulta da conta em foco agora usa Opus ou Fable.
  * Se esse modelo responder 404, 400, 529 ou 503 sem headers `unified-*`, o erro conta como falha da conta.
  * A linha 265 diz que a sonda não afeta o ponto, mas sonda e consulta da conta agora são a mesma requisição.
  * A proposta M7 da etapa 5 tinha duas salvaguardas, e as duas sumiram: repetir a consulta com `PROBE_MODEL` e tratar falha do modelo como diferente de falha da conta.
* **Correção:** acrescentar esta regra na §2 e na tabela da §4.
  * Se a resposta tiver headers `unified-*`, a conta está OK, seja qual for o código HTTP. O código fica registrado só para o modelo.
  * Se não tiver esses headers, a resposta vale só para o modelo. Na hora, sai um `USAGE` com `PROBE_MODEL`, e é ele que define a classe da conta.
  * Registrar esse caso em Risks.

### 🟡 3. A segunda metade da regra de "falhou" nunca é acionada (Maior)
* **Revisor:** `egm-zoom-out`
* **Onde:** §4, linha 269; item 11, linhas 738–740; item 13, linha 766; item 16, linha 818; Risks, linha 568
* **Problema:** a condição "tem dado, relógio válido e nenhum `OK` em 2 ciclos" não cobre nenhum caso real.
  * **Conta em espera:** ela só entra em espera depois de uma falha `AUTH`. Essa falha já basta para o estado "falhou".
  * **Task travada:** o reinício acontece em 36 s, antes de fecharem 2 ciclos (no mínimo uns 60 s).
  * **Custo:** essa metade exige `cyclesSinceOk`, `clockValid`, `usageAtEpoch` e testes próprios.
  * **Inconsistências:** a linha 766 ainda testa a regra antiga, por tempo. A linha 568 conta com essa regra para a task travada.
* **Correção:**
  * "Falhou" passa a ser "a última tentativa concluída é falha, ou `keyOk = false`".
  * Tirar os três campos do `AccountView` e do agendador.
  * Corrigir as linhas 766 e 568.
  * Manter o estado `pendente`: ele é necessário e proporcional.

### 🟡 4. O pool do LVGL na PSRAM não tem item no plano e o mecanismo descrito não funciona (Maior)
* **Revisor:** `egm-zoom-out`
* **Onde:** §1, linha 180; Risks, linha 562; item 38, linhas 1027–1033
* **Problema:**
  * Essa é a principal mitigação de RAM interna, e a fase 2 depende dela. Mesmo assim, nenhuma fase se encarrega dela.
  * `LV_MEM_ADR` é um endereço fixo definido na compilação. Não dá para usá-lo para "alocar com `MALLOC_CAP_SPIRAM`".
  * Hoje o pool é `LV_STDLIB_BUILTIN` com 96 KB (`lv_conf.h:35-38`).
* **Correção:**
  * Criar um item na fase 2 que use `LV_MEM_POOL_INCLUDE <esp_heap_caps.h>` e `LV_MEM_POOL_ALLOC(size)` → `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`.
  * Medir o heap antes e depois da mudança.
  * Incluir o pool na PSRAM no spike de tempo de quadro da fase 3.

### 🟢 5. Na §1, os detalhes de zerar buffers não protegem nada de verdade (Menor)
* **Onde:** linhas 176–179 e 448; item 17, linha 829
* **Problema:**
  * O próprio texto admite que a cópia dentro do `HTTPClient` não é zerada.
  * Também admite que quem consegue ler a RAM já vê os tokens decifrados.
  * Com isso, os `memset` viram detalhe de código no nível de arquitetura.
* **Correção:**
  * Na §1, dizer só "tabela sob mutex, nunca passa pela fila".
  * Levar os `memset` para o item 15/17 ou removê-los.
  * A tabela com mutex em si é proporcional.

### 🟢 6. A espera crescente depois de `AUTH` é mais do que o caso pede (Menor)
* **Onde:** §2, linhas 218–220; item 16, linha 820
* **Problema:** um token revogado não volta a funcionar sozinho. Esperar e tentar de novo não resolve nada.
* **Correção:**
  * Depois de um `AUTH`, parar de consultar a conta até o token ser trocado ou o dono tocar em "Atualizar agora".
  * Remover o `backoffCycles`.

### 🟢 7. "Sem relógio por 60 s" aparece como `NOWIFI` (Menor)
* **Onde:** linhas 206 e 263; item 16, linha 817
* **Problema:** o card mostra `⚠ NOWIFI` mesmo com o WiFi conectado, o que confunde.
* **Correção:** usar a classe `NET`, que já existe.

### 🟢 8. Itens do plano na fase errada ou em contradição com as fases (Menor)
* **Item 21 (linha 864):** está sob o título "Fase 2", mas a linha 1046 o coloca na fase 4. Mover para a fase 4.
* **Item 20 (linha 856):** edita `src/ui/screen_main.cpp` na fase 2, mas o item 7 (linha 687) só tira as telas do `.ino` na fase 3. Escolher uma das duas: editar a tela dentro do `.ino` ou criar o arquivo já na fase 2.
* **Item 3 (linha 636):** está sob "Fase 0", mas não aparece na lista da linha 627. Indicar a fase.

### 🟢 9. O item 8 reorganiza pastas sem ganho para nenhum objetivo (Menor)
* **Onde:** linhas 701–706
* **Problema:** mover para `src/` os arquivos que já estão separados só mexe em includes e no build. O achado M8 da etapa 5 continua em parte aberto.
* **Correção:** deixar esses arquivos onde estão e colocar em `src/` só os módulos novos.

### 🟢 10. Sobra da variante cinza do pássaro (Menor)
* **Onde:** item 22, linha 880
* **Problema:** o item ainda pede uma "variante cinza para o incidente". Isso contradiz o item 24 e a resolução de N1, que trocou a imagem separada por `image_recolor`.
* **Correção:** apagar o trecho.

### 🟢 11. Falta `FC_ERR` no enum `FetchClass` (Menor)
* **Onde:** item 4, linha 658
* **Problema:** a §4 define a classe `FC_ERR` para "outros 4xx", mas o enum não a inclui.
* **Correção:** acrescentar `FC_ERR`.

### 🟢 12. O texto cita um CI que não existe (Menor)
* **Onde:** Risks, linha 561; Verification Plan, linha 1076
* **Problema:** o texto fala em "grep no CI", mas `.github/` só tem `PULL_REQUEST_TEMPLATE.md`.
* **Correção:** colocar o grep em `firmware/tests/run.sh` ou em `build.sh`.

### 🟢 13. `/health` e os logs de ciclo se sobrepõem (Menor)
* **Onde:** §9, linhas 468–479
* **Problema:** o tamanho da §9 é proporcional, mas o campo `cycles_over_interval` não tem definição.
* **Correção:** definir o campo. Como alternativa opcional, imprimir os mesmos campos numa linha de serial por ciclo e dispensar o endpoint.

### 🟢 14. Ponto azul enquanto espera o NTP (Menor)
* **Onde:** linha 279; teste 5, linha 1090
* **Problema:** enquanto o NTP não sincroniza, nenhum job sai. O texto não diz se o primeiro ciclo já conta como "em andamento".
* **Correção:** dizer que o primeiro ciclo conta como em andamento desde o desbloqueio até terminar.

---

## Avaliação das mudanças pedidas

| Mudança | Avaliação |
| --- | --- |
| Risks & Mitigations | Proporcional. Corrigir as linhas 561 e 568 e acrescentar o risco do rodízio (achados 2, 3 e 12) |
| §9 Observabilidade | Proporcional, porque existe só na build de desenvolvimento (achado 13) |
| Estado `pendente` e regra por ciclos | `pendente` é necessário. A regra de 2 ciclos deve sair (achado 3) |
| Tabela de tokens com mutex | Proporcional. Os detalhes de zerar buffers sobram (achado 5) |
| Token e WiFi como conteúdo do shell | Proporcional. Dizer na linha 169 que a tela de WiFi é a única exceção a "só a task mexe no rádio" |
| Bridge na fase 4 | Resolvido. Falta mover o item 21 (achado 8) |
| `PROBE` dentro do `USAGE` | Boa simplificação: economiza uma requisição por ciclo em relação à v2.3. Falta a regra de separação (achado 2) |
| Uma família de fonte | Proporcional. A Montserrat 14 continua como fallback para ícones, um custo pequeno |

**Custo que sobra das decisões do dono** (registrado, não é defeito):

- **Pulso em todas as cores:** um `lv_timer` permanente, que pode chegar a cerca de 2,5 quadros cheios por segundo.
- **3 colunas:** exigem a fonte `jbmono_num_30` e limitam o rótulo a 11 caracteres.
- **Intervalo fixo:** cerca de 480 requisições por hora com 4 contas a cada 30 s.

## Situação dos achados da etapa 5

| Achado | Status | Observação |
|---|---|---|
| M1 dado velho e azul | parcial | Falta tirar a regra de 2 ciclos e corrigir a linha 766 (achado 3) |
| M2 watchdog | resolvido | — |
| M3 esperar a task | resolvido | A `xQueueReset` criou um problema novo (achado 1) |
| M4 pulso | decisão do dono | — |
| M5 RAM interna | parcial | O pool na PSRAM não tem item no plano e o mecanismo está errado (achado 4) |
| M6 bridge no caminho crítico | resolvido | Falta mover o item 21 (achado 8) |
| M7 PROBE + USAGE | parcial | Faltam a repetição sem headers e a separação entre modelo e conta (achado 2) |
| M8 fase 0 grande | parcial | Falta resolver o item 8 e a contradição entre os itens 7 e 20 (achados 8 e 9) |
| M9 fontes | resolvido | — |
| M10 4 layouts | decisão do dono | — |
| M11 polling adaptativo | decisão do dono | — |
| N1 pássaro cinza | parcial | A linha 880 ainda pede a variante (achado 10) |
| N2, N3, N4, N6, N7, N8 | resolvido | N8 ainda tem a contradição de arquivo do achado 8 |
| N5 detalhe de código | aberto (recusado pelo autor) | A tabela de tokens trouxe mais detalhe (achado 5) |
