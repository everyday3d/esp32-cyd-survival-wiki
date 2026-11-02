#include "doc.h"
#include "config.h"
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SD.h>
#include "l10n.h"

extern L10nTft tft;

static const int TEXT_X = (SCR_W < 300) ? 8 : 10;
static const int TEXT_W = SCR_W - (TEXT_X * 2);

#ifdef LANG_TH
struct ThItem { uint8_t type, fg, bg; int16_t x, y; uint16_t w, h; uint32_t off; };
static std::vector<ThItem> thItems;
static File thFile;
static bool thLoad(const char *path, int &totalH);
#endif

static int lineHeight(uint8_t s) {
  if (SCR_W < 300) {
    switch (s) {
      case S_H1: return 28;
      case S_H2: return 24;
      case S_CAPTION: return 16;
      default: return 20;
    }
  } else {
    switch (s) {
      case S_H1: return 34;
      case S_H2: return 28;
      case S_CAPTION: return 18;
      default: return 22;
    }
  }
}

static void setStyleFont(uint8_t s) {
  if (SCR_W < 300) {
    switch (s) {
      case S_H1: tft.setFreeFont(&FreeSansBold9pt7b); break;
      case S_H2: tft.setFreeFont(&FreeSansBold9pt7b); break;
      case S_CAPTION: tft.setTextFont(2); break;
      default: tft.setFreeFont(&FreeSans9pt7b); break;
    }
  } else {
    switch (s) {
      case S_H1: tft.setFreeFont(&FreeSansBold12pt7b); break;
      case S_H2: tft.setFreeFont(&FreeSansBold9pt7b); break;
      case S_CAPTION: tft.setTextFont(2); break;
      default: tft.setFreeFont(&FreeSans9pt7b); break;
    }
  }
}

// ---------- UTF-8 -> ASCII (the built-in fonts only have ASCII) ----------
void docSanitize(char *s, int &len) {
  int w = 0;
  for (int r = 0; r < len;) {
    uint8_t c = (uint8_t)s[r];
    if (c < 0x80) { if (c != '\r') s[w++] = (char)c; r++; continue; }
    uint8_t c1 = r + 1 < len ? (uint8_t)s[r + 1] : 0;
    uint8_t c2 = r + 2 < len ? (uint8_t)s[r + 2] : 0;
    if (c == 0xC2 && c1 == 0xB0) { r += 2; continue; }                    // degree sign: dropped (212F)
    if (c == 0xC2 && c1 == 0xB5) { s[w++] = 'u'; r += 2; continue; }      // micro
    if (c == 0xC3 && c1 == 0x97) { s[w++] = 'x'; r += 2; continue; }      // multiply
    if (c == 0xC2 && c1 == 0xA0) { s[w++] = ' '; r += 2; continue; }      // nbsp
    if (c == 0xE2 && c1 == 0x80) {
      if (c2 == 0x98 || c2 == 0x99) s[w++] = '\'';
      else if (c2 == 0x9C || c2 == 0x9D) s[w++] = '"';
      else if (c2 == 0x93 || c2 == 0x94) s[w++] = '-';
      else if (c2 == 0xA2) s[w++] = '*';
      else if (c2 == 0xA6) { s[w++] = '.'; s[w++] = '.'; s[w++] = '.'; }
      else s[w++] = '?';
      r += 3; continue;
    }
    if (c == 0xE2 && c1 == 0x86 && c2 == 0x92) { s[w++] = '-'; s[w++] = '>'; r += 3; continue; }
    if (c == 0xE2 && c1 == 0x89 && c2 == 0x88) { s[w++] = '~'; r += 3; continue; }
    if (c == 0xE2 && c1 == 0x89 && c2 == 0xA5) { s[w++] = '>'; s[w++] = '='; r += 3; continue; }
    if (c == 0xE2 && c1 == 0x89 && c2 == 0xA4) { s[w++] = '<'; s[w++] = '='; r += 3; continue; }
    // unknown multi-byte character: skip it, show '?'
    int n = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
    s[w++] = '?';
    r += n;
  }
  len = w;
  s[len] = 0;
}

