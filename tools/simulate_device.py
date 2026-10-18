#!/usr/bin/env python3
"""Preview how the device screen will look, without flashing anything.

Re-creates the firmware's layout (same sizes, colors, wrapping and markup rules)
using desktop fonts, so spacing is close but not pixel-identical.

    python3 tools/simulate_device.py sdcard out_folder [article-id ...]
"""
import os, re, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qrcodegen import QrCode

SD = sys.argv[1] if len(sys.argv) > 1 else "sdcard"
OUT = sys.argv[2] if len(sys.argv) > 2 else "device_preview"
os.makedirs(OUT, exist_ok=True)

W, H, HEADER_H, FOOTER_H = 320, 480, 44, 52
CONTENT_Y, CONTENT_H = HEADER_H, H - HEADER_H - FOOTER_H

def c565(v):
    r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)

C_BG, C_PANEL, C_HEADER, C_TEXT, C_MUTED = map(c565, (0x0861, 0x18E3, 0x2124, 0xFFFF, 0xAD55))
C_ACCENT, C_WARN_BG, C_LINE, C_GREEN = map(c565, (0xFD20, 0x4A00, 0x3186, 0x07E9))

FD = "/usr/share/fonts/truetype/dejavu/"
def F(name, size):
    try:
        return ImageFont.truetype(FD + name, size)
    except OSError:
        return ImageFont.load_default()
SANS9 = F("DejaVuSansCondensed.ttf", 15)       # ~ FreeSans9pt7b
BOLD9 = F("DejaVuSansCondensed-Bold.ttf", 15)  # ~ FreeSansBold9pt7b
BOLD12 = F("DejaVuSansCondensed-Bold.ttf", 20) # ~ FreeSansBold12pt7b
BOLD18 = F("DejaVuSansCondensed-Bold.ttf", 30) # ~ FreeSansBold18pt7b
SMALL = F("DejaVuSans.ttf", 11)                # ~ built-in font 2

def tw(font, s):
    return font.getlength(s)

def fit(font, s, maxw):
    if tw(font, s) <= maxw:
        return s
    while len(s) > 1 and tw(font, s + "...") > maxw:
        s = s[:-1]
    return s.rstrip() + "..."

def ascii_only(s):
    rep = {"°": "", "‘": "'", "’": "'", "“": '"', "”": '"', "–": "-", "—": "-",
           "…": "...", "→": "->", "≈": "~", "≥": ">=", "≤": "<=", "×": "x", " ": " "}
    return "".join(rep.get(ch, ch if ord(ch) < 128 else "?") for ch in s)

