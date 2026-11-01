// =====================================================================
//  Survival Wiki - offline survival reference for the ESP32 CYD
//  Supports:
//   - Hosyond / LCDWiki 3.5" ESP32-32E (E32R35T) [320x480 ST7796]
//   - Sunton 2.8" ESP32-2432S028R CYD [240x320 ILI9341]
//  * Touch UI on the screen, reading articles + pictures from SD
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
#include "mesh.h"
#include "gps.h"
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
enum Screen { SC_HOME, SC_CATEGORY, SC_ARTICLE, SC_LIBRARY, SC_BOOK, SC_WIFI, SC_SETTINGS, SC_NOSD, SC_LIGHT, SC_MESH, SC_QUICK, SC_KEYS, SC_NEAR, SC_MAP };
static Screen screen = SC_HOME;
static int curCat = 0, listPage = 0, scrollY = 0, homePage = 0;
static int bookIdx = 0, bookPart = 1, bookParts = 1;
static String articleTitle;

static uint8_t brightness = 200;
static uint8_t timeoutIdx = 2;
static const uint16_t TIMEOUTS_S[] = {0, 60, 180, 300, 600};
static const char *TIMEOUT_NAMES[] = {"Never", "1 min", "3 min", "5 min", "10 min"};
static bool invertColors = false;
static bool wifiOn = true;
static bool sleeping = false;
static bool nightAuto = false;       // dim the backlight when the room is dark (CYD light sensor)
static bool nightNow = false;        // currently dimmed by night mode
static uint8_t lightMode = 0;        // flashlight: 0 white, 1 red, 2 SOS strobe
static bool sdUp = false;
static bool meshEnabled = true;      // ESP-NOW text messaging between boards
static String kbBuf;                 // text being typed on the on-screen keyboard
static uint8_t kbTarget = 0;         // 0 = new message, 1 = this board's mesh name
static bool kbShift = false;
static int mapZ = 13, mapMinZ = 8, mapMaxZ = 16;   // current / allowed zoom levels
static int32_t mapCx = 0, mapCy = 0;                // map centre, in world pixels at zoom mapZ
static bool mapReady = false;                       // /maps/center.txt was found
static uint32_t ledOffMs = 0;        // green "message received" blink
static int goodboyIdx = -1;         // index of the hidden "goodboy" category, -1 if absent
static uint32_t lastTouchMs = 0, lastStatusMs = 0;
static void startMesh();             // (re)start mesh messaging on the right radio channel (defined below)
static void saveBackup();            // settings backup to the SD card (defined below)
static void restoreBackup();
static int lastClients = -1;

// ---------------------------------------------------------------- buttons
enum BtnId : uint16_t {
  B_NONE = 0, B_BACK, B_UP, B_DOWN, B_PREV, B_NEXT, B_LIBRARY, B_WIFI, B_SETTINGS,
  B_WIFI_TOGGLE, B_BRIGHT_DN, B_BRIGHT_UP, B_TIMEOUT, B_INVERT, B_CALIB, B_RETRY,
  B_LIGHT, B_NIGHT, B_BACKUP_SAVE, B_BACKUP_LOAD,
  B_MESH, B_MESH_TOGGLE, B_MESH_QUICK, B_MESH_TYPE, B_MESH_NAME, B_MESH_NEAR, B_KB_DEL, B_KB_SHIFT, B_KB_SEND, B_MAP, B_MAP_IN, B_MAP_OUT,
  B_QUICK_BASE = 60,           // 60..69: quick messages
  B_TILE = 100, B_ROW = 200,
  B_KEY_BASE = 1000            // 1000 + character: on-screen keyboard keys
};
enum Icon : uint8_t { I_NONE, I_UP, I_DOWN, I_LEFT, I_RIGHT };
struct Btn { int16_t x, y, w, h; uint16_t id; String label; uint8_t icon; bool enabled; };
static std::vector<Btn> btns;

static const int ROWS_PER_PAGE = CONTENT_H / ROW_H;

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
  goodboyIdx = -1;
  for (int i = 0; i < nCats; i++) if (cats[i].id == "goodboy") goodboyIdx = i;
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
  int half = (SCR_W < 300) ? 8 : 10;
  int hSide = (SCR_W < 300) ? 5 : 6;
  switch (icon) {
    case I_UP:    tft.fillTriangle(cx - half, cy + hSide, cx + half, cy + hSide, cx, cy - (hSide + 1), col); break;
    case I_DOWN:  tft.fillTriangle(cx - half, cy - hSide, cx + half, cy - hSide, cx, cy + (hSide + 1), col); break;
    case I_LEFT:  tft.fillTriangle(cx + hSide, cy - half, cx + hSide, cy + half, cx - (hSide + 1), cy, col); break;
    case I_RIGHT: tft.fillTriangle(cx - hSide, cy - half, cx - hSide, cy + half, cx + (hSide + 1), cy, col); break;
  }
}

