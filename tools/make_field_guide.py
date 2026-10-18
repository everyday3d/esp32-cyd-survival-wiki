#!/usr/bin/env python3
"""Bundle every wiki article into one book for the Library, plus a printable PDF.

    python3 tools/make_field_guide.py sdcard

Writes sdcard/library/field-guide/ (text parts the device pages through) and,
if reportlab is installed (pip install reportlab), field-guide.pdf with all
the pictures - print it as a paper backup for when batteries run out.
"""
import os, re, sys
ROOT = sys.argv[1] if len(sys.argv) > 1 else "sdcard"
WIKI = os.path.join(ROOT, "wiki")
DEST = os.path.join(ROOT, "library", "field-guide")
PART_CHARS = 9000
BOOK_ID, TITLE = "field-guide", "Survival Wiki Field Guide (all articles)"

def tsv(path):
    return [l.rstrip("\n").split("\t") for l in open(path, encoding="utf-8") if l.strip() and not l.startswith("#")]

def split_front(text):
    meta = {}
    if text.startswith("---"):
        end = text.find("\n---", 4)
        for line in text[4:end].splitlines():
            if ":" in line:
                k, v = line.split(":", 1); meta[k.strip()] = v.strip()
        text = text[text.find("\n", end + 1) + 1:]
    return meta, text

cats = tsv(os.path.join(WIKI, "categories.tsv"))
catalog = tsv(os.path.join(WIKI, "catalog.tsv"))
articles = []
for c in cats:
    for a in catalog:
        if a[1] == c[0]:
            meta, body = split_front(open(os.path.join(WIKI, "articles", a[0] + ".md"), encoding="utf-8").read())
            articles.append((c, a[0], meta.get("title", a[2]), body))

# ---------------- plain text parts for the device / phone reader
def to_plain(body):
    out = []
    for l in body.splitlines():
        l = l.strip()
        if not l:
            out.append(""); continue
        if l.startswith("!["):
            m = re.match(r"!\[([^\]]*)\]", l); out.append(f"[Picture: {m.group(1)}]" if m else ""); continue
        l = re.sub(r"\*\*|__|`", "", l)
        l = re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", l)
        if l.startswith("#"):
            l = l.lstrip("#").strip().upper()
        elif l.startswith(">"):
            l = "WARNING: " + l.lstrip("> ").strip()
        elif re.match(r"^[-*] ", l):
            l = "- " + l[2:]
        out.append(l)
    text = "\n".join(out)
    return [p.strip() for p in re.split(r"\n\s*\n", text) if p.strip()]

os.makedirs(DEST, exist_ok=True)
for f in os.listdir(DEST):
    if f.endswith(".txt"):
        os.remove(os.path.join(DEST, f))
paras = []
current = None
for c, aid, title, body in articles:
    if c[0] != current:
        paras.append(f"=== {c[1].upper()} ==="); current = c[0]
    paras.append(title.upper())
    paras.extend(to_plain(body))
parts, cur, size = [], [], 0
for p in paras:
    if size + len(p) > PART_CHARS and cur:
        parts.append(cur); cur, size = [], 0
    cur.append(p); size += len(p) + 2
