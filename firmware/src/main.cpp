// =====================================================================
//  Survival Wiki - offline survival reference for the ESP32-32E 3.5" board
//  * Touch UI on the 320x480 screen, reading articles + pictures from SD
//  * Wi-Fi hotspot with captive portal: phones get the full wiki in a browser
// =====================================================================
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <vector>
#include <algorithm>
#include "config.h"
#include "doc.h"
#include "web.h"
#include "qrcodegen.h"

TFT_eSPI tft;
static SPIClass sdSPI(VSPI);
static Preferences prefs;
static Doc doc;
static WifiConfig wcfg;

// ---------------------------------------------------------------- data
struct Category { String id, name, blurb; uint16_t color; uint16_t count; };
struct Entry { String id, title, extra; };

static Category cats[MAX_CATEGORIES];
static int nCats = 0;
static int nArticles = 0;
static std::vector<Entry> list;   // current category's articles, or the library books

// ---------------------------------------------------------------- state
enum Screen { SC_HOME, SC_CATEGORY, SC_ARTICLE, SC_LIBRARY, SC_BOOK, SC_WIFI, SC_SETTINGS, SC_NOSD };
static Screen screen = SC_HOME;
static int curCat = 0, listPage = 0, scrollY = 0;
static int bookIdx = 0, bookPart = 1, bookParts = 1;
static String articleTitle;

static uint8_t brightness = 200;
static uint8_t timeoutIdx = 2;
static const uint16_t TIMEOUTS_S[] = {0, 60, 180, 300, 600};
static const char *TIMEOUT_NAMES[] = {"Never", "1 min", "3 min", "5 min", "10 min"};
static bool invertColors = false;
static bool wifiOn = true;
static bool sleeping = false;
static uint32_t lastTouchMs = 0, lastStatusMs = 0;
static int lastClients = -1;

// ---------------------------------------------------------------- buttons
enum BtnId : uint16_t {
  B_NONE = 0, B_BACK, B_UP, B_DOWN, B_PREV, B_NEXT, B_LIBRARY, B_WIFI, B_SETTINGS,
  B_WIFI_TOGGLE, B_BRIGHT_DN, B_BRIGHT_UP, B_TIMEOUT, B_INVERT, B_CALIB, B_RETRY,
  B_TILE = 100, B_ROW = 200
};
enum Icon : uint8_t { I_NONE, I_UP, I_DOWN, I_LEFT, I_RIGHT };
struct Btn { int16_t x, y, w, h; uint16_t id; String label; uint8_t icon; bool enabled; };
static std::vector<Btn> btns;

static const int ROWS_PER_PAGE = CONTENT_H / 48;   // 8

