# Referência de Hardware & LVGL — Gadget Valorant

Extraído do projeto que já funciona (`../esp32_controller/`). **Esta é a configuração de tela testada e validada** — usar como base do firmware do gadget para não repetir a dor de acertar as cores.

---

## 0. ⭐ TL;DR — o que faz as cores funcionarem

A placa tem display **AXS15231B por QSPI**. O que dá **cores corretas** (depois de muita tentativa) é:

1. `Arduino_Canvas` (framebuffer em RAM/PSRAM) criado com **`rotation = 0`** — comentário no código original: *"Canvas rotation=0 (perfect colors)"*.
2. A **rotação para paisagem é feita manualmente no flush** (gira 270° CW, USB à esquerda), copiando os pixels do buffer do LVGL para o framebuffer do Canvas.
3. O push pro painel é `gfx->flush()` (empurra o Canvas inteiro via QSPI).

> ❌ **NÃO** usar a abordagem do `display.h.bak` (Canvas `rotation=1` + `draw16bitRGBBitmap` no flush): foi a que dava **cores erradas**. A versão correta é a do `esp32_controller.ino`.

Por que funciona: o `Arduino_AXS15231B` espera o framebuffer no formato nativo (retrato 320×480) sem rotação interna; rotacionar via Canvas embaralhava a ordem de bytes/cor. Mantendo o Canvas em `rotation=0` e rotacionando "na mão" no flush, os pixels RGB565 chegam na ordem certa.

---

## 1. Placa & display

| | |
|---|---|
| Placa | **Guition JC4832W535** (ESP32‑S3) |
| Flash / PSRAM | 16 MB flash · **8 MB PSRAM OPI** (obrigatória) |
| Display | **AXS15231B**, interface **QSPI**, **480×320** paisagem (nativo 320×480 retrato) |
| Touch | **AXS15231B capacitivo**, I²C, addr `0x3B` |
| USB | CDC nativo (Serial pela própria USB) |

## 2. Pinos (`config.h`)

```
// Display QSPI (AXS15231B)
TFT_CS=45  TFT_SCK=47  TFT_SDA0=21  TFT_SDA1=48  TFT_SDA2=40  TFT_SDA3=39
TFT_BL=1 (backlight, HIGH=on)   TFT_TE=38
QSPI_FREQ = 40 MHz

// Touch I2C (AXS15231B)
TOUCH_SDA=4  TOUCH_SCL=8  TOUCH_INT=3  TOUCH_ADDR=0x3B

// SD Card SPI (provavelmente não usaremos — temos LittleFS)
SD_CS=10  SD_MOSI=11  SD_SCLK=12  SD_MISO=13

SCREEN_WIDTH=480  SCREEN_HEIGHT=320
```

## 3. Bibliotecas (versões testadas)

| Lib | Versão | Uso |
|---|---|---|
| `esp32` (core) | **3.3.8** | toolchain ESP32‑S3 (a v3 foi validada nesta versão; as APIs de timeout do `HTTPClient` foram conferidas nela) |
| `GFX Library for Arduino` (Arduino_GFX) | **1.6.5** | driver QSPI + Canvas (`Arduino_ESP32QSPI`, `Arduino_AXS15231B`, `Arduino_Canvas`) |
| `lvgl` | **9.2.2** | UI (API v9: `lv_display_create`, etc.) |
| `ArduinoJson` | **7.2.0** | parse do payload do backend |
| `LittleFS`, `WiFi`, `Preferences` | builtin (core) | assets / rede / config NVS |

> Tem `LovyanGFX` instalada também, mas **não é usada** nesta placa — é Arduino_GFX.

---

## 4. ⭐ Inicialização do display (código que funciona)

```cpp
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

Arduino_Canvas *gfx = nullptr;
static uint16_t *canvas_fb = nullptr;   // framebuffer do Canvas (320x480)

// --- setup() ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);   // ⭐ rotation = 0

if (!gfx->begin(40000000UL)) { /* FATAL */ }
gfx->fillScreen(0x0000);
gfx->flush();
canvas_fb = gfx->getFramebuffer();                 // ⭐ acesso direto ao FB

pinMode(TFT_BL, OUTPUT);
digitalWrite(TFT_BL, HIGH);                         // backlight ON
```

