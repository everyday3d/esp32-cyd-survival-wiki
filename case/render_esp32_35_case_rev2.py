#!/usr/bin/env python3
"""
Blender 5.2 Studio Renderer for Hosyond 3.5" ESP32-32E Touch Display Case (E32R35T — rev2)
Renders:
- preview_hero_assembled_rev2.png (Assembled 3/4 studio hero view showing centered USB-C port & side RESET/BOOT)
- preview_exploded_snap_fit_rev2.png (Exploded 2-piece snap-fit + E32R35T PCB view)
- preview_bottom_boxerdawg3d_rev2.png (Bottom exterior highlighting debossed BOXERDAWG3D + cooling louvers + RESET/BOOT holes)
- preview_print_orientation_rev2.png (Dual showcase layout)
- MakerWorld_Clean_4x3_rev2.png (1600x1200)
- MakerWorld_Clean_3x4_rev2.png (1200x1600)
- MakerWorld_Clean_16x9_rev2.png (1920x1080)
"""

import os
import math
import bpy
import bmesh
from mathutils import Vector

OUTPUT_DIR = os.path.dirname(os.path.abspath(__file__))
BLEND_PATH = os.path.join(OUTPUT_DIR, "ESP32_35_Touch_Case_rev2.blend")


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def create_mat(name, hex_col, roughness=0.40, metallic=0.05, emission_hex=None, emission_strength=0.0):
    h = hex_col.lstrip("#")
    r = srgb_to_linear(int(h[0:2], 16) / 255.0)
    g = srgb_to_linear(int(h[2:4], 16) / 255.0)
    b = srgb_to_linear(int(h[4:6], 16) / 255.0)
    mat = bpy.data.materials.new(name=name)
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
        bsdf.inputs["Roughness"].default_value = roughness
        bsdf.inputs["Metallic"].default_value = metallic
        if emission_hex and "Emission Color" in bsdf.inputs:
            eh = emission_hex.lstrip("#")
            er = srgb_to_linear(int(eh[0:2], 16) / 255.0)
            eg = srgb_to_linear(int(eh[2:4], 16) / 255.0)
            eb = srgb_to_linear(int(eh[4:6], 16) / 255.0)
            bsdf.inputs["Emission Color"].default_value = (er, eg, eb, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def verify_manifold(obj):
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    non_man = [e for e in bm.edges if not e.is_manifold]
    print(f"[Manifold Check] {obj.name}: verts={len(bm.verts)}, faces={len(bm.faces)}, non_manifold_edges={len(non_man)}")
    bm.free()


def add_screen_ui_accent():
    """Adds a sleek cyber-deck UI glow bar on the virtual 3.5-inch screen for studio renders."""
    ui_objs = []
    mat_cyan = create_mat("UI_Cyan", "#00E5FF", roughness=0.2, emission_hex="#00E5FF", emission_strength=2.2)
    mat_amber = create_mat("UI_Amber", "#FFB300", roughness=0.2, emission_hex="#FFB300", emission_strength=2.0)
    mat_green = create_mat("UI_Green", "#00E676", roughness=0.2, emission_hex="#00E676", emission_strength=2.5)

    bars = [
        ("UI_Header", (64.0, 3.0, 0.12), (2.35, 19.0, 13.45), mat_cyan),
        ("UI_Card1",  (29.0, 18.0, 0.10), (-14.5, 4.0, 13.45), mat_cyan),
        ("UI_Card2",  (29.0, 18.0, 0.10), (19.5, 4.0, 13.45), mat_amber),
        ("UI_BarBot", (64.0, 6.0, 0.10), (2.35, -14.0, 13.45), mat_green),
    ]
    for name, sz, loc, mat in bars:
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=loc)
        o = bpy.context.active_object
        o.name = name
        o.scale = sz
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        o.data.materials.append(mat)
        ui_objs.append(o)
    return ui_objs


