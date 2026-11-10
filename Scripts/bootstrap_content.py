"""Create Orbit Breakout's editable mission using the installed Unreal Editor.

Run in this project's Unreal Editor Python environment. Existing maps are checked
and preserved; --validate checks an existing mission without regeneration. Set
ORBIT_REPAIR_GENERATED=1 or --repair explicitly to back up the existing map, then
replace only tagged generator-owned actors. Untagged actors/materials are preserved.
--repair-env-materials explicitly backs up and refreshes the generated alloy
master; on its own it does not regenerate or modify any map actors.
The full editor stays alive asynchronously while navigation settles and is verified.
"""
import hashlib
import json
import math
import os
import random
import shutil
import sys
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path

import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
MAP = "/Game/OrbitBreakout/Maps/L_EmergencyDeparture"
BASE = "/Game/OrbitBreakout"
TAG = "OB_GENERATED"
REPAIR = os.environ.get("ORBIT_REPAIR_GENERATED") == "1" or "--repair" in sys.argv
VALIDATE_ONLY = "--validate" in sys.argv
REPAIR_ENV_MATERIALS = "--repair-env-materials" in sys.argv
NAV_TIMEOUT_SECONDS = 180.0
_NAV_STATE = None
ASSETS = u.AssetToolsHelpers.get_asset_tools()
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)
LEVELS = u.get_editor_subsystem(u.LevelEditorSubsystem)
MATS = {}
COUNTS = {}
ENV_MATERIALS = BASE + "/Materials/EnvironmentV3"

ZONES = [
    (1, "HOLDING CELLS", -800, 800, 600, 450),
    (2, "SECURITY CORRIDOR", 1400, 4600, 450, 500),
    (3, "CARGO PROCESSING", 5000, 8600, 1400, 800),
    (4, "SECURITY JUNCTION", 9000, 11000, 900, 550),
    (5, "REACTOR CHAMBER", 11900, 15100, 900, 750),
    (6, "ESCAPE-POD BAY", 15600, 19200, 1300, 850),
]
START = (-600, 0, 100)
CARGO_CP = (5070, 0, 100)
REACTOR_CP = (12000, 0, 100)
ENEMIES = [
    (1, 1, 2350, 80, False), (1, 2, 3200, -150, False), (1, 3, 4100, 140, False),
    (2, 1, 6200, -700, False), (2, 2, 7100, 650, False), (2, 3, 7900, -550, False),
    (3, 1, 8120, 900, False), (3, 2, 8250, 1130, False),
    (4, 1, 10000, 600, False), (4, 2, 10550, 650, False), (4, 3, 10600, -200, False),
    # The reactor belongs to the Bull alone. Stable IDs for all survivors stay
    # unchanged so encounter bookkeeping remains compatible with checkpoints.
    (5, 3, 14400, 0, True),
    (6, 1, 16500, -650, False), (6, 2, 17100, 650, False),
    (6, 3, 17900, -700, False), (6, 4, 18300, 600, False),
]


def vec(value):
    return u.Vector(*value)


def cls(name):
    result = u.load_class(None, "/Script/OrbitBreakout." + name)
    if not result:
        raise RuntimeError("Missing compiled native class: " + name)
    return result


def tagged(actor, name, folder="Architecture"):
    actor.set_actor_label(name)
    actor.set_editor_property("tags", [u.Name(TAG)])
    actor.set_folder_path(u.Name("OB_Generated/" + folder))
    COUNTS[folder] = COUNTS.get(folder, 0) + 1
    return actor


def spawn(kind, name, pos, yaw=0, folder="Gameplay"):
    actor = ACTORS.spawn_actor_from_class(kind, vec(pos), u.Rotator(pitch=0, yaw=yaw, roll=0))
    if not actor:
        raise RuntimeError("Actor creation failed: " + name)
    return tagged(actor, name, folder)


def save_with_nanite_usage(mat):
    """Persist the required permutation without changing graphs or instances."""
    edit = u.MaterialEditingLibrary
    usage = u.MaterialUsage.MATUSAGE_NANITE
    changed = not edit.has_material_usage(mat, usage)
    edit.set_base_material_usage(mat, usage, True)
    if changed:
        edit.recompile_material(mat)
    if not edit.has_material_usage(mat, usage):
        raise RuntimeError("Nanite material usage was not enabled: " + mat.get_path_name())
    # Editor auto-discovery can leave a flag enabled in memory but unsaved.
    # Force persistence even when the material is not marked dirty.
    if not u.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError("Could not save Nanite material usage: " + mat.get_path_name())


