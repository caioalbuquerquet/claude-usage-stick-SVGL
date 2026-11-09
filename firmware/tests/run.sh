#!/usr/bin/env bash
# Testes de host do firmware: compila src/logic/*.cpp com tests/test_*.cpp
# (g++ C++17), roda, e checa que o código de rede não chama o LVGL.
# Uso: firmware/tests/run.sh  (de qualquer diretório)
set -euo pipefail

FW="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$FW/claude_stick/src"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT

fail=0

echo "== compilando"
g++ -std=c++17 -Wall -Wextra -Werror -I "$SRC" -I "$FW/tests" \
    "$SRC"/logic/*.cpp "$FW"/tests/test_*.cpp \
    -o "$OUT/tests"

echo "== rodando"
"$OUT/tests" || fail=1

echo "== grep: lv_ em src/net/"
if grep -rn 'lv_' "$SRC/net/"; then
    echo "FALHA: src/net/ não pode chamar o LVGL"
    fail=1
else
    echo "ok"
fi

exit $fail
