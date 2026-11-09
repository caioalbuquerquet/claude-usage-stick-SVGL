# Relatório de Revisão Goldfish — Stage 7 (Critic Review), rodada 1

**Alvo:** `/Users/caiotorres/Development/claude-usage-stick/docs/designs/thep-usage-stick-v3.md` (linhas 1–448, do título até o fim de "Non-Functional Requirements", antes de "## Detailed Implementation Plan")
**Modo:** REVIEW
**Padrão:** `feature-spec-standard.md` (design e structural); só os padrões da persona (style)

## Status dos revisores

| Revisor | Status | Achados brutos |
| --- | --- | --- |
| `egm-design` | completed | 0 🔴 · 11 🟡 · 7 🟢 |
| `egm-structural` | completed | 0 🔴 · 6 🟡 · 8 🟢 |
| `egm-style` | completed | 0 🔴 · 13 🟡 · 14 🟢 |

Os revisores fizeram 59 achados. Depois de juntar os repetidos, ficaram **52 itens: 0 🔴 Blocker, 26 🟡 Major e 26 🟢 Nit**. Quando dois ou mais achados foram juntados num item, ele lista todos os ids.

Nenhum revisor encontrou Blocker. O documento segue a estrutura do template, e os revisores conferiram no código várias afirmações:
- `lv_conf.h:43` (`LV_OS_NONE`) e `LV_MEM_SIZE` de 96 KB;
- `STATUS_POLL_SEC` definida e não usada;
- `API_TIMEOUT_MS` = 15 s;
- `trend_redraw` em `.ino:1542`;
- `RENDER_MODE_FULL`;
- a derivação de chave em `crypto.cpp`;
- o tamanho de ~5 KB por conta;
- o 409 em `handleTokensPost`.

Os problemas se concentram em três pontos:
1. A máquina de estados do ponto de status é contraditória.
2. Faltam casos de borda na arquitetura de rede: NTP, WiFi, watchdog e shell versus telas cheias.
3. Faltam uma seção de riscos e justificativas técnicas para as alternativas rejeitadas.

---

## Achados priorizados (Blockers → Major → Nits)

### 🟡 Major

#### 1. O ponto não consegue ficar azul depois do desbloqueio e dá alarme falso ao adicionar conta
* **IDs:** `design-1-1`
* **Revisor:** `egm-design`
* **Linhas:** 52–56, 93, 111, 218–228
* **Discrepância:** a linha 219 marca uma conta como `falhou` quando `agora − usageAtEpoch > 2 × intervalo`. Como `usageAtEpoch` não é salvo (não está em `HistFileV2`), ele vale 0 logo após o desbloqueio.
  - No primeiro ciclo, todas as contas ficam `falhou` e o vermelho, que tem prioridade 1, vence. A linha azul "primeiro ciclo após o desbloqueio" (228) nunca é alcançada.
  - No CUJ 4, a conta nova ainda não tem dado, então o ponto fica amarelo enquanto o card mostra `consultando…`.
  - Antes do NTP, o epoch fica perto de 0 e todas as contas parecem velhas.
* **Justificativa e correção:**
  - Criar um terceiro estado, `pendente` (nenhuma resposta desde o desbloqueio ou desde a última mudança de `gen`), que não conta como `falhou`.
  - Aplicar a regra de idade só quando `usageAtEpoch != 0` e o relógio for válido.
  - Pôr "primeiro ciclo em andamento / conta `pendente` sem falhas" acima do amarelo, ou declarar a ordem que se quer.
  - Incluir esses casos nos testes de computador da fase 1.

#### 2. A regra do azul (`fresca`) tem três definições diferentes
* **IDs:** `style-1-2`, `design-1-12`
* **Revisores:** `egm-style`, `egm-design`
* **Linhas:** 55, 220, 228, 231
* **Discrepância:** cada trecho usa uma regra:
  - G4 (55): "todas atualizadas há menos de 10 s";
  - linha 220: `fresca` conta a partir do fim da tentativa de cada conta;
  - linha 231: o azul conta a partir do fim do ciclo;
  - linha 228: o azul também acende durante o primeiro ciclo, o que G4 não cita.

  Quem implementar não sabe qual relógio usar.