def material(name, color, emissive=False, glass=False):
    path = BASE + "/Materials/" + name
    old = u.load_asset(path)
    if old:
        if not glass:
            save_with_nanite_usage(old)
        return old
    mat = ASSETS.create_asset(name, BASE + "/Materials", u.Material, u.MaterialFactoryNew())
    edit = u.MaterialEditingLibrary
    c = edit.create_material_expression(mat, u.MaterialExpressionVectorParameter, -420, 0)
    c.set_editor_property("parameter_name", "Color")
    c.set_editor_property("default_value", u.LinearColor(*color, 1))
    edit.connect_material_property(c, "", u.MaterialProperty.MP_BASE_COLOR)
    rough = edit.create_material_expression(mat, u.MaterialExpressionScalarParameter, -420, 180)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.52)
    edit.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
    metal = edit.create_material_expression(mat, u.MaterialExpressionConstant, -420, 280)
    metal.set_editor_property("r", 0.32)
    edit.connect_material_property(metal, "", u.MaterialProperty.MP_METALLIC)
    if emissive:
        power = edit.create_material_expression(mat, u.MaterialExpressionScalarParameter, -420, -160)
        power.set_editor_property("parameter_name", "EmissiveStrength")
        power.set_editor_property("default_value", 2.0)
        mult = edit.create_material_expression(mat, u.MaterialExpressionMultiply, -170, -100)
        edit.connect_material_expressions(c, "", mult, "A")
        edit.connect_material_expressions(power, "", mult, "B")
        edit.connect_material_property(mult, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    if glass:
        mat.set_editor_property("blend_mode", u.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("two_sided", True)
        opacity = edit.create_material_expression(mat, u.MaterialExpressionConstant, -420, 380)
        opacity.set_editor_property("r", 0.055)
        edit.connect_material_property(opacity, "", u.MaterialProperty.MP_OPACITY)
    edit.recompile_material(mat)
    if glass:
        u.EditorAssetLibrary.save_loaded_asset(mat)
    else:
        save_with_nanite_usage(mat)
    return mat


def instance(name, parent, color, glow=0):
    path = BASE + "/Materials/" + name
    old = u.load_asset(path)
    if old:
        return old
    mat = ASSETS.create_asset(name, BASE + "/Materials", u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    u.MaterialEditingLibrary.set_material_instance_parent(mat, parent)
    u.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mat, "Color", u.LinearColor(*color, 1))
    if glow:
        u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mat, "EmissiveStrength", glow)
    u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def backup_environment_material(mat):
    """Preserve the current generated master before an explicit graph refresh."""
    if not u.EditorAssetLibrary.save_loaded_asset(mat):
        raise RuntimeError("Cannot save environment material before its backup")
    asset_name = mat.get_path_name().split(".")[0]
    relative = Path("Content") / (asset_name.removeprefix("/Game/") + ".uasset")
    source = ROOT / relative
    if not source.is_file():
        raise RuntimeError("Cannot find saved environment material for backup: " + str(source))
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    destination = ROOT / "Saved" / "Backups" / ("EnvironmentMaterial_" + stamp)
    records = []
    for suffix in (".uasset", ".uexp", ".ubulk"):
        item = source.with_suffix(suffix)
        if not item.is_file():
            continue
        target = destination / item.relative_to(ROOT)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(item, target)
        digest = hashlib.sha256(item.read_bytes()).hexdigest()
        if digest != hashlib.sha256(target.read_bytes()).hexdigest():
            raise RuntimeError("Environment material backup verification failed")
        records.append({"path": item.relative_to(ROOT).as_posix(), "sha256": digest})
    (destination / "BACKUP_MANIFEST.json").write_text(json.dumps({"asset": asset_name, "files": records}, indent=2) + "\n")
    u.log("ORBIT_ENVIRONMENT_BACKUP_OK " + str(destination))


def environment_surface():
    """Versioned alloy master; rewriting requires explicit opt-in and backup."""
    name = "M_Alloy_V3"
    path = ENV_MATERIALS + "/" + name
    old = u.load_asset(path)
    complete_tag = "OrbitEnvironmentGraphComplete"
    complete = old and u.EditorAssetLibrary.get_metadata_tag(old, complete_tag) == "1"
    if old and complete and not REPAIR_ENV_MATERIALS:
        return old
    if old and not REPAIR_ENV_MATERIALS:
        raise RuntimeError("Incomplete EnvironmentV3 material found; rerun with --repair-env-materials to rebuild only this generated graph")
    mat = old or ASSETS.create_asset(name, ENV_MATERIALS, u.Material, u.MaterialFactoryNew())
    edit = u.MaterialEditingLibrary
    if old:
        backup_environment_material(mat)
        u.EditorAssetLibrary.set_metadata_tag(mat, complete_tag, "0")
        edit.delete_all_material_expressions(mat)

    def node(kind, x, y):
        return edit.create_material_expression(mat, kind, x, y)

    def scalar(name, value, x, y):
        result = node(u.MaterialExpressionScalarParameter, x, y)
        result.set_editor_property("parameter_name", name)
        result.set_editor_property("default_value", value)
        return result

    def link(source, output, target, pin):
        if not edit.connect_material_expressions(source, output, target, pin):
            raise RuntimeError("Environment material connection failed: " + pin)

    color = node(u.MaterialExpressionVectorParameter, -900, -350)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", u.LinearColor(.08, .105, .12, 1))
    roughness = scalar("Roughness", .48, -600, 360)
    metallic = scalar("Metallic", .7, -600, 560)
    # A small, installed engine texture supplies continuous surface variation.
    # World-space projection keeps its scale independent of long modular cubes.
    noise_texture = u.load_asset("/Engine/MaterialTemplates/Textures/T_Noise01")
    if not noise_texture:
        raise RuntimeError("Installed engine surface-noise texture is missing")
    world = node(u.MaterialExpressionWorldPosition, -1450, 0)
    coordinates = []
    for n, axes in enumerate(((.08, .016, .048), (.016, .08, .112))):
        axis = node(u.MaterialExpressionConstant3Vector, -1450, 180 + n * 160)
        axis.set_editor_property("constant", u.LinearColor(*axes, 1))
        dot = node(u.MaterialExpressionDotProduct, -1180, n * 180)
        link(world, "XYZ", dot, "A")
        link(axis, "", dot, "B")
        coordinates.append(dot)
    uv = node(u.MaterialExpressionAppendVector, -980, 80)
    link(coordinates[0], "", uv, "A")
    link(coordinates[1], "", uv, "B")
    noise = node(u.MaterialExpressionTextureSample, -770, 80)
    noise.set_editor_property("texture", noise_texture)
    # Setting Texture triggers Unreal's sampler-type update for the actual asset.
    # MaterialEditingLibrary uses shortened graph-pin names: UE 5.8 exposes
    # TextureSample.Coordinates as "UVs", unlike the C++ member name.
    input_names = list(edit.get_material_expression_input_names(noise))
    uv_pin = "UVs" if "UVs" in input_names else "Coordinates"
    link(uv, "", noise, uv_pin)
    variation = node(u.MaterialExpressionMultiply, -510, 30)
    variation.set_editor_property("const_b", .18)
    link(noise, "R", variation, "A")
    tint = node(u.MaterialExpressionAdd, -290, -80)
    tint.set_editor_property("const_b", .82)
    link(variation, "", tint, "A")
    colored = node(u.MaterialExpressionMultiply, -70, -180)
    link(color, "", colored, "A")
    link(tint, "", colored, "B")
    rough = node(u.MaterialExpressionAdd, -290, 260)
    rough_variation = node(u.MaterialExpressionMultiply, -510, 240)
    rough_variation.set_editor_property("const_b", .12)
    link(noise, "R", rough_variation, "A")
    link(roughness, "", rough, "A")
    link(rough_variation, "", rough, "B")
    edit.connect_material_property(colored, "", u.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(rough, "", u.MaterialProperty.MP_ROUGHNESS)
    edit.connect_material_property(metallic, "", u.MaterialProperty.MP_METALLIC)
    edit.recompile_material(mat)
    u.EditorAssetLibrary.set_metadata_tag(mat, complete_tag, "1")
    save_with_nanite_usage(mat)
    return mat


def environment_instance(key, parent, color, roughness=.45, metallic=.7, glow=None):
    name = "MI_" + key.title() + "_V3"
    path = ENV_MATERIALS + "/" + name
    old = u.load_asset(path)
    if old:
        return old
    mat = ASSETS.create_asset(name, ENV_MATERIALS, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    edit = u.MaterialEditingLibrary
    edit.set_material_instance_parent(mat, parent)
    edit.set_material_instance_vector_parameter_value(mat, "Color", u.LinearColor(*color, 1))
    edit.set_material_instance_scalar_parameter_value(mat, "Roughness", roughness)
    if glow is None:
        edit.set_material_instance_scalar_parameter_value(mat, "Metallic", metallic)
    if glow is not None:
        edit.set_material_instance_scalar_parameter_value(mat, "EmissiveStrength", glow)
    u.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def create_materials():
    # Retain native-character/weapon material paths. Environment V3 is a new
    # asset family, so applying the art pass never silently edits old materials.
    material("M_Metal", (.16, .22, .28))
    glow = material("M_Emissive", (.05, .8, .7), True)
    metal = environment_surface()
    MATS["glass"] = material("M_Window_V3", (.022, .075, .10), glass=True)
    palette = {
        "floor": ((.035, .044, .055), .42, .68),
        "wall": ((.105, .135, .15), .36, .66),
        "dark": ((.009, .014, .020), .36, .78),
        "white": ((.36, .39, .38), .27, .68),
        "crate": ((.055, .095, .105), .51, .45),
        "cargo": ((.24, .15, .055), .55, .3),
        "orange": ((.55, .16, .025), .48, .25),
        "blue": ((.025, .065, .095), .38, .65),
        "steel": ((.22, .26, .28), .22, .88),
        "brass": ((.34, .22, .075), .3, .82),
        "rubber": ((.012, .015, .018), .76, .02),
        "screen": ((.008, .02, .025), .18, .25),
    }
    for key, (color, rough, metallic) in palette.items():
        MATS[key] = environment_instance(key, metal, color, rough, metallic)
    for key, color, strength in [
        ("teal", (.015, .42, .60), 1.2), ("amber", (.95, .23, .025), 1.45),
        ("light", (.58, .72, .80), 1.1), ("red", (.72, .025, .012), 1.3),
        ("planet", (.018, .075, .14), .55), ("star", (.55, .7, .9), 2.2),
    ]:
        MATS[key] = environment_instance(key, glow, color, .35, .4, strength)


def mesh(name, pos, size, mat="wall", shape="Cube", collision=True, rotation=None, folder="Architecture"):
    actor = spawn(u.StaticMeshActor, name, pos, folder=folder)
    component = actor.static_mesh_component
    component.set_static_mesh(u.load_asset("/Engine/BasicShapes/" + shape))
    component.set_material(0, MATS[mat])
    component.set_mobility(u.ComponentMobility.STATIC)
    component.set_collision_profile_name("BlockAll" if collision else "NoCollision")
    component.set_editor_property("cast_shadow", collision)
    actor.set_actor_scale3d(vec(tuple(v / 100 for v in size)))
    if rotation:
        actor.set_actor_rotation(u.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]), False)
    return actor


def label(name, text, pos, yaw=180, size=30, color=(170, 225, 235), folder="Signage"):
    actor = spawn(u.TextRenderActor, name, pos, yaw, folder)
    comp = actor.text_render
    comp.set_text(text)
    comp.set_world_size(size)
    comp.set_text_render_color(u.Color(r=color[0], g=color[1], b=color[2], a=255))
    comp.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
    comp.set_vertical_alignment(u.VerticalTextAligment.EVRTA_TEXT_CENTER)
    return actor


def gate_sign(name, text, x, z=370, yaw=180, size=28, color=(170, 225, 235)):
    """Readable, nonblocking sign fixed to a bulkhead's flat face."""
    # Text faces its local +X. Keep the backing behind that face, avoiding
    # coplanar flicker without adding geometry to the walkable route.
    facing = 1 if yaw == 0 else -1
    mesh(name + "Backing", (x - facing * 7, 0, z), (8, 620, 70),
         "dark", collision=False, folder="Signage")
    mesh(name + "Underline", (x - facing * 2, 0, z - 29), (3, 570, 3),
         "teal", collision=False, folder="Signage")
    return label(name, text, (x, 0, z), yaw=yaw, size=size, color=color)


def fill(name, pos, radius=1300, intensity=22000, color=(170, 215, 255), shadows=False):
    actor = spawn(u.PointLight, name, pos, folder="Lighting")
    light = actor.point_light_component
    light.set_mobility(u.ComponentMobility.MOVABLE)
    light.set_editor_property("intensity_units", u.LightUnits.LUMENS)
    light.set_intensity(intensity)
    light.set_light_color(u.LinearColor(color[0] / 255, color[1] / 255, color[2] / 255, 1))
    light.set_attenuation_radius(radius)
    light.set_cast_shadows(shadows)
    light.set_source_radius(28.0)
    light.set_soft_source_radius(36.0)
    light.set_editor_property("use_inverse_squared_falloff", True)
    return actor


def doorway(x, halfwidth, height, name, opening=300, opening_height=320):
    side = halfwidth - opening / 2
    for sign in (-1, 1):
        mesh(name + "Side" + str(sign), (x, sign * (opening / 2 + side / 2), height / 2), (60, side, height), "dark")
        mesh(name + "Jamb" + str(sign), (x - 36, sign * (opening / 2 + 10), opening_height / 2), (20, 20, opening_height), "white")
        mesh(name + "Status" + str(sign), (x - 48, sign * (opening / 2 + 22), opening_height / 2), (8, 8, opening_height - 40), "teal", collision=False)
    mesh(name + "Lintel", (x, 0, opening_height + (height - opening_height) / 2), (60, opening, height - opening_height), "dark")
    mesh(name + "Header", (x - 42, 0, opening_height + 17), (18, opening + 44, 25), "white")


def door(index, x, height=500):
    actor = spawn(cls("OBDoor"), "D%02d_SlidingGate" % index, (x, 0, 160))
    actor.set_editor_property("door_id", index)
    actor.set_editor_property("panel_size", vec((60, 300, 320)))
    actor.set_editor_property("open_distance", 340.0)
    return actor


def shell(zone):
    zid, title, left, right, halfwidth, height = zone
    width = right - left
    prefix = "Z%02d_" % zid
    center = (left + right) / 2
    # Walkable top surface is exactly z=0. Ceilings are solid throughout.
    mesh(prefix + "Deck", (center, 0, -25), (width, halfwidth * 2, 50), "floor")
    mesh(prefix + "Ceiling", (center, 0, height + 20), (width, halfwidth * 2, 40), "dark")
    for sign in (-1, 1):
        window = zid in (3, 6) and sign == -1
        if window:
            mesh(prefix + "WindowSill", (center, -halfwidth - 15, 80), (width, 30, 160), "dark")
            mesh(prefix + "WindowHeader", (center, -halfwidth - 15, height - 65), (width, 30, 130), "dark")
            mesh(prefix + "ObservationGlass", (center, -halfwidth - 15, (height + 30) / 2), (width, 22, height - 290), "glass")
        else:
            mesh(prefix + "Hull" + str(sign), (center, sign * (halfwidth + 20), height / 2), (width, 40, height), "wall")
        for n, x in enumerate(range(int(left + 170), int(right), 420)):
            mesh(prefix + "Rib_%d_%d" % (sign, n), (x, sign * (halfwidth - 10), height / 2), (36, 50, height), "dark")
            mesh(prefix + "Foot_%d_%d" % (sign, n), (x, sign * (halfwidth - 30), 42), (58, 70, 84), "white")
            if not window:
                # Clamp the final facade panel to its room. Earlier panels
                # overhung into connectors; no new dressing changes collision.
                panel_width = min(320, max(0, right - 55 - (x + 30)))
                if panel_width > 35:
                    mesh(prefix + "Panel_%d_%d" % (sign, n), (x + 30 + panel_width / 2, sign * (halfwidth - 5), height * .55), (panel_width, 12, height * .47), "blue" if zid == 1 else "wall")
    # Deck seams and route edges are physical, readable structure.
    for x in range(int(left + 350), int(right), 400):
        mesh(prefix + "DeckJoint_%d" % x, (x, 0, 1), (4, halfwidth * 2 - 35, 2), "dark", collision=False)
    for sign in (-1, 1):
        mesh(prefix + "RouteStrip" + str(sign), (center, sign * 210, 2.5), (width - 70, 7, 4), "teal", collision=False)
    doorway(left, halfwidth, height, prefix + "Entry")
    doorway(right, halfwidth, height, prefix + "Exit", opening=900 if zid == 6 else 300, opening_height=700 if zid == 6 else 320)
    gate_sign(prefix + "Destination", title, left + 42, yaw=0,
              size=28 if len(title) > 17 else 32)
    if zid > 1:
        # The interior title faces back into its room. A second face tells
        # approaching players which room they are entering before crossing.
        gate_sign(prefix + "Arrival", "%02d  /  %s" % (zid, title), left - 42,
                  size=25)
    destination = ("%02d  /  %s" % (zid + 1, ZONES[zid][1])) if zid < 6 else "EVACUATION  /  POD 07"
    gate_sign(prefix + "ExitSign", destination, right - 45,
              z=370 if zid < 6 else 750, size=25,
              color=(170, 225, 235) if zid < 6 else (255, 190, 95))
    # Pools of practical light, with one shadow-casting key per room. Warm
    # machinery and cool route light retain readable silhouettes without
    # flattening the entire hull into an evenly illuminated white room.
    count = max(2, math.ceil(width / 1100))
    light_color = {1: (145, 198, 225), 2: (175, 207, 225), 3: (230, 195, 148),
                   4: (150, 205, 213), 5: (255, 166, 85), 6: (150, 187, 232)}[zid]
    for n in range(count):
        x = left + width * (n + .5) / count
        mesh(prefix + "LuminaireHousing_%d" % n, (x, 0, height - 30), (230, 115, 32), "dark", collision=False, folder="Fixtures")
        for side in (-1, 1):
            mesh(prefix + "Overhead_%d_%d" % (n, side), (x, side * 38, height - 49), (180, 14, 6), "light", collision=False, folder="Fixtures")
        fill(prefix + "Fill_%d" % n, (x, 0, height - 100), radius=max(1050, halfwidth * 1.15), intensity=34000 if zid in (3, 6) else 17500, color=light_color, shadows=n == count // 2)
    industrial_dressing(zone)


def connector(left, right, index):
    center = (left + right) / 2
    length = right - left
    mesh("Link%d_Deck" % index, (center, 0, -25), (length, 600, 50), "floor")
    mesh("Link%d_Ceiling" % index, (center, 0, 420), (length, 640, 40), "dark")
    for sign in (-1, 1):
        mesh("Link%d_Wall%d" % (index, sign), (center, sign * 320, 200), (length, 40, 400), "wall")
        mesh("Link%d_Route%d" % (index, sign), (center, sign * 210, 3), (length, 7, 4), "teal", collision=False)
    for sign in (-1, 1):
        mesh("Link%d_ServicePipe%d" % (index, sign), (center, sign * 270, 335), (16, 16, length - 50), "steel", "Cylinder", collision=False, rotation=(90, 0, 0), folder="Dressing")
    mesh("Link%d_LightHousing" % index, (center, 0, 390), (95, 190, 18), "dark", collision=False, folder="Dressing")
    mesh("Link%d_Practical" % index, (center, 0, 376), (72, 150, 5), "light", collision=False, folder="Dressing")
    fill("Link%d_Fill" % index, (center, 0, 335), 650, 11500)


def crate(name, x, y, sx=280, sy=230, height=240, color="crate"):
    mesh(name + "Body", (x, y, height / 2), (sx, sy, height), color, folder="Cover")
    for sign in (-1, 1):
        mesh(name + "Band" + str(sign), (x + sign * (sx / 2 - 35), y, height + 3), (18, sy + 4, 9), "white", collision=False, folder="Cover")
    mesh(name + "Code", (x - sx / 2 - 2, y, height * .62), (4, sy * .7, 16), "amber", collision=False, folder="Cover")
    # Machined corner rails and recessed transport fittings give the same
    # simple gameplay cover volume an industrial, human-scale construction.
    for sign in (-1, 1):
        for edge in (-1, 1):
            mesh(name + "Corner_%d_%d" % (sign, edge), (x + sign * (sx / 2 - 6), y + edge * (sy / 2 + 2), height / 2), (17, 13, height - 8), "steel", collision=False, folder="Dressing")
        mesh(name + "BaseRail%d" % sign, (x, y + sign * (sy / 2 + 3), 18), (sx - 18, 10, 23), "dark", collision=False, folder="Dressing")
        mesh(name + "Inset%d" % sign, (x, y + sign * (sy / 2 + 6), height * .48), (sx * .7, 6, height * .51), "blue", collision=False, folder="Dressing")
        mesh(name + "Latch%d" % sign, (x - sx / 2 - 5, y + sign * sy * .27, height * .4), (8, 27, 46), "steel", collision=False, folder="Dressing")


def detail(name, pos, size, mat="steel", shape="Cube", rotation=None):
    """Presentation only: cannot alter cover, pawn movement or projectile hits."""
    return mesh(name, pos, size, mat, shape, collision=False, rotation=rotation, folder="Dressing")


def wall_console(name, x, y, sign, title, color="teal", width=210):
    """Thin wall-mounted instrument panel; front faces into the room."""
    detail(name + "Case", (x, y, 220), (width + 24, 28, 180), "dark")
    detail(name + "Screen", (x, y - sign * 17, 235), (width, 5, 108), "screen")
    for side in (-1, 1):
        detail(name + "Frame" + str(side), (x + side * (width / 2 + 4), y - sign * 19, 235), (5, 6, 120), "steel")
    detail(name + "Header", (x, y - sign * 21, 280), (width - 20, 3, 3), color)
    for n, amount in enumerate((.75, .43, .62, .3)):
        length = width * amount
        detail(name + "Readout%d" % n, (x - width * .4 + length / 2, y - sign * 21, 263 - n * 17), (length, 3, 2.5), color)
    detail(name + "KeyDeck", (x, y - sign * 20, 155), (width - 10, 42, 12), "steel", rotation=(0, 0, -sign * 10))
    for n in range(5):
        detail(name + "Key%d" % n, (x - width * .32 + n * width * .16, y - sign * 45, 160), (12, 8, 3), "amber" if n == 4 else "dark")
    label(name + "Label", title, (x, y - sign * 34, 326), yaw=-90 if sign > 0 else 90, size=16, color=(125, 180, 192))


def wall_vent(name, x, y, sign, width=220, z=170):
    detail(name + "Recess", (x, y, z), (width, 16, 100), "rubber")
    for n in range(6):
        detail(name + "Louvre%d" % n, (x, y - sign * 12, z - 40 + n * 16), (width - 10, 18, 7), "steel", rotation=(0, 0, sign * 25))
    for side in (-1, 1):
        detail(name + "Trim%d" % side, (x + side * (width / 2 + 4), y - sign * 10, z), (7, 20, 110), "dark")


def industrial_dressing(zone):
    zid, title, left, right, halfwidth, height = zone
    center = (left + right) / 2
    width = right - left
    prefix = "V2_Z%02d_" % zid
    # Long utility lines and brackets make the high spaces feel serviced rather
    # than empty. Everything is safely overhead and has collision disabled.
    for sign in (-1, 1):
        y = sign * (halfwidth - 110)
        for n, z in enumerate((height - 105, height - 150)):
            detail(prefix + "Conduit_%d_%d" % (sign, n), (center, y, z), (20 if n else 30, 20 if n else 30, width - 180), "steel" if n else "dark", "Cylinder", rotation=(90, 0, 0))
        for n, x in enumerate(range(int(left + 250), int(right - 100), 850)):
            detail(prefix + "PipeMount_%d_%d" % (sign, n), (x, y, height - 128), (16, 68, 102), "dark")
            detail(prefix + "PipeMark_%d_%d" % (sign, n), (x + 25, y, height - 105), (33, 33, 18), "orange" if zid in (3, 5) else "blue", "Cylinder", rotation=(90, 0, 0))
    for n, x in enumerate(range(int(left + 300), int(right - 160), 800)):
        detail(prefix + "RoofBeam%d" % n, (x, 0, height - 70), (52, halfwidth * 2 - 130, 68), "dark")
        detail(prefix + "RoofBeamEdge%d" % n, (x - 29, 0, height - 72), (4, halfwidth * 2 - 180, 6), "steel")
        for sign in (-1, 1):
            detail(prefix + "KneeBrace_%d_%d" % (n, sign), (x, sign * (halfwidth - 120), height - 130), (32, 230, 32), "steel", rotation=(0, 0, sign * 35))
    # Inset deck modules, contrasting seams and service hatch handles are
    # physically flush. The central teal route remains a useful visual guide.
    for n, x in enumerate(range(int(left + 350), int(right - 250), 650)):
        for sign in (-1, 1):
            y = sign * min(halfwidth * .58, 760)
            sy = min(halfwidth - 250, 660)
            detail(prefix + "DeckPanel_%d_%d" % (n, sign), (x, y, .8), (590, sy, 1), "blue" if n % 3 == 0 else "floor")
            detail(prefix + "DeckSeam_%d_%d" % (n, sign), (x - 290, y, 1.7), (4, sy, 1), "steel")
            detail(prefix + "HatchHandle_%d_%d" % (n, sign), (x - 225, y, 2.5), (7, 32, 2), "steel")
    # Only the occupied wall gets instrument banks beside windowed cargo/bay.
    for sign in (-1, 1):
        if zid in (3, 6) and sign < 0:
            continue
        y = sign * (halfwidth - 30)
        for n, t in enumerate((.25, .75)):
            x = left + width * t
            wall_vent(prefix + "Vent_%d_%d" % (sign, n), x, y, sign, width=180 if zid == 2 else 240, z=120)
            detail(prefix + "ServiceStripe_%d_%d" % (sign, n), (x, y - sign * 13, 200), (210, 4, 8), "orange" if zid == 5 else "steel")
        if zid > 1:
            wall_console(prefix + "Status%d" % sign, center, sign * (halfwidth + 2), sign,
                         {2: "PATROL / ACCESS", 3: "FREIGHT / MANIFEST", 4: "TRANSIT / SYSTEMS", 5: "CONTAINMENT / 04", 6: "EVAC / POD STATUS"}[zid],
                         color="amber" if zid == 5 else "teal")
    # A small warm source provides a complementary edge near each far wall,
    # while the core route remains readable under cool practical lamps.
    x = right - min(500, width * .3)
    y = halfwidth - 90
    detail(prefix + "BeaconCase", (x, y, 310), (55, 36, 86), "dark")
    detail(prefix + "Beacon", (x, y - 22, 310), (18, 8, 58), "amber")
    fill(prefix + "BeaconSpill", (x, y - 65, 300), 600, 2100, (255, 130, 50))
    # Large stencilled zone numbers anchor the space at a human scale.
    label(prefix + "ZoneNumber", "%02d" % zid, (left + width * .21, halfwidth - 45, height * .58), yaw=-90, size=96, color=(84, 113, 123))
    label(prefix + "DeckCode", "HELIOS ARK  /  DECK C", (left + width * .21, halfwidth - 47, height * .58 - 80), yaw=-90, size=16, color=(125, 151, 158))


def machinery_details():
    """Room-specific silhouettes with no additions to gameplay collision."""
    # Detention wall: light spill, identification strip and a recessed terminal.
    wall_console("V2_CellIntake", -590, -262, -1, "INTAKE / CELL 014", width=150)
    for x in (-700, -480):
        detail("V2_CellCeilingSlat%d" % x, (x, 0, 418), (70, 475, 14), "steel")
    fill("V2_CellEdgeLight", (-650, -210, 240), 440, 2500, (65, 180, 215))
    # Cargo overhead rails and a suspended hoist; floor gameplay is unchanged.
    for y in (-650, 650):
        detail("V2_CraneRail%d" % y, (6800, y, 725), (3000, 80, 75), "steel")
        detail("V2_CraneRailStripe%d" % y, (6800, y - 44, 725), (2950, 5, 12), "orange")
    detail("V2_CraneTrolley", (7050, 0, 700), (310, 1420, 130), "cargo")
    for y in (-230, 230):
        detail("V2_HoistCable%d" % y, (7050, y, 555), (6, 6, 180), "dark", "Cylinder")
    detail("V2_HoistBlock", (7050, 0, 460), (170, 550, 70), "steel")
    label("V2_CargoLoad", "CARGO  /  MAX 8T", (7049, -720, 700), yaw=90, size=22, color=(225, 181, 100))
    # Transit control has dense server cabinets against the perimeter wall.
    for n, x in enumerate((9600, 9950, 10300)):
        detail("V2_ServerRack%d" % n, (x, 935, 195), (280, 95, 390), "dark")
        for row in range(6):
            detail("V2_ServerDrawer%d_%d" % (n, row), (x, 883, 60 + row * 52), (245, 8, 40), "steel")
            detail("V2_ServerLED%d_%d" % (n, row), (x - 90, 875, 60 + row * 52), (18, 4, 3), "teal" if row % 3 else "amber")
    # Reactor fins, segmented containment rings and routed power cabling.
    for angle in range(0, 360, 45):
        theta = math.radians(angle)
        x, y = 13500 + math.cos(theta) * 180, 640 + math.sin(theta) * 180
        detail("V2_ReactorFin%d" % angle, (x, y, 360), (22, 55, 410), "steel", rotation=(0, angle, 0))
    for z in (135, 305, 475):
        detail("V2_ReactorHoop%d" % z, (13500, 640, z), (405, 405, 18), "dark", "Cylinder")
    for y in (-780, 780):
        detail("V2_ReactorPowerBus%d" % y, (13600, y, 640), (45, 45, 2400), "steel", "Cylinder", rotation=(90, 0, 0))
        for x in (12500, 13500, 14500):
            detail("V2_ReactorCoupler%d_%d" % (x, y), (x, y, 640), (70, 70, 85), "dark", "Cylinder", rotation=(90, 0, 0))
    # Escape bay launch gantries and emergency light heads frame the pod.
    for x in (16100, 17200, 18300):
        detail("V2_BayTruss%d" % x, (x, 0, 760), (90, 2420, 100), "steel")
        for sign in (-1, 1):
            detail("V2_BayBrace%d_%d" % (x, sign), (x, sign * 1080, 625), (70, 370, 75), "dark", rotation=(0, 0, sign * 45))
            detail("V2_BayWarning%d_%d" % (x, sign), (x - 50, sign * 990, 720), (8, 75, 16), "amber")
    fill("V2_PodKey", (18250, -650, 520), 1050, 18000, (195, 223, 240), shadows=True)
    # Wall-mounted pressure canisters have 285 cm of floor clearance, keeping
    # decorative silhouettes out of the unchanged traversable floor plan.
    for index, (x, y) in enumerate(((5680, -1340), (7950, -1340), (15970, 1240), (18050, 1240))):
        detail("V2_PressureCanister%d" % index, (x, y, 365), (115, 115, 160), "blue", "Cylinder")
        for z in (300, 430):
            detail("V2_CanisterRim%d_%d" % (index, z), (x, y, z), (122, 122, 10), "steel", "Cylinder")
        detail("V2_CanisterBand%d" % index, (x, y, 380), (117, 117, 12), "orange", "Cylinder")


def interaction(kind, stable_id, zone, x, y, yaw=0):
    # Native interaction labels already face local -X, toward approaching
    # players. An extra 180-degree turn exposes their mirrored back faces.
    actor = spawn(cls("OBInteractable"), stable_id, (x, y, 0), yaw)
    actor.set_editor_property("kind", getattr(u.OBInteractionKind, kind))
    actor.set_editor_property("stable_id", u.Name(stable_id))
    actor.set_editor_property("zone", zone)
    return actor


def zone_trigger(zone, x, halfwidth, checkpoint=False):
    actor = spawn(cls("OBZoneTrigger"), ("CP" if checkpoint else "ENTER") + "_Z%02d" % zone, (x, 0, 150))
    actor.set_editor_property("zone", zone)
    actor.set_editor_property("trigger_extent", vec((80, halfwidth, 150)))
    actor.set_editor_property("checkpoint", checkpoint)
    # New custom channels inherit their default response in built-in profiles.
    # Persist this explicitly as well as the Trigger profile override in config.
    actor.get_component_by_class(u.BoxComponent).set_collision_response_to_channel(
        u.CollisionChannel.ECC_ENEMY_BOLT, u.CollisionResponseType.ECR_IGNORE)
    return actor


def cell_details():
    doorway(-280, 260, 450, "CellFront")
    door(0, -280)
    mesh("CellSouthPartition", (-540, -280, 225), (520, 40, 450), "wall")
    mesh("CellNorthPartition", (-540, 280, 225), (520, 40, 450), "wall")
    mesh("CellRear", (-790, 0, 225), (20, 520, 450), "dark")
    mesh("RookBench", (-625, 205, 40), (230, 75, 80), "dark", folder="Fixtures")
    mesh("RookBenchCushion", (-625, 205, 85), (220, 70, 12), "blue", folder="Fixtures")
    label("CellNumber", "DETENTION  /  014", (-745, 0, 280), yaw=0, size=24)
    for x in (50, 450):
        mesh("SealedCellFront_%d" % x, (x, 460, 190), (320, 20, 380), "dark")
        for dx in range(-140, 150, 56):
            mesh("CellBar_%d_%d" % (x, dx), (x + dx, 445, 190), (10, 16, 360), "white")
        label("SealedCellLabel_%d" % x, "SEALED", (x, 429, 320), yaw=-90, size=23)
    interaction("WEAPON", "I01_ARC9_Rack", 1, 350, -220)
    mesh("WeaponRackBack", (375, -335, 125), (160, 32, 250), "dark", folder="Fixtures")
    label("WeaponRackSign", "ARC-9  /  SECURITY ISSUE", (374, -313, 240), yaw=90, size=19)
    label("DetentionWelcome", "HELIOS ARK", (740, 0, 408), size=37)


def cargo_details():
    crate("CargoA", 6470, -140, 650, 370, 300)
    crate("CargoB", 7120, 260, 460, 380, 260, "cargo")
    crate("CargoC", 7630, -700, 280, 280, 130)
    crate("CargoD", 5820, 750, 320, 260, 245)
    # Accessible inspection platform at north side, continuous ramp below 25 degrees.
    mesh("CargoMezzanine", (7350, 1160, 135), (1450, 400, 30), "dark", folder="Fixtures")
    mesh("CargoRamp", (6430, 1160, 65), (600, 360, 30), "floor", rotation=(14.5, 0, 0), folder="Fixtures")
    for x in (6750, 7250, 7800):
        mesh("GantryPost_%d" % x, (x, 1360, 340), (35, 35, 680), "dark", folder="Fixtures")
        mesh("GantryRailPost_%d" % x, (x, 965, 205), (12, 12, 110), "white", folder="Fixtures")
    mesh("GantryRail", (7350, 965, 260), (1450, 12, 14), "white", folder="Fixtures")
    mesh("GantryTopBeam", (7350, 1360, 690), (1450, 100, 100), "dark", folder="Fixtures")
    mesh("CargoFreightDoor", (7200, 1374, 330), (700, 35, 600), "blue", folder="Fixtures")
    label("CargoFreightSign", "FREIGHT LIFT  /  OFFLINE", (7200, 1340, 590), yaw=-90, size=33)
    # Arrival alcove conceals delayed E03 enemies until the far cargo lane is reached.
    mesh("CargoServiceScreen", (7950, 820, 180), (340, 45, 360), "dark", folder="Fixtures")
    label("CargoServiceSign", "SECURITY ACCESS", (8110, 1390, 380), yaw=-90, size=26)
    label("CargoWindowSign", "OBSERVATION  /  KEEP CLEAR", (6800, -1375, 130), yaw=90, size=25)


def reactor_details():
    # The hero reactor is recessed at the perimeter, leaving the arena's
    # center available for the Bull's rushes and the player's sideways dodges.
    mesh("ReactorCoreHousing", (13500, 640, 290), (370, 370, 580), "glass", "Cylinder", folder="Fixtures")
    mesh("ReactorLuminousCore", (13500, 640, 310), (300, 300, 490), "amber", "Cylinder", collision=False, folder="Fixtures")
    for angle in range(0, 360, 60):
        radians = math.radians(angle)
        mesh("ReactorRib_%d" % angle, (13500 + math.cos(radians) * 200, 640 + math.sin(radians) * 200, 310), (30, 30, 640), "steel", folder="Fixtures")
    for z in (45, 590):
        mesh("ReactorRing_%d" % z, (13500, 640, z), (460, 460, 65), "dark", "Cylinder", folder="Fixtures")
    fill("ReactorCoreGlow", (13500, 370, 530), 1100, 20000, (255, 115, 40), shadows=True)
    crate("ReactorShieldA", 12850, -620, 170, 220, 270, "dark")
    crate("ReactorShieldB", 14200, 640, 170, 220, 270, "dark")
    for y in (-850, 850):
        mesh("ReactorPipe%d" % y, (13500, y, 480), (70, 70, 2700), "orange", "Cylinder", rotation=(90, 0, 0), folder="Fixtures")
    label("ReactorNotice", "BULL SENTINEL  /  CONTAINMENT BREACH", (15045, 0, 430), size=29, color=(255, 170, 70))


def bay_details():
    crate("BayCoverA", 16450, 350, 310, 250, 250)
    crate("BayCoverB", 17250, -350, 360, 260, 270, "dark")
    crate("BayCoverC", 18000, 350, 250, 290, 130)
    # Evacuation pod skin is scenery around its native interactive boarding hatch.
    mesh("PodCradle", (18900, 0, 35), (530, 680, 70), "dark", folder="Fixtures")
    pod_visuals = []
    pod_visuals.append(mesh("PodFuselage", (19020, 0, 240), (570, 490, 470), "white", "Sphere", folder="Fixtures"))
    pod_visuals.append(mesh("PodCanopy", (18850, 0, 330), (170, 390, 190), "blue", "Sphere", collision=False, folder="Fixtures"))
    for y in (-270, 270):
        pod_visuals.append(mesh("PodThruster%d" % y, (19080, y, 125), (150, 150, 310), "dark", "Cylinder", rotation=(90, 0, 0), folder="Fixtures"))
        pod_visuals.append(mesh("PodEngineGlow%d" % y, (19240, y, 125), (120, 120, 12), "teal", "Cylinder", collision=False, rotation=(90, 0, 0), folder="Fixtures"))
    door(6, 18640)
    pod = interaction("POD", "I06_EscapePod", 6, 18530, 0)
    # The mission director translates this actor during departure. Carry the
    # actual pod silhouette with it, while the cradle remains fixed to the ship.
    for actor in pod_visuals:
        actor.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE)
        actor.set_editor_property("tags", [u.Name(TAG), u.Name("OB_POD_VISUAL")])
        if not actor.attach_to_actor(pod, u.Name("None"), u.AttachmentRule.KEEP_WORLD,
                                     u.AttachmentRule.KEEP_WORLD, u.AttachmentRule.KEEP_WORLD, False):
            raise RuntimeError("Could not attach pod visual: " + actor.get_actor_label())
    label("Pod07", "07", (18640, 0, 430), size=100, color=(110, 235, 220))
    label("PodLaunchSign", "MAINTENANCE ESCAPE POD", (18750, 0, 590), size=32)
    for x in range(18100, 18600, 125):
        mesh("EvacMark%d" % x, (x, -190, 3), (45, 110, 4), "amber", collision=False, rotation=(0, -25, 0))
        mesh("EvacMarkR%d" % x, (x, 190, 3), (45, 110, 4), "amber", collision=False, rotation=(0, 25, 0))


def scenery():
    mesh("Planet_Aurelia", (8700, -14500, -2500), (13000, 13000, 13000), "planet", "Sphere", collision=False, folder="Space")
    # Original broad continent shapes and a distant orbital structure.
    mesh("PlanetCloudBand", (8700, -14400, -2450), (13100, 13100, 13100), "glass", "Sphere", collision=False, folder="Space")
    rng = random.Random(92)
    for index in range(100):
        x = rng.uniform(-18000, 38000)
        y = rng.uniform(-25000, -16000)
        z = rng.uniform(3000, 20000)
        size = rng.uniform(10, 35)
        mesh("Star_%03d" % index, (x, y, z), (size, size, size), "star", "Sphere", collision=False, folder="Space")
    for x in (6600, 17400):
        mesh("ExteriorSpine_%d" % x, (x, -2450, -100), (2100, 120, 140), "dark", folder="Space")
        for dx in (-700, 0, 700):
            mesh("ExteriorFin_%d_%d" % (x, dx), (x + dx, -2600, -350), (110, 800, 600), "blue", folder="Space")
            mesh("ExteriorLamp_%d_%d" % (x, dx), (x + dx, -2250, 80), (60, 40, 30), "amber", collision=False, folder="Space")


def planet_surface():
    """A lit ocean/cloud planet with a blue atmospheric rim, not a flat ball."""
    name = "M_Aurelia_V3"
    old = u.load_asset(ENV_MATERIALS + "/" + name)
    if old:
        return old
    mat = ASSETS.create_asset(name, ENV_MATERIALS, u.Material, u.MaterialFactoryNew())
    edit = u.MaterialEditingLibrary

    def node(kind, x, y):
        return edit.create_material_expression(mat, kind, x, y)

    def connect(source, output, target, pin):
        if not edit.connect_material_expressions(source, output, target, pin):
            raise RuntimeError("Planet material connection failed: " + pin)

    uv = node(u.MaterialExpressionTextureCoordinate, -1000, 0)
    uv.set_editor_property("u_tiling", 3.0)
    uv.set_editor_property("v_tiling", 2.0)
    noise = node(u.MaterialExpressionTextureSample, -800, 0)
    noise.set_editor_property("texture", u.load_asset("/Engine/MaterialTemplates/Textures/T_Noise01"))
    uv_pin = "UVs" if "UVs" in list(edit.get_material_expression_input_names(noise)) else "Coordinates"
    connect(uv, "", noise, uv_pin)
    ocean = node(u.MaterialExpressionConstant3Vector, -600, -300)
    ocean.set_editor_property("constant", u.LinearColor(.008, .045, .095, 1))
    cloud = node(u.MaterialExpressionConstant3Vector, -600, -160)
    cloud.set_editor_property("constant", u.LinearColor(.16, .28, .30, 1))
    clouds = node(u.MaterialExpressionPower, -550, 0)
    clouds.set_editor_property("const_exponent", 4.0)
    connect(noise, "R", clouds, "Base")
    surface = node(u.MaterialExpressionLinearInterpolate, -300, -200)
    connect(ocean, "", surface, "A")
    connect(cloud, "", surface, "B")
    connect(clouds, "", surface, "Alpha")
    rim = node(u.MaterialExpressionFresnel, -550, 250)
    rim.set_editor_property("exponent", 5.0)
    rim.set_editor_property("base_reflect_fraction", .02)
    blue = node(u.MaterialExpressionConstant3Vector, -550, 400)
    blue.set_editor_property("constant", u.LinearColor(.015, .19, .42, 1))
    rim_tint = node(u.MaterialExpressionMultiply, -300, 250)
    connect(rim, "", rim_tint, "A")
    connect(blue, "", rim_tint, "B")
    combined = node(u.MaterialExpressionAdd, -50, 0)
    connect(surface, "", combined, "A")
    connect(rim_tint, "", combined, "B")
    edit.connect_material_property(surface, "", u.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(combined, "", u.MaterialProperty.MP_EMISSIVE_COLOR)
    edit.recompile_material(mat)
    save_with_nanite_usage(mat)
    return mat


def cinematic_architecture():
    """Large readable forms. Decorative geometry never blocks a combat lane."""
    for zid, title, left, right, halfwidth, height in ZONES:
        prefix = "V3_Z%02d_" % zid
        center = (left + right) / 2
        width = right - left
        accent = "amber" if zid == 5 else "teal"
        # Continuous lower armor separates the walkable deck from the hull;
        # sloped shoulders give the former box-shaped rooms a stronger profile.
        for side in (-1, 1):
            if zid in (3, 6) and side < 0:
                continue
            detail(prefix + "WallArmor%d" % side, (center, side * (halfwidth - 19), 72),
                   (width - 150, 14, 122), "dark")
            detail(prefix + "WallArmorEdge%d" % side, (center, side * (halfwidth - 31), 138),
                   (width - 170, 9, 9), "steel")
            detail(prefix + "Shoulder%d" % side, (center, side * (halfwidth - 47), height - 95),
                   (width - 150, 18, 150), "blue", rotation=(0, 0, side * 30))
        # Broad portals create intentional foreground/middle/background depth.
        for n, t in enumerate((.22, .70)):
            x = left + width * t
            detail(prefix + "PortalTop%d" % n, (x, 0, height - 48),
                   (110, halfwidth * 2 - 300, 80), "blue")
            for side in (-1, 1):
                y = side * (halfwidth - 110)
                detail(prefix + "PortalChamfer%d_%d" % (n, side), (x, y, height - 135),
                       (110, 260, 90), "white", rotation=(0, 0, side * 43))
                detail(prefix + "PortalKey%d_%d" % (n, side), (x - 58, side * (halfwidth - 38), height * .43),
                       (7, 16, min(220, height * .4)), accent)
        # Matte central walkway panels keep luminous edge lines legible.
        for n, x in enumerate(range(int(left + 270), int(right - 200), 600)):
            detail(prefix + "Walkway%d" % n, (x, 0, .5), (530, 355, .6), "rubber")
            for side in (-1, 1):
                detail(prefix + "WalkwayFastener%d_%d" % (n, side), (x + 248, side * 165, 1.2),
                       (12, 12, .8), "steel")
        # Each room has one visually bold side wall, not wall-to-wall neon.
        x = left + width * .58
        y = halfwidth - 47
        detail(prefix + "IdentityPanel", (x, y, height * .55), (480, 18, 205), "dark")
        label(prefix + "IdentityTitle", {1: "DETENTION", 2: "SECURITY", 3: "FREIGHT / 03", 4: "CONTROL", 5: "REACTOR / 04", 6: "DEPARTURES"}[zid],
              (x, y - 13, height * .55 + 40), yaw=-90, size=40,
              color=(235, 165, 75) if zid == 5 else (145, 191, 207))
        label(prefix + "IdentitySub", {1: "CELL BLOCK / C-14", 2: "AUTHORIZED PERSONNEL", 3: "ORBITAL LOGISTICS", 4: "EMERGENCY TRANSIT", 5: "HEAVY UNIT / ISOLATION", 6: "ESCAPE SYSTEMS / ONLINE"}[zid],
              (x, y - 13, height * .55 - 22), yaw=-90, size=16, color=(115, 147, 156))
        detail(prefix + "IdentityUnderline", (x, y - 13, height * .55 - 67), (400, 4, 5), accent)


def hero_landmarks():
    # Detention: a framed observation slit and a blue service recess establish
    # the ship's materials before the first encounter.
    detail("V3_CellBackPanel", (-777, 0, 235), (10, 430, 245), "blue")
    detail("V3_CellBackInset", (-770, 0, 245), (6, 340, 135), "dark")
    label("V3_CellWarning", "LOCKDOWN IN EFFECT", (-764, 0, 245), yaw=0, size=22, color=(215, 151, 75))
    detail("V3_CellBackLight", (-763, 0, 182), (4, 300, 4), "amber")
    # Security's striped bays frame cover and create clear distance markers.
    for n, x in enumerate((2120, 3310, 4260)):
        side = 1 if n % 2 else -1
        detail("V3_SecurityWallPanel%d" % n, (x, side * 423, 280), (420, 12, 245), "blue")
        label("V3_SecurityRoute%d" % n, "%02d / PATROL" % (n + 1), (x, side * 414, 290),
              yaw=-90 if side > 0 else 90, size=26, color=(140, 178, 190))
        detail("V3_SecurityAmber%d" % n, (x, side * 412, 210), (310, 5, 6), "amber")
    # Large suspended freight pods and dock markings support Cargo's scale.
    # All overhead forms start above 300 cm; the existing ramp stays unchanged.
    for n, x in enumerate((5610, 8280)):
        detail("V3_FreightPod%d" % n, (x, 1110, 505), (420, 380, 350), "cargo")
        for side in (-1, 1):
            detail("V3_FreightPodRail%d_%d" % (n, side), (x + side * 176, 902, 505), (24, 24, 355), "steel")
        label("V3_FreightPodCode%d" % n, "ARK / %03d" % (120 + n), (x, 905, 510), yaw=-90, size=32, color=(225, 193, 120))
    label("V3_CargoWindowLegend", "AURELIA  /  LOW ORBIT", (6800, -1372, 660), yaw=90, size=38, color=(135, 185, 215))
    fill("V3_CargoWindowKey", (6850, -1170, 470), 1200, 14500, (65, 133, 230))
    # Control: a large system diagram across the wall behind the console.
    detail("V3_ControlScreenBack", (10220, -873, 330), (980, 20, 260), "dark")
    detail("V3_ControlScreen", (10220, -859, 345), (900, 6, 195), "screen")
    for n in range(6):
        x = 9870 + n * 140
        detail("V3_ControlRouteNode%d" % n, (x, -853, 345), (58, 4, 58), "amber" if n == 4 else "teal")
        if n < 5:
            detail("V3_ControlRouteLink%d" % n, (x + 70, -853, 345), (82, 4, 5), "steel")
    label("V3_ControlReadout", "DECK C  //  EVACUATION ROUTE", (10220, -849, 415), yaw=90, size=26, color=(160, 205, 214))
    label("V3_ControlFault", "REACTOR 04 : HEAVY SECURITY UNIT", (10220, -849, 280), yaw=90, size=20, color=(239, 165, 75))
    # Reactor arena: brass floor ring and broad hazard bands visibly separate
    # the boss's space. Everything is flush and does not catch the player's feet.
    for n in range(24):
        angle = n * 15
        r = math.radians(angle)
        detail("V3_ArenaRing%02d" % n, (13800 + math.cos(r) * 950, math.sin(r) * 550, 1.4),
               (185, 12, 1), "brass", rotation=(0, angle + 90, 0))
    for side in (-1, 1):
        detail("V3_ArenaSideBand%d" % side, (13800, side * 790, 2), (1950, 23, 2), "orange")
        for x in (12900, 14700):
            detail("V3_ArenaHazard%d_%d" % (x, side), (x, side * 660, 2), (60, 225, 2), "orange", rotation=(0, side * 30, 0))
    detail("V3_ReactorBackplate", (13500, 878, 365), (1080, 18, 665), "dark")
    for x in (13080, 13920):
        detail("V3_ReactorTower%d" % x, (x, 823, 380), (135, 100, 650), "blue")
        detail("V3_ReactorTowerAccent%d" % x, (x, 766, 405), (16, 5, 360), "amber")
    label("V3_ReactorIsolation", "04", (13500, 862, 695), yaw=-90, size=95, color=(226, 163, 78))
    fill("V3_BullRim", (14650, -625, 470), 900, 16500, (65, 145, 215))
    # The HELIX rack is visibly on the safe side of the trigger. The native
    # interaction supplies the actual gun so its model disappears on pickup.
    detail("V3_HelixArmoryBack", (12200, 465, 165), (380, 28, 315), "dark")
    detail("V3_HelixArmoryInset", (12200, 446, 180), (315, 8, 235), "blue")
    for side in (-1, 1):
        detail("V3_HelixArmoryEdge%d" % side, (12200 + side * 163, 438, 182), (6, 5, 225), "teal")
    label("V3_HelixArmoryTitle", "HELIX-12", (12200, 435, 278), yaw=-90, size=35, color=(125, 227, 240))
    label("V3_HelixArmorySub", "PLASMA REPEATER  /  E TO EQUIP", (12200, 435, 235), yaw=-90, size=14, color=(162, 202, 213))
    label("V3_HelixBossHint", "HEAVY UNIT AHEAD", (12200, 435, 71), yaw=-90, size=18, color=(235, 175, 75))
    fill("V3_HelixDisplayLight", (12160, 300, 275), 400, 3400, (65, 205, 235))
    # The escape gantry has a coherent launch axis and a dramatic blue view.
    for side in (-1, 1):
        detail("V3_LaunchGuide%d" % side, (18720, side * 515, 2), (850, 18, 2), "amber")
        detail("V3_LaunchPylon%d" % side, (18480, side * 735, 280), (140, 150, 560), "blue")
        detail("V3_LaunchPylonLight%d" % side, (18402, side * 735, 310), (5, 60, 310), "teal")
    label("V3_EvacWindowLegend", "ORBITAL DEPARTURES  //  AURELIA", (17300, -1274, 710), yaw=90, size=36, color=(135, 185, 215))
    fill("V3_BayWindowKey", (17300, -1080, 520), 1400, 17500, (65, 130, 230))


def orbital_landmark():
    # Silhouetted exterior station gives the observation windows real depth.
    # A twenty-four-piece orbital arc reads as one large structure, not clutter.
    for n in range(24):
        angle = n * 15
        r = math.radians(angle)
        x = 11000 + math.cos(r) * 3500
        z = 2200 + math.sin(r) * 3500
        mesh("V3_OrbitalArc%02d" % n, (x, -8600, z), (925, 160, 140), "steel",
             collision=False, rotation=(angle + 90, 0, 0), folder="Space")
    for x in (7400, 14600):
        mesh("V3_OrbitalHub%d" % x, (x, -8500, 2050), (650, 600, 900), "blue", collision=False, folder="Space")
        mesh("V3_OrbitalHubBeacon%d" % x, (x, -8160, 2200), (220, 25, 40), "amber", collision=False, folder="Space")
        for z in (1200, 2900):
            mesh("V3_SolarArray%d_%d" % (x, z), (x, -8800, z), (1800, 1400, 40), "blue", collision=False, rotation=(0, 0, 18), folder="Space")
    for actor in ACTORS.get_all_level_actors():
        if actor.get_actor_label() == "Planet_Aurelia":
            actor.static_mesh_component.set_material(0, planet_surface())
            break


def populate_gameplay():
    director = spawn(cls("OBMissionDirector"), "Mission_EmergencyDeparture", (0, 0, 0))
    director.set_editor_property("start_location", vec(START))
    director.set_editor_property("cargo_checkpoint_location", vec(CARGO_CP))
    director.set_editor_property("reactor_checkpoint_location", vec(REACTOR_CP))
    spawn(u.PlayerStart, "PlayerStart_Rook", START)
    for eid, member, x, y, heavy in ENEMIES:
        actor = spawn(cls("OBEnemyCharacter"), "E%02d_%02d_%s" % (eid, member, "Sentinel" if heavy else "Trooper"), (x, y, 116 if heavy else 100), yaw=180, folder="Encounters")
        actor.set_editor_property("stable_id", u.Name("E%02d_EN%02d" % (eid, member)))
        actor.set_editor_property("encounter_id", eid)
        actor.set_editor_property("heavy", heavy)
        if heavy:
            actor.set_editor_property("sentinel_health", 2400.0)
        if heavy and u.EditorAssetLibrary.does_asset_exist("/Game/SciFi_Beast06/Mesh/SK_SciFi_Beast06_Skin1"):
            import importlib.util
            bull_spec = importlib.util.spec_from_file_location("orbit_heavy_bull", str(ROOT / "Scripts" / "heavy_bull.py"))
            bull_module = importlib.util.module_from_spec(bull_spec)
            bull_spec.loader.exec_module(bull_module)
            bull_module.apply_to_enemy(actor)
            # Persist the same authored mesh/idle pose in the editable scene;
            # BeginPlay reapplies these properties for the combat version.
            component = actor.get_component_by_class(u.SkeletalMeshComponent)
            component.set_skeletal_mesh_asset(u.load_asset(bull_module.MESH))
            component.set_relative_location(vec((0, 0, -112)), False, False)
            component.set_relative_rotation(u.Rotator(pitch=0, yaw=-90, roll=0), False, False)
            component.set_relative_scale3d(vec((1.08, 1.08, 1.08)))
            component.override_animation_data(u.load_asset(bull_module.ANIMS + "Idle1"), True, True, 0.0, 1.0)
            component.set_update_animation_in_editor(True)
    for index, x in enumerate((800, 4600, 8600, 11000, 15100), start=1):
        door(index, x)
    zone_trigger(2, 1780, 450)
    zone_trigger(3, 5000, 300, True)
    zone_trigger(3, 5450, 1400)
    zone_trigger(4, 9250, 900)
    zone_trigger(5, 11930, 300, True)
    zone_trigger(5, 12400, 900)
    zone_trigger(6, 16020, 1300)
    interaction("TERMINAL", "I04_TransitTerminal", 4, 10450, -520)
    interaction("HEAVY_WEAPON", "I05_HelixRifle", 5, 12200, 280)
    stations = [(2, 1570, -290), (3, 5190, -280), (4, 9280, -520), (5, 12070, -280), (6, 15790, -500)]
    for zone, x, y in stations:
        interaction("AMMO_STATION", "ST_Z%02d_SecurityAmmo" % zone, zone, x, y)
        label("StationSign%d" % zone, "SECURITY AMMUNITION", (x, y - 85, 205), yaw=90, size=17, color=(255, 190, 95))
    pickups = [
        (2, "MEDICAL", 2620, -260), (2, "AMMO", 3950, -250),
        (3, "MEDICAL", 5840, -900), (3, "MEDICAL", 7560, 880), (3, "AMMO", 6020, 400), (3, "AMMO", 8080, -950),
        (4, "MEDICAL", 9670, -650), (4, "AMMO", 10650, 450),
        (5, "MEDICAL", 12620, 500), (5, "MEDICAL", 14530, -550), (5, "AMMO", 13020, 650), (5, "AMMO", 14700, 500),
        (6, "MEDICAL", 16280, -850), (6, "MEDICAL", 17950, 900), (6, "AMMO", 16900, -850), (6, "AMMO", 18200, -550),
    ]
    for index, (zone, kind, x, y) in enumerate(pickups):
        interaction(kind, "P%02d_Z%02d_%s" % (index + 1, zone, kind), zone, x, y)


def nav_and_world():
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", cls("OBGameMode"))
    settings.set_editor_property("kill_z", -1200.0)
    settings.set_editor_property("enable_world_bounds_checks", True)
    nav = spawn(u.NavMeshBoundsVolume, "Nav_MissionRoute", (9200, 0, 250), folder="Navigation")
    # EditorActorSubsystem uses the registered box-volume actor factory.
    # Read its actual brush bounds instead of assuming a particular default cube.
    _, extents = nav.get_actor_bounds(False)
    if min(extents.x, extents.y, extents.z) <= 0:
        raise RuntimeError("NavMeshBoundsVolume has an empty brush; build a box volume before validating navigation.")
    nav.set_actor_scale3d(vec((10200 / extents.x, 1700 / extents.y, 700 / extents.z)))
    navigation = u.NavigationSystemV1.get_navigation_system(world)
    if navigation:
        navigation.on_navigation_bounds_updated(nav)
    # Navigation is requested and awaited after all geometry is placed.
    # Exposure is fixed so dark walls do not turn exterior windows into white glare.
    post = spawn(u.PostProcessVolume, "Exposure_Comfort", (0, 0, 0), folder="Lighting")
    post.set_editor_property("unbound", True)
    pp = post.get_editor_property("settings")
    for name, value in [("override_auto_exposure_min_brightness", True), ("override_auto_exposure_max_brightness", True), ("auto_exposure_min_brightness", 5.0), ("auto_exposure_max_brightness", 5.0), ("override_motion_blur_amount", True), ("motion_blur_amount", 0.0), ("override_bloom_intensity", True), ("bloom_intensity", .16), ("override_ambient_occlusion_intensity", True), ("ambient_occlusion_intensity", 1.1), ("override_ambient_occlusion_radius", True), ("ambient_occlusion_radius", 110.0), ("override_color_contrast", True), ("color_contrast", u.Vector4(1.04, 1.04, 1.04, 1.0)), ("override_color_saturation", True), ("color_saturation", u.Vector4(.92, .92, .92, 1.0))]:
        pp.set_editor_property(name, value)
    post.set_editor_property("settings", pp)
    # Very restrained non-volumetric distance haze; no expensive volumetric
    # shadows or dense smoke are required for the small-room prototype.
    fog = spawn(u.ExponentialHeightFog, "Atmosphere_DeckHaze", (0, 0, -100), folder="Lighting")
    fog_component = fog.get_component_by_class(u.ExponentialHeightFogComponent)
    fog_component.set_fog_density(.006)
    fog_component.set_fog_height_falloff(.3)
    fog_component.set_fog_inscattering_color(u.LinearColor(.025, .045, .065, 1))
    fog_component.set_fog_max_opacity(.18)
    fog_component.set_start_distance(800.0)
    fog_component.set_volumetric_fog(False)


def import_audio():
    source = ROOT / "SourceAssets" / "Audio"
    tasks = []
    for path in sorted(source.glob("S_*.wav")):
        if u.EditorAssetLibrary.does_asset_exist(BASE + "/Audio/" + path.stem):
            continue
        task = u.AssetImportTask()
        task.set_editor_property("filename", str(path))
        task.set_editor_property("destination_path", BASE + "/Audio")
        task.set_editor_property("destination_name", path.stem)
        task.set_editor_property("automated", True)
        task.set_editor_property("save", True)
        task.set_editor_property("replace_existing", False)
        tasks.append(task)
    if tasks:
        ASSETS.import_asset_tasks(tasks)
    for cue in ("Ambience", "Alarm"):
        sound = u.load_asset(BASE + "/Audio/S_" + cue)
        if sound:
            sound.set_editor_property("looping", cue == "Ambience")
            u.EditorAssetLibrary.save_loaded_asset(sound)
    for cue in ("Shot", "Reload", "DryFire", "Damage", "Pickup", "Charge", "EnemyShot", "Impact", "EnemyDamage", "EnemyDeath", "Door", "Terminal", "Checkpoint", "Ambience", "Alarm", "Launch"):
        if not u.EditorAssetLibrary.does_asset_exist(BASE + "/Audio/S_" + cue):
            raise RuntimeError("Required audio asset absent: S_" + cue)


def validate_saved():
    actors = ACTORS.get_all_level_actors()
    types = {name: cls(name) for name in ("OBMissionDirector", "OBDoor", "OBInteractable", "OBEnemyCharacter", "OBZoneTrigger")}
    grouped = {name: [a for a in actors if u.MathLibrary.class_is_child_of(a.get_class(), kind)] for name, kind in types.items()}
    failures = []
    if len(grouped["OBMissionDirector"]) != 1:
        failures.append("Expected exactly one mission director")
    if len([a for a in actors if isinstance(a, u.PlayerStart)]) != 1:
        failures.append("Expected exactly one PlayerStart")
    door_ids = sorted(a.get_editor_property("door_id") for a in grouped["OBDoor"])
    if door_ids != list(range(7)):
        failures.append("Door roster differs from D00-D06: " + repr(door_ids))
    counts = {eid: 0 for eid in range(1, 7)}
    identifiers = set()
    heavy = 0
    for enemy in grouped["OBEnemyCharacter"]:
        stable = str(enemy.get_editor_property("stable_id"))
        if stable in identifiers:
            failures.append("Repeated enemy identifier: " + stable)
        identifiers.add(stable)
        eid = enemy.get_editor_property("encounter_id")
        counts[eid] = counts.get(eid, 0) + 1
        heavy += int(enemy.get_editor_property("heavy"))
    if counts != {1: 3, 2: 3, 3: 2, 4: 3, 5: 1, 6: 4} or heavy != 1:
        failures.append("Encounter roster requires 15 troopers + one solo reactor sentinel")
    boss_roster = [a for a in grouped["OBEnemyCharacter"] if a.get_editor_property("encounter_id") == 5]
    if len(boss_roster) != 1 or str(boss_roster[0].get_editor_property("stable_id")) != "E05_EN03" or not boss_roster[0].get_editor_property("heavy"):
        failures.append("Reactor must contain only Bull sentinel E05_EN03")
    elif boss_roster[0].get_editor_property("sentinel_health") != 2400.0:
        failures.append("Bull sentinel must have 2400 health")
    helix_racks = [a for a in grouped["OBInteractable"] if a.get_editor_property("kind") == u.OBInteractionKind.HEAVY_WEAPON]
    if len(helix_racks) != 1:
        failures.append("Expected exactly one HELIX-12 rack")
    else:
        rack = helix_racks[0]
        if str(rack.get_editor_property("stable_id")) != "I05_HelixRifle" or rack.get_editor_property("zone") != 5 or rack.get_actor_location().x >= 12320:
            failures.append("HELIX-12 must be in the safe alcove before the reactor trigger")
    station_zones = sorted(a.get_editor_property("zone") for a in grouped["OBInteractable"] if a.get_editor_property("kind") == u.OBInteractionKind.AMMO_STATION)
    if station_zones != [2, 3, 4, 5, 6]:
        failures.append("Each combat zone requires exactly one recovery station")
    for trigger in grouped["OBZoneTrigger"]:
        box = trigger.get_component_by_class(u.BoxComponent)
        if box.get_collision_response_to_channel(u.CollisionChannel.ECC_ENEMY_BOLT) != u.CollisionResponseType.ECR_IGNORE:
            failures.append("Encounter trigger blocks enemy shots: " + trigger.get_name())
    report = {"map": MAP, "actor_count": len(actors), "generated_counts": COUNTS, "encounters": counts, "heavy_count": heavy, "heavy_health": boss_roster[0].get_editor_property("sentinel_health") if len(boss_roster) == 1 else None, "helix_racks": len(helix_racks), "station_zones": station_zones, "errors": failures, "navigation": {"status": "PENDING", "scope": "Authored spawn projections and encounter paths; gameplay traversal remains unverified"}, "status": "FAIL" if failures else "PASS_STRUCTURAL"}
    (ROOT / "docs").mkdir(parents=True, exist_ok=True)
    (ROOT / "docs" / "CONTENT_VALIDATION.json").write_text(json.dumps(report, indent=2) + "\n")
    if failures:
        raise RuntimeError("; ".join(failures))
    u.log("ORBIT_CONTENT_STRUCTURE_OK " + json.dumps(report))
    return report


def backup_existing_map():
    """Copy the saved map and any sidecars before touching generated actors."""
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if world and world.get_path_name().split(".")[0] == MAP:
        # Include any current edits to this mission in the recoverable copy.
        if not LEVELS.save_current_level():
            raise RuntimeError("Cannot save the current mission before repair backup")
    relative_map = Path("Content") / (MAP.removeprefix("/Game/") + ".umap")
    map_file = ROOT / relative_map
    if not map_file.is_file():
        raise RuntimeError("Repair refused: saved map is unavailable for backup: " + str(map_file))
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S_%fZ")
    destination = ROOT / "Saved" / "Backups" / ("EmergencyDeparture_" + stamp)
    destination.mkdir(parents=True, exist_ok=False)
    sources = [map_file]
    for suffix in (".uexp", ".ubulk"):
        sidecar = map_file.with_suffix(suffix)
        if sidecar.is_file():
            sources.append(sidecar)
    for suffix in (".uasset", ".uexp", ".ubulk"):
        built_data = map_file.with_name(map_file.stem + "_BuiltData" + suffix)
        if built_data.is_file():
            sources.append(built_data)
    # This mission is normally a non-partitioned level. Preserve external actor
    # packages too if an owner has converted it to a partitioned/external map.
    for folder in ("__ExternalActors__", "__ExternalObjects__"):
        external = ROOT / "Content" / folder / MAP.removeprefix("/Game/")
        if external.is_dir():
            sources.extend(path for path in external.rglob("*") if path.is_file())
    records = []
    for source in sorted(sources):
        relative = source.relative_to(ROOT)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
        if source_hash != hashlib.sha256(target.read_bytes()).hexdigest():
            raise RuntimeError("Repair refused: backup verification failed for " + str(relative))
        records.append({"path": relative.as_posix(), "bytes": target.stat().st_size, "sha256": source_hash})
    metadata = {"map": MAP, "created_utc": stamp, "source_project": str(ROOT), "files": records,
                "restore": "Close Unreal Editor. Preserve current work, then copy these Content-relative files back into the project. Reopen and validate."}
    (destination / "BACKUP_MANIFEST.json").write_text(json.dumps(metadata, indent=2) + "\n")
    u.log("ORBIT_REPAIR_BACKUP_OK " + str(destination))
    return destination


def project_nav_point(world, point):
    projected = u.NavigationSystemV1.project_point_to_navigation(
        world, point, None, None, vec((90, 90, 220)))
    # UE Python normally returns the FVector (or None for bool-return failure).
    # Also handle bindings that retain a tuple containing the bool and out value.
    if hasattr(projected, "x"):
        return projected
    if isinstance(projected, tuple):
        if any(isinstance(item, bool) and not item for item in projected):
            return None
        return next((item for item in projected if hasattr(item, "x")), None)
    return None


def validate_navigation(world):
    """Check saved authored spaces without opening gates or moving any actors."""
    actors = ACTORS.get_all_level_actors()
    enemy_type = cls("OBEnemyCharacter")
    director_type = cls("OBMissionDirector")
    enemies = [actor for actor in actors if u.MathLibrary.class_is_child_of(actor.get_class(), enemy_type)]
    directors = [actor for actor in actors if u.MathLibrary.class_is_child_of(actor.get_class(), director_type)]
    anchors = {1: (1850, 0, 100), 2: (5520, 0, 100), 3: (5520, 0, 100),
               4: (9350, 0, 100), 5: (12500, 0, 116), 6: (16100, 0, 100)}
    projections = []
    paths = []
    failures = []

    def project(name, location):
        projected = project_nav_point(world, location)
        projections.append({"name": name, "source": [location.x, location.y, location.z],
                            "projected": [projected.x, projected.y, projected.z] if projected is not None else None})
        if projected is None:
            failures.append("No navigation at " + name)
        return projected

    def path_check(name, start, end, context=None):
        start_nav = project(name + "_start", start)
        end_nav = project(name + "_target", end)
        if start_nav is None or end_nav is None:
            paths.append({"name": name, "valid": False, "reason": "projection failed"})
            return
        path = u.NavigationSystemV1.find_path_to_location_synchronously(world, start_nav, end_nav, context, None)
        valid = bool(path and path.is_valid() and not path.is_partial())
        paths.append({"name": name, "valid": valid,
                      "partial": bool(path and path.is_partial()),
                      "length_cm": path.get_path_length() if valid else None})
        if not valid:
            failures.append("No complete navigation path: " + name)

    if len(directors) == 1:
        for name in ("start_location", "cargo_checkpoint_location", "reactor_checkpoint_location"):
            project(name, directors[0].get_editor_property(name))
    for enemy in enemies:
        eid = enemy.get_editor_property("encounter_id")
        if eid in anchors:
            path_check(str(enemy.get_editor_property("stable_id")), enemy.get_actor_location(), vec(anchors[eid]), enemy)
    # Test optional ramp and both large obstacle areas in addition to every enemy.
    for name, start, end in [
        ("CargoAcross", (5520, 0, 100), (8380, 0, 100)),
        ("CargoRamp", (6200, 720, 100), (7350, 1160, 190)),
        ("ReactorAcross", (12500, 0, 100), (14850, 0, 100)),
        ("HelixApproach", REACTOR_CP, (12090, 280, 100)),
        ("ReactorDodgeSouth", (13800, 0, 100), (13800, -620, 100)),
        ("ReactorDodgeNorth", (14400, 0, 100), (14500, 610, 100)),
        ("BayAcross", (16100, 0, 100), (18300, 250, 100)),
    ]:
        path_check(name, vec(start), vec(end))
    return {"status": "FAIL" if failures else "PASS", "projections": projections,
            "paths": paths, "errors": failures,
            "scope": "Editor navmesh spawn projections and encounter/ramp paths with authored gates closed; runtime door updates, player collision and playthrough still require testing"}


def wait_for_navigation(report, save_after, completion_marker, rebuild=False):
    """Yield to real editor ticks; never busy-wait the game thread."""
    global _NAV_STATE
    if _NAV_STATE is not None:
        raise RuntimeError("Navigation verification is already pending")
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not u.NavigationSystemV1.get_navigation_system(world):
        raise RuntimeError("No navigation system exists in the mission world")
    state = {"started": time.monotonic(), "idle_since": None, "last_message": 0.0,
             "keep_alive_before": u.EditorPythonScripting.get_keep_python_script_alive(), "handle": None,
             "rebuild_pending": rebuild, "rebuilt_at": None}
    _NAV_STATE = state
    u.EditorPythonScripting.set_keep_python_script_alive(True)

    def finish():
        global _NAV_STATE
        u.EditorPythonScripting.set_keep_python_script_alive(state["keep_alive_before"])
        _NAV_STATE = None

    def fail(message):
        report["status"] = "FAIL"
        report["errors"].append(message)
        if report["navigation"].get("status") == "PENDING":
            report["navigation"]["status"] = "FAIL"
        (ROOT / "docs" / "CONTENT_VALIDATION.json").write_text(json.dumps(report, indent=2) + "\n")
        u.log_error("ORBIT_BOOTSTRAP_FAILED " + message)

    def tick(delta_seconds):
        now = time.monotonic()
        elapsed = now - state["started"]
        try:
            if elapsed > NAV_TIMEOUT_SECONDS:
                raise RuntimeError("Navigation did not settle within %.0f seconds" % NAV_TIMEOUT_SECONDS)
            current_world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
            if current_world != world:
                raise RuntimeError("Loaded world changed during navigation verification")
            busy = u.NavigationSystemV1.is_navigation_being_built_or_locked(world)
            if busy or elapsed < 2.0:
                state["idle_since"] = None
                if elapsed - state["last_message"] >= 15.0:
                    u.log("ORBIT_NAVIGATION_WAIT %.1f seconds" % elapsed)
                    state["last_message"] = elapsed
                return True
            # Asset loading can lock navigation during the initial authoring
            # call. Request the rebuild on an unlocked editor tick, then let
            # the new tiles settle before querying or saving their paths.
            if state["rebuild_pending"]:
                u.SystemLibrary.execute_console_command(world, "RebuildNavigation")
                state["rebuild_pending"] = False
                state["rebuilt_at"] = time.monotonic()
                state["idle_since"] = None
                return True
            if state["rebuilt_at"] is not None and now - state["rebuilt_at"] < 2.0:
                return True
            if state["idle_since"] is None:
                state["idle_since"] = now
                return True
            if now - state["idle_since"] < 1.0:
                return True
            nav_report = validate_navigation(world)
            report["navigation"] = nav_report
            if nav_report["errors"]:
                raise RuntimeError("; ".join(nav_report["errors"]))
            if save_after:
                if not LEVELS.save_current_level():
                    raise RuntimeError("Mission map save failed after navigation settled")
                if not u.EditorAssetLibrary.save_directory(BASE, only_if_is_dirty=True, recursive=True):
                    raise RuntimeError("Generated asset save failed after navigation settled")
            report["status"] = "PASS_STRUCTURE_AND_NAVIGATION"
            report["navigation"]["wait_seconds"] = round(elapsed, 3)
            report["saved_after_navigation"] = save_after
            (ROOT / "docs" / "CONTENT_VALIDATION.json").write_text(json.dumps(report, indent=2) + "\n")
            u.log("ORBIT_NAVIGATION_OK " + json.dumps({"paths": len(nav_report["paths"]), "projections": len(nav_report["projections"])}))
            u.log(completion_marker)
        except Exception as error:
            try:
                fail(str(error))
                u.log_error(traceback.format_exc())
            except Exception as report_error:
                # Even a disk/report failure must release the editor runner and
                # return False so FTSTicker does not keep calling this delegate.
                u.log_error("ORBIT_BOOTSTRAP_FAILED: report write failed: " + str(report_error))
            finally:
                finish()
            return False
        finish()
        return False

    try:
        state["handle"] = u.register_ticker_callback(tick, 0.25)
        u.log("ORBIT_NAVIGATION_PENDING: editor kept alive until verification completes")
    except Exception:
        finish()
        raise


def build():
    if REPAIR and VALIDATE_ONLY:
        raise RuntimeError("--repair and --validate are mutually exclusive")
    if ROOT.name != "OrbitBreakout":
        raise RuntimeError("Refusing to author a different Unreal project: " + str(ROOT))
    if REPAIR_ENV_MATERIALS and not REPAIR:
        if VALIDATE_ONLY:
            raise RuntimeError("--repair-env-materials and --validate are mutually exclusive")
        create_materials()
        u.AutomationLibrary.finish_loading_before_screenshot()
        u.log("ORBIT_ENVIRONMENT_MATERIALS_COMPLETE: generated alloy refreshed; map actors unchanged")
        return
    for name in ("OBGameMode", "OBMissionDirector", "OBDoor", "OBInteractable", "OBZoneTrigger", "OBEnemyCharacter"):
        cls(name)
    exists = u.EditorAssetLibrary.does_asset_exist(MAP)
    if VALIDATE_ONLY and not exists:
        raise RuntimeError("Cannot validate: mission map has not been generated")
    if exists and not REPAIR:
        if not LEVELS.load_level(MAP):
            raise RuntimeError("Existing mission could not be loaded")
        report = validate_saved()
        u.log("Existing mission preserved. Use explicit --repair only to rebuild generated actors.")
        wait_for_navigation(report, False, "ORBIT_VALIDATION_COMPLETE: existing mission preserved; structure and navigation verified")
        return
    if exists:
        backup_existing_map()
    create_materials()
    import_audio()
    if exists:
        if not LEVELS.load_level(MAP):
            raise RuntimeError("Mission could not be loaded for repair")
        for actor in ACTORS.get_all_level_actors():
            if u.Name(TAG) in actor.tags:
                ACTORS.destroy_actor(actor)
    elif not LEVELS.new_level(MAP):
        raise RuntimeError("Failed to create real mission map")
    for zone in ZONES:
        shell(zone)
    for index in range(5):
        connector(ZONES[index][3], ZONES[index + 1][2], index + 1)
    # Seal the route's ends. Internal entry/exit openings remain clear.
    mesh("AftHullCap", (-825, 0, 225), (50, 1200, 450), "dark")
    # A sealed transparent launch field keeps gameplay collision inside the
    # ship while letting the cinematic camera see the departing pod.
    mesh("ForwardLaunchField", (19225, 0, 350), (15, 900, 700), "glass")
    cell_details()
    crate("SecurityCartA", 2780, -300, 180, 200, 300, "dark")
    crate("SecurityCartB", 3650, 300, 180, 200, 240)
    cargo_details()
    crate("JunctionShieldA", 9780, 300, 230, 210, 260, "dark")
    crate("JunctionShieldB", 10300, 380, 230, 230, 130)
    crate("JunctionShieldC", 10000, -300, 180, 220, 220)
    label("TransitControlSign", "TRANSIT CONTROL", (10450, -700, 270), yaw=90, size=30)
    label("SecurityServiceDoor", "RESPONSE UNIT  /  STANDBY", (10400, 875, 350), yaw=-90, size=25, color=(255, 185, 90))
    reactor_details()
    bay_details()
    machinery_details()
    scenery()
    cinematic_architecture()
    hero_landmarks()
    orbital_landmark()
    populate_gameplay()
    nav_and_world()
    if not LEVELS.save_current_level():
        raise RuntimeError("Mission map save failed")
    u.EditorAssetLibrary.save_directory(BASE, only_if_is_dirty=True, recursive=True)
    report = validate_saved()
    u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(vec((-730, -140, 220)), u.Rotator(pitch=-8, yaw=7, roll=0))
    manifest = {"map": MAP, "zones": ZONES, "start": START, "checkpoints": {"CP01_CARGO": CARGO_CP, "CP02_REACTOR": REACTOR_CP}, "enemies": ENEMIES, "tag": TAG, "version": 3, "visual_revision": "cinematic_ship_v3", "boss": {"stable_id": "E05_EN03", "health": 2400, "solo": True}, "pre_boss_weapon": {"stable_id": "I05_HelixRifle", "location": [12200, 280, 0], "kind": "HELIX-12"}}
    (ROOT / "docs" / "LAYOUT_MANIFEST.json").write_text(json.dumps(manifest, indent=2) + "\n")
    # Finish deferred mesh/material jobs once before timing navigation. UE's
    # normal async-load navigation lock then releases on subsequent ticks.
    # Without submitting these jobs first, a generated scene can remain locked
    # until a later viewport/screenshot request happens to finish compilation.
    u.AutomationLibrary.finish_loading_before_screenshot()
    wait_for_navigation(report, True, "ORBIT_BOOTSTRAP_COMPLETE: real map and assets saved after navigation verification", rebuild=True)


build()