### Flush callback (rotação manual 270° CW — USB à esquerda)

```cpp
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t *src = (uint16_t *)px_map;
    for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {        // 320
        uint16_t *src_row = src + ly * SCREEN_WIDTH;    // 480
        for (int lx = 0; lx < SCREEN_WIDTH; lx++) {
            canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];  // gira 270° CW
        }
    }
    gfx->flush();
    lv_disp_flush_ready(disp);
}
```

> Esse flush usa **render mode FULL** (o LVGL entrega a tela inteira de uma vez). Por isso o buffer do LVGL é do tamanho da tela toda (480×320) e mora na **PSRAM**.

### ⭐ v3: render parcial em faixas de linhas nativas (padrão desde 2026-11-09)

O flush acima (FULL + rotação pixel a pixel) custa **~96–109 ms por quadro** no firmware: qualquer invalidação, até o ponto pulsante do cabeçalho, gira e envia os 480×320. O firmware v3 usa, por padrão, `DISP_BANDS=1` + `DISP_ROT_FAST=1` (`claude_stick/config.h`, `disp_flush_cb` no `.ino`):

- **Render `LV_DISPLAY_RENDER_MODE_PARTIAL`**, com o buffer do LVGL ainda do tamanho da tela (na PSRAM), para cada área invalidada sair inteira de uma vez.
- **Rotação em blocos 16×16** (`src/logic/rot270`, gravação de 32 bits): cada área é girada para o framebuffer do Canvas, que guarda o resto da imagem. Mesma imagem da rotação pixel a pixel, **< 1 ms** por quadro.
- **Envio de linhas nativas inteiras a partir da linha 0.** O pixel de paisagem `(lx, ly)` vai para a linha nativa `479 − lx`, então uma área `[x1..x2]` toca as linhas nativas `[479−x2 .. 479−x1]`; a borda **direita** da paisagem é a linha nativa 0. No fim do quadro vai ao painel o prefixo `0 .. r1` (`draw16bitRGBBitmap(0, 0, …, 320, r1+1)`), com `r1 = 479 − (menor x1 do quadro)`.

**Por que um prefixo e não a faixa exata:** o AXS15231B em QSPI **ignora a janela de linhas (RASET)**. O primeiro modo parcial mandava `CASET 0..319` + `RASET r0..r1` e o conteúdo saiu deslocado na horizontal (na paisagem, linha nativa errada): o `RAMWR` recomeça na linha 0. É o que o driver da Espressif (`esp_lcd_axs15231b`) assume: em QSPI ele nem manda RASET, usa `RAMWR` (0x2C) para a faixa que começa na linha 0 e `RAMWRC` (0x3C) para as seguintes, em ordem. `DISP_BANDS=2` repete o envio com RASET, só como experimento.

**Medido na placa** (`[HEAP]` e `/health`): pior quadro **44–46 ms**, com `rows_max` 459 (a troca do relógio do rodapé, na borda esquerda da paisagem, obriga a mandar quase a tela toda); rotação < 1 ms; envio ~37 ms. Um degrau do pulso do ponto (x ≈ 368–378) manda só ~112 linhas, e o pulso ficou no ciclo aprovado: 4 degraus (100/70/40/70%), um a cada 400 ms (`DOT_PULSE_STEP_MS`, ciclo de 1,6 s). O requisito de quadro < 40 ms vale para tudo, exceto os quadros que tocam a borda esquerda.

**Rotação pelo painel não existe:** o AXS15231B em QSPI só espelha (MADCTL MX/MY); trocar eixos (MV) não funciona nele, por isso a rotação continua no software. Para voltar ao modo FULL: `THEP_FLAGS="-DDISP_BANDS=0" ./build.sh upload-dev`.

---

## 5. LVGL 9 — setup

