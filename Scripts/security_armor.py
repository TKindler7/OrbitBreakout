"""Preserve Manny's authored surface detail while adding a full armor tint.

Run in the Unreal editor. Source materials and textures stay intact; the cloned
master retains their masks, normal maps and material responses. Re-running is
safe and does not add extra multiply nodes.
"""
import unreal as u

ROOT = "/Game/OrbitBreakout/Materials/Characters"
SOURCE = "/Game/Characters/Mannequins/Materials"
MASTER = ROOT + "/M_Armor"
edit = u.MaterialEditingLibrary
assets = u.EditorAssetLibrary
assets.make_directory(ROOT)
material = u.load_asset(MASTER)

if material is None:
    material = assets.duplicate_asset(SOURCE + "/M_Mannequin", MASTER)
    if material is None:
        raise RuntimeError("Could not duplicate the authored mannequin shader")

parameter_names = {str(name) for name in edit.get_vector_parameter_names(material)}
if "ArmorTint" not in parameter_names:
    base = edit.get_material_property_input_node(material, u.MaterialProperty.MP_BASE_COLOR)
    if base is None:
        raise RuntimeError("Authored mannequin shader has no BaseColor input")
    output = edit.get_material_property_input_node_output_name(material, u.MaterialProperty.MP_BASE_COLOR)
    tint = edit.create_material_expression(material, u.MaterialExpressionVectorParameter, 100, -300)
    tint.set_editor_property("parameter_name", "ArmorTint")
    tint.set_editor_property("default_value", u.LinearColor(r=1, g=1, b=1, a=1))
    multiply = edit.create_material_expression(material, u.MaterialExpressionMultiply, 400, -100)
    if not edit.connect_material_expressions(base, output, multiply, "A"):
        raise RuntimeError("Could not preserve the authored BaseColor connection")
    if not edit.connect_material_expressions(tint, "", multiply, "B"):
        raise RuntimeError("Could not connect the full armor tint")
    if not edit.connect_material_property(multiply, "", u.MaterialProperty.MP_BASE_COLOR):
        raise RuntimeError("Could not connect the armor shader BaseColor")

# Calibrated under the mission's practical lights: keep dark armor from
# washing out while retaining the source's specular edges and normal detail.
if 'ArmorExposure' not in {str(name) for name in edit.get_scalar_parameter_names(material)}:
    base = edit.get_material_property_input_node(material, u.MaterialProperty.MP_BASE_COLOR)
    output = edit.get_material_property_input_node_output_name(material, u.MaterialProperty.MP_BASE_COLOR)
    exposure = edit.create_material_expression(material, u.MaterialExpressionScalarParameter, 400, -330)
    exposure.set_editor_property('parameter_name', 'ArmorExposure')
    exposure.set_editor_property('default_value', .12)
    calibrated = edit.create_material_expression(material, u.MaterialExpressionMultiply, 650, -100)
    if not edit.connect_material_expressions(base, output, calibrated, 'A'):
        raise RuntimeError('Could not preserve tinted armor output')
    if not edit.connect_material_expressions(exposure, '', calibrated, 'B'):
        raise RuntimeError('Could not connect armor light calibration')
    edit.connect_material_property(calibrated, '', u.MaterialProperty.MP_BASE_COLOR)

edit.set_base_material_usage(material, u.MaterialUsage.MATUSAGE_SKELETAL_MESH, True)
edit.set_base_material_usage(material, u.MaterialUsage.MATUSAGE_NANITE, True)
edit.recompile_material(material)
if not assets.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("Could not save the armor master")

instances = {}
for slot in (1, 2):
    source_path = SOURCE + "/Manny/MI_Manny_%02d_New" % slot
    target_path = ROOT + "/MI_Armor_%02d" % slot
    instance = u.load_asset(target_path)
    if instance is None:
        instance = assets.duplicate_asset(source_path, target_path)
    if instance is None:
        raise RuntimeError("Could not duplicate Manny material slot %d" % slot)
    # Manny's second slot inherits scalar/paint overrides from its first slot.
    # Keep that inheritance as well as each slot's own texture overrides.
    edit.set_material_instance_parent(instance, material if slot == 1 else instances[1])
    edit.set_material_instance_vector_parameter_value(instance, "ArmorTint", u.LinearColor(r=1, g=1, b=1, a=1))
    edit.update_material_instance(instance)
    if not assets.save_loaded_asset(instance, only_if_is_dirty=False):
        raise RuntimeError("Could not save armor material slot %d" % slot)
    instances[slot] = instance

u.log("OB_ARMOR_MATERIALS_READY " + ROOT)