static void drawButton(const Btn &b, bool pressed) {
  uint16_t bg = pressed ? C_PRESS : C_PANEL;
  tft.fillRoundRect(b.x + 2, b.y + 2, b.w - 4, b.h - 4, (SCR_W < 300 ? 5 : 6), bg);
  uint16_t fg = b.enabled ? C_TEXT : C_LINE;
  int cx = b.x + b.w / 2, cy = b.y + b.h / 2;
  if (b.icon != I_NONE) {
    if (b.label.length()) {
      if (SCR_W < 300) tft.setTextFont(2); else tft.setFreeFont(&FreeSansBold9pt7b);
      int tw = tft.textWidth(b.label);
      int total = 18 + 6 + tw;
      int ix = cx - total / 2 + 9;
      drawIcon(b.icon, ix, cy, b.enabled ? C_ACCENT : C_LINE);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(fg);
      tft.drawString(b.label, ix + (SCR_W < 300 ? 12 : 15), cy);
    } else {
      drawIcon(b.icon, cx, cy, b.enabled ? C_ACCENT : C_LINE);
    }
  } else {
    if (SCR_W < 300) tft.setTextFont(2); else tft.setFreeFont(&FreeSansBold9pt7b);
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
  int bw = (SCR_W < 300) ? 58 : 74;
  int x = SCR_W - bw, y = 0;
  tft.fillRect(x, y, bw, HEADER_H, C_HEADER);
  int cx = x + (SCR_W < 300 ? 12 : 16);
  int cy = (SCR_W < 300 ? 22 : 30);
  uint16_t col = wifiOn ? 0x07E9 : C_LINE;
  int rStart = (SCR_W < 300) ? 3 : 4;
  int rEnd = (SCR_W < 300) ? 11 : 14;
  int rStep = (SCR_W < 300) ? 4 : 5;
  for (int r = rStart; r <= rEnd; r += rStep) {
    tft.drawArc(cx, cy, r + 1, r - 1, 135, 225, col, C_HEADER, false);
  }
  tft.fillCircle(cx, cy, 2, col);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(wifiOn ? C_TEXT : C_MUTED);
  tft.drawString(wifiOn ? String(webClientCount()) + " on" : String("off"), cx + (SCR_W < 300 ? 11 : 14), (SCR_W < 300 ? 18 : 24));
}

static void drawHeader(const String &title, bool back) {
  tft.fillRect(0, 0, SCR_W, HEADER_H, C_HEADER);
  tft.drawFastHLine(0, HEADER_H - 1, SCR_W, C_ACCENT);
  int tx = (SCR_W < 300 ? 8 : 12);
  int bw = (SCR_W < 300) ? 58 : 74;
  if (back) {
    int backBtnW = (SCR_W < 300) ? 38 : 52;
    btns.push_back({0, 0, (int16_t)backBtnW, HEADER_H, B_BACK, "", I_LEFT, true});
    drawIcon(I_LEFT, backBtnW / 2, HEADER_H / 2, C_ACCENT);
    tx = backBtnW;
  }
  if (SCR_W < 300) tft.setFreeFont(&FreeSansBold9pt7b); else tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_TEXT);
  int maxW = SCR_W - tx - (back ? 10 : (bw + 6));
  String t = fitText(title, maxW);
  if (SCR_W >= 300 && (tft.textWidth(t) > maxW || t.endsWith("..."))) {   // long titles: smaller font
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
  } else if (id == "weather") {          // cloud with a lightning bolt
    tft.fillCircle(cx - 6, cy - 3, 6, w);
    tft.fillCircle(cx + 3, cy - 6, 7, w);
    tft.fillCircle(cx + 9, cy - 1, 5, w);
    tft.fillRect(cx - 9, cy - 3, 22, 7, w);
    tft.fillTriangle(cx + 1, cy + 1, cx - 5, cy + 11, cx + 1, cy + 8, TFT_YELLOW);
    tft.fillTriangle(cx - 2, cy + 6, cx + 5, cy + 6, cx - 1, cy + 14, TFT_YELLOW);
  } else if (id == "vehicles") {          // car
    tft.fillRoundRect(cx - 12, cy - 1, 25, 9, 3, w);
    tft.fillTriangle(cx - 8, cy - 1, cx - 4, cy - 8, cx + 4, cy - 8, w);
    tft.fillTriangle(cx - 8, cy - 1, cx + 4, cy - 8, cx + 8, cy - 1, w);
    tft.fillCircle(cx - 7, cy + 8, 3, C_BG); tft.drawCircle(cx - 7, cy + 8, 3, w);
    tft.fillCircle(cx + 7, cy + 8, 3, C_BG); tft.drawCircle(cx + 7, cy + 8, 3, w);
  } else if (id == "radio") {             // handheld radio with an antenna
    tft.fillRoundRect(cx - 8, cy - 4, 17, 17, 3, w);
    tft.drawLine(cx + 4, cy - 4, cx + 9, cy - 13, w); tft.drawLine(cx + 5, cy - 4, cx + 10, cy - 13, w);
    tft.fillCircle(cx, cy + 2, 3, C_BG);
    tft.fillRect(cx - 5, cy + 8, 11, 2, C_BG);
  } else if (id == "family") {            // adult and child
    tft.fillCircle(cx - 5, cy - 8, 4, w);
    tft.fillRoundRect(cx - 10, cy - 2, 11, 14, 3, w);
    tft.fillCircle(cx + 8, cy - 1, 3, w);
    tft.fillRoundRect(cx + 3, cy + 3, 10, 9, 3, w);
  } else if (id == "home") {              // house with a door
    tft.fillTriangle(cx - 13, cy - 1, cx + 13, cy - 1, cx, cy - 12, w);
    tft.fillRect(cx - 9, cy - 1, 19, 12, w);
    tft.fillRect(cx - 2, cy + 4, 5, 7, C_BG);
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
  const int cols = 2, rows = 4, gap = TILE_GAP, perPage = cols * rows;
  int vis[MAX_CATEGORIES], nVis = 0;
  for (int i = 0; i < nCats; i++) if (i != goodboyIdx) vis[nVis++] = i;   // goodboy is the hidden easter egg
  int pages = max(1, (nVis + perPage - 1) / perPage);
  if (homePage >= pages) homePage = pages - 1;
  const int strip = pages > 1 ? (SCR_W < 300 ? 26 : 34) : 0;   // page switcher under the tiles
  int tw = (SCR_W - gap * (cols + 1)) / cols;
  int th = (CONTENT_H - strip - gap * (rows + 1)) / rows;
  if (strip) {
    int sy = SCR_H - FOOTER_H - strip;
    char pg[12]; snprintf(pg, sizeof(pg), "%d / %d", homePage + 1, pages);
    addBtn(gap, sy, SCR_W / 3 - gap, strip - 2, B_PREV, "", I_LEFT, homePage > 0);
    addBtn(SCR_W - SCR_W / 3, sy, SCR_W / 3 - gap, strip - 2, B_NEXT, "", I_RIGHT, homePage < pages - 1);
    tft.setTextFont(2); tft.setTextDatum(MC_DATUM); tft.setTextColor(C_MUTED);
    tft.drawString(pg, SCR_W / 2, sy + strip / 2 - 1);
  }
  for (int k = homePage * perPage; k < nVis && k < (homePage + 1) * perPage; k++) {
    int i = vis[k], slot = k - homePage * perPage;
    int x = gap + (slot % cols) * (tw + gap);
    int y = CONTENT_Y + gap + (slot / cols) * (th + gap);
    btns.push_back({(int16_t)x, (int16_t)y, (int16_t)tw, (int16_t)th, (uint16_t)(B_TILE + i), "", I_NONE, true});
    tft.fillRoundRect(x, y, tw, th, (SCR_W < 300 ? 8 : 10), cats[i].color);
    if (SCR_W < 300) {
      // 2.8" compact split: icon on left, title + count on right
      drawCatIcon(cats[i].id, x + 18, y + th / 2);
      tft.setTextFont(2);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(fitText(cats[i].name, tw - 38), x + 34, y + 8);
      tft.setTextFont(1);
      tft.drawString(String(cats[i].count) + (cats[i].count == 1 ? " art." : " arts."), x + 35, y + 26);
    } else {
      // 3.5" full vertical stack
      drawCatIcon(cats[i].id, x + 24, y + 26);
      tft.setFreeFont(&FreeSansBold9pt7b);
      tft.setTextDatum(TL_DATUM);
      tft.setTextColor(TFT_WHITE);
      tft.drawString(fitText(cats[i].name, tw - 16), x + 10, y + th - 40);
      tft.setTextFont(2);
      tft.drawString(String(cats[i].count) + (cats[i].count == 1 ? " article" : " articles"), x + 10, y + th - 20);
    }
  }
  if (nCats == 0) {
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("No articles found on the SD card.", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 90 : 150));
    tft.drawString("Copy the 'sdcard' folder contents to it.", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 112 : 172));
  }
  static char meshLabel[12];
  snprintf(meshLabel, sizeof(meshLabel), meshUnread() > 0 ? "Mesh*" : "Mesh");
  FootDef f[] = {{B_LIBRARY, "Books", I_NONE, true}, {B_WIFI, "Wi-Fi", I_NONE, true},
                 {B_LIGHT, "Light", I_NONE, true}, {B_MESH, meshLabel, I_NONE, true},
                 {B_MAP, "Map", I_NONE, true}, {B_SETTINGS, "Set", I_NONE, true}};
  drawFooter(f, 6);
}

static void drawList(const String &title, bool books) {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader(title, true);
  int pages = max(1, (int)((list.size() + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE));
  if (listPage >= pages) listPage = pages - 1;
  int first = listPage * ROWS_PER_PAGE;
  for (int r = 0; r < ROWS_PER_PAGE && first + r < (int)list.size(); r++) {
    int y = CONTENT_Y + r * ROW_H;
    const Entry &e = list[first + r];
    btns.push_back({0, (int16_t)y, SCR_W, (int16_t)ROW_H, (uint16_t)(B_ROW + first + r), "", I_NONE, true});
    if (SCR_W < 300) tft.setTextFont(2); else tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TEXT);
    if (books) {
      tft.drawString(fitText(e.title, SCR_W - (SCR_W < 300 ? 40 : 50)), (SCR_W < 300 ? 8 : 12), y + (SCR_W < 300 ? 13 : 17));
      tft.setTextFont(SCR_W < 300 ? 1 : 2);
      tft.setTextColor(C_MUTED);
      tft.drawString(e.extra + " parts", (SCR_W < 300 ? 8 : 12), y + (SCR_W < 300 ? 27 : 36));
    } else {
      tft.drawString(fitText(e.title, SCR_W - (SCR_W < 300 ? 36 : 50)), (SCR_W < 300 ? 8 : 12), y + ROW_H / 2);
    }
    drawIcon(I_RIGHT, SCR_W - (SCR_W < 300 ? 14 : 20), y + ROW_H / 2, C_LINE);
    tft.drawFastHLine(8, y + ROW_H - 1, SCR_W - 16, C_LINE);
  }
  if (list.empty()) {
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString(books ? "No manuals in /library yet." : "Nothing here yet.", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 80 : 120));
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
  int y = CONTENT_Y + (SCR_W < 300 ? 6 : 10);
  if (wifiOn) {
    int qrMax = (SCR_W < 300) ? 120 : 180;
    tft.setTextFont(2);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("Scan to connect phone", SCR_W / 2, y);
    drawQR(webQRText(wcfg), (SCR_W - qrMax) / 2, y + 18, qrMax);
    y += qrMax + (SCR_W < 300 ? 22 : 24);
    tft.setTextDatum(TL_DATUM);
    if (SCR_W < 300) tft.setTextFont(2); else tft.setFreeFont(&FreeSans9pt7b);
    int col1W = (SCR_W < 300) ? 76 : 112;
    int lineGap = (SCR_W < 300) ? 20 : 24;
    tft.setTextColor(C_MUTED);  tft.drawString("Network", 12, y);
    tft.setTextColor(C_TEXT);   tft.drawString(fitText(wcfg.ssid, SCR_W - col1W - 8), col1W, y);
    y += lineGap;
    tft.setTextColor(C_MUTED);  tft.drawString("Password", 12, y);
    tft.setTextColor(C_TEXT);   tft.drawString(wcfg.password.length() >= 8 ? wcfg.password : String("none (open)"), col1W, y);
    y += lineGap;
    tft.setTextColor(C_MUTED);  tft.drawString("Address", 12, y);
    tft.setTextColor(C_ACCENT); tft.drawString("http://" + webIP(), col1W, y);
    y += lineGap;
    tft.setTextColor(C_MUTED);  tft.drawString("Phones", 12, y);
    tft.setTextColor(C_TEXT);   tft.drawString(String(webClientCount()) + " connected", col1W, y);
  } else {
    if (SCR_W < 300) tft.setTextFont(2); else tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("Wi-Fi hotspot is off.", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 90 : 140));
    tft.setTextFont(2);
    tft.drawString("Turn it on to share the wiki with phones.", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 116 : 166));
  }
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_WIFI_TOGGLE, wifiOn ? "Turn Wi-Fi off" : "Turn Wi-Fi on", I_NONE, true}};
  drawFooter(f, 2);
  lastClients = webClientCount();
}