class Screen:
    def __init__(self):
        self.im = Image.new("RGB", (W, H), C_BG)
        self.d = ImageDraw.Draw(self.im)

    def header(self, title, back):
        d = self.d
        d.rectangle([0, 0, W, HEADER_H - 1], fill=C_HEADER)
        d.line([(0, HEADER_H - 1), (W, HEADER_H - 1)], fill=C_ACCENT)
        tx = 12
        if back:
            self.tri("L", 22, HEADER_H // 2, C_ACCENT); tx = 44
        maxw = W - tx - (12 if back else 80)
        font = BOLD12
        t = fit(font, title, maxw)
        if t.endswith("..."):
            font = BOLD9; t = fit(font, title, maxw)
        d.text((tx, HEADER_H // 2), t, font=font, fill=C_TEXT, anchor="lm")
        if not back:
            self.wifi_badge(1)

    def wifi_badge(self, clients):
        d = self.d; cx, cy = W - 74 + 16, 30
        for r in (4, 9, 14):
            d.arc([cx - r, cy - r, cx + r, cy + r], 225, 315, fill=C_GREEN, width=2)
        d.ellipse([cx - 2, cy - 2, cx + 2, cy + 2], fill=C_GREEN)
        d.text((cx + 14, 24), f"{clients} on", font=SMALL, fill=C_TEXT, anchor="lm")

    def tri(self, kind, cx, cy, col):
        pts = {"U": [(cx - 10, cy + 6), (cx + 10, cy + 6), (cx, cy - 7)], "D": [(cx - 10, cy - 6), (cx + 10, cy - 6), (cx, cy + 7)],
               "L": [(cx + 6, cy - 10), (cx + 6, cy + 10), (cx - 7, cy)], "R": [(cx - 6, cy - 10), (cx - 6, cy + 10), (cx + 7, cy)]}[kind]
        self.d.polygon(pts, fill=col)

    def button(self, x, y, w, h, label="", icon=None, enabled=True):
        d = self.d
        d.rounded_rectangle([x + 2, y + 2, x + w - 3, y + h - 3], 6, fill=C_PANEL)
        fg = C_TEXT if enabled else C_LINE
        cx, cy = x + w // 2, y + h // 2
        if icon and label:
            tot = 18 + 6 + tw(BOLD9, label); ix = int(cx - tot / 2 + 9)
            self.tri(icon, ix, cy, C_ACCENT if enabled else C_LINE)
            d.text((ix + 15, cy), label, font=BOLD9, fill=fg, anchor="lm")
        elif icon:
            self.tri(icon, cx, cy, C_ACCENT if enabled else C_LINE)
        else:
            d.text((cx, cy), label, font=BOLD9, fill=fg, anchor="mm")

    def footer(self, defs):
        y = H - FOOTER_H
        self.d.rectangle([0, y, W, H], fill=C_HEADER)
        n = len(defs); w = W // n
        for i, (label, icon, en, plain) in enumerate(defs):
            x = i * w; ww = W - x if i == n - 1 else w
            if plain:
                self.d.text((x + ww // 2, y + FOOTER_H // 2), label, font=SMALL, fill=C_MUTED, anchor="mm")
            else:
                self.button(x, y + 2, ww, FOOTER_H - 4, label, icon, en)

    def save(self, name):
        self.im.save(os.path.join(OUT, name))
        return os.path.join(OUT, name)

def read_tsv(path):
    rows = []
    if os.path.exists(path):
        for l in open(path, encoding="utf-8"):
            l = l.rstrip("\n")
            if l and not l.startswith("#"):
                rows.append([p.strip() for p in l.split("\t")])
    return rows

cats = read_tsv(os.path.join(SD, "wiki/categories.tsv"))
catalog = read_tsv(os.path.join(SD, "wiki/catalog.tsv"))

def hexrgb(h):
    h = h.lstrip("#"); return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

def cat_icon(d, cid, cx, cy):
    w = (255, 255, 255)
    if cid == "water":
        d.ellipse([cx - 8, cy - 4, cx + 8, cy + 12], fill=w); d.polygon([(cx - 8, cy + 2), (cx + 8, cy + 2), (cx, cy - 12)], fill=w)
    elif cid == "fire":
        d.polygon([(cx - 9, cy + 9), (cx + 9, cy + 9), (cx, cy - 12)], fill=w)
        d.polygon([(cx - 9, cy + 9), (cx + 1, cy + 9), (cx - 6, cy - 4)], fill=w); d.polygon([(cx - 1, cy + 9), (cx + 9, cy + 9), (cx + 7, cy - 6)], fill=w)
    elif cid == "shelter":
        d.polygon([(cx - 13, cy + 9), (cx + 13, cy + 9), (cx, cy - 11)], fill=w); d.polygon([(cx - 3, cy + 9), (cx + 3, cy + 9), (cx, cy)], fill=C_BG)
    elif cid == "firstaid":
        d.rectangle([cx - 3, cy - 11, cx + 3, cy + 10], fill=w); d.rectangle([cx - 11, cy - 3, cx + 10, cy + 3], fill=w)
    elif cid == "food":
        d.ellipse([cx - 13, cy - 6, cx + 9, cy + 6], fill=w); d.polygon([(cx + 7, cy), (cx + 14, cy - 7), (cx + 14, cy + 7)], fill=w)
    elif cid == "navigation":
        d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], outline=w)
        d.polygon([(cx - 4, cy), (cx + 4, cy), (cx, cy - 9)], fill=(255, 0, 0)); d.polygon([(cx - 4, cy), (cx + 4, cy), (cx, cy + 9)], fill=w)
    elif cid == "signals":
        d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], fill=w)
        for r in (7, 13): d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=w)
    elif cid == "basics":
        d.ellipse([cx - 11, cy - 11, cx + 11, cy + 11], outline=w)
        d.rectangle([cx - 1, cy - 7, cx + 1, cy + 1], fill=w); d.rectangle([cx - 1, cy + 4, cx + 1, cy + 6], fill=w)

