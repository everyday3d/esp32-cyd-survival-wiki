# ESP32 CYD Survival Wiki

[![ESP Web Flasher](https://img.shields.io/badge/Web%20Flasher-1--Click%20Install-orange?style=for-the-badge&logo=espressif)](https://everyday3d.github.io/esp32-cyd-survival-wiki/)
[![3D Printable Case](https://img.shields.io/badge/3D%20Case-Snap--Fit%20(No%20Supports)-blue?style=for-the-badge&logo=bambulab)](case/README.md)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)

An offline survival knowledge base, mobile captive portal, and field reference engineered for both the classic **2.8" Cheap Yellow Display (`ESP32-2432S028R`)** and the larger **3.5" Hosyond Touch Display (`E32R35T`)**. 

> [!important] Supported Hardware & Dual-Target Architecture
> I built this firmware and companion snap-fit cases to support both popular ESP32 display form factors out of the box:
> 
> | Feature / Spec | 3.5" Hosyond (`E32R35T`) | 2.8" Classic CYD (`ESP32-2432S028R`) |
> |---|---|---|
> | **Board Model** | QDtech / LCDWiki `E32R35T` (Resistive) / `E32N35T` ([Amazon ASIN B0D93MBWC2](https://www.amazon.com/dp/B0D93MBWC2)) | Sunton `ESP32-2432S028R` (Micro-USB or Dual USB-C `CYD2USB`) |
> | **Screen & Resolution** | 3.5" $320 \times 480$ (ST7796 SPI controller) | 2.8" $240 \times 320$ (ILI9341 SPI controller) |
> | **Touch Panel** | XPT2046 Resistive Touch (`CS 33`, `IRQ 36` on HSPI) | XPT2046 Resistive Touch (`CS 33`, `IRQ 36` on HSPI) |
> | **USB Ports & Power** | Centered Type-C USB (`Y = 0.00 mm` on left edge) | Micro-USB (left edge) or Micro-USB + Type-C bottom |
> | **PCB Dimensions** | $101.50 \times 55.50\text{ mm}$ ($R3.5\text{ mm}$ corners) | $86.00 \times 50.00\text{ mm}$ ($R2.0\text{ mm}$ corners) |
> | **Mounting / Alignment** | 4× Ø3.20 mm (M3) holes at $94.50 \times 47.90\text{ mm}$ | 4 Corner Support Bosses & Side Perimeter Snaps |
> | **Precompiled Binary** | [`survival-wiki-full.bin`](firmware/bin/survival-wiki-full.bin) (`0x0`) | [`survival-wiki-28-full.bin`](firmware/bin/survival-wiki-28-full.bin) (`0x0`) |
> | **Web Flasher Target** | `3.5" Display (Hosyond E32R35T)` | `2.8" Display (Classic CYD)` |
> | **3D Printable Case** | [`case/3.5inch/`](case/3.5inch/README.md) (`ESP32_35_Touch_Case_rev2.3mf`) | [`case/2.8inch/`](case/2.8inch/README.md) (`ESP32_28_Touch_Case_rev1.3mf`) |
>
> *(Note: The older Sunton `ESP32-3248S035` board with off-center Micro-USB has different pinouts and will not fit the 3.5" case).*

- **On the Screen:** Browse 8 emergency categories and 61 illustrated emergency articles, plus the complete 233-page **US Army Survival Manual (FM 21-76)** pre-indexed across 62 paged chapters and the field guide book (rendered with native resolution layouts on both 320×480 and 240×320 screens).
- **On Phones & Laptops:** The device hosts its own standalone Wi-Fi captive portal network (`SurvivalWiki`). Phones that join get the entire search-indexed wiki with color diagrams in their browser, plus direct PDF download access to the complete US Army FM 21-76 manual.
- **100% Offline & Grid-Down:** No internet required, ever. Everything lives on the microSD card and runs off any 5V USB-C battery bank.
- **Custom Snap-Fit Cases Included:** Support-free 2-piece enclosures designed specifically for each board geometry with dedicated port cutouts, cooling vents, and toolless snap tabs.

```
web-flasher/     1-click browser web flasher (ESP Web Tools / Web Serial) + precompiled binaries
firmware/        source code (PlatformIO) + bin/ ready-to-flash binaries
sdcard/          copy the CONTENTS of this folder to the root of a FAT32 microSD card
case/            3D printable 2-piece snap-fit case (.3mf, .stl, .blend, renders & instructions)
screenshots/     UI preview screenshots
```

---

## 🌟 Interface & Feature Spotlight

Explore the visual modules, emergency manuals, touch UI, and wireless capabilities built into the ESP32 CYD Survival Wiki:

### 1. Touchscreen Emergency Dashboard (Responsive 3.5" & 2.8" UI)
[![Touch Dashboard Spotlight](docs/screenshots/01_touch_dashboard_spotlight.png)](docs/screenshots/01_touch_dashboard_spotlight.png)
- **8 Instant Emergency Tiles**: 1-touch navigation for First Steps, Water, Fire, Shelter, First Aid, Food, Navigation, and Signals & Rescue.
- **Hardware Status Bar**: Displays live captive Wi-Fi state, active client counter, battery voltage, and MicroSD card status.
- **Sub-80ms Catalog Loading**: Instant category switching reads directly from indexed TSV tables on FAT32 MicroSD cards.
- **Dedicated Bottom Bar**: Touch-friendly navigation bar provides one-tap switching between Library reader, Wi-Fi hotspot toggle, and system settings.

### 2. Illustrated Field Guides & Color Diagrams
[![Field Guide Reader Spotlight](docs/screenshots/02_field_guide_reader_spotlight.png)](docs/screenshots/02_field_guide_reader_spotlight.png)
- **61 Illustrated Field Articles**: Visual schematics for gravity water filtration, solar stills, wilderness traps, splinting, and shelter construction.
- **High-Contrast Dark Theme**: Specially tuned contrast preserves readability in bright direct sunlight while preventing night blindness in dark conditions.
- **Paged Navigation Engine**: Smooth touchscreen paging with percentage progress bar, fast-jump buttons, and instant return to category index.
- **Immediate Trauma Access**: Rapid-access medical summaries provide step-by-step guidance for severe bleeding, hypothermia, CPR, and shock.

### 3. US Army Survival Manual (FM 21-76) Complete Library
[![US Army Survival Manual Spotlight](docs/screenshots/03_us_army_survival_manual_spotlight.png)](docs/screenshots/03_us_army_survival_manual_spotlight.png)
- **Complete Military Survival Doctrine**: Full 233-page field manual pre-indexed across 62 paged chapters and subchapters on MicroSD.
- **Psychology & Survival Craft**: In-depth coverage of wilderness psychology, survival kit assembly, water procurement, and emergency firecraft.
- **Dangerous Plants & Wildlife**: Comprehensive plant identification guide, universal edibility testing protocol, and snake venom treatment.
- **100% Grid-Down Autonomy**: Entirely offline with zero reliance on cellular towers, satellite connections, or external APIs.

### 4. Wireless Smartphone Captive Portal & Search
[![Wireless Smartphone Hub Spotlight](docs/screenshots/04_wifi_portal_search_spotlight.png)](docs/screenshots/04_wifi_portal_search_spotlight.png)
- **Zero-Install Wi-Fi Hotspot**: Scan the device's on-screen QR code with any smartphone or tablet to instantly join the `SurvivalWiki` captive portal network.
- **Client-Side Instant Search**: Type keywords into the local browser interface to search across all 61 guides and FM 21-76 subchapters in real time.
- **Night-Vision Palette**: Red-light-friendly dark mode minimizes glare and light emissions during power outages or nighttime field operations.
- **Simultaneous Multi-User Sharing**: Up to 8 connected mobile devices can read different survival manuals and emergency protocols simultaneously.

### 5. 1-Click Browser Web Flasher via Web Serial
[![Web Serial Flasher Spotlight](docs/screenshots/05_web_serial_flasher_spotlight.png)](docs/screenshots/05_web_serial_flasher_spotlight.png)
- **Zero-Toolchain Turnkey Installation**: Flash the complete firmware image directly from Google Chrome or Microsoft Edge via the Web Serial API.
- **Auto CH340 / CP2102 Detection**: Automatically connects to USB serial adapters without manual driver installation or terminal commands.
- **Pre-Compiled Full Image**: Flashes bootloader, partition table, and the Survival Wiki OS in ~40 seconds.
- **Single-Click Maintenance**: Effortlessly update or erase firmware directly from the web interface.

### 6. Support-Free 3D Printed Snap-Fit Case
[![Snap-Fit Field Case Spotlight](docs/screenshots/06_snap_fit_case_spotlight.png)](docs/screenshots/06_snap_fit_case_spotlight.png)
- **100% Support-Free Printing**: Both halves print flat on `Z = 0`; face-down top bezel takes the textured finish of your PEI build plate.
- **Centered USB-C Recess**: Accommodates standard USB-C cables, 90-degree adapters, and allows portable battery banks to sit flush against the case.
- **MicroSD Card Scoop & Hardware Bosses**: MicroSD card thumb scoop allows instant card swaps; 4 tapered M3 corner alignment pins secure the board without screws.
- **Passive Thermal Ventilation**: 9 angled cooling louvers vent heat from the ESP32-WROOM module during continuous Wi-Fi hotspot operation.

---

## ⚡ Quick Start in 3 Steps

### 1. Flash the Firmware (1-Click in Browser)
> [!tip] 1-Click Web Flasher — Zero Tools or Drivers Needed
> 1. Open Chrome or Edge and head over to the **[ESP32 CYD Survival Wiki Web Flasher](https://everyday3d.github.io/esp32-cyd-survival-wiki/)** (or open `web-flasher/index.html` locally).
> 2. Plug your display board into your computer with a good USB data cable.
> 3. Select your hardware target at the top of the flasher:
>    - **3.5" Display (Hosyond E32R35T)**: $320 \times 480$ ST7796 with centered USB-C.
>    - **2.8" Display (Classic CYD)**: $240 \times 320$ ILI9341 (`ESP32-2432S028R`).
> 4. Click **Connect / Install**, pick your serial COM port (`USB-SERIAL CH340` or `CP2102`), check **Erase Device** on fresh install, and let Web Serial write the full unified image in ~40 seconds!
>    - *If connection fails:* Hold down the **BOOT** button on the side of the board, click Connect, then release BOOT once connected.

*Alternative manual method:* Flash ready-to-flash unified binaries at offset `0x0` via [esptool-js](https://espressif.github.io/esptool-js/) or CLI `esptool.py`:
- **3.5" Hosyond Board:** [`firmware/bin/survival-wiki-full.bin`](firmware/bin/survival-wiki-full.bin)
- **2.8" Classic CYD Board:** [`firmware/bin/survival-wiki-28-full.bin`](firmware/bin/survival-wiki-28-full.bin)

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

## 🖨️ 3D Printable Snap-Fit Cases (`case/`)
The `case/` directory includes official precision snap-fit enclosures designed in Blender 5.2 for both supported screen sizes:
- **Dedicated 2.8" CYD Enclosure (`case/2.8inch/`):** Tailored for Sunton `ESP32-2432S028R` with universal Micro-USB & Type-C clearance, 4 raised corner support bosses, rear stylus pinholes for BOOT/RESET, and debossed BoxerDawg3D base. Download [`case/2.8inch/ESP32_28_Touch_Case_rev1.3mf`](case/2.8inch/ESP32_28_Touch_Case_rev1.3mf).
- **Dedicated 3.5" CYD Enclosure (`case/3.5inch/`):** Tailored for Hosyond `E32R35T` / `E32N35T` with centered USB-C, 4 tapered M3 corner alignment pins, peripheral headers clearance, and passive cooling louvers. Download [`case/3.5inch/ESP32_35_Touch_Case_rev2.3mf`](case/3.5inch/ESP32_35_Touch_Case_rev2.3mf).
- **100% Support-Free Printing:** Both models print flat on `Z = 0` (Base on its back, Top Bezel face-down on textured PEI). Zero screws, brass inserts, or glue required.
- See the [`case/README.md`](case/README.md) hub and [`case/PRINT_INSTRUCTIONS.md`](case/PRINT_INSTRUCTIONS.md) for full hardware comparison and slicer guidelines.

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

Each article file added to `sdcard/wiki/articles/` is referenced in `sdcard/wiki/catalog.tsv` (Title, Category, Summary, Filename) and displays in its respective category index immediately upon reboot.

---

## 🛠️ Building Firmware from Source (PlatformIO)
Requirements: VS Code with PlatformIO extension (or `pio` CLI).
```bash
cd firmware

# Build and flash for 3.5" Hosyond / QDtech E32R35T:
pio run -e survival-wiki-35 -t upload

# Build and flash for 2.8" Sunton ESP32-2432S028R CYD:
pio run -e survival-wiki-28 -t upload
```

---

## 📜 Board Pinout Reference

### 1. 3.5" Display Module (`Hosyond E32R35T`)
| Function | ESP32 Pin | Details |
|---|---|---|
| **LCD SPI** | SCK 14, MOSI 13, MISO 12 | ST7796U controller ($320 \times 480$), 40MHz SPI |
| **LCD Control** | CS 15, DC 2, Backlight 27 | PWM backlight control |
| **Touch SPI** | CS 33, IRQ 36 | XPT2046 resistive touch (HSPI) |
| **MicroSD SPI** | CS 5, SCK 18, MISO 19, MOSI 23 | VSPI bus |
| **RGB LED** | Red 22, Green 16, Blue 17 | Active LOW |
| **Audio** | Amp Enable 4, DAC 26 | Internal speaker driver |

### 2. 2.8" Display Module (`Sunton ESP32-2432S028R CYD`)
| Function | ESP32 Pin | Details |
|---|---|---|
| **LCD SPI** | SCK 14, MOSI 13, MISO 12 | ILI9341 controller ($240 \times 320$), 40MHz SPI |
| **LCD Control** | CS 15, DC 2, Backlight 21 | PWM backlight control |
| **Touch SPI** | CS 33, IRQ 36 | XPT2046 resistive touch (HSPI) |
| **MicroSD SPI** | CS 5, SCK 18, MISO 19, MOSI 23 | VSPI bus |
| **RGB LED** | Red 4, Green 16, Blue 17 | Active LOW |
| **Audio** | DAC 26 | Speaker output |

---

## ⚖️ Safety & License
- **Field Reference Notice:** This software is an informational reference guide, not a substitute for professional medical advice, certified wilderness survival training, or emergency services.
- **License:** Open source under the [MIT License](LICENSE).
- **Credits:**
  - Case design by [BoxerDawg 3D](https://boxerdawg3d.com).
  - TFT_eSPI & TJpg_Decoder by Bodmer.
  - QR Code generator by Project Nayuki.
