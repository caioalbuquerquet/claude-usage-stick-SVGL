# THEP — Design System

Sistema de design da **Thep Inovação Sustentável**, derivado do *Manual da Marca
THEP v1.1* (Junho · 2026) e do código do produto **THEP EDU**. Serve quatro
produtos com dois públicos muito diferentes, e a estrutura existe para que o que
é da marca seja compartilhado e o que é do público não vaze de um produto para
o outro.

---

## 1. Referências — de onde vêm as decisões

Nenhuma decisão deste sistema saiu de preferência. Abaixo, o que sustenta cada
uma. O peso importa: **revisada** é pesquisa revisada por pares ou meta-análise;
**institucional** é lei, norma técnica ou pesquisa oficial; **setorial** é
agência ou consultoria — útil e prescritiva, mas não revisada, usada quando
confirma outra fonte.

**Onde o raciocínio completo mora**

- ***Pesquisa UI/UX para 9–14 anos*** — a evidência, a auditoria do sistema e a seção "Decisões fechadas", com data, fundamentação e a consequência aceita de cada decisão.
- ***Lei de Uso — THEP Academy*** — as regras normativas que derivam dela, com o checklist bloqueante de aprovação de feature.

**Comportamento e usabilidade da faixa**

| Fonte | Peso | Sustenta |
|---|---|---|
| NN/g — *Children's UX* (3–12), 125 crianças em três rodadas | revisada | Faixas etárias estreitas; crianças pulam instrução longa; navegação redundante confunde |
| NN/g — *Teenager's UX* (13–17), 100 adolescentes em 15 anos | revisada | Rejeição ao tom infantil e ao tipo pequeno; social sob controle do aluno; lentidão é deal-breaker |
| NN/g — *Usability Testing with Minors* | revisada | Protocolo: sessões individuais para 9–11, criança como especialista |
| Kidscorp — 10 princípios U18 | setorial | Alvos de toque por idade, memória de trabalho, hub &amp; spoke, padrões escuros vedados |
| Aufait UX | setorial | Co-design com crianças |
| Anne Fortes — TDC 2019 | setorial | Perfil 9–12: menos cor, mais vocabulário |

**Motivação e gamificação**

| Fonte | Peso | Sustenta |
|---|---|---|
| Li et al. (2024), *Educational Technology R&D* | revisada | Rankings prejudicam alunos de baixo desempenho; ranking relativo mitiga; escolha de tarefa atende autonomia |
| Kurnaz (2025), *Psychology in the Schools* | revisada | Recompensa produz motivação controlada fora de ambiente que apoie autonomia, competência e pertencimento |
| *Snapchat Streaks* (2019) | revisada | Sequências associadas a uso problemático de smartphone em adolescentes |

**Pressão de tempo**

| Fonte | Peso | Sustenta |
|---|---|---|
| Boaler, sintetizando Beilock (2011) e Ramirez et al. (2013) | revisada | Pressão de tempo bloqueia a memória de trabalho; para ~1/3 dos alunos o teste cronometrado inicia a ansiedade matemática |
| WCAG 2.2 — SC 2.2.1 e 2.2.3 | institucional | Evento ao vivo é exceção; onde há tempo, exigir alternativa sem tempo com o mesmo crédito |

**Acessibilidade e tipografia**

| Fonte | Peso | Sustenta |
|---|---|---|
| WCAG 2.2 — SC 1.3.4 Orientation | institucional | Não travar orientação — base do paisagem-first com retrato funcional |
| Zorzi et al. (2012), *PNAS* | revisada | Espaçamento entre letras: +20% na velocidade de leitura, metade dos erros |
| Wery &amp; Diliberto (2017); Kuster et al. (2017) | revisada | OpenDyslexic e Dyslexie não produzem ganho — **contrariam** a Kidscorp |
| British Dyslexia Association | institucional | Sans-serif legível, entrelinha mínima 1,5, alinhamento à esquerda |
| Viénot, Brettel &amp; Mollon (1999) | revisada | Simulação de protanopia e deuteranopia — base da escala de séries |

**Contexto brasileiro e regulação**