static int settingRowH() { return (SCR_W < 300) ? 32 : 48; }

static void drawSettingRow(int y, const char *name, const String &value) {
  int rh = settingRowH();
  if (SCR_W < 300) tft.setTextFont(2); else tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C_TEXT);
  tft.drawString(name, 12, y + rh / 2);
  tft.setTextColor(C_ACCENT);
  tft.setTextDatum(MR_DATUM);
  int btnW = (SCR_W < 300) ? 80 : 98;
  if (value.length()) tft.drawString(value, SCR_W - btnW - 14, y + rh / 2);
  tft.drawFastHLine(8, y + rh - 1, SCR_W - 16, C_LINE);
}

static void drawSettings() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Settings", true);
  int y = CONTENT_Y + 2;
  int rowStep = settingRowH();
  int btnH = rowStep - 4;
  int btnW = (SCR_W < 300) ? 80 : 98;
  int halfBtnW = (btnW - 6) / 2;

  drawSettingRow(y, "Brightness", String(brightness * 100 / 255) + "%");
  addBtn(SCR_W - btnW - 6, y + 2, halfBtnW, btnH, B_BRIGHT_DN, "-");
  addBtn(SCR_W - halfBtnW - 4, y + 2, halfBtnW, btnH, B_BRIGHT_UP, "+");
  y += rowStep;

  drawSettingRow(y, "Screen off after", "");
  addBtn(SCR_W - btnW - 6, y + 2, btnW, btnH, B_TIMEOUT, TIMEOUT_NAMES[timeoutIdx]);
  y += rowStep;

  drawSettingRow(y, "Invert colors", "");
  addBtn(SCR_W - btnW - 6, y + 2, btnW, btnH, B_INVERT, invertColors ? "On" : "Off");
  y += rowStep;

#if PIN_LDR >= 0
  drawSettingRow(y, "Auto night dim", "");
  addBtn(SCR_W - btnW - 6, y + 2, btnW, btnH, B_NIGHT, nightAuto ? "On" : "Off");
  y += rowStep;
#endif

  drawSettingRow(y, "Touch calibration", "");
  addBtn(SCR_W - btnW - 6, y + 2, btnW, btnH, B_CALIB, "Redo");
  y += rowStep;

  drawSettingRow(y, "SD backup", "");
  addBtn(SCR_W - btnW - 6, y + 2, halfBtnW, btnH, B_BACKUP_SAVE, "Save");
  addBtn(SCR_W - halfBtnW - 4, y + 2, halfBtnW, btnH, B_BACKUP_LOAD, "Load");
  y += rowStep + 6;

  tft.setTextFont(SCR_W < 300 ? 1 : 2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_MUTED);
  uint32_t totalMB = SD.totalBytes() / (1024 * 1024), usedMB = SD.usedBytes() / (1024 * 1024);
  int lineH = (SCR_W < 300 ? 12 : 18);
  int shownCats = nCats - (goodboyIdx >= 0 ? 1 : 0);
  tft.drawString("Survival Wiki " FW_VERSION " (" BOARD_NAME ")", 12, y);
  tft.drawString(String(nArticles) + " articles in " + shownCats + " categories", 12, y + lineH);
  tft.drawString("SD card: " + String(usedMB) + " MB used of " + totalMB + " MB", 12, y + lineH * 2);
  if (gpsPresent()) {
    GpsFix g = gpsGet();
    String gl = g.sentences == 0 ? String("GPS: no data (check wiring)")
              : g.valid ? "GPS: " + String(g.lat, 5) + ", " + String(g.lon, 5) + " (" + g.sats + " sats)"
              : "GPS: searching, " + String(g.sats) + " sats";
    tft.drawString(gl, 12, y + lineH * 3);
  }
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
  tft.drawString("SD card not found", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 70 : 110));
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED);
  tft.drawString("Insert the micro SD card (FAT32)", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 100 : 150));
  tft.drawString("with the Survival Wiki files, then tap Retry.", SCR_W / 2, CONTENT_Y + (SCR_W < 300 ? 120 : 170));
  // The flashlight and mesh messaging don't need the card, so keep them reachable.
  FootDef f[] = {{B_RETRY, "Retry", I_NONE, true}, {B_LIGHT, "Light", I_NONE, true}, {B_MESH, "Mesh", I_NONE, true}};
  drawFooter(f, 3);
}

// ---------------------------------------------------------------- mesh messaging screens
// Greedy word wrap of s into at most `lines` lines no wider than maxW pixels (current font).
static int wrapLines(const String &s, int maxW, String *out, int lines) {
  int n = 0;
  String cur;
  int i = 0;
  while (i < (int)s.length() && n < lines) {
    int sp = s.indexOf(' ', i);
    String word = (sp < 0) ? s.substring(i) : s.substring(i, sp);
    String trial = cur.length() ? cur + " " + word : word;
    if (tft.textWidth(trial) <= maxW) cur = trial;
    else if (cur.length()) { out[n++] = cur; cur = ""; if (n >= lines) break; continue; }
    else {                                           // a single word wider than the line: cut it
      int k = word.length();
      while (k > 1 && tft.textWidth(word.substring(0, k)) > maxW) k--;
      out[n++] = word.substring(0, k);
      cur = "";
      i += k;
      continue;
    }
    i = (sp < 0) ? s.length() : sp + 1;
  }
  if (cur.length() && n < lines) out[n++] = cur;
  return n;
}

static String ageText(uint32_t ms) {
  uint32_t s = (millis() - ms) / 1000;
  if (s < 60) return "now";
  if (s < 3600) return String(s / 60) + "m";
  return String(s / 3600) + "h";
}