* **Justificativa e correção:** adotar o fim do ciclo, que é a intenção declarada.
  - Linha 220: "`fresca` se a última tentativa foi OK e o ciclo que a incluiu terminou há menos de 10 s."
  - G4: "azul: primeiro ciclo após o desbloqueio em andamento, ou ciclo terminado há menos de 10 s com todas as contas OK."

#### 3. A definição de "falhou" em G4 é mais estreita que a regra técnica
* **IDs:** `style-1-3`
* **Revisor:** `egm-style`
* **Linhas:** 53–54, 219
* **Discrepância:** G4 diz "falharam na última consulta". A linha 219 também conta como falha um dado com mais de 2 × intervalo de idade. Uma conta em espera por AUTH não é consultada no ciclo, então pela letra de G4 ela não "falhou".
* **Justificativa e correção:** reescrever G4 assim: "vermelho: todas as contas em falha (última tentativa falhou ou dados com mais de 2 × intervalo); amarelo: ao menos uma conta em falha."

#### 4. O ponto verde é "pulsante", "fixo" e "com respiração" ao mesmo tempo
* **IDs:** `style-1-1`, `design-1-7` (parte sobre o verde)
* **Revisores:** `egm-style`, `egm-design`; o `egm-structural` também apontou isso fora da sua área
* **Linhas:** 22, 229, 276
* **Discrepância:** a linha 22 fala em "ponto pulsante", e a tabela (229) diz "Fixo, com respiração lenta". "Respiração" não é definida em lugar nenhum, e se for animação contínua obriga a redesenhar a tela o tempo todo (ver item 5).
* **Justificativa e correção:** deixar o verde parado de fato ("Fixo, sem animação"). Na linha 22, escrever "Um ponto de status colorido substitui…". Se a respiração for mantida, defini-la como os mesmos 4 degraus de opacidade num ciclo de N s.

#### 5. O plano B `PARTIAL` para o pulso não reduz o custo como está escrito
* **IDs:** `design-1-7`
* **Revisor:** `egm-design`
* **Linhas:** 276, 421
* **Discrepância:** `disp_flush_cb` gira o quadro inteiro de 480×320 pixel a pixel e chama `gfx->flush()`, que envia o `Arduino_Canvas` inteiro (`.ino:215-224`).
  - Trocar para `PARTIAL` sem reescrever o flush não ganha nada.
  - Gravar só uma janela parcial no AXS15231B por QSPI nunca foi provado neste projeto.
  - A PSRAM já guarda ~600 KB entre o buffer do LVGL e o canvas.
* **Justificativa e correção:**
  - Descrever o plano B como "PARTIAL + flush só da área".
  - Incluir na fase 3 um spike que prove a gravação parcial nesse painel.
  - Ter um segundo plano B mais barato: mudar a opacidade 1 vez por segundo, ou pulsar só no azul, amarelo e vermelho.

#### 6. O shell fixo conflita com as telas cheias de token e WiFi
* **IDs:** `design-1-2`
* **Revisor:** `egm-design`
* **Linhas:** 110–111, 239, 241, 246
* **Discrepância:** o shell é "criado uma vez", mas adicionar conta (CUJ 4) passa pela tela de token, que continua sendo um estado de tela cheia. Hoje cada troca de estado roda `lv_obj_clean(...)` e `stop_web()` (`.ino:2620-2635`). Entrar na tela de token destruiria o shell, derrubaria o servidor do bridge e pausaria a task (246). O mesmo vale para o WiFi aberto a partir de `/ajustes`.
* **Justificativa e correção:** escolher uma das saídas e escrevê-la no documento:
  - (a) os fluxos de token e WiFi viram conteúdo do painel ou um modal no `lv_layer_top`, e o servidor e as consultas continuam rodando; ou
  - (b) sair do shell o destrói e depois o recria, e as consultas e o bridge param nesse intervalo.

  Ajustar o CUJ 4 conforme a escolha.