| Fonte | Peso | Sustenta |
|---|---|---|
| Cetic.br — TIC Kids Online Brasil 2025 | institucional | Celular único para 77% das classes DE; queda do acesso escolar; uso de IA para estudar |
| Lei 15.211/2025 — ECA Digital | institucional | Proteção por padrão; verificação de idade; vedação a loot boxes e perfilamento publicitário |
| Data Privacy Brasil | institucional | Leitura aplicada da lei; design manipulativo e abusivo |
| ICO — Children's Code, 15 padrões | institucional | Sinal de supervisão, nudges a favor do usuário, escolhas simétricas |
| D4CR / UNICEF | institucional | Participação da criança, consentimento duplo, devolutiva |

**Quando as fontes divergem.** Peso maior vence, e a divergência fica
registrada. Aconteceu uma vez: a Kidscorp recomenda OpenDyslexic e dois estudos
revisados mostram que a fonte não produz ganho — o sistema seguiu os estudos.

Os links de cada fonte estão no card *Referências*, em Components, e nos dois
documentos citados acima.

---

---

## 2. Como este sistema se organiza

Três camadas. Cada card deste sistema pertence a exatamente uma delas.

| Camada | O que é | Quem usa |
|---|---|---|
| **1 · Fundamentos** | Cor, tipografia, espaço, marca, ícones, gráficos, bibliotecas | Os quatro produtos |
| **2a · Base Pro** | Componentes de público adulto: densidade alta, alvo 44px, tema claro padrão | Gestão, Nexus, Formação |
| **2b · Base Academy** | Componentes de aluno: alvo 48/56px, modos f1/f2, tema escuro, linguagem da faixa | Academy |
| **3 · Produtos** | Telas e composições de cada produto | um cada |

### Os quatro produtos

| Produto | Quem usa | Base | Tokens que lê |
|---|---|---|---|
| **Gestão (ERP)** | Professores, diretores, gestores e responsáveis | Pro | `--thep-*` `--color-*` |
| **Nexus** | Colaboradores internos da THEP — kanban, calendário, financeiro, formações | Pro | `--thep-*` `--color-*` |
| **Formação** | Profissionais do município — horário, data e local das formações, emissão de certificado | Pro | `--thep-*` `--color-*` |
| **Academy** | Alunos do 4º ao 9º ano (9 a 14 anos) | Academy | `--thep-*` `--color-*` `--ac-*` |

**A regra que sustenta a separação.** Nenhum componente da Base Pro lê `--ac-*`.
Nenhum componente do Academy assume densidade de ERP. Se um componente precisa
de exceção para servir aos dois, ele não pertence à camada compartilhada — ele
é dois componentes.

Card: *Fundamentos · Arquitetura → Camadas e produtos*.

### Governança — qual documento manda em quê

| Documento | Responde | Manda em |
|---|---|---|
| **Lei de Uso — THEP Academy** | *Podemos construir isso?* | Privacidade, persuasão, tempo de uso, contato entre alunos, recompensa, IA, equidade. Traz o checklist bloqueante de aprovação de feature. |
| **Pesquisa UI/UX para 9–14 anos** | *Por que decidimos assim?* | Evidência e registro das decisões, com data e fundamentação. |
| **Este design system** | *Como isso se parece?* | Tokens, componentes, telas, responsivo. |

Em conflito: lei aplicável > melhor interesse da criança > evidência de pesquisa
> preferência de produto. A Lei de Uso vence o design system; quem muda é o
design system.

**De onde vêm as decisões.** Nenhuma saiu de preferência. O índice das fontes,
com nota de confiabilidade de cada uma, está na seção 1 deste guia, e com os links no card *Referências*. O raciocínio
está em *Pesquisa UI/UX para 9–14 anos*, seção "Decisões fechadas"; as regras que
dele derivam, em *Lei de Uso — THEP Academy*.

---

## 3. Sobre a Thep

Fundada em **2019, em Maceió (AL)**, a Thep aproxima tecnologia, formação
pedagógica e gestão da realidade da **escola pública brasileira**. Atende redes
de ensino em todo o Brasil, combinando **produtos digitais** e **serviços
presenciais** num único ecossistema.

