# Drift report — READMEs vs. código (firmware v2.3)

Gerado em 2026-11-08 com `/egm:egm-drift-report`. Escopo: `README.md` e `README.pt-BR.md`
comparados com `firmware/claude_stick/`, `firmware/bringup/` e `tools/`. Só lista divergências.

## README.md

- **Barra de refresh clicável (README.md:71-72).** Diz que tocar na barra coral fina "refreshes
  immediately". No código a barra tem `LV_OBJ_FLAG_CLICKABLE` removido
  (`claude_stick.ino:2171`); quem atualiza é o botão central `LV_SYMBOL_REFRESH`
  (`claude_stick.ino:2131`). O próprio README se contradiz na linha 167 ("the thin coral bar below
  it is just the countdown indicator").
- **Pílulas de status dos modelos (README.md:93-95).** O README lista `OK` / `LIMITADO` /
  `ERRO` (5xx/rede) / `N/D`/`--`. O código (`model_chip`, `claude_stick.ino:1313-1321`) tem mais
  estados e outra semântica: `AUTH` (401/403, vermelho), `REDE` (erro de rede, vermelho),
  `ERRO <código>` (outros códigos, com o número), `N/D` só para HTTP 404 e `--` para "nunca
  sondado". `AUTH` e `REDE` não aparecem no README.
- **Ordem dos Ajustes (README.md:178-191).** A ordem documentada é Refresh now → interval →
  slideshow → timezone → brightness → WiFi → accounts → token → language → about → erase. No código
  (`ui_settings`, `claude_stick.ino:2353-2373`) é: Refresh now → interval → slideshow →
  **language** → timezone → brightness → WiFi → accounts → token → about → erase.
- **Fuso "GMT±N" (README.md:182).** Sugere qualquer offset. O código só alterna entre
  `TZ_OPTS = {-3,-4,-5,-6,-7,-8,-2,-1,0,1,2,3}` (`claude_stick.ino:2212`): não existe GMT+4 ou
  mais, nem GMT−9 ou menos. O padrão é GMT−3.
- **Bloqueio por PIN (README.md:448-449).** Diz que "each failure doubles the lockout time" e não
  fala em limite. O código começa em 60 s (`LOCKOUT_BASE_SEC`) e **para em 3600 s**
  (`claude_stick.ino:487-488`).
- **Derivação da chave (README.md:445).** Diz "key derived from the PIN via SHA-256". O código
  (`crypto.cpp:10-27`) faz **10.000 rodadas** de SHA-256 (`KDF_ROUNDS`), com **salt = MAC do
  chip** (`esp_efuse_mac_get_default`). O MAC como salt amarra o blob à placa, e o README omite
  isso.
- **Frequência do status.claude.com.** O `config.h:40` define `STATUS_POLL_SEC 300` ("a cada 5
  min"), mas a constante **não é usada**: `fetchModelStatus()` roda em todo refresh
  (`claude_stick.ino:2696`, `2723`). Na prática o intervalo é o mesmo do polling configurado (30 s a
  5 min). O README não diz a frequência, e o código contraria a própria intenção declarada.
- **Headers lidos (README.md:249-258).** O código também coleta e usa
  `anthropic-ratelimit-unified-5h-status`, `anthropic-ratelimit-unified-7d-status` e
  `anthropic-ratelimit-unified-reset` (`api.cpp:10-16`, `UsageData.status5h/status7d/unifiedResetEpoch`).
  Os três não estão na lista.
- **Escopo do token bridge (README.md:273).** Diz que os transcripts são
  `~/.claude/projects/**/*.jsonl` (recursivo). O `token_bridge.py:51` usa
  `glob(PROJECTS/*/*.jsonl)`, que pega **só um nível**. Transcripts em subpastas (por exemplo, os
  de subagentes) não entram na soma, então a contagem pode sair menor que a real.
- **Payload do bridge.** O README só menciona "entrada • saída". O bridge também envia `cache`
  (leituras de cache) e `sessions` (`token_bridge.py:96`), e o firmware guarda os dois
  (`claude_stick.ino:808-809`), mas eles não são exibidos nem documentados. A "entrada" inclui
  `cache_creation_input_tokens` (`token_bridge.py:71`), o que também não está documentado. A opção
  `--host` existe (`token_bridge.py:122`) e não aparece no README.
- **Nome da placa.** O README chama de "Mini ESP32-S3 3.5"" e aponta o STL
  `Case_JC3248W535C.stl`. O firmware se identifica como **Guition JC4832W535**, tanto no
  cabeçalho (`config.h:5`, `claude_stick.ino:3`) quanto na tela Sobre (`claude_stick.ino:2596`).
  Ou um dos lados está errado, ou falta explicar que é a mesma placa.
- **Mockups "match v2.3" (README.md:65-68).** O `gen_mockups.py` tem duas divergências além do
  badge `@label` já citado:
  - `mock_modelos()` ainda desenha a linha "status.claude.com: OK • sem incidentes"
    (`gen_mockups.py:177`), que o README (linha 96) diz ter saído dessa tela.
  - `mock_incidente()` desenha o chip `GRAVE` em âmbar (`WARN`, `gen_mockups.py:254`). O
    README e o firmware usam laranja `#FB923C` (`C_SEV_MAJOR`, `claude_stick.ino:53`).
- **Tabela de partições (README.md:499).** Diz "16 MB partition (app + nvs + LittleFS)". O
  `partitions.csv` tem **dois slots de app OTA** (`app0`/`app1`, 3 MB cada), `otadata` e
  `coredump`. Não há OTA implementado, então metade do espaço de app fica sem uso, e isso não está
  documentado.
- **Layout do repositório (README.md:486-511).** Faltam arquivos que existem:
  `firmware/claude_stick/clawd_dead.h`, `firmware/claude_stick/logo_assets.h`,
  `firmware/README.md` e `tools/gen_dead_clawd.py` (que gera o Clawd "morto" do dialog de
  incidente). Em `bringup/` faltam `bringup.ino`, `config.h` e `touch.h`.
- **Auto-close dos momentos (README.md:165).** Diz "~4.5 s". O código usa 4600 ms
  (`claude_stick.ino:1734`). Diferença pequena, citada só por rigor.

## README.pt-BR.md

- É tradução do `README.md`, com as mesmas afirmações, então **herda todas as divergências
  acima**. As correspondências principais:
  - barra de refresh clicável: linhas 72-73, contradizendo a linha 170;
  - pílulas `ERRO`/`N/D` sem `AUTH`/`REDE`: linhas 93-96;
  - ordem dos Ajustes e "Fuso: GMT±N": linhas ~180-193 e 186;
  - lockout sem limite e "via SHA-256": linhas 448-452;
  - glob `**/*.jsonl` do bridge e campos `cache`/`sessions`/`--host` omitidos: seção "Tokens por
    sessão";
  - lista de headers incompleta;
  - mockups (linha "status.claude.com" e cor do `GRAVE`);
  - partições OTA não documentadas;
  - layout do repositório incompleto;
  - nome da placa (JC3248W535C vs. JC4832W535).
- Nenhuma divergência própria do pt-BR em relação ao EN foi encontrada.

## Sugestão para facilitar as próximas checagens

As seções "Screens" e "Settings" não citam funções nem arquivos. Vale adicionar links como
`model_chip()`, `ui_settings()`, `incident_sync()` e `TZ_OPTS` em `claude_stick.ino`. Assim um
drift report futuro acha o código sem precisar de grep.