// ---------------------------------------------------------------- helpers
static uint16_t hexColor(const String &h) {
  if (h.length() < 7 || h[0] != '#') return C_ACCENT;
  long v = strtol(h.c_str() + 1, nullptr, 16);
  return tft.color565((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

static int splitTabs(const String &line, String *out, int maxParts) {
  int n = 0, start = 0;
  while (n < maxParts) {
    int t = line.indexOf('\t', start);
    if (t < 0) { out[n++] = line.substring(start); break; }
    out[n++] = line.substring(start, t);
    start = t + 1;
  }
  for (int i = 0; i < n; i++) out[i].trim();
  return n;
}

static String readLine(File &f) {
  String s = f.readStringUntil('\n');
  s.replace("\r", "");
  return s;
}

static String fitText(String s, int maxW) {
  if (tft.textWidth(s) <= maxW) return s;
  while (s.length() > 1 && tft.textWidth(s + "...") > maxW) s.remove(s.length() - 1);
  s.trim();
  return s + "...";
}

static String asciiOnly(const String &in) {
  // reuse the document sanitizer for titles from the catalog
  static char b[160];
  int n = in.length();
  if (n > 150) n = 150;
  memcpy(b, in.c_str(), n);
  b[n] = 0;
  docSanitize(b, n);
  return String(b);
}

static void setBacklight(uint8_t v) { ledcWrite(0, v); }

// ---------------------------------------------------------------- loading
static void loadConfig() {
  File f = SD.open(F_CONFIG, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String l = readLine(f);
    l.trim();
    if (!l.length() || l[0] == '#') continue;
    int eq = l.indexOf('=');
    if (eq < 0) continue;
    String k = l.substring(0, eq), v = l.substring(eq + 1);
    k.trim(); v.trim();
    if (k == "ssid" && v.length()) wcfg.ssid = v.substring(0, 31);
    else if (k == "password") wcfg.password = v.substring(0, 63);
    else if (k == "channel") { int c = v.toInt(); if (c >= 1 && c <= 11) wcfg.channel = c; }
  }
  f.close();
}

static void loadCatalog() {
  nCats = 0;
  nArticles = 0;
  File f = SD.open(F_CATEGORIES, FILE_READ);
  if (f) {
    while (f.available() && nCats < MAX_CATEGORIES) {
      String l = readLine(f);
      if (!l.length() || l[0] == '#') continue;
      String p[4];
      int n = splitTabs(l, p, 4);
      if (n < 2) continue;
      cats[nCats] = {p[0], asciiOnly(p[1]), n > 3 ? asciiOnly(p[3]) : String(""), hexColor(n > 2 ? p[2] : ""), 0};
      nCats++;
    }
    f.close();
  }
  f = SD.open(F_CATALOG, FILE_READ);
  if (f) {
    while (f.available()) {
      String l = readLine(f);
      if (!l.length() || l[0] == '#') continue;
      String p[3];
      if (splitTabs(l, p, 3) < 3) continue;
      for (int i = 0; i < nCats; i++) if (cats[i].id == p[1]) { cats[i].count++; nArticles++; break; }
    }
    f.close();
  }
}

static void loadCategoryList(int ci) {
  list.clear();
  File f = SD.open(F_CATALOG, FILE_READ);
  if (!f) return;
  while (f.available() && list.size() < MAX_ARTICLES) {
    String l = readLine(f);
    if (!l.length() || l[0] == '#') continue;
    String p[3];
    if (splitTabs(l, p, 3) < 3) continue;
    if (p[1] == cats[ci].id) list.push_back({p[0], asciiOnly(p[2]), ""});
  }
  f.close();
}

static void loadLibraryList() {
  list.clear();
  File f = SD.open(F_LIBRARY, FILE_READ);
  if (!f) return;
  while (f.available() && list.size() < MAX_BOOKS) {
    String l = readLine(f);
    if (!l.length() || l[0] == '#') continue;
    String p[4];
    if (splitTabs(l, p, 4) < 3) continue;
    list.push_back({p[0], asciiOnly(p[1]), p[2]});
  }
  f.close();
}

// ---------------------------------------------------------------- drawing
static void drawIcon(uint8_t icon, int cx, int cy, uint16_t col) {
  switch (icon) {
    case I_UP:    tft.fillTriangle(cx - 10, cy + 6, cx + 10, cy + 6, cx, cy - 7, col); break;
    case I_DOWN:  tft.fillTriangle(cx - 10, cy - 6, cx + 10, cy - 6, cx, cy + 7, col); break;
    case I_LEFT:  tft.fillTriangle(cx + 6, cy - 10, cx + 6, cy + 10, cx - 7, cy, col); break;
    case I_RIGHT: tft.fillTriangle(cx - 6, cy - 10, cx - 6, cy + 10, cx + 7, cy, col); break;
  }
}

static void drawButton(const Btn &b, bool pressed) {
  uint16_t bg = pressed ? C_PRESS : C_PANEL;
  tft.fillRoundRect(b.x + 2, b.y + 2, b.w - 4, b.h - 4, 6, bg);
  uint16_t fg = b.enabled ? C_TEXT : C_LINE;
  int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
  if (b.icon != I_NONE) {
    if (b.label.length()) {
      tft.setFreeFont(&FreeSansBold9pt7b);
      int tw = tft.textWidth(b.label);
      int total = 18 + 6 + tw;
      int ix = cx - total / 2 + 9;
      drawIcon(b.icon, ix, cy, b.enabled ? C_ACCENT : C_LINE);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(fg);
      tft.drawString(b.label, ix + 15, cy);
    } else {
      drawIcon(b.icon, cx, cy, b.enabled ? C_ACCENT : C_LINE);
    }
  } else {
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg);
    tft.drawString(b.label, cx, cy);
  }
}

static void addBtn(int x, int y, int w, int h, uint16_t id, const String &label, uint8_t icon = I_NONE, bool en = true) {
  btns.push_back({(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, id, label, icon, en});
  drawButton(btns.back(), false);
}

struct FootDef { uint16_t id; const char *label; uint8_t icon; bool en; };
static void drawFooter(const FootDef *defs, int n) {
  int y = SCR_H - FOOTER_H;
  tft.fillRect(0, y, SCR_W, FOOTER_H, C_HEADER);
  int w = SCR_W / n;
  for (int i = 0; i < n; i++) {
    int x = i * w;
    int ww = (i == n - 1) ? SCR_W - x : w;
    if (defs[i].id == B_NONE) {   // plain label (page / percent indicator)
      tft.setTextFont(2);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(C_MUTED);
      tft.drawString(defs[i].label, x + ww / 2, y + FOOTER_H / 2);
    } else {
      addBtn(x, y + 2, ww, FOOTER_H - 4, defs[i].id, defs[i].label, defs[i].icon, defs[i].en);
    }
  }
}

static void drawWifiBadge() {
  int x = SCR_W - 74, y = 0;
  tft.fillRect(x, y, 74, HEADER_H, C_HEADER);
  int cx = x + 16, cy = 30;
  uint16_t col = wifiOn ? 0x07E9 : C_LINE;
  for (int r = 4; r <= 14; r += 5) {
    tft.drawArc(cx, cy, r + 1, r - 1, 135, 225, col, C_HEADER, false);
  }
  tft.fillCircle(cx, cy, 2, col);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(wifiOn ? C_TEXT : C_MUTED);
  tft.drawString(wifiOn ? String(webClientCount()) + " on" : String("off"), cx + 14, 24);
}

static void drawHeader(const String &title, bool back) {
  tft.fillRect(0, 0, SCR_W, HEADER_H, C_HEADER);
  tft.drawFastHLine(0, HEADER_H - 1, SCR_W, C_ACCENT);
  int tx = 12;
  if (back) {
    btns.push_back({0, 0, 52, HEADER_H, B_BACK, "", I_LEFT, true});
    drawIcon(I_LEFT, 22, HEADER_H / 2, C_ACCENT);
    tx = 44;
  }
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_TEXT);
  int maxW = SCR_W - tx - (back ? 12 : 80);
  String t = fitText(title, maxW);
  if (tft.textWidth(t) > maxW || t.endsWith("...")) {   // long titles: smaller font
    tft.setFreeFont(&FreeSansBold9pt7b);
    t = fitText(title, maxW);
  }
  tft.drawString(t, tx, HEADER_H / 2);
  if (!back) drawWifiBadge();
}

static void drawCatIcon(const String &id, int cx, int cy) {
  uint16_t w = TFT_WHITE;
  if (id == "water") {
    tft.fillCircle(cx, cy + 4, 8, w);
    tft.fillTriangle(cx - 8, cy + 2, cx + 8, cy + 2, cx, cy - 12, w);
  } else if (id == "fire") {
    tft.fillTriangle(cx - 9, cy + 9, cx + 9, cy + 9, cx, cy - 12, w);
    tft.fillTriangle(cx - 9, cy + 9, cx + 1, cy + 9, cx - 6, cy - 4, w);
    tft.fillTriangle(cx - 1, cy + 9, cx + 9, cy + 9, cx + 7, cy - 6, w);
  } else if (id == "shelter") {
    tft.fillTriangle(cx - 13, cy + 9, cx + 13, cy + 9, cx, cy - 11, w);
    tft.fillTriangle(cx - 3, cy + 9, cx + 3, cy + 9, cx, cy, C_BG);
  } else if (id == "firstaid") {
    tft.fillRect(cx - 3, cy - 11, 7, 22, w);
    tft.fillRect(cx - 11, cy - 3, 22, 7, w);
  } else if (id == "food") {
    tft.fillEllipse(cx - 2, cy, 11, 6, w);
    tft.fillTriangle(cx + 7, cy, cx + 14, cy - 7, cx + 14, cy + 7, w);
  } else if (id == "navigation") {
    tft.drawCircle(cx, cy, 11, w);
    tft.fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy - 9, TFT_RED);
    tft.fillTriangle(cx - 4, cy, cx + 4, cy, cx, cy + 9, w);
  } else if (id == "signals") {
    tft.fillCircle(cx, cy, 3, w);
    for (int r = 7; r <= 13; r += 6) tft.drawCircle(cx, cy, r, w);
  } else if (id == "basics") {
    tft.drawCircle(cx, cy, 11, w);
    tft.fillRect(cx - 1, cy - 7, 3, 9, w);
    tft.fillRect(cx - 1, cy + 4, 3, 3, w);
  } else {
    tft.drawCircle(cx, cy, 10, w);
  }
}

static void drawHome() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("SURVIVAL WIKI", false);
  const int cols = 2, rows = 4, gap = 8;
  int tw = (SCR_W - gap * (cols + 1)) / cols;
  int th = (CONTENT_H - gap * (rows + 1)) / rows;
  for (int i = 0; i < nCats && i < cols * rows; i++) {
    int x = gap + (i % cols) * (tw + gap);
    int y = CONTENT_Y + gap + (i / cols) * (th + gap);
    btns.push_back({(int16_t)x, (int16_t)y, (int16_t)tw, (int16_t)th, (uint16_t)(B_TILE + i), "", I_NONE, true});
    tft.fillRoundRect(x, y, tw, th, 10, cats[i].color);
    drawCatIcon(cats[i].id, x + 24, y + 26);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(fitText(cats[i].name, tw - 16), x + 10, y + th - 40);
    tft.setTextFont(2);
    tft.drawString(String(cats[i].count) + (cats[i].count == 1 ? " article" : " articles"), x + 10, y + th - 20);
  }
  if (nCats == 0) {
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("No articles found on the SD card.", SCR_W / 2, CONTENT_Y + 150);
    tft.drawString("Copy the 'sdcard' folder contents to it.", SCR_W / 2, CONTENT_Y + 172);
  }
  FootDef f[] = {{B_LIBRARY, "Library", I_NONE, true}, {B_WIFI, "Wi-Fi", I_NONE, true}, {B_SETTINGS, "Settings", I_NONE, true}};
  drawFooter(f, 3);
}