def setup_studio():
    world = bpy.data.worlds.new("StudioWorld")
    bpy.context.scene.world = world
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.14, 0.16, 0.19, 1.0)
        bg.inputs["Strength"].default_value = 0.95

    bpy.ops.mesh.primitive_plane_add(size=2400.0, location=(0.0, 0.0, -0.15))
    bg_plane = bpy.context.active_object
    bg_plane.name = "Studio_Backdrop"
    mat_bg = create_mat("Studio_Dark_Slate", "#181C22", roughness=0.65, metallic=0.05)
    bg_plane.data.materials.append(mat_bg)

    for name, rot_deg, energy, angle_deg, col in [
        ("Key_Sun",  (32.0, -26.0, -20.0), 4.0, 12.0, (1.0, 0.98, 0.95)),
        ("Fill_Sun", (-28.0, 32.0, 25.0),  2.0, 26.0, (0.90, 0.95, 1.0)),
        ("Rim_Sun",  (48.0, 14.0, 155.0),  2.6, 15.0, (0.85, 0.95, 1.0)),
    ]:
        ldata = bpy.data.lights.new(name=name, type='SUN')
        ldata.energy = energy
        ldata.angle = math.radians(angle_deg)
        ldata.color = col
        lobj = bpy.data.objects.new(name=name, object_data=ldata)
        bpy.context.collection.objects.link(lobj)
        lobj.rotation_euler = tuple(math.radians(a) for a in rot_deg)

    return bg_plane


def point_camera_at(cam_obj, target_pt):
    direction = Vector(target_pt) - cam_obj.location
    rot_quat = direction.to_track_quat('-Z', 'Y')
    cam_obj.rotation_euler = rot_quat.to_euler()


def render_view(cam_obj, filepath, res_x, res_y, lens=55.0):
    scene = bpy.context.scene
    scene.camera = cam_obj
    cam_obj.data.lens = lens
    cam_obj.data.clip_start = 1.0
    cam_obj.data.clip_end = 5000.0
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y
    scene.render.resolution_percentage = 100
    scene.render.filepath = filepath
    bpy.ops.render.render(write_still=True)
    print(f"Rendered: {os.path.basename(filepath)} ({res_x}x{res_y})")