- **Produtos** — ERP de gestão escolar; plataforma pedagógica do aluno (THEP EDU).
- **Serviços** — capacitação presencial: workshops temáticos, jornadas pedagógicas, formação continuada.
- **Missão** — transformar a educação pública por meio de tecnologia confiável e formação de excelência. **Visão** — ser a principal referência em soluções para a educação pública no Brasil até 2030.
- **Valores** — compromisso público, profissionalismo, inovação sustentável, proximidade, clareza, impacto medido.

**Dados institucionais** (rodapés e papelaria): CNPJ 35.428.971/0001-11 ·
contato@thep.com.br · 82 99666 8588 · Av. Alm. Álvaro Calheiros, 1000 — Jatiúca,
Maceió · AL, 57035-558 · thep.com.br · @thep.incompany

> O subtema **"pixel/retro"** que existia no perfil do aluno do THEP EDU foi
> descontinuado. O ambiente do aluno agora é o **THEP Academy**.

---

## 4. Tom de voz

Falamos com **servidores públicos, gestores escolares e alunos**. Somos
**confiáveis, profissionais, diretos, humanos e acessíveis**; **não** somos
burocráticos, arrogantes, vagos, frios nem pretensiosos.

- **Pessoa:** trate o leitor por **"você"**. Fale da empresa como **"a gente / nós"**, sem juridiquês.
- **Casing:** títulos em **caixa baixa natural**; **eyebrows e tags** em CAIXA ALTA com tracking 0.16em. Sem Title Case em inglês.
- **Frases curtas e acionáveis.** A complexidade fica do nosso lado.
- **Emoji:** não usar em peças de marca ou produto.
- **Números** com separador de milhar em ponto (pt-BR): `1.284`, `+1.200`.

**Diga:** "Sua escola já está cadastrada. Faltam 3 passos para começar."
**Evite:** "Prezada instituição de ensino, em conformidade com o cadastro previamente efetuado…"

O Academy tem voz própria, descrita na seção 8.

### Bordão — formas aprovadas (§04.1)

O bordão é **assinatura institucional**, nunca copy funcional. Use sempre no
formato oficial, sem cortar.

**Quando usar** — peças institucionais e capas; campanhas externas e materiais
para gestores públicos; assinatura de fim de e-mail, vídeo ou peça de marca;
ambientes físicos. **Quando evitar** — dentro de telas de produto; comunicações
operacionais e transacionais; como subtítulo funcional de um serviço; quebrado,
parafraseado ou traduzido sem aprovação.

| Forma | Como é |
|---|---|
| **1 · Bloco** | Frase completa em duas linhas, Instrument Serif, sobre neutro claro. |
| **2 · Inline com logo** | Logo reverso + régua vertical + frase em itálico, sobre Azul Marinho. |
| **3 · Curta (uso restrito)** | "O caminho a gente ensina." — **somente** onde a frase completa não couber. |
| **4 · Sobre faixa-marca** | Frase em branco sobre o gradiente do símbolo. |

---

## 5. Cor

Paleta ancorada no **Azul Marinho `#122852`** (principal) e **Azul Thep
`#1272C2`** (ação), apoiados por **Teal** e **Verde**. Secundárias (ciano, roxo,
lavanda, magenta, rosas, pêssego) vêm do gradiente do símbolo e servem a
ilustrações, gráficos e materiais do aluno. Neutros: branco, cinza `#4B4B4B`,
preto. **Proporção 60 / 30 / 10** — neutros / primárias / acento.

**Acessibilidade cromática (§09).** Mínimo **WCAG AA**: **≥ 4.5:1** para texto
corpo, **≥ 3:1** para texto grande (≥ 24px) e para objeto gráfico.

| Aprovadas | Ratio | A evitar | Ratio |
|---|---|---|---|
| navy sobre branco | AAA · 14.5 | branco sobre pêssego | FAIL · 1.6 |
| branco sobre navy | AAA · 14.5 | pêssego sobre branco | FAIL · 1.6 |
| azul Thep sobre branco | AA · 5.0 | branco sobre verde | FAIL · 2.7 |
| navy sobre pêssego | AAA · 8.9 | roxo sobre lavanda | FAIL · 1.5 |

