# Revisão de compreensão (Stage 6): THEP Usage Stick v3

Li o documento inteiro, linhas 1 a 917. Não abri nenhum outro arquivo do projeto. Não apliquei nenhuma norma de documento.

## O que isto propõe (spec, linhas 1–448)

Hoje o Claude Usage Stick é um aparelho com ESP32-S3 e tela touch de 3,5". Ele mostra quanto do limite de uso do Claude Code já foi gasto, lendo os headers de rate limit de uma requisição mínima à API. A v2.3 guarda até 4 contas, mas consulta e mostra só uma, a "ativa". Para ver outra, o usuário precisa trocar pelos Ajustes. Hoje a requisição HTTPS roda no mesmo `loop()` que desenha a tela, e por isso a interface trava durante a consulta.

A v3 consulta todas as contas a cada intervalo e mostra todas juntas na tela inicial. O layout muda conforme o número de slots salvos (1, 2, 3 ou 4). Cada conta aparece com:
- a % da janela de 5h;
- uma barra de texto;
- os resets da 5h e da semana;
- a % da semana;
- uma projeção curta de quando a cota acaba.

Um ponto colorido no cabeçalho resume a saúde das consultas, com prioridade vermelho > amarelo > azul > verde. Ele substitui o texto "atualizado há". Um menu de comandos (`/agora`, `/modelos` etc.) substitui o swipe. O visual passa a imitar a estética de terminal do "Hermes Agent", com as cores da THEP: logo em ASCII art, pássaro em braille, fonte JetBrains Mono, painel com título na borda e barra de status.

Custos aceitos: mais requisições, visíveis aos admins da organização, e até 4 tokens decifrados na RAM. A volta para a v2.3 não pode perder dados.

## Plano de implementação (linhas 449–917)

O plano tem cinco fases. Cada uma compila e pode ser gravada sozinha.

- **Fase 0:** cria a tag `v2.3`, anota tamanho e heap como linha de base e divide o `.ino` de cerca de 2.900 linhas em módulos dentro de `src/`, sem mudar o comportamento.
- **Fase 1:** extrai funções puras (`projection`, `dot_state`, níveis, barra, `fit_label`) com testes em g++ no computador. Um array `AccountState` em PSRAM substitui as variáveis globais.
- **Fase 2:** cria uma `net_task` no core 0, a única que faz HTTPS. Ela conversa com o `loop()` por duas filas (`jobQ`/`resQ`). Um contador `gen` por slot descarta resultados atrasados. O plano ainda traz um agendador de ciclos sem sobreposição, espera crescente para erros 401/403, desbloqueio que decifra todos os slots de uma vez, tela inicial de 2 colunas no visual antigo e o contrato novo do bridge (`?account=`, `--config-dir`).
- **Fase 3:** tema, fontes geradas com `lv_font_conv`, imagens geradas por script, shell fixo com menu modal, ponto pulsante, os 4 layouts, `/modelos` em texto, repintura das outras telas e remoção dos mascotes Clawd.
- **Fase 4:** READMEs e organização dos documentos.

A verificação inclui build com teto de 2,8 MB, testes unitários, um grep contra `lv_` dentro de `src/net/` e 8 testes manuais no aparelho, entre eles 1 hora de execução contínua e o ensaio de rollback.

## Achados

### 🟡 1. O ponto fica vermelho no primeiro ciclo e em contas novas, em vez de azul ou "consultando…"
- **Onde:** §4 (linhas 218–229), CUJ 4 (linhas 107–111), item 11 (linha 574).
- **Problema:** uma conta conta como `falhou` quando `agora − usageAtEpoch > 2 × intervalo`. Logo depois do desbloqueio, ou numa conta recém-adicionada, ainda não existe `usageAtEpoch`, e a regra trata a conta como falha. Como vermelho e amarelo vêm antes do azul, a linha "primeiro ciclo ainda em andamento → azul" nunca chega a valer. A conta nova da CUJ 4 ("consultando…") também deixaria o ponto amarelo. Quem lê não sabe qual comportamento é o esperado.
- **Correção:** dizer na regra de `falhou` que uma conta sem nenhum dado e sem nenhuma tentativa concluída fica "pendente" e não conta como falha. Incluir esse caso na tabela e no `test_dot_state`.

### 🟡 2. O azul é medido de jeitos diferentes em três lugares
- **Onde:** G4 (linha 55), §4 (linhas 220 e 231), item 11 (linha 574).
- **Problema:**
  - O G4 diz "todas atualizadas há menos de 10 s".
  - O §4 define `fresca` pelo fim da tentativa de cada conta.
  - Logo depois, o §4 diz que o azul conta a partir do **fim do ciclo**, para todas acenderem juntas.
  - Já o `AccountView` só recebe `lastTryEndEpoch` por conta, sem nenhum horário de fim de ciclo.

  Quem implementa não sabe qual regra seguir.
- **Correção:** escolher uma das regras. Se for "fim do ciclo", passar `cycleEndEpoch` para `dot_state` e reescrever a definição de `fresca`.

