#include "l10n.h"

#ifdef LANG_TH
#include <SD.h>
#include "config.h"

extern L10nTft tft;

struct AtlasEntry { uint32_t key; uint32_t off; uint16_t w, h; };
static AtlasEntry *atlas = nullptr;
static uint32_t nAtlas = 0;
static File atlasFile;

enum { TS_S, TS_SB, TS_MB, TS_LB, TS_XL };

bool l10nActive() { return atlas != nullptr && nAtlas > 0; }

bool l10nBegin() {
  if (atlas) { free(atlas); atlas = nullptr; nAtlas = 0; }
  if (atlasFile) atlasFile.close();
  atlasFile = SD.open(SCR_W < 300 ? "/th/atlas28.thb" : "/th/atlas35.thb", FILE_READ);
  if (!atlasFile) return false;
  uint8_t hdr[8];
  if (atlasFile.read(hdr, 8) != 8 || memcmp(hdr, "THA1", 4) != 0) { atlasFile.close(); return false; }
  uint32_t n = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
  if (n == 0 || n > 20000) { atlasFile.close(); return false; }
  atlas = (AtlasEntry *)malloc(n * sizeof(AtlasEntry));
  if (!atlas) { atlasFile.close(); return false; }
  for (uint32_t i = 0; i < n; i++) {
    uint8_t r[12];
    if (atlasFile.read(r, 12) != 12) { free(atlas); atlas = nullptr; atlasFile.close(); return false; }
    atlas[i].key = r[0] | (r[1] << 8) | (r[2] << 16) | ((uint32_t)r[3] << 24);
    atlas[i].off = r[4] | (r[5] << 8) | (r[6] << 16) | ((uint32_t)r[7] << 24);
    atlas[i].w = r[8] | (r[9] << 8);
    atlas[i].h = r[10] | (r[11] << 8);
  }
  nAtlas = n;
  Serial.printf("[th] Thai pack loaded: %lu pictures\n", (unsigned long)n);
  return true;
}

static uint32_t fnv(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) { h ^= (uint8_t)*s; h *= 16777619u; }
  return h;
}

static const AtlasEntry *findEntry(const char *s, uint8_t style) {
  if (!atlas || !s || !*s) return nullptr;
  uint32_t key = fnv(s) + (uint32_t)style * 2654435761u;
  uint32_t lo = 0, hi = nAtlas;
  while (lo < hi) {
    uint32_t mid = (lo + hi) / 2;
    if (atlas[mid].key < key) lo = mid + 1; else hi = mid;
  }
  return (lo < nAtlas && atlas[lo].key == key) ? &atlas[lo] : nullptr;
}

bool l10nHas(const char *s) { return findEntry(s, TS_S) != nullptr; }

// Draws a 4-bit alpha picture over the background, using the text colour. The background colour is the
// text background if the code set one, otherwise whatever is on the screen at the left edge of the text.
static int16_t drawAtlas(L10nTft &t, const AtlasEntry *e, int32_t x, int32_t y) {
  const int w = e->w, h = e->h;
  uint8_t d = t.textdatum;
  int hz = d < 9 ? d % 3 : d - 9, vt = d < 9 ? d / 3 : 2;
  int x0 = hz == 0 ? x : (hz == 1 ? x - w / 2 : x - w);
  int y0 = vt == 0 ? y : (vt == 1 ? y - h / 2 : y - h);
  uint16_t fg = (uint16_t)t.textcolor, bg = (uint16_t)t.textbgcolor;
  uint16_t pad = t.getTextPadding();
  if (bg == fg) bg = t.readPixel(constrain(x0 + 1, 0, SCR_W - 1), constrain(y0 + h / 2, 0, SCR_H - 1));
  else if (pad > 0) {                                        // clear the old text, like the normal fonts do
    int px0 = hz == 0 ? x : (hz == 1 ? x - pad / 2 : x - pad);
    t.fillRect(max(0, px0), max(0, y0), min<int>(pad, SCR_W - max(0, px0)), h, bg);
  }
  static uint8_t raw[8 * 160];
  static uint16_t line[8 * 320];
  const int rs = (w + 1) / 2;
  if (w > 320 || rs > 160) return w;
  for (int r0 = 0; r0 < h; r0 += 8) {
    int rows = min(8, h - r0);
    atlasFile.seek(e->off + (uint32_t)r0 * rs);
    atlasFile.read(raw, rows * rs);
    for (int r = 0; r < rows; r++) {
      for (int c = 0; c < w; c++) {
        uint8_t b = raw[r * rs + (c >> 1)];
        uint8_t a = (c & 1) ? (b & 15) : (b >> 4);
        line[r * w + c] = a == 0 ? bg : (a == 15 ? fg : t.alphaBlend(a * 17, fg, bg));
      }
    }
    int yy = y0 + r0, cx0 = x0, cw = w, skip = 0;
    if (cx0 < 0) { skip = -cx0; cw -= skip; cx0 = 0; }
    if (cx0 + cw > SCR_W) cw = SCR_W - cx0;
    if (cw <= 0 || yy >= SCR_H) continue;
    if (skip == 0 && cw == w) t.pushImage(cx0, yy, w, rows, line);
    else for (int r = 0; r < rows; r++) t.pushImage(cx0, yy + r, cw, 1, line + r * w + skip);
  }
  return w;
}

int16_t L10nTft::drawString(const char *s, int32_t x, int32_t y) {
  const AtlasEntry *e = findEntry(s, thStyle);
  if (e) return drawAtlas(*this, e, x, y);
  return TFT_eSPI::drawString(s, x, y);
}
int16_t L10nTft::drawString(const String &s, int32_t x, int32_t y) { return drawString(s.c_str(), x, y); }
int16_t L10nTft::textWidth(const char *s) {
  const AtlasEntry *e = findEntry(s, thStyle);
  return e ? e->w : TFT_eSPI::textWidth(s);
}
int16_t L10nTft::textWidth(const String &s) { return textWidth(s.c_str()); }

void L10nTft::setFreeFont(const GFXfont *f) {
  if (f == &FreeSansBold9pt7b) thStyle = TS_SB;
  else if (f == &FreeSansBold12pt7b || f == &FreeSans12pt7b) thStyle = TS_MB;
  else if (f == &FreeSansBold18pt7b) thStyle = TS_LB;
  else if (f == &FreeSansBold24pt7b) thStyle = TS_XL;
  else thStyle = TS_S;
  TFT_eSPI::setFreeFont(f);
}
void L10nTft::setTextFont(uint8_t f) {
  thStyle = (f == 4) ? TS_LB : TS_S;
  TFT_eSPI::setTextFont(f);
}
#endif
