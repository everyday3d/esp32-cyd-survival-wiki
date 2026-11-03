# 2.8" CYD + GPS case

## rev2 (use this one): taller pod with its own lid

The first version did not close with a 4-pin plug standing on the GPS module: the plug and wires stood about 3 to 4 mm taller than the pod. rev2 fixes that and adds somewhere to keep the spare cable.

- **Taller pod, 5 mm more room.** The pod walls rise 5 mm above the main lid, and a **separate pod lid** (third part) covers them. The main lid has a notch where the pod stands.
- **Cable pegs.** Two pegs in the module bay between the wall opening and the module, to wind the spare cable around.
- **Antenna on a pillar** so it still sits just under the pod lid.
- **Module bay is 17 mm wide and 32 mm long**, with the module at the far end.

Parts and files:

| File | What it is |
| :--- | :--- |
| `ESP32_28_GPS_Case_rev2.3mf` | All three parts on one plate (base floor-down, main lid and pod lid face-down), about 250 x 95 mm |
| `1_ESP32_28_GPS_Case_Base_rev2.stl` | Base with the tall pod |
| `2_ESP32_28_GPS_Case_Top_rev2.stl` | Main lid |
| `3_ESP32_28_GPS_Case_PodLid_rev2.stl` | Pod lid, press fits into the pod (a dab of glue or a bit of tape keeps it shut) |
| `ESP32_28_GPS_PodLid_rev2_snug.3mf` | **Snug pod lid, only.** The first pod lid sat loose. This one has tighter fit and small crush ribs on its rim that press into the bay walls, plus a small tab at the far end to get a fingernail or coin under it. Print just this part if you already have the base. |
| `ESP32_28_GPS_Case_rev2.blend` | Blender 5.2 model |

**Not printed yet.** The 5 mm figure is from your measurement of how much the first print was short. If the lid still will not close, tell me how much and I will change one number and rebuild.

---
# Older version (rev1)

Snap-fit two-piece case for the Sunton `ESP32-2432S028R` with a pod on one long edge for a Teyleten ATGM336H GPS module ([Amazon affiliate link](https://link.amazon/B066Kz9AX)) (about 16 x 13 mm) and its ceramic antenna (19 x 6 x 6.5 mm, 100 mm cable with a 3 x 3 mm plug).

- Walls are 2.4 mm taller than the plain rev1 case so the lid reaches the snap.
- microSD card goes in through an opening in the same wall as the pod.
- The antenna sits just under a thin roof in the lid; the module sits in its own bay.
- Antenna cable drops into an open notch in the wall between the bays; a passage through the main wall lines up with the CN1 connector.
- **Room for the 4-pin connector (updated):** the module bay is 34 mm long. The module sits at the far end, and the black 4-pin Dupont housing (about 14 mm long, 10 mm wide) that plugs onto its header lies in the space between the module and the wall opening. The first version had no room for it, so the lid would not close. The case is now 94 x 94.5 mm and the print plate needs about 198 x 95 mm.
- Lid text: SurvivalWiki. Base text: BOXERDAWG3D.

Print: `ESP32_28_GPS_Case_rev1.3mf` has both parts on one plate (base floor-down, lid face-down), no supports. Fit of the main body confirmed by print. The longer pod for the connector is newer than that print and has not been printed yet.

Wiring: see [WIRING.md](WIRING.md).