### 🟡 3. Duas regras para quando falta WiFi
- **Onde:** §2, passo 1 (linha 166), contra o item 16 (linha 644).
- **Problema:** o §2 diz que "nenhum job é enviado" quando não há WiFi. O item 16 diz que o agendador enfileira `RECONNECT`. O §1 também diz que só a task chama `WiFi.reconnect()`, ou seja, algum job precisa ser enviado.
- **Correção:** no §2, trocar por "nenhum job de consulta é enviado; só um `RECONNECT`".

### 🟡 4. A palavra "token" tem dois sentidos, e o bridge nunca é explicado
- **Onde:** Open Questions (linha 83), §3 (`tok`, `TokenStats`), §8 (linhas 324–335), item 31 ("linha de tokens"), item 21.
- **Problema:** "token" aparece como credencial OAuth (`sk-ant-oat01`, `token[200]`) e também como contagem de consumo ("linha de tokens", `POST /tokens`, `TokenStats`). O "token bridge" aparece na linha 83 sem nenhuma definição. Pelo §8 dá para deduzir que é um script no PC que lê arquivos `.jsonl` do Claude Code e envia algo ao aparelho. Mas o documento não diz o que é enviado, nem o que a "linha de tokens" mostra na tela.
- **Correção:** na Overview, uma frase sobre o bridge, mais ou menos assim: "script Python no PC que soma os tokens consumidos nas sessões do Claude Code e envia ao aparelho via `POST /tokens`". Sugiro também chamar a credencial de "token OAuth" e a contagem de "contagem de tokens" ao longo do texto.

### 🟡 5. A paleta de cores depende de um arquivo fora do documento
- **Onde:** §6 (linha 280), item 28 (linha 747), G6.
- **Problema:** "tabela da paleta no brief" e os papéis `C_BG`, `C_SURFACE`, `C_TITLE` etc. não têm valores no documento. Só aparecem alguns hexadecimais soltos (ponto, faixas do logo, `#8a98a8`). O G6 exige "somente cores do design system THEP", mas o leitor não tem como verificar isso, nem como montar o `theme`.
- **Correção:** colar no documento a tabela papel → hexadecimal, ou pelo menos os 16 papéis do item 28.

### 🟡 6. "Hermes Agent" e "THEP" não são apresentados
- **Onde:** título, Overview (linhas 16–20), G6.
- **Problema:** o visual inteiro é definido como "estilo Hermes", mas o documento só explica o que é o Hermes Agent num link no fim (linha 439): uma CLI de agente com banner em ANSI. "THEP" também não é definido (empresa? marca do dono?). Sem os mockups, o leitor não consegue imaginar a interface.
- **Correção:** uma frase na Overview para cada um, por exemplo: "Hermes Agent: agente de terminal da Nous Research, cuja CLI tem banner em ANSI Shadow, mascote em braille e painéis com título na borda" e "THEP: empresa/marca do dono".

### 🟡 7. As fases da tabela §9 não batem com as fases do plano detalhado
- **Onde:** §9 (linhas 346–347) contra os itens 31–36 e 40–42.
- **Problema:** o §9 coloca os layouts de 1, 3 e 4 contas, a repintura das outras telas e os momentos por conta na Fase 4. O plano detalhado coloca tudo isso na Fase 3 e deixa a Fase 4 só com documentação. O teste de longa duração e o ensaio de rollback também não aparecem como itens da Fase 4. Com isso, o mapeamento dos Goals para as fases (G2, G7, G8) fica sem confirmação.
- **Correção:** alinhar a tabela do §9 com os itens, ou mover os itens 31–36 para uma seção "Fase 4".

### 🟡 8. "Bloqueado" quer dizer três coisas diferentes
- **Onde:** §7 (linha 319, `BLOQUEADA`), Open Questions (linha 85, marca `bloq`), §7 e NFR ("bloqueio por PIN errado"), G1 ("desbloqueada").
- **Problema:** o mesmo radical cobre:
  - o slot que não decifrou;
  - o estado `unified-status` da API;
  - o lockout por PIN errado.

  A conta com falha de decifração também não aparece na tabela de classes do §4, e não fica claro como ela pesa no ponto. O texto "bloqueio por PIN errado" sugere que o aparelho volta a ser bloqueado no meio da sessão, mas o documento não explica quando isso acontece.
- **Correção:** usar nomes diferentes (por exemplo, `ILEGÍVEL` para o slot e `bloq` só para `unified-status`). Dizer como o slot ilegível entra em `dot_state` e o que dispara o rebloqueio.

### 🟡 9. A marca `ok` / `atenção` / `bloq` não tem regra de conversão
- **Onde:** linha 85 e item 31 (linha 780).
- **Problema:** o documento não diz quais valores do header `unified-status` viram `ok`, `atenção` e `bloq`, nem qual cor cada um recebe. No layout de 4 contas, o item 32 diz "— (cor da %)", o que também não é explicado.
- **Correção:** uma tabela com valor do header → texto → cor.

