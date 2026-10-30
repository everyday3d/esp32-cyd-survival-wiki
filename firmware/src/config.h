// Survival Wiki - board configuration
// Supports:
//  - Hosyond / LCDWiki 3.5" ESP32-32E (E32R35T), ST7796U + XPT2046 resistive touch (320x480)
//  - Sunton 2.8" ESP32-2432S028R CYD, ILI9341 + XPT2046 resistive touch (240x320)
#pragma once

#define FW_VERSION "1.2.0"

// ---- Hardware Profile ----
#if defined(BOARD_CYD_28)
  #define BOARD_NAME     "ESP32-2432S028 (2.8\" CYD)"
  #define SCR_W          240
  #define SCR_H          320
  #define PIN_BACKLIGHT   21   // LCD backlight PWM on 2.8" CYD
  #define PIN_SD_CS        5   // microSD on VSPI bus
  #define PIN_SD_SCK      18
  #define PIN_SD_MISO     19
  #define PIN_SD_MOSI     23
  #define PIN_LED_R        4   // RGB LED, active low
  #define PIN_LED_G       16
  #define PIN_LED_B       17
  #define PIN_AUDIO_EN    -1   // No external amp enable (GPIO 26 DAC audio out)
  #define PIN_LDR         34   // light sensor (input only)
  #define HEADER_H        36
  #define FOOTER_H        38
  #define ROW_H           38
  #define TILE_GAP         6
#else
  #define BOARD_NAME     "Hosyond 3.5\" E32R35T"
  #define SCR_W          320
  #define SCR_H          480
  #define PIN_BACKLIGHT   27   // LCD backlight PWM on 3.5" E32R35T
  #define PIN_SD_CS        5   // microSD on VSPI bus
  #define PIN_SD_SCK      18
  #define PIN_SD_MISO     19
  #define PIN_SD_MOSI     23
  #define PIN_LED_R       22   // RGB LED, active low
  #define PIN_LED_G       16
  #define PIN_LED_B       17
  #define PIN_AUDIO_EN     4   // speaker amp enable, LOW = on (kept off)
  #define PIN_LDR         -1   // no light sensor wired on this board
  #define HEADER_H        44
  #define FOOTER_H        52
  #define ROW_H           48
  #define TILE_GAP         8
#endif

#define CONTENT_Y  HEADER_H
#define CONTENT_H  (SCR_H - HEADER_H - FOOTER_H)
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
