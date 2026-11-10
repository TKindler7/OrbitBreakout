"""Persist native prop material Nanite permutations without regenerating content.

Run inside the OrbitBreakout Unreal Editor. Existing material graphs, instances
and map actors are preserved; only M_Metal/M_Emissive usage flags are updated.
"""
from pathlib import Path

import unreal as u


if Path(u.Paths.project_dir()).resolve().name != "OrbitBreakout":
    raise RuntimeError("Refusing material repair in a different Unreal project")

paths = ["/Game/OrbitBreakout/Materials/" + name for name in ("M_Metal", "M_Emissive")]
materials = [u.load_asset(path) for path in paths]
for path, material in zip(paths, materials):
    if not isinstance(material, u.Material):
        raise RuntimeError("Required base material is missing: " + path)

edit = u.MaterialEditingLibrary
usage = u.MaterialUsage.MATUSAGE_NANITE
for path, material in zip(paths, materials):
    edit.set_base_material_usage(material, usage, True)
    edit.recompile_material(material)
    if not edit.has_material_usage(material, usage):
        raise RuntimeError("Nanite usage was not enabled: " + path)
    if not u.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError("Nanite usage could not be saved: " + path)
    u.log("ORBIT_MATERIAL_USAGE_SAVED " + path)

u.AutomationLibrary.finish_loading_before_screenshot()
u.log("ORBIT_MATERIAL_USAGE_COMPLETE: M_Metal and M_Emissive Nanite usage persisted; map unchanged")
