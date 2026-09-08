#!/usr/bin/env python3
"""
gen_dead_clawd.py — converte assets/brand/clawd-dead.png (Clawd "morto", usado
no dialog de incidente) na imagem LVGL embutida firmware/claude_stick/clawd_dead.h.

Separado do gen_logo_assets.py porque a fonte aqui e um PNG, nao um SVG oficial:
este script so precisa de Pillow (sem rsvg-convert).

Uso: python3 tools/gen_dead_clawd.py
"""
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "assets", "brand", "clawd-dead.png")
OUT = os.path.join(ROOT, "firmware", "claude_stick", "clawd_dead.h")

TARGET_W = 104          # largura final na caixa do dialog (420x176, pad 18)


def to_c(im: Image.Image, name: str) -> str:
    """PIL RGBA -> lv_image_dsc_t ARGB8888 (bytes B,G,R,A little-endian)."""
    w, h = im.size
    px = im.tobytes()
    data = bytearray()
    for i in range(0, len(px), 4):
        r, g, b, a = px[i], px[i + 1], px[i + 2], px[i + 3]
        data += bytes((b, g, r, a))
    rows = [",".join(str(v) for v in data[i:i + 20]) for i in range(0, len(data), 20)]
    body = ",\n  ".join(rows)
    return (
        f"static const uint8_t {name}_map[] = {{\n  {body}\n}};\n"
        f"static const lv_image_dsc_t {name} = {{\n"
        f"  {{ LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_ARGB8888, 0, {w}, {h}, {w * 4}, 0 }},\n"
        f"  sizeof({name}_map), {name}_map\n"
        f"}};\n"
    )


def main() -> None:
    im = Image.open(SRC).convert("RGBA")
    im = im.crop(im.getbbox())                       # tira a moldura transparente
    h = max(1, round(im.height * TARGET_W / im.width))
    im = im.resize((TARGET_W, h), Image.LANCZOS)

    parts = ["// GERADO por tools/gen_dead_clawd.py — nao editar na mao.",
             "#pragma once", "#include <lvgl.h>", "",
             to_c(im, "img_clawd_dead"),
             f"#define CLAWD_DEAD_W {im.width}",
             f"#define CLAWD_DEAD_H {im.height}", ""]
    with open(OUT, "w") as f:
        f.write("\n".join(parts))
    print(f"gerado: {OUT} ({im.width}x{im.height}, "
          f"{im.width * im.height * 4 // 1024} KB em flash)")


if __name__ == "__main__":
    main()
