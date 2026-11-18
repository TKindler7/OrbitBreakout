"""Validate skeleton-compatible character sets and apply imported meshes to saved level."""
import unreal as u
import json, traceback
from pathlib import Path
out=Path(u.Paths.project_saved_dir())/'FabCastValidation.json'
r={}
try:
    paths={
        'wraith':'/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith',
        'trooper':'/Game/SciFITrooper_Man_03/SkeletalMesh/SK_SciFITrooper_Man_03',
        'ship':'/Game/StarSparrow/Meshes/Examples/SM_StarSparrow01'}
    assets={k:u.load_asset(v) for k,v in paths.items()}
    assert all(assets.values()),'An imported mesh is missing'
    r['meshes']={k:v.get_path_name() for k,v in assets.items()}
    root='/Game/ParagonWraith/Characters/Heroes/Wraith/Animations/'
    names=['Idle_NonCombat','Idle_Ability_RMB','Fire_A_Fast_V1','Jump_Apex','Death_Backward']
    for direction in ['Fwd','Right','Bwd','Left']:
        names += ['Jog_'+direction,'Ability_RMB_Walk_'+direction,'Locomotion_Combat/Jog_'+direction+'_Combat']
    for key,paths_to_check in [('wraith',[root+n for n in names]),('trooper',u.EditorAssetLibrary.list_assets('/Game/OrbitBreakout/Animations/Trooper'))]:
        checked=[]
        sk=assets[key].get_editor_property('skeleton')
        for path in paths_to_check:
            seq=u.load_asset(path)
            assert isinstance(seq,u.AnimSequence),'Missing sequence '+path
            assert seq.get_editor_property('skeleton')==sk,key+' mismatched '+seq.get_name()
            assert seq.get_play_length()>0,key+' empty clip'
            checked.append(seq.get_name())
        r[key+'_animations']=checked
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    guards=[]; bosses=[]; ships=[]
    for a in actors:
        if isinstance(a,u.OBEnemyCharacter):
            if a.get_editor_property('heavy'): bosses.append(a.get_actor_label())
            else:
                a.get_component_by_class(u.SkeletalMeshComponent).set_skeletal_mesh_asset(assets['trooper'])
                guards.append(a.get_actor_label())
        if isinstance(a,u.OBInteractable) and a.get_editor_property('kind')==u.OBInteractionKind.POD:
            a.set_editor_property('kind',u.OBInteractionKind.POD)
            ships.append(a.get_actor_label())
    assert len(guards)==15 and len(bosses)==1,(len(guards),len(bosses))
    assert len(ships)==1,ships
    r.update(guards=guards,bosses=bosses,ships=ships)
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    r['success']=True
except Exception:
    r.update(success=False,error=traceback.format_exc())
    raise
finally:
    out.write_text(json.dumps(r,indent=2))