```cpp
lv_init();
lv_tick_set_cb([]() -> uint32_t { return millis(); });   // tick por millis()

uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t); // 480*320*2
lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
    bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);       // ⭐ PSRAM

lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT); // 480x320
lv_display_set_flush_cb(disp, disp_flush_cb);
lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_FULL);

lv_indev_t *indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev, touch_read_cb);
```

No **loop**: `lv_task_handler();` + `delay(16);` (~60 fps). Atualizar telas com dados ao vivo em intervalo (ex. a cada 1 s).

### `lv_conf.h` (pontos críticos)
- `LV_COLOR_DEPTH 16` (RGB565).
- Fontes: na v3 só a **Montserrat 14** fica ligada, como fallback das JetBrains Mono geradas (ícones `LV_SYMBOL_*`, como o `↻`). As outras Montserrat foram desligadas.
- `LV_USE_OS 0` (loop cooperativo) e tick via `lv_tick_set_cb` (acima).
- PSRAM: o buffer de tela é alocado manualmente. Na v3 o pool do LVGL (`LV_MEM_SIZE`, 96 KB) também vai para a PSRAM, por `LV_MEM_POOL_INCLUDE "hw/lv_pool.h"` + `LV_MEM_POOL_ALLOC(size)`, para sobrar RAM interna para as sessões TLS (~40–50 KB cada). Medido: ~166 KB de heap interno livre durante os ciclos, maior bloco ~98–106 KB.
- `LV_USE_LOG` ligado só na build `THEP_DEV` (callback que escreve na `Serial`).

---

## 6. Touch (AXS15231B, I²C) — `touch.h`

- Driver custom (classe `AXS15231B_Touch`), I²C 400 kHz, INT em `FALLING`.
- Comando de leitura: `{0xB5,0xAB,0xA5,0x5A,0x00,0x00,0x00,0x08}` → lê 8 bytes; X/Y nos bytes 2–5.
- Instanciar com **rotation = 3** para a orientação "USB à esquerda":
  ```cpp
  AXS15231B_Touch touch_dev(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR, 3);
  ```
- Mapeamento (nativo retrato 320×480):
  ```
  rot 1 (paisagem):  x = raw_y;        y = 319 - raw_x
  rot 3 (USB-esq.):  x = 479 - raw_y;  y = raw_x
  ```

## 7. ⚠️ Orientação — display e touch TÊM que casar

USB à esquerda → **display gira 270° CW no flush** (`479 - lx`) **e** **touch rotation = 3**. Se inverter um sem o outro, o toque fica espelhado/trocado. Mantê-los em par.

---

## 8. WiFi — reaproveitar `wifi_manager.h` (quase como está)

Classe `WiFiManager` pronta, guarda até **3 redes na NVS** (`Preferences` namespace `"wifi"`):

```cpp
g_wifi.begin();                       // carrega redes salvas (NVS)
bool ok = g_wifi.autoConnect(5000);   // tenta as salvas, 5 s cada
// se falhar → tela de config:
int n = g_wifi.scanNetworks(results, max);          // lista redes
g_wifi.connectTo(ssid, pass, 15000);  // conecta E salva na NVS (promove p/ topo)
g_wifi.getIP(); g_wifi.getSSID(); g_wifi.getSavedSSID();
```

**Fluxo no firmware do gadget:** no boot, `begin()` → `autoConnect()`. Se não conectar, abrir tela de WiFi (scan + teclado LVGL para a senha) → `connectTo()`. Reaproveitar `ui_wifi.cpp` como base da tela.

## 9. Cliente HTTP — adaptar `api_client.h`

Padrão da referência (manter): `HTTPClient` + `WiFiClientSecure` com `setInsecure()` para **HTTPS**, parse com `ArduinoJson`.

**Diferença para o nosso gadget:** não há login email/senha+JWT. O gadget só faz:
```cpp
GET https://<backend>/api/v1/gadget/dashboard
Header: Authorization: Bearer <DEVICE_TOKEN>   // o código curto tipo "SV76-7KX2"
```
e dá parse no payload enxuto (ver `../docs/03-CONTRATO-API.md`). O device token é digitado uma vez no gadget e salvo na **NVS** (mesmo padrão `Preferences` do WiFi).

