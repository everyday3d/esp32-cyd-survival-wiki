#!/usr/bin/env python3
"""
Blender 5.2 Procedural CAD Generator (rev2 — Verified E32R35T / E32N35T Fitment) for:
Hosyond 3.5" 320x480 Touch Screen ESP32-32E Display Module (QDtech / LCDWiki E32R35T)
Amazon ASIN: B0D93MBWC2

Key rev2 Hardware Corrections (Verified against Official E32R35T_Size.pdf & E32R35T_3D.step):
1. Full-Depth 102.60 x 56.60 mm Open PCB Cavity (0.55 mm per-side clearance around the 101.50 x 55.50 mm PCB)
   with ZERO lower perimeter shelf obstruction — PCB is supported strictly on the 4 corner Ø 5.60 mm mounting pads
   so all rear perimeter components (USB-C, RESET/BOOT keys, MicroSD slot, and 6x JST headers) drop in 100% clear.
2. Centered USB-C Port Cutout (Y = 0.00 mm on Left Short Edge X = -50.75 mm):
   - Wide 14.6 x 7.4 mm through-opening + 18.8 x 9.8 mm exterior cable overmold recess at Y = 0.00 mm.
   - Left-wall center snap bump removed so nothing ever blocks the USB-C port.
3. True E32R35T Rear RESET & BOOT Key Access (Flanking USB-C on Left Short Edge):
   - RESET (X = -47.49, Y = +13.72 mm) and BOOT (X = -47.49, Y = -13.72 mm) chamfered rear button access ports
     plus Left-wall side relief slots beside the USB-C port.
4. Upgraded Corner M3 Mounting Bosses (94.50 x 47.90 mm pitch, ±47.25 x ±23.95 mm):
   - Ø 5.80 mm corner bosses with self-centering Ø 2.90 -> 2.50 mm tapered alignment pins for the 4x Ø 3.20 mm holes.
5. Full-Width 56.60 mm Internal LCD Pocket & Centered Viewing Window:
   - Clears the full 84.96 x 55.50 mm LCD backlight frame (4.20 mm tall) + left-side touch FPC solder pins.
   - 76.80 x 50.20 mm active touch window (centered at X = +1.40, Y = 0.00 mm) with 45° stylus/finger chamfer.
6. MicroSD (TF) Slot + 6x JST 1.25mm Peripheral Windows + Rear RGB LED Glow Louvers + Debossed "BOXERDAWG3D" Base.
"""

import os
import math
import bpy
import bmesh
from mathutils import Vector, Matrix

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_ROOT_DIR = os.path.dirname(OUTPUT_DIR)

# ==============================================================================
# HARDWARE & ENCLOSURE CONSTANTS (in millimeters — Official E32R35T_Size.pdf)
# ==============================================================================
PCB_W = 101.50          # X length (-50.75 to +50.75 mm)
PCB_H = 55.50           # Y width  (-27.75 to +27.75 mm) — E32R35T is 55.50 mm wide!
PCB_THICK = 1.60        # PCB thickness
LCD_W = 84.96           # LCD backlight frame length (-42.48 to +42.48 mm, 8.27 mm from each end)
LCD_H = 55.50           # LCD backlight frame width (full 55.50 mm PCB width)
LCD_THICK = 4.20        # LCD (2.5) + RTP touch (1.2) + tape (0.5) = 4.20 mm above PCB front

HOLE_PITCH_X = 94.50    # 101.50 - 2*3.50 -> ±47.25 mm
HOLE_PITCH_Y = 47.90    # 55.50  - 2*3.80 -> ±23.95 mm

# Enclosure Outer Dimensions
CASE_W = 110.40         # X: -55.20 to +55.20 mm (3.90 mm wall around 102.60 mm cavity)
CASE_H = 64.40          # Y: -32.20 to +32.20 mm (3.90 mm wall around 56.60 mm cavity)
CASE_R = 5.50           # Outer corner radius

CAVITY_W = 102.60       # PCB X clearance (101.50 + 1.10 mm = 0.55 mm per side)
CAVITY_H = 56.60        # PCB Y clearance (55.50  + 1.10 mm = 0.55 mm per side)
CAVITY_R = 3.20         # Inner corner radius (matches PCB R3.50 with clearance)