static void drawMesh() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Mesh", true);
  int y = CONTENT_Y + 3;
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(meshActive() ? C_ACCENT : C_MUTED);
  String st = String(meshName()) + (meshActive() ? "  " + String(meshPeersHeard()) + " heard" : "  off");
  tft.drawString(fitText(st, SCR_W - 76), 8, y + 11);
  addBtn(SCR_W - 66, y, 60, 22, B_MESH_TOGGLE, meshActive() ? "Off" : "On");
  int top = y + 28, bottom = SCR_H - FOOTER_H - 2;
  int rh = (SCR_W < 300) ? 36 : 44;
  int rows = (bottom - top) / rh;
  int n = meshCount();
  if (!meshActive()) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("Mesh is off. Tap On to talk to", SCR_W / 2, top + 40);
    tft.drawString("nearby boards.", SCR_W / 2, top + 58);
  } else if (n == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("No messages yet.", SCR_W / 2, top + 40);
    tft.drawString("Tap Quick or Type to send one.", SCR_W / 2, top + 58);
  }
  int first = max(0, n - rows);
  int ry = top;
  for (int i = first; i < n; i++) {
    const MeshMsg &m = meshAt(i);
    bool me = m.mine && strcmp(m.from, meshName()) == 0;       // sent from a phone: show the phone's name
    String line = (me ? String("me") : String(m.from)) + ": " + m.text;
    String ln[2];
    int cnt = wrapLines(line, SCR_W - 52, ln, 2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(m.mine ? C_ACCENT : C_TEXT);
    for (int k = 0; k < cnt; k++) tft.drawString(ln[k], 8, ry + 2 + k * 16);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString(ageText(m.ms), SCR_W - 6, ry + 2);
    if (m.mine) {                                              // like "Read": how many boards said they got it
      tft.setTextFont(1);
      tft.setTextColor(m.ackN ? 0x07E0 : C_MUTED);
      tft.drawString(m.ackN ? "heard " + String(m.ackN) : String("sent"), SCR_W - 6, ry + 18);
      tft.setTextFont(2);
    }
    tft.drawFastHLine(8, ry + rh - 2, SCR_W - 16, C_LINE);
    ry += rh;
  }
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_MESH_QUICK, "Quick", I_NONE, meshActive()},
                 {B_MESH_TYPE, "Type", I_NONE, meshActive()}, {B_MESH_NAME, "Name", I_NONE, true},
                 {B_MESH_NEAR, "Near", I_NONE, meshActive()}};
  drawFooter(f, 5);
}

// Nearby boards with signal bars and a rough distance. Signal strength is only a guess at range.
static String distText(int rssi) {
  float d = meshDistanceM(rssi);
  if (d < 0) return "n/a";                              // no signal reading (hotspot on, or only heard via a relay)
  if (d < 10) return "~" + String((int)(d + 0.5f)) + " m";
  if (d < 100) return "~" + String((int)(d / 5 + 0.5f) * 5) + " m";
  return "~" + String((int)(d / 10 + 0.5f) * 10) + " m";
}

static void drawNear() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Nearby", true);
  MeshPeer pl[8];
  int n = meshPeerList(pl, 8);
  int rh = (SCR_W < 300) ? 40 : 48;
  int y = CONTENT_Y + 6;
  tft.setTextFont(2);
  if (n == 0) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_MUTED);
    tft.drawString("No boards heard yet.", SCR_W / 2, y + 50);
    tft.drawString("Boards say hello every 30 s.", SCR_W / 2, y + 70);
  }
  for (int i = 0; i < n; i++) {
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TEXT);
    tft.drawString(fitText(pl[i].name, SCR_W / 2 - 40), 10, y + 14);
    tft.setTextColor(C_MUTED);
    tft.drawString(String(pl[i].rssi ? String((int)pl[i].rssi) + " dBm" : String("-")) + "  " +
                   (pl[i].ageS < 5 ? String("now") : String(pl[i].ageS) + "s ago"), 10, y + 31);
    // signal bars
    int bars = meshBars(pl[i].rssi);
    int bx = SCR_W / 2 + 6;
    for (int b = 0; b < 4; b++) {
      int bh = 6 + b * 5;
      uint16_t col = (b < bars) ? (bars >= 3 ? 0x07E0 : (bars == 2 ? 0xFFE0 : C_ACCENT)) : C_LINE;
      tft.fillRect(bx + b * 8, y + 26 - bh, 6, bh, col);
    }
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_ACCENT);
    tft.drawString(distText(pl[i].rssi), SCR_W - 10, y + 20);
    tft.drawFastHLine(8, y + rh - 3, SCR_W - 16, C_LINE);
    y += rh;
  }
  tft.setTextFont(1);
  tft.setTextDatum(BC_DATUM);
  tft.setTextColor(C_MUTED);
  // The signal listener can't run while the hotspot is on (phones couldn't join), so say why there is no distance.
  tft.drawString(webRunning() ? "Turn Wi-Fi off to see distance" : "Distance is a rough guess from signal strength",
                 SCR_W / 2, SCR_H - FOOTER_H - 4);
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}};
  drawFooter(f, 1);
}

static void drawQuick() {
  static const char *QUICK[] = {"OK", "Need help", "Heading out", "At camp", "Water found", "Come to me"};
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Quick message", true);
  int y = CONTENT_Y + 4;
  for (int i = 0; i < 6; i++) {
    addBtn(8, y, SCR_W - 16, ROW_H - 4, B_QUICK_BASE + i, QUICK[i]);
    y += ROW_H;
  }
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}};
  drawFooter(f, 1);
}

#define KB_MAX_MSG  50
static int kbMax() { return kbTarget == 0 ? KB_MAX_MSG : MESH_NAME_LEN - 1; }

static void drawKbText() {
  int bx = 6, by = CONTENT_Y + 4, bw = SCR_W - 12, bh = 44;
  tft.fillRoundRect(bx, by, bw, bh, 6, C_PANEL);
  tft.drawRoundRect(bx, by, bw, bh, 6, C_LINE);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TEXT);
  String ln[2];
  int cnt = wrapLines(kbBuf + "_", bw - 12, ln, 2);
  for (int k = 0; k < cnt; k++) tft.drawString(ln[k], bx + 6, by + 5 + k * 17);
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(C_MUTED);
  tft.drawString(String(kbBuf.length()) + "/" + kbMax(), bx + bw - 4, by + bh - 2);
}

static void drawKeys() {
  static const char *ROWS[] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader(kbTarget == 0 ? "New message" : "Your name", true);
  drawKbText();
  int kw = SCR_W / 10;
  int top = CONTENT_Y + 54;
  int kh = (SCR_H - FOOTER_H - 4 - top) / 5;
  for (int r = 0; r < 4; r++) {
    int len = strlen(ROWS[r]);
    int x0 = (SCR_W - len * kw) / 2;
    if (r == 3) x0 = kw / 2;                         // leave room for delete on the right
    for (int c = 0; c < len; c++) {
      char ch = ROWS[r][c];
      String label = String(kbShift && isalpha(ch) ? (char)toupper(ch) : ch);
      addBtn(x0 + c * kw, top + r * kh, kw, kh, B_KEY_BASE + ch, label);
    }
    if (r == 3) addBtn(x0 + len * kw, top + r * kh, SCR_W - (x0 + len * kw), kh, B_KB_DEL, "<-");
  }
  int y = top + 4 * kh;
  addBtn(0, y, kw * 2, kh, B_KB_SHIFT, kbShift ? "ABC" : "abc");
  addBtn(kw * 2, y, kw * 4, kh, B_KEY_BASE + ' ', "space");
  addBtn(kw * 6, y, kw * 2, kh, B_KEY_BASE + '.', ".");
  addBtn(kw * 8, y, SCR_W - kw * 8, kh, B_KEY_BASE + '?', "?");
  FootDef f[] = {{B_BACK, "Cancel", I_NONE, true}, {B_KB_SEND, kbTarget == 0 ? "Send" : "Save", I_NONE, true}};
  drawFooter(f, 2);
}

// ---------------------------------------------------------------- offline map viewer
// Tiles live on the SD card as /maps/{zoom}/{x}/{y}.jpg (256x256, standard "slippy map" layout),
// made by scripts/get-map-tiles.ps1. /maps/center.txt holds: lat lon startZoom minZoom maxZoom.
static double mapWorld(int z) { return 256.0 * (double)(1UL << z); }

static void mapLatLonToPx(double lat, double lon, int z, int32_t &px, int32_t &py) {
  double n = mapWorld(z), r = lat * M_PI / 180.0;
  px = (int32_t)((lon + 180.0) / 360.0 * n);
  py = (int32_t)((1.0 - log(tan(r) + 1.0 / cos(r)) / M_PI) / 2.0 * n);
}

static void mapPxToLatLon(int32_t px, int32_t py, int z, double &lat, double &lon) {
  double n = mapWorld(z);
  lon = px / n * 360.0 - 180.0;
  lat = atan(sinh(M_PI * (1.0 - 2.0 * py / n))) * 180.0 / M_PI;
}