static void drawList(const String &title, bool books) {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader(title, true);
  int pages = max(1, (int)((list.size() + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE));
  if (listPage >= pages) listPage = pages - 1;
  int first = listPage * ROWS_PER_PAGE;
  for (int r = 0; r < ROWS_PER_PAGE && first + r < (int)list.size(); r++) {
    int y = CONTENT_Y + r * 48;
    const Entry &e = list[first + r];
    btns.push_back({0, (int16_t)y, SCR_W, 48, (uint16_t)(B_ROW + first + r), "", I_NONE, true});
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TEXT);
    if (books) {
      tft.drawString(fitText(e.title, SCR_W - 50), 12, y + 17);
      tft.setTextFont(2);
      tft.setTextColor(C_MUTED);
      tft.drawString(e.extra + " parts", 12, y + 36);
    } else {
      tft.drawString(fitText(e.title, SCR_W - 50), 12, y + 24);
    }
    drawIcon(I_RIGHT, SCR_W - 20, y + 24, C_LINE);
    tft.drawFastHLine(8, y + 47, SCR_W - 16, C_LINE);
  }
  if (list.empty()) {
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString(books ? "No manuals in /library yet." : "Nothing here yet.", SCR_W / 2, CONTENT_Y + 120);
  }
  String pg = String(listPage + 1) + " / " + pages;
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_UP, "", I_UP, listPage > 0}, {B_NONE, pg.c_str(), I_NONE, true},
                 {B_DOWN, "", I_DOWN, listPage < pages - 1}};
  drawFooter(f, 4);
}

