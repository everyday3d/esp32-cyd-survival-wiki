#!/usr/bin/env python3
"""
Builds the Bambu Studio Print-Ready 3MF & Rule 7 MakerWorld Cover Graphics for:
Hosyond 3.5" ESP32-32E Touch Screen Display Case (QDtech / LCDWiki E32R35T / E32N35T — B0D93MBWC2) (rev2)

Outputs:
- ESP32_35_Touch_Case_rev2.3mf (Single-Plate 2-Piece Snap-Fit Project: Base + Face-Down Top Bezel, 100% Support-Free)
- MakerWorld_Cover_4x3.jpg (1600x1200)
- MakerWorld_Cover_3x4.jpg (1200x1600 Bambu Handy)
- MakerWorld_Banner_16x9.jpg (1920x1080)
"""

import os
import json
import struct
import shutil
import zipfile
import subprocess

REV_DIR = os.path.dirname(os.path.abspath(__file__))
MODEL_ROOT_DIR = os.path.dirname(REV_DIR)
REPO_ROOT_DIR = os.path.dirname(os.path.dirname(MODEL_ROOT_DIR))
ARTIFACT_DIR = r"C:\Users\jbuch\.gemini\antigravity\brain\1a50bb33-dab6-4e7a-97a8-30a3d6d683e6"

FILAMENT_COLORS_3 = [
    "#242A34",  # Slot 1: Matte Charcoal / Cyber Slate (Snap-Fit Base Shell)
    "#3A4454",  # Slot 2: Titanium Slate (Face-Down Top Bezel — or print all in Slot 1!)
    "#00E5FF",  # Slot 3: Optional Accent Color
]


def read_binary_stl(filepath, offset_xyz=(0.0, 0.0, 0.0)):
    ox, oy, oz = offset_xyz
    vert_map = {}
    verts = []
    tris = []
    with open(filepath, "rb") as f:
        f.read(80)
        num_tris = struct.unpack("<I", f.read(4))[0]
        for _ in range(num_tris):
            data = f.read(50)
            if len(data) < 50:
                break
            vals = struct.unpack("<12fH", data)
            tri_idx = []
            for k in range(3):
                vx = round(vals[3 + k * 3] + ox, 4)
                vy = round(vals[4 + k * 3] + oy, 4)
                vz = round(vals[5 + k * 3] + oz, 4)
                key = (vx, vy, vz)
                idx = vert_map.get(key)
                if idx is None:
                    idx = len(verts)
                    vert_map[key] = idx
                    verts.append(key)
                tri_idx.append(idx)
            tris.append(tuple(tri_idx))
    return verts, tris


def load_bambu_project_settings(colors_list):
    ps = None
    template_candidates = [
        os.path.join(REPO_ROOT_DIR, "models", "thanksgiving_door_corner", "rev1", "Thanksgiving_Door_Corner_4Color_AMS_rev1.3mf"),
        os.path.join(REPO_ROOT_DIR, "models", "santa_stop_here_sign", "rev2", "Santa_Stop_Here_NOAMS 6 Plates.3mf"),
    ]
    for cand in template_candidates:
        if os.path.exists(cand):
            try:
                with zipfile.ZipFile(cand, "r") as zf:
                    if "Metadata/project_settings.config" in zf.namelist():
                        ps = json.loads(zf.read("Metadata/project_settings.config").decode("utf-8"))
                        break
            except Exception:
                pass

    if ps is None:
        ps = {"from": "project", "version": "02.08.02.61"}
        ps["filament_colour"] = list(colors_list)
        ps["filament_type"] = ["PLA"] * len(colors_list)
    else:
        ps["filament_colour"] = list(colors_list)
        if "default_filament_colour" in ps:
            ps["default_filament_colour"] = list(colors_list)
        for key in list(ps.keys()):
            if isinstance(ps[key], list) and len(ps[key]) == 4 and len(colors_list) == 3:
                ps[key] = ps[key][:3]

    ps["enable_support"] = "0"
    ps["brim_type"] = "no_brim"
    ps["wall_loops"] = "3"
    ps["sparse_infill_density"] = "15%"
    ps["sparse_infill_pattern"] = "gyroid"
    ps["enable_prime_tower"] = "0"
    return ps