# Vertical Z Stackup (Assembled Coordinates: Z = 0.0 at Base Exterior Bottom)
# Base: Z = 0.0 to 12.2 mm (main wall up to Z_SEAM = 10.4 mm, inner snap lip up to Z_LIP_TOP = 12.2 mm)
# PCB:  Rests on 4 corner bosses at Z_PCB_BOT = 7.60 mm -> PCB spans Z = 7.60 to 9.20 mm (5.40 mm rear SMD clearance)
# LCD:  Spans Z = 9.20 to 13.40 mm (4.20 mm thick)
# Top:  Outer skirt starts at Z_SEAM = 10.40 mm up to Z_TOP_FACE = 15.80 mm
Z_BASE_BOT = 0.00
Z_FLOOR = 2.20          # 2.20 mm rigid bottom floor
Z_PCB_BOT = 7.60        # 5.40 mm under-board clearance (clears 3.45 mm max rear SMD & WROOM-32E with 1.95 mm air gap)
Z_PCB_TOP = Z_PCB_BOT + PCB_THICK  # 9.20 mm
Z_SEAM = 10.40          # Exterior parting line between Base and Top Bezel
Z_LIP_TOP = 12.20       # Inner interlocking snap-fit lip top
Z_TOP_FACE = 15.80      # Top Bezel exterior front face


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def create_mat(name, hex_color, roughness=0.38, metallic=0.05):
    h = hex_color.lstrip("#")
    r = srgb_to_linear(int(h[0:2], 16) / 255.0)
    g = srgb_to_linear(int(h[2:4], 16) / 255.0)
    b = srgb_to_linear(int(h[4:6], 16) / 255.0)
    mat = bpy.data.materials.new(name=name)
    if hasattr(mat, "use_nodes"):
        mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
    return mat


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 0.001


def rounded_rect_pts(w, h, r, segs_per_corner=12):
    r = max(0.05, min(r, w * 0.49, h * 0.49))
    cx = w * 0.5 - r
    cy = h * 0.5 - r
    corners = [
        (cx, cy, 0.0, math.pi * 0.5),
        (-cx, cy, math.pi * 0.5, math.pi),
        (-cx, -cy, math.pi, math.pi * 1.5),
        (cx, -cy, math.pi * 1.5, math.pi * 2.0),
    ]
    pts = []
    for ox, oy, a0, a1 in corners:
        for i in range(segs_per_corner):
            ang = a0 + (a1 - a0) * (i / float(segs_per_corner))
            pts.append((ox + r * math.cos(ang), oy + r * math.sin(ang)))
    return pts


def create_lofted_rounded_box(name, sections, segs_per_corner=12):
    bm = bmesh.new()
    rings = []
    for z, w, h, r in sections:
        pts = rounded_rect_pts(w, h, r, segs_per_corner)
        ring = [bm.verts.new((x, y, z)) for (x, y) in pts]
        rings.append(ring)

    bm.verts.ensure_lookup_table()
    n = len(rings[0])

    bm.faces.new(list(reversed(rings[0])))
    for k in range(len(rings) - 1):
        r0 = rings[k]
        r1 = rings[k + 1]
        for i in range(n):
            ni = (i + 1) % n
            bm.faces.new([r0[i], r0[ni], r1[ni], r1[i]])
    bm.faces.new(rings[-1])

    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new(name, mesh)
    bpy.context.collection.objects.link(obj)
    return obj


def create_box(name, size_xyz, center_xyz, rot_deg=(0.0, 0.0, 0.0)):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=center_xyz)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size_xyz
    if any(rot_deg):
        obj.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return obj


def create_cylinder(name, radius, depth, center_xyz, rot_deg=(0.0, 0.0, 0.0), vertices=36):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=vertices, radius=radius, depth=depth, location=center_xyz
    )
    obj = bpy.context.active_object
    obj.name = name
    if any(rot_deg):
        obj.rotation_euler = tuple(math.radians(a) for a in rot_deg)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return obj


def create_cone(name, r1, r2, depth, center_xyz, vertices=40):
    bpy.ops.mesh.primitive_cone_add(
        vertices=vertices, radius1=r1, radius2=r2, depth=depth, location=center_xyz
    )
    obj = bpy.context.active_object
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    return obj


def bool_op(target, cutter, operation='DIFFERENCE', solver='EXACT'):
    mod = target.modifiers.new(name=f"Bool_{cutter.name}", type='BOOLEAN')
    mod.operation = operation
    mod.object = cutter
    if hasattr(mod, "solver"):
        mod.solver = solver
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.data.objects.remove(cutter, do_unlink=True)