static void drawDocView() {
  tft.setViewport(0, CONTENT_Y, SCR_W, CONTENT_H);
  doc.draw(scrollY);
  tft.resetViewport();
}

static void drawDocFooter() {
  btns.erase(std::remove_if(btns.begin(), btns.end(), [](const Btn &b) { return b.id != B_BACK || b.y != 0; }), btns.end());
  int maxScroll = max(0, doc.height() - CONTENT_H);
  int pct = maxScroll ? (scrollY * 100 / maxScroll) : 100;
  String p = String(pct) + "%";
  if (screen == SC_BOOK) {
    String part = String(bookPart) + "/" + bookParts;
    FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_PREV, "", I_LEFT, bookPart > 1}, {B_UP, "", I_UP, scrollY > 0 || bookPart > 1},
                   {B_NONE, part.c_str(), I_NONE, true}, {B_DOWN, "", I_DOWN, scrollY < maxScroll || bookPart < bookParts},
                   {B_NEXT, "", I_RIGHT, bookPart < bookParts}};
    drawFooter(f, 6);
  } else {
    FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_UP, "", I_UP, scrollY > 0}, {B_NONE, p.c_str(), I_NONE, true},
                   {B_DOWN, "", I_DOWN, scrollY < maxScroll}};
    drawFooter(f, 4);
  }
}