#### 7. NTP, primeira carga e erro de boot não estão definidos no fluxo de várias contas
* **IDs:** `design-1-3`
* **Revisor:** `egm-design`
* **Linhas:** 166, 213, 246
* **Discrepância:** antes do NTP sincronizar, o TLS falha com `http_-1` (`.ino:2667-2681`; foi o que o commit e80448f corrigiu). O agendador só confere o WiFi, então o primeiro ciclo após o boot manda todas as contas para `NET`, e o ponto fica vermelho. O documento também mantém o "erro de boot" como tela cheia sem dizer quando a v3 entra nele. Na v2.3 ele aparece quando a primeira consulta falha (`.ino:2702`), o que conflita com G2.
* **Justificativa e correção:**
  - Adicionar ao passo 1 do agendador a condição "relógio não sincronizado": nenhum job é enviado e o estado `SEM_RELOGIO` é tratado como `NOWIFI`.
  - Dizer se `ST_LOADING` e `ST_ERROR` continuam existindo. A recomendação é que não continuem depois do desbloqueio, e que o shell abra direto com os cards em `consultando…`.

#### 8. A reconexão do WiFi não tem quem a dispare e pode travar
* **IDs:** `design-1-4`
* **Revisor:** `egm-design`
* **Linhas:** 137, 166
* **Discrepância:** só a `net_task` chama `WiFi.reconnect()`, mas ela só age quando recebe jobs, e o loop não envia jobs sem WiFi. Ninguém dispara a reconexão.
  - A v2.3 usa `g_wifi.autoConnect()`, que percorre todas as redes salvas (`wifi_manager.h:28-43`). `reconnect()` só tenta o último SSID, então o comportamento muda sem aviso.
  - `g_wifi` também é usado pela tela de WiFi, que roda no loop, e as duas threads disputariam o objeto.
* **Justificativa e correção:**
  - Criar um job `RECONNECT`, ou um timer próprio na task que tente reconectar quando o link cair.
  - Dizer se o fallback entre várias redes continua (se continuar, passar à task uma cópia de `_nets`).
  - Definir como a tela de WiFi ganha acesso exclusivo, por exemplo esvaziando a `jobQ` e esperando a task ficar ociosa.

#### 9. O watchdog que recria a task não é seguro e dispara à toa
* **IDs:** `design-1-5`
* **Revisor:** `egm-design`
* **Linha:** 235
* **Discrepância:**
  - Sem WiFi, sem relógio ou com todas as contas em espera, nenhum job é enviado e a task não publica nada. O watchdog então a recria sem motivo.
  - Um `vTaskDelete` no meio de um handshake TLS vaza o contexto mbedTLS (~40–50 KB de RAM interna), o socket e o heap do `HTTPClient`. Poucas recriações esgotam a RAM interna, e o LVGL passa a falhar sem aviso.
* **Justificativa e correção:**
  - Medir "job em andamento há mais de N × timeout", e não "nada publicado".
  - Recuperar com `esp_restart()`, já que o estado está salvo, em vez de recriar a task.
  - Registrar o motivo na NVS.

#### 10. Os tempos de ciclo e a idade dos dados estão subestimados, e o timeout de 8 s não limita o handshake
* **IDs:** `design-1-6`, `style-1-11`
* **Revisores:** `egm-design`, `egm-style`
* **Linhas:** 93, 173, 379
* **Discrepância:**
  - Um ciclo tem 4 `USAGE` + `STATUS` + `PROBE`, ou seja, até 6 × 8 s = 48 s, e não "~40 s".
  - `setTimeout()` (`api.cpp:42`) só cobre a leitura. O handshake do `WiFiClientSecure` tem timeout próprio (o padrão do arduino-esp32 é 120 s), e a conexão tem outro.
  - O CUJ 1 (93) diz que os dados têm "no máximo um intervalo" de idade. Como o próximo ciclo começa em fim + intervalo (172), a idade real chega a intervalo + duração do ciclo.
  - A rejeição da Alternativa E e o watchdog dependem desses números.
* **Justificativa e correção:**
  - Especificar `setConnectTimeout`, `setHandshakeTimeout` e `setTimeout`, com valores, e conferi-los com a versão do core usada no build.
  - Refazer a conta do pior caso: "6 jobs × 8 s = 48 s; 32 s só com os `USAGE`".
  - Corrigir a linha 379 e o CUJ 1: "no máximo um intervalo mais a duração de um ciclo".