---

## 10. Build & upload (arduino-cli)

- **arduino-cli 1.4.1**, core **esp32:esp32 3.3.8** já instalados.
- **Placa conectada:** `/dev/cu.usbmodem101`.
- `partitions.csv` (16 MB): `nvs`, `otadata`, `app0` + `app1` (3 MB cada; não há OTA, o firmware roda sempre do `app0`), `spiffs` ~9,9 MB (o LittleFS monta essa partição) e `coredump`. Não mudar: qualquer mudança apaga os dados no próximo flash.

FQBN recomendado (ESP32‑S3 + **PSRAM OPI** + 16 MB + partição custom + **USB CDC**):
```bash
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio"

arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload  --fqbn "$FQBN" -p /dev/cu.usbmodem101 .
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```
> A `PartitionScheme=custom` usa o `partitions.csv` da pasta do sketch. **PSRAM=opi é obrigatório** (o framebuffer 480×320×2 ≈ 300 KB + buffer LVGL não cabem na RAM interna). Confirmar os nomes exatos das opções do FQBN com `arduino-cli board details --fqbn esp32:esp32:esp32s3` na hora de compilar.

---

## 11. Sequência sugerida de implementação do firmware (Valorant)

> Ordem pensada para validar o difícil (tela/cor/touch) cedo e ir somando.

- **Fase 0 — Esqueleto:** estrutura do sketch em `firmware/`, copiar `config.h` (pinos), `touch.h`, `wifi_manager.h` da referência; `lv_conf.h` com depth 16 + fontes.
- **Fase 1 — Bring-up (CRÍTICO):** ✅ **FEITO e validado no hardware** — sketch em `firmware/bringup/` (cores, orientação USB-esquerda e touch 100% OK). É a base conhecida-boa pra copiar a config de display/touch.
- **Fase 2 — Base LVGL + tema Valorant:** estilos (cores `#0b0b18`/`#7c3aed`/`#f43f5e`), helpers de label/painel/botão (espelhar `ui_theme.h`).
- **Fase 3 — WiFi:** `autoConnect` + tela de config (scan + teclado) reusando `wifi_manager.h`/`ui_wifi.cpp`.
- **Fase 4 — Device token:** tela de onboarding no gadget (teclado LVGL) → salva o código curto na NVS.
- **Fase 5 — API client:** `GET /gadget/dashboard` com o token → `ArduinoJson` parse do payload.
- **Fase 6 — Tela Dashboard:** elo (imagem por `rank.id`), `lv_bar` de RR, tendência. Com dados reais.
- **Fase 7 — Navegação + telas 2–4:** `lv_tabview` por swipe → Histórico, Gráfico (`lv_chart`), Status.
- **Fase 8 — Assets + estados:** ícones de elo/agente na LittleFS (por id); loading / erro / "desatualizado" (stale).
- **Fase 9 — Polling + extras:** atualizar a cada `poll` do payload; LED WS2812B opcional (cor do elo / pós-jogo).

## 12. Adaptações vs. a referência (roaster)

| Referência (esp32_controller) | Gadget Valorant |
|---|---|
| BLE (`ble_roaster.h`) | ❌ remover (não usa) |
| SD Card (`sd_storage.h`) | usar **LittleFS** (assets) + **NVS** (config) |
| Auth: email/senha → JWT | **device token** (Bearer), sem login |
| Tema laranja industrial | **violeta/vermelho** (paleta Valorant) |
| Telas de roast/perfis | Dashboard / Histórico / Gráfico / Status |

## 13. Armadilhas (lembrar)

- 🔴 **PSRAM OPI obrigatória** — sem ela, o `heap_caps_malloc` do framebuffer falha e a placa trava no boot.
- 🔴 **Usar o esquema do `.ino`** (Canvas rot=0 + rotação manual no flush), **não** o do `.bak` — senão cores erradas.
- 🔴 **USB CDC On Boot** habilitado, senão o `Serial` não aparece na USB nativa.
- 🟡 Display e touch precisam ter a **mesma orientação** (270°/rot=3 para USB à esquerda).
- 🟡 LVGL **9.2.x** — API nova (`lv_display_create`, `lv_tick_set_cb`); exemplos antigos (v8) não compilam.