static void showLoading(const char *msg) {
  tft.fillRect(0, CONTENT_Y, SCR_W, CONTENT_H, C_BG);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_MUTED);
  tft.drawString(msg, SCR_W / 2, CONTENT_Y + CONTENT_H / 2);
}

static void openArticle(const Entry &e) {
  screen = SC_ARTICLE;
  scrollY = 0;
  articleTitle = e.title;
  btns.clear();
  drawHeader(e.title, true);
  showLoading("Loading...");
  String path = String(D_ARTICLES) + e.id + ".md";
  if (!doc.load(path.c_str(), true)) {
    showLoading("Could not open this article.");
  } else {
    drawDocView();
  }
  drawDocFooter();
}

static void openBookPart(bool atEnd) {
  screen = SC_BOOK;
  btns.clear();
  drawHeader(list[bookIdx].title, true);
  showLoading("Loading...");
  char name[16];
  snprintf(name, sizeof(name), "/%04d.txt", bookPart);
  String path = String(D_LIBRARY) + list[bookIdx].id + name;
  if (!doc.load(path.c_str(), false)) {
    showLoading("Could not open this part.");
    scrollY = 0;
  } else {
    scrollY = atEnd ? max(0, doc.height() - CONTENT_H) : 0;
    drawDocView();
  }
  drawDocFooter();
}

