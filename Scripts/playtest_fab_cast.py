"""Runs a temporary visual pose check in PIE without starting/saving a mission."""
import unreal as u
import json, traceback, time
from pathlib import Path
out=Path(u.Paths.project_saved_dir())/'FabRuntimePreview.json'
result={'status':'starting','samples':[]}
state={'started':time.monotonic(),'ready':False}
def tick(dt):
    try:
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world:
            return
        p=u.GameplayStatics.get_player_character(world,0)
        if not p:return
        c=u.GameplayStatics.get_player_controller(world,0)
        if not state['ready']:
            if str(c.get_editor_property('menu_screen'))!='Main':return
            state['ready']=True;state['started']=time.monotonic()
            p.restore_loadout(True,24,96)
            c.hide_screen()
            # Inspect the launch silhouette with bay doors raised, as after combat.
            for door in u.GameplayStatics.get_all_actors_of_class(world,u.OBDoor):
                door.set_actor_location(door.get_actor_location()+u.Vector(0,0,1000),False,False)
            m=p.get_component_by_class(u.SkeletalMeshComponent)
            state['mesh']=m
            result['player_mesh']=m.get_skeletal_mesh_asset().get_path_name()
            result['animation_class']=m.get_anim_instance().get_class().get_name()
            result['enemies']=[]
            for a in u.GameplayStatics.get_all_actors_of_class(world,u.OBEnemyCharacter):
                if not a.get_editor_property('heavy'):
                    comp=a.get_component_by_class(u.SkeletalMeshComponent)
                    result['enemies'].append({'mesh':comp.get_skeletal_mesh_asset().get_path_name(),'anim':comp.get_anim_instance().get_class().get_name()})
                    if 'guard_pos' not in state:state['guard_pos']=a.get_actor_location()
            result['ships']=[]
            for a in u.GameplayStatics.get_all_actors_of_class(world,u.OBInteractable):
                if a.get_editor_property('kind')==u.OBInteractionKind.POD:
                    result['ships']=[{'mesh':comp.get_editor_property('static_mesh').get_path_name(),'location':str(comp.get_world_location())} for comp in a.get_components_by_class(u.StaticMeshComponent) if comp.get_editor_property('static_mesh') and 'StarSparrow' in comp.get_editor_property('static_mesh').get_path_name()]
            result['status']='previewing'
        age=time.monotonic()-state['started']
        position=u.Vector(18100,-350,100) if age>40 else (state['guard_pos']+u.Vector(-300,-120,0) if age>25 else u.Vector(150,-100,100))
        p.set_actor_location(position,False,False)
        p.set_actor_rotation(u.Rotator(0,0,0),False)
        c.set_control_rotation(u.Rotator(pitch=-5,yaw=0,roll=0))
        movement=p.get_component_by_class(u.CharacterMovementComponent)
        movement.set_editor_property('velocity',u.Vector(350,120,0) if age>8 and age<25 else u.Vector())
        if len(result['samples'])<int(age):
            result['samples'].append({'time':round(age,2),'velocity':str(p.get_velocity()),'muzzle':str(p.get_muzzle_location()),'hand':str(state['mesh'].get_socket_location('hand_r'))})
        out.write_text(json.dumps(result,indent=2))
        if age>60:
            result['status']='complete';out.write_text(json.dumps(result,indent=2))
            u.unregister_slate_post_tick_callback(state['handle'])
    except Exception:
        result['status']='failed';result['error']=traceback.format_exc();out.write_text(json.dumps(result,indent=2))
        u.unregister_slate_post_tick_callback(state['handle'])
state['handle']=u.register_slate_post_tick_callback(tick)
u.get_editor_subsystem(u.LevelEditorSubsystem).editor_request_begin_play()