> **Reaferição de 18/09/2026.** Ratios recalculados pela fórmula WCAG 2.x a
> partir dos hex de `tokens.json`. **Branco sobre teal é 3.4:1** (a tabela
> anterior dizia 4.6) — reprovado para corpo, aprovado só para texto ≥ 24px e
> elementos de UI. **Branco sobre verde é 2.7:1** (dizia 1.5). **Navy sobre teal
> é 4.25:1**, também abaixo do mínimo de corpo.

Texto sobre cor secundária é **sempre branco ou azul-marinho** — mas teal e
verde não carregam texto corpo em nenhuma das duas tintas: sobre esses fundos,
use texto ≥ 24px, ícone ou rótulo em caixa alta, ou inverta (tinta teal
`--ac-dominada-ink` sobre fundo de espaço, 8.2:1). Cores claras (pêssego, rosas)
só carregam texto em **fundo escuro**.

### Séries de gráfico

Cinco séries, `--chart-1` a `--chart-5`, com variantes próprias no tema escuro.
Verificadas em visão típica, protanopia e deuteranopia: pior separação de ΔE 18
no tema claro e 22 no escuro, muito acima do limiar de percepção.

Quando a cor da série **também vira texto** (valor colorido ao lado da barra, nome
destacado na legenda), o mínimo sobe de 3:1 para 4,5:1 e três séries não alcançam. Para
isso existe `--chart-N-ink`, definido para as cinco: onde a série já passa, o `-ink`
repete o valor dela, de modo que o consumidor nunca precise saber qual slot tem exceção
nem em que tema está.

Teto de **cinco séries** — acima disso, agrupe o excedente. **Cor nunca sozinha:**
rótulo direto, marcador ou padrão de preenchimento. Card: *Fundamentos · Gráficos*.

---

## 6. Tipografia

Duas famílias: **Instrument Serif** (display e editorial — capas, títulos,
citações) e **Geist** (texto e UI). Escala modular 1.25× (64/72 H1 → 10/14
eyebrow). O contraste vem da hierarquia, não da variedade. Office: Cambria /
Aptos. Fallback web: `system-ui`. O Academy usa **Baloo 2** no lugar do
Instrument Serif (seção 8).

---

## 7. Layout, forma e movimento

**Espaçamento.** Base **4px** (4·8·12·16·24·32·48·64·96). Grid de **12 colunas**,
gutter 8px, container web máx. **1200px** (margem responsiva 16–48px). A4:
margens 56/56/48. Slides 16:9: margens internas 64px, reserva inferior 80px.

**Cantos e sombras.** Raios suaves (md 10px em botões e inputs, lg 14px em cards,
pill em chips). Sombras discretas, luz vinda de cima (`--shadow-xs…lg`); nada de
drop-shadow pesado. Card = superfície branca + borda 1px + `--shadow-sm` + raio lg.

**Fundos e gradiente.** Majoritariamente **neutros claros**; blocos
institucionais em **Azul Marinho sólido** (ou `--gradient-institutional`
navy→azul). A **faixa-marca** (`--gradient-brand`) é o único elemento
expressivo: faixa fina horizontal (8–24px) ou recorte diagonal — sutil, nunca
dominante. Sobre fotografia, busque fundo uniforme e aplique camada navy 60–80%
atrás do logo.

**Movimento.** Transições breves e funcionais (`--duration-fast/base/slow`,
`--ease-out` para entradas). Fades e translações leves (`translateY(14px)`); sem
bounces. Respeita `prefers-reduced-motion`.

**Estados.** Hover: escurece o primário, preenche levemente o outline, fundo
suave no ghost. Press: `translateY(1px)`. Foco: anel `--shadow-focus` (azul 32%).
Disabled: opacidade ~0.45.

---

## 8. Marca, ícones e aplicações

### Logo

`assets/logo/`: `thep-logo-lockup.svg` (positivo, fundo claro) ·
`thep-logo-reverse.svg` (wordmark branco, fundo escuro) · `thep-symbol.svg`
(só o pássaro — avatares, favicons, selos) · `thep-icon-192.png` e
`thep-icon-512.png` (app/PWA).