static void drawQR(const String &text, int x, int y, int maxPx) {
  static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  static uint8_t tmpb[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
  if (!qrcodegen_encodeText(text.c_str(), tmpb, qr, qrcodegen_Ecc_MEDIUM, 1, 10, qrcodegen_Mask_AUTO, true)) return;
  int size = qrcodegen_getSize(qr);
  int scale = maxPx / (size + 4);
  int px = (size + 4) * scale;
  int ox = x + (maxPx - px) / 2, oy = y;
  tft.fillRect(ox, oy, px, px, TFT_WHITE);
  for (int r = 0; r < size; r++)
    for (int c = 0; c < size; c++)
      if (qrcodegen_getModule(qr, c, r)) tft.fillRect(ox + (c + 2) * scale, oy + (r + 2) * scale, scale, scale, TFT_BLACK);
}

static void drawWifi() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Phone Access", true);
  int y = CONTENT_Y + 10;
  if (wifiOn) {
    tft.setTextFont(2);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("Scan with a phone camera to join", SCR_W / 2, y);
    drawQR(webQRText(wcfg), 70, y + 22, 180);
    y += 214;
    tft.setTextDatum(TL_DATUM);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(C_MUTED);  tft.drawString("Network", 16, y);
    tft.setTextColor(C_TEXT);   tft.drawString(fitText(wcfg.ssid, 190), 112, y);
    y += 24;
    tft.setTextColor(C_MUTED);  tft.drawString("Password", 16, y);
    tft.setTextColor(C_TEXT);   tft.drawString(wcfg.password.length() >= 8 ? wcfg.password : String("none (open)"), 112, y);
    y += 24;
    tft.setTextColor(C_MUTED);  tft.drawString("Address", 16, y);
    tft.setTextColor(C_ACCENT); tft.drawString("http://" + webIP(), 112, y);
    y += 24;
    tft.setTextColor(C_MUTED);  tft.drawString("Phones", 16, y);
    tft.setTextColor(C_TEXT);   tft.drawString(String(webClientCount()) + " connected", 112, y);
  } else {
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("Wi-Fi hotspot is off.", SCR_W / 2, CONTENT_Y + 140);
    tft.setTextFont(2);
    tft.drawString("Turn it on to share the wiki with phones.", SCR_W / 2, CONTENT_Y + 166);
  }
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_WIFI_TOGGLE, wifiOn ? "Turn Wi-Fi off" : "Turn Wi-Fi on", I_NONE, true}};
  drawFooter(f, 2);
  lastClients = webClientCount();
}

static void drawSettingRow(int y, const char *name, const String &value) {
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_TEXT);
  tft.drawString(name, 14, y + 24);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(MR_DATUM);
  if (value.length()) tft.drawString(value, SCR_W - 110, y + 24);
  tft.drawFastHLine(8, y + 51, SCR_W - 16, C_LINE);
}

static void drawSettings() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Settings", true);
  int y = CONTENT_Y + 4;
  drawSettingRow(y, "Brightness", String(brightness * 100 / 255) + "%");
  addBtn(SCR_W - 104, y + 4, 48, 44, B_BRIGHT_DN, "-");
  addBtn(SCR_W - 54, y + 4, 48, 44, B_BRIGHT_UP, "+");
  y += 52;
  drawSettingRow(y, "Screen off after", "");
  addBtn(SCR_W - 104, y + 4, 98, 44, B_TIMEOUT, TIMEOUT_NAMES[timeoutIdx]);
  y += 52;
  drawSettingRow(y, "Invert colors", "");
  addBtn(SCR_W - 104, y + 4, 98, 44, B_INVERT, invertColors ? "On" : "Off");
  y += 52;
  drawSettingRow(y, "Touch calibration", "");
  addBtn(SCR_W - 104, y + 4, 98, 44, B_CALIB, "Redo");
  y += 64;
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED);
  uint32_t totalMB = SD.totalBytes() / (1024 * 1024), usedMB = SD.usedBytes() / (1024 * 1024);
  tft.drawString("Survival Wiki firmware " FW_VERSION, 14, y);
  tft.drawString(String(nArticles) + " articles in " + nCats + " categories", 14, y + 20);
  tft.drawString("SD card: " + String(usedMB) + " MB used of " + totalMB + " MB", 14, y + 40);
  tft.drawString("Free memory: " + String(ESP.getFreeHeap() / 1024) + " KB", 14, y + 60);
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}};
  drawFooter(f, 1);
}

static void drawNoSD() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("SURVIVAL WIKI", false);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_ACCENT);
  tft.drawString("SD card not found", SCR_W / 2, CONTENT_Y + 110);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED);
  tft.drawString("Insert the micro SD card (FAT32)", SCR_W / 2, CONTENT_Y + 150);
  tft.drawString("with the Survival Wiki files, then tap Retry.", SCR_W / 2, CONTENT_Y + 170);
  FootDef f[] = {{B_RETRY, "Retry", I_NONE, true}};
  drawFooter(f, 1);
}

