#!/usr/bin/env python3
"""
gen_mockups.py — mockups 480x320 das telas da v3 (visual Hermes × THEP) em
assets/mock-*.png, com as MESMAS fontes e imagens do firmware:

  - JetBrains Mono 2.304 Regular/Bold de tools/.cache/fonts/ (baixadas pelo
    tools/gen_fonts.sh), nos tamanhos das fontes LVGL geradas (11, 13, bold 13
    e bold 20/30/38/50). As linhas de base vêm do .line_height/.base_line dos
    .c gerados em firmware/claude_stick/src/ui/fonts/;
  - logo THEP e pássaro em braille de tools/gen_hermes_assets.py.

A geometria copia o firmware (shell.cpp, overlay_menu.cpp, screen_*.cpp,
overlay_*.cpp). Os ícones LV_SYMBOL_* (Montserrat 14 no aparelho) não são
desenhados aqui, exceto o ↻ do rodapé e do menu.

Antes de desenhar, confere a tabela de campos do item 37 (e os textos fixos
das outras telas) com a largura real de cada texto na fonte do firmware. Se
algum texto passar do espaço (largura do card do item 36; as barras contam os
2 colchetes), o script lista os excessos e sai com erro, sem gravar nada.

Saída: assets/mock-home-{1,2,3,4}.png, mock-menu.png, mock-modelos.png,
mock-janela5h.png, mock-ritmo.png, mock-incidente.png, mock-momento.png e
mock-splash.png (abertura no meio da passada do brilho).

Requisitos: Python 3 + Pillow; tools/gen_fonts.sh já rodado (cache das TTF).
Uso: python3 tools/gen_mockups.py [--check]   (--check só confere, não grava)
"""
import argparse
import math
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(ROOT, "tools")
FONT_CACHE = os.path.join(TOOLS, ".cache", "fonts")
FONT_SRC = os.path.join(ROOT, "firmware", "claude_stick", "src", "ui", "fonts")
OUT = os.path.join(ROOT, "assets")

sys.path.insert(0, TOOLS)
import gen_hermes_assets as hermes  # noqa: E402

# ---- Paleta (theme.h) ----
C_BG, C_STATUSBAR, C_SURFACE, C_SELECTED = "#0c1222", "#070b18", "#141e33", "#1a2740"
C_BORDER, C_TITLE, C_ACCENT, C_TEXT = "#1272c2", "#2fb5dd", "#2f8fd6", "#e2e8f0"
C_DIM, C_UNLIT, C_OK, C_OK_INK = "#8a98a8", "#34457a", "#5caf4e", "#8ed57f"
C_WARN, C_DOT_YELLOW, C_BAD, C_BAD_INK = "#f0a32a", "#f0c419", "#f05252", "#ff8a8a"

W, H = 480, 320
CONTENT_X, CONTENT_Y = 8 + 1 + 12, 56 + 1 + 14     # origem do conteúdo do shell
CONTENT_W, CONTENT_H = 438, 214


def rgb(h, a=255):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)) + (a,)


# ---------------------------------------------------------------------------
# Fontes: mesma TTF e tamanho das fontes LVGL; métricas dos .c gerados
# ---------------------------------------------------------------------------
FONT_DEF = {  # nome LVGL -> (TTF, tamanho)
    "jbmono_11": ("JetBrainsMono-Regular.ttf", 11),
    "jbmono_13": ("JetBrainsMono-Regular.ttf", 13),
    "jbmono_bold_13": ("JetBrainsMono-Bold.ttf", 13),
    "jbmono_bold_num_20": ("JetBrainsMono-Bold.ttf", 20),
    "jbmono_bold_num_30": ("JetBrainsMono-Bold.ttf", 30),
    "jbmono_bold_num_38": ("JetBrainsMono-Bold.ttf", 38),
    "jbmono_bold_num_50": ("JetBrainsMono-Bold.ttf", 50),
}


class Font:
    def __init__(self, name):
        ttf, size = FONT_DEF[name]
        path = os.path.join(FONT_CACHE, ttf)
        if not os.path.exists(path):
            sys.exit(f"ERRO: {path} não existe. Rode tools/gen_fonts.sh antes.")
        self.name = name
        self.pil = ImageFont.truetype(path, size)
        src = open(os.path.join(FONT_SRC, name + ".c")).read()
        self.line_height = int(re.search(r"\.line_height = (\d+)", src).group(1))
        self.base_line = int(re.search(r"\.base_line = (\d+)", src).group(1))
        self.ascent = self.line_height - self.base_line   # ui_ascent()

    def w(self, s):
        return self.pil.getlength(s)


F11, F13, FB13 = Font("jbmono_11"), Font("jbmono_13"), Font("jbmono_bold_13")
N20, N30, N38, N50 = (Font(f"jbmono_bold_num_{n}") for n in (20, 30, 38, 50))