static bool mapLoadCenter() {
  mapReady = false;
  if (!sdUp) return false;
  File f = SD.open("/maps/center.txt", FILE_READ);
  if (!f) return false;
  String l = readLine(f);
  f.close();
  double lat, lon;
  int z, zmin, zmax;
  if (sscanf(l.c_str(), "%lf %lf %d %d %d", &lat, &lon, &z, &zmin, &zmax) != 5) return false;
  mapMinZ = constrain(zmin, 0, 18);
  mapMaxZ = constrain(zmax, mapMinZ, 18);
  mapZ = constrain(z, mapMinZ, mapMaxZ);
  mapLatLonToPx(lat, lon, mapZ, mapCx, mapCy);
  mapReady = true;
  return true;
}

static int mapClipX0, mapClipY0, mapClipX1, mapClipY1;
static bool mapJpgOut(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bmp) {
  int x0 = max<int>(x, mapClipX0), y0 = max<int>(y, mapClipY0);
  int x1 = min<int>(x + w, mapClipX1), y1 = min<int>(y + h, mapClipY1);
  if (x1 <= x0 || y1 <= y0) return true;                    // block is outside the map area
  for (int yy = y0; yy < y1; yy++) tft.pushImage(x0, yy, x1 - x0, 1, bmp + (yy - y) * w + (x0 - x));
  return true;
}

static void mapDrawTiles() {
  mapClipX0 = 0; mapClipX1 = SCR_W;
  mapClipY0 = CONTENT_Y; mapClipY1 = CONTENT_Y + CONTENT_H;
  tft.fillRect(0, CONTENT_Y, SCR_W, CONTENT_H, C_PANEL);
  int32_t left = mapCx - SCR_W / 2, top = mapCy - CONTENT_H / 2;
  int tiles = 1 << mapZ;
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(mapJpgOut);
  for (int ty = (int)floor(top / 256.0); ty <= (int)floor((top + CONTENT_H - 1) / 256.0); ty++) {
    for (int tx = (int)floor(left / 256.0); tx <= (int)floor((left + SCR_W - 1) / 256.0); tx++) {
      if (tx < 0 || ty < 0 || tx >= tiles || ty >= tiles) continue;
      char path[40];
      snprintf(path, sizeof(path), "/maps/%d/%d/%d.jpg", mapZ, tx, ty);
      TJpgDec.drawFsJpg(tx * 256 - left, CONTENT_Y + ty * 256 - top, path, SD);   // a missing tile just stays grey
    }
  }
  // crosshair at the centre and the centre's coordinates
  int cx = SCR_W / 2, cy = CONTENT_Y + CONTENT_H / 2;
  tft.drawCircle(cx, cy, 7, TFT_RED);
  tft.drawFastHLine(cx - 11, cy, 23, TFT_RED);
  tft.drawFastVLine(cx, cy - 11, 23, TFT_RED);
  double lat, lon;
  mapPxToLatLon(mapCx, mapCy, mapZ, lat, lon);
  char buf[40];
  snprintf(buf, sizeof(buf), "%.4f, %.4f  z%d", lat, lon, mapZ);
  tft.setTextFont(1);
  tft.setTextDatum(BL_DATUM);
  int tw = tft.textWidth(buf) + 6;
  tft.fillRect(0, CONTENT_Y + CONTENT_H - 12, tw, 12, C_BG);
  tft.setTextColor(C_TEXT);
  tft.drawString(buf, 3, CONTENT_Y + CONTENT_H - 2);
}

static void drawMap() {
  btns.clear();
  tft.fillScreen(C_BG);
  drawHeader("Map", true);
  if (!mapReady) {
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_ACCENT);
    tft.drawString("No maps on the SD card", SCR_W / 2, CONTENT_Y + 50);
    tft.setTextColor(C_MUTED);
    tft.drawString("Run scripts/get-map-tiles.ps1", SCR_W / 2, CONTENT_Y + 80);
    tft.drawString("on a PC, then copy the", SCR_W / 2, CONTENT_Y + 100);
    tft.drawString("maps folder to the card.", SCR_W / 2, CONTENT_Y + 120);
  } else {
    mapDrawTiles();
  }
  FootDef f[] = {{B_BACK, "Back", I_LEFT, true}, {B_MAP_OUT, "-", I_NONE, mapReady && mapZ > mapMinZ},
                 {B_MAP_IN, "+", I_NONE, mapReady && mapZ < mapMaxZ}};
  drawFooter(f, 3);
}

// Drag to pan: the map jumps to the new spot when you lift your finger.
static void mapTouch(bool down, int tx, int ty) {
  static bool dragging = false;
  static int sx, sy, lx, ly;
  if (down) {
    if (!dragging) {
      if (ty < CONTENT_Y || ty >= CONTENT_Y + CONTENT_H) return;   // started on a button
      dragging = true; sx = tx; sy = ty;
    }
    lx = tx; ly = ty;
  } else if (dragging) {
    dragging = false;
    int dx = lx - sx, dy = ly - sy;
    if (abs(dx) + abs(dy) < 8 || !mapReady) return;
    double n = mapWorld(mapZ);
    mapCx = constrain((int32_t)(mapCx - dx), 0, (int32_t)n - 1);
    mapCy = constrain((int32_t)(mapCy - dy), 0, (int32_t)n - 1);
    mapDrawTiles();
  }
}

static void redraw() {
  switch (screen) {
    case SC_MAP: drawMap(); break;
    case SC_NEAR: drawNear(); break;
    case SC_MESH: meshMarkRead(); drawMesh(); break;
    case SC_QUICK: drawQuick(); break;
    case SC_KEYS: drawKeys(); break;
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
#if defined(BOARD_CYD_28)
#include "touch28.h"
static Touch28 touch28;

static void touchBegin() { touch28.begin(SCR_W, SCR_H); }
static bool touchRead(uint16_t *x, uint16_t *y) { return touch28.read(*x, *y); }
static bool touchHeld() { return touch28.pressed(); }

// Returns false if nothing was tapped within 15 s.
static bool waitTap(int px, int py, uint16_t &rx, uint16_t &ry) {
  while (touch28.pressed()) delay(10);                 // wait for release
  tft.fillScreen(TFT_BLACK);
  tft.drawCircle(px, py, 10, TFT_MAGENTA);
  tft.drawFastHLine(px - 14, py, 29, TFT_MAGENTA);
  tft.drawFastVLine(px, py - 14, 29, TFT_MAGENTA);
  uint32_t t0 = millis();
  while (!touch28.readRaw(rx, ry)) {
    if (millis() - t0 > 15000) return false;
    delay(10);
  }
  delay(150);
  touch28.readRaw(rx, ry);                             // settled reading
  while (touch28.pressed()) delay(10);
  return true;
}

static void calibrateTouch() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Touch calibration", SCR_W / 2, SCR_H / 2 - 20);
  tft.drawString("Tap each marker as it appears", SCR_W / 2, SCR_H / 2 + 6);
  delay(1500);
  uint16_t a, b, c, d, e, f;
  bool ok = false, tapped = true;
  for (int attempt = 0; attempt < 4 && tapped && !ok; attempt++) {
    tapped = waitTap(CAL_INSET, CAL_INSET, a, b) &&
             waitTap(SCR_W - CAL_INSET, CAL_INSET, c, d) &&
             waitTap(CAL_INSET, SCR_H - CAL_INSET, e, f);
    if (!tapped) break;
    touch28.setFromTaps(a, b, c, d, e, f);
    // Sanity check: both axes must span a real part of the panel. A collapsed range
    // (finger carried over, same spot tapped twice) would leave a sliver of touch area.
    ok = (touch28.xMax - touch28.xMin) > 1500 && (touch28.yMax - touch28.yMin) > 1500 &&
         abs((int)c - (int)a) + abs((int)d - (int)b) > 1200 &&
         abs((int)e - (int)a) + abs((int)f - (int)b) > 1200;
    if (!ok) {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED);
      tft.drawString("Taps didn't register - try again", SCR_W / 2, SCR_H / 2);
      delay(1500);
    }
  }
  if (tapped && ok) {
    // calibration good
  } else {
    // No taps: keep the built-in defaults (the ranges from the working ESPHome CYD) and don't save.
    touch28 = Touch28();
    touch28.begin(SCR_W, SCR_H);
    return;
  }
  uint16_t cal[8] = {touch28.swapXY, touch28.flipX, touch28.flipY, touch28.xMin, touch28.xMax, touch28.yMin, touch28.yMax, 1};
  prefs.putBytes("cal28b", cal, sizeof(cal));
}