**Anatomia (§05).** **A. Símbolo** — pássaro em gradiente (liberdade, leveza,
diversidade). **B. Wordmark** — "THEP" em caligrafia única; nunca substituir por
outra fonte. **C. Assinatura** — "Inovação Sustentável", sempre subordinada ao
wordmark.

**Variações (§05.1).** 01 Principal/positiva (preferencial) · 02 Reversa · 03
Monocromática (impressão 1 cor) · 04 Símbolo. Sempre o arquivo original — nunca
recriar.

**Respiro e mínimos (§06).** Respiro = **1× a altura do "H"** do wordmark em
todos os lados.

| Uso | Mínimo | Token |
|---|---|---|
| Logo completo · digital | 24 px / 12 mm | `--logo-min-digital` |
| Logo completo · impresso | 18 px / 8 mm | `--logo-min-print` |
| Símbolo isolado · favicon | 16 px / 6 mm | `--symbol-min` |

**Usos incorretos (§07.1).** Não distorcer · não trocar as cores · não
rotacionar · não usar com baixo contraste · não aplicar efeitos · não reduzir a
opacidade.

### Marca do Academy

`assets/Academy/thep-academy-logo-lockup.png` — mascote astronauta + "thep
academy", para login do aluno, abertura e material para escolas. As tintas do
arquivo (teal `#0D9991`, azul `#1079C8`) são praticamente as institucionais: a
marca do Academy nasce dentro da paleta; o que muda é o desenho. Ainda sem
versão vetorial, reversa ou reduzida.

### Ícones

Linhas **outline**, traço **1.75px**, grid 24×24 com 2px de padding, cantos 2px.
Geométrico, sem floreios; sem preenchimento sólido exceto badges e estados
ativos. Biblioteca base **Lucide** (ou Phosphor outline). Emoji ou unicode como
ícone: não usar.

### Aplicações (§14)

Cartão de visita · post 1080×1080, story 1080×1920 e avatar 400×400 ·
assinatura de e-mail · header do site (Soluções · Produtos · Workshops · Sobre,
com CTA "Entrar na plataforma"). Todas token-driven, no grupo *Marca · Site e
aplicações*.

**Certificados.** `assets/Certificados/` traz frente e verso do modelo, 2000 ×
1414 px. Todo texto variável fica nos ~70% à esquerda; a faixa curva ocupa a
borda direita e não recebe texto. **Atenção:** as faixas usam roxo `#6C48EC` e
verde `#30BA52`, fora da paleta institucional, e o símbolo difere do
`thep-symbol.svg`. A divergência está registrada e pende de decisão.

> ⚠️ **Papel timbrado A4 — pendente.** O modelo do manual v1.1 está em revisão
> pelo time de marca e não foi portado. Não derive timbrado daqui.

---

## 9. THEP Academy — o subtema do aluno

Ambiente de ensino gamificado para alunos do **4º ao 9º ano**. Mundo de jogo
espacial: mundos (matérias), planetas (unidades), fases, naves, avatares, XP,
moedas, meta da semana, esquadrões e um tutor NPC.

> **Normativo.** As regras de uso responsável vivem em *Lei de Uso — THEP
> Academy*. Toda feature nova passa pelo checklist de lá antes de entrar no
> backlog. Este guia descreve o sistema visual; aquele documento descreve o que
> é permitido construir com ele.

**Princípio:** o Academy **não inventa cores**. Ele promove as secundárias da
marca (teal, azul, pêssego, rosa, magenta) ao primeiro plano e as assenta sobre
uma escala de "espaço" derivada do Azul Marinho `#122852`. A marca continua
reconhecível; só a hierarquia muda. Prefixo de token: `--ac-*`.

**Tipografia.** Display **Baloo 2** em títulos, números e botões; corpo em
**Geist**, porque enunciado de questão tem 3–5 linhas. Instrument Serif não entra
no Academy.

### Dois modos, um sistema

| | `[data-ac-mode="f1"]` | `[data-ac-mode="f2"]` |
|---|---|---|
| Série | 4º e 5º ano | 6º ao 9º ano |
| Tipo | 18–20% maior | próximo do produto adulto |
| Alvo de toque | 56px | 48px |
| Nó de fase | 76px | 64px |
| Contorno / raio | 3px · 24–32px | 2px · 16–22px |
| Brilho | completo | reduzido a um anel |