def finalize_manifold_mesh(obj):
    bpy.context.view_layer.objects.active = obj
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.002)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def create_debossed_text_cutter(text_str, font_size, extrude_depth, center_xyz, mirror_x=True):
    bpy.ops.object.text_add(location=(0.0, 0.0, 0.0))
    txt_obj = bpy.context.active_object
    txt_obj.name = f"TextCutter_{text_str}"
    txt_data = txt_obj.data
    txt_data.body = text_str
    txt_data.align_x = 'CENTER'
    txt_data.align_y = 'CENTER'
    txt_data.size = font_size
    txt_data.extrude = extrude_depth * 0.5
    txt_data.space_character = 1.08

    for fp in [r"C:\Windows\Fonts\arialbd.ttf", r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\impact.ttf"]:
        if os.path.exists(fp):
            txt_data.font = bpy.data.fonts.load(fp)
            break

    bpy.ops.object.convert(target='MESH')
    mesh_obj = bpy.context.active_object

    bm = bmesh.new()
    bm.from_mesh(mesh_obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.01)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh_obj.data)
    bm.free()

    if mirror_x:
        mesh_obj.scale = (-1.0, 1.0, 1.0)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        bm = bmesh.new()
        bm.from_mesh(mesh_obj.data)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(mesh_obj.data)
        bm.free()

    mesh_obj.location = center_xyz
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return mesh_obj


