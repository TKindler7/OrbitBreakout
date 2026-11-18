"""Retarget the existing rifle set onto the imported trooper skeleton. Run in UE editor."""
import unreal as u
import json, traceback
from pathlib import Path
report = Path(u.Paths.project_saved_dir()) / 'FabUpgradeReport.json'
try:
    tools=u.AssetToolsHelpers.get_asset_tools()
    root='/Game/OrbitBreakout/Animations'
    def create(name, cls, factory):
        return u.load_asset(root+'/'+name) or tools.create_asset(name,root,cls,factory())
    source=u.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
    target=u.load_asset('/Game/SciFITrooper_Man_03/SkeletalMesh/SK_SciFITrooper_Man_03')
    assert source and target
    rigs=[]
    for name, mesh in [('IK_SourceManny',source),('IK_TargetTrooper',target)]:
        rig=create(name,u.IKRigDefinition,u.IKRigDefinitionFactory)
        c=u.IKRigController.get_controller(rig)
        c.set_skeletal_mesh(mesh)
        assert c.apply_auto_generated_retarget_definition(), name
        rigs.append(rig)
    retarget=create('RTG_Manny_Trooper',u.IKRetargeter,u.IKRetargetFactory)
    c=u.IKRetargeterController.get_controller(retarget)
    c.set_ik_rig(u.RetargetSourceOrTarget.SOURCE,rigs[0])
    c.set_ik_rig(u.RetargetSourceOrTarget.TARGET,rigs[1])
    c.add_default_ops()
    c.auto_map_chains(u.AutoMapChainType.FUZZY,True)
    assets=[]
    for path in u.EditorAssetLibrary.list_assets('/Game/Characters/Mannequins/Anims',recursive=True):
        a=u.load_asset(path)
        if isinstance(a,u.AnimSequence): assets.append(u.EditorAssetLibrary.find_asset_data(path))
    inputs=u.IKRetargetBatchOperationInputs()
    for key,value in dict(assets_to_retarget=assets,source_mesh=source,target_mesh=target,ik_retarget_asset=retarget,target_path=root+'/Trooper',include_referenced_assets=True,overwrite_existing_files=True).items():
        inputs.set_editor_property(key,value)
    output=u.IKRetargetBatchOperation.run_batch_retarget(inputs)
    assert output, 'No retargeted animations'
    u.EditorAssetLibrary.save_directory(root,only_if_is_dirty=False,recursive=True)
    report.write_text(json.dumps({'success':True,'animations':len(output)},indent=2))
except Exception:
    report.write_text(json.dumps({'success':False,'error':traceback.format_exc()},indent=2))
    raise
