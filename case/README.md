# ESP32 Touch CYD Snap-Fit Enclosures (3.5" & 2.8")

![Working Hardware in Hand](images/preview_real_hardware_in_hand.jpg)

Custom 2-piece screwless snap-fit enclosures tailored for ESP32 Cheap Yellow Display (CYD) modules:
- **3.5" Module (`rev2`)**: Engineered for **Hosyond 3.5" 320x480 ESP32-32E** (QDtech / LCDWiki `E32R35T` / `E32N35T` — Amazon ASIN [B0D93MBWC2](https://www.amazon.com/dp/B0D93MBWC2)).
- **2.8" Module (`rev1`)**: Engineered for **Sunton 2.8" 240x320 ESP32-2432S028R** (classic CYD with universal Micro-USB & USB-C clearance).

---

## Directory Structure

```
case/
├── 3mf/          Ready-to-print 1-plate project files for Bambu Studio & OrcaSlicer
├── stl/          Individual STL meshes for 3.5" (rev2) and 2.8" (rev1) models
├── cad/          Native Blender 5.2 source projects (.blend)
└── images/       Assembly diagrams, exploded views, and print orientation renders
```

---

## Download Files

### 1. 3.5" Display Case (`rev2` — Hosyond E32R35T)
- **Bambu Studio / OrcaSlicer Project**: [`3mf/ESP32_35_Touch_Case_rev2.3mf`](3mf/ESP32_35_Touch_Case_rev2.3mf)
- **Base Shell STL**: [`stl/1_ESP32_35_Case_Base_rev2.stl`](stl/1_ESP32_35_Case_Base_rev2.stl)
- **Top Bezel STL**: [`stl/2_ESP32_35_Case_Top_rev2.stl`](stl/2_ESP32_35_Case_Top_rev2.stl)
- **Assembly STL**: [`stl/ESP32_35_Case_Assembly_rev2.stl`](stl/ESP32_35_Case_Assembly_rev2.stl)
- **Blender 5.2 Source**: [`cad/ESP32_35_Touch_Case_rev2.blend`](cad/ESP32_35_Touch_Case_rev2.blend)

### 2. 2.8" Display Case (`rev1` — Sunton ESP32-2432S028R CYD)
- **Base Shell STL**: [`stl/1_ESP32_28_Case_Base_rev1.stl`](stl/1_ESP32_28_Case_Base_rev1.stl)
- **Top Bezel STL**: [`stl/2_ESP32_28_Case_Top_rev1.stl`](stl/2_ESP32_28_Case_Top_rev1.stl)
- **Assembly STL**: [`stl/ESP32_28_Case_Assembly_rev1.stl`](stl/ESP32_28_Case_Assembly_rev1.stl)
- **Blender 5.2 Source**: [`cad/ESP32_28_Touch_Case_rev1.blend`](cad/ESP32_28_Touch_Case_rev1.blend)

---

## Hardware Compatibility

| Feature | 3.5" Variant (`E32R35T`) | 2.8" Variant (`ESP32-2432S028R`) |
| :--- | :--- | :--- |
| **Board Footprint** | 101.50 × 55.50 mm (R 3.50 mm) | 86.00 × 50.00 mm (R 2.50 mm) |
| **Cavity Dimensions** | 102.60 × 56.60 mm | 87.00 × 51.00 mm |
| **Mounting / Retention** | 4× M3 tapered alignment pins | 4 corner support ledges + perimeter alignment shoulders |
| **USB Port** | Centered Type-C (`Y = 0.00 mm`) | Universal Micro-USB & USB-C cutout with overmold relief |
| **Buttons** | Side and bottom `BOOT` & `RESET` | Rear stylus access pinholes for `BOOT` & `RESET` |
| **MicroSD Slot** | Bottom edge with thumb scoop | Right short edge with thumb scoop |

*(Note: Older Sunton `ESP32-3248S035` boards with off-center Micro-USB ports and M2 holes will not fit this enclosure).*

---

## Recommended Print Settings

- **Supports**: **None (`0`)** — 100% support-free geometry.
- **Orientation**: Both parts print flat on `Z = 0`:
  - **Base Shell**: Floor-down with cooling louvers facing the bed.
  - **Top Bezel**: **Face-down on `Z = 0`** so the front bezel inherits your build plate's textured PEI finish and the 45° screen chamfer prints cleanly without supports.
- **Layer Height**: 0.20 mm Standard (0.4 mm nozzle).
- **Wall Loops**: 3 walls.
- **Infill**: 15% Gyroid.
- **Material**: PLA, PLA Matte, or PETG.
- **Print Time**: ~55 minutes for both parts on a Bambu Lab X1/P1/A1.

---

## Assembly Instructions

1. **Drop in the Board**:
   Lower your Hosyond 3.5" ESP32 module into the base shell with the USB-C port facing the left short wall and the MicroSD card slot facing the bottom thumb scoop. The 4 corner holes on the board will seat directly over the 4 tapered alignment pins.

2. **Snap on the Top Bezel**:
   Align the top bezel over the display and press down firmly around the perimeter until the locking detents click into place.

3. **Disassembly (If Needed)**:
   Use a coin or fingernail in either of the two pry slots along the bottom seam to pop the bezel off without tools.

---

## Visual Gallery

### Studio Render
![Studio Assembly](images/preview_hero_assembled_rev2.png)

### Exploded Stackup View
![Exploded View](images/preview_exploded_snap_fit_rev2.png)

### Print Bed Layout (Flat on Z=0, Zero Supports)
![Print Bed Layout](images/preview_print_orientation_rev2.png)

### Bottom Passive Cooling Louvers
![Bottom Ventilation](images/preview_bottom_boxerdawg3d_rev2.png)
