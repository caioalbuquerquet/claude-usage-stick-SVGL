# firmware/ — Claude Usage Stick (ESP32-S3 + LVGL)

Firmware v3 para a tela touch **Guition JC4832W535** (AXS15231B QSPI, 480×320; vendida também
como JC3248W535C).

- **`claude_stick/`** — o projeto. Sketch arduino-cli: consulta o uso de **todas** as contas
  salvas (headers `unified-*` da `api.anthropic.com`) numa task de rede, a saúde dos modelos
  (`status.claude.com`) e desenha tudo em LVGL 9 num shell fixo com menu. Tokens OAuth guardados
  cifrados (AES-256-GCM, chave derivada do PIN).
- **`tests/`** — testes de computador da lógica pura (`claude_stick/src/logic/`).
- **`bringup/`** — bring-up validado no hardware (cores certas, orientação USB-à-esquerda e touch
  alinhado). Referência conhecida-boa da config de display/touch; não é o app. Compile com o
  `build.sh` **desta pasta** ([`bringup/build.sh`](bringup/build.sh)) — ele usa
  `PartitionScheme=huge_app` (não há `partitions.csv` aqui) e reaproveita o `lv_conf.h` do
  `claude_stick`; o FQBN do firmware não serve para esta pasta.
- **`REFERENCIA-HARDWARE-LVGL.md`** — pinos, libs testadas, pipeline de flush (rotação 270° e
  envio por faixas) e armadilhas (PSRAM OPI obrigatória, etc.).

Comece por [`../README.md`](../README.md) para a visão geral, as telas e o rollback para a v2.3. O
design da v3 está em [`../docs/designs/thep-usage-stick-v3.md`](../docs/designs/thep-usage-stick-v3.md).

## Build

Core `esp32:esp32` **3.3.8**, `arduino-cli` 1.4.x, GFX Library for Arduino 1.6.5 e lvgl 9.2.2.

```bash
cd firmware/claude_stick
./build.sh                       # compila em build/ e falha se o app passar de 2,8 MB
./build.sh upload [porta]        # compila + grava (padrão /dev/cu.usbmodem101)
./build.sh dev                   # compila com -DTHEP_DEV
./build.sh upload-dev [porta]    # idem dev + grava
./build.sh monitor <porta>       # serial a 115200
THEP_FLAGS="-D..." ./build.sh …  # flags extras (ex.: -DDISP_BANDS=0 para voltar ao modo FULL)
```

O `build.sh` passa `-DLV_CONF_INCLUDE_SIMPLE -I<sketch> -I<sketch>/src` nas flags de C, C++ e
assembler: o LVGL acha o `lv_conf.h` do sketch e os módulos se incluem pelo caminho a partir de
`src/` (`"net/api.h"`, `"ui/theme.h"`). O arduino-cli compila `src/` de forma recursiva.

A build `THEP_DEV` liga o `GET /health` (JSON sem tokens nem rótulos: heap interno livre e maior
bloco, PSRAM, `lv_mem_used_pct`, high-water da stack da `net_task`, duração do último ciclo,
ciclos acima do intervalo, tempo de quadro e classe/idade/espera por slot) e o log do LVGL no
serial. Em qualquer build o serial mostra `[CYC]` (início e fim de cada ciclo, classes, espera do
AUTH) e `[HEAP]` (heap, stack da task e tempos de quadro: `frame_max`, `flush_max`, `rot_max`,
`send_max`, `rows_max`).

## Módulos de `claude_stick/src/`