#### 11. `/health` aparece nos NFRs mas não existe nem está definido
* **IDs:** `design-1-8`
* **Revisor:** `egm-design`
* **Linhas:** 417–418, 420–422
* **Discrepância:** o firmware só tem as rotas `/`, `/window` e `/tokens` (`.ino:830-833`), e o Technical Plan não cria `/health`. O item I20 do brief propunha esse endpoint, com estado por slot, heap interno, `lv_mem_monitor`, stack high-water, duração do ciclo e uptime, e ele saiu sem justificativa. Sem ele, não há como conferir os NFRs de toque, quadro e RAM no aparelho em uso.
* **Justificativa e correção:** adicionar ao Technical Plan uma subseção "Observabilidade", com o contrato de `/health` (sem tokens) e os logs por ciclo. A outra opção é remover a linha 418 e dizer como as linhas 420–422 serão medidas.

#### 12. A fonte não bate com os mockups, e o número de flash não diz o que inclui
* **IDs:** `design-1-9`, `style-1-12`
* **Revisores:** `egm-design`, `egm-style`
* **Linhas:** 59, 296, 300–305, 310
* **Discrepância:**
  - `mockup-home.dc.html` usa `font-weight: 700/800` nos rótulos, títulos e porcentagens. O documento especifica só a JetBrains Mono Regular.
  - Os "~66 KB medidos" não dizem qual build foi medido e deixam de fora os tamanhos de 30 e 20 px (303).
  - A linha 310 diz que remover o Clawd "compensa" os 66 KB, mas não dá o tamanho de `logo_assets.h` nem de `clawd_dead.h`.
* **Justificativa e correção:**
  - Incluir a Bold (ou ExtraBold) nos tamanhos de destaque e medir o flash de novo, ou registrar o desvio do mockup como decisão.
  - Informar o total com 20 e 30 px e qual build foi medido.
  - Escrever "Remover `logo_assets.h` e `clawd_dead.h` libera X KB".

#### 13. A zeragem dos tokens promete mais do que o novo desenho cumpre
* **IDs:** `design-1-11`
* **Revisor:** `egm-design`
* **Linhas:** 143, 320, 416
* **Discrepância:** o desenho novo cria cópias do token em claro que o `memset` de `acct[i].token` não alcança:
  - no armazenamento da `jobQ`, porque o FreeRTOS não limpa o slot da fila depois de ler;
  - na stack da `net_task`;
  - no `String("Bearer ") + token` (`api.cpp:37,100`).

  O gatilho "bloqueio por PIN errado" não faz nada, porque o bloqueio acontece antes de qualquer token ser decifrado (`.ino:470-483`). O documento também não diz se a chave derivada ou `g_sessionPin` ficam em RAM para cifrar contas novas.
* **Justificativa e correção:**
  - Zerar o buffer do job na task depois de usar, ou passar só `{slot, gen}` na fila e ler o token protegido por mutex.
  - Montar o header `Authorization` num buffer de stack que é zerado depois.
  - Trocar o gatilho "PIN errado" pelos eventos reais.
  - Dizer se a chave ou o PIN ficam na RAM, citando o modelo de ameaça atual (`.ino:105-107`).

#### 14. Algumas rejeições de alternativas dependem só da escolha do dono, sem link nem trade-off técnico
* **IDs:** `structural-1-2`, `design-1-10`
* **Revisores:** `egm-structural`, `egm-design`
* **Linhas:** 355, 361, 367, 391, 403, 409
* **Discrepância:** a Alternatives Rule exige trade-off técnico ou de escopo concreto e proíbe atribuir a rejeição ao mantenedor sem um link.
  - A (355) e G (391) se apoiam só em "o dono escolheu/definiu".
  - J (409) diz "o dono pediu vermelho".
  - I (403) repete a decisão sem dar o motivo.
  - B e C já passam.
* **Justificativa e correção:**
  - Linkar o registro da decisão, por exemplo o canvas dos mockups ou uma nota de "decisões do draft".
  - Começar cada Reason pelo trade-off:
    - **I:** "o bloco braille custa 256 glifos × N tamanhos de flash, mais que 3 bitmaps, e o alinhamento depende da métrica da fonte."
    - **J:** "rosa sobre azul-marinho não se lê como alerta; o vermelho semântico vale a exceção declarada."
    - **G:** "sem o azul, não dá para distinguir 'recém-atualizado' de 'ok'; o custo de render fica limitado pelos degraus de 8 Hz."
    - **A:** "a fonte proporcional perde o alinhamento em colunas das barras de blocos."