Paleta, estados de trilha, ícones, ordem do HUD e regras de contraste **não**
mudam entre os modos. O corte em 5º/6º acompanha a mudança de regime escolar do
Fundamental I para o II, que o próprio aluno reconhece.

### Estados de trilha

`dominada` (teal) · `concluida` (azul) · `disponivel` (pêssego) · `bloqueada`
(cinza-azul) · `dificuldade` (magenta). Cada estado tem variante `-ink` para
texto sobre o espaço, com contraste AA garantido. Todo nó carrega **cor + ícone
+ rótulo em texto** — cor sozinha não é acessível.

### Mecânicas: o que o sistema não faz

Decisões de 18/09/2026, tomadas sobre pesquisa e registradas na Lei de Uso:

| Decisão | O que mudou |
|---|---|
| **Sem ranking individual** | A home mostra o placar do **esquadrão** (grupos de 4–5, com rotação) e a **meta coletiva da turma**. Ranking de XP absoluto transmite feedback negativo a quem tem baixo desempenho. |
| **Sem ofensiva** | Entraram a **meta da semana**, definida pelo aluno, e o **contador de dias estudados**, que só aumenta. Sequência diária punitiva é padrão escuro vedado para menores de 18 e pune desigualdade de acesso. |
| **Sem baú** | Recompensa determinística e anunciada antes da tarefa. O ECA Digital veda loot boxes para esse público. |
| **Duelo opt-in, só no f2** | Vive num planeta de gincanas, convite restrito a proficiência próxima. Vence quem **acerta mais**; tempo só desempata. |
| **Nada se perde** | Nenhuma mecânica subtrai XP, moeda, medalha ou posição por ausência, erro ou desistência. |

### Responsivo — paisagem-first, retrato funcional

Composto para **celular em paisagem**, onde o mapa de trilha respira. Mas
**nenhuma tela trava orientação**: o critério 1.3.4 do WCAG 2.2 exige que o
conteúdo não restrinja sua operação a uma única orientação.

| Faixa | Token | Comportamento |
|---|---|---|
| `< 560px` | `--bp-mobile-portrait` | Retrato. HUD quebra com a barra de XP em linha própria; trilha vira lista vertical. |
| `560–1023px` | `--bp-mobile-landscape` | Paisagem e tablet. Coluna lateral vira duas colunas. |
| `>= 1024px` | `--bp-desktop` | Desktop e Chromebook escolar. Mapa de trilha absoluto. |

### Componentes de conformidade

`SupervisionIndicator` · `PermissionPrompt` · `AILabel` · `PrivacidadeAluno` ·
`ConnectionStates` · `FirstRun` · `AvatarIdentity` · `ShareLink` ·
`AccessibilityPrefs`. Obrigatórios pela Lei de Uso; cada um traz no próprio
preview as regras e a evidência que as sustenta.

### Restrições que moldam o subtema

- **Chromebook escolar fraco** — sem `backdrop-filter`, sem blur grande, no máximo 2 camadas de elevação, animação só em `transform`/`opacity`, no máximo 3 elementos animando ao mesmo tempo, cenário ≤ 400KB.
- **Acessibilidade obrigatória** (rede pública) — texto sempre na variante `-ink`, foco de teclado sempre visível (`--ac-ring-focus`), `prefers-reduced-motion` zera durações sem alterar o estado final.
- **Celular é o único aparelho** para 77% dos alunos das classes DE: orçamento de performance apertado, tolerância a rede instável, salvamento frequente.

---

## 10. Bibliotecas de terceiros

**shadcn/ui** é o padrão da Base Pro. **EvilCharts** (shadcn + Recharts, MIT) é o
padrão de gráficos. Ambos são **copiados** para o repositório, não instalados:
o código vira nosso, e com ele a manutenção e o alinhamento com os tokens.

Ao copiar, re-tokenize sempre — um componente colado sem ajuste traz a paleta da
biblioteca. Se já existe equivalente na Base Pro, use o nosso.

