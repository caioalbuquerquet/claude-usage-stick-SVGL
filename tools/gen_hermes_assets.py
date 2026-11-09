#!/usr/bin/env python3
"""
gen_hermes_assets.py — gera as imagens da marca THEP no estilo Hermes
(logo em ANSI Shadow e pássaro em braille) como imagens LVGL embutidas.

Saída (arquivos gerados, não editar na mão):
  firmware/claude_stick/src/ui/assets_thep.h  — LV_IMAGE_DECLARE de cada imagem
  firmware/claude_stick/src/ui/assets_thep.c  — dados e lv_image_dsc_t

Imagens (formato LV_COLOR_FORMAT_RGB565A8, LVGL 9.2):
  img_thep_logo_hdr     104x38   logo THEP do cabeçalho
  img_bird_braille_124  124x124  pássaro em braille (1 conta, limiar, incidente)
  img_bird_braille_13   13x13    pássaro em braille do rodapé

Entrada: assets/brand/thep-bird.png (PNG 1024x1024 com transparência).

Requisitos: Python 3 e Pillow (pip install Pillow). Nada mais.

Uso:
  python3 tools/gen_hermes_assets.py                 # gera os .h/.c
  python3 tools/gen_hermes_assets.py --preview DIR   # idem + PNGs de conferência

As funções logo_hdr(), bird_braille_124() e bird_braille_13() devolvem as
imagens RGBA (Pillow) e podem ser importadas por outros scripts (gen_mockups).
"""
import argparse
import os

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIRD_PNG = os.path.join(ROOT, "assets", "brand", "thep-bird.png")
UI_DIR = os.path.join(ROOT, "firmware", "claude_stick", "src", "ui")
OUT_H = os.path.join(UI_DIR, "assets_thep.h")
OUT_C = os.path.join(UI_DIR, "assets_thep.c")

BG_PREVIEW = (0x0C, 0x12, 0x22)  # color-bg (dark), só para os PNGs de conferência

# ---------------------------------------------------------------------------
# Logo do cabeçalho: THEP em ANSI Shadow (mesma fonte figlet do logo do Hermes)
# ---------------------------------------------------------------------------
THEP = [
    "████████╗██╗  ██╗███████╗██████╗ ",
    "╚══██╔══╝██║  ██║██╔════╝██╔══██╗",
    "   ██║   ███████║█████╗  ██████╔╝",
    "   ██║   ██╔══██║██╔══╝  ██╔═══╝ ",
    "   ██║   ██║  ██║███████╗██║     ",
    "   ╚═╝   ╚═╝  ╚═╝╚══════╝╚═╝     ",
]
# 3 faixas, 2 linhas cada: thep-cyan, color-primary (dark), thep-blue
BANDS = ["#2fb5dd", "#2fb5dd", "#2f8fd6", "#2f8fd6", "#1272c2", "#1272c2"]

LOGO_W, LOGO_H = 104, 38
SS = 8                                   # desenho em 8x, reduzido no fim
STROKE = SS                              # traço de 1 px na escala final


def _hex(c: str) -> tuple:
    return tuple(int(c[i:i + 2], 16) for i in (1, 3, 5))