#### 15. Não há uma seção de Riscos e Mitigações
* **IDs:** `structural-1-1`
* **Revisor:** `egm-structural`
* **Linhas:** 36–39, 119–347, 413–429
* **Discrepância:** o padrão exige que os riscos e suas mitigações sejam declarados de forma explícita. Hoje eles estão espalhados:
  - custos na linha 36;
  - watchdog na linha 235;
  - plano B `PARTIAL` na linha 276;
  - tokens em RAM na linha 321;
  - Alternativa D como contingência na linha 373.

  Alguns riscos não têm mitigação nenhuma, como a fragmentação da RAM interna durante o TLS.
* **Justificativa e correção:** criar `## Risks & Mitigations` (ou uma H3 dentro dos NFRs) com a tabela Risco | Gatilho | Impacto | Mitigação | Onde no plano. Ela deve cobrir no mínimo:
  - instabilidade da `net_task`, com a Alternativa D como saída;
  - fragmentação da RAM interna no TLS;
  - quadro acima de 40 ms;
  - flash acima de 2,8 MB;
  - cota e visibilidade para os admins;
  - 4 tokens em RAM;
  - resultado com `gen` velho;
  - task travada.

  Trocar o bloco "O que a mudança custa" por um link para essa seção.

#### 16. Quem lê pela primeira vez não consegue abrir os mockups e as referências visuais
* **IDs:** `structural-1-3`
* **Revisor:** `egm-structural`
* **Linhas:** 24, 50, 263, 437–439
* **Discrepância:** G2 e a tabela de layouts dependem dos mockups, e eles só existem como artifacts do claude.ai que pedem login. O design system THEP está na mesma situação. Já existem cópias locais em `docs/designs/review/context/`, mas o documento não aponta para elas.
* **Justificativa e correção:** em Additional Resources, adicionar links relativos para `mockup-home.dc.html`, `mockup-canvas.json`, `thep-design-system-README.md`, `thep-design-system-tokens.json`, `hermes-banner.py.txt` e `hermes-skin_engine.py.txt`. Em G2 e na linha 263, apontar para `./review/context/mockup-home.dc.html`.

#### 17. A paleta exigida pelo G6 existe só num arquivo de notas externo
* **IDs:** `structural-1-4`
* **Revisor:** `egm-structural`
* **Linhas:** 65, 280
* **Discrepância:** "tabela da paleta no brief" manda o leitor para `concepts_brief.md` (linhas 38–53). Como o G6 transforma "só cores THEP" em critério de sucesso, a tabela faz parte do requisito.
* **Justificativa e correção:** copiar para a §6 a tabela Papel | Token THEP | Valor, que passa a ser a fonte única também das exceções `#f05252` e `#f0c419`. No mínimo, linkar a âncora do brief.

#### 18. Duas frases aparecem dentro do item de lista errado no Markdown renderizado
* **IDs:** `structural-1-5`
* **Revisor:** `egm-structural`
* **Linhas:** 49, 57
* **Discrepância:** as frases "Nenhuma ação é necessária…" (49) e "O card de uma conta com falha…" (57) têm 4 espaços de recuo depois de uma lista aninhada. Pelo CommonMark, elas viram continuação do último subitem. A linha 57 aparece dentro de "verde: ok", o que inverte o sentido.
* **Justificativa e correção:** pôr uma linha em branco antes de cada frase, mantendo os 4 espaços, ou transformar cada uma num subitem próprio.

#### 19. A marca de estado `unified-status` (`ok`/`atenção`/`bloq`) foi decidida mas não aparece no plano, e `bloq` colide com `BLOQUEADA`
* **IDs:** `structural-1-6`, `design-1-17`, `style-1-6`
* **Revisores:** `egm-structural`, `egm-design`, `egm-style`
* **Linhas:** 43–49, 79–85, 265–270, 319
* **Discrepância:**
  - A decisão da linha 85 só existe em Open Questions. Ela não aparece no G1, na tabela de layouts nem na §4, e só volta no plano de implementação, na linha 780. Sem a marca, uma conta bloqueada parece igual a uma em 95%.
  - `bloq` (conta no limite, segundo a API) e `BLOQUEADA` (slot que não decifra) são estados sem relação, quase com o mesmo nome, no mesmo card.
  - Open Questions também guarda decisões já fechadas, que deveriam estar no corpo do documento.