static bool loadTouchCal() {
  uint16_t cal[8];
  if (prefs.getBytes("cal28b", cal, sizeof(cal)) != sizeof(cal) || cal[7] != 1) return false;
  touch28.swapXY = cal[0]; touch28.flipX = cal[1]; touch28.flipY = cal[2];
  touch28.xMin = cal[3]; touch28.xMax = cal[4]; touch28.yMin = cal[5]; touch28.yMax = cal[6];
  if (touch28.xMax <= touch28.xMin || touch28.yMax <= touch28.yMin ||
      touch28.xMax - touch28.xMin < 1500 || touch28.yMax - touch28.yMin < 1500) {
    touch28 = Touch28();
    touch28.begin(SCR_W, SCR_H);
    return false;                                      // bad saved range: calibrate again
  }
  return true;
}
#else
static void touchBegin() {}
static bool touchRead(uint16_t *x, uint16_t *y) { return tft.getTouch(x, y, 500); }
static bool touchHeld() { return tft.getTouchRawZ() > 500; }
static bool loadTouchCal() {
  uint16_t cal[5];
  if (prefs.getBytes("cal", cal, sizeof(cal)) != sizeof(cal)) return false;
  tft.setTouch(cal);
  return true;
}

static void calibrateTouch() {
  uint16_t cal[5];
  tft.fillScreen(TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("Touch calibration", SCR_W / 2, SCR_H / 2 - 30);
  tft.drawString("Tap each arrow corner as it appears", SCR_W / 2, SCR_H / 2);
  tft.drawString("(a fingernail or stylus works best)", SCR_W / 2, SCR_H / 2 + 24);
  delay(1200);
  tft.calibrateTouch(cal, TFT_MAGENTA, TFT_BLACK, 15);
  prefs.putBytes("cal", cal, sizeof(cal));
  tft.setTouch(cal);
}
#endif

static bool startSD() {
  if (sdUp) return true;
  sdSPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  for (int i = 0; i < 3; i++) {
    if (SD.begin(PIN_SD_CS, sdSPI, 20000000)) { sdUp = true; return true; }
    delay(200);
  }
  return false;
}

static void startup() {
  if (!startSD()) {
    Serial.println("[sd] card NOT found");
    startMesh();                                       // mesh works without a card (radio only, no hotspot)
    screen = SC_NOSD;
    redraw();
    return;
  }
  loadConfig();
  loadCatalog();
  Serial.printf("[sd] OK: %d categories, %d articles\n", nCats, nArticles);
  if (wifiOn) webStart(wcfg);
  startMesh();
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
    case SC_QUICK:
    case SC_NEAR:
    case SC_KEYS: screen = SC_MESH; break;
    default: list.clear(); screen = sdUp ? SC_HOME : SC_NOSD; break;
  }
  redraw();
}

// ---------------------------------------------------------------- flashlight + SOS
// Tap cycles white light -> red light (keeps night vision) -> SOS strobe -> exit.
static const uint8_t SOS_UNITS[] = {1,1, 1,1, 1,3,  3,1, 3,1, 3,3,  1,1, 1,1, 1,7};   // on,off,on,off... in units
static const int SOS_UNIT_MS = 220;
static int sosIdx = 0;
static uint32_t sosNextMs = 0;
static bool sosOn = false;

static void setRgbLed(bool on) {
  digitalWrite(PIN_LED_R, on ? LOW : HIGH);
  digitalWrite(PIN_LED_G, on ? LOW : HIGH);
  digitalWrite(PIN_LED_B, on ? LOW : HIGH);
}

static void drawLight() {
  static const char *HINTS[] = {"Tap: red light", "Tap: SOS strobe", "Tap: exit"};
  uint16_t bg = (lightMode == 1) ? TFT_RED : TFT_WHITE;
  if (lightMode == 2) bg = TFT_BLACK;                  // SOS starts dark and flashes
  tft.fillScreen(bg);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(lightMode == 1 ? TFT_BLACK : (lightMode == 2 ? TFT_DARKGREY : TFT_BLACK), bg);
  tft.drawString(HINTS[lightMode], SCR_W / 2, SCR_H - 16);
  sosIdx = 0;
  sosOn = false;
  sosNextMs = millis();
}

static void enterLight() {
  screen = SC_LIGHT;
  lightMode = 0;
  btns.clear();
  setBacklight(255);
  drawLight();
}

static void exitLight() {
  setRgbLed(false);
  setBacklight(brightness);
  screen = sdUp ? SC_HOME : SC_NOSD;
  redraw();
}

static void lightTap() {
  lightMode++;
  setRgbLed(false);
  if (lightMode > 2) { exitLight(); return; }
  drawLight();
}

static void lightLoop(uint32_t now) {
  if (lightMode != 2 || (int32_t)(now - sosNextMs) < 0) return;
  sosOn = (sosIdx % 2 == 0);                           // even steps are "on"
  tft.fillScreen(sosOn ? TFT_WHITE : TFT_BLACK);
  setRgbLed(sosOn);
  sosNextMs = now + (uint32_t)SOS_UNITS[sosIdx] * SOS_UNIT_MS;
  sosIdx = (sosIdx + 1) % (int)(sizeof(SOS_UNITS));
}

// ---------------------------------------------------------------- night dim (light sensor)
#if PIN_LDR >= 0
// Measured on the board: ~0 in bright light, ~60 in a lit room, ~270 with a finger over it (higher = darker).
#define LDR_DARK_ADC   200     // dim the screen above this
#define LDR_LIGHT_ADC  120     // back to normal below this
static int ldrRead() {
  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(PIN_LDR);
  return sum / 8;
}

static void nightLoop(uint32_t now) {
  static uint32_t next = 0;
  static uint32_t logMs = 0;
  if ((int32_t)(now - next) < 0) return;
  next = now + 500;
  int v = ldrRead();
  if (now - logMs > 3000) { logMs = now; Serial.printf("[ldr] %d\n", v); }
  if (!nightAuto || sleeping || screen == SC_LIGHT) return;
  bool wasNight = nightNow;
  if (!nightNow && v > LDR_DARK_ADC) nightNow = true;
  else if (nightNow && v < LDR_LIGHT_ADC) nightNow = false;
  if (nightNow != wasNight) setBacklight(nightNow ? min<int>(brightness, 25) : brightness);
}
#else
static void nightLoop(uint32_t) {}
#endif

// ---------------------------------------------------------------- settings backup on the SD card
#define F_BACKUP "/swiki-settings.txt"
#if defined(BOARD_CYD_28)
  #define CAL_KEY "cal28b"
  #define CAL_N 8
#else
  #define CAL_KEY "cal"
  #define CAL_N 5
#endif

static void saveBackup() {
  if (!sdUp) return;
  SD.remove(F_BACKUP);
  File f = SD.open(F_BACKUP, FILE_WRITE);
  if (!f) return;
  f.println("# Survival Wiki settings backup (restored automatically after a fresh flash)");
  f.printf("bright=%u\n", brightness);
  f.printf("tmo=%u\n", timeoutIdx);
  f.printf("inv=%d\n", invertColors ? 1 : 0);
  f.printf("wifi=%d\n", wifiOn ? 1 : 0);
  f.printf("night=%d\n", nightAuto ? 1 : 0);
  f.printf("mesh=%d\n", meshEnabled ? 1 : 0);
  f.printf("nick=%s\n", meshName());
  uint16_t cal[CAL_N];
  if (prefs.getBytes(CAL_KEY, cal, sizeof(cal)) == sizeof(cal)) {
    f.print("cal=");
    for (int i = 0; i < CAL_N; i++) f.printf(i ? ",%u" : "%u", cal[i]);
    f.println();
  }
  f.close();
}

