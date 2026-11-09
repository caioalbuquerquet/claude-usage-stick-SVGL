#!/usr/bin/env bash
#
# Gera as fontes LVGL da v3 (JetBrains Mono 2.304) em
# firmware/claude_stick/src/ui/fonts/.
#
# Uso:
#   tools/gen_fonts.sh
#
# Pré-requisitos: curl, unzip, shasum, python3 (para o venv do fontTools),
# node/npx (para o lv_font_conv@1.5.3). Opcional: o toolchain ESP32-S3 do
# Arduino15 e a lib lvgl, para medir o flash de cada fonte no fim.
#
# Passos:
#   1. baixa o zip da release 2.304 para tools/.cache/fonts/ e confere o sha256
#      do zip e das duas TTF extraídas;
#   2. confere com fontTools se a TTF tem cada glifo fora de ASCII/Latin-1 das
#      faixas. Falta algum -> erro. Exceção: U+26A0 (⚠) só avisa e sai das
#      faixas (o card em falha mostra "<classe> · <idade>" sem o ícone);
#   3. converte com lv_font_conv (bpp 4, sem compressão). As fontes de texto
#      saem com fallback = &lv_font_montserrat_14 (o campo é const no LVGL 9
#      e não pode ser atribuído em tempo de execução);
#   4. compila cada .c sozinho com o xtensa-esp32s3-elf-gcc e imprime o flash.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE="$ROOT/tools/.cache"
FONT_CACHE="$CACHE/fonts"
VENV="$CACHE/venv"
OUT="$ROOT/firmware/claude_stick/src/ui/fonts"
SKETCH="$ROOT/firmware/claude_stick"

JBM_VERSION="2.304"
JBM_URL="https://github.com/JetBrains/JetBrainsMono/releases/download/v${JBM_VERSION}/JetBrainsMono-${JBM_VERSION}.zip"
JBM_ZIP_SHA256="6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf"
JBM_REGULAR_SHA256="a0bf60ef0f83c5ed4d7a75d45838548b1f6873372dfac88f71804491898d138f"
JBM_BOLD_SHA256="5590990c82e097397517f275f430af4546e1c45cff408bde4255dad142479dcb"

LV_FONT_CONV="lv_font_conv@1.5.3"
FONTTOOLS="fonttools==4.68.0"
FALLBACK="lv_font_montserrat_14"

# Faixas (item 29 do design). TEXT_EXTRA são os code points fora de
# ASCII/Latin-1, conferidos um a um antes de gerar.
TEXT_BASE="0x20-0x7E,0xA0-0xFF"
TEXT_EXTRA="0x2014 0x2022 0x2026 0x2500 0x2502 0x256D 0x256E 0x256F 0x2570 0x2588 0x2591 0x26A0 0x2713 0x2717 0x276F"
NUM_RANGE="0x25,0x2D,0x30-0x39"

sha256_of() { shasum -a 256 "$1" | awk '{print $1}'; }

check_sha() {  # arquivo esperado
  local got
  got="$(sha256_of "$1")"
  if [ "$got" != "$2" ]; then
    echo "ERRO: sha256 de $1 não confere" >&2
    echo "  esperado: $2" >&2
    echo "  obtido:   $got" >&2
    exit 1
  fi
}

# ── 1. Download fixo ────────────────────────────────────────────────────────
mkdir -p "$FONT_CACHE"
ZIP="$FONT_CACHE/JetBrainsMono-${JBM_VERSION}.zip"
if [ ! -f "$ZIP" ] || [ "$(sha256_of "$ZIP")" != "$JBM_ZIP_SHA256" ]; then
  echo "==> baixando JetBrains Mono $JBM_VERSION"
  curl -fsSL -o "$ZIP.part" "$JBM_URL"
  mv "$ZIP.part" "$ZIP"
fi
check_sha "$ZIP" "$JBM_ZIP_SHA256"

REG="$FONT_CACHE/JetBrainsMono-Regular.ttf"
BOLD="$FONT_CACHE/JetBrainsMono-Bold.ttf"
unzip -o -j -q "$ZIP" fonts/ttf/JetBrainsMono-Regular.ttf fonts/ttf/JetBrainsMono-Bold.ttf OFL.txt -d "$FONT_CACHE"
check_sha "$REG" "$JBM_REGULAR_SHA256"
check_sha "$BOLD" "$JBM_BOLD_SHA256"
echo "==> TTF conferidas (sha256)"