* **Justificativa e correção:**
  - Reduzir Open Questions a "Nenhuma."
  - Levar a marca para o G1 e para cada linha da tabela de layouts (ou criar um parágrafo "Marca de estado" na §5).
  - Renomear um dos dois estados, por exemplo `SEM CHAVE` na linha 319 ou `limite` na linha 85.

#### 20. Na linha 200, "conta" tem o sentido de cálculo
* **IDs:** `style-1-4`
* **Revisor:** `egm-style`
* **Linha:** 200
* **Discrepância:** em todo o resto do documento, "conta" é conta do Claude. "A conta da taxa de consumo" se lê errado na primeira vez.
* **Justificativa e correção:** "O cálculo da taxa de consumo sai de `trend_redraw()`…".

#### 21. O recurso "momento de limiar" tem quatro nomes e nenhuma definição
* **IDs:** `style-1-5`
* **Revisor:** `egm-style`
* **Linhas:** 30, 252, 274, 312, 347
* **Discrepância:** o texto usa "animações de limiar", "momento de limiar", "Momentos de limiar" e "Momentos por conta". Quem não conhece a v2.3 não sabe o que é.
* **Justificativa e correção:** definir o termo na linha 30 ("overlay de tela cheia quando a 5h ou a semana cruza 50/80/100%, chamado aqui de *momento de limiar*") e usar só esse nome depois. Na linha 347, escrever "Momentos de limiar por conta".

#### 22. A regra do que conta para o layout está escrita de formas diferentes
* **IDs:** `style-1-7`
* **Revisor:** `egm-style`
* **Linhas:** 43, 50, 167, 319
* **Discrepância:** G1 fala em "conta salva e desbloqueada", e G2 fala em "slots salvos". "Desbloqueada" pode ser a sessão por PIN ou o slot decifrado. Pela linha 319, um slot que não decifra continua na tela.
* **Justificativa e correção:** na linha 43, escrever "cada slot salvo (1 a 4), depois do desbloqueio por PIN". Na linha 167, usar "slot decifrado" se o sentido for esse.

#### 23. A informação de erro tem três nomes
* **IDs:** `style-1-8`
* **Revisor:** `egm-style`
* **Linhas:** 57, 99, 233
* **Discrepância:** o texto usa "tipo de erro" (G4), "etiqueta `AUTH`" (CUJ 2) e "classe" (206 e 233).
* **Justificativa e correção:** usar "classe do erro" sempre. Na linha 57: "mostra a classe do erro e a idade dos dados (`⚠ AUTH · 4m`)".

#### 24. A lista do menu não bate com as telas existentes
* **IDs:** `style-1-9`
* **Revisor:** `egm-style`
* **Linhas:** 58, 245, 253
* **Discrepância:** G5 lista 6 itens. A linha 245 lista 7 conteúdos, incluindo `about`, que não tem item de menu, e mistura nomes internos em inglês (`home`, `trend`, `heat`) com os rótulos em português. A linha 253 acrescenta "Atualizar agora", que G5 não cita. Por isso não dá para conferir a frase "todas as telas da v2.3 continuam acessíveis".
* **Justificativa e correção:** no G5, listar `Atualizar agora` e as 6 telas, e dizer onde fica a tela Sobre (por exemplo, dentro de `/ajustes`). Na linha 245, pôr o rótulo do menu ao lado de cada nome interno, como em "`trend` (`/janela`)".

#### 25. A frase do PROBE contradiz a si mesma
* **IDs:** `style-1-10`
* **Revisor:** `egm-style`
* **Linhas:** 169, 175
* **Discrepância:** a linha 169 diz "só enquanto `/modelos` estiver aberta. Fora dela, um a cada 5 ciclos". A linha 175 diz "é absorvido por ele", que é jargão.
* **Justificativa e correção:**
  - Linha 169: "um modelo por vez, em rodízio: a cada ciclo com `/modelos` aberta e a cada 5 ciclos fora dela".
  - Linha 175: "durante um ciclo não tem efeito, porque o ciclo em andamento já atende ao pedido".