# ---------------------------------------------------------------------------
# Conferência de largura (falha no fim, com a lista de excessos)
# ---------------------------------------------------------------------------
OVERFLOWS = []


def fits(what, text, font, maxw):
    w = font.w(text)
    if w > maxw:
        OVERFLOWS.append(f"{what}: \"{text}\" mede {w:.1f} px em {font.name}, "
                         f"o espaço é {maxw} px")
    return w


def fit_label(s, n):
    """fit_label() de src/logic/levels.cpp: corta em n code points com '…'."""
    return s if len(s) <= n else s[:n - 1] + "…"


def proj_fit(s, n):
    """projection_text(): acima de n code points, o trecho ' · <tempo>' sai."""
    if len(s) > n and " · " in s:
        s = s[:s.rindex(" · ")]
    return fit_label(s, n)


def wrap_lines(s, font, maxw):
    """Linhas de um label LV_LABEL_LONG_WRAP (quebra em espaço)."""
    lines, cur = 1, ""
    for word in s.split(" "):
        t = (cur + " " + word) if cur else word
        if font.w(t) <= maxw:
            cur = t
        else:
            lines, cur = lines + 1, word
    return lines


def text_bar(pct, cells):
    on = max(0, min(cells, int(pct * cells / 100 + 0.5)))
    return "█" * on, "░" * (cells - on)


def level_color(p):
    return C_OK if p < 50 else C_WARN if p < 80 else C_BAD


# ---------------------------------------------------------------------------
# Desenho
# ---------------------------------------------------------------------------
class Canvas:
    def __init__(self):
        self.im = Image.new("RGBA", (W, H), rgb(C_BG))
        self.d = ImageDraw.Draw(self.im)

    def text(self, x, base, s, font, col, anchor="ls"):
        self.d.text((x, base), s, font=font.pil, fill=rgb(col), anchor=anchor)
        return font.w(s)

    def text_top(self, x, top, s, font, col):
        """Label LVGL posicionado pelo topo (mklabel + set_pos)."""
        return self.text(x, top + font.ascent, s, font, col)

    def spans(self, x, base, parts, anchor="l"):
        """Trechos de cores diferentes (lv_spangroup). parts = [(txt, font, cor)]."""
        total = sum(f.w(t) for t, f, _ in parts)
        if anchor == "r":
            x -= total
        for t, f, c in parts:
            x += self.text(x, base, t, f, c)
        return total

    def bar(self, x, base, pct, cells, font, col, anchor="l"):
        on, off = text_bar(pct, cells)
        return self.spans(x, base, [("[", font, C_DIM), (on, font, col),
                                    (off, font, C_UNLIT), ("]", font, C_DIM)], anchor)

    def refresh_icon(self, x, base, col):
        """LV_SYMBOL_REFRESH (Montserrat 14 no aparelho): arco + seta. Devolve a largura."""
        cy = base - 4
        self.d.arc((x, cy - 5, x + 10, cy + 5), 40, 320, fill=rgb(col), width=2)
        self.d.polygon([(x + 7, cy - 7), (x + 11, cy - 2), (x + 6, cy - 1)], fill=rgb(col))
        return 12

    def rect(self, box, fill=None, outline=None, width=1, radius=0):
        self.d.rounded_rectangle(box, radius, fill=rgb(fill) if fill else None,
                                 outline=rgb(outline) if outline else None, width=width)

    def image(self, im, x, y):
        self.im.alpha_composite(im, (int(x), int(y)))

    def scrim(self, col, opa):
        layer = Image.new("RGBA", (W, H), rgb(col, opa))
        self.im.alpha_composite(layer)

    def save(self, name):
        p = os.path.join(OUT, name)
        self.im.convert("RGB").save(p)
        print(f"gerado: {p}")


LOGO = hermes.logo_hdr()
BIRD = hermes.bird_braille_124()


def recolor(im, col, opa):
    """image_recolor + image_recolor_opa do LVGL (o alfa fica)."""
    r, g, b, a = im.split()
    flat = Image.new("RGBA", im.size, rgb(col))
    mixed = Image.blend(Image.merge("RGBA", (r, g, b, Image.new("L", im.size, 255))),
                        flat, opa / 255)
    mixed.putalpha(a)
    return mixed