// Remove inline Markdown (bold, code, links) so the text wraps cleanly.
static void stripInline(char *s, int &len) {
  int w = 0;
  for (int r = 0; r < len;) {
    char c = s[r];
    if ((c == '*' && r + 1 < len && s[r + 1] == '*') || (c == '_' && r + 1 < len && s[r + 1] == '_')) { r += 2; continue; }
    if (c == '`') { r++; continue; }
    if (c == '[' && (r == 0 || s[r - 1] != '!')) {
      // [text](url) -> text   (only when it all sits on one line)
      int close = -1, end = -1;
      for (int k = r + 1; k < len && s[k] != '\n'; k++) if (s[k] == ']') { close = k; break; }
      if (close > 0 && close + 1 < len && s[close + 1] == '(') {
        for (int k = close + 2; k < len && s[k] != '\n'; k++) if (s[k] == ')') { end = k; break; }
      }
      if (end > 0) {
        for (int k = r + 1; k < close; k++) s[w++] = s[k];
        r = end + 1;
        continue;
      }
    }
    s[w++] = c;
    r++;
  }
  len = w;
  s[len] = 0;
}

bool Doc::load(const char *path, bool markdown) {
  free();
#ifdef LANG_TH
  if (markdown && thLoad(path, totalH)) {
    thMode = true;
    buf = (char *)malloc(1);                // loaded means buf is set
    return buf != nullptr;
  }
#endif
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  buf = (char *)malloc(MAX_DOC_BYTES + 1);
  if (!buf) { f.close(); return false; }
  len = f.read((uint8_t *)buf, MAX_DOC_BYTES);
  f.close();
  if (len < 0) len = 0;
  buf[len] = 0;
  docSanitize(buf, len);
  if (markdown) stripInline(buf, len);
  layout(markdown);
  return true;
}

void Doc::free() {
#ifdef LANG_TH
  if (thMode) { thFile.close(); thItems.clear(); }
#endif
  thMode = false;
  if (buf) ::free(buf);
  buf = nullptr;
  len = 0;
  items.clear();
  items.shrink_to_fit();
  totalH = 0;
  docTitle = "";
}

static char tmp[256];

static const char *slice(const char *b, int start, int n) {
  if (n > (int)sizeof(tmp) - 1) n = sizeof(tmp) - 1;
  memcpy(tmp, b + start, n);
  tmp[n] = 0;
  return tmp;
}

void Doc::wrap(int start, int end, int x0, int width, uint8_t style) {
  setStyleFont(style);
  int lh = lineHeight(style);
  int pos = start;
  while (pos < end) {
    while (pos < end && buf[pos] == ' ') pos++;
    if (pos >= end) break;
    int lineStart = pos, lastFit = -1, p = pos;
    while (p < end) {
      int we = p;
      while (we < end && buf[we] != ' ') we++;
      if (tft.textWidth(slice(buf, lineStart, we - lineStart)) <= width) {
        lastFit = we;
        p = we;
        while (p < end && buf[p] == ' ') p++;
      } else break;
    }
    if (lastFit < 0) {  // one very long word: break it by characters
      int e = lineStart + 1;
      while (e < end && tft.textWidth(slice(buf, lineStart, e + 1 - lineStart)) <= width) e++;
      lastFit = e;
    }
    items.push_back({(uint16_t)lineStart, (uint16_t)(lastFit - lineStart), (int16_t)x0, (int16_t)cursorY, (int16_t)lh, style});
    cursorY += lh;
    pos = lastFit;
  }
}