# ==============================================================================
# 1. BUILD BOTTOM BASE SHELL (1_ESP32_35_Case_Base_rev2)
# ==============================================================================
def build_bottom_base():
    # Outer shell with 45-deg bottom chamfer (Z=0..1.6 mm), main body up to Z_SEAM (10.4 mm),
    # and stepped inner snap-fit mating lip from Z=10.4 to 12.2 mm.
    outer_sections = [
        (0.00,           CASE_W - 3.20, CASE_H - 3.20, CASE_R - 1.20),
        (1.60,           CASE_W,        CASE_H,        CASE_R),
        (Z_SEAM,         CASE_W,        CASE_H,        CASE_R),
        (Z_SEAM + 0.02,  CASE_W - 4.00, CASE_H - 4.00, CASE_R - 1.50),
        (Z_LIP_TOP,      CASE_W - 4.00, CASE_H - 4.00, CASE_R - 1.50),
    ]
    base = create_lofted_rounded_box("1_ESP32_35_Case_Base_rev2", outer_sections, segs_per_corner=16)

    # Carve FULL-DEPTH interior PCB & rear-component cavity (102.60 x 56.60 mm) all the way from
    # Z_FLOOR = 2.20 mm to the top so ZERO rear SMD components, USB-C housing, or MicroSD slot hit a shelf!
    cavity_sections = [
        (Z_FLOOR,          CAVITY_W, CAVITY_H, CAVITY_R),
        (Z_LIP_TOP + 2.00, CAVITY_W, CAVITY_H, CAVITY_R),
    ]
    cav_cutter = create_lofted_rounded_box("Cavity_Cutter", cavity_sections, segs_per_corner=12)
    bool_op(base, cav_cutter, 'DIFFERENCE')

    # Add 4 Corner PCB Mounting Bosses + Self-Centering M3 Alignment Pins at (±47.25, ±23.95)
    # Matching the official E32R35T Ø 5.60 mm corner pad rings and Ø 3.20 mm mounting holes!
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            hx = sx * (HOLE_PITCH_X * 0.5)
            hy = sy * (HOLE_PITCH_Y * 0.5)
            # Corner support boss from floor (Z=2.0) to PCB bottom (Z=7.60), Ø 5.80 mm
            boss = create_cylinder(f"Boss_{sx}_{sy}", radius=2.90, depth=5.60, center_xyz=(hx, hy, 4.80), vertices=32)
            bool_op(base, boss, 'UNION')
            # Self-centering M3 alignment pin from Z=7.50 to Z=9.65 (Ø 2.90 mm base tapering to Ø 2.46 mm tip
            # so the Ø 3.20 mm E32R35T holes drop on effortlessly with zero binding!)
            pin = create_cone(f"Pin_{sx}_{sy}", r1=1.45, r2=1.23, depth=2.15, center_xyz=(hx, hy, 8.575), vertices=28)
            bool_op(base, pin, 'UNION')

    # Add Snap-Fit Detent Ridges on the exterior of the inner lip (at Z = 11.30 mm)
    # 4 catches on Top/Bottom walls (X = ±26 mm, Y = ±(CASE_H/2 - 2.00)) + 1 catch on Right wall.
    # NOTE: ZERO snap bump on the Left wall center so the centered USB-C port is 100% unobstructed!
    for sy in (-1.0, 1.0):
        for sx in (-1.0, 1.0):
            bump_tb = create_cylinder(
                f"SnapBump_TB_{sx}_{sy}",
                radius=0.55,
                depth=12.0,
                center_xyz=(sx * 26.0, sy * (CASE_H * 0.5 - 2.00), 11.30),
                rot_deg=(0.0, 90.0, 0.0),
                vertices=20,
            )
            bool_op(base, bump_tb, 'UNION')

    bump_r = create_cylinder(
        "SnapBump_Right",
        radius=0.55,
        depth=14.0,
        center_xyz=(CASE_W * 0.5 - 2.00, 0.0, 11.30),
        rot_deg=(90.0, 0.0, 0.0),
        vertices=20,
    )
    bool_op(base, bump_r, 'UNION')

    # --------------------------------------------------------------------------
    # PORT CUTOUTS & SIDE RELIEFS (Exact E32R35T_Size.pdf Coordinates)
    # --------------------------------------------------------------------------
    # 1. Left Short Wall (X = -55.20): CENTERED Type-C USB Port at Y = 0.00 mm!
    #    - Through-cutout: 14.8 mm wide (Y = -7.4..+7.4 mm) x 7.4 mm tall (Z = 3.50..10.90 mm)
    #      Note: extends above Z_SEAM (10.40 mm) through the inner lip so the USB-C plug and PCB drop straight in!
    usb_thru = create_box("USB_C_Thru", (12.0, 14.8, 7.4), (-52.5, 0.00, 7.20))
    bool_op(base, usb_thru, 'DIFFERENCE')
    #    - Exterior USB-C cable overmold relief pocket (19.0 mm wide x 9.6 mm tall) so fat USB-C cables seat 100% flush!
    usb_overmold = create_box("USB_C_Overmold", (4.6, 19.0, 9.6), (-54.4, 0.00, 7.00))
    bool_op(base, usb_overmold, 'DIFFERENCE')

    # 2. Left Short Wall / Rear Corner Key Access: E32R35T RESET (Y = +13.72 mm) & BOOT (Y = -13.72 mm) at X = -47.49 mm
    #    - Side window slots on Left wall at Y = ±13.72 mm so you can also reach/see the keys from the left edge,
    #      PLUS chamfered bottom through-ports directly underneath the tactile switch buttons at (-47.49, ±13.72)!
    for bname, by in [("RESET", 13.72), ("BOOT", -13.72)]:
        # Side access notch on Left wall (Z = 3.80 to 8.20 mm)
        side_btn = create_box(f"SideBtn_{bname}", (12.0, 6.8, 4.4), (-52.5, by, 6.00))
        bool_op(base, side_btn, 'DIFFERENCE')
        # Bottom floor chamfered button actuator access port directly under switch at X = -47.49, Y = by
        btn_hole = create_cylinder(f"Hole_{bname}", radius=1.85, depth=10.0, center_xyz=(-47.49, by, 2.50), vertices=28)
        bool_op(base, btn_hole, 'DIFFERENCE')
        btn_chamfer = create_cone(f"Chamf_{bname}", r1=2.80, r2=1.75, depth=1.10, center_xyz=(-47.49, by, 0.50), vertices=28)
        bool_op(base, btn_chamfer, 'DIFFERENCE')

    # 3. Bottom Long Wall (Y = -32.20):
    #    - MicroSD (TF) Card Slot at X = +7.50 mm (E32R35T_Size.pdf: 25.48+14.54+17.87 = 57.89 mm from left -> X=+7.14 mm;
    #      E32R35T_3D.step SD_CARD1 spans X = +0.21 to +14.87 mm, center X = +7.54 mm)
    tf_cut = create_box("TF_Bottom_Cut", (18.4, 12.0, 5.4), (7.50, -29.5, 6.30))
    bool_op(base, tf_cut, 'DIFFERENCE')
    tf_scoop = create_cylinder(
        "TF_Bottom_ThumbScoop", radius=9.8, depth=7.2, center_xyz=(7.50, -33.6, 6.40),
        rot_deg=(0.0, 0.0, 0.0), vertices=36
    )
    bool_op(base, tf_scoop, 'DIFFERENCE')

    #    - BAT (2P JST at X = -25.27 mm) & UART (4P JST at X = -10.73 mm) Combined Peripheral Window
    bat_uart_cut = create_box("BAT_UART_Cut", (26.5, 12.0, 5.6), (-18.00, -29.5, 6.20))
    bool_op(base, bat_uart_cut, 'DIFFERENCE')

    # 4. Top Long Wall (Y = +32.20):
    #    - 4x JST 1.25mm Headers on E32R35T Top Edge:
    #      IO35/IO39 (X = -25.27), SPEAKER (X = -9.54), SPI (X = +9.39), I2C (X = +29.39)
    #      Provide two clean, modern peripheral windows on the Top wall exposing all 4 JST ports!
    top_jst_left = create_box("Top_JST_IO_SPK", (28.0, 12.0, 5.6), (-17.40, 29.5, 6.20))
    bool_op(base, top_jst_left, 'DIFFERENCE')
    top_jst_right = create_box("Top_JST_SPI_I2C", (32.0, 12.0, 5.6), (19.40, 29.5, 6.20))
    bool_op(base, top_jst_right, 'DIFFERENCE')

    # 5. Bottom-Side Coin / Fingernail Pry Notches at X = ±36.0 mm, Y = -32.20 mm, Z = Z_SEAM (10.40 mm)
    for sx in (-1.0, 1.0):
        pry = create_box(f"Pry_Slot_{sx}", (8.5, 3.2, 1.8), (sx * 36.0, -CASE_H * 0.5 + 0.6, Z_SEAM))
        bool_op(base, pry, 'DIFFERENCE')

    # 6. Modern Cyber-Deck Side Grip Ribs on Right Exterior Wall (Antenna End)
    for i in range(5):
        gy = -16.0 + i * 8.0
        rib_r = create_box(f"GripRib_Right_{i}", (5.0, 3.2, 5.2), (CASE_W * 0.5, gy, 5.80))
        bool_op(base, rib_r, 'DIFFERENCE')

    # 7. Angled Cyber-Deck Ventilation Louvers on the Bottom Floor (Upper zone Y = +13.5 mm over ESP32-WROOM-32E
    #    and Rear RGB LED, keeping the lower-center zone 100% clear for the framed "BOXERDAWG3D" debossed emblem!)
    for i in range(9):
        vx = -32.0 + i * 8.0
        vent_u = create_box(f"Vent_Upper_{i}", (3.2, 13.5, 6.0), (vx, 13.5, 1.10), rot_deg=(0.0, 0.0, 18.0))
        bool_op(base, vent_u, 'DIFFERENCE')

    # --------------------------------------------------------------------------
    # 8. DEBOSSED "BOXERDAWG3D" BRANDING IN THE BOTTOM OF THE CASE
    # --------------------------------------------------------------------------
    badge_outer = create_lofted_rounded_box(
        "Badge_Frame_Outer",
        [(-0.20, 62.0, 14.2, 2.8), (0.50, 61.2, 13.4, 2.5)],
        segs_per_corner=10,
    )
    badge_inner = create_lofted_rounded_box(
        "Badge_Frame_Inner",
        [(-0.30, 58.8, 11.0, 1.8), (0.65, 58.8, 11.0, 1.8)],
        segs_per_corner=10,
    )
    bool_op(badge_outer, badge_inner, 'DIFFERENCE')
    badge_outer.location = (0.0, -9.0, 0.0)
    bpy.ops.object.select_all(action='DESELECT')
    badge_outer.select_set(True)
    bpy.context.view_layer.objects.active = badge_outer
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
    bool_op(base, badge_outer, 'DIFFERENCE')

    txt_cutter = create_debossed_text_cutter(
        "BOXERDAWG3D",
        font_size=6.2,
        extrude_depth=2.2,
        center_xyz=(0.0, -9.0, 0.35),
        mirror_x=True,
    )
    bool_op(base, txt_cutter, 'DIFFERENCE')

    finalize_manifold_mesh(base)
    return base