#### 26. O custo de cota não é medido e a cor amarela tem dois nomes
* **IDs:** `style-1-13`
* **Revisor:** `egm-style`
* **Linhas:** 38, 65, 283
* **Discrepância:** "fração mínima da cota" não diz quanto, e "só de usage" é jargão. A linha 283 chama de "âmbar" a mesma cor que o G6 chama de "amarelo `#f0c419`".
* **Justificativa e correção:**
  - Linha 38: dizer o custo real ("`max_tokens: 1`, ~N tokens de entrada") e escrever "só para medir o uso".
  - Linha 283: "amarelo (`#f0c419`) de 50% a menos de 80%".

---

### 🟢 Nit

#### 27. Falta classe para os outros 4xx
* **IDs:** `design-1-13` · **Linhas:** 204–214
* **Discrepância e correção:** 400, 404 e 413 não têm classe, e é assim que um `PROBE_MODEL` aposentado apareceria. Adicionar a linha "4xx outros → `ERR`, conta como falha". Registrar também que um 401 com headers `unified-*` hoje é lido como OK (`api.cpp:63`).

#### 28. A citação de linha da linha 239 está errada
* **IDs:** `design-1-14` · **Linha:** 239
* **Discrepância e correção:** `.ino:827, 838` aponta para `start_data_web()` e `ui_token()`. A limpeza por estado fica em `render_state()`, `.ino:2620-2635`.

#### 29. "Compatível com o bridge atual" só vale para a conta em foco
* **IDs:** `design-1-15` · **Linhas:** 326–329
* **Discrepância e correção:** o bridge atual espera para sempre por uma conta que não está em foco (`token_bridge.py:87-89`) e trata o 404 como erro genérico, porque só reconhece o 409. Escrever "compatível só para a conta em foco; as outras exigem o bridge novo".

#### 30. A exposição na LAN aumenta e o documento não registra isso
* **IDs:** `design-1-16` · **Linhas:** 326–329, §7
* **Discrepância e correção:** `accounts[]` em `/window` expõe os rótulos e o uso de todas as contas corporativas para a LAN sem autenticação. O `POST /tokens` sem autenticação passa a poder falsificar qualquer conta. Acrescentar uma frase reconhecendo isso.

#### 31. A fase 2 não atende o CUJ 2
* **IDs:** `design-1-18` · **Linhas:** 99, 345
* **Discrepância e correção:** a fase 2 entrega a tela de várias contas sem o card de falha e sem o ponto, mas diz ser "usável sozinha". Trazer para a fase 2 um marcador mínimo de falha (classe + idade).

#### 32. Dois estilos de marcador de lista
* **IDs:** `structural-1-7`
* **Discrepância e correção:** o documento mistura `*   ` com `- `. Usar `*   ` em todo o documento, como no template.

#### 33. Links e referências cruzadas sem padrão
* **IDs:** `structural-1-8`, `structural-1-9` · **Linhas:** 24, 168, 280, 332, 437–439
* **Discrepância e correção:** há URLs soltas ao lado de links `[texto](caminho)`. "(drift report)" e "no brief" aparecem sem link. Usar texto de link em todos e linkar os arquivos de `./notes/`.

#### 34. A seção Detailed Implementation aparece com o nome errado e a tabela de layouts tem valores provisórios
* **IDs:** `structural-1-10`, `style-1-27` · **Linhas:** 263, 303
* **Discrepância e correção:** o cabeçalho real é "Detailed Implementation Plan". A linha 263 diz que a tabela "é fechada" lá, mas já traz valores exatos. Usar o nome exato ou uma âncora, e escrever "Valores provisórios; a versão final fica no Detailed Implementation Plan…".

#### 35. Referências de código formatadas de jeitos diferentes
* **IDs:** `structural-1-11` · **Linhas:** 200, 239, 280
* **Discrepância e correção:** `claude_stick.ino:N` aparece em texto simples, enquanto `lv_conf.h:43` está em código. Usar formatação de código em todas.

#### 36. Hierarquia de cabeçalhos
* **IDs:** `structural-1-12` · **Linhas:** 433, 441
* **Discrepância e correção:** Additional Resources e Future Improvements são H3 e ficam, na estrutura, debaixo dos NFRs. O template faz o mesmo, então é aceitável. Opcional: promover para H2.

