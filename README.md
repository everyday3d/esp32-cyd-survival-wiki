# ESP32 CYD Survival Wiki

[![ESP Web Flasher](https://img.shields.io/badge/Web%20Flasher-1--Click%20Install-orange?style=for-the-badge&logo=espressif)](https://everyday3d.github.io/esp32-cyd-survival-wiki/)
[![3D Printable Case](https://img.shields.io/badge/3D%20Case-Snap--Fit%20(No%20Supports)-blue?style=for-the-badge&logo=bambulab)](case/README.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)

An offline survival knowledge base and field reference engineered specifically for the **Hosyond 3.5" 320x480 ESP32-32E Touchscreen Display Module** ([Amazon ASIN B0D93MBWC2](https://www.amazon.com/dp/B0D93MBWC2) — model **QDtech / LCDWiki `E32R35T` / `E32N35T`**, 3.5" Cheap Yellow Display / CYD with centered USB-C).

> [!important] Exact Target Hardware Compatibility
> Both the firmware pinout/bus configuration and the 3D printable snap-fit case are tailored to this exact board:
> - **Product Link:** [Amazon B0D93MBWC2 — Hosyond 3.5" ESP32 Display Module](https://www.amazon.com/dp/B0D93MBWC2)
> - **Manufacturer / Board Model:** QDtech / LCDWiki **`E32R35T`** (Resistive Touch) / **`E32N35T`** (Non-Touch)
> - **Dimensions:** `101.50 mm × 55.50 mm` PCB (`R 3.50 mm` corners)
> - **Mounting Holes:** `4× Ø 3.20 mm` (M3) at `94.50 mm × 47.90 mm` pitch (`±47.25 mm, ±23.95 mm`)
> - **Connector:** **Centered Type-C USB Port (`Y = 0.00 mm` on Left short edge)** flanked by `RESET` and `BOOT` tactile switches (`Y = ±13.72 mm`)
> - **Display:** 3.5" $320 \times 480$ TFT LCD (ST7796U controller) with XPT2046 resistive touch panel
> - **Storage:** Onboard MicroSD (TF) card slot on dedicated VSPI bus (`PIN_SD_CS 5`, `SCK 18`, `MISO 19`, `MOSI 23`)
> - *(Note: Older Sunton `ESP32-3248S035` boards with off-center Micro-USB and M2 holes will NOT fit this case)*

- **On the 3.5" Screen:** Browse 8 categories and 61 illustrated emergency articles, plus the complete 233-page **US Army Survival Manual (FM 21-76)** pre-indexed across 62 paged chapters and the field guide book.
- **On Phones & Laptops:** The device hosts its own standalone Wi-Fi captive portal network (`SurvivalWiki`). Phones that join get the entire search-indexed wiki with color diagrams in their browser, plus direct PDF download access to the complete US Army FM 21-76 manual.
- **100% Offline & Grid-Down:** No internet required, ever. Everything lives on the microSD card and runs off any 5V USB-C battery bank.
- **Custom Snap-Fit Case Included:** Includes 3D printable, support-free 2-piece enclosure designed specifically for the 101.50 × 55.50 mm E32R35T board with centered USB-C port and M3 alignment pins.

```
web-flasher/     1-click browser web flasher (ESP Web Tools / Web Serial) + precompiled binaries
firmware/        source code (PlatformIO) + bin/ ready-to-flash binaries
sdcard/          copy the CONTENTS of this folder to the root of a FAT32 microSD card
case/            3D printable 2-piece snap-fit case (.3mf, .stl, .blend, renders & instructions)
screenshots/     UI preview screenshots
```

---

## ⚡ Quick Start in 3 Steps

### 1. Flash the Firmware (1-Click in Browser)
> [!tip] Web Flasher — No Software or Toolchain Required
> 1. Open Chrome or Edge and go to the **[ESP32 CYD Survival Wiki Web Flasher](https://everyday3d.github.io/esp32-cyd-survival-wiki/)** (or open `web-flasher/index.html` locally).
> 2. Plug the board into your computer using a USB-C data cable.
> 3. Click **Connect / Install**, select your serial port (`USB-SERIAL CH340` or `CP2102`), check **Erase Device** on fresh install, and let Web Serial flash the full image in ~40 seconds!
>    - *If connection fails:* Hold the **BOOT** button on the side of the board, click Connect, then release BOOT.

*Alternative manual method:* Use [esptool-js](https://espressif.github.io/esptool-js/) with `firmware/bin/survival-wiki-full.bin` at address `0x0`.

---

### 2. Prepare the MicroSD Card
1. Format a microSD card as **FAT32** (cards up to 32GB work out-of-the-box; for 64GB+, format with FAT32 via Rufus/GUIFormat).
2. Copy **everything inside** the `sdcard/` folder directly to the **root** of your microSD card:
   ```
   / (microSD root)
   ├── config.txt
   ├── wiki/
   │   ├── categories.tsv
   │   ├── catalog.tsv
   │   ├── search.json.gz
   │   ├── articles/
   │   └── img/
   ├── library/
   │   ├── catalog.tsv
   │   ├── field-guide/       # All articles compiled into single book
   │   └── fm21-76/           # US Army Survival Manual (FM 21-76, 62 parts + PDF)
   └── web/
       ├── index.html
       └── index.html.gz
   ```
3. Insert the card into the board's TF card slot.

---

### 3. Power On & Calibrate Touch
1. Power the board from any USB-C phone charger or power bank.
2. On first boot, the board prompts for **touchscreen calibration**: tap the 4 corner arrows as they appear (using a fingernail, pen tip, or stylus for best precision).
3. The home screen loads! Tap **Wi-Fi** to display the QR code for phones to scan.

---

## 📱 Connecting Mobile Phones (Captive Portal)
- Join the Wi-Fi network **`SurvivalWiki`** (open network, no password required) or scan the on-screen QR code.
- The portal will pop up automatically. If not, open your browser and navigate to **`http://192.168.4.1`**.
- If your phone prompts "Wi-Fi has no internet access", select **Stay Connected** or **Use as-is**.
- Up to 8 devices can browse simultaneously. Includes dark mode and instant offline fuzzy search.
- To configure network SSID or set a Wi-Fi password: edit `config.txt` on the microSD card.

---

## 🖨️ 3D Printable Snap-Fit Case (`case/`)
The `case/` directory includes the official **rev2** precision snap-fit enclosure designed in Blender 5.2:
- **100% Support-Free Printing:** Both parts print flat on `Z = 0` (Base on its back, Top Bezel face-down).
- **Exact E32R35T Clearances:** Accommodates the `101.50 × 55.50 mm` PCB with an open cavity that leaves all rear perimeter components unobstructed.
- **Centered USB-C Port:** Wide cutout with external overmold relief so thick USB-C cable plugs seat flush.
- **Port Access:** Features a bottom thumb scoop for microSD insertion/removal, dedicated access ports for `RESET` and `BOOT` tactile switches, top peripheral windows for JST 1.25mm connectors, and angled passive cooling louvers.
- See [`case/PRINT_INSTRUCTIONS.md`](case/PRINT_INSTRUCTIONS.md) and [`case/ESP32_35_Touch_Case_rev2.3mf`](case/ESP32_35_Touch_Case_rev2.3mf).

---

## 🔋 Power Consumption
- Typical power draw is ~1.0–1.5 W with screen on and Wi-Fi active.
- A standard 10,000 mAh battery bank provides over 24 hours of continuous operation.
- **Power saving:** Dim screen brightness or configure sleep timeout in Settings. Tap the screen to wake instantly.

---

## 📝 Customizing & Adding Articles
Articles are standard Markdown documents located in `sdcard/wiki/articles/` with YAML metadata frontmatter:

```markdown
---
title: Purifying Water
category: water
order: 2
tags: boil, bleach, filter
summary: One-line summary displayed in article lists.
---
## Boiling
Bring water to a rolling boil for at least 1 full minute (3 minutes at high altitude).

![Water Filter Diagram](img/water_filter.jpg)
```

Run the content builder script after modifying articles:
```bash
python3 tools/build_content.py sdcard
```

---

## 🛠️ Building Firmware from Source (PlatformIO)
Requirements: VS Code with PlatformIO extension (or `pio` CLI).
```bash
cd firmware
pio run -t upload
```

---

## 📜 Board Pinout Reference (`Hosyond E32R35T`)
| Function | ESP32 Pin | Details |
|---|---|---|
| **LCD SPI** | SCK 14, MOSI 13, MISO 12 | ST7796U controller, 40MHz SPI |
| **LCD Control** | CS 15, DC 2, Backlight 27 | PWM backlight control |
| **Touch SPI** | CS 33, IRQ 36 | XPT2046 resistive touch (HSPI) |
| **MicroSD SPI** | CS 5, SCK 18, MISO 19, MOSI 23 | VSPI bus |
| **RGB LED** | Red 22, Green 16, Blue 17 | Active LOW |
| **Audio** | Amp Enable 4, DAC 26 | Internal speaker driver |

---

## ⚖️ Safety & License
- **Field Reference Notice:** This software is an informational reference guide, not a substitute for professional medical advice, certified wilderness survival training, or emergency services.
- **License:** Open source under the [MIT License](LICENSE).
- **Credits:**
  - Case design by [BoxerDawg 3D](https://boxerdawg3d.com).
  - TFT_eSPI & TJpg_Decoder by Bodmer.
  - QR Code generator by Project Nayuki.