# ---------------------------------------------------------------------------
# Shell (shell.cpp)
# ---------------------------------------------------------------------------
def shell(title, dot=C_OK, menu_open=False, accounts=2, failed=0, poll="2min", clock="14:32"):
    c = Canvas()
    # cabeçalho: logo, ponto e "❯ menu" (flex, pad 6/8/4/10, itens centrados em y)
    c.image(LOGO, 10, 6 + (40 - LOGO.height) // 2)
    bx, by, bw, bh = W - 8 - 84, 6 + 1, 84, 38
    c.rect((bx, by, bx + bw - 1, by + bh - 1), fill=C_SELECTED if menu_open else None,
           outline=C_BORDER, radius=6)
    mw = FB13.w("❯") + 8 + FB13.w("menu")
    mx = bx + (bw - mw) / 2
    mid = by + bh / 2 + 4
    mx += c.text(mx, mid, "❯", FB13, C_DIM) + 8
    c.text(mx, mid, "menu", FB13, C_TITLE)
    dx = bx - 10 - 10
    c.d.ellipse((dx, 6 + 15, dx + 9, 6 + 24), fill=rgb(dot))
    # painel e título sobre a borda
    c.rect((8, 56, 8 + 464 - 1, 56 + 238 - 1), outline=C_BORDER, radius=8)
    tw = FB13.w(title) + 12
    tx = W - (8 + 1 + 14) - tw
    ty = 56 - FB13.line_height // 2
    c.rect((tx, ty + 3, tx + tw, ty + FB13.line_height - 3), fill=C_BG)
    c.text_top(tx + 6, ty, title, FB13, C_TITLE)
    fits("título do painel", title, FB13, 464 - 30 - 84)
    # rodapé em 13 px: "N contas · K ✗ │ ↻ 2min" à esquerda e o relógio na
    # borda direita (sem pássaro nem "thep"), como em shell.cpp
    c.rect((0, H - 20, W - 1, H - 1), fill=C_STATUSBAR)
    acct = "1 conta" if accounts == 1 else f"{accounts} contas"
    if failed:
        acct += f" · {failed} ✗"
    base = H - 20 + 10 + 5
    x = 10
    for part, col in ((acct, C_TEXT), ("│", C_UNLIT), ("↻", C_DIM), (poll, C_TEXT)):
        pad = 2 if part == "│" else 0
        x += pad
        if part == "↻":
            x += c.refresh_icon(x, base, col) + 5
            continue
        x += c.text(x, base, part, F13, col) + pad + 5
    c.text(W - 10 - F13.w(clock), base, clock, F13, C_TEXT)
    return c


def content(c, x, y):
    return CONTENT_X + x, CONTENT_Y + y


# ---------------------------------------------------------------------------
# /agora (screen_home.cpp, item 36/37)
# ---------------------------------------------------------------------------
# rótulo, 5h, semana, reset 5h (in·at), reset 5h (at), reset semana, ok?, fim, esgota
RAW = [
    ("Conta 1", 62, 38, "1h12m · 16:40", "16:40", "qui 09:00", True, "~78%", ""),
    ("Conta 2", 87, 71, "2h41m · 18:09", "18:09", "sáb 14:00", False, "", "15:20 · 38m"),
    ("Conta 3", 24, 55, "3h05m · 19:33", "19:33", "dom 11:00", True, "~41%", ""),
    ("Conta 4", 45, 92, "0h50m · 16:18", "16:18", "ter 08:00", True, "~52%", ""),
]
MARKS = ["ok", "atenção", "ok", "ok"]


def proj(a, n):
    label, p5, p7, r5, r5s, r7, ok, end, runout = a
    s = f"✓ não esgota · {end}" if ok else f"✗ esgota {runout}"
    return proj_fit(s, n), (C_OK_INK if ok else C_BAD_INK)


# Tabela de campos do item 37 (como no SPEC de screen_home.cpp).
SPEC = {
    1: dict(card_w=272, label_max=16, bar5=18, bar5_font=F13, bar7=10, proj=41),
    2: dict(card_w=210, label_max=16, bar5=14, bar5_font=F13, bar7=8, proj=26),
    3: dict(card_w=138, label_max=10, bar5=9, bar5_font=F11, bar7=0, proj=20),
    4: dict(card_w=210, label_max=14, bar5=14, bar5_font=F11, bar7=0, proj=31),
}

# Piores casos de cada texto variável (largura monoespaçada: conta o tamanho).
WORST = {
    "label": "M" * 16,
    "mark": ["ok", "atenção", "limite", "warning", "limit", "abcdefgh"],
    "pct": "100%",
    "reset_in_at": ["passou · 16:40", "passed · 16:40", "23h59m · 16:40", "6d23h · 16:40"],
    "reset_at": ["16:40", "--"],
    "reset_week": ["qui 09:00", "Thu 09:00"],
    "proj": ["✓ não esgota · ~100%", "✓ ok to reset · ~100%", "✗ esgota 15:20 · 4h38m",
             "✗ runs out 15:20 · 4h38m", "✗ esgotada", "✗ exhausted", "— sem histórico",
             "— no history", "— janela reiniciou", "— window reset", "consultando…",
             "querying…", "SEM CHAVE", "NO KEY", "⚠ NOCLOCK · 59m", "⚠ NOWIFI · 23h"],
    "tok": ["tokens: 999.9k entrada · 999.9M saída", "tokens: 999.9k in · 999.9M out"],
}


def check_table():
    for n, S in SPEC.items():
        cw = S["card_w"]
        tag = f"layout {n}"
        lab = fit_label(WORST["label"], S["label_max"])
        lw = fits(f"{tag} rótulo", lab, FB13, cw)
        ft = F13 if n == 2 else F11          # texto secundário (layout 2 em 13 px)
        if n in (2, 3):                       # rótulo + marca na mesma linha
            for m in WORST["mark"]:
                if lw + 6 + ft.w(m) > cw:
                    OVERFLOWS.append(f"{tag}: rótulo ({lw:.0f} px) + marca \"{m}\" "
                                     f"({ft.w(m):.0f} px) passam de {cw} px")
        if n == 1:
            for m in WORST["mark"]:
                fits(f"{tag} status sob o pássaro", "status: " + m, F11, 150)
        num = {1: N50, 2: N38, 3: N30, 4: N20}[n]
        pw = fits(f"{tag} % da 5h", WORST["pct"], num, cw)
        if n == 4:
            if lw + 6 + F11.w("5h ") + pw > cw:
                OVERFLOWS.append(f"{tag}: rótulo + \"5h 100%\" passam de {cw} px")
        bar = "[" + "█" * S["bar5"] + "]"
        fits(f"{tag} barra da 5h", bar, S["bar5_font"], cw)
        key = {1: "reset: ", 2: "reset: ", 3: "", 4: "reset "}[n]
        for r in (WORST["reset_at"] if n == 4 else WORST["reset_in_at"]):
            if n == 4:
                week = "sem 100%"
                if F11.w("sem ") + FB13.w("100%") + 8 + F11.w(key + r) > cw:
                    OVERFLOWS.append(f"{tag}: \"{week}\" + \"{key + r}\" passam de {cw} px")
            else:
                fits(f"{tag} reset da 5h", key + r, ft, cw)
        if n in (1, 2, 3):
            for r in WORST["reset_week"]:
                fits(f"{tag} reset da semana", ("reset " if n == 3 else "reset: ") + r, ft, cw)
        if n == 1:
            fits(f"{tag} semana + barra", "100%", FB13, 44 - 4)
            fits(f"{tag} barra da semana", "[" + "█" * S["bar7"] + "]", F11, cw - 44)
        if n == 2:
            for wk in ("semana: ", "week: "):
                ww = ft.w(wk) + FB13.w("100%") + 8 + ft.w("[" + "█" * S["bar7"] + "]")
                if ww > cw:
                    OVERFLOWS.append(f"{tag}: \"{wk}100%\" + barra da semana medem {ww:.0f} px "
                                     f"(espaço {cw} px)")
        if n == 3:
            for wk in ("semana ", "week "):
                fits(f"{tag} semana", wk + "100%", F11, cw)
        for p in WORST["proj"]:
            fits(f"{tag} projeção", proj_fit(p, S["proj"]), ft, cw)
        if n == 1:
            for t in WORST["tok"]:
                fits(f"{tag} tokens", fit_label(t, S["proj"]), F11, cw)


def card_outline(c, x, y, w, h):
    c.rect((x - 4, y - 4, x + w + 3, y + h + 3), outline=C_ACCENT, radius=4)


def home(n):
    title = f"THEP Usage v3.0 · {n} conta" + ("" if n == 1 else "s")
    c = shell(title, accounts=n)
    S = SPEC[n]
    if n == 1:
        a = RAW[0]
        ox, oy = content(c, 0, 0)
        c.image(BIRD, ox + (150 - 124) // 2, oy + 18)
        lab = fit_label(a[0], S["label_max"])
        c.text(ox + 75, oy + 160, lab, FB13, C_ACCENT, anchor="ms")
        sw = F11.w("status: ") + F11.w("ok")
        c.spans(ox + 75 - sw / 2, oy + 178, [("status: ", F11, C_DIM), ("ok", F11, C_OK_INK)])
        X = ox + 166
        c.text(X, oy + 12, "Janela 5h", FB13, C_ACCENT)
        c.text(X, oy + 56, f"{a[1]}%", N50, level_color(a[1]))
        c.bar(X, oy + 76, a[1], S["bar5"], F13, level_color(a[1]))
        c.spans(X, oy + 93, [("reset: ", F11, C_DIM), (a[3], F11, C_TEXT)])
        c.text(X, oy + 116, "Semana", FB13, C_ACCENT)
        c.text(X, oy + 134, f"{a[2]}%", FB13, level_color(a[2]))
        c.bar(X + 44, oy + 134, a[2], S["bar7"], F11, level_color(a[2]))
        c.spans(X, oy + 151, [("reset: ", F11, C_DIM), (a[5], F11, C_TEXT)])
        t, col = proj(a, S["proj"])
        c.text(X, oy + 172, t, F11, col)
        c.text(X, oy + 190, fit_label("tokens: 412.3k entrada · 1.9M saída", S["proj"]), F11, C_DIM)
        return c
    if n in (2, 3):
        two = n == 2
        ft = F13 if two else F11
        num = N38 if two else N30
        pos = [0, 228] if two else [0, 150, 300]
        y = dict(label=14, h5=36, pct=76, bar5=98, r5=118, wk=146, r7=166, proj=196) if two else \
            dict(label=12, h5=28, pct=56, bar5=73, r5=89, wk=112, r7=128, proj=148)
        for i in range(n):
            a = RAW[i]
            ox, oy = content(c, pos[i], 0)
            cw = S["card_w"]
            if i == 0:
                card_outline(c, ox, oy, cw, CONTENT_H)
            c.text(ox, oy + y["label"], fit_label(a[0], S["label_max"]), FB13, C_ACCENT)
            mark = MARKS[i]
            c.text(ox + cw, oy + y["label"], mark, ft, C_WARN if mark == "atenção" else C_OK_INK,
                   anchor="rs")
            c.text(ox, oy + y["h5"], "5h", ft, C_DIM)
            c.text(ox, oy + y["pct"], f"{a[1]}%", num, level_color(a[1]))
            c.bar(ox, oy + y["bar5"], a[1], S["bar5"], S["bar5_font"], level_color(a[1]))
            if two:
                c.spans(ox, oy + y["r5"], [("reset: ", ft, C_DIM), (a[3], ft, C_TEXT)])
                kx = c.text(ox, oy + y["wk"], "semana: ", ft, C_DIM)
                c.text(ox + kx, oy + y["wk"], f"{a[2]}%", FB13, level_color(a[2]))
                c.bar(ox + cw, oy + y["wk"], a[2], S["bar7"], ft, level_color(a[2]), anchor="r")
                c.spans(ox, oy + y["r7"], [("reset: ", ft, C_DIM), (a[5], ft, C_TEXT)])
            else:
                c.text(ox, oy + y["r5"], a[3], ft, C_TEXT)
                kx = c.text(ox, oy + y["wk"], "semana ", ft, C_DIM)
                c.text(ox + kx, oy + y["wk"], f"{a[2]}%", FB13, level_color(a[2]))
                c.spans(ox, oy + y["r7"], [("reset ", ft, C_DIM), (a[5], ft, C_TEXT)])
            t, col = proj(a, S["proj"])
            c.text(ox, oy + y["proj"], t, ft, col)
        return c
    # 4 contas: 2 x 2 de 210 x 103
    pos = [(0, 0), (228, 0), (0, 111), (228, 111)]
    for i in range(4):
        a = RAW[i]
        ox, oy = content(c, *pos[i])
        cw = S["card_w"]
        if i == 0:
            card_outline(c, ox, oy, cw, 103)
        c.text(ox, oy + 18, fit_label(a[0], S["label_max"]), FB13, C_ACCENT)
        pct = f"{a[1]}%"
        col5 = C_WARN if MARKS[i] == "atenção" else level_color(a[1])
        pw = c.text(ox + cw, oy + 18, pct, N20, col5, anchor="rs")
        c.text(ox + cw - pw - F11.w("5h "), oy + 18, "5h ", F11, C_DIM)
        c.bar(ox, oy + 40, a[1], S["bar5"], F11, col5)
        kx = c.text(ox, oy + 60, "sem ", F11, C_DIM)
        c.text(ox + kx, oy + 60, f"{a[2]}%", FB13, level_color(a[2]))
        c.spans(ox + cw, oy + 60, [("reset ", F11, C_DIM), (a[4], F11, C_TEXT)], anchor="r")
        t, col = proj(a, S["proj"])
        c.text(ox, oy + 82, t, F11, col)
    return c


# ---------------------------------------------------------------------------
# Menu (overlay_menu.cpp)
# ---------------------------------------------------------------------------
MENU = [(None, None), ("/agora", "uso das contas"), ("/modelos", "status por modelo"),
        ("/janela", "gráfico 5h"), ("/ritmo", "uso por hora"), ("/contas", "gerenciar contas"),
        ("/ajustes", "configurações")]


def check_menu():
    for cmd, desc in MENU[1:]:
        fits("menu, comando", cmd, FB13, 86)
        fits("menu, descrição", desc, F11, 252 - 122 - 10)
    fits("menu, atualizar", "↻ Atualizar agora", FB13, 252 - 28 - 10)


def menu():
    c = home(2)
    c.rect((W - 8 - 84, 7, W - 9, 44), fill=C_SELECTED, outline=C_BORDER, radius=6)
    mx = W - 8 - 84 + (84 - FB13.w("❯") - 8 - FB13.w("menu")) / 2
    mx += c.text(mx, 7 + 19 + 4, "❯", FB13, C_DIM) + 8
    c.text(mx, 7 + 19 + 4, "menu", FB13, C_TITLE)
    x0, y0 = W - 8 - 262, 50
    c.rect((x0, y0, x0 + 261, y0 + 261), fill=C_SURFACE, outline=C_BORDER, radius=6)
    for i, (cmd, desc) in enumerate(MENU):
        rx, ry = x0 + 1 + 4, y0 + 1 + 4 + i * 36
        cur = i == 1
        if cur:
            c.rect((rx, ry, rx + 251, ry + 35), fill=C_SELECTED, radius=4)
        base = ry + 18 + 5
        if cur:
            c.text(rx + 10, base, "❯", FB13, C_TITLE)
        if cmd is None:
            w = c.refresh_icon(rx + 28, base, C_TEXT)
            c.text(rx + 28 + w + 6, base, "Atualizar agora", FB13, C_TEXT)
        else:
            c.text(rx + 28, base, cmd, FB13, C_TITLE if cur else C_TEXT)
            c.text(rx + 122, base, desc, F11, C_DIM)
    return c


# ---------------------------------------------------------------------------
# /modelos (screen_models.cpp)
# ---------------------------------------------------------------------------
def modelos():
    c = shell("/modelos")
    ox, oy = content(c, 0, 0)
    c.text(ox, oy + 14, "Modelos · Conta 1", FB13, C_ACCENT)
    sx = F13.w("123456789")
    rows = [("Haiku", "ok · 0.9s", C_OK_INK), ("Sonnet", "ok · 1.4s", C_OK_INK),
            ("Opus", "limitado", C_WARN), ("Fable", "erro 503", C_BAD_INK)]
    for i, (name, st, col) in enumerate(rows):
        base = oy + 44 + i * 24
        c.text(ox, base, name, F13, C_TEXT)
        c.text(ox + sx, base, st, F13, col)
    inc = fit_label("incidente GRAVE · Opus 4.8 • Fable 5", 66)
    fits("/modelos, incidente", fit_label("incidente IMPACTO NÃO DECLARADO · Haiku • Sonnet • Opus", 66),
         F11, CONTENT_W)
    c.text(ox, oy + 44 + 4 * 24 + 16, inc, F11, C_BAD_INK)
    return c


# ---------------------------------------------------------------------------
# /janela (screen_trend.cpp)
# ---------------------------------------------------------------------------
TR_X0, TR_Y0, TR_W, TR_H = 30, 30, CONTENT_W - 30 - 4, 110


def tr_y(p):
    return TR_Y0 + TR_H - int(max(0, min(100, p)) * TR_H / 100)


def janela():
    c = shell("Conta 1")
    ox, oy = content(c, 0, 0)
    c.text(ox, oy + 14, "Janela 5h", FB13, C_ACCENT)
    c.text(ox + CONTENT_W, oy + 14, "62%", FB13, level_color(62), anchor="rs")
    for p in (0, 100):
        c.d.line((ox + TR_X0, oy + tr_y(p), ox + TR_X0 + TR_W, oy + tr_y(p)), fill=rgb(C_UNLIT))
    for p in (25, 50, 75):
        y = oy + tr_y(p)
        for x in range(TR_X0, TR_X0 + TR_W, 6):
            c.d.line((ox + x, y, ox + x + 1, y), fill=rgb(C_UNLIT))
    for p, s in ((100, "100"), (50, " 50"), (0, "  0")):
        c.text(ox, oy + tr_y(p) + 4, s, F11, C_DIM)
    # 3h48m de janela usados: 62% agora, ritmo ~0,16 %/min -> ~78% no reset
    now_f = 0.76
    pts = []
    for k in range(0, 39):
        f = k / 38 * now_f
        p = 62 * (f / now_f) ** 1.15 + 2 * math.sin(k / 3)
        pts.append((ox + TR_X0 + f * TR_W, oy + tr_y(max(0, min(62, p)))))
    pts[-1] = (ox + TR_X0 + now_f * TR_W, oy + tr_y(62))
    c.d.line(pts, fill=rgb(C_TITLE), width=2, joint="curve")
    cx, cy = pts[-1]
    ex, ey = ox + TR_X0 + TR_W, oy + tr_y(78)
    n = 14
    for k in range(n):
        if k % 2:
            continue
        a, b = k / n, (k + 1) / n
        c.d.line((cx + (ex - cx) * a, cy + (ey - cy) * a, cx + (ex - cx) * b, cy + (ey - cy) * b),
                 fill=rgb(C_OK), width=2)
    c.d.ellipse((cx - 3, cy - 3, cx + 3, cy + 3), fill=rgb(C_TEXT))
    axis = oy + tr_y(0) + 16
    c.text(ox + TR_X0, axis, "11:40", F11, C_DIM)
    c.text(ox + CONTENT_W - 4, axis, "16:40", F11, C_DIM, anchor="rs")
    cap = proj_fit("✓ não esgota · ~78%", 60)
    for p in WORST["proj"]:
        fits("/janela, legenda", proj_fit(p, 60), F11, CONTENT_W)
    c.text(ox, oy + 186, cap, F11, C_OK_INK)
    c.text(ox, oy + 204, "tokens: 412.3k entrada · 1.9M saída", F11, C_DIM)
    return c


# ---------------------------------------------------------------------------
# /ritmo (screen_heat.cpp)
# ---------------------------------------------------------------------------
def ritmo():
    c = shell("Conta 1")
    ox, oy = content(c, 0, 0)
    c.text(ox, oy + 16, "Ritmo por hora", FB13, C_ACCENT)
    names = ["hoje", "7d", "30d", "tudo"]
    for i in range(4):
        bx = ox + CONTENT_W - 52 - (3 - i) * 58
        on = i == 3
        c.rect((bx, oy, bx + 51, oy + 25), fill=C_SELECTED if on else None,
               outline=C_TITLE if on else C_BORDER, radius=6)
        c.text(bx + 26, oy + 13 + 4, names[i], F11, C_TITLE if on else C_DIM, anchor="ms")
    base_y, bar_w, step, max_h = 168, 12, 18, 120
    x0 = (CONTENT_W - 23 * step - bar_w) // 2
    data = [2, 1, 0, 0, 0, 0, 1, 3, 9, 16, 22, 25, 14, 18, 27, 30, 24, 19, 12, 9, 11, 8, 5, 3]
    mx = max(data)
    for h, v in enumerate(data):
        r = v / mx
        hgt = 2 + int(r * (max_h - 2))
        x = ox + x0 + h * step
        col = rgb(C_TITLE if h == 14 else C_ACCENT, 90 + int(r * 165))
        layer = Image.new("RGBA", (bar_w, hgt), col)
        mask = Image.new("L", (bar_w, hgt), 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, bar_w - 1, hgt - 1), 2, fill=255)
        c.im.paste(Image.alpha_composite(c.im.crop((x, oy + base_y - hgt, x + bar_w, oy + base_y)),
                                         layer), (x, oy + base_y - hgt), mask)
    c.d.line((ox + x0 - 2, oy + base_y + 1, ox + x0 + 23 * step + bar_w + 2, oy + base_y + 1),
             fill=rgb(C_UNLIT))
    for t in (0, 6, 12, 18, 23):
        c.text(ox + x0 + t * step, oy + base_y + 16, f"{t}h", F11, C_DIM)
    leg = "quota da janela 5h queimada em cada hora local"
    fits("/ritmo, legenda", leg, F11, CONTENT_W)
    c.text(ox, oy + 208, leg, F11, C_DIM)
    return c


# ---------------------------------------------------------------------------
# Incidente (overlay_incident.cpp)
# ---------------------------------------------------------------------------
def incidente():
    c = home(2)
    c.scrim(C_BG, 210)
    bw, bh = 420, 206
    x0, y0 = (W - bw) // 2, (H - bh) // 2
    sev = C_BAD_INK                     # GRAVE
    c.rect((x0, y0, x0 + bw - 1, y0 + bh - 1), fill=C_SURFACE, outline=sev, radius=8)
    px, py = x0 + 16, y0 + 16
    c.image(recolor(BIRD, C_DIM, 230), px, py + (172 - 124) // 2)
    tx, tw = px + 124 + 16, 386 - 140
    title = "⚠ INCIDENTE ATIVO"
    fits("incidente, título", "⚠ ACTIVE INCIDENT", FB13, tw)
    fits("incidente, título", title, FB13, tw)
    c.text(tx, py + 16, title, FB13, sev)
    chip = "GRAVE"
    fits("incidente, chip", "IMPACTO NÃO DECLARADO", FB13, tw - 16)
    cw = FB13.w(chip) + 16
    c.rect((tx, py + 26, tx + cw, py + 26 + 21), fill="#3a2a3a", outline=sev, radius=4)
    c.text(tx + 8, py + 26 + 11 + 4, chip, FB13, sev)
    c.text(tx, py + 72, "Opus 4.8 • Fable 5", F13, C_TEXT)
    fits("incidente, modelos (2 com versão)", "Sonnet 5 • Haiku 4.5", F13, tw)
    c.text(tx, py + 92, "veja status.claude.com", F11, C_DIM)
    c.rect((tx, py + 172 - 34, tx + 119, py + 171), outline=C_BORDER, radius=6)
    c.text(tx + 60, py + 172 - 17 + 4, "OK", FB13, C_TITLE, anchor="ms")
    return c


# ---------------------------------------------------------------------------
# Momento de limiar (overlay_moment.cpp): 70% na 5h da Conta 2
# ---------------------------------------------------------------------------
def momento():
    c = home(2)
    c.scrim(C_BG, 248)
    c.rect((6, 6, W - 7, H - 7), outline=C_BORDER, radius=8)
    c.image(BIRD, 40 + 2, 98)                       # tremendo
    X = 196
    colw = W - X - 20
    c.text(X, 64, "Conta 2", FB13, C_ACCENT)
    c.text(X, 84, "Janela 5h", F13, C_DIM)
    c.text(X, 140, "70%", N50, level_color(70))
    c.bar(X, 162, 70, 18, F13, level_color(70))
    fits("momento, barra", "[" + "█" * 18 + "]", F13, colw)
    for m in ("Começando · ritmo tranquilo", "Metade da janela usada", "Atenção · uso alto",
              "Limite atingido · aguarde o reset", "Limit reached · wait for the reset"):
        if wrap_lines(m, F13, colw) > 2:
            OVERFLOWS.append(f"momento, mensagem: \"{m}\" passa de 2 linhas em {colw} px")
    c.text_top(X, 178, "Atenção · uso alto", F13, C_TEXT)
    c.text(X, 236, "reset em 1h 32m", F11, C_DIM)
    c.text(W - 20, 300, "toque para fechar", F11, C_DIM, anchor="rs")
    return c


# ---------------------------------------------------------------------------
# Abertura (screen_splash.cpp): mesma conta inteira do firmware
# ---------------------------------------------------------------------------
SPLASH_MS, SHINE_HW, SHINE_K256, SHINE_PEAK = 4000, 71, 69, 217
C_SHINE, C_SPLASH_DOT = "#e2f0ff", "#2fb5dd"


def css_ease(t):
    """cubic-bezier(.42, 0, .58, 1) com t em 0..1 (lv_cubic_bezier no firmware)."""
    lo, hi = 0.0, 1.0
    for _ in range(40):                       # acha u com x(u) = t
        u = (lo + hi) / 2
        x = 3 * u * (1 - u) ** 2 * 0.42 + 3 * u * u * (1 - u) * 0.58 + u ** 3
        lo, hi = (u, hi) if x < t else (lo, u)
    u = (lo + hi) / 2
    return 3 * u * u * (1 - u) + u ** 3


def splash(t_ms=SPLASH_MS // 2):
    c = Canvas()
    logo, bird = hermes.logo_splash(), hermes.bird_braille_56()
    lw, lh = logo.size
    top = (H - (56 + 18 + lh + 18 + 8)) // 2
    c.image(bird, (W - 56) // 2, top)
    # faixa de brilho misturada só nos pixels do logo
    skew = SHINE_K256 * (lh - 1) // 256
    c0, c1 = -SHINE_HW, lw - 1 + skew + SHINE_HW
    cx = c0 + (c1 - c0) * css_ease(min(t_ms, SPLASH_MS) / SPLASH_MS)
    sr, sg, sb, _ = rgb(C_SHINE)
    px = logo.load()
    for y in range(lh):
        for x in range(lw):
            r, g, b, a = px[x, y]
            if not a:
                continue
            d = abs(x * 256 + SHINE_K256 * y - cx * 256)
            if d < SHINE_HW * 256:
                k = SHINE_PEAK * (SHINE_HW * 256 - d) / (SHINE_HW * 256) / 255
                px[x, y] = (round(r + (sr - r) * k), round(g + (sg - g) * k),
                            round(b + (sb - b) * k), a)
    c.image(logo, (W - lw) // 2, top + 56 + 18)
    # pontos: 25% -> 100% (40% do ciclo de 1,8 s) -> 25%, 0,3 s entre eles
    y = top + 56 + 18 + lh + 18 + 1
    for i in range(3):
        t = t_ms - 300 * i
        ph = (t % 1800) / 1800 if t >= 0 else 0
        e = css_ease(ph / 0.4) if ph < 0.4 else 1 - css_ease((ph - 0.4) / 0.6)
        opa = round(64 + (255 - 64) * e)
        x = W // 2 - 19 + i * 16
        c.d.ellipse((x, y, x + 5, y + 5), fill=rgb(C_SPLASH_DOT, opa))
    return c


def main():
    ap = argparse.ArgumentParser(description="mockups da v3 com as fontes do firmware")
    ap.add_argument("--check", action="store_true", help="só confere as larguras")
    args = ap.parse_args()

    check_table()
    check_menu()
    shots = {
        "mock-home-1.png": home(1),
        "mock-home-2.png": home(2),
        "mock-home-3.png": home(3),
        "mock-home-4.png": home(4),
        "mock-menu.png": menu(),
        "mock-modelos.png": modelos(),
        "mock-janela5h.png": janela(),
        "mock-ritmo.png": ritmo(),
        "mock-incidente.png": incidente(),
        "mock-momento.png": momento(),
        "mock-splash.png": splash(),
    }
    if OVERFLOWS:
        print("ERRO: textos que não cabem no espaço do firmware:", file=sys.stderr)
        for o in OVERFLOWS:
            print("  - " + o, file=sys.stderr)
        sys.exit(1)
    print("larguras ok (tabela do item 37 e textos fixos)")
    if args.check:
        return
    for name, c in shots.items():
        c.save(name)


if __name__ == "__main__":
    main()