void Doc::layout(bool markdown) {
  items.clear();
  items.reserve(160);
  cursorY = 8;
  int i = 0;

  // Optional front matter: ---\n title: ...\n ---
  if (markdown && len > 4 && strncmp(buf, "---\n", 4) == 0) {
    int p = 4;
    while (p < len) {
      int e = p;
      while (e < len && buf[e] != '\n') e++;
      if (e - p >= 3 && strncmp(buf + p, "---", 3) == 0) { i = e + 1; break; }
      if (strncmp(buf + p, "title:", 6) == 0) {
        int t = p + 6;
        while (t < e && buf[t] == ' ') t++;
        docTitle = String(slice(buf, t, e - t));
        wrap(t, e, TEXT_X, TEXT_W, S_H1);
        addGap(4);
      }
      p = e + 1;
    }
  }

  bool lastBlank = true, inWarn = false;
  while (i < len) {
    int e = i;
    while (e < len && buf[e] != '\n') e++;
    int s = i;
    i = e + 1;
    while (s < e && buf[s] == ' ') s++;
    int n = e - s;

    bool isWarn = markdown && n >= 1 && buf[s] == '>';
    if (inWarn && !isWarn) {
      items.push_back({0, 0, 0, (int16_t)cursorY, 8, S_WARNPAD});
      cursorY += 8 + 6;
      inWarn = false;
    }

    if (n == 0) {
      if (!lastBlank) addGap(8);
      lastBlank = true;
      continue;
    }
    lastBlank = false;

    if (!markdown) { wrap(s, e, TEXT_X, TEXT_W, S_BODY); continue; }

    if (isWarn) {
      if (!inWarn) {
        addGap(2);
        items.push_back({0, 0, 0, (int16_t)cursorY, 8, S_WARNPAD});
        cursorY += 8;
        inWarn = true;
      }
      int t = s + 1;
      while (t < e && buf[t] == ' ') t++;
      wrap(t, e, (SCR_W < 300 ? 18 : 22), SCR_W - (SCR_W < 300 ? 30 : 40), S_WARN);
    } else if (n > 2 && buf[s] == '#' ) {
      int t = s;
      while (t < e && buf[t] == '#') t++;
      bool h1 = (t - s) == 1;
      while (t < e && buf[t] == ' ') t++;
      addGap(h1 ? (SCR_W < 300 ? 6 : 8) : (SCR_W < 300 ? 4 : 6));
      wrap(t, e, TEXT_X, TEXT_W, h1 ? S_H1 : S_H2);
    } else if (n >= 3 && strncmp(buf + s, "---", 3) == 0) {
      items.push_back({0, 0, 0, (int16_t)cursorY, (int16_t)(SCR_W < 300 ? 12 : 16), S_RULE});
      cursorY += (SCR_W < 300 ? 12 : 16);
    } else if (n > 2 && (buf[s] == '-' || buf[s] == '*') && buf[s + 1] == ' ') {
      items.push_back({0, 0, (int16_t)(SCR_W < 300 ? 10 : 14), (int16_t)cursorY, (int16_t)lineHeight(S_BULLET), S_MARK});
      wrap(s + 2, e, (SCR_W < 300 ? 22 : 28), SCR_W - (SCR_W < 300 ? 30 : 38), S_BULLET);
    } else if (n > 2 && isdigit((unsigned char)buf[s])) {
      int t = s;
      while (t < e && isdigit((unsigned char)buf[t])) t++;
      if (t < e - 1 && buf[t] == '.' && buf[t + 1] == ' ') {
        items.push_back({(uint16_t)s, (uint16_t)(t + 1 - s), (int16_t)(SCR_W < 300 ? 8 : 10), (int16_t)cursorY, (int16_t)lineHeight(S_BULLET), S_MARK});
        wrap(t + 2, e, (SCR_W < 300 ? 26 : 32), SCR_W - (SCR_W < 300 ? 34 : 42), S_BULLET);
      } else {
        wrap(s, e, TEXT_X, TEXT_W, S_BODY);
      }
    } else if (n > 5 && buf[s] == '!' && buf[s + 1] == '[') {
      // ![caption](img/file.jpg)
      int cb = s + 2, ce = cb;
      while (ce < e && buf[ce] != ']') ce++;
      int ps = ce + 2, pe = ps;
      while (pe < e && buf[pe] != ')') pe++;
      if (ce < e && ps < e && pe <= e) {
        String full = String("/wiki/") + slice(buf, ps, pe - ps);
        uint16_t w = 0, h = 0;
        addGap(4);
        if (TJpgDec.getFsJpgSize(&w, &h, full.c_str(), SD) == JDR_OK && w > 0) {
          int sc = 1;
          while (w / sc > TEXT_W && sc < 8) sc *= 2;
          int dh = h / sc;
          items.push_back({(uint16_t)ps, (uint16_t)(pe - ps), (int16_t)((SCR_W - w / sc) / 2), (int16_t)cursorY, (int16_t)dh, S_IMG});
          cursorY += dh + 4;
        } else {
          items.push_back({(uint16_t)ps, (uint16_t)(pe - ps), 0, (int16_t)cursorY, 40, S_IMG});
          cursorY += 44;
        }
        if (ce > cb) wrap(cb, ce, TEXT_X, TEXT_W, S_CAPTION);
        addGap(6);
      }
    } else {
      wrap(s, e, TEXT_X, TEXT_W, S_BODY);
    }
  }
  if (inWarn) {
    items.push_back({0, 0, 0, (int16_t)cursorY, 8, S_WARNPAD});
    cursorY += 8;
  }
  totalH = cursorY + 16;
}

