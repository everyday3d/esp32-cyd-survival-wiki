#include "l10n.h"

#ifdef LANG_TH
#include "config.h"
#include "utf8.h"
#include "thai_font_data.h"
#include "th_strings.h"

extern L10nTft tft;

// ---------- English -> Thai lookup ----------

static const char *findStr(const char *s) {
  int lo = 0, hi = TH_STR_N - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    int c = strcmp(TH_STR[m].en, s);
    if (c == 0) return TH_STR[m].th;
    if (c < 0) lo = m + 1; else hi = m - 1;
  }
  return nullptr;
}

bool l10nIsThai(const char *s) {
  for (; *s; s++) if ((uint8_t)*s >= 0x80) return true;
  return false;
}

// "6 arts." has no entry of its own, but " arts." does: keep the count and translate the end.
const char *l10nTr(const char *s) {
  if (!s || !*s) return s;
  if (l10nIsThai(s)) return s;
  const char *t = findStr(s);
  if (t) return t;
  static String out;
  for (const char *p = s + 1; *p; p++) {
    if (*p != ' ') continue;
    bool count = true;
    for (const char *q = s; q < p; q++) if (!isdigit((uint8_t)*q) && *q != '.' && *q != ',') { count = false; break; }
    if (!count) break;
    const char *tail = findStr(p);
    if (tail) { out = String(s).substring(0, p - s); out += tail; return out.c_str(); }
  }
  return s;
}

bool l10nHas(const char *s) { return s && (l10nIsThai(s) || findStr(s) != nullptr); }
bool l10nActive() { return true; }
bool l10nBegin() { return true; }

// ---------- Thai glyphs ----------

static const ThaiShape *findShape(uint32_t key) {
  int lo = 0, hi = thaiShapeCount - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    uint32_t k = thaiShapes[m].key;
    if (k == key) return &thaiShapes[m];
    if (k < key) lo = m + 1; else hi = m - 1;
  }
  return nullptr;
}

static const ThaiCodepoint *findCodepoint(uint32_t cp) {
  int lo = 0, hi = thaiCodepointCount - 1;
  while (lo <= hi) {
    int m = (lo + hi) / 2;
    uint32_t c = thaiCodepoints[m].codepoint;
    if (c == cp) return &thaiCodepoints[m];
    if (c < cp) lo = m + 1; else hi = m - 1;
  }
  return nullptr;
}

void L10nTft::setThaiSize(uint8_t px) { thStyle = px <= 12 ? 0 : px <= 14 ? 1 : px <= 16 ? 2 : px <= 18 ? 3 : 4; }
int L10nTft::thaiLineHeight() const { return thaiAtlases[thStyle].height; }

void L10nTft::setFreeFont(const GFXfont *f) {
  if (f == &FreeSansBold12pt7b || f == &FreeSans12pt7b) thStyle = 3;
  else if (f == &FreeSansBold18pt7b || f == &FreeSansBold24pt7b) thStyle = 4;
  else thStyle = 2;
  TFT_eSPI::setFreeFont(f);
}
void L10nTft::setTextFont(uint8_t f) {
  thStyle = (f == 1) ? 0 : (f == 4) ? 4 : 1;
  TFT_eSPI::setTextFont(f);
}

