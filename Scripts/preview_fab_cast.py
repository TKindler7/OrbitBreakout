"""Temporary PIE pose preview; never starts or saves a mission."""
import unreal as u
import json, traceback
from pathlib import Path
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world,'Start Play in Editor first'
p=u.GameplayStatics.get_player_character(world,0)
c=u.GameplayStatics.get_player_controller(world,0)
# Temporary runtime pose inspection only; no game-instance or checkpoint changes.
p.restore_loadout(True,24,96)
p.set_actor_location(u.Vector(150,-100,100),False,False)
p.set_actor_rotation(u.Rotator(0,0,0),False)
c.set_control_rotation(u.Rotator(-5,0,0))
c.hide_screen()
m=p.get_component_by_class(u.SkeletalMeshComponent)
report={'player_mesh':m.get_skeletal_mesh_asset().get_path_name(),'anim_class':m.get_anim_instance().get_class().get_name(),'muzzle':str(p.get_muzzle_location()),'sockets':[str(x) for x in m.get_all_socket_names() if any(s in str(x).lower() for s in ['gun','muzzle','weapon'])]}
report['enemies']=[];report['ships']=[]
for a in u.GameplayStatics.get_all_actors_of_class(world,u.OBEnemyCharacter):
    if not a.get_editor_property('heavy'):
        comp=a.get_component_by_class(u.SkeletalMeshComponent)
        report['enemies'].append({'name':a.get_name(),'mesh':comp.get_skeletal_mesh_asset().get_path_name(),'anim':comp.get_anim_instance().get_class().get_name()})
for a in u.GameplayStatics.get_all_actors_of_class(world,u.OBInteractable):
    if a.get_editor_property('kind')==u.OBInteractionKind.POD:
        report['ships']=[{'mesh':comp.get_editor_property('static_mesh').get_path_name(),'location':str(comp.get_world_location())} for comp in a.get_components_by_class(u.StaticMeshComponent) if comp.get_editor_property('static_mesh') and 'StarSparrow' in comp.get_editor_property('static_mesh').get_path_name()]
(Path(u.Paths.project_saved_dir())/'FabRuntimePreview.json').write_text(json.dumps(report,indent=2))