| Pasta | Arquivos | O que faz |
|---|---|---|
| `app/` | `app_state` | Estado da aplicação (PIN, onboarding, shell), troca de conteúdo do painel adiada para o `loop()` e as intenções das telas (`intent_open`, `intent_focus`, `intent_refresh`) |
| | `web_api` | Servidor HTTP local: formulário do token (`POST /token` → job `VALIDATE`, `GET /token/status`), `GET /window[?account=]`, `POST /tokens`, `GET /info` e `GET /health` (`THEP_DEV`); mDNS `claude-stick.local` |
| `hw/` | `touch.h`, `wifi_manager.h` | Driver de toque AXS15231B e redes salvas na NVS (até 3; `connectList` sem NVS para a task) |
| | `lv_pool` | Pool do LVGL (`LV_MEM_SIZE`, 96 KB) alocado na PSRAM pelo `lv_conf.h` |
| | `frame_stats` | Tempo de quadro e do flush (rotação e envio, linhas enviadas) para o `[HEAP]` e o `/health` |
| `logic/` | `projection`, `dot_state`, `classify`, `levels`, `reset_text`, `schedule`, `rot270`, `bridge_route`, `fetch_class.h`, `sample.h` | Funções puras, testadas no computador: projeção da 5h e o texto dela, cor do ponto, classe da resposta e texto de `/modelos`, níveis/barras/layout/rótulos/marca da API, texto do reset, regras do agendador e da gravação do histórico, rotação de 270° em blocos, rótulo → slot do contrato do bridge |
| `model/` | `account_state` | `AccountState acct[ACCT_MAX]` na PSRAM: último uso, classe, espera, histórico, limiares, contagem de tokens. Fonte única dos dados de cada conta |
| | `accounts` | Slots de conta na NVS (rótulo + token cifrado), rótulos únicos, migração da v2.1 |
| | `history` | Histórico, heatmap por hora/dia e `/hist{0..3}.bin` no LittleFS (formato da v2.3) |
| `net/` | `poller` | `net_task` (core 0, 14 KB de stack), filas `jobQ`/`resQ`, aplicação dos resultados no `loop()` e o reinício por requisição travada (> 90 s) |
| | `scheduler` | Ciclos de consulta no `loop()`: pré-condições (WiFi, NTP), um `USAGE` por conta, rodízio de modelos na conta em foco, `STATUS` a cada `STATUS_POLL_SEC`, espera do AUTH, pausa na tela de WiFi, fuso |
| | `token_store` | Tokens decifrados, token em validação e cópia das redes, tudo sob um mutex |
| | `api`, `status`, `certs` | Consulta de uso (headers `unified-*`), incidentes do `status.claude.com` e as CAs raiz |
| `sec/` | `crypto` | AES-256-GCM, chave derivada do PIN (SHA-256 × 10.000, salt = MAC) e a chave da sessão |
| `ui/` | `shell`, `overlay_menu` | Shell fixo (cabeçalho com logo, ponto pulsante e `❯ menu`; painel com título na borda; rodapé) e o menu modal |
| | `screen_*` | Conteúdos do painel (`home`, `models`, `trend`, `heat`, `accounts`, `settings`, `about`, `token`, `wifi`) e as telas cheias de antes do desbloqueio (`pin`, `wifi` e `token` do onboarding) |
| | `overlay_moment`, `overlay_incident` | Momento de limiar e alerta de incidente no `lv_layer_top` |
| | `theme`, `widgets`, `i18n.h` | Papéis de cor THEP, estilos e fontes; peças compartilhadas; textos pt/en (`TRS`, `tr()`) |
| | `fonts/`, `assets_thep.{h,c}` | **Gerados:** JetBrains Mono 11/13/bold 13 e numéricas bold 20/30/38/50; logo THEP e pássaro em braille |

O `claude_stick.ino` ficou com o hardware (display, touch, backlight), o PIN e a sessão
(desbloqueio de todos os slots, chave da sessão), o registro das telas e o `loop()`.

## Regra de concorrência (`net_task` × `loop()`)

O LVGL roda sem suporte a threads (`LV_USE_OS LV_OS_NONE`). Por isso:

- **`net_task`** (core 0): é a única que abre HTTPS e a única que mexe no rádio depois do
  desbloqueio (job `RECONNECT`). **Nunca** chama `lv_*`, nunca escreve no modelo e nunca acessa
  LittleFS ou NVS. Lê o token do slot com `token_store_copy*` para um buffer de stack, que zera no
  fim do job.
- **`loop()`** (core 1): é o **único dono** do modelo (`acct[]`, `g_accts`), do LVGL, do LittleFS e
  da NVS. Monta os jobs (`scheduler`, `web_api`) e aplica os resultados
  (`poller_apply_results()`).
- **Comunicação:** duas filas FreeRTOS. `jobQ` (loop → net) leva `{tipo, slot, gen, modelo}`,
  **sem token**; `resQ` (net → loop) leva o resultado.
- **`gen` por slot:** muda ao adicionar, remover ou trocar o token. A task não consulta um job com
  `gen` diferente do da tabela (devolve `FC_DISCARDED`), e o `loop()` descarta um resultado com
  `gen` velho, então uma resposta atrasada nunca cai numa conta removida.
