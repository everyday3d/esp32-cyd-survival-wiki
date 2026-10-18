#!/usr/bin/env python3
"""Survival Wiki content builder.

Run this after adding or editing articles, then copy the sdcard folder to the card.

    python3 tools/build_content.py sdcard

It will:
  * read wiki/categories.tsv and every wiki/articles/*.md
  * write wiki/catalog.tsv (what the device lists) and wiki/search.json(.gz)
  * check every picture exists and resize/convert it so the device can show it
    (baseline JPEG, max 600 px wide)
  * check links between articles
  * gzip the web page for faster loading on phones
Needs Python 3 and Pillow (pip install pillow).
"""
import gzip, json, os, re, sys

try:
    from PIL import Image
except ImportError:
    Image = None

ROOT = sys.argv[1] if len(sys.argv) > 1 else "sdcard"
WIKI = os.path.join(ROOT, "wiki")
ART = os.path.join(WIKI, "articles")
IMG = os.path.join(WIKI, "img")
MAX_W = 600
MAX_ARTICLE_BYTES = 24000   # device loads one article into a 24 KB buffer

problems = []
def warn(msg):
    problems.append(msg)
    print("  !", msg)

def front_matter(text):
    meta, body = {}, text
    if text.startswith("---\n") or text.startswith("---\r\n"):
        end = text.find("\n---", 4)
        if end > 0:
            for line in text[4:end].splitlines():
                if ":" in line:
                    k, v = line.split(":", 1)
                    meta[k.strip()] = v.strip()
            body = text[text.find("\n", end + 1) + 1:]
    return meta, body

def plain(body):
    t = re.sub(r"!\[([^\]]*)\]\([^)]*\)", r"\1", body)
    t = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", t)
    t = re.sub(r"[*_`#>]", "", t)
    t = re.sub(r"^\s*[-]\s+", "", t, flags=re.M)
    return re.sub(r"\s+", " ", t).strip()

def fix_image(path):
    if Image is None:
        return
    with Image.open(path) as im:
        progressive = im.info.get("progressive") or im.info.get("progression")
        needs = im.format != "JPEG" or im.width > MAX_W or progressive or im.mode not in ("RGB", "L")
        if not needs:
            return
        im = im.convert("RGB")
        if im.width > MAX_W:
            im = im.resize((MAX_W, round(im.height * MAX_W / im.width)), Image.LANCZOS)
        out = os.path.splitext(path)[0] + ".jpg"
        im.save(out, "JPEG", quality=82, optimize=True, progressive=False)
    if out != path:
        os.remove(path)
        warn(f"converted {os.path.basename(path)} to {os.path.basename(out)} - update the article to use .jpg")
    else:
        print("  resized/converted", os.path.basename(path))

def main():
    print("Building content in", ROOT)
    cats = []
    with open(os.path.join(WIKI, "categories.tsv"), encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line and not line.startswith("#"):
                cats.append(line.split("\t"))
    cat_ids = [c[0] for c in cats]
    if len(cats) > 8:
        warn("the device home screen shows the first 8 categories only")

    # pictures first, so references can be checked afterwards
    if os.path.isdir(IMG):
        for name in sorted(os.listdir(IMG)):
            if name.lower().endswith((".jpg", ".jpeg", ".png", ".webp", ".gif", ".bmp")):
                fix_image(os.path.join(IMG, name))

    articles = []
    for name in sorted(os.listdir(ART)):
        if not name.endswith(".md"):
            continue
        aid = name[:-3]
        if not re.fullmatch(r"[a-z0-9-]+", aid):
            warn(f"{name}: use only lowercase letters, numbers and dashes in file names")
        path = os.path.join(ART, name)
        raw = open(path, "rb").read()
        if len(raw) > MAX_ARTICLE_BYTES:
            warn(f"{name}: {len(raw)} bytes - the device shows only the first {MAX_ARTICLE_BYTES}; split it")
        text = raw.decode("utf-8")
        meta, body = front_matter(text)
        if not meta.get("title"):
            warn(f"{name}: missing 'title:' in the front matter")
        if meta.get("category") not in cat_ids:
            warn(f"{name}: category '{meta.get('category')}' is not in categories.tsv")
        for img in re.findall(r"!\[[^\]]*\]\(([^)]+)\)", body):
            if not os.path.exists(os.path.join(WIKI, img)):
                warn(f"{name}: picture not found: {img}")
            elif not img.lower().endswith((".jpg", ".jpeg")):
                warn(f"{name}: {img} - the device can only show JPEG pictures")
        for link in re.findall(r"(?<!!)\[[^\]]+\]\(([^)]+\.md)\)", body):
            if not os.path.exists(os.path.join(ART, link)):
                warn(f"{name}: link to missing article {link}")
        try:
            order = int(meta.get("order", "99"))
        except ValueError:
            order = 99
        articles.append({"id": aid, "c": meta.get("category", ""), "t": meta.get("title", aid),
                         "s": meta.get("summary", ""), "k": meta.get("tags", ""), "x": plain(body), "o": order})

    articles.sort(key=lambda a: (cat_ids.index(a["c"]) if a["c"] in cat_ids else 99, a["o"], a["t"].lower()))

    with open(os.path.join(WIKI, "catalog.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# id\tcategory\ttitle  (generated by build_content.py - do not edit)\n")
        for a in articles:
            f.write(f"{a['id']}\t{a['c']}\t{a['t']}\n")

    index = [{k: a[k] for k in ("id", "c", "t", "s", "k", "x")} for a in articles]
    data = json.dumps(index, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    # The device serves the .gz copy when the plain file is absent (smaller and faster over Wi-Fi).
    with gzip.open(os.path.join(WIKI, "search.json.gz"), "wb", compresslevel=9) as f:
        f.write(data)
    plain_path = os.path.join(WIKI, "search.json")
    if os.path.exists(plain_path):
        os.remove(plain_path)

    web = os.path.join(ROOT, "web", "index.html")
    if os.path.exists(web):
        with open(web, "rb") as src, gzip.open(web + ".gz", "wb", compresslevel=9) as dst:
            dst.write(src.read())

    lib = os.path.join(ROOT, "library", "catalog.tsv")
    if not os.path.exists(lib):
        os.makedirs(os.path.dirname(lib), exist_ok=True)
        with open(lib, "w", encoding="utf-8", newline="\n") as f:
            f.write("# id\ttitle\tparts\tpdf file\tsource  (written by import_manual.py)\n")

    counts = {c: sum(1 for a in articles if a["c"] == c) for c in cat_ids}
    print(f"\n{len(articles)} articles: " + ", ".join(f"{c} {n}" for c, n in counts.items()))
    print(f"search index {len(data)//1024} KB (gzipped on card)")
    if problems:
        print(f"\n{len(problems)} problem(s) to look at - see the ! lines above.")
        sys.exit(1)
    print("All good. Copy the contents of the", ROOT, "folder to the root of the SD card.")

if __name__ == "__main__":
    main()