static void redraw() {
  switch (screen) {
    case SC_HOME: drawHome(); break;
    case SC_CATEGORY: drawList(cats[curCat].name, false); break;
    case SC_LIBRARY: drawList("Library", true); break;
    case SC_WIFI: drawWifi(); break;
    case SC_SETTINGS: drawSettings(); break;
    case SC_NOSD: drawNoSD(); break;
    case SC_ARTICLE:
    case SC_BOOK:
      btns.clear();
      drawHeader(screen == SC_BOOK ? list[bookIdx].title : articleTitle, true);
      drawDocView();
      drawDocFooter();
      break;
  }
}

// ---------------------------------------------------------------- touch
static void calibrateTouch() {
  uint16_t cal[5];
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Touch calibration", SCR_W / 2, 200);
  tft.drawString("Tap each arrow corner as it appears", SCR_W / 2, 224);
  tft.drawString("(a fingernail or stylus works best)", SCR_W / 2, 244);
  delay(1200);
  tft.calibrateTouch(cal, TFT_MAGENTA, TFT_BLACK, 15);
  prefs.putBytes("cal", cal, sizeof(cal));
  tft.setTouch(cal);
}

static bool startSD() {
  sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  for (int i = 0; i < 3; i++) {
    if (SD.begin(PIN_SD_CS, sdSPI, 20000000)) return true;
    delay(200);
  }
  return false;
}

static void startup() {
  if (!startSD()) { Serial.println("[sd] card NOT found"); screen = SC_NOSD; redraw(); return; }
  loadConfig();
  loadCatalog();
  Serial.printf("[sd] OK: %d categories, %d articles\n", nCats, nArticles);
  if (wifiOn) webStart(wcfg);
  screen = SC_HOME;
  redraw();
}

static void scrollDoc(int dir) {
  int maxScroll = max(0, doc.height() - CONTENT_H);
  int step = CONTENT_H - 44;
  if (screen == SC_BOOK && dir > 0 && scrollY >= maxScroll && bookPart < bookParts) { bookPart++; openBookPart(false); return; }
  if (screen == SC_BOOK && dir < 0 && scrollY <= 0 && bookPart > 1) { bookPart--; openBookPart(true); return; }
  int ns = constrain(scrollY + dir * step, 0, maxScroll);
  if (ns == scrollY) return;
  scrollY = ns;
  drawDocView();
  drawDocFooter();
}

static void goBack() {
  switch (screen) {
    case SC_ARTICLE: doc.free(); screen = SC_CATEGORY; break;
    case SC_BOOK: doc.free(); screen = SC_LIBRARY; break;
    default: list.clear(); screen = SC_HOME; break;
  }
  redraw();
}

static void onButton(uint16_t id) {
  if (id >= B_ROW) {
    int idx = id - B_ROW;
    if (idx >= (int)list.size()) return;
    if (screen == SC_CATEGORY) openArticle(list[idx]);
    else if (screen == SC_LIBRARY) {
      bookIdx = idx;
      bookParts = max(1, (int)list[idx].extra.toInt());
      bookPart = 1;
      openBookPart(false);
    }
    return;
  }
  if (id >= B_TILE) {
    curCat = id - B_TILE;
    listPage = 0;
    loadCategoryList(curCat);
    screen = SC_CATEGORY;
    redraw();
    return;
  }
  switch (id) {
    case B_BACK: goBack(); break;
    case B_UP:
      if (screen == SC_ARTICLE || screen == SC_BOOK) scrollDoc(-1);
      else if (listPage > 0) { listPage--; redraw(); }
      break;
    case B_DOWN:
      if (screen == SC_ARTICLE || screen == SC_BOOK) scrollDoc(1);
      else if ((listPage + 1) * ROWS_PER_PAGE < (int)list.size()) { listPage++; redraw(); }
      break;
    case B_PREV: if (bookPart > 1) { bookPart--; openBookPart(false); } break;
    case B_NEXT: if (bookPart < bookParts) { bookPart++; openBookPart(false); } break;
    case B_LIBRARY: listPage = 0; loadLibraryList(); screen = SC_LIBRARY; redraw(); break;
    case B_WIFI: screen = SC_WIFI; redraw(); break;
    case B_SETTINGS: screen = SC_SETTINGS; redraw(); break;
    case B_WIFI_TOGGLE:
      wifiOn = !wifiOn;
      prefs.putBool("wifi", wifiOn);
      showLoading(wifiOn ? "Starting hotspot..." : "Stopping hotspot...");
      if (wifiOn) webStart(wcfg); else webStop();
      redraw();
      break;
    case B_BRIGHT_DN: brightness = max(20, brightness - 30); setBacklight(brightness); prefs.putUChar("bright", brightness); redraw(); break;
    case B_BRIGHT_UP: brightness = min(255, brightness + 30); setBacklight(brightness); prefs.putUChar("bright", brightness); redraw(); break;
    case B_TIMEOUT: timeoutIdx = (timeoutIdx + 1) % 5; prefs.putUChar("tmo", timeoutIdx); redraw(); break;
    case B_INVERT: invertColors = !invertColors; tft.invertDisplay(invertColors); prefs.putBool("inv", invertColors); redraw(); break;
    case B_CALIB: calibrateTouch(); redraw(); break;
    case B_RETRY: startup(); break;
  }
}