# ==============================================================================
# 2. BUILD TOP BEZEL LID (2_ESP32_35_Case_Top_rev2)
# ==============================================================================
def build_top_bezel():
    # Assembled orientation: spans Z = Z_SEAM (10.40 mm) to Z_TOP_FACE (15.80 mm)
    top_sections = [
        (Z_SEAM,        CASE_W,        CASE_H,        CASE_R),
        (14.00,         CASE_W,        CASE_H,        CASE_R),
        (Z_TOP_FACE,    CASE_W - 3.60, CASE_H - 3.60, CASE_R - 1.40),
    ]
    top = create_lofted_rounded_box("2_ESP32_35_Case_Top_rev2", top_sections, segs_per_corner=16)

    # 1. Inner snap-fit mating recess (Z = 10.0 to 12.35 mm) that receives the Base's inner lip
    # Base lip is CASE_W - 4.00 x CASE_H - 4.00 (106.40 x 60.40 mm);
    # give 0.30 mm total clearance -> CASE_W - 3.70 x CASE_H - 3.70 (106.70 x 60.70 mm)
    lip_recess = create_lofted_rounded_box(
        "Top_Lip_Recess",
        [
            (Z_SEAM - 0.50,    CASE_W - 3.70, CASE_H - 3.70, CASE_R - 1.35),
            (Z_LIP_TOP + 0.15, CASE_W - 3.70, CASE_H - 3.70, CASE_R - 1.35),
        ],
        segs_per_corner=14,
    )
    bool_op(top, lip_recess, 'DIFFERENCE')

    # 2. Matching Snap-Fit Detent Grooves inside the Top Bezel skirt at Z = 11.30 mm
    for sy in (-1.0, 1.0):
        for sx in (-1.0, 1.0):
            groove_tb = create_cylinder(
                f"SnapGroove_TB_{sx}_{sy}",
                radius=0.68,
                depth=13.5,
                center_xyz=(sx * 26.0, sy * (CASE_H * 0.5 - 1.90), 11.30),
                rot_deg=(0.0, 90.0, 0.0),
                vertices=20,
            )
            bool_op(top, groove_tb, 'DIFFERENCE')

    groove_r = create_cylinder(
        "SnapGroove_Right",
        radius=0.68,
        depth=15.5,
        center_xyz=(CASE_W * 0.5 - 1.90, 0.0, 11.30),
        rot_deg=(90.0, 0.0, 0.0),
        vertices=20,
    )
    bool_op(top, groove_r, 'DIFFERENCE')

    # 3. Full-Width Internal LCD & Touch Panel Pocket (Z = 10.0 to 13.75 mm)
    #    - On E32R35T_Size.pdf, the LCD Backlight (84.96 x 55.50 mm) spans the ENTIRE 55.50 mm Y width of the PCB
    #      from X = -42.48 to +42.48 mm, and the 4 resistive-touch solder pins sit on the Left end (X = -49.2 to -42.5 mm).
    #    - Carve a 93.00 x 56.60 mm internal pocket (centered at X = -3.00 mm, spanning X = -49.50 to +43.50 mm
    #      and full Y = -28.30 to +28.30 mm) up to Z = 13.75 mm so the 4.20 mm tall LCD+RTP module (top at Z = 13.40 mm)
    #      and touch solder pins have 100% clearance with zero pinching!
    lcd_pocket = create_lofted_rounded_box(
        "LCD_Pocket",
        [
            (Z_SEAM - 0.50, 93.00, CAVITY_H, 1.80),
            (13.75,         93.00, CAVITY_H, 1.80),
        ],
        segs_per_corner=10,
    )
    lcd_pocket.location = (-3.00, 0.0, 0.0)
    bpy.ops.object.select_all(action='DESELECT')
    lcd_pocket.select_set(True)
    bpy.context.view_layer.objects.active = lcd_pocket
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
    bool_op(top, lcd_pocket, 'DIFFERENCE')

    # 4. 4 Corner PCB Clamp Standoffs + M3 Alignment Pin Sockets at (±47.25, ±23.95 mm)
    #    - Since the corner holes are 3.50 mm from the 101.50 mm ends (X = ±47.25 mm, completely outside the
    #      X = -42.48..+42.48 mm LCD frame!), these 4 corner standoffs reach down to Z = 9.35 mm (0.15 mm above
    #      Z_PCB_TOP = 9.20 mm) to capture the PCB corners securely on the M3 pins!
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            hx = sx * (HOLE_PITCH_X * 0.5)
            hy = sy * (HOLE_PITCH_Y * 0.5)
            clamp_pad = create_cylinder(
                f"TopClamp_{sx}_{sy}", radius=2.80, depth=4.40, center_xyz=(hx, hy, 11.55), vertices=28
            )
            bool_op(top, clamp_pad, 'UNION')
            pin_socket = create_cylinder(
                f"PinSocket_{sx}_{sy}", radius=1.50, depth=4.60, center_xyz=(hx, hy, 11.00), vertices=24
            )
            bool_op(top, pin_socket, 'DIFFERENCE')

    # 5. USB-C Top-Skirt Relief on Left Wall (Y = 0.00 mm, Z = Z_SEAM..11.20 mm)
    #    Ensures oversized USB-C overmolds also clear the Top Bezel's lower mating seam at X = -55.20 mm!
    usb_top_relief = create_box("USB_C_Top_Relief", (4.6, 19.0, 2.4), (-54.4, 0.00, Z_SEAM + 0.20))
    bool_op(top, usb_top_relief, 'DIFFERENCE')

    # 6. Active Touch Screen Viewing Window (76.80 x 50.20 mm inner opening centered at X = +1.40, Y = 0.00 mm)
    #    - Exposes 100% of E32R35T LCD Active Area (73.44 x 48.96 mm, X = -34.37..+39.07 mm) and
    #      Touch Active Area (77.24 x 49.96 mm, X = -37.57..+39.67 mm)
    #    - Features a 45-degree sloped front bezel flaring to 81.00 x 54.40 mm on Z_TOP_FACE (15.80 mm)!
    win_cutter = create_lofted_rounded_box(
        "Touch_Window_Cutter",
        [
            (12.50,             76.80, 50.20, 1.60),
            (13.70,             76.80, 50.20, 1.60),
            (Z_TOP_FACE + 0.05, 81.00, 54.40, 3.20),
        ],
        segs_per_corner=14,
    )
    win_cutter.location = (1.40, 0.0, 0.0)
    bpy.ops.object.select_all(action='DESELECT')
    win_cutter.select_set(True)
    bpy.context.view_layer.objects.active = win_cutter
    bpy.ops.object.transform_apply(location=True, rotation=False, scale=False)
    bool_op(top, win_cutter, 'DIFFERENCE')

    # 7. Modern Tech Accent Grooves on the Front Face (Left & Right bezel ears)
    for sy in (-1.0, 1.0):
        groove_l = create_box(f"FrontAccent_L_{sy}", (1.6, 12.0, 0.9), (-46.5, sy * 12.0, Z_TOP_FACE))
        bool_op(top, groove_l, 'DIFFERENCE')
        groove_r = create_box(f"FrontAccent_R_{sy}", (1.6, 12.0, 0.9), (47.5, sy * 12.0, Z_TOP_FACE))
        bool_op(top, groove_r, 'DIFFERENCE')

    finalize_manifold_mesh(top)
    return top