def validate_and_resave_with_bambu_studio(path_3mf):
    bs_exe = r"C:\Program Files\Bambu Studio\bambu-studio.exe"
    if not os.path.exists(bs_exe):
        return
    tmp_out = path_3mf + ".bs_validated.3mf"
    if os.path.exists(tmp_out):
        os.remove(tmp_out)
    r = subprocess.run([bs_exe, "--export-3mf", tmp_out, path_3mf])
    if r.returncode == 0 and os.path.exists(tmp_out) and os.path.getsize(tmp_out) > 1000:
        shutil.move(tmp_out, path_3mf)
        print(f"  [BambuStudio Verified] Native re-export succeeded: {os.path.basename(path_3mf)} ({os.path.getsize(path_3mf) / 1024:.1f} KB)")
    else:
        if os.path.exists(tmp_out):
            os.remove(tmp_out)
        raise RuntimeError(f"Bambu Studio failed to validate {path_3mf} (rc={r.returncode})")


def build_multiplate_3mf(output_3mf_path, plates_assemblies, title, description):
    object_files = {}
    model_rels_entries = []
    root_resources_xml = []
    build_items_xml = []
    config_objects_xml = []
    config_plates_xml = []
    assemble_items_xml = []
    cut_info_entries = []

    next_obj_id = 1

    for p_info in plates_assemblies:
        p_idx = p_info["plate_idx"]
        tx, ty = p_info["plate_center"]
        parts = p_info["parts"]

        loaded_parts = []
        min_x, max_x = 1e9, -1e9
        min_y, max_y = 1e9, -1e9
        min_z, max_z = 1e9, -1e9

        for pt in parts:
            stl_path = os.path.join(REV_DIR, pt["stl"])
            verts, tris = read_binary_stl(stl_path, pt.get("offset_xyz", (0.0, 0.0, 0.0)))
            loaded_parts.append((pt, verts, tris))
            for vx, vy, vz in verts:
                if vx < min_x: min_x = vx
                if vx > max_x: max_x = vx
                if vy < min_y: min_y = vy
                if vy > max_y: max_y = vy
                if vz < min_z: min_z = vz
                if vz > max_z: max_z = vz

        cx = 0.5 * (min_x + max_x)
        cy = 0.5 * (min_y + max_y)
        cz = 0.5 * (min_z + max_z)
        half_z = cz - min_z

        part_ids = []
        for _ in loaded_parts:
            part_ids.append(next_obj_id)
            next_obj_id += 1
        parent_id = next_obj_id
        next_obj_id += 1

        obj_blocks = []
        comp_entries = []
        part_configs = []
        total_faces = 0
        obj_rel_path = f"3D/Objects/object_{p_idx}.model"

        for vol_idx, ((pt, verts, tris), pid) in enumerate(zip(loaded_parts, part_ids)):
            ext_id = pt["extruder"]
            n_faces = len(tris)
            total_faces += n_faces

            uuid_obj = f"{p_idx:04x}{pid:04x}-81cb-4c03-9d28-80fed5dfa1dc"
            uuid_comp = f"{p_idx:04x}{vol_idx:04x}-b206-40ff-9872-83e8017abed1"
            uuid_part = f"{p_idx:04x}{pid:04x}-7e6d-4931-9a9f-7218cba9de1c"

            ob = [
                f'  <object id="{pid}" p:UUID="{uuid_obj}" type="model">',
                '   <mesh>',
                '    <vertices>',
            ]
            for vx, vy, vz in verts:
                ob.append(f'     <vertex x="{vx - cx:.5f}" y="{vy - cy:.5f}" z="{vz - cz:.5f}"/>')
            ob.append('    </vertices>')
            ob.append('    <triangles>')
            for v1, v2, v3 in tris:
                ob.append(f'     <triangle v1="{v1}" v2="{v2}" v3="{v3}"/>')
            ob.extend([
                '    </triangles>',
                '   </mesh>',
                '  </object>',
            ])
            obj_blocks.append("\n".join(ob))

            comp_entries.append(
                f'    <component p:path="/{obj_rel_path}" objectid="{pid}" '
                f'p:UUID="{uuid_comp}" transform="1 0 0 0 1 0 0 0 1 0 0 0"/>'
            )

            part_configs.append(
                f'    <part id="{pid}" subtype="normal_part" uuid="{uuid_part}">\n'
                f'      <metadata key="name" value="{pt["name"]}"/>\n'
                f'      <metadata key="matrix" value="1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1"/>\n'
                f'      <metadata key="source_file" value="{pt["stl"]}"/>\n'
                f'      <metadata key="source_object_id" value="0"/>\n'
                f'      <metadata key="source_volume_id" value="{vol_idx}"/>\n'
                f'      <metadata key="source_offset_x" value="{cx:.5f}"/>\n'
                f'      <metadata key="source_offset_y" value="{cy:.5f}"/>\n'
                f'      <metadata key="source_offset_z" value="{half_z:.5f}"/>\n'
                f'      <metadata key="extruder" value="{ext_id}"/>\n'
                f'      <mesh_stat face_count="{n_faces}" edges_fixed="0" degenerate_facets="0" facets_removed="0" facets_reversed="0" backwards_edges="0"/>\n'
                f'    </part>'
            )

        object_files[obj_rel_path] = "\n".join([
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" '
            'xmlns:BambuStudio="http://schemas.bambulab.com/package/2021" '
            'xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06" requiredextensions="p">',
            ' <metadata name="BambuStudio:3mfVersion">1</metadata>',
            ' <resources>',
            *obj_blocks,
            ' </resources>',
            ' <build/>',
            '</model>',
        ])

        model_rels_entries.append(
            f' <Relationship Target="/{obj_rel_path}" Id="rel-{p_idx}" '
            f'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
        )

        uuid_parent = f"0000000{p_idx}-61cb-4c03-9d28-80fed5dfa1dc"
        uuid_build = f"000000{parent_id:02x}-b1ec-4553-aec9-835e5b724bb4"

        root_resources_xml.extend([
            f'  <object id="{parent_id}" p:UUID="{uuid_parent}" type="model">',
            '   <components>',
            *comp_entries,
            '   </components>',
            '  </object>',
        ])

        build_items_xml.append(
            f'  <item objectid="{parent_id}" p:UUID="{uuid_build}" '
            f'transform="1 0 0 0 1 0 0 0 1 {tx:.4f} {ty:.4f} {half_z:.5f}" printable="1"/>'
        )

        config_objects_xml.extend([
            f'  <object id="{parent_id}">',
            f'    <metadata key="name" value="{p_info["assembly_name"]}"/>',
            '    <metadata key="extruder" value="1"/>',
            f'    <metadata face_count="{total_faces}"/>',
            *part_configs,
            '  </object>',
        ])

        config_plates_xml.extend([
            '  <plate>',
            f'    <metadata key="plater_id" value="{p_idx}"/>',
            f'    <metadata key="plater_name" value="{p_info["plate_name"]}"/>',
            '    <metadata key="locked" value="false"/>',
            '    <metadata key="filament_map_mode" value="Auto For Flush"/>',
            '    <metadata key="filament_maps" value="1 1 1"/>',
            '    <metadata key="filament_volume_maps" value="0 0 0"/>',
            '    <model_instance>',
            f'      <metadata key="object_id" value="{parent_id}"/>',
            '      <metadata key="instance_id" value="0"/>',
            f'      <metadata key="identify_id" value="{72 + (p_idx - 1) * 100}"/>',
            '    </model_instance>',
            '  </plate>',
        ])

        assemble_items_xml.append(
            f'   <assemble_item object_id="{parent_id}" instance_id="0" '
            f'transform="1 0 0 0 1 0 0 0 1 {tx - 128.0:.4f} {ty - 128.0:.4f} {half_z:.5f}" offset="0 0 0" />'
        )
        for vol_idx in range(len(loaded_parts)):
            assemble_items_xml.append(
                f'   <assemble_item object_id="{parent_id}" volume_id="{vol_idx}" transform="1 0 0 0 1 0 0 0 1 0 0 0" />'
            )

        cut_info_entries.extend([
            f' <object id="{p_idx}">',
            '  <cut_id id="0" check_sum="1" connectors_cnt="0"/>',
            ' </object>',
        ])

    root_model_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02" '
        'xmlns:BambuStudio="http://schemas.bambulab.com/package/2021" '
        'xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06" requiredextensions="p">',
        ' <metadata name="Application">BambuStudio-02.08.02.61</metadata>',
        ' <metadata name="BambuStudio:3mfVersion">1</metadata>',
        ' <metadata name="CreationDate">2026-10-16</metadata>',
        f' <metadata name="Description">{description}</metadata>',
        ' <metadata name="Designer">BoxerDawg 3D / Everyday3D</metadata>',
        ' <metadata name="DesignerUserId">4212740641</metadata>',
        ' <metadata name="ModificationDate">2026-10-16</metadata>',
        f' <metadata name="ProfileTitle">{title}</metadata>',
        f' <metadata name="Title">{title}</metadata>',
        ' <resources>',
        *root_resources_xml,
        ' </resources>',
        ' <build p:UUID="2c7c17d8-22b5-4d84-8835-1976022ea369">',
        *build_items_xml,
        ' </build>',
        '</model>',
    ])

    model_settings_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<config>',
        *config_objects_xml,
        *config_plates_xml,
        '  <assemble>',
        *assemble_items_xml,
        '  </assemble>',
        '</config>',
    ])

    content_types_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">',
        ' <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>',
        ' <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>',
        ' <Default Extension="png" ContentType="image/png"/>',
        ' <Default Extension="gcode" ContentType="text/x.gcode"/>',
        '</Types>',
    ])

    root_rels_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">',
        ' <Relationship Target="/3D/3dmodel.model" Id="rel-1" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>',
        '</Relationships>',
    ])

    model_rels_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">',
        *model_rels_entries,
        '</Relationships>',
    ])

    slice_info_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<config>',
        '  <header>',
        '    <header_item key="X-BBL-Client-Type" value="slicer"/>',
        '    <header_item key="X-BBL-Client-Version" value="02.08.02.61"/>',
        '  </header>',
        '</config>',
    ])

    cut_info_xml = "\n".join([
        '<?xml version="1.0" encoding="UTF-8"?>',
        '<objects>',
        *cut_info_entries,
        '</objects>',
    ])

    filament_seq = {
        f"plate_{p['plate_idx']}": {"nozzle_sequence": [], "optimal_assignment": [], "sequence": []}
        for p in plates_assemblies
    }

    ps = load_bambu_project_settings(FILAMENT_COLORS_3)

    with zipfile.ZipFile(output_3mf_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("[Content_Types].xml", content_types_xml.encode("utf-8"))
        zf.writestr("_rels/.rels", root_rels_xml.encode("utf-8"))
        zf.writestr("3D/3dmodel.model", root_model_xml.encode("utf-8"))
        zf.writestr("3D/_rels/3dmodel.model.rels", model_rels_xml.encode("utf-8"))
        for rel_path, xml_str in object_files.items():
            zf.writestr(rel_path, xml_str.encode("utf-8"))
        zf.writestr("Metadata/model_settings.config", model_settings_xml.encode("utf-8"))
        zf.writestr("Metadata/project_settings.config", json.dumps(ps, indent=4).encode("utf-8"))
        zf.writestr("Metadata/slice_info.config", slice_info_xml.encode("utf-8"))
        zf.writestr("Metadata/cut_information.xml", cut_info_xml.encode("utf-8"))
        zf.writestr("Metadata/filament_sequence.json", json.dumps(filament_seq).encode("utf-8"))

    validate_and_resave_with_bambu_studio(output_3mf_path)


def load_font(size, style="black"):
    from PIL import ImageFont
    font_map = {
        "black": [r"C:\Windows\Fonts\seguibl.ttf", r"C:\Windows\Fonts\arialbd.ttf"],
        "bold":  [r"C:\Windows\Fonts\segoeuib.ttf", r"C:\Windows\Fonts\arialbd.ttf"],
        "semi":  [r"C:\Windows\Fonts\seguisb.ttf", r"C:\Windows\Fonts\segoeui.ttf"],
        "tech":  [r"C:\Windows\Fonts\bahnschrift.ttf", r"C:\Windows\Fonts\consolab.ttf"],
        "reg":   [r"C:\Windows\Fonts\segoeui.ttf", r"C:\Windows\Fonts\arial.ttf"],
    }
    for fp in font_map.get(style, font_map["bold"]):
        if os.path.exists(fp):
            return ImageFont.truetype(fp, size)
    return ImageFont.load_default()


def apply_studio_vignette(img, mode="left_top"):
    from PIL import Image, ImageDraw, ImageFilter
    w, h = img.size
    scrim = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(scrim)

    if mode == "left_top":
        d.rectangle((0, 0, int(w * 0.42), h), fill=(8, 11, 16, 155))
        d.rectangle((0, 0, int(w * 0.72), int(h * 0.26)), fill=(8, 11, 16, 145))
        scrim = scrim.filter(ImageFilter.GaussianBlur(radius=95))
    elif mode == "top_bottom":
        d.rectangle((0, 0, w, int(h * 0.24)), fill=(8, 11, 16, 175))
        d.rectangle((0, int(h * 0.76), w, h), fill=(8, 11, 16, 185))
        scrim = scrim.filter(ImageFilter.GaussianBlur(radius=85))
    elif mode == "left_wide":
        d.rectangle((0, 0, int(w * 0.46), h), fill=(8, 11, 16, 175))
        scrim = scrim.filter(ImageFilter.GaussianBlur(radius=95))

    return Image.alpha_composite(img, scrim)


def draw_shadow_text(img, pos, text, font, fill=(255, 255, 255, 255), shadow_offset=(0, 4), shadow_blur=8, shadow_alpha=180):
    from PIL import Image, ImageDraw, ImageFilter
    sh_layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sd = ImageDraw.Draw(sh_layer)
    sd.text((pos[0] + shadow_offset[0], pos[1] + shadow_offset[1]), text, font=font, fill=(0, 0, 0, shadow_alpha))
    sh_layer = sh_layer.filter(ImageFilter.GaussianBlur(radius=shadow_blur))
    img = Image.alpha_composite(img, sh_layer)
    d = ImageDraw.Draw(img)
    d.text(pos, text, font=font, fill=fill)
    return img


def draw_pill(img, box, fill_rgba, outline_rgba=None, outline_width=1, radius=24, shadow=True):
    from PIL import Image, ImageDraw, ImageFilter
    if shadow:
        sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
        sd = ImageDraw.Draw(sh)
        sd.rounded_rectangle((box[0], box[1] + 5, box[2], box[3] + 5), radius=radius, fill=(0, 0, 0, 115))
        sh = sh.filter(ImageFilter.GaussianBlur(radius=8))
        img = Image.alpha_composite(img, sh)
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    od.rounded_rectangle(box, radius=radius, fill=fill_rgba, outline=outline_rgba, width=outline_width)
    return Image.alpha_composite(img, ov)


def draw_callout_pointer(img, label_pos, elbow_pos, target_pos, title, subtitle, accent=(255, 184, 46, 255)):
    from PIL import Image, ImageDraw
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(ov)

    f_title = load_font(24, "bold")
    f_sub = load_font(17, "semi")
    lx, ly = label_pos

    d.line([label_pos, elbow_pos, target_pos], fill=(255, 255, 255, 195), width=2)
    d.ellipse((lx - 3, ly - 3, lx + 3, ly + 3), fill=accent)

    tx, ty = target_pos
    d.ellipse((tx - 9, ty - 9, tx + 9, ty + 9), fill=(accent[0], accent[1], accent[2], 80), outline=accent, width=2)
    d.ellipse((tx - 4, ty - 4, tx + 4, ty + 4), fill=(255, 255, 255, 255))

    img = Image.alpha_composite(img, ov)
    img = draw_shadow_text(img, (lx, ly - 34), title, f_title, fill=(255, 255, 255, 255), shadow_offset=(0, 2), shadow_blur=5, shadow_alpha=215)
    img = draw_shadow_text(img, (lx, ly + 6), subtitle, f_sub, fill=accent, shadow_offset=(0, 2), shadow_blur=5, shadow_alpha=215)
    return img


def make_covers():
    from PIL import Image, ImageDraw

    # --------------------------------------------------------------------------
    # 1. 4:3 Cover (1600x1200) — Editorial Magazine / MakerWorld Pro Style
    # --------------------------------------------------------------------------
    clean_43 = os.path.join(REV_DIR, "MakerWorld_Clean_4x3_rev2.png")
    if os.path.exists(clean_43):
        img = Image.open(clean_43).convert("RGBA")
        img = apply_studio_vignette(img, mode="left_top")

        img = draw_pill(img, (54, 42, 470, 80), (242, 158, 18, 245), radius=19)
        d = ImageDraw.Draw(img)
        d.text((74, 48), "FITS E32R35T / E32N35T • B0D93MBWC2", font=load_font(19, "black"), fill=(18, 20, 24, 255))

        img = draw_shadow_text(img, (52, 88), "3.5\" ESP32 TOUCH CYD", load_font(58, "black"), fill=(255, 255, 255, 255))
        img = draw_shadow_text(img, (52, 154), "SNAP-FIT CASE", load_font(62, "black"), fill=(255, 190, 45, 255))
        img = draw_shadow_text(
            img, (56, 228),
            "Two-piece tool-less enclosure  •  Debossed BoxerDawg3D base",
            load_font(23, "semi"), fill=(215, 224, 235, 255)
        )

        img = draw_callout_pointer(
            img,
            label_pos=(56, 390),
            elbow_pos=(380, 390),
            target_pos=(605, 420),
            title="2-Piece Snap-Fit Shell",
            subtitle="4x M3 alignment pins • No screws needed",
            accent=(110, 225, 255, 255),
        )
        img = draw_callout_pointer(
            img,
            label_pos=(56, 535),
            elbow_pos=(380, 535),
            target_pos=(585, 535),
            title="Centered USB-C & Port Access",
            subtitle="Wide USB-C recess • MicroSD scoop • 6x JST",
            accent=(255, 200, 75, 255),
        )
        img = draw_callout_pointer(
            img,
            label_pos=(56, 755),
            elbow_pos=(510, 755),
            target_pos=(965, 765),
            title="Debossed BoxerDawg3D Base",
            subtitle="Recessed emblem • Cooling vents • BOOT/RST",
            accent=(125, 245, 165, 255),
        )

        img = draw_pill(img, (54, 915, 450, 976), (34, 185, 92, 245), radius=30)
        d = ImageDraw.Draw(img)
        d.text((82, 929), "EASY TO PRINT • NO SUPPORTS", font=load_font(22, "black"), fill=(255, 255, 255, 255))

        img = draw_pill(img, (54, 994, 530, 1138), (20, 25, 32, 205), outline_rgba=(255, 255, 255, 45), outline_width=1, radius=22)
        d = ImageDraw.Draw(img)
        d.text((76, 1010), "1-PLATE 3MF  •  PRINT FLAT ON Z=0", font=load_font(18, "tech"), fill=(255, 195, 65, 255))
        d.ellipse((76, 1046, 102, 1072), fill=(36, 42, 52, 255), outline=(255, 255, 255, 210), width=2)
        d.text((114, 1047), "Charcoal Base", font=load_font(18, "semi"), fill=(240, 244, 248, 255))
        d.ellipse((275, 1046, 301, 1072), fill=(68, 80, 98, 255), outline=(255, 255, 255, 210), width=2)
        d.text((313, 1047), "Slate Bezel", font=load_font(18, "semi"), fill=(240, 244, 248, 255))
        d.text((76, 1090), "0.20mm Layer • 3 Walls • Single or 2-Color • Fits A1 Mini", font=load_font(16, "semi"), fill=(180, 195, 210, 255))

        out_rev = os.path.join(REV_DIR, "MakerWorld_Cover_4x3.jpg")
        img.convert("RGB").save(out_rev, quality=95)
        shutil.copy2(out_rev, os.path.join(MODEL_ROOT_DIR, "MakerWorld_Cover_4x3.jpg"))
        if os.path.exists(ARTIFACT_DIR):
            shutil.copy2(out_rev, os.path.join(ARTIFACT_DIR, "MakerWorld_Cover_4x3.jpg"))
        print(f"  Saved 4:3 Cover: {out_rev}")

    # --------------------------------------------------------------------------
    # 2. 3:4 Cover (1200x1600 Bambu Handy Vertical Poster Style)
    # --------------------------------------------------------------------------
    clean_34 = os.path.join(REV_DIR, "MakerWorld_Clean_3x4_rev2.png")
    if os.path.exists(clean_34):
        img = Image.open(clean_34).convert("RGBA")
        img = apply_studio_vignette(img, mode="top_bottom")

        img = draw_pill(img, (56, 48, 495, 88), (242, 158, 18, 245), radius=20)
        d = ImageDraw.Draw(img)
        d.text((78, 55), "FITS E32R35T / E32N35T • B0D93MBWC2", font=load_font(20, "black"), fill=(18, 20, 24, 255))

        img = draw_shadow_text(img, (54, 100), "3.5\" ESP32 TOUCH CYD", load_font(62, "black"), fill=(255, 255, 255, 255))
        img = draw_shadow_text(img, (54, 174), "SNAP-FIT CASE", load_font(68, "black"), fill=(255, 190, 45, 255))
        img = draw_shadow_text(
            img, (58, 254),
            "2-Piece Tool-Less Shell  •  Debossed BoxerDawg3D Bottom",
            load_font(25, "semi"), fill=(220, 228, 238, 255)
        )

        img = draw_pill(img, (56, 1295, 495, 1359), (34, 185, 92, 245), radius=32)
        d = ImageDraw.Draw(img)
        d.text((86, 1310), "EASY TO PRINT • NO SUPPORTS", font=load_font(23, "black"), fill=(255, 255, 255, 255))

        img = draw_pill(img, (515, 1295, 1144, 1359), (24, 30, 38, 215), outline_rgba=(255, 255, 255, 55), outline_width=1, radius=32)
        d = ImageDraw.Draw(img)
        d.ellipse((540, 1313, 568, 1341), fill=(36, 42, 52, 255), outline=(255, 255, 255, 210), width=2)
        d.text((580, 1313), "Base Shell", font=load_font(21, "semi"), fill=(240, 244, 248, 255))
        d.ellipse((715, 1313, 743, 1341), fill=(68, 80, 98, 255), outline=(255, 255, 255, 210), width=2)
        d.text((755, 1313), "Face-Down Bezel", font=load_font(21, "semi"), fill=(240, 244, 248, 255))
        d.ellipse((955, 1313, 983, 1341), fill=(242, 169, 0, 255), outline=(255, 255, 255, 210), width=2)
        d.text((995, 1313), "3.5\" CYD", font=load_font(21, "semi"), fill=(255, 205, 75, 255))

        for bx, bw, btxt in [
            (56,  355, "Face-Down Bezel (Flat Z=0)"),
            (425, 355, "Centered USB-C + MicroSD"),
            (794, 350, "Debossed BoxerDawg3D"),
        ]:
            img = draw_pill(img, (bx, 1384, bx + bw, 1442), (18, 22, 28, 205), outline_rgba=(255, 255, 255, 45), radius=18, shadow=False)
            d = ImageDraw.Draw(img)
            cx, cy = bx + 26, 1413
            d.ellipse((cx - 10, cy - 10, cx + 10, cy + 10), fill=(34, 185, 92, 255))
            d.line([(cx - 4, cy + 1), (cx - 1, cy + 4), (cx + 5, cy - 3)], fill=(255, 255, 255, 255), width=2)
            d.text((bx + 46, 1398), btxt, font=load_font(19, "semi"), fill=(235, 242, 250, 255))

        img = draw_shadow_text(
            img, (58, 1472),
            "1-Plate Bambu Studio 3MF  •  Screwless M3 Corner Pin Cradle  •  Cooling Louvers  •  Fits A1 Mini",
            load_font(21, "tech"), fill=(185, 200, 215, 255)
        )

        out_rev = os.path.join(REV_DIR, "MakerWorld_Cover_3x4.jpg")
        img.convert("RGB").save(out_rev, quality=95)
        shutil.copy2(out_rev, os.path.join(MODEL_ROOT_DIR, "MakerWorld_Cover_3x4.jpg"))
        if os.path.exists(ARTIFACT_DIR):
            shutil.copy2(out_rev, os.path.join(ARTIFACT_DIR, "MakerWorld_Cover_3x4.jpg"))
        print(f"  Saved 3:4 Cover: {out_rev}")

    # --------------------------------------------------------------------------
    # 3. 16:9 Banner (1920x1080 Widescreen Editorial Hero)
    # --------------------------------------------------------------------------
    clean_169 = os.path.join(REV_DIR, "MakerWorld_Clean_16x9_rev2.png")
    if os.path.exists(clean_169):
        img = Image.open(clean_169).convert("RGBA")
        img = apply_studio_vignette(img, mode="left_wide")

        img = draw_pill(img, (68, 68, 505, 108), (242, 158, 18, 245), radius=20)
        d = ImageDraw.Draw(img)
        d.text((90, 75), "FITS E32R35T / E32N35T • B0D93MBWC2", font=load_font(20, "black"), fill=(18, 20, 24, 255))

        img = draw_shadow_text(img, (64, 124), "3.5\" ESP32 TOUCH CYD", load_font(62, "black"), fill=(255, 255, 255, 255))
        img = draw_shadow_text(img, (64, 200), "SNAP-FIT CASE", load_font(70, "black"), fill=(255, 190, 45, 255))
        img = draw_shadow_text(
            img, (68, 286),
            "Modern 2-Piece Enclosure  •  Debossed BoxerDawg3D Base",
            load_font(25, "semi"), fill=(215, 226, 238, 255)
        )

        img = draw_pill(img, (68, 355, 515, 418), (34, 185, 92, 245), radius=31)
        d = ImageDraw.Draw(img)
        d.text((98, 370), "EASY TO PRINT • NO SUPPORTS", font=load_font(23, "black"), fill=(255, 255, 255, 255))

        img = draw_callout_pointer(
            img,
            label_pos=(68, 505),
            elbow_pos=(560, 505),
            target_pos=(835, 365),
            title="Face-Down Top Bezel",
            subtitle="45° touch chamfer • Full 55.5mm LCD clearance",
            accent=(110, 225, 255, 255),
        )
        img = draw_callout_pointer(
            img,
            label_pos=(68, 640),
            elbow_pos=(580, 640),
            target_pos=(800, 470),
            title="Centered USB-C & M3 Lock",
            subtitle="Wide USB-C recess • 4x M3 pins • MicroSD & JST slots",
            accent=(255, 200, 75, 255),
        )
        img = draw_callout_pointer(
            img,
            label_pos=(68, 775),
            elbow_pos=(660, 775),
            target_pos=(1180, 725),
            title="Debossed BoxerDawg3D Base",
            subtitle="Recessed bottom badge • Cooling vents • BOOT/RST",
            accent=(125, 245, 165, 255),
        )

        img = draw_pill(img, (68, 865, 675, 1005), (20, 25, 32, 205), outline_rgba=(255, 255, 255, 45), outline_width=1, radius=22)
        d = ImageDraw.Draw(img)
        d.text((92, 882), "1-PLATE BAMBU STUDIO 3MF  •  SINGLE OR 2-COLOR", font=load_font(19, "tech"), fill=(255, 195, 65, 255))
        d.ellipse((92, 918, 118, 944), fill=(36, 42, 52, 255), outline=(255, 255, 255, 210), width=2)
        d.text((130, 918), "Part 1: Base Shell", font=load_font(19, "semi"), fill=(240, 244, 248, 255))
        d.ellipse((330, 918, 356, 944), fill=(68, 80, 98, 255), outline=(255, 255, 255, 210), width=2)
        d.text((368, 918), "Part 2: Top Bezel", font=load_font(19, "semi"), fill=(240, 244, 248, 255))
        d.text((92, 962), "0.20mm Layer  •  3 Walls  •  15% Gyroid  •  Fits All Printers (A1 Mini)", font=load_font(18, "semi"), fill=(180, 195, 210, 255))

        out_rev = os.path.join(REV_DIR, "MakerWorld_Banner_16x9.jpg")
        img.convert("RGB").save(out_rev, quality=95)
        shutil.copy2(out_rev, os.path.join(MODEL_ROOT_DIR, "MakerWorld_Banner_16x9.jpg"))
        if os.path.exists(ARTIFACT_DIR):
            shutil.copy2(out_rev, os.path.join(ARTIFACT_DIR, "MakerWorld_Banner_16x9.jpg"))
        print(f"  Saved 16:9 Banner: {out_rev}")


def main():
    plate1 = {
        "plate_idx": 1,
        "plate_name": "Plate 1 - ESP32 3.5 Touch CYD Case (Base + Top Bezel)",
        "assembly_name": "ESP32_35_Touch_Case_rev2",
        "plate_center": (128.0, 128.0),
        "parts": [
            {
                "name": "1_ESP32_35_Case_Base_rev2",
                "stl": "1_ESP32_35_Case_Base_rev2.stl",
                "offset_xyz": (0.0, -36.0, 0.0),
                "extruder": 1,
            },
            {
                "name": "2_ESP32_35_Case_Top_rev2",
                "stl": "2_ESP32_35_Case_Top_rev2.stl",
                "offset_xyz": (0.0, 36.0, 0.0),
                "extruder": 2,
            },
        ],
    }

    out_3mf = os.path.join(REV_DIR, "ESP32_35_Touch_Case_rev2.3mf")
    build_multiplate_3mf(
        out_3mf,
        [plate1],
        title="3.5 ESP32 Touch CYD Snap-Fit Case (E32R35T)",
        description="2-Piece Support-Free Snap-Fit Enclosure for Hosyond 3.5-Inch ESP32-32E Touch Display (E32R35T / B0D93MBWC2) with Centered USB-C and Debossed BOXERDAWG3D Base"
    )
    shutil.copy2(out_3mf, os.path.join(MODEL_ROOT_DIR, "ESP32_35_Touch_Case_rev2.3mf"))

    make_covers()


if __name__ == "__main__":
    main()