## 14. Truques e pegadinhas LVGL 9.2 (aprendidos no redesign v2.x)

> Descobertos na prática durante o redesign de 2026-07 (medidor segmentado, sprites
> oficiais, animações de limiar, i18n). Valem para qualquer firmware desta placa.

- 🔴 **Fontes Montserrat embutidas só têm ASCII + `°` (0xB0) + bullet `•` (0x2022).**
  Nada de acentos nem travessão (—) — renderizam como glifo faltante. A v2.x escrevia a UI
  sem acento por isso. A v3 resolve com fontes geradas: JetBrains Mono 2.304 com Latin-1,
  box drawing, `█ ░` e `❯ ✓ ✗ ⚠ …` (`tools/gen_fonts.sh`, `lv_font_conv` bpp 4), com a
  Montserrat 14 só como fallback. O fallback é um campo `const` no LVGL 9: entra na geração
  (`--lv-fallback`), não em tempo de execução.
- 🔴 **Protótipos automáticos do `.ino` quebram com tipos próprios**: uma função que
  retorna `MeuStruct*` ganha protótipo gerado NO TOPO do arquivo, antes da definição do
  struct → erro `'MeuStruct' does not name a type`. Solução: retornar índice `int`
  (ver `day_slot()` no sketch) ou usar só tipos já conhecidos (`lv_obj_t*` etc.).
- 🟢 **Imagens embutidas**: `lv_image_dsc_t` na 9.2 aceita init posicional
  `{{LV_IMAGE_HEADER_MAGIC, cf, 0, w, h, stride, 0}, size, data}` (ordem little-endian
  verificada em `src/draw/lv_image_dsc.h`). A v2.x usava ARGB8888 (bytes **B,G,R,A**,
  gerados pelo `gen_logo_assets.py`, removido na v3). A v3 usa
  **`LV_COLOR_FORMAT_RGB565A8`**: o plano RGB565 inteiro e depois o plano de alfa (1 byte
  por pixel), ~3 bytes por pixel em vez de 4. Pipeline: `tools/gen_hermes_assets.py`
  (só Pillow) gera o logo THEP e o pássaro em braille → `src/ui/assets_thep.{h,c}`.
- 🟢 **`lv_obj_set_style_image_recolor(+_opa)`** tinge a imagem em runtime — usado para
  deixar o pássaro cinza no alerta de incidente (e, na v2.x, o Clawd em erro) sem gerar um
  segundo asset.
- 🟡 **Animações procedurais no `loop()` > `lv_anim`** para overlays que podem morrer a
  qualquer momento (toque fecha): `lv_anim` com `exec_cb` custom apontando para objetos
  deletados = crash. Padrão do firmware: guardar ponteiros num struct, animar por tempo
  (`millis()`) e deletar tudo junto (`lv_obj_delete(scrim)`).
- 🟡 **Overlays devem viver em `lv_layer_top()`** (sobrevivem a updates da tela), mas
  precisam ser limpos manualmente em cada troca de estado (`lv_obj_clean(lv_layer_top())`
  no `render_state()`), senão vazam entre telas.
- 🟡 **Alvo de toque pequeno**: além de aumentar o botão, usar
  `lv_obj_set_ext_click_area(btn, px)` — a engrenagem do header usa 58×40 + 12px extras.
- 🟢 **i18n barato**: macro `#define TRS(pt, en) (g_lang ? (en) : (pt))` inline em cada
  string (sem tabela paralela para desalinhar). Trocar idioma = salvar NVS +
  `request_state()` para reconstruir a tela atual.
- 🟢 **Olhos do Clawd oficial são buracos transparentes no path** — sobre fundo escuro já
  parecem olhos; expressões viram overlays (pálpebras/X) posicionados pelos defines
  `CLAWD_*_EYE*` que o gerador de assets emite.
