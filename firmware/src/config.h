// Survival Wiki - board configuration
// Board: Hosyond / LCDWiki 3.5" ESP32-32E (E32R35T), ST7796U + XPT2046 resistive touch
#pragma once

#define FW_VERSION "1.0.0"

// ---- Pins (display + touch pins are set in platformio.ini for TFT_eSPI) ----
#define PIN_BACKLIGHT 27   // LCD backlight, HIGH = on (PWM dimmed)
#define PIN_SD_CS      5   // microSD on its own SPI bus (VSPI)
#define PIN_SD_SCK    18
#define PIN_SD_MISO   19
#define PIN_SD_MOSI   23
#define PIN_LED_R     22   // RGB LED, common anode: LOW = on
#define PIN_LED_G     16
#define PIN_LED_B     17
#define PIN_AUDIO_EN   4   // speaker amp enable, LOW = on (kept off)

// ---- Screen layout (portrait 320 x 480) ----
#define SCR_W      320
#define SCR_H      480
#define HEADER_H    44
#define FOOTER_H    52
#define CONTENT_Y  HEADER_H
#define CONTENT_H  (SCR_H - HEADER_H - FOOTER_H)   // 384
#define MARGIN       8

// ---- Colors (RGB565) ----
#define C_BG        0x0861   // near-black blue
#define C_PANEL     0x18E3   // dark grey
#define C_HEADER    0x2124
#define C_TEXT      0xFFFF
#define C_MUTED     0xAD55
#define C_ACCENT    0xFD20   // safety orange
#define C_WARN_BG   0x4A00   // dark amber
#define C_WARN_BAR  0xFD20
#define C_LINE      0x3186
#define C_PRESS     0x4A69

// ---- Files on the SD card ----
#define F_CONFIG     "/config.txt"
#define F_CATEGORIES "/wiki/categories.tsv"
#define F_CATALOG    "/wiki/catalog.tsv"
#define D_ARTICLES   "/wiki/articles/"
#define F_LIBRARY    "/library/catalog.tsv"
#define D_LIBRARY    "/library/"
#define F_WEB_INDEX  "/web/index.html"

// ---- Limits (no PSRAM on this board: keep RAM use modest) ----
#define MAX_CATEGORIES   12
#define MAX_ARTICLES    400
#define MAX_BOOKS        40
#define MAX_DOC_BYTES  24576   // largest article / book part loaded at once