static int jpgClipBottom = CONTENT_H;
static bool jpgOut(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bmp) {
  if (y >= jpgClipBottom) return false;   // rest of the picture is off-screen
  tft.pushImage(x, y, w, h, bmp);
  return true;
}

#ifdef LANG_TH
// ---------- Thai pages: pre-drawn on a PC (scripts/thai/build-thai-pack.ps1), see l10n.h ----------

static bool thLoad(const char *path, int &totalH) {
  const char *pre = "/wiki/articles/";
  if (strncmp(path, pre, strlen(pre)) != 0 || !l10nActive()) return false;
  String id = String(path + strlen(pre));
  if (id.endsWith(".md")) id.remove(id.length() - 3);
  String f = String(SCR_W < 300 ? "/th/p28/" : "/th/p35/") + id + ".thp";
  if (!SD.exists(f)) return false;
  thFile = SD.open(f, FILE_READ);
  if (!thFile) return false;
  uint8_t h[12];
  if (thFile.read(h, 12) != 12 || memcmp(h, "THP1", 4) != 0) { thFile.close(); return false; }
  int n = h[8] | (h[9] << 8);
  totalH = h[6] | (h[7] << 8);
  thItems.clear();
  thItems.reserve(n);
  for (int i = 0; i < n; i++) {
    uint8_t r[16];
    if (thFile.read(r, 16) != 16) { thFile.close(); thItems.clear(); return false; }
    thItems.push_back({r[0], r[1], r[2], (int16_t)(r[4] | (r[5] << 8)), (int16_t)(r[6] | (r[7] << 8)), (uint16_t)(r[8] | (r[9] << 8)), (uint16_t)(r[10] | (r[11] << 8)),
                       (uint32_t)(r[12] | (r[13] << 8) | (r[14] << 16) | ((uint32_t)r[15] << 24))});
  }
  return true;
}

static void thDrawText(const ThItem &it, int y) {
  static const uint16_t FG[] = {C_TEXT, C_ACCENT, C_MUTED};
  uint16_t fg = FG[it.fg > 2 ? 0 : it.fg], bg = it.bg == 1 ? C_WARN_BG : C_BG;
  const int rs = (it.w + 1) / 2;
  if (it.w > SCR_W || rs > 160) return;
  static uint8_t raw[8 * 160];
  static uint16_t line[8 * 320];
  int r0 = max(0, -y), r1 = min<int>(it.h, CONTENT_H - y);
  for (int r = r0; r < r1; r += 8) {
    int rows = min(8, r1 - r);
    thFile.seek(it.off + (uint32_t)r * rs);
    thFile.read(raw, rows * rs);
    for (int j = 0; j < rows; j++)
      for (int c = 0; c < it.w; c++) {
        uint8_t b = raw[j * rs + (c >> 1)];
        uint8_t a = (c & 1) ? (b & 15) : (b >> 4);
        line[j * it.w + c] = a == 0 ? bg : (a == 15 ? fg : tft.alphaBlend(a * 17, fg, bg));
      }
    int cw = min<int>(it.w, SCR_W - it.x);
    if (cw == it.w) tft.pushImage(it.x, y + r, it.w, rows, line);
    else if (cw > 0) for (int j = 0; j < rows; j++) tft.pushImage(it.x, y + r + j, cw, 1, line + j * it.w);
  }
}