def main():
    bpy.ops.wm.open_mainfile(filepath=BLEND_PATH)
    scene = bpy.context.scene

    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties['engine'].enum_items]
    if 'BLENDER_EEVEE_NEXT' in engines:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'
    elif 'BLENDER_EEVEE' in engines:
        scene.render.engine = 'BLENDER_EEVEE'
    else:
        scene.render.engine = 'CYCLES'

    base_obj = bpy.data.objects["1_ESP32_35_Case_Base_rev2"]
    top_obj = bpy.data.objects["2_ESP32_35_Case_Top_rev2"]
    verify_manifold(base_obj)
    verify_manifold(top_obj)

    board_objs = [o for o in bpy.data.objects if o.name.startswith("Virtual_")]
    ui_objs = add_screen_ui_accent()

    mat_base = create_mat("Render_Base_Charcoal", "#242A34", roughness=0.35, metallic=0.12)
    mat_top = create_mat("Render_Top_Cyber_Titanium", "#3A4454", roughness=0.30, metallic=0.18)
    mat_pcb_gold = create_mat("Render_PCB_CYD_Yellow", "#F2A900", roughness=0.32, metallic=0.15)
    base_obj.data.materials.clear()
    base_obj.data.materials.append(mat_base)
    top_obj.data.materials.clear()
    top_obj.data.materials.append(mat_top)

    pcb_obj = bpy.data.objects.get("Virtual_ESP32_PCB")
    if pcb_obj:
        pcb_obj.data.materials.clear()
        pcb_obj.data.materials.append(mat_pcb_gold)

    bg_plane = setup_studio()

    cam_data = bpy.data.cameras.new("StudioCam")
    cam_obj = bpy.data.objects.new("StudioCam", cam_data)
    bpy.context.collection.objects.link(cam_obj)

    # 1. Assembled Hero 3/4 View (showing centered USB-C port on Left, Top Bezel, MicroSD scoop on Bottom)
    cam_obj.location = Vector((-98.0, -115.0, 125.0))
    point_camera_at(cam_obj, (0.0, 0.0, 7.5))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "preview_hero_assembled_rev2.png"), 1600, 1200, lens=55.0)

    # 2. Exploded Snap-Fit + E32R35T PCB View (Base at Z=0, PCB lifted +18 mm, Top Bezel lifted +38 mm)
    for o in board_objs + ui_objs:
        o.location.z += 18.0
    top_obj.location.z += 38.0

    cam_obj.location = Vector((-120.0, -145.0, 145.0))
    point_camera_at(cam_obj, (0.0, 0.0, 24.0))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "preview_exploded_snap_fit_rev2.png"), 1600, 1200, lens=52.0)

    # Reset positions
    for o in board_objs + ui_objs:
        o.location.z -= 18.0
    top_obj.location.z -= 38.0

    # 3. Create Upside-Down Base Copy showing the Debossed "BOXERDAWG3D" bottom, cooling vents & RESET/BOOT holes
    bpy.ops.object.select_all(action='DESELECT')
    base_obj.select_set(True)
    bpy.context.view_layer.objects.active = base_obj
    bpy.ops.object.duplicate()
    base_flipped = bpy.context.active_object
    base_flipped.name = "Base_Bottom_Showcase"
    base_flipped.rotation_euler = (0.0, math.pi, 0.0)
    base_flipped.location = (0.0, 0.0, 12.2)

    for o in [base_obj, top_obj] + board_objs + ui_objs:
        o.hide_render = True

    cam_obj.location = Vector((0.0, -52.0, 165.0))
    point_camera_at(cam_obj, (0.0, -1.0, 10.0))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "preview_bottom_boxerdawg3d_rev2.png"), 1600, 1100, lens=55.0)

    for o in [base_obj, top_obj] + board_objs + ui_objs:
        o.hide_render = False

    # 4. Compact Diagonal Dual-Showcase Layout
    shift_main = Vector((-16.0, 32.0, 0.0))
    base_obj.location += shift_main
    top_obj.location += shift_main + Vector((0.0, 0.0, 6.5))
    for o in board_objs + ui_objs:
        o.location += shift_main + Vector((0.0, 0.0, 2.5))

    base_flipped.location = Vector((16.0, -34.0, 12.2))

    cam_obj.location = Vector((0.0, -175.0, 225.0))
    point_camera_at(cam_obj, (0.0, -1.0, 8.0))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "preview_print_orientation_rev2.png"), 1600, 1150, lens=50.0)

    # 5. MakerWorld Clean 4:3 (1600x1200)
    cam_obj.location = Vector((-38.0, -175.0, 265.0))
    point_camera_at(cam_obj, (-38.0, 18.0, 8.0))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "MakerWorld_Clean_4x3_rev2.png"), 1600, 1200, lens=44.0)

    # 6. MakerWorld Clean 3:4 (1200x1600)
    cam_obj.location = Vector((0.0, -165.0, 285.0))
    point_camera_at(cam_obj, (0.0, -2.0, 8.0))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "MakerWorld_Clean_3x4_rev2.png"), 1200, 1600, lens=45.0)

    # 7. MakerWorld Clean 16:9 (1920x1080)
    cam_obj.location = Vector((-46.0, -185.0, 285.0))
    point_camera_at(cam_obj, (-46.0, 2.0, 8.0))
    render_view(cam_obj, os.path.join(OUTPUT_DIR, "MakerWorld_Clean_16x9_rev2.png"), 1920, 1080, lens=40.0)


if __name__ == "__main__":
    main()
