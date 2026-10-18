#!/usr/bin/env python3
"""Add a full manual (PDF or text file) to the Survival Wiki library.

    python3 tools/import_manual.py FM21-76.pdf --id fm21-76 --title "US Army Survival Manual (FM 21-76)" \
        --source "US Army, 1992. Public domain."

What it does:
  * copies the PDF to sdcard/library/<id>/ so phones can open the original
  * extracts the text, rejoins broken lines into paragraphs, and splits it into
    numbered parts (0001.txt, 0002.txt, ...) that the device screen can page through
  * adds or updates the entry in sdcard/library/catalog.tsv

Needs:  pip install pypdf      (only for PDF input)
Good public-domain choices: US Army FM 21-76 "Survival" (1992), FM 3-05.70 "Survival" (2002),
and FEMA's "Are You Ready?" guide. Only add material you have the right to copy.
"""
import argparse, os, re, shutil, sys

PART_CHARS = 9000    # device loads up to 24 KB per part; ~9 KB is about 12 screens

def extract_pdf(path):
    try:
        from pypdf import PdfReader
    except ImportError:
        sys.exit("Please install pypdf first:  pip install pypdf")
    reader = PdfReader(path)
    pages = []
    for i, page in enumerate(reader.pages):
        try:
            pages.append(page.extract_text() or "")
        except Exception as e:  # damaged page: keep going
            print(f"  page {i + 1}: could not read ({e})")
            pages.append("")
    print(f"  read {len(pages)} pages")
    return "\n\n".join(pages)

def reflow(text):
    """Join lines broken by the PDF layout back into paragraphs."""
    text = text.replace("\r", "")
    text = re.sub(r"[ \t]+", " ", text)
    lens = sorted(len(l.strip()) for l in text.split("\n") if len(l.strip()) > 20)
    typical = lens[len(lens) // 2] if lens else 60
    out, para = [], []
    for raw in text.split("\n"):
        line = raw.strip()
        if not line:
            if para:
                out.append(" ".join(para)); para = []
            continue
        if re.fullmatch(r"(page\s*)?[\divxlcm\-\s]{1,8}", line, flags=re.I):   # page numbers
            continue
        if para:
            prev = para[-1]
            if prev.endswith("-") and line[:1].islower():
                para[-1] = prev[:-1] + line
                continue
            # short heading-like lines or list starts begin a new paragraph
            # a line much shorter than usual ends a paragraph (or is a heading)
            if len(prev) < 0.7 * typical and (line[:1].isupper() or line[:1].isdigit()):
                out.append(" ".join(para)); para = []
            elif re.match(r"^([-*•]|\d+\.|[a-z]\.)\s", line):
                out.append(" ".join(para)); para = []
        para.append(line)
    if para:
        out.append(" ".join(para))
    clean = []
    for p in out:
        p = p.replace("•", "-").strip()
        if len(p) > 1:
            clean.append(p)
    return clean

def split_parts(paras):
    parts, cur, size = [], [], 0
    for p in paras:
        while len(p) > PART_CHARS:            # giant paragraph: cut at a sentence
            cut = p.rfind(". ", 0, PART_CHARS) + 1 or PART_CHARS
            if cur:
                parts.append(cur); cur, size = [], 0
            parts.append([p[:cut].strip()])
            p = p[cut:].strip()
        if size + len(p) > PART_CHARS and cur:
            parts.append(cur); cur, size = [], 0
        cur.append(p); size += len(p) + 2
    if cur:
        parts.append(cur)
    return parts

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("file", help="PDF or .txt file")
    ap.add_argument("--id", required=True, help="short folder name, e.g. fm21-76")
    ap.add_argument("--title", required=True)
    ap.add_argument("--source", default="", help="author, year, license note")
    ap.add_argument("--sdcard", default="sdcard")
    ap.add_argument("--no-pdf", action="store_true", help="do not copy the PDF (saves space)")
    a = ap.parse_args()

    if not re.fullmatch(r"[a-z0-9-]+", a.id):
        sys.exit("--id must use only lowercase letters, numbers and dashes")
    lib = os.path.join(a.sdcard, "library")
    dest = os.path.join(lib, a.id)
    if os.path.isdir(dest):
        for f in os.listdir(dest):
            if f.endswith(".txt"):
                os.remove(os.path.join(dest, f))
    os.makedirs(dest, exist_ok=True)

    print("Importing", a.file)
    is_pdf = a.file.lower().endswith(".pdf")
    text = extract_pdf(a.file) if is_pdf else open(a.file, encoding="utf-8", errors="replace").read()
    paras = reflow(text)
    if not paras:
        sys.exit("No text found. Scanned PDFs need OCR first (for example with 'ocrmypdf').")
    parts = split_parts(paras)
    for i, part in enumerate(parts, 1):
        with open(os.path.join(dest, f"{i:04d}.txt"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n\n".join(part) + "\n")
    pdf_name = ""
    if is_pdf and not a.no_pdf:
        pdf_name = a.id + ".pdf"
        shutil.copyfile(a.file, os.path.join(dest, pdf_name))

    cat = os.path.join(lib, "catalog.tsv")
    rows = []
    if os.path.exists(cat):
        rows = [l.rstrip("\n") for l in open(cat, encoding="utf-8")]
    rows = [r for r in rows if r and (r.startswith("#") or r.split("\t")[0] != a.id)]
    if not rows or not rows[0].startswith("#"):
        rows.insert(0, "# id\ttitle\tparts\tpdf file\tsource  (written by import_manual.py)")
    clean = lambda s: s.replace("\t", " ").strip()
    rows.append("\t".join([a.id, clean(a.title), str(len(parts)), pdf_name, clean(a.source)]))
    with open(cat, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(rows) + "\n")
    print(f"Done: {len(paras)} paragraphs in {len(parts)} parts -> {dest}")

if __name__ == "__main__":
    main()