- **Uma conexão por vez:** cada sessão TLS ocupa ~40–50 KB de RAM interna.
- **Dependências entre módulos:** `src/logic/` só inclui a biblioteca padrão e outros headers de
  `src/logic/` (os valores do `config.h` entram como parâmetro), para compilar no computador.
  `src/net/` nunca chama `lv_*` nem `src/ui/`/`src/app/`: o que precisa da interface passa por
  hooks registrados pelo `.ino`. As telas leem o modelo por funções e mudam o estado pelas
  intenções do `app_state`.

O `tests/run.sh` falha se aparecer `lv_` em `src/net/`.

## Testes

```bash
firmware/tests/run.sh
```

Compila `claude_stick/src/logic/*.cpp` com `tests/test_*.cpp` (`g++ -std=c++17 -Wall -Wextra
-Werror`), roda e faz o grep contra `lv_` em `src/net/`. Não há CI: rode antes de abrir o PR e cole
a saída nele. Os testes da ponte de tokens ficam em `tools/test_token_bridge.py`
(`python3 -I tools/test_token_bridge.py`).

## Arquivos gerados

Não edite à mão; rode o gerador de novo.

- **`tools/gen_fonts.sh`** → `claude_stick/src/ui/fonts/*.c`. Baixa a JetBrains Mono 2.304 para
  `tools/.cache/fonts/` (confere o sha256), confere os glifos com fontTools num venv em
  `tools/.cache/venv/`, converte com `lv_font_conv@1.5.3` (bpp 4, sem compressão, fallback na
  Montserrat 14 para os `LV_SYMBOL_*`) e, se achar a toolchain e o lvgl, imprime o flash de cada
  fonte. Um glifo fora de ASCII/Latin-1 que falte na TTF é erro, exceto o `⚠` (U+26A0), que só
  avisa e sai das faixas (a JetBrains Mono 2.304 tem o glifo, e as fontes geradas o incluem).
- **`tools/gen_hermes_assets.py`** → `claude_stick/src/ui/assets_thep.{h,c}`: logo THEP em ANSI
  Shadow (104×38) e o pássaro em braille (124×124 e 13×13), em `LV_COLOR_FORMAT_RGB565A8`, a partir
  de `assets/brand/thep-bird.png`. `--preview DIR` grava PNGs de conferência.
- **`tools/gen_mockups.py`** → `assets/mock-*.png` do README, com as mesmas fontes (precisa do
  `gen_fonts.sh` rodado antes) e imagens do firmware. Confere a largura de cada texto da tabela de
  layouts e sai com erro se algum estourar; `--check` só confere.

### Dependências de ferramenta

- Python 3 com **Pillow** (`gen_hermes_assets.py`, `gen_mockups.py`); o `gen_fonts.sh` cria o
  próprio venv com **fontTools**.
- **Node.js** com `npx` (`lv_font_conv@1.5.3`), mais `curl`, `unzip` e `shasum`.
- **`g++`** com C++17 (`tests/run.sh`).
- Para medir o flash das fontes no `gen_fonts.sh` (opcional):
  `~/Library/Arduino15/packages/esp32/tools/esp-x32/<versão>/bin/xtensa-esp32s3-elf-gcc` (a
  toolchain instalada com o core 3.3.8, hoje `2601`) e a lib lvgl em
  `~/Documents/Arduino/libraries/lvgl` (ou `LVGL_DIR`).

## Medições na placa (v3, 2026-11-09)

| Item | Medido | Requisito |
|---|---|---|
| App | ~1,64 MB (v2.3: ~2,05 MB), sem o Clawd e sem as Montserrat não usadas | < 2,8 MB (`build.sh` falha acima) |
| Heap interno livre durante os ciclos | ~166 KB | ≥ 60 KB |
| Maior bloco interno | ~98–106 KB | ≥ 45 KB |
| Pior quadro (`DISP_BANDS=1` + `DISP_ROT_FAST=1`) | 44–46 ms, na troca do relógio do rodapé (`rows_max` 459) | < 40 ms |
| Rotação / envio | < 1 ms / ~37 ms | — |

O pool do LVGL fica na PSRAM. O quadro fica abaixo de 40 ms, exceto os que tocam a borda esquerda da
paisagem (o relógio do rodapé, a cada minuto), porque o envio sempre começa na linha nativa 0 (ver
[`REFERENCIA-HARDWARE-LVGL.md`](REFERENCIA-HARDWARE-LVGL.md), §4). O modo FULL da v2.3 dava
~96–109 ms por quadro.