static void thDrawPage(int scroll) {
  for (const ThItem &it : thItems) {
    int y = it.y - scroll;
    if (y + it.h < 0 || y > CONTENT_H) continue;
    switch (it.type) {
      case 1:
        tft.fillRect((SCR_W < 300 ? 6 : 10), y, SCR_W - (SCR_W < 300 ? 12 : 20), it.h, C_WARN_BG);
        tft.fillRect((SCR_W < 300 ? 6 : 10), y, (SCR_W < 300 ? 3 : 4), it.h, C_WARN_BAR);
        break;
      case 2:
        tft.drawFastHLine(TEXT_X, y + (SCR_W < 300 ? 6 : 8), TEXT_W, C_LINE);
        break;
      case 3:
        tft.fillCircle(it.x + (SCR_W < 300 ? 3 : 4), y + it.h / 2, (SCR_W < 300 ? 2 : 3), C_ACCENT);
        break;
      case 4: {
        char rel[80];
        thFile.seek(it.off);
        int n = thFile.read((uint8_t *)rel, sizeof(rel) - 1);
        rel[n < 0 ? 0 : n] = 0;
        String full = String("/wiki/") + rel;
        uint16_t w = 0, h = 0;
        if (TJpgDec.getFsJpgSize(&w, &h, full.c_str(), SD) == JDR_OK) {
          int sc = 1;
          while (w / sc > TEXT_W && sc < 8) sc *= 2;
          TJpgDec.setJpgScale(sc);
          TJpgDec.setCallback(jpgOut);
          jpgClipBottom = CONTENT_H;
          TJpgDec.drawFsJpg(it.x, y, full.c_str(), SD);
        }
        break;
      }
      default:
        thDrawText(it, y);
        break;
    }
  }
}
#endif
void Doc::draw(int scroll) {
  tft.fillRect(0, 0, SCR_W, CONTENT_H, C_BG);
  if (!buf) return;
#ifdef LANG_TH
  if (thMode) { thDrawPage(scroll); return; }
#endif
  tft.setTextDatum(TL_DATUM);
  for (const DocItem &it : items) {
    int y = it.y - scroll;
    if (y + it.h < 0 || y > CONTENT_H) continue;
    switch (it.style) {
      case S_WARNPAD:
        tft.fillRect((SCR_W < 300 ? 6 : 10), y, SCR_W - (SCR_W < 300 ? 12 : 20), it.h, C_WARN_BG);
        tft.fillRect((SCR_W < 300 ? 6 : 10), y, (SCR_W < 300 ? 3 : 4), it.h, C_WARN_BAR);
        break;
      case S_WARN:
        tft.fillRect((SCR_W < 300 ? 6 : 10), y, SCR_W - (SCR_W < 300 ? 12 : 20), it.h, C_WARN_BG);
        tft.fillRect((SCR_W < 300 ? 6 : 10), y, (SCR_W < 300 ? 3 : 4), it.h, C_WARN_BAR);
        setStyleFont(S_WARN);
        tft.setTextColor(C_TEXT);
        tft.drawString(slice(buf, it.off, it.len), it.x, y + (SCR_W < 300 ? 2 : 4));
        break;
      case S_RULE:
        tft.drawFastHLine(TEXT_X, y + (SCR_W < 300 ? 6 : 8), TEXT_W, C_LINE);
        break;
      case S_MARK:
        if (it.len == 0) {
          tft.fillCircle(it.x + (SCR_W < 300 ? 3 : 4), y + (SCR_W < 300 ? 9 : 11), (SCR_W < 300 ? 2 : 3), C_ACCENT);
        } else {
          setStyleFont(S_BODY);
          tft.setTextColor(C_ACCENT);
          tft.drawString(slice(buf, it.off, it.len), it.x, y + (SCR_W < 300 ? 2 : 4));
        }
        break;
      case S_IMG: {
        String full = String("/wiki/") + slice(buf, it.off, it.len);
        uint16_t w = 0, h = 0;
        if (it.x == 0 && it.h == 40) {
          tft.drawRect(TEXT_X, y, TEXT_W, (SCR_W < 300 ? 30 : 36), C_LINE);
          tft.setTextFont(2);
          tft.setTextColor(C_MUTED);
          tft.drawString("[picture missing]", TEXT_X + 6, y + 8);
        } else if (TJpgDec.getFsJpgSize(&w, &h, full.c_str(), SD) == JDR_OK) {
          int sc = 1;
          while (w / sc > TEXT_W && sc < 8) sc *= 2;
          TJpgDec.setJpgScale(sc);
          TJpgDec.setCallback(jpgOut);
          jpgClipBottom = CONTENT_H;
          TJpgDec.drawFsJpg(it.x, y, full.c_str(), SD);
        }
        break;
      }
      default: {
        setStyleFont(it.style);
        uint16_t col = C_TEXT;
        if (it.style == S_H2) col = C_ACCENT;
        if (it.style == S_CAPTION) col = C_MUTED;
        tft.setTextColor(col);
        tft.drawString(slice(buf, it.off, it.len), it.x, y + (it.style == S_CAPTION ? 1 : (SCR_W < 300 ? 2 : 4)));
        break;
      }
    }
  }
}