# ── 2. Checagem de glifos ───────────────────────────────────────────────────
if [ ! -x "$VENV/bin/python" ]; then
  echo "==> criando venv com $FONTTOOLS"
  python3 -m venv "$VENV"
  "$VENV/bin/pip" install -q --disable-pip-version-check "$FONTTOOLS"
fi

# Imprime as faixas extras que existem nas duas TTF; sai com erro se faltar
# algo além do ⚠.
TEXT_EXTRA_OK="$("$VENV/bin/python" -I - "$REG" "$BOLD" $TEXT_EXTRA <<'PY'
import sys
from fontTools.ttLib import TTFont
fonts = sys.argv[1:3]
cps = [int(x, 16) for x in sys.argv[3:]]
OPTIONAL = {0x26A0}
keep, fatal = [], False
for cp in cps:
    missing = [f.rsplit("/", 1)[-1] for f in fonts if cp not in TTFont(f, lazy=True).getBestCmap()]
    if not missing:
        keep.append(cp)
    elif cp in OPTIONAL:
        print(f"AVISO: U+{cp:04X} falta em {', '.join(missing)}; sai das faixas "
              "(card em falha sem o ícone)", file=sys.stderr)
    else:
        print(f"ERRO: U+{cp:04X} falta em {', '.join(missing)}", file=sys.stderr)
        fatal = True
if fatal:
    sys.exit(1)
print(",".join(f"0x{cp:04X}" for cp in keep))
PY
)"
TEXT_RANGE="$TEXT_BASE,$TEXT_EXTRA_OK"
echo "==> glifos conferidos"

# ── 3. Conversão ────────────────────────────────────────────────────────────
mkdir -p "$OUT"

gen() {  # nome tamanho ttf faixas [fallback]
  local name="$1" size="$2" ttf="$3" range="$4" fb="${5:-}"
  local args=(--bpp 4 --no-compress --format lvgl --size "$size"
              --font "${FONT_CACHE#$ROOT/}/$ttf" -r "$range"
              --lv-font-name "$name" --lv-include lvgl.h
              -o "${OUT#$ROOT/}/$name.c")
  [ -n "$fb" ] && args+=(--lv-fallback "$fb")
  echo "  $name"
  # Roda a partir da raiz, com caminhos relativos, para o "Opts:" do
  # cabeçalho não gravar caminhos absolutos da máquina.
  (cd "$ROOT" && npx --yes "$LV_FONT_CONV" "${args[@]}")
}

echo "==> gerando em ${OUT#$ROOT/}"
gen jbmono_11        11 JetBrainsMono-Regular.ttf "$TEXT_RANGE" "$FALLBACK"
gen jbmono_13        13 JetBrainsMono-Regular.ttf "$TEXT_RANGE" "$FALLBACK"
gen jbmono_bold_13   13 JetBrainsMono-Bold.ttf    "$TEXT_RANGE" "$FALLBACK"
for n in 20 30 38 50; do
  gen "jbmono_bold_num_$n" "$n" JetBrainsMono-Bold.ttf "$NUM_RANGE"
done

# ── 4. Flash real de cada fonte ─────────────────────────────────────────────
GCC="$(ls -d "$HOME"/Library/Arduino15/packages/esp32/tools/esp-x32/*/bin/xtensa-esp32s3-elf-gcc 2>/dev/null | tail -1 || true)"
LVGL="${LVGL_DIR:-$HOME/Documents/Arduino/libraries/lvgl}"
if [ -z "$GCC" ] || [ ! -d "$LVGL/src" ]; then
  echo "==> toolchain ESP32-S3 ou lvgl não encontrados; medição de flash pulada"
  exit 0
fi
SIZE="${GCC%gcc}size"
OBJ="$(mktemp -d)"
trap 'rm -rf "$OBJ"' EXIT
echo "==> flash por fonte (xtensa-esp32s3-elf-gcc -Os)"
total=0
for c in "$OUT"/*.c; do
  o="$OBJ/$(basename "${c%.c}").o"
  "$GCC" -Os -ffunction-sections -fdata-sections -c -DLV_CONF_INCLUDE_SIMPLE -I"$SKETCH" -I"$SKETCH/src" \
    -I"$LVGL/src" -I"$LVGL" "$c" -o "$o"
  bytes="$("$SIZE" -A "$o" | awk '$1 ~ /^\.(text|rodata|data)/ {s += $2} END {print s+0}')"
  total=$((total + bytes))
  printf '  %-20s %7d bytes\n' "$(basename "$c")" "$bytes"
done
printf '  %-20s %7d bytes\n' "total" "$total"