def logo_hdr() -> Image.Image:
    """Desenha o THEP por células (sem fonte) em 8x e reduz para 104x38."""
    rows, cols = len(THEP), len(THEP[0])
    W, H = LOGO_W * SS, LOGO_H * SS
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    # bordas das células arredondadas na escala 8x: cobrem a imagem sem buraco
    xs = [round(c * W / cols) for c in range(cols + 1)]
    ys = [round(r * H / rows) for r in range(rows + 1)]
    half = STROKE // 2
    for r, line in enumerate(THEP):
        col = _hex(BANDS[r]) + (255,)
        for c, ch in enumerate(line):
            x0, x1, y0, y1 = xs[c], xs[c + 1], ys[r], ys[r + 1]
            cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
            # retângulos semiabertos [a, b) -> rectangle() usa canto final inclusivo
            def box(a, b, c_, d_):
                if b > a and d_ > c_:
                    d.rectangle((a, c_, b - 1, d_ - 1), fill=col)
            hl, hr = cx - half, cx + half          # traço vertical (x)
            vt, vb = cy - half, cy + half          # traço horizontal (y)
            if ch == "█":
                box(x0, x1, y0, y1)
            elif ch == "═":
                box(x0, x1, vt, vb)
            elif ch == "║":
                box(hl, hr, y0, y1)
            elif ch == "╔":                        # direita + baixo
                box(hl, x1, vt, vb)
                box(hl, hr, vt, y1)
            elif ch == "╗":                        # esquerda + baixo
                box(x0, hr, vt, vb)
                box(hl, hr, vt, y1)
            elif ch == "╚":                        # direita + cima
                box(hl, x1, vt, vb)
                box(hl, hr, y0, vb)
            elif ch == "╝":                        # esquerda + cima
                box(x0, hr, vt, vb)
                box(hl, hr, y0, vb)
            elif ch != " ":
                raise ValueError(f"caractere sem desenho: {ch!r}")
    out = im.resize((LOGO_W, LOGO_H), Image.LANCZOS)
    # O LANCZOS tem lóbulos negativos: corrige o "ringing" da redução.
    #  - alfa < 8 vira 0 (pontinhos invisíveis fora das letras) e > 247 vira 255;
    #  - cada canal RGB fica dentro do intervalo das 3 cores das faixas (sem
    #    halo mais claro nem cor fora da paleta nas bordas).
    cols = [_hex(c) for c in BANDS]
    lo = [min(c[k] for c in cols) for k in range(3)]
    hi = [max(c[k] for c in cols) for k in range(3)]
    px = bytearray(out.tobytes())
    for k in range(0, len(px), 4):
        a = px[k + 3]
        a = 0 if a < 8 else 255 if a > 247 else a
        px[k + 3] = a
        for ch in range(3):
            px[k + ch] = 0 if a == 0 else min(hi[ch], max(lo[ch], px[k + ch]))
    return Image.frombytes("RGBA", out.size, bytes(px))


# ---------------------------------------------------------------------------
# Pássaro em braille: grade de pontos com a cor do logo
# ---------------------------------------------------------------------------
def _bird_grid(n: int) -> list:
    """Reduz o PNG para n x n com Image.BOX. Devolve [(i, j, (r, g, b))] dos
    pontos acesos (alfa >= 128). A cor é a média RGB do bloco de origem
    (o Pillow faz a média ponderada pelo alfa ao reduzir RGBA)."""
    src = Image.open(BIRD_PNG).convert("RGBA")
    small = src.resize((n, n), Image.BOX)
    dots = []
    for j in range(n):
        for i in range(n):
            r, g, b, a = small.getpixel((i, j))
            if a >= 128:
                dots.append((i, j, (r, g, b)))
    return dots


def bird_braille_124() -> Image.Image:
    """124x124: 40x40 pontos, passo 3 px, 2 px de margem. Cada ponto é o
    círculo de 2 px de diâmetro rasterizado sem antisserrilhado, ou seja,
    2x2 px opacos (com borda suave ele cairia para ~78% de alfa e apagaria
    as cores). Ponto (i, j) em x = 2 + 3i, y = 2 + 3j."""
    n, step, margin, dot, size = 40, 3, 2, 2, 124
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i, j, rgb in _bird_grid(n):
        x, y = margin + i * step, margin + j * step
        d.rectangle((x, y, x + dot - 1, y + dot - 1), fill=rgb + (255,))
    return im


def bird_braille_13() -> Image.Image:
    """13x13: 6x6 pontos de 2x2 px, passo 2 px, de (0,0) a (11,11). A sobra de
    1 px à direita e embaixo fica transparente."""
    n, step, dot, size = 6, 2, 2, 13
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    for i, j, rgb in _bird_grid(n):
        x, y = i * step, j * step
        d.rectangle((x, y, x + dot - 1, y + dot - 1), fill=rgb + (255,))
    return im


