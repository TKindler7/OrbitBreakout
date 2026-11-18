"""Author missing pilot and loader profiles. Existing designer edits are preserved."""
import unreal as u

ROOT = '/Game/OrbitBreakout/Tuning'
tools = u.AssetToolsHelpers.get_asset_tools()
for name, cls, values in [
    ('DA_OBParkourTuning', u.OBParkourTuning, {}),
    ('DA_Arc9Sight', u.OBWeaponSightProfile, {}),
    ('DA_Helix12Sight', u.OBWeaponSightProfile, {'offset': u.Vector(44, 0, -20.4), 'field_of_view': 62.0}),
    ('DA_OBMechTuning', u.OBMechTuning, {}),
]:
    path = ROOT + '/' + name
    if u.EditorAssetLibrary.does_asset_exist(path):
        if not isinstance(u.EditorAssetLibrary.load_asset(path), cls):
            raise RuntimeError('Existing asset has a different class at ' + path)
        continue
    factory = u.DataAssetFactory()
    factory.set_editor_property('data_asset_class', cls)
    asset = tools.create_asset(name, ROOT, cls, factory)
    if not asset:
        raise RuntimeError('Could not create ' + path)
    asset.set_editor_properties(values)
    if not u.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Could not save ' + path)
    u.log('OB_TUNING_CREATED ' + path)