def home():
    s = Screen(); d = s.d
    s.header("SURVIVAL WIKI", False)
    cols, rows, gap = 2, 4, 8
    tile_w = (W - gap * 3) // 2; tile_h = (CONTENT_H - gap * 5) // 4
    for i, c in enumerate(cats[:8]):
        x = gap + (i % 2) * (tile_w + gap); y = CONTENT_Y + gap + (i // 2) * (tile_h + gap)
        d.rounded_rectangle([x, y, x + tile_w - 1, y + tile_h - 1], 10, fill=hexrgb(c[2]))
        cat_icon(d, c[0], x + 24, y + 26)
        d.text((x + 10, y + tile_h - 40), fit(BOLD9, c[1], tile_w - 16), font=BOLD9, fill=(255, 255, 255))
        n = sum(1 for a in catalog if a[1] == c[0])
        d.text((x + 10, y + tile_h - 20), f"{n} article" + ("" if n == 1 else "s"), font=SMALL, fill=(255, 255, 255))
    s.footer([("Library", None, True, False), ("Wi-Fi", None, True, False), ("Settings", None, True, False)])
    return s.save("01_home.png")

def list_screen(title, entries, name, books=False):
    s = Screen(); d = s.d
    s.header(title, True)
    for r, (t, extra) in enumerate(entries[:8]):
        y = CONTENT_Y + r * 48
        if books:
            d.text((12, y + 17), fit(SANS9, t, W - 50), font=SANS9, fill=C_TEXT, anchor="lm")
            d.text((12, y + 36), extra + " parts", font=SMALL, fill=C_MUTED, anchor="lm")
        else:
            d.text((12, y + 24), fit(SANS9, t, W - 50), font=SANS9, fill=C_TEXT, anchor="lm")
        s.tri("R", W - 20, y + 24, C_LINE)
        d.line([(8, y + 47), (W - 8, y + 47)], fill=C_LINE)
    pages = max(1, (len(entries) + 7) // 8)
    s.footer([("Back", "L", True, False), ("", "U", False, False), (f"1 / {pages}", None, True, True), ("", "D", pages > 1, False)])
    return s.save(name)

# ---------------- article renderer (mirrors doc.cpp) ----------------
LH = {"H1": 34, "H2": 28, "CAP": 18}
FONT = {"H1": BOLD12, "H2": BOLD9, "CAP": SMALL}

def layout(text):
    text = ascii_only(text).replace("\r", "")
    text = re.sub(r"\*\*|__|`", "", text)
    text = re.sub(r"(?<!!)\[([^\]\n]+)\]\([^)\n]+\)", r"\1", text)
    items, y = [], 8
    def wrap(s, x0, width, style):
        nonlocal y
        font = FONT.get(style, SANS9); lh = LH.get(style, 22)
        words = s.split(); line = ""
        for wd in words:
            cand = (line + " " + wd).strip()
            if tw(font, cand) <= width or not line:
                line = cand
            else:
                items.append((style, line, x0, y, lh)); y += lh; line = wd
        if line:
            items.append((style, line, x0, y, lh)); y += lh
    lines = text.split("\n"); i = 0
    if lines and lines[0].strip() == "---":
        i = 1
        while i < len(lines) and lines[i].strip() != "---":
            if lines[i].startswith("title:"):
                wrap(lines[i][6:].strip(), 10, 300, "H1"); y += 4
            i += 1
        i += 1
    last_blank, in_warn = True, False
    for raw in lines[i:]:
        l = raw.strip()
        is_warn = l.startswith(">")
        if in_warn and not is_warn:
            items.append(("WPAD", "", 0, y, 8)); y += 14; in_warn = False
        if not l:
            if not last_blank: y += 8
            last_blank = True; continue
        last_blank = False
        if is_warn:
            if not in_warn:
                y += 2; items.append(("WPAD", "", 0, y, 8)); y += 8; in_warn = True
            wrap(l[1:].strip(), 22, W - 40, "WARN")
        elif l.startswith("#"):
            h1 = not l.startswith("##"); y += 8 if h1 else 6
            wrap(l.lstrip("#").strip(), 10, 300, "H1" if h1 else "H2")
        elif l.startswith("---"):
            items.append(("RULE", "", 0, y, 16)); y += 16
        elif re.match(r"^[-*] ", l):
            items.append(("DOT", "", 14, y, 22)); wrap(l[2:], 28, W - 38, "BODY")
        elif re.match(r"^\d+\. ", l):
            m = re.match(r"^(\d+\.) (.*)", l)
            items.append(("NUM", m.group(1), 10, y, 22)); wrap(m.group(2), 32, W - 42, "BODY")
        elif l.startswith("!["):
            m = re.match(r"!\[([^\]]*)\]\(([^)]+)\)", l)
            if m:
                path = os.path.join(SD, "wiki", m.group(2)); y += 4
                if os.path.exists(path):
                    im = Image.open(path); sc = 1
                    while im.width // sc > 300 and sc < 8: sc *= 2
                    dw, dh = im.width // sc, im.height // sc
                    items.append(("IMG", path, (W - dw) // 2, y, dh)); y += dh + 4
                else:
                    items.append(("MISSING", "", 0, y, 40)); y += 44
                if m.group(1): wrap(m.group(1), 10, 300, "CAP")
                y += 6
        else:
            wrap(l, 10, 300, "BODY")
    if in_warn:
        items.append(("WPAD", "", 0, y, 8)); y += 8
    return items, y + 16

def article(aid, scroll_to=None, page=0, name=None):
    text = open(os.path.join(SD, "wiki/articles", aid + ".md"), encoding="utf-8").read()
    title = next((r[2] for r in catalog if r[0] == aid), aid)
    items, total = layout(text)
    max_scroll = max(0, total - CONTENT_H)
    scroll = 0
    if scroll_to == "image":
        img_y = next((it[3] for it in items if it[0] == "IMG"), 0)
        scroll = min(max_scroll, max(0, img_y - 40))
    else:
        scroll = min(max_scroll, page * (CONTENT_H - 44))
    s = Screen(); s.header(ascii_only(title), True)
    view = Image.new("RGB", (W, CONTENT_H), C_BG); d = ImageDraw.Draw(view)
    for style, txt, x, y0, h in items:
        y = y0 - scroll
        if y + h < 0 or y > CONTENT_H: continue
        if style in ("WPAD", "WARN"):
            d.rectangle([10, y, W - 11, y + h - 1], fill=C_WARN_BG); d.rectangle([10, y, 13, y + h - 1], fill=C_ACCENT)
            if style == "WARN": d.text((x, y + 4), txt, font=SANS9, fill=C_TEXT)
        elif style == "RULE":
            d.line([(10, y + 8), (310, y + 8)], fill=C_LINE)
        elif style == "DOT":
            d.ellipse([x + 1, y + 8, x + 7, y + 14], fill=C_ACCENT)
        elif style == "NUM":
            d.text((x, y + 4), txt, font=SANS9, fill=C_ACCENT)
        elif style == "IMG":
            im = Image.open(txt).convert("RGB"); sc = 1
            while im.width // sc > 300 and sc < 8: sc *= 2
            view.paste(im.resize((im.width // sc, im.height // sc), Image.BILINEAR), (x, y))
        elif style == "MISSING":
            d.rectangle([10, y, 310, y + 36], outline=C_LINE); d.text((18, y + 10), "[picture missing]", font=SMALL, fill=C_MUTED)
        else:
            col = C_ACCENT if style == "H2" else C_MUTED if style == "CAP" else C_TEXT
            d.text((x, y + (1 if style == "CAP" else 4)), txt, font=FONT.get(style, SANS9), fill=col)
    s.im.paste(view, (0, CONTENT_Y))
    pct = int(scroll * 100 / max_scroll) if max_scroll else 100
    s.footer([("Back", "L", True, False), ("", "U", scroll > 0, False), (f"{pct}%", None, True, True), ("", "D", scroll < max_scroll, False)])
    return s.save(name or f"article_{aid}.png")

def wifi_screen(ssid="SurvivalWiki", password=""):
    s = Screen(); d = s.d
    s.header("Phone Access", True)
    y = CONTENT_Y + 10
    d.text((W // 2, y), "Scan with a phone camera to join", font=SMALL, fill=C_MUTED, anchor="ma")
    qtext = f"WIFI:T:WPA;S:{ssid};P:{password};;" if len(password) >= 8 else f"WIFI:T:nopass;S:{ssid};;"
    qr = QrCode.encode_text(qtext, QrCode.Ecc.MEDIUM)
    size = qr.get_size(); scale = 180 // (size + 4); px = (size + 4) * scale
    ox, oy = 70 + (180 - px) // 2, y + 22
    d.rectangle([ox, oy, ox + px - 1, oy + px - 1], fill=(255, 255, 255))
    for r in range(size):
        for c in range(size):
            if qr.get_module(c, r):
                d.rectangle([ox + (c + 2) * scale, oy + (r + 2) * scale, ox + (c + 3) * scale - 1, oy + (r + 3) * scale - 1], fill=(0, 0, 0))
    y += 214
    for k, v, col in (("Network", ssid, C_TEXT), ("Password", password if len(password) >= 8 else "none (open)", C_TEXT),
                      ("Address", "http://192.168.4.1", C_ACCENT), ("Phones", "2 connected", C_TEXT)):
        d.text((16, y), k, font=SANS9, fill=C_MUTED); d.text((112, y), v, font=SANS9, fill=col); y += 24
    s.footer([("Back", "L", True, False), ("Turn Wi-Fi off", None, True, False)])
    return s.save("05_wifi.png")

def settings_screen():
    s = Screen(); d = s.d
    s.header("Settings", True)
    y = CONTENT_Y + 4
    def row(name, value=""):
        d.text((14, y + 24), name, font=SANS9, fill=C_TEXT, anchor="lm")
        if value: d.text((W - 110, y + 24), value, font=SANS9, fill=C_ACCENT, anchor="rm")
        d.line([(8, y + 51), (W - 8, y + 51)], fill=C_LINE)
    row("Brightness", "78%"); s.button(W - 104, y + 4, 48, 44, "-"); s.button(W - 54, y + 4, 48, 44, "+"); y += 52
    row("Screen off after"); s.button(W - 104, y + 4, 98, 44, "3 min"); y += 52
    row("Invert colors"); s.button(W - 104, y + 4, 98, 44, "Off"); y += 52
    row("Touch calibration"); s.button(W - 104, y + 4, 98, 44, "Redo"); y += 64
    for i, t in enumerate(["Survival Wiki firmware 1.0.0", f"{len(catalog)} articles in {len(cats)} categories",
                           "SD card: 9 MB used of 15193 MB", "Free memory: 212 KB"]):
        d.text((14, y + i * 20), t, font=SMALL, fill=C_MUTED)
    s.footer([("Back", "L", True, False)])
    return s.save("07_settings.png")

if __name__ == "__main__":
    out = [home()]
    water = [(ascii_only(a[2]), "") for a in catalog if a[1] == "water"]
    out.append(list_screen("Water", water, "02_category.png"))
    out.append(article("purifying-water", name="03_article.png"))
    out.append(article("improvised-filter", scroll_to="image", name="04_article_picture.png"))
    out.append(wifi_screen())
    books = [(ascii_only(b[1]), b[2]) for b in read_tsv(os.path.join(SD, "library/catalog.tsv"))]
    out.append(list_screen("Library", books, "06_library.png", books=True))
    out.append(settings_screen())
    for aid in sys.argv[3:]:
        out.append(article(aid))
    print("\n".join(out))
