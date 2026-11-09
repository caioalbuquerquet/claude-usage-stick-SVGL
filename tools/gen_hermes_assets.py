#!/usr/bin/env python3
"""
gen_hermes_assets.py — gera as imagens da marca THEP no estilo Hermes
(logo em ANSI Shadow e pássaro em braille) como imagens LVGL embutidas.

Saída (arquivos gerados, não editar na mão):
  firmware/claude_stick/src/ui/assets_thep.h  — LV_IMAGE_DECLARE de cada imagem
  firmware/claude_stick/src/ui/assets_thep.c  — dados e lv_image_dsc_t

Imagens (LVGL 9.2; RGB565A8, salvo indicação):
  img_thep_logo_hdr     104x38   logo THEP do cabeçalho
  img_bird_braille_124  124x124  pássaro em braille (1 conta, limiar, incidente)
  img_bird_braille_13   13x13    pássaro em braille do rodapé
  img_thep_logo_splash  264x96   logo THEP da tela de abertura, A1 (só o alfa;
                                 as faixas THEP_LOGO_SPLASH_* vão no .h)
  img_bird_braille_56   56x56    pássaro em braille da tela de abertura

Entrada: assets/brand/thep-bird.png (PNG 1024x1024 com transparência).

Requisitos: Python 3 e Pillow (pip install Pillow). Nada mais.

Uso:
  python3 tools/gen_hermes_assets.py                 # gera os .h/.c
  python3 tools/gen_hermes_assets.py --preview DIR   # idem + PNGs de conferência

As funções logo_hdr(), logo_splash(), bird_braille_124(), bird_braille_56() e
bird_braille_13() devolvem as imagens RGBA (Pillow) e podem ser importadas por
outros scripts (gen_mockups).
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


def _draw_thep(W: int, H: int, stroke: int, colors=None) -> Image.Image:
    """Desenha o THEP por células (sem fonte) numa imagem W x H. As bordas das
    células são arredondadas para pixels inteiros e cobrem a imagem sem buraco;
    o traço dos caracteres de linha tem `stroke` px, centrado na célula."""
    rows, cols = len(THEP), len(THEP[0])
    colors = colors or BANDS
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    xs = [round(c * W / cols) for c in range(cols + 1)]
    ys = [round(r * H / rows) for r in range(rows + 1)]
    half = stroke // 2
    for r, line in enumerate(THEP):
        col = _hex(colors[r]) + (255,)
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
    return im


def logo_hdr() -> Image.Image:
    """Desenha o THEP por células (sem fonte) em 8x e reduz para 104x38."""
    im = _draw_thep(LOGO_W * SS, LOGO_H * SS, STROKE)
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


# Logo da tela de abertura (screen_splash): 264x96 = células de 8x16 px exatos,
# traço de 2 px. Sem redução nem antisserrilhado: cada pixel é opaco ou
# transparente, então o firmware guarda só o alfa em 1 bit (LV_COLOR_FORMAT_A1,
# 3.168 bytes) e pinta as 3 faixas (SPLASH_BAND_H px cada) ao montar a tela.
SPLASH_W, SPLASH_H = 264, 96
SPLASH_STROKE = 2
SPLASH_BAND_H = SPLASH_H // 3


def logo_splash() -> Image.Image:
    """THEP 264x96 RGBA com as cores das faixas (mockups e conferência)."""
    im = _draw_thep(SPLASH_W, SPLASH_H, SPLASH_STROKE)
    assert sum(im.getchannel("A").histogram()[1:255]) == 0, "alfa parcial no logo da abertura"
    return im


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


def bird_braille_56() -> Image.Image:
    """56x56 (tela de abertura): 18x18 pontos de 2x2 px, passo 3 px, 2 px de
    margem, como o de 124 px. Ponto (i, j) em x = 2 + 3i, y = 2 + 3j."""
    n, step, margin, dot, size = 18, 3, 2, 2, 56
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


def to_a1(im: Image.Image) -> bytes:
    """LV_COLOR_FORMAT_A1: 1 bit por pixel, bit 7 = pixel mais à esquerda,
    cada linha com ceil(w/8) bytes. Pixel aceso = alfa >= 128."""
    a = im.convert("RGBA").getchannel("A")
    w, h = a.size
    stride = (w + 7) // 8
    out = bytearray(stride * h)
    for y in range(h):
        for x in range(w):
            if a.getpixel((x, y)) >= 128:
                out[y * stride + x // 8] |= 0x80 >> (x % 8)
    return bytes(out)


def from_a1(data: bytes, w: int, h: int) -> Image.Image:
    """Decodifica de volta, em branco (para os PNGs de conferência)."""
    stride = (w + 7) // 8
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for y in range(h):
        for x in range(w):
            if data[y * stride + x // 8] & (0x80 >> (x % 8)):
                im.putpixel((x, y), (255, 255, 255, 255))
    return im


# formato -> (codificador, decodificador, stride(w), bytes(w, h))
FORMATS = {
    "RGB565A8": (to_rgb565a8, from_rgb565a8, lambda w: w * 2, lambda w, h: w * h * 3),
    "A1": (to_a1, from_a1, lambda w: (w + 7) // 8, lambda w, h: (w + 7) // 8 * h),
}


def to_c(name: str, w: int, h: int, data: bytes, cf: str = "RGB565A8") -> str:
    stride = FORMATS[cf][2](w)
    assert len(data) == FORMATS[cf][3](w, h)
    rows = [",".join(f"0x{v:02x}" for v in data[i:i + 24]) for i in range(0, len(data), 24)]
    body = ",\n    ".join(rows)
    return (
        f"/* {name}: {w}x{h}, {cf}, {len(data)} bytes */\n"
        f"static const uint8_t {name}_map[] __attribute__((aligned(4))) = {{\n"
        f"    {body}\n"
        f"}};\n\n"
        f"const lv_image_dsc_t {name} = {{\n"
        f"    .header = {{\n"
        f"        .magic = LV_IMAGE_HEADER_MAGIC,\n"
        f"        .cf = LV_COLOR_FORMAT_{cf},\n"
        f"        .flags = 0,\n"
        f"        .w = {w},\n"
        f"        .h = {h},\n"
        f"        .stride = {stride},\n"
        f"        .reserved_2 = 0,\n"
        f"    }},\n"
        f"    .data_size = sizeof({name}_map),\n"
        f"    .data = {name}_map,\n"
        f"    .reserved = NULL,\n"
        f"}};\n"
    )


IMAGES = [
    ("img_thep_logo_hdr", logo_hdr, "RGB565A8", "logo THEP do cabeçalho"),
    ("img_bird_braille_124", bird_braille_124, "RGB565A8",
     "pássaro em braille: 1 conta, limiar e incidente (recolor cinza)"),
    ("img_bird_braille_13", bird_braille_13, "RGB565A8", "pássaro em braille do rodapé"),
    ("img_thep_logo_splash", logo_splash, "A1",
     "logo THEP da tela de abertura, só o alfa: as faixas são pintadas no firmware"),
    ("img_bird_braille_56", bird_braille_56, "RGB565A8",
     "pássaro em braille da tela de abertura"),
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
    for name, fn, cf, desc in IMAGES:
        im = fn()
        w, h = im.size
        enc, dec_fn = FORMATS[cf][0], FORMATS[cf][1]
        data = enc(im)
        h_parts.append(f"// {desc} ({w}x{h}, {cf})")
        h_parts.append(f"LV_IMAGE_DECLARE({name});")
        c_parts.append(to_c(name, w, h, data, cf))
        print(f"{name}: {w}x{h}, {cf}, {len(data)} bytes")
        if args.preview:
            os.makedirs(args.preview, exist_ok=True)
            dec = dec_fn(data, w, h)
            dec.save(os.path.join(args.preview, f"{name}.png"))
            bg = Image.new("RGBA", (w, h), BG_PREVIEW + (255,))
            bg.alpha_composite(dec)
            k = 8 if max(w, h) < 200 else 4
            bg.resize((w * k, h * k), Image.NEAREST).save(
                os.path.join(args.preview, f"{name}_x{k}.png"))
    # Faixas do logo da abertura (img_thep_logo_splash é só alfa): altura de
    # cada faixa e as cores RGB888, de cima para baixo.
    h_parts += ["", "// Faixas de img_thep_logo_splash, de cima para baixo (RGB888).",
                f"#define THEP_LOGO_SPLASH_BAND_H {SPLASH_BAND_H}",
                "#define THEP_LOGO_SPLASH_BANDS { "
                + ", ".join("0x" + c[1:] for c in BANDS[::2]) + " }"]
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
