#!/usr/bin/env python3
"""Draws the starter diagrams for the Survival Wiki (600 px wide baseline JPEGs).

    python3 tools/make_diagrams.py sdcard/wiki/img
"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont

OUT = sys.argv[1] if len(sys.argv) > 1 else "sdcard/wiki/img"
os.makedirs(OUT, exist_ok=True)

W = 600
PAPER = (246, 242, 233)
INK = (34, 34, 38)
MUTED = (110, 108, 104)
ORANGE = (230, 110, 20)
RED = (200, 40, 35)
BLUE = (40, 110, 200)
LBLUE = (170, 205, 240)
GREEN = (60, 130, 60)
LGREEN = (150, 190, 120)
BROWN = (120, 80, 45)
SOIL = (170, 135, 95)
SAND = (225, 200, 150)
GREY = (150, 150, 150)
NIGHT = (18, 26, 52)
STAR = (255, 240, 190)

FONT_DIR = "/usr/share/fonts/truetype/dejavu/"
def font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    for d in (FONT_DIR, "/Library/Fonts/", "C:/Windows/Fonts/"):
        try:
            return ImageFont.truetype(d + name, size)
        except OSError:
            pass
    return ImageFont.load_default()

F_T = font(30, True)     # title
F_B = font(24, True)     # bold label
F_L = font(22)           # label
F_S = font(19)           # small note

def canvas(h, bg=PAPER):
    im = Image.new("RGB", (W, h), bg)
    return im, ImageDraw.Draw(im)

def title(d, text, color=INK, y=14):
    d.text((W // 2, y), text, font=F_T, fill=color, anchor="ma")

def label(d, xy, text, f=F_L, fill=INK, anchor="la"):
    d.text(xy, text, font=f, fill=fill, anchor=anchor)

def arrow(d, a, b, fill=INK, width=4, head=14):
    d.line([a, b], fill=fill, width=width)
    ang = math.atan2(b[1] - a[1], b[0] - a[0])
    p1 = (b[0] - head * math.cos(ang - 0.45), b[1] - head * math.sin(ang - 0.45))
    p2 = (b[0] - head * math.cos(ang + 0.45), b[1] - head * math.sin(ang + 0.45))
    d.polygon([b, p1, p2], fill=fill)

def save(im, name):
    im.save(os.path.join(OUT, name), "JPEG", quality=82, optimize=True, progressive=False)
    print("wrote", name, im.size)

# ------------------------------------------------------------------ basics
def rule_of_threes():
    im, d = canvas(400)
    title(d, "The Rule of 3s")
    rows = [("AIR", "3 minutes", RED, 0.12), ("SHELTER", "3 hours in harsh weather", ORANGE, 0.35),
            ("WATER", "3 days", BLUE, 0.62), ("FOOD", "3 weeks", GREEN, 0.95)]
    y = 70
    for name, txt, col, frac in rows:
        d.rounded_rectangle([20, y, 170, y + 62], 10, fill=col)
        label(d, (95, y + 31), name, F_B, (255, 255, 255), "mm")
        bw = 390
        tint = tuple(int(c + (255 - c) * 0.75) for c in col)
        d.rounded_rectangle([185, y + 8, 185 + max(bw, 30), y + 54], 8, fill=tint, outline=col, width=3)
        label(d, (195, y + 31), txt, F_L, INK, "lm")
        y += 76
    label(d, (W // 2, 384), "Rough limits - deal with problems in this order", F_S, MUTED, "ms")
    save(im, "rule_of_threes.jpg")

# ------------------------------------------------------------------ water
def water_filter():
    im, d = canvas(560)
    title(d, "Improvised Gravity Filter")
    x0, x1 = 150, 330
    layers = [("Cloth / grass", (200, 190, 170), 40), ("Fine sand", SAND, 70), ("Crushed charcoal", (60, 60, 60), 60),
              ("Fine sand", SAND, 50), ("Small gravel", (160, 155, 145), 50), ("Cloth", (200, 190, 170), 24)]
    y = 110
    d.polygon([(x0 - 10, 90), (x1 + 10, 90), (x1, 100), (x0, 100)], fill=GREY)
    for name, col, h in layers:
        d.rectangle([x0, y, x1, y + h], fill=col, outline=INK)
        if name == "Small gravel":
            for i in range(14):
                cx = x0 + 12 + (i * 37) % (x1 - x0 - 20); cy = y + 12 + (i * 17) % (h - 20)
                d.ellipse([cx - 6, cy - 5, cx + 6, cy + 5], fill=(120, 115, 105))
        label(d, (x1 + 26, y + h // 2), name, F_L, INK, "lm")
        d.line([(x1 + 4, y + h // 2), (x1 + 20, y + h // 2)], fill=MUTED, width=2)
        y += h
    d.rectangle([x0, 110, x1, y], outline=INK, width=4)
    arrow(d, (110, 88), (x0 + 30, 112), BLUE, 5)
    label(d, (20, 52), "Dirty water", F_B, BLUE)
    for i in range(3):
        d.ellipse([235, y + 12 + i * 22, 245, y + 26 + i * 22], fill=BLUE)
    d.rectangle([185, y + 70, 295, y + 110], outline=INK, width=3, fill=LBLUE)
    label(d, (W // 2, 550), "Clears dirt only. ALWAYS boil or disinfect after.", F_S, RED, "ms")
    save(im, "water_filter.jpg")

def solar_still():
    im, d = canvas(420)
    title(d, "Solar Still")
    d.rectangle([0, 150, W, 420], fill=SOIL)
    d.polygon([(110, 150), (490, 150), (420, 330), (180, 330)], fill=(120, 90, 60))
    for x in (150, 445):
        for k in range(4):
            d.line([(x + k * 8 - 12, 330 - k * 40), (x + k * 8, 300 - k * 40)], fill=GREEN, width=5)
    d.rectangle([265, 285, 335, 330], fill=LBLUE, outline=INK, width=3)
    label(d, (300, 352), "Container", F_S, INK, "ma")
    d.line([(95, 150), (300, 260), (505, 150)], fill=(90, 160, 210), width=5)
    d.ellipse([288, 238, 312, 262], fill=GREY, outline=INK)
    label(d, (330, 228), "Small rock", F_S, INK)
    for x in (190, 230, 370, 410):
        y = 150 + int(abs(x - 300) * -0.53 + 110)
        d.ellipse([x - 4, y + 4, x + 4, y + 14], fill=BLUE)
    d.rectangle([60, 140, 120, 158], fill=SOIL); d.rectangle([480, 140, 540, 158], fill=SOIL)
    label(d, (20, 112), "Seal edges with soil", F_S, INK)
    label(d, (395, 112), "Clear plastic", F_S, INK)
    d.ellipse([500, 50, 560, 110], fill=(250, 200, 40))
    label(d, (W // 2, 410), "Yields little (often under 0.5 L a day). Use with other sources.", F_S, INK, "ms")
    save(im, "solar_still.jpg")

# ------------------------------------------------------------------ fire
def fire_triangle():
    im, d = canvas(380)
    title(d, "The Fire Triangle")
    a, b, c = (300, 70), (120, 310), (480, 310)
    d.polygon([a, b, c], outline=ORANGE, width=10)
    flame = [(300, 150), (340, 230), (325, 290), (300, 300), (275, 290), (260, 230)]
    d.polygon(flame, fill=ORANGE)
    d.polygon([(300, 210), (318, 260), (300, 290), (282, 260)], fill=(250, 210, 60))
    label(d, (430, 170), "HEAT", F_B, RED)
    label(d, (60, 170), "OXYGEN", F_B, BLUE)
    label(d, (300, 322), "FUEL", F_B, BROWN, "ma")
    label(d, (W // 2, 378), "Remove any side and the fire dies", F_S, MUTED, "ms")
    save(im, "fire_triangle.jpg")

def fire_lays():
    im, d = canvas(380)
    title(d, "Two Reliable Fire Lays")
    # teepee
    cx, base = 160, 300
    d.ellipse([cx - 30, base - 30, cx + 30, base + 5], fill=SAND)
    for k in range(-5, 6):
        d.line([(cx + k * 16, base), (cx + k * 2, 110)], fill=BROWN, width=7)
    label(d, (cx, 330), "Teepee", F_B, INK, "ma")
    label(d, (cx, 356), "fast, hot start", F_S, MUTED, "ma")
    # log cabin
    cx = 440
    d.ellipse([cx - 25, base - 40, cx + 25, base - 5], fill=SAND)
    for i in range(5):
        y = base - i * 26
        if i % 2 == 0:
            d.rectangle([cx - 85, y - 12, cx - 65, y + 4], fill=BROWN); d.rectangle([cx + 65, y - 12, cx + 85, y + 4], fill=BROWN)
            d.line([(cx - 95, y - 4), (cx + 95, y - 4)], fill=BROWN, width=12)
        else:
            d.line([(cx - 75, y - 4), (cx - 75, y - 4)], fill=BROWN)
            d.rectangle([cx - 85, y - 14, cx - 65, y + 2], fill=(150, 100, 55)); d.rectangle([cx + 65, y - 14, cx + 85, y + 2], fill=(150, 100, 55))
    label(d, (cx, 330), "Log cabin", F_B, INK, "ma")
    label(d, (cx, 356), "long, steady burn", F_S, MUTED, "ma")
    label(d, (300, 70), "Tinder goes in the center", F_S, ORANGE, "ma")
    save(im, "fire_lays.jpg")

def fire_materials():
    im, d = canvas(330)
    title(d, "Tinder, Kindling, Fuel")
    # tinder fluff
    for i in range(40):
        x = 60 + (i * 53) % 110; y = 110 + (i * 29) % 70
        d.arc([x - 14, y - 10, x + 14, y + 10], 0, 300, fill=(180, 150, 100), width=2)
    label(d, (115, 220), "Tinder", F_B, INK, "ma")
    label(d, (115, 250), "dry grass, bark", F_S, MUTED, "ma"); label(d, (115, 274), "fluff, cotton", F_S, MUTED, "ma")
    for i in range(7):
        d.line([(240 + i * 8, 190), (290 + i * 8, 100)], fill=BROWN, width=5)
    label(d, (300, 220), "Kindling", F_B, INK, "ma")
    label(d, (300, 250), "match to", F_S, MUTED, "ma"); label(d, (300, 274), "pencil thick", F_S, MUTED, "ma")
    for i in range(3):
        d.line([(420 + i * 28, 195), (500 + i * 28, 95)], fill=BROWN, width=22)
    label(d, (485, 220), "Fuel", F_B, INK, "ma")
    label(d, (485, 250), "thumb to", F_S, MUTED, "ma"); label(d, (485, 274), "wrist thick", F_S, MUTED, "ma")
    label(d, (W // 2, 322), "Gather twice as much as you think you need", F_S, ORANGE, "ms")
    save(im, "fire_materials.jpg")

# ------------------------------------------------------------------ shelter
def lean_to():
    im, d = canvas(400)
    title(d, "Lean-To with Fire Reflector")
    g = 330
    d.rectangle([0, g, W, 400], fill=SOIL)
    for x in (110, 300):
        d.rectangle([x - 10, 80, x + 10, g], fill=BROWN)
        d.ellipse([x - 45, 50, x + 45, 120], fill=LGREEN)
    d.line([(95, 150), (330, 150)], fill=BROWN, width=12)
    d.polygon([(110, 150), (300, 150), (420, g), (230, g)], fill=(110, 150, 80))
    for k in range(8):
        d.line([(120 + k * 24, 152), (240 + k * 24, g)], fill=BROWN, width=4)
    d.rectangle([60, g - 8, 400, g], fill=(140, 170, 100))
    label(d, (130, g + 18), "Insulated bed", F_S, INK)
    for k in range(5):
        d.line([(520, g - k * 18), (560, g - k * 18)], fill=BROWN, width=12)
    d.polygon([(470, g), (480, g - 40), (490, g - 15), (500, g - 50), (510, g)], fill=ORANGE)
    label(d, (440, 210), "Reflector", F_S, INK)
    arrow(d, (480, 230), (535, g - 60), MUTED, 3, 10)
    label(d, (430, 100), "Back to the wind", F_S, BLUE)
    arrow(d, (580, 135), (440, 135), BLUE, 3, 10)
    save(im, "lean_to.jpg")

def debris_hut():
    im, d = canvas(400)
    title(d, "Debris Hut (cross-section)")
    g = 330
    d.rectangle([0, g, W, 400], fill=SOIL)
    d.polygon([(110, g), (300, 90), (490, g)], fill=(130, 100, 60))
    d.polygon([(200, g), (300, 200), (400, g)], fill=PAPER)
    d.polygon([(200, g), (300, 200), (400, g)], outline=BROWN, width=6)
    d.rectangle([200, g - 22, 400, g], fill=(150, 175, 100))
    label(d, (300, g - 11), "Thick leaf bed", F_S, INK, "mm")
    d.ellipse([290, 192, 310, 212], fill=BROWN)
    label(d, (390, 120), "Leaves / debris", F_S, INK)
    label(d, (390, 145), "60-90 cm thick", F_B, ORANGE)
    arrow(d, (400, 175), (370, 205), MUTED, 3, 10)
    label(d, (40, 140), "Ridge pole", F_S, INK)
    arrow(d, (110, 165), (285, 202), MUTED, 3, 10)
    label(d, (W // 2, 392), "Just big enough to lie in - small spaces stay warm", F_S, INK, "ms")
    save(im, "debris_hut.jpg")

def tarp_shelter():
    im, d = canvas(380)
    title(d, "Tarp A-Frame")
    g = 320
    d.rectangle([0, g, W, 380], fill=SOIL)
    for x in (70, 530):
        d.rectangle([x - 10, 70, x + 10, g], fill=BROWN)
    d.line([(70, 130), (530, 130)], fill=INK, width=3)
    d.polygon([(130, 130), (470, 130), (520, g - 10), (80, g - 10)], fill=(60, 120, 160))
    d.line([(300, 130), (300, g - 10)], fill=(40, 90, 130), width=3)
    for x in (80, 520):
        d.line([(x, g - 10), (x + (-20 if x < 300 else 20), g + 8)], fill=INK, width=3)
    label(d, (300, 100), "Ridgeline (taut)", F_S, INK, "ma")
    label(d, (300, g + 20), "Stake corners low on the windy side", F_S, INK, "ma")
    save(im, "tarp_shelter.jpg")

# ------------------------------------------------------------------ signals
def ground_signals():
    im, d = canvas(520)
    title(d, "Ground-to-Air Signals")
    items = [("V", "Need help"), ("X", "Need medical help"), ("N", "No"), ("Y", "Yes"), ("ARROW", "Going this way")]
    for i, (sym, txt) in enumerate(items):
        x = 110; y = 62 + i * 78
        d.rounded_rectangle([x, y, x + 70, y + 70], 8, fill=(90, 140, 70))
        c = (255, 255, 255)
        if sym == "ARROW":
            d.line([(x + 35, y + 60), (x + 35, y + 18)], fill=c, width=9)
            d.polygon([(x + 35, y + 8), (x + 18, y + 30), (x + 52, y + 30)], fill=c)
        else:
            label(d, (x + 35, y + 36), sym, font(52, True), c, "mm")
        label(d, (x + 90, y + 35), txt, F_B, INK, "lm")
    label(d, (W // 2, 482), "Make each symbol 3 m (10 ft) or larger,", F_S, MUTED, "ms")
    label(d, (W // 2, 506), "in a color that contrasts with the ground.", F_S, MUTED, "ms")
    save(im, "ground_signals.jpg")

def sos_morse():
    im, d = canvas(300)
    title(d, "SOS in Morse Code")
    x = 34
    for grp, letter in (("s", "S"), ("l", "O"), ("s", "S")):
        start = x
        for _ in range(3):
            if grp == "s":
                d.ellipse([x, 120, x + 26, 146], fill=ORANGE); x += 40
            else:
                d.rounded_rectangle([x, 120, x + 60, 146], 12, fill=ORANGE); x += 76
        label(d, ((start + x - 18) // 2, 170), letter, F_T, INK, "ma")
        x += 26
    label(d, (W // 2, 230), "3 short - 3 long - 3 short, pause, repeat", F_L, INK, "ma")
    label(d, (W // 2, 270), "Use a flashlight, whistle, mirror or horn", F_S, MUTED, "ma")
    save(im, "sos_morse.jpg")

def signal_mirror():
    im, d = canvas(400)
    title(d, "Aiming a Signal Mirror")
    d.polygon([(470, 175), (520, 160), (560, 180), (520, 187)], fill=GREY)
    label(d, (515, 200), "Target", F_S, INK, "ma")
    # arm + V fingers
    d.line([(150, 330), (330, 200)], fill=(205, 160, 120), width=26)
    d.line([(330, 200), (395, 140)], fill=(205, 160, 120), width=12)
    d.line([(330, 200), (405, 170)], fill=(205, 160, 120), width=12)
    # mirror at eye
    d.rectangle([60, 300, 120, 360], fill=LBLUE, outline=INK, width=3)
    label(d, (40, 370), "Mirror under eye", F_S, INK)
    d.line([(95, 320), (510, 175)], fill=(250, 200, 40), width=3)
    label(d, (24, 66), "1. Frame the target in a V of your fingers", F_S, INK)
    label(d, (24, 92), "2. Tilt the mirror until the light spot hits the V", F_S, INK)
    label(d, (420, 270), "Sweep slowly", F_B, ORANGE)
    label(d, (420, 300), "across the target", F_S, MUTED)
    save(im, "signal_mirror.jpg")

# ------------------------------------------------------------------ navigation
def shadow_stick():
    im, d = canvas(400)
    title(d, "Shadow-Stick Method")
    d.rectangle([0, 80, W, 400], fill=SAND)
    sx, sy = 300, 250
    d.line([(sx, sy), (sx, sy - 70)], fill=BROWN, width=8)
    p1, p2 = (150, 300), (420, 310)
    d.line([(sx, sy), p1], fill=(120, 110, 90), width=4)
    d.line([(sx, sy), p2], fill=(150, 140, 115), width=4)
    for p, t in ((p1, "1st mark"), (p2, "2nd mark (15 min later)")):
        d.ellipse([p[0] - 10, p[1] - 10, p[0] + 10, p[1] + 10], fill=RED)
    label(d, (p1[0] - 10, p1[1] + 18), "1st mark", F_S, INK, "ma")
    label(d, (p2[0], p2[1] + 18), "2nd mark, 15 min later", F_S, INK, "ma")
    arrow(d, (80, 295), (520, 316), INK, 4)
    label(d, (40, 280), "W", F_T, INK, "mm"); label(d, (560, 318), "E", F_T, INK, "mm")
    arrow(d, (300, 380), (300, 110), BLUE, 4)
    label(d, (300, 92), "N", F_T, BLUE, "mm")
    label(d, (330, 140), "N-S is at right angles", F_S, BLUE)
    save(im, "shadow_stick.jpg")

def watch_method():
    im, d = canvas(420)
    title(d, "Finding South with a Watch")
    cx, cy, r = 300, 230, 140
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(255, 255, 255), outline=INK, width=6)
    for h in range(12):
        a = math.radians(h * 30 - 90)
        d.line([(cx + (r - 18) * math.cos(a), cy + (r - 18) * math.sin(a)), (cx + (r - 6) * math.cos(a), cy + (r - 6) * math.sin(a))], fill=INK, width=4)
    label(d, (cx, cy - r + 34), "12", F_B, INK, "mm")
    ha = math.radians(4 * 30 - 90)
    d.line([(cx, cy), (cx + 90 * math.cos(ha), cy + 90 * math.sin(ha))], fill=INK, width=10)
    sa = math.radians(2 * 30 - 90)
    d.line([(cx, cy), (cx + (r + 40) * math.cos(sa), cy + (r + 40) * math.sin(sa))], fill=BLUE, width=5)
    sx, sy = cx + 235 * math.cos(ha), cy + 235 * math.sin(ha)
    d.ellipse([sx - 26, sy - 26, sx + 26, sy + 26], fill=(250, 200, 40))
    label(d, (sx - 20, sy - 60), "Point hour", F_S, INK, "ma")
    label(d, (sx - 20, sy - 38), "hand at sun", F_S, INK, "ma")
    ex, ey = cx + (r + 60) * math.cos(sa), cy + (r + 60) * math.sin(sa)
    label(d, (ex, ey - 6), "SOUTH", F_B, BLUE, "mm")
    label(d, (W // 2, 400), "South = halfway between the hour hand and 12", F_S, INK, "ms")
    label(d, (W // 2, 418), "(use 1 instead of 12 during daylight saving time)", F_S, MUTED, "ms")
    save(im, "watch_method.jpg")

def stars(d, pts, r=6):
    for x, y in pts:
        d.ellipse([x - r, y - r, x + r, y + r], fill=STAR)

def north_star():
    im, d = canvas(420, NIGHT)
    title(d, "Finding the North Star", STAR)
    # bowl: Phecda, Merak, Dubhe, Megrez, then handle
    A, B, C, D = (350, 345), (420, 335), (423, 290), (358, 300)
    handle = [(300, 282), (250, 255), (190, 262)]
    d.line([A, B, C, D, A], fill=(80, 90, 140), width=2)
    d.line([D] + handle, fill=(80, 90, 140), width=2)
    stars(d, [A, B, C, D] + handle)
    pol = (C[0] + (C[0] - B[0]) * 5, C[1] + (C[1] - B[1]) * 5)   # 5x the pointer gap
    for k in range(0, 100, 6):  # dashed guide line
        t0, t1 = k / 100, min(1, (k + 3) / 100)
        d.line([(C[0] + (pol[0] - C[0]) * t0, C[1] + (pol[1] - C[1]) * t0),
                (C[0] + (pol[0] - C[0]) * t1, C[1] + (pol[1] - C[1]) * t1)], fill=(250, 200, 40), width=3)
    stars(d, [pol], 10)
    label(d, (pol[0] - 20, pol[1]), "Polaris (North Star)", F_B, STAR, "rm")
    label(d, (440, 312), "Pointers", F_S, (250, 200, 40), "lm")
    label(d, (200, 300), "Big Dipper", F_S, (200, 200, 230))
    label(d, (40, 110), "Go about 5 times the gap", F_S, (200, 200, 230))
    label(d, (40, 134), "between the two pointer stars", F_S, (200, 200, 230))
    label(d, (W // 2, 408), "Face Polaris = facing north", F_L, STAR, "ms")
    save(im, "north_star.jpg")

def southern_cross():
    im, d = canvas(420, NIGHT)
    title(d, "Southern Cross (South)", STAR)
    top, bot, left, right = (380, 70), (395, 150), (355, 115), (418, 102)
    d.line([top, bot], fill=(80, 90, 140), width=2); d.line([left, right], fill=(80, 90, 140), width=2)
    stars(d, [top, bot, left, right], 7)
    pole = (int(bot[0] + (bot[0] - top[0]) * 2.4), int(bot[1] + (bot[1] - top[1]) * 2.4))
    for k in range(0, 100, 6):
        t0, t1 = k / 100, min(1, (k + 3) / 100)
        d.line([(bot[0] + (pole[0] - bot[0]) * t0, bot[1] + (pole[1] - bot[1]) * t0),
                (bot[0] + (pole[0] - bot[0]) * t1, bot[1] + (pole[1] - bot[1]) * t1)], fill=(250, 200, 40), width=3)
    d.ellipse([pole[0] - 7, pole[1] - 7, pole[0] + 7, pole[1] + 7], outline=(250, 200, 40), width=3)
    label(d, (pole[0] - 18, pole[1]), "South pole of the sky", F_S, STAR, "rm")
    stars(d, [(200, 175), (255, 195)], 7)
    label(d, (150, 215), "Pointer stars", F_S, (200, 200, 230))
    label(d, (40, 90), "Extend the long axis", F_S, (200, 200, 230))
    label(d, (40, 114), "about 4.5 times its length", F_S, (200, 200, 230))
    label(d, (40, 138), "(shortened here)", F_S, (150, 150, 190))
    label(d, (W // 2, 412), "Drop straight down to the horizon = south", F_L, STAR, "ms")
    save(im, "southern_cross.jpg")

# ------------------------------------------------------------------ first aid
def cpr():
    im, d = canvas(420)
    title(d, "Hands-Only CPR")
    # simple torso outline
    d.ellipse([230, 60, 330, 150], fill=(230, 190, 160), outline=INK, width=3)
    d.rounded_rectangle([170, 150, 390, 400], 40, fill=(230, 190, 160), outline=INK, width=3)
    d.line([(280, 175), (280, 300)], fill=(200, 160, 130), width=3)
    d.ellipse([250, 215, 310, 265], fill=RED)
    label(d, (280, 240), "", F_S)
    arrow(d, (440, 170), (318, 236), INK, 4)
    label(d, (420, 120), "Heel of hand,", F_S, INK)
    label(d, (420, 144), "center of chest", F_S, INK)
    label(d, (30, 300), "Push hard", F_B, RED); label(d, (30, 330), "and fast", F_B, RED)
    label(d, (420, 300), "100-120", F_B, INK); label(d, (420, 330), "per minute", F_S, INK)
    label(d, (420, 360), "5-6 cm deep", F_B, INK)
    label(d, (30, 380), "Let chest rise", F_S, MUTED)
    save(im, "cpr.jpg")

def tourniquet():
    im, d = canvas(380)
    title(d, "Tourniquet Placement")
    d.rounded_rectangle([40, 160, 560, 240], 38, fill=(230, 190, 160), outline=INK, width=3)
    d.ellipse([500, 150, 580, 250], fill=(230, 190, 160), outline=INK, width=3)
    d.ellipse([400, 185, 440, 215], fill=RED)
    label(d, (420, 260), "Wound", F_S, RED, "ma")
    d.rectangle([300, 150, 330, 250], fill=(40, 40, 40))
    d.line([(315, 120), (315, 280)], fill=(90, 90, 90), width=8)
    label(d, (250, 290), "5-7 cm above the wound", F_B, INK, "ma")
    label(d, (250, 320), "never on a joint - tighten until bleeding stops", F_S, INK, "ma")
    label(d, (200, 110), "Write the time it went on", F_B, ORANGE, "ma")
    d.ellipse([170, 185, 200, 215], outline=MUTED, width=3)
    label(d, (185, 260), "elbow", F_S, MUTED, "ma")
    save(im, "tourniquet.jpg")


# ------------------------------------------------------------------ batch 2
def ice_1101():
    im, d = canvas(400)
    title(d, "The 1-10-1 Rule")
    rows = [("1", "minute", "Control your breathing.", "Cold shock passes.", BLUE),
            ("10", "minutes", "Of useful movement.", "Get yourself out now.", ORANGE),
            ("1", "hour", "Before you pass out", "from cold. Keep arms on the ice.", RED)]
    y = 70
    for big, unit, l1, l2, col in rows:
        d.rounded_rectangle([20, y, 190, y + 92], 12, fill=col)
        label(d, (105, y + 38), big, font(50, True), (255, 255, 255), "mm")
        label(d, (105, y + 76), unit, F_S, (255, 255, 255), "mm")
        label(d, (210, y + 30), l1, F_B, INK, "lm")
        label(d, (210, y + 62), l2, F_L, INK, "lm")
        y += 106
    save(im, "ice_1101.jpg")

def tick_removal():
    im, d = canvas(380)
    title(d, "Removing a Tick")
    sy = 250
    d.rectangle([0, sy, W, 380], fill=(230, 190, 160))
    d.line([(0, sy), (W, sy)], fill=(190, 140, 110), width=4)
    # tick
    cx = 300
    d.ellipse([cx - 22, sy - 64, cx + 22, sy - 14], fill=(90, 50, 30))
    d.ellipse([cx - 9, sy - 20, cx + 9, sy - 2], fill=(60, 30, 20))
    for k in (-1, 1):
        for j in range(4):
            yy = sy - 54 + j * 11
            d.line([(cx + k * 20, yy), (cx + k * 42, yy - 8 + j * 6)], fill=(60, 30, 20), width=3)
    # tweezers gripping at skin
    d.polygon([(cx - 14, sy - 6), (cx - 6, sy - 6), (cx - 60, sy - 210), (cx - 74, sy - 206)], fill=GREY, outline=INK)
    d.polygon([(cx + 6, sy - 6), (cx + 14, sy - 6), (cx + 74, sy - 206), (cx + 60, sy - 210)], fill=GREY, outline=INK)
    arrow(d, (470, 220), (470, 90), BLUE, 6, 18)
    label(d, (490, 150), "Pull", F_B, BLUE)
    label(d, (490, 178), "straight up", F_S, BLUE)
    label(d, (30, 90), "Grip close", F_B, INK)
    label(d, (30, 118), "to the skin", F_B, INK)
    arrow(d, (130, 150), (cx - 18, sy - 8), MUTED, 3, 10)
    label(d, (W // 2, 312), "Slow, steady pressure. No twisting or jerking.", F_L, INK, "ma")
    label(d, (W // 2, 344), "No heat, nail polish or petroleum jelly.", F_S, RED, "ma")
    save(im, "tick_removal.jpg")

def ors_recipe():
    im, d = canvas(380)
    title(d, "Rehydration Drink")
    items = [("1 liter", "clean, safe water", BLUE), ("6 level tsp", "sugar", (200, 160, 60)), ("1/2 level tsp", "salt", GREY)]
    x = 20
    for big, small, col in items:
        d.rounded_rectangle([x, 80, x + 175, 250], 14, fill=PAPER, outline=col, width=5)
        if small.startswith("clean"):
            d.rectangle([x + 60, 102, x + 115, 175], outline=BLUE, width=4, fill=LBLUE)
        else:
            if small == "sugar":
                for i in range(6):
                    cx = x + 40 + (i % 3) * 48; cy = 120 + (i // 3) * 38
                    d.ellipse([cx - 16, cy - 10, cx + 16, cy + 10], fill=col, outline=INK)
            else:  # one teaspoon, half full
                cx, cy = x + 87, 138
                d.ellipse([cx - 26, cy - 16, cx + 26, cy + 16], fill=PAPER, outline=INK, width=2)
                d.chord([cx - 26, cy - 16, cx + 26, cy + 16], 0, 180, fill=col)
                d.ellipse([cx - 26, cy - 16, cx + 26, cy + 16], outline=INK, width=2)
                d.line([(cx + 26, cy), (cx + 70, cy - 4)], fill=INK, width=5)
        label(d, (x + 87, 200), big, F_B, INK, "mm")
        label(d, (x + 87, 228), small, F_S, MUTED, "mm")
        if x < 300:
            label(d, (x + 192, 165), "+", F_T, INK, "mm")
        x += 195
    label(d, (W // 2, 290), "Stir until dissolved. Small sips, often.", F_L, INK, "ma")
    label(d, (W // 2, 326), "Measure carefully - too much salt is harmful.", F_S, RED, "ma")
    save(im, "ors_recipe.jpg")

def hand_sunset():
    im, d = canvas(420, (250, 225, 190))
    for i in range(160):   # sky gradient
        c = (255, 200 - i // 2, 140 - i // 3)
        d.line([(0, 60 + i * 2), (W, 60 + i * 2)], fill=c, width=2)
    title(d, "Daylight Left: Use Your Hand", INK)
    hz = 340
    d.rectangle([0, hz, W, 420], fill=(70, 90, 60))
    # hand: 4 fingers stacked horizontally above horizon
    fx0, fx1, fh = 190, 360, 34
    for i in range(4):
        y1 = hz - i * fh
        d.rounded_rectangle([fx0, y1 - fh + 2, fx1, y1], 15, fill=(225, 180, 145), outline=(150, 110, 85), width=2)
    d.rounded_rectangle([fx1 - 30, hz - 4 * fh, fx1 + 30, hz + 30], 20, fill=(225, 180, 145), outline=(150, 110, 85), width=2)
    sun_y = hz - 4 * fh - 26
    d.ellipse([fx0 + 40, sun_y - 24, fx0 + 88, sun_y + 24], fill=(255, 210, 40))
    for i in range(4):
        y1 = hz - i * fh - fh // 2
        label(d, (fx0 - 14, y1), f"{15 * (i + 1)} min", F_S, INK, "rm")
    label(d, (430, 150), "Each finger", F_B, INK)
    label(d, (430, 180), "~ 15 minutes", F_L, INK)
    label(d, (430, 215), "4 fingers", F_B, INK)
    label(d, (430, 245), "~ 1 hour", F_L, INK)
    label(d, (W // 2, 386), "Arm straight out, bottom finger on the horizon", F_S, (255, 255, 255), "mm")
    save(im, "hand_sunset.jpg")

for fn in (ice_1101, tick_removal, ors_recipe, hand_sunset, rule_of_threes, water_filter, solar_still, fire_triangle, fire_lays, fire_materials, lean_to, debris_hut,
           tarp_shelter, ground_signals, sos_morse, signal_mirror, shadow_stick, watch_method, north_star,
           southern_cross, cpr, tourniquet):
    fn()