static void handleTap(int x, int y) {
  for (size_t i = 0; i < btns.size(); i++) {
    Btn b = btns[i];
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
      if (!b.enabled) return;
      if (b.id >= B_TILE) {             // tiles and list rows: quick highlight frame
        tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, C_ACCENT);
        tft.drawRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 8, C_ACCENT);
      } else if (!(b.y == 0 && b.id == B_BACK)) {
        drawButton(b, true);
      }
      onButton(b.id);
      return;
    }
  }
}

// ---------------------------------------------------------------- main
void setup() {
  Serial.begin(115200);
  pinMode(PIN_AUDIO_EN, OUTPUT); digitalWrite(PIN_AUDIO_EN, HIGH);   // speaker amp off
  pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, HIGH);         // RGB LED off
  pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, HIGH);
  pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, HIGH);

  prefs.begin("swiki", false);
  brightness = prefs.getUChar("bright", 200);
  timeoutIdx = prefs.getUChar("tmo", 2) % 5;
  invertColors = prefs.getBool("inv", false);
  wifiOn = prefs.getBool("wifi", true);

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(invertColors);
  tft.fillScreen(C_BG);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, 0);
  setBacklight(brightness);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(nullptr);

  // splash
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_ACCENT);
  tft.drawString("SURVIVAL", SCR_W / 2, 200);
  tft.setTextColor(C_TEXT);
  tft.drawString("WIKI", SCR_W / 2, 242);
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED);
  tft.drawString("offline field reference", SCR_W / 2, 280);
  tft.drawString("hold the screen now to recalibrate touch", SCR_W / 2, 440);

  uint16_t cal[5];
  bool haveCal = prefs.getBytes("cal", cal, sizeof(cal)) == sizeof(cal);
  delay(1500);
  if (!haveCal || tft.getTouchRawZ() > 500) calibrateTouch();
  else tft.setTouch(cal);

  startup();
  lastTouchMs = millis();
}

void loop() {
  static bool wasDown = false;
  uint16_t tx, ty;
  bool down = tft.getTouch(&tx, &ty, 500);
  uint32_t now = millis();

  if (down && !wasDown) {
    lastTouchMs = now;
    if (sleeping) {               // first tap only wakes the screen
      sleeping = false;
      setBacklight(brightness);
    } else {
      handleTap(tx, ty);
    }
  }
  if (down) lastTouchMs = now;
  wasDown = down;

  uint16_t tmo = TIMEOUTS_S[timeoutIdx];
  if (!sleeping && tmo && now - lastTouchMs > (uint32_t)tmo * 1000) {
    sleeping = true;
    setBacklight(0);
  }

  // keep the phone counter fresh
  if (!sleeping && now - lastStatusMs > 2000) {
    lastStatusMs = now;
    int c = webClientCount();
    if (c != lastClients) {
      lastClients = c;
      if (screen == SC_HOME) drawWifiBadge();
      else if (screen == SC_WIFI) redraw();
    }
  }
  delay(15);
}