# ---------------------------------------------------------------------------
# Saída LVGL: RGB565A8 = w*h pixels RGB565 little-endian + w*h bytes de alfa
# ---------------------------------------------------------------------------
def to_rgb565a8(im: Image.Image) -> bytes:
    im = im.convert("RGBA")
    px = im.tobytes()
    color, alpha = bytearray(), bytearray()
    for k in range(0, len(px), 4):
        r, g, b, a = px[k:k + 4]
        if a == 0:
            r = g = b = 0
        v = ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | ((b * 31 + 127) // 255)
        color += bytes((v & 0xFF, v >> 8))
        alpha.append(a)
    return bytes(color + alpha)


def from_rgb565a8(data: bytes, w: int, h: int) -> Image.Image:
    """Decodifica de volta (para os PNGs de conferência)."""
    px = bytearray()
    for k in range(w * h):
        v = data[2 * k] | data[2 * k + 1] << 8
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        px += bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31,
                     data[2 * w * h + k]))
    return Image.frombytes("RGBA", (w, h), bytes(px))


def to_c(name: str, w: int, h: int, data: bytes) -> str:
    assert len(data) == w * h * 3
    rows = [",".join(f"0x{v:02x}" for v in data[i:i + 24]) for i in range(0, len(data), 24)]
    body = ",\n    ".join(rows)
    return (
        f"/* {name}: {w}x{h}, RGB565A8, {len(data)} bytes */\n"
        f"static const uint8_t {name}_map[] __attribute__((aligned(4))) = {{\n"
        f"    {body}\n"
        f"}};\n\n"
        f"const lv_image_dsc_t {name} = {{\n"
        f"    .header = {{\n"
        f"        .magic = LV_IMAGE_HEADER_MAGIC,\n"
        f"        .cf = LV_COLOR_FORMAT_RGB565A8,\n"
        f"        .flags = 0,\n"
        f"        .w = {w},\n"
        f"        .h = {h},\n"
        f"        .stride = {w * 2},\n"
        f"        .reserved_2 = 0,\n"
        f"    }},\n"
        f"    .data_size = sizeof({name}_map),\n"
        f"    .data = {name}_map,\n"
        f"    .reserved = NULL,\n"
        f"}};\n"
    )


IMAGES = [
    ("img_thep_logo_hdr", logo_hdr, "logo THEP do cabeçalho"),
    ("img_bird_braille_124", bird_braille_124,
     "pássaro em braille: 1 conta, limiar e incidente (recolor cinza)"),
    ("img_bird_braille_13", bird_braille_13, "pássaro em braille do rodapé"),
]

HEADER_NOTE = "GERADO por tools/gen_hermes_assets.py — não editar na mão."


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--preview", metavar="DIR",
                    help="grava PNGs de conferência (decodificados do RGB565A8)")
    args = ap.parse_args()

    h_parts = [f"// {HEADER_NOTE}", "#pragma once", "#include <lvgl.h>", "",
               "#ifdef __cplusplus", 'extern "C" {', "#endif", ""]
    c_parts = [f"/* {HEADER_NOTE} */", '#include "assets_thep.h"', ""]
    for name, fn, desc in IMAGES:
        im = fn()
        w, h = im.size
        data = to_rgb565a8(im)
        h_parts.append(f"// {desc} ({w}x{h}, RGB565A8)")
        h_parts.append(f"LV_IMAGE_DECLARE({name});")
        c_parts.append(to_c(name, w, h, data))
        print(f"{name}: {w}x{h}, {len(data)} bytes")
        if args.preview:
            os.makedirs(args.preview, exist_ok=True)
            dec = from_rgb565a8(data, w, h)
            dec.save(os.path.join(args.preview, f"{name}.png"))
            bg = Image.new("RGBA", (w, h), BG_PREVIEW + (255,))
            bg.alpha_composite(dec)
            k = 8 if max(w, h) < 200 else 4
            bg.resize((w * k, h * k), Image.NEAREST).save(
                os.path.join(args.preview, f"{name}_x{k}.png"))
    h_parts += ["", "#ifdef __cplusplus", "}", "#endif", ""]

    os.makedirs(UI_DIR, exist_ok=True)
    with open(OUT_H, "w") as f:
        f.write("\n".join(h_parts))
    with open(OUT_C, "w") as f:
        f.write("\n".join(c_parts))
    print(f"gerado: {OUT_H}")
    print(f"gerado: {OUT_C} ({os.path.getsize(OUT_C) // 1024} KB de fonte)")


if __name__ == "__main__":
    main()
