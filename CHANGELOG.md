# Changelog

All notable changes to the **ESP32 CYD Survival Wiki** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- **2.8" CYD Hardware Profile & PlatformIO Target**: Added dedicated `survival-wiki-28` build environment in `platformio.ini` for Sunton 2.8" ESP32-2432S028R Cheap Yellow Display modules with ILI9341 driver and XPT2046 touch configuration (Fixes #4).
- **Responsive 240x320 UI Engine**: Scaled touch UI across all screens (Header, Footer, 2x4 Emergency Home Grid with horizontal icon/label split, Category article lists with 6 rows/page, Settings, and Phone Access Wi-Fi QR view) to fit compact $240 \times 320$ displays natively (Fixes #4).
- **Dynamic Multi-Target Web Flasher**: Added model selector in `docs/index.html` and `web-flasher/index.html` allowing makers to choose between 3.5" (`E32R35T`) and 2.8" (`ESP32-2432S028R`) targets before flashing in 1 click over Web Serial (Fixes #4).
- **SPI Bus Decoupling**: Explicitly deselects `PIN_SD_CS` (GPIO 5) and `TOUCH_CS` (GPIO 33) high during early setup to prevent bus contention across shared HSPI/VSPI operations (Fixes #4).

---

## [1.0.0] - 2026-10-18

### Added
- **Target Board Calibration**: Explicit support and documentation for the Hosyond 3.5" ESP32-32E Touchscreen Display ([Amazon ASIN B0D93MBWC2](https://www.amazon.com/dp/B0D93MBWC2)), QDtech / LCDWiki `E32R35T` (Resistive Touch) / `E32N35T` (CYD).
- **Consolidated Firmware**: Pre-built single-binary firmware (`survival-wiki-full.bin`) mapped at offset `0x0` with bootloader, partition table, and application flash image.
- **Hardware Test Utility**: Factory board test firmware (`board-test-full.bin`) for diagnosing display, backlight, and touch hardware.
- **1-Click Web Flasher**: Zero-install browser flashing utility using ESP Web Tools (Web Serial) live on GitHub Pages (`https://everyday3d.github.io/esp32-cyd-survival-wiki/`).
- **US Army Survival Manual (FM 21-76)**: Complete 233-page public-domain manual extracted into 62 paged chapters (`0001.txt` .. `0062.txt`) in `sdcard/library/fm21-76/`, plus uncompressed full PDF download over Wi-Fi.
- **Pre-Indexed MicroSD Content**: 61 full-text emergency guides across 8 categories, compressed search database (`search.json.gz`), and 10-part compiled `field-guide/`.
- **Standalone Wi-Fi Captive Portal**: Built-in AP (`SurvivalWiki`) serving an offline mobile web app with instant keyword search and diagram viewing.
- **3D Printable Snap-Fit Case (`rev2`)**: Modern 2-piece enclosure engineered in Blender 5.2 and Bambu Studio `.3mf` with centered USB-C cutout, M3 alignment pins, microSD scoop, cooling louvers, and debossed `BOXERDAWG3D` branding. 100% support-free flat printing.
