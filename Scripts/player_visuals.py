"""Create or repair the textured ARC-9 shader from the installed Epic rifle textures.

Run in the full editor after Content/Weapons/Rifle has been copied. Re-running
repairs the shader's required usage flags without rebuilding its authored graph.
"""
import unreal as u

ROOT = "/Game/OrbitBreakout/Materials"
PATH = ROOT + "/M_Arc9"
tools = u.AssetToolsHelpers.get_asset_tools()
edit = u.MaterialEditingLibrary
u.EditorAssetLibrary.make_directory(ROOT)
material = u.load_asset(PATH)
if material is None:
    material = tools.create_asset("M_Arc9", ROOT, u.Material, u.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("Could not create ARC-9 material")
    color = edit.create_material_expression(material, u.MaterialExpressionTextureSampleParameter2D, -600, -160)
    color.set_editor_property("parameter_name", "WeaponColor")
    color.set_editor_property("texture", u.load_asset("/Game/Weapons/Rifle/Textures/T_Rifle_BC"))
    normal = edit.create_material_expression(material, u.MaterialExpressionTextureSampleParameter2D, -600, 180)
    normal.set_editor_property("parameter_name", "WeaponNormal")
    normal.set_editor_property("texture", u.load_asset("/Game/Weapons/Rifle/Textures/T_Rifle_N"))
    normal.set_editor_property("sampler_type", u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    tint = edit.create_material_expression(material, u.MaterialExpressionVectorParameter, -600, -380)
    tint.set_editor_property("parameter_name", "PaintTint")
    tint.set_editor_property("default_value", u.LinearColor(r=.36, g=.45, b=.52, a=1.0))
    multiply = edit.create_material_expression(material, u.MaterialExpressionMultiply, -300, -160)
    edit.connect_material_expressions(color, "RGB", multiply, "A")
    edit.connect_material_expressions(tint, "", multiply, "B")
    edit.connect_material_property(multiply, "", u.MaterialProperty.MP_BASE_COLOR)
    edit.connect_material_property(normal, "RGB", u.MaterialProperty.MP_NORMAL)
    for name, value, prop, y in (("Metallic", .55, u.MaterialProperty.MP_METALLIC, 360),
                                 ("Roughness", .36, u.MaterialProperty.MP_ROUGHNESS, 500)):
        expression = edit.create_material_expression(material, u.MaterialExpressionScalarParameter, -300, y)
        expression.set_editor_property("parameter_name", name)
        expression.set_editor_property("default_value", value)
        edit.connect_material_property(expression, "", prop)
# SM_Rifle has Nanite enabled. Persist this permutation explicitly instead of
# relying on the editor to discover it at runtime and display a default shader.
edit.set_base_material_usage(material, u.MaterialUsage.MATUSAGE_NANITE, True)
edit.recompile_material(material)
if not edit.has_material_usage(material, u.MaterialUsage.MATUSAGE_NANITE):
    raise RuntimeError("ARC-9 material Nanite usage was not enabled")
if not u.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("ARC-9 material could not be saved")
u.log("OB_PLAYER_VISUALS_READY " + PATH)