Card: *Fundamentos · Bibliotecas*, que traz a lista completa, o que ajustar na
entrada e o critério para adotar uma biblioteca nova.

---

## 11. Consumindo este sistema no código

Os componentes React reais têm `.d.ts` (contrato de props) e preview vivo. Os
cards do Academy são **especificação visual**, não componentes montáveis — o
Academy ainda não tem primitivos em React.

Os tokens vivem em `tokens.json` e são compilados para `tokens.css`. Num
monorepo Tailwind v4, mapeie-os com `@theme inline` e troque a variável crua no
seletor de tema; não redeclare `--color-*`.

---

## 12. Caveats

- **Fontes via Google Fonts** (`@import`), não auto-hospedadas. Instrument Serif, Geist e Baloo 2 são OFL (uso livre e comercial).
- O **subtema pixel** do app foi descontinuado.
- Não foi fornecido template de deck; os slides seguem §13 do manual.
- **Papel timbrado A4** intencionalmente ausente.
- **Nexus** e **Formação** ainda não têm cards próprios: consomem a Base Pro.
- O **planeta de gincanas** do Academy não tem tela; suas regras estão na Lei de Uso.

---

## 13. Histórico

| Versão | Data | Mudanças |
|---|---|---|
| **1.2** | Set · 2026 | Guia reconstruído por camada e produto. Quatro produtos declarados (Gestão, Nexus, Formação, Academy); Base dividida em Pro e Academy. Previews para os 11 componentes React. Tokens documentados. Escala de séries de gráfico verificada para daltonismo. Marca do Academy e modelo de certificado adicionados. Correções de 18/09: modos f1/f2 religados, entrelinha 1,55, piso de 14px, contraste reaferido, durações de movimento restauradas. |
| **1.1** | Junho · 2026 | Bordão com 4 formas aprovadas (§04.1); respiro e tamanhos mínimos do logo (§06); usos incorretos (§07.1); acessibilidade cromática com ratios (§09); aplicações (§14). |
| 1.0 | Maio · 2026 | Versão inicial — paleta, tipografia, grid, logo, componentes de produto. |

**Fontes originais.** *Manual da Marca THEP v1.1* (PDF) é a fonte de verdade para
cores, tipografia, logo, grid, aplicações e tom de voz; a v1.0 fica como
histórico. Tokens e logos também foram lidos do repositório privado
`THEP-Inovacao-Sustentavel/edu`. As seções do manual são citadas pelos rótulos
impressos em cada página (§04.1 Bordão, §08 Cores, §10 Tipografia, §13 Layout,
§14 Aplicações, §15 Componentes), que divergem do sumário a partir do item 05.

Dúvidas ou nova aplicação: **contato@thep.com.br** — fale com o time de marca antes.

---

## Migrated from a legacy design system

This system was carried over from the standalone version on 2026-09-18. The part of this README the author wrote predates the move, so any file names in it are the old ones. Where things are now:

- `styles.css`, `tokens/fonts.css`, `tokens/colors.css`, `tokens/typography.css`, `tokens/spacing.css`, `tokens/effects.css`, … and 2 more (the global stylesheets) → `project/components/bundle.css`, with the token declarations moved to `project/tokens.json` (`project/tokens.css` is generated from them)
- `_ds_bundle.js` → `project/components/bundle.js`
- showcase pages, each kept whole as one component’s preview (a page of examples, not an export of the bundle): `academy/hud.html` → `project/components/Hud/preview.html`; `academy/mission-nodes.html` → `project/components/MissionNodes/preview.html`; `academy/modes.html` → `project/components/Modes/preview.html`; `academy/screen-atividade.html` → `project/components/ScreenAtividade/preview.html`; `academy/screen-home.html` → `project/components/ScreenHome/preview.html`; `academy/screen-trilha.html` → `project/components/ScreenTrilha/preview.html`; `academy/surfaces.html` → `project/components/Surfaces/preview.html`; `applications/email-signature.card.html` → `project/components/EmailSignature/preview.html`; … and 12 more
- the migration report, which lists what did not come across: `project/assets/notes/MIGRATION-REPORT.md`