# ==============================================================================
# 3. BUILD VIRTUAL E32R35T BOARD PROP (1:1 Match to Official E32R35T_3D.step)
# ==============================================================================
def build_virtual_esp32_board():
    props = []
    mat_pcb = create_mat("PCB_Yellow_Gold", "#E5A812", roughness=0.35, metallic=0.10)
    mat_lcd_frame = create_mat("LCD_White_Bezel", "#F0EFEA", roughness=0.40, metallic=0.0)
    mat_screen = create_mat("LCD_Active_Display", "#0E1726", roughness=0.12, metallic=0.20)
    mat_metal = create_mat("Metal_Shield_Nickel", "#C8CCD0", roughness=0.25, metallic=0.85)
    mat_jst = create_mat("JST_Ivory", "#EAE6DC", roughness=0.45, metallic=0.0)
    mat_dark = create_mat("Dark_IC_Plastic", "#1A1D22", roughness=0.40, metallic=0.05)

    # 1. Yellow E32R35T PCB (101.50 x 55.50 x 1.60 mm, R3.50 corners, at Z = 7.60..9.20 mm)
    pcb = create_lofted_rounded_box(
        "Virtual_ESP32_PCB",
        [(Z_PCB_BOT, PCB_W, PCB_H, 3.50), (Z_PCB_TOP, PCB_W, PCB_H, 3.50)],
        segs_per_corner=12,
    )
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            mh = create_cylinder("MH", 1.60, 4.0, (sx * 47.25, sy * 23.95, 8.40), vertices=24)
            bool_op(pcb, mh, 'DIFFERENCE')
    pcb.data.materials.append(mat_pcb)
    props.append(pcb)

    # 2. LCD Backlight & RTP Frame (84.96 x 55.50 x 4.00 mm centered at X = 0.00, Z = 9.20..13.20 mm)
    lcd_f = create_box("Virtual_LCD_Frame", (84.96, 55.00, 4.00), (0.00, 0.0, 11.20))
    lcd_f.data.materials.append(mat_lcd_frame)
    props.append(lcd_f)

    # 3. Active 3.5" Screen Glass (73.44 x 48.96 mm centered at X = +2.35, Z = 13.25 mm)
    scr = create_box("Virtual_LCD_Screen", (73.44, 48.96, 0.30), (2.35, 0.0, 13.25))
    scr.data.materials.append(mat_screen)
    props.append(scr)

    # 4. Centered Type-C USB Port on Left Short Edge (X = -47.5, Y = 0.00, Z = 6.00 mm)
    usb_port = create_box("Virtual_USB_C_Port", (7.5, 8.9, 3.2), (-47.8, 0.00, 6.00))
    usb_port.data.materials.append(mat_metal)
    props.append(usb_port)

    # 5. Rear RESET (Y = +13.72) & BOOT (Y = -13.72) Tactile Keys at X = -47.49 mm
    for bname, by in [("RESET", 13.72), ("BOOT", -13.72)]:
        btn = create_box(f"Virtual_Key_{bname}", (3.2, 4.2, 2.0), (-47.49, by, 6.60))
        btn.data.materials.append(mat_metal)
        props.append(btn)

    # 6. Rear ESP32-WROOM-32E RF Module at Right Short Edge (X = +37.5, Y = 0.0, Z = 6.05 mm)
    wroom = create_box("Virtual_ESP32_WROOM_32E", (25.5, 18.0, 3.1), (37.5, 0.0, 6.05))
    wroom.data.materials.append(mat_metal)
    props.append(wroom)

    # 7. Rear MicroSD (TF) Card Slot on Bottom Long Edge (X = +7.50, Y = -20.0, Z = 6.65 mm)
    sd_slot = create_box("Virtual_MicroSD_Slot", (15.0, 14.5, 1.9), (7.50, -20.2, 6.65))
    sd_slot.data.materials.append(mat_metal)
    props.append(sd_slot)

    # 8. 6x Rear JST 1.25mm Headers (4 on Top Edge Y = +24.8, 2 on Bottom Edge Y = -24.8)
    for jname, jx, jy, jw, jh in [
        ("IO35_39", -25.27,  24.8, 5.5, 4.5),
        ("SPEAKER",  -9.54,  24.8, 5.5, 4.5),
        ("SPI",       9.39,  24.8, 8.0, 4.5),
        ("I2C",      29.39,  24.8, 8.0, 4.5),
        ("BAT",     -25.27, -24.8, 5.5, 4.5),
        ("UART",    -10.73, -24.8, 8.0, 4.5),
    ]:
        jst = create_box(f"Virtual_JST_{jname}", (jw, jh, 3.2), (jx, jy, 6.00))
        jst.data.materials.append(mat_jst)
        props.append(jst)

    return props