static void restoreBackup() {
  if (!sdUp) return;
  File f = SD.open(F_BACKUP, FILE_READ);
  if (!f) return;
  while (f.available()) {
    String l = readLine(f);
    l.trim();
    int eq = l.indexOf('=');
    if (l.isEmpty() || l[0] == '#' || eq < 0) continue;
    String k = l.substring(0, eq), v = l.substring(eq + 1);
    if (k == "bright") prefs.putUChar("bright", constrain(v.toInt(), 20, 255));
    else if (k == "tmo") prefs.putUChar("tmo", v.toInt() % 5);
    else if (k == "inv") prefs.putBool("inv", v.toInt() != 0);
    else if (k == "wifi") prefs.putBool("wifi", v.toInt() != 0);
    else if (k == "night") prefs.putBool("night", v.toInt() != 0);
    else if (k == "mesh") prefs.putBool("mesh", v.toInt() != 0);
    else if (k == "nick" && v.length()) prefs.putString("nick", v.substring(0, MESH_NAME_LEN - 1));
    else if (k == "cal") {
      uint16_t cal[CAL_N];
      int n = 0, start = 0;
      while (n < CAL_N && start <= (int)v.length()) {
        int comma = v.indexOf(',', start);
        if (comma < 0) comma = v.length();
        cal[n++] = (uint16_t)v.substring(start, comma).toInt();
        start = comma + 1;
      }
      if (n == CAL_N) prefs.putBytes(CAL_KEY, cal, sizeof(cal));
    }
  }
  f.close();
}

// ---------------------------------------------------------------- easter egg
static void goodBoy() {
  tft.fillScreen(C_BG);
  int cx = SCR_W / 2, cy = SCR_H / 2 - 20;
  uint16_t pad = C_ACCENT;
  tft.fillEllipse(cx, cy + 22, 34, 26, pad);                 // big pad
  tft.fillEllipse(cx - 44, cy - 14, 11, 15, pad);            // toes
  tft.fillEllipse(cx - 16, cy - 36, 11, 15, pad);
  tft.fillEllipse(cx + 16, cy - 36, 11, 15, pad);
  tft.fillEllipse(cx + 44, cy - 14, 11, 15, pad);
  tft.setFreeFont(SCR_W < 300 ? &FreeSansBold12pt7b : &FreeSansBold18pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_TEXT);
  tft.drawString("Good boy!", cx, cy + 80);
  if (goodboyIdx < 0) {                                       // dog guides not on the SD card yet
    tft.setTextFont(2);
    tft.setTextColor(C_MUTED);
    tft.drawString("Copy the newest sdcard files", cx, cy + 112);
    tft.drawString("to unlock the dog first aid.", cx, cy + 132);
    delay(2500);
    redraw();
    return;
  }
  delay(1400);
  curCat = goodboyIdx;
  listPage = 0;
  loadCategoryList(curCat);
  screen = SC_CATEGORY;
  redraw();
}

static void startMesh() {
  if (meshEnabled) meshBegin(wcfg.channel, webRunning());
}

static void onButton(uint16_t id) {
  if (id >= B_KEY_BASE) {                              // on-screen keyboard character
    char c = (char)(id - B_KEY_BASE);
    if (kbShift && isalpha(c)) c = toupper(c);
    if ((int)kbBuf.length() < kbMax()) kbBuf += c;
    drawKbText();
    return;
  }
  if (id >= B_QUICK_BASE && id < B_QUICK_BASE + 10) {  // quick message
    static const char *QUICK[] = {"OK", "Need help", "Heading out", "At camp", "Water found", "Come to me"};
    if (id - B_QUICK_BASE < 6) meshSend(QUICK[id - B_QUICK_BASE]);
    screen = SC_MESH;
    redraw();
    return;
  }
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
    case B_PREV:
      if (screen == SC_HOME) { if (homePage > 0) { homePage--; redraw(); } }
      else if (bookPart > 1) { bookPart--; openBookPart(false); }
      break;
    case B_NEXT:
      if (screen == SC_HOME) { homePage++; redraw(); }   // drawHome clamps to the last page
      else if (bookPart < bookParts) { bookPart++; openBookPart(false); }
      break;
    case B_LIBRARY: listPage = 0; loadLibraryList(); screen = SC_LIBRARY; redraw(); break;
    case B_WIFI: screen = SC_WIFI; redraw(); break;
    case B_SETTINGS: screen = SC_SETTINGS; redraw(); break;
    case B_LIGHT: enterLight(); break;
    case B_MESH: screen = SC_MESH; redraw(); break;
    case B_MESH_TOGGLE:
      meshEnabled = !meshEnabled;
      prefs.putBool("mesh", meshEnabled);
      if (meshEnabled) startMesh(); else meshEnd();
      saveBackup();
      redraw();
      break;
    case B_MESH_QUICK: screen = SC_QUICK; redraw(); break;
    case B_MESH_NEAR: screen = SC_NEAR; redraw(); break;
    case B_MAP: if (!mapReady) mapLoadCenter(); screen = SC_MAP; redraw(); break;
    case B_MAP_IN:
      if (mapZ < mapMaxZ) { mapZ++; mapCx *= 2; mapCy *= 2; redraw(); }
      break;
    case B_MAP_OUT:
      if (mapZ > mapMinZ) { mapZ--; mapCx /= 2; mapCy /= 2; redraw(); }
      break;
    case B_MESH_TYPE: kbTarget = 0; kbBuf = ""; kbShift = false; screen = SC_KEYS; redraw(); break;
    case B_MESH_NAME: kbTarget = 1; kbBuf = meshName(); kbShift = false; screen = SC_KEYS; redraw(); break;
    case B_KB_DEL: if (kbBuf.length()) kbBuf.remove(kbBuf.length() - 1); drawKbText(); break;
    case B_KB_SHIFT: kbShift = !kbShift; drawKeys(); break;
    case B_KB_SEND:
      if (kbTarget == 0) {
        kbBuf.trim();
        if (kbBuf.length()) meshSend(kbBuf.c_str());
      } else {
        meshSetName(kbBuf.c_str());
        prefs.putString("nick", meshName());
        saveBackup();
      }
      screen = SC_MESH;
      redraw();
      break;
    case B_NIGHT:
      nightAuto = !nightAuto;
      prefs.putBool("night", nightAuto);
      if (!nightAuto && nightNow) { nightNow = false; setBacklight(brightness); }
      saveBackup();
      redraw();
      break;
    case B_BACKUP_SAVE: saveBackup(); showLoading("Saved to SD card"); delay(900); redraw(); break;
    case B_BACKUP_LOAD:
      if (!sdUp || !SD.exists(F_BACKUP)) { showLoading("No backup on the SD card"); delay(1200); redraw(); break; }
      restoreBackup();
      showLoading("Restored - restarting...");
      delay(900);
      ESP.restart();
      break;
    case B_WIFI_TOGGLE:
      wifiOn = !wifiOn;
      prefs.putBool("wifi", wifiOn);
      saveBackup();
      showLoading(wifiOn ? "Starting hotspot..." : "Stopping hotspot...");
      meshEnd();                                        // the radio is reconfigured; mesh restarts after
      if (wifiOn) webStart(wcfg); else webStop();
      startMesh();
      redraw();
      break;
    case B_BRIGHT_DN: brightness = max(20, brightness - 30); setBacklight(nightNow ? min<int>(brightness, 25) : brightness); prefs.putUChar("bright", brightness); saveBackup(); redraw(); break;
    case B_BRIGHT_UP: brightness = min(255, brightness + 30); setBacklight(nightNow ? min<int>(brightness, 25) : brightness); prefs.putUChar("bright", brightness); saveBackup(); redraw(); break;
    case B_TIMEOUT: timeoutIdx = (timeoutIdx + 1) % 5; prefs.putUChar("tmo", timeoutIdx); saveBackup(); redraw(); break;
    case B_INVERT: invertColors = !invertColors; tft.invertDisplay(invertColors); prefs.putBool("inv", invertColors); saveBackup(); redraw(); break;
    case B_CALIB: calibrateTouch(); saveBackup(); redraw(); break;
    case B_RETRY: startup(); break;
  }
}