### 🟡 10. A sonda (PROBE) não é explicada nem entra no custo
- **Onde:** Non-Goals (linha 77, "sonda"), §2, passo 4, item 33.
- **Problema:** dá para deduzir que o PROBE é uma requisição de teste por modelo para ver se ele está disponível. Mas o documento não diz isso, nem conta essas requisições (e as de STATUS) no custo de "480 requisições por hora". O pior caso de "~40 s" (4 × 8 s) também deixa de fora os jobs de STATUS e PROBE, que podem levar o total para cerca de 48 s.
- **Correção:** definir PROBE em uma linha e recalcular o custo por hora e o pior caso com todos os tipos de job.

### 🟡 11. A tabela de layout pede uma fonte em negrito que não é gerada
- **Onde:** item 32 (linha 801, "jbmono_13 bold"), contra o §6 e o item 26.
- **Problema:** o plano só gera JetBrains Mono Regular. Não existe arquivo bold.
- **Correção:** acrescentar `jbmono_13b` aos itens 25 e 26 e ao orçamento de flash, ou trocar por outra ênfase (cor, por exemplo).

### 🟡 12. O conteúdo de `/health` e de `/token/status` não é definido
- **Onde:** item 6 (linhas 514–515), NFR (linha 418), Verification Plan, teste 2.
- **Problema:** o item 6 aponta para o Verification Plan, mas lá só se deduz que `/health` mostra heap, stack e duração de ciclo. O formato da resposta não aparece. `GET /token/status` também não tem resposta descrita.
- **Correção:** listar os campos JSON dos dois endpoints.

### 🟢 13. Duas assinaturas diferentes para `projection`
- **Onde:** §3 (linha 200), contra o item 10 (linha 558).
- **Problema:** o §3 usa `projection(const AccountState&, now)`. O item 10 usa `projection(const Sample*, int, float, uint32_t, uint32_t)`. O tipo `Sample` não é definido em lugar nenhum.
- **Correção:** dizer qual assinatura vale (provavelmente a do item 10, que não depende do modelo) e o que `Sample` contém.

### 🟢 14. A espera crescente não bate com a fórmula
- **Onde:** linha 174 ("1, 2, 4… ciclos") contra a linha 646 (`min(2^fails, …)`).
- **Problema:** com `fails = 1`, a fórmula dá 2, não 1.
- **Correção:** usar `2^(fails−1)` ou corrigir o texto.

### 🟢 15. O ponto verde é "fixo" e "respira" ao mesmo tempo
- **Onde:** tabela do §4 (linha 229) e linha 276.
- **Problema:** "Fixo, com respiração lenta" se contradiz, e não fica claro se a respiração usa o mesmo timer de 8 Hz.
- **Correção:** descrever a animação do verde (período e degraus).

### 🟢 16. Tela inicial da Fase 2 com 1, 3 ou 4 contas
- **Onde:** item 20.
- **Problema:** o item só descreve 2 colunas. Não diz o que aparece na Fase 2 quando existem 1, 3 ou 4 slots.
- **Correção:** uma frase, por exemplo "N colunas iguais" ou "só as duas primeiras".

### 🟢 17. "Momento de limiar" não é definido
- **Onde:** Problem (linha 30), §5 (linha 274), item 35.
- **Problema:** o documento não diz que é um overlay animado nem quais limiares o disparam. Só aparecem 70% e 100% no item 35.
- **Correção:** definir o termo na primeira vez que aparece e listar os limiares, para 5h e para semana.

### 🟢 18. Jargão do processo e referências soltas
- **Onde:** linhas 435 e 883 ("revisores Goldfish", "Step 6", `design_draft.md`), linha 872 (itens do drift report sem explicação), linha 770 (`TRS(pt,en)`, que indica uma UI bilíngue nunca mencionada), linha 829 ("regras de dispensa e escalada da v2.3").
- **Correção:** retirar o jargão do EGM ou explicar em uma linha. Dizer que a interface tem dois idiomas e resumir as regras herdadas da v2.3 ou apontar onde estão.

### 🟢 19. Clawd aparece antes de ser apresentado
- **Onde:** linha 84.
- **Problema:** "Clawd: os mascotes saem" aparece antes da explicação da linha 310 ("pixel art do Clawd").
- **Correção:** apresentar o Clawd na primeira menção, como "mascote em pixel art da v2.3".

## Resultado

Consegui parafrasear o documento inteiro, então não há nenhum 🔴. As partes se encaixam: rede → modelo → interface, com fases incrementais. Os problemas principais estão na lógica do ponto de status (achados 1 e 2) e em contradições entre o Technical Plan e o plano detalhado (achados 3, 7 e 11). Também falta informação: paleta, Hermes Agent e THEP, bridge e tokens, marca de estado, endpoints. Hoje o leitor depende do brief, dos mockups ou de suposições para preencher essas lacunas.

Arquivo revisado: `docs/designs/thep-usage-stick-v3.md`