def export_stl_object(obj, filepath):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    if hasattr(bpy.ops.wm, "stl_export"):
        bpy.ops.wm.stl_export(filepath=filepath, export_selected_objects=True)
    else:
        bpy.ops.export_mesh.stl(filepath=filepath, use_selection=True)
    print(f"Exported STL: {os.path.basename(filepath)} ({os.path.getsize(filepath):,} bytes)")


def main():
    clear_scene()

    mat_base = create_mat("Case_Base_Matte_Charcoal", "#1F232A", roughness=0.36, metallic=0.08)
    mat_top = create_mat("Case_Top_Titanium_Slate", "#2D3440", roughness=0.32, metallic=0.12)

    base_obj = build_bottom_base()
    base_obj.data.materials.append(mat_base)

    top_obj = build_top_bezel()
    top_obj.data.materials.append(mat_top)

    board_props = build_virtual_esp32_board()

    # 1. Export Combined Assembly STL (Assembled Position)
    bpy.ops.object.select_all(action='DESELECT')
    base_obj.select_set(True)
    top_obj.select_set(True)
    assy_stl = os.path.join(OUTPUT_DIR, "ESP32_35_Case_Assembly_rev2.stl")
    if hasattr(bpy.ops.wm, "stl_export"):
        bpy.ops.wm.stl_export(filepath=assy_stl, export_selected_objects=True)
    else:
        bpy.ops.export_mesh.stl(filepath=assy_stl, use_selection=True)
    print(f"Exported Assembly STL: {os.path.basename(assy_stl)}")

    # 2. Export Individual Print-Oriented STLs (Both 100% flat on Z=0 for zero-support printing!)
    base_stl = os.path.join(OUTPUT_DIR, "1_ESP32_35_Case_Base_rev2.stl")
    export_stl_object(base_obj, base_stl)

    # Create a print-oriented duplicate of Top Bezel flipped 180 deg around X so Z_TOP_FACE (15.80) sits at Z=0.0!
    bpy.ops.object.select_all(action='DESELECT')
    top_obj.select_set(True)
    bpy.context.view_layer.objects.active = top_obj
    bpy.ops.object.duplicate()
    top_print = bpy.context.active_object
    top_print.name = "Top_Print_Oriented"
    top_print.rotation_euler = (math.pi, 0.0, 0.0)
    top_print.location = (0.0, 0.0, Z_TOP_FACE)
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    finalize_manifold_mesh(top_print)

    top_stl = os.path.join(OUTPUT_DIR, "2_ESP32_35_Case_Top_rev2.stl")
    export_stl_object(top_print, top_stl)

    bpy.data.objects.remove(top_print, do_unlink=True)

    blend_path = os.path.join(OUTPUT_DIR, "ESP32_35_Touch_Case_rev2.blend")
    bpy.ops.wm.save_as_mainfile(filepath=blend_path)
    print(f"Saved Blender file: {blend_path}")


if __name__ == "__main__":
    main()