static void handleTap(int x, int y) {
  if (screen == SC_LIGHT) { lightTap(); return; }
  // Easter egg: tap the title on the home screen five times.
  if (screen == SC_HOME && y < HEADER_H && x < SCR_W - 70) {
    static uint8_t eggTaps = 0;
    static uint32_t eggT0 = 0;
    uint32_t now = millis();
    if (now - eggT0 > 3000) eggTaps = 0;
    if (eggTaps == 0) eggT0 = now;
    if (++eggTaps >= 5) { eggTaps = 0; goodBoy(); return; }
  }
  for (size_t i = 0; i < btns.size(); i++) {
    Btn b = btns[i];
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) {
      if (!b.enabled) return;
      if (b.id >= B_KEY_BASE) {
        // keyboard keys: no highlight, the text box updates instead
      } else if (b.id >= B_TILE) {      // tiles and list rows: quick highlight frame
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

  gpsBegin();                          // no-op unless the board profile wires a GPS (PIN_GPS_RX)

  // Audio amp (if present on 3.5" board)
#if PIN_AUDIO_EN >= 0
  pinMode(PIN_AUDIO_EN, OUTPUT);
  digitalWrite(PIN_AUDIO_EN, HIGH);   // speaker amp off
#endif

  // RGB LED pins (active low)
  pinMode(PIN_LED_R, OUTPUT); digitalWrite(PIN_LED_R, HIGH);
  pinMode(PIN_LED_G, OUTPUT); digitalWrite(PIN_LED_G, HIGH);
  pinMode(PIN_LED_B, OUTPUT); digitalWrite(PIN_LED_B, HIGH);

  // Deselect SPI devices early to avoid bus conflicts during init
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
#if defined(TOUCH_CS) && (TOUCH_CS >= 0)
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
#endif

#if PIN_LDR >= 0
  pinMode(PIN_LDR, INPUT);
  analogSetPinAttenuation(PIN_LDR, ADC_11db);
#endif

  prefs.begin("swiki", false);
  // Fresh flash (no saved settings): bring them back from the SD card backup if there is one.
  if (!prefs.isKey("bright")) {
    if (startSD() && SD.exists(F_BACKUP)) { restoreBackup(); Serial.println("[backup] settings restored from SD"); }
    if (!prefs.isKey("bright")) prefs.putUChar("bright", 200);
  }
  brightness = prefs.getUChar("bright", 200);
  timeoutIdx = prefs.getUChar("tmo", 2) % 5;
  invertColors = prefs.getBool("inv", false);
  wifiOn = prefs.getBool("wifi", true);
  nightAuto = prefs.getBool("night", false);
  meshEnabled = prefs.getBool("mesh", true);
  if (prefs.isKey("nick")) meshSetName(prefs.getString("nick").c_str());

  tft.init();
  tft.setRotation(0);
  tft.invertDisplay(invertColors);
  tft.fillScreen(C_BG);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, 0);
  setBacklight(brightness);

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(nullptr);

  // Splash screen scaled to display height
  int splashCenter = SCR_H / 2 - 40;
  if (SCR_W < 300) tft.setFreeFont(&FreeSansBold12pt7b); else tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_ACCENT);
  tft.drawString("SURVIVAL", SCR_W / 2, splashCenter);
  tft.setTextColor(C_TEXT);
  tft.drawString("WIKI", SCR_W / 2, splashCenter + (SCR_W < 300 ? 30 : 42));
  tft.setTextFont(2);
  tft.setTextColor(C_MUTED);
  tft.drawString("offline field reference", SCR_W / 2, splashCenter + (SCR_W < 300 ? 56 : 80));
  tft.drawString("hold screen to calibrate touch", SCR_W / 2, SCR_H - (SCR_W < 300 ? 24 : 40));

  touchBegin();
  bool haveCal = loadTouchCal();
  delay(1500);
  bool justCalibrated = false;
  if (!haveCal || touchHeld()) { calibrateTouch(); justCalibrated = true; }

  startup();
  if (justCalibrated) saveBackup();
  lastTouchMs = millis();
}

#ifdef SHOT_SERIAL
// Developer tool (build with -DSHOT_SERIAL=1): drive the UI and dump the screen over USB serial
// so docs screenshots come from the real device. Not part of the release binaries.
static void shotRow(int y) {
  static uint8_t row[SCR_W * 3];
  static char line[SCR_W * 6 + 16];
  tft.readRectRGB(0, y, SCR_W, 1, row);
  int n = snprintf(line, sizeof(line), "R%03d:", y);
  for (int x = 0; x < SCR_W * 3; x++) n += snprintf(line + n, sizeof(line) - n, "%02X", row[x]);
  line[n++] = '\n';
  for (int i = 0; i < n; i += 32) {            // small paced chunks: the CH340 drops bytes if flooded
    Serial.write((uint8_t *)line + i, min(32, n - i));
    Serial.flush();
    delay(3);
  }
}

// 'p' dumps every row as an ASCII line; 'r' + 3 digits re-sends a single row (the USB link drops
// bytes at this size, so the host re-requests damaged rows).
static void shotDump() {
  Serial.printf("SHOT %d %d\n", SCR_W, SCR_H);
  for (int y = 0; y < SCR_H; y++) shotRow(y);
  Serial.println("SHOTEND");
}

static void shotCommand(char c) {
  switch (c) {
    case 'p': shotDump(); break;
    case 'r': {                                   // followed by three digits: re-send that row
      char d[4] = {0};
      for (int i = 0; i < 3; i++) { uint32_t t0 = millis(); while (!Serial.available() && millis() - t0 < 200) {} d[i] = Serial.available() ? Serial.read() : '0'; }
      shotRow(atoi(d));
      break;
    }
    case 'H': goBack(); screen = SC_HOME; redraw(); break;
    case 'S': screen = SC_SETTINGS; redraw(); break;
    case 'W': screen = SC_WIFI; redraw(); break;
    case 'L': enterLight(); break;
    case 'F': meshDemo(); break;
    case 'Z': { bool ok = meshSend("Hello from the bench board"); Serial.printf("[mesh] sent test message: %s\n", ok ? "ok" : "FAILED (mesh off or rate limit)"); break; }
    case 'M': screen = SC_MESH; redraw(); break;
    case 'N': screen = SC_NEAR; redraw(); break;
    case 'P': if (!mapReady) mapLoadCenter(); screen = SC_MAP; redraw(); break;
    case 'I': onButton(B_MAP_IN); break;
    case 'O': onButton(B_MAP_OUT); break;
    case 'Q': screen = SC_QUICK; redraw(); break;
    case 'K': kbTarget = 0; kbBuf = "Water found at the creek"; screen = SC_KEYS; redraw(); break;
    case 'T': lightTap(); break;
    case 'G': goodBoy(); break;
    case 'B': listPage = 0; loadLibraryList(); screen = SC_LIBRARY; redraw(); break;
    case 'C': curCat = 1; listPage = 0; loadCategoryList(curCat); screen = SC_CATEGORY; redraw(); break;
    case 'A': if (!list.empty()) openArticle(list[0]); break;
    case 'D': if (!list.empty()) { scrollY = 0; scrollDoc(1); } break;
  }
}
#endif

void loop() {
#ifdef SHOT_SERIAL
  while (Serial.available()) shotCommand(Serial.read());
#endif
  static bool wasDown = false;
  uint16_t tx, ty;
  bool down = touchRead(&tx, &ty);
  uint32_t now = millis();
  if (screen == SC_MAP) mapTouch(down, tx, ty);

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

  if (screen == SC_LIGHT) { lastTouchMs = now; lightLoop(now); }   // flashlight keeps the screen awake
  nightLoop(now);
  gpsLoop();

  meshLoop();
  {                                                    // a message sent from a phone changes the list too
    static uint32_t lastSeq = 0;
    uint32_t sq = meshChangeCount();                   // also changes when a "got it" reply arrives
    if (sq != lastSeq) { lastSeq = sq; if (screen == SC_MESH && !sleeping) redraw(); }
  }
  if (meshTakeNew()) {                                 // a message arrived from another board
    if (sleeping) { sleeping = false; setBacklight(nightNow ? min<int>(brightness, 25) : brightness); }
    lastTouchMs = now;
    if (screen != SC_LIGHT) {
      digitalWrite(PIN_LED_G, LOW);                    // brief green blink
      ledOffMs = now + 300;
    }
    if (screen == SC_MESH || screen == SC_HOME) redraw();
  }
  if (ledOffMs && (int32_t)(now - ledOffMs) >= 0) { digitalWrite(PIN_LED_G, HIGH); ledOffMs = 0; }
  if (screen == SC_NEAR && !sleeping) {                // keep the signal bars live
    static uint32_t nearMs = 0;
    if (now - nearMs > 2000) { nearMs = now; drawNear(); }
  }

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
