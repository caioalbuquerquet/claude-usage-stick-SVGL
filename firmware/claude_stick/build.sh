#!/usr/bin/env bash
#
# Build / upload / monitor do Claude Usage Stick (Guition JC4832W535, ESP32-S3).
#
# Uso:
#   ./build.sh                 # compila
#   ./build.sh upload          # compila + grava (porta padrão abaixo)
#   ./build.sh upload <porta>  # compila + grava na porta indicada
#   ./build.sh dev             # compila com -DTHEP_DEV (rotas e logs de diagnóstico)
#   ./build.sh upload-dev [porta] # idem dev + grava
#   ./build.sh monitor <porta> # abre o serial monitor (115200)
#   THEP_FLAGS="-D..." ./build.sh ... # flags extras (ex.: -DDISP_BANDS=0 volta ao modo FULL)
#
# Pré-requisitos (ver firmware/REFERENCIA-HARDWARE-LVGL.md):
#   - arduino-cli 1.4.x, core esp32:esp32 3.3.8 (versão em que a v3 foi validada)
#   - libs: GFX Library for Arduino 1.6.5, lvgl 9.2.2
#
# O -DLV_CONF_INCLUDE_SIMPLE + -I<sketch> faz o LVGL achar o nosso lv_conf.h.
# O -I<sketch>/src deixa os módulos se incluírem pelo caminho a partir de src/
# (ex.: "model/history.h", "net/api.h").
# Fallback, se der "lv_conf.h not found": copie lv_conf.h para a pasta de
# libraries do Arduino (um nível acima da pasta `lvgl`).
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio"
PORT_DEFAULT="/dev/cu.usbmodem101"

LVFLAGS="-DLV_CONF_INCLUDE_SIMPLE -I${SKETCH_DIR} -I${SKETCH_DIR}/src"
OUT_DIR="$SKETCH_DIR/build"
BIN="$OUT_DIR/claude_stick.ino.bin"
MAX_APP_BYTES=2936012   # 2,8 MB: teto da v3 num slot de app de 3 MB

cmd="${1:-build}"
port="${2:-$PORT_DEFAULT}"

case "$cmd" in
  dev|upload-dev) LVFLAGS="$LVFLAGS -DTHEP_DEV" ;;
esac
# Flags extras de teste, ex.: THEP_FLAGS="-DDISP_BANDS=1 -DDISP_ROT_FAST=1" ./build.sh upload-dev
if [ -n "${THEP_FLAGS:-}" ]; then LVFLAGS="$LVFLAGS $THEP_FLAGS"; fi

# compile [args extras do arduino-cli...]
compile() {
  arduino-cli compile \
    --fqbn "$FQBN" \
    --build-property "compiler.cpp.extra_flags=$LVFLAGS" \
    --build-property "compiler.c.extra_flags=$LVFLAGS" \
    --build-property "compiler.S.extra_flags=$LVFLAGS" \
    --output-dir "$OUT_DIR" \
    "$@" \
    "$SKETCH_DIR"
}

check_size() {
  local size
  size=$(wc -c < "$BIN")
  size=$((size + 0))
  echo "==> app: $size bytes (teto $MAX_APP_BYTES)"
  if [ "$size" -gt "$MAX_APP_BYTES" ]; then
    echo "ERRO: o app passou do teto de $MAX_APP_BYTES bytes (2,8 MB)" >&2
    exit 1
  fi
}

case "$cmd" in
  monitor)
    exec arduino-cli monitor -p "$port" -c baudrate=115200
    ;;
  build|dev)
    echo "==> compilando ($cmd, $FQBN)"
    compile
    check_size
    ;;
  upload|upload-dev)
    # `compile --upload` compila e grava num passo só (upload puro não aceita --build-property)
    echo "==> compilando + gravando em $port ($cmd, $FQBN)"
    compile --upload -p "$port"
    check_size
    ;;
  *)
    echo "comando desconhecido: $cmd (use: build | upload | dev | upload-dev | monitor)" >&2
    exit 1
    ;;
esac