#### 37. Conteúdo duplicado que pode divergir
* **IDs:** `structural-1-13` · **Linhas:** 166/219, 314/423, 320/416
* **Discrepância e correção:** a zeragem de tokens, o orçamento de flash e as regras de NOWIFI e de dados velhos aparecem em dois lugares cada. Manter cada regra num lugar só e referenciá-la no outro.

#### 38. A §5 é longa e usa negrito no lugar de subtítulos
* **IDs:** `structural-1-14` · **Linhas:** 237–276
* **Discrepância e correção:** transformar "Layouts da tela inicial", "Momentos de limiar" e "Pulso do ponto" em H4.

#### 39. Os textos da projeção aparecem sem explicação
* **IDs:** `style-1-14` · **Linha:** 66
* **Discrepância e correção:** escrever `~78%` "(uso previsto no reset)" e `38m` "(tempo até esgotar)".

#### 40. Ressalvas defensivas
* **IDs:** `style-1-15` · **Linhas:** 36, 231
* **Discrepância e correção:** cortar "registrado para não ser esquecido" e "para não ser lido como bug". Escrever "Custos aceitos pelo dono:" e "…por decisão do dono."

#### 41. Repetição e julgamento na seção do problema
* **IDs:** `style-1-16` · **Linhas:** 31, 33
* **Discrepância e correção:** "o que importa", "o que interessa" e "nem faz sentido" repetem a mesma ideia. Linha 33: "ocupa espaço e não mostra se alguma conta está falhando. Com várias contas, uma idade única não descreve o estado."

#### 42. "Área de toque lateral" é vago
* **IDs:** `style-1-17` · **Linha:** 32
* **Discrepância e correção:** escrever "libera as bordas laterais, hoje reservadas ao swipe".

#### 43. Frase truncada
* **IDs:** `style-1-18` · **Linha:** 39
* **Discrepância e correção:** escrever "faz as chamadas HTTPS de forma bloqueante dentro do `loop()`" e cortar "maior que o visual".

#### 44. Inglês sem necessidade
* **IDs:** `style-1-19` · **Linhas:** 81, 347, 422
* **Discrepância e correção:** trocar "draft" por "rascunho", "Restyle" por "Novo visual" e "home de 4 contas" por "tela inicial com 4 contas".

#### 45. "Juntados" é informal
* **IDs:** `style-1-20` · **Linhas:** 82, 296
* **Discrepância e correção:** usar "mesclados".

#### 46. Sujeito errado na linha 314
* **IDs:** `style-1-21` · **Linha:** 314
* **Discrepância e correção:** escrever "A fase 0 registra o tamanho do app da v2.3…".

#### 47. "Hoje…" na segurança é ambíguo
* **IDs:** `style-1-22` · **Linha:** 318
* **Discrepância e correção:** escrever "A v3 mantém a chave única para todos os slots (SHA-256, 10 mil rodadas, MAC como salt)."

#### 48. Opiniões apresentadas como fatos nas alternativas
* **IDs:** `style-1-23` · **Linhas:** 353–354, 359, 408
* **Discrepância e correção:** "o mais legível de longe", "visual genérico", "identidade forte" e "lido como enfeite" são opiniões. Atribuí-las ao dono. Na linha 359, dizer qual é a "foto do caduceu".

#### 49. A duração da travada da Alternativa D não mostra o pior caso
* **IDs:** `style-1-24` · **Linha:** 372
* **Discrepância e correção:** escrever "~1–2 s, até 15 s em timeout, como na v2.3".

#### 50. Medida imprecisa da Iosevka
* **IDs:** `style-1-25` · **Linha:** 396
* **Discrepância e correção:** escrever "largura de 0,5 em vez de 0,6 em".

#### 51. Detalhe de processo nas referências
* **IDs:** `style-1-26` · **Linha:** 435
* **Discrepância e correção:** cortar "com 6 bloqueios e 21 itens… de 7 revisores Goldfish".

#### 52. Ressalva sobre a linha 175 (jargão "absorvido")
* **IDs:** `style-1-10` (parte Nit)
* **Discrepância e correção:** está coberta no item 25 e foi listada aqui só para rastrear o id.
