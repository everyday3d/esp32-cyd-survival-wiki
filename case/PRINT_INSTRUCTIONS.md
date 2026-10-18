---
title: Print & Assembly Instructions — 3.5" ESP32 Touch CYD Case (rev2 — E32R35T)
tags:
  - 3d-printing
  - esp32
  - cyd
  - instructions
---

# Print & Assembly Guide — 3.5" ESP32 Touch CYD Case (`rev2`)

## 1. Recommended Slicer Settings
- **Project File**: Open `ESP32_35_Touch_Case_rev2.3mf` in Bambu Studio / OrcaSlicer (or import `1_ESP32_35_Case_Base_rev2.stl` and `2_ESP32_35_Case_Top_rev2.stl`).
- **Orientation**: Both parts are pre-oriented **flat on `Z = 0`** (`1_ESP32_35_Case_Base_rev2` on its bottom exterior; `2_ESP32_35_Case_Top_rev2` face-down on its top bezel).
- **Supports**: **None (`0`)** — 100% support-free geometry.
- **Layer Height**: `0.20 mm` Standard (`0.4 mm` nozzle).
- **Wall Loops**: `3` walls.
- **Infill**: `15%` Gyroid.
- **Material**: PLA, PLA Matte, PETG, or ABS/ASA.

## 2. Snap-Fit Assembly (`E32R35T` / `E32N35T` — Amazon `B0D93MBWC2`)
1. **Drop in the Board**: Lower your `101.50 × 55.50 mm` Hosyond 3.5" ESP32-32E (`E32R35T`) board into `1_ESP32_35_Case_Base_rev2` with the **USB-C port facing the Left short wall (`Y = 0.00 mm`)** and the **MicroSD card slot facing the Bottom thumb scoop (`X = +7.50 mm`)**. The 4 corner `Ø 3.20 mm` mounting holes seat directly onto the 4 tapered M3 alignment pins (`94.50 × 47.90 mm` pitch).
2. **Snap on the Top Bezel**: Align `2_ESP32_35_Case_Top_rev2` over the display and press down along the top, bottom, and right edges until the snap-fit detent ridges click into place.
3. **Easy Removal**: Use either of the two fingernail/coin pry slots on the bottom seam to pop the bezel off whenever needed.