// Returns the width. With paint set it also draws, blending each 4-bit glyph over what is on the screen so that
// tone marks and vowels never wipe the letter under them.
int L10nTft::thaiRender(const char *s, int x, int y, bool paint) {
  const ThaiAtlas &a = thaiAtlases[thStyle];
  const uint16_t ink = (uint16_t)textcolor;
  int len = strlen(s), p = 0;
  int32_t pen = 0;
  auto glyph = [&](uint16_t gid, int gx, int gy) {
    if (!paint) return;
    const ThaiBitmap &b = a.glyphs[gid];
    int x0 = x + gx + b.left, y0 = y + a.baseline - gy - b.top;
    int stride = (b.width + 1) / 2;
    static uint16_t tile[48 * 48];
    if (b.width > 48 || b.height > 48 || !b.width || !b.height) return;
    if (x0 >= SCR_W || y0 >= SCR_H || x0 + b.width <= 0 || y0 + b.height <= 0) return;
    memset(tile, 0, sizeof(tile));
    readRect(x0, y0, b.width, b.height, tile);
    for (int row = 0; row < b.height; row++)
      for (int col = 0; col < b.width; col++) {
        uint8_t packed = a.pixels[b.offset + row * stride + col / 2];
        uint8_t alpha = col & 1 ? packed & 15 : packed >> 4;
        if (!alpha) continue;
        uint16_t color = ink;
        if (alpha < 15) {
          uint16_t raw = tile[row * b.width + col];
          uint16_t bg = (raw << 8) | (raw >> 8);       // readRect and pushRect use byte swapped RGB565
          int r = (((ink >> 11) & 31) * alpha + ((bg >> 11) & 31) * (15 - alpha) + 7) / 15;
          int g = (((ink >> 5) & 63) * alpha + ((bg >> 5) & 63) * (15 - alpha) + 7) / 15;
          int bl = ((ink & 31) * alpha + (bg & 31) * (15 - alpha) + 7) / 15;
          color = (r << 11) | (g << 5) | bl;
        }
        tile[row * b.width + col] = (color << 8) | (color >> 8);
      }
    pushRect(x0, y0, b.width, b.height, tile);
  };
  while (p < len) {
    uint32_t cp = utf8Decode(s, len, p);
    if (cp == 0x200B || cp == 0xFEFF) continue;
    const ThaiShape *sh = nullptr;
    if (cp >= 0xE01 && cp <= 0xE5B) {
      uint32_t key = (cp & 255) << 24;
      sh = findShape(key);
      int q = p, used = p;
      for (int n = 1; n < 4 && q < len; n++) {
        uint32_t next = utf8Decode(s, len, q);
        if (!(thaiMark(next) || next == 0xE33 || next == 0xE32)) break;
        key |= (next & 255) << (24 - 8 * n);
        const ThaiShape *candidate = findShape(key);
        if (candidate) { sh = candidate; used = q; }
      }
      p = used;
    }
    if (sh) {
      for (int i = 0; i < sh->count; i++) {
        const ThaiRunGlyph &g = thaiRuns[sh->offset + i];
        glyph(g.glyph, (pen + g.x * a.size + 500) / 1000, (int)roundf(g.y * a.size / 1000.0f));
      }
      pen += sh->advance * a.size;
    } else {
      const ThaiCodepoint *g = findCodepoint(cp);
      if (!g) g = findCodepoint('?');
      glyph(g->glyph, (pen + 500) / 1000, 0);
      pen += g->advance * a.size;
    }
  }
  return (pen + 999) / 1000;
}

int16_t L10nTft::textWidth(const char *s) {
  const char *t = l10nTr(s);
  if (!l10nIsThai(t)) return TFT_eSPI::textWidth(s);
  return thaiRender(t, 0, 0, false);
}
int16_t L10nTft::textWidth(const String &s) { return textWidth(s.c_str()); }

int16_t L10nTft::drawString(const char *s, int32_t x, int32_t y) {
  const char *t = l10nTr(s);
  if (!l10nIsThai(t)) return TFT_eSPI::drawString(s, x, y);
  String keep = t;                                   // l10nTr may hand back a buffer that the next lookup reuses
  const uint8_t d = textdatum < 9 ? textdatum : 0;
  const int hz = d % 3, vt = d / 3;
  int w = thaiRender(keep.c_str(), 0, 0, false), h = thaiLineHeight();
  int x0 = hz == 0 ? x : hz == 1 ? x - w / 2 : x - w;
  int y0 = vt == 0 ? y : vt == 1 ? y - h / 2 : y - h;
  uint16_t pad = getTextPadding();
  if (textbgcolor != textcolor && pad > 0) {         // clear the old text, like the normal fonts do
    int px0 = hz == 0 ? x : hz == 1 ? x - pad / 2 : x - pad;
    fillRect(max(0, px0), max(0, y0), min<int>(pad, SCR_W - max(0, px0)), h, textbgcolor);
  }
  thaiRender(keep.c_str(), x0, y0, true);
  return w;
}
int16_t L10nTft::drawString(const String &s, int32_t x, int32_t y) { return drawString(s.c_str(), x, y); }
#endif