parts.append(cur)
for i, part in enumerate(parts, 1):
    with open(os.path.join(DEST, f"{i:04d}.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n\n".join(part) + "\n")

# ---------------- printable PDF
pdf_name = ""
try:
    from reportlab.lib.pagesizes import A5
    from reportlab.lib.units import mm
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.lib import colors
    from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Image as RLImage, PageBreak,
                                    ListFlowable, ListItem, Table, TableStyle, KeepTogether)
    from xml.sax.saxutils import escape

    ss = getSampleStyleSheet()
    body_st = ParagraphStyle("b", parent=ss["BodyText"], fontSize=9.5, leading=12.5, spaceAfter=4)
    h1 = ParagraphStyle("h1", parent=ss["Heading1"], fontSize=17, leading=20, spaceAfter=6, textColor=colors.HexColor("#1C2A22"))
    h2 = ParagraphStyle("h2", parent=ss["Heading2"], fontSize=11.5, leading=14, spaceBefore=8, spaceAfter=3, textColor=colors.HexColor("#B4470A"))
    cat_st = ParagraphStyle("c", parent=h1, fontSize=24, leading=28, spaceBefore=60)
    cap = ParagraphStyle("cap", parent=body_st, fontSize=8, textColor=colors.grey)
    warn_st = ParagraphStyle("w", parent=body_st, fontSize=9, leading=12)

    def inline(s):
        s = escape(s)
        s = re.sub(r"\*\*([^*]+)\*\*", r"<b>\1</b>", s)
        s = re.sub(r"`([^`]+)`", r"<font face='Courier'>\1</font>", s)
        return re.sub(r"\[([^\]]+)\]\([^)]+\)", r"\1", s)

    def flow(body, width):
        story, items, kind, para = [], [], None, []
        def flush_list():
            nonlocal items, kind
            if items:
                story.append(ListFlowable([ListItem(Paragraph(inline(t), body_st), leftIndent=12) for t in items],
                                          bulletType="1" if kind == "ol" else "bullet", leftIndent=12, bulletFontSize=8))
            items, kind = [], None
        def flush_para():
            nonlocal para
            if para: story.append(Paragraph(inline(" ".join(para)), body_st)); para = []
        for raw in body.splitlines():
            l = raw.strip()
            if not l:
                flush_para(); flush_list(); continue
            m_ul, m_ol = re.match(r"^[-*] (.*)", l), re.match(r"^\d+\. (.*)", l)
            if m_ul or m_ol:
                flush_para(); k = "ul" if m_ul else "ol"
                if kind and kind != k: flush_list()
                kind = k; items.append((m_ul or m_ol).group(1)); continue
            flush_list()
            if l.startswith("#"):
                flush_para(); story.append(Paragraph(inline(l.lstrip("#").strip()), h2))
            elif l.startswith(">"):
                flush_para()
                t = Table([[Paragraph(inline(l.lstrip("> ")), warn_st)]], colWidths=[width])
                t.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#FCEBDD")),
                                       ("LINEBEFORE", (0, 0), (0, -1), 3, colors.HexColor("#E2580B")),
                                       ("LEFTPADDING", (0, 0), (-1, -1), 6), ("TOPPADDING", (0, 0), (-1, -1), 4),
                                       ("BOTTOMPADDING", (0, 0), (-1, -1), 4)]))
                story += [Spacer(1, 3), t, Spacer(1, 3)]
            elif l.startswith("!["):
                flush_para()
                m = re.match(r"!\[([^\]]*)\]\(([^)]+)\)", l)
                p = os.path.join(WIKI, m.group(2))
                if os.path.exists(p):
                    from PIL import Image as PI
                    w, h = PI.open(p).size
                    iw = min(width, 105 * mm)
                    story.append(KeepTogether([RLImage(p, iw, iw * h / w), Paragraph(escape(m.group(1)), cap)]))
            else:
                para.append(l)
        flush_para(); flush_list()
        return story

    pdf_name = BOOK_ID + ".pdf"
    doc = SimpleDocTemplate(os.path.join(DEST, pdf_name), pagesize=A5, leftMargin=12 * mm, rightMargin=12 * mm,
                            topMargin=12 * mm, bottomMargin=14 * mm, title=TITLE)
    width = A5[0] - 24 * mm
    story = [Spacer(1, 50 * mm), Paragraph("Survival Wiki", ParagraphStyle("t", parent=h1, fontSize=30, leading=34)),
             Paragraph("Field Guide", ParagraphStyle("t2", parent=h1, fontSize=20, textColor=colors.HexColor("#B4470A"))),
             Spacer(1, 10 * mm), Paragraph("A printed copy of every article on the device. Keep it with the device "
             "as a backup for when batteries run out.", body_st), Spacer(1, 6 * mm),
             Paragraph("This is a field reference, not a substitute for training or professional medical care.", cap), PageBreak()]
    current = None
    for c, aid, title, body in articles:
        if c[0] != current:
            if current: story.append(PageBreak())
            story.append(Paragraph(escape(c[1]), cat_st)); story.append(Paragraph(escape(c[3] if len(c) > 3 else ""), body_st))
            story.append(PageBreak()); current = c[0]
        story.append(Paragraph(escape(title), h1))
        story += flow(body, width)
        story.append(Spacer(1, 8 * mm))
    def footer(canvas, d):
        canvas.setFont("Helvetica", 7); canvas.setFillColor(colors.grey)
        canvas.drawCentredString(A5[0] / 2, 7 * mm, f"Survival Wiki Field Guide  -  {d.page}")
    doc.build(story, onFirstPage=lambda c, d: None, onLaterPages=footer)
except ImportError:
    print("reportlab not installed - skipped the printable PDF (pip install reportlab)")

cat = os.path.join(ROOT, "library", "catalog.tsv")
rows = [l.rstrip("\n") for l in open(cat, encoding="utf-8")] if os.path.exists(cat) else []
rows = [r for r in rows if r and (r.startswith("#") or r.split("\t")[0] != BOOK_ID)]
if not rows or not rows[0].startswith("#"):
    rows.insert(0, "# id\ttitle\tparts\tpdf file\tsource  (written by import_manual.py)")
rows.insert(1, "\t".join([BOOK_ID, TITLE, str(len(parts)), pdf_name, "Every article in one book. Print the PDF as a paper backup."]))
open(cat, "w", encoding="utf-8", newline="\n").write("\n".join(rows) + "\n")
print(f"Field guide: {len(articles)} articles, {len(parts)} parts" + (f", printable {pdf_name}" if pdf_name else ""))
