"""Temporary PIE-only lineup of the four Trooper variants; no authored assets/save files edited."""
import unreal as u
import builtins,time,json,traceback
from pathlib import Path
class TrooperPreview:
 def __init__(self):
  self.editor=u.get_editor_subsystem(u.UnrealEditorSubsystem);self.levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
  self.handle=None;self.started=time.monotonic();self.done=False
 def tick(self,dt):
  if self.done:return
  try:
   w=self.editor.get_game_world()
   if not w:return
   ds=u.GameplayStatics.get_all_actors_of_class(w,u.OBLevel2Director);p=u.GameplayStatics.get_player_character(w,0)
   if not ds or not p:return
   d=ds[0]
   if not d.is_cinematic_active() and not p.is_gameplay_active():return
   if d.is_cinematic_active():d.finish_cinematic()
   p.set_actor_location(u.Vector(48000,0,110),False,True)
   d.set_actor_tick_enabled(False);p.set_actor_tick_enabled(False)
   p.set_actor_hidden_in_game(True);p.set_actor_enable_collision(False);p.get_component_by_class(u.CharacterMovementComponent).disable_movement()
   roster=u.GameplayStatics.get_all_actors_of_class(w,u.OBMachineEnemy)
   picked={}
   for m in roster:
    k=str(m.get_editor_property('kind'))
    if k not in picked:picked[k]=m
   cube=u.load_asset('/Engine/BasicShapes/Cube')
   floor=next(a for a in u.GameplayStatics.get_all_actors_of_class(w,u.StaticMeshActor) if u.Name('OB_L2_GENERATED') in a.tags)
   floor.static_mesh_component.set_mobility(u.ComponentMobility.MOVABLE);floor.set_actor_location(u.Vector(50000,500,-30),False,True)
   floor.static_mesh_component.set_static_mesh(cube);floor.set_actor_scale3d(u.Vector(35,35,.6))
   mat=u.load_asset('/Game/OrbitBreakout/Materials/M_Metal');floor.static_mesh_component.set_material(0,mat)
   report=[]
   for i,m in enumerate(picked.values()):
    m.activate();m.set_actor_tick_enabled(False)
    ai=m.get_controller()
    if ai:ai.stop_movement();ai.set_actor_tick_enabled(False)
    move=m.get_component_by_class(u.CharacterMovementComponent);move.disable_movement()
    cap=m.get_component_by_class(u.CapsuleComponent);half=cap.get_scaled_capsule_half_height()
    m.set_actor_location(u.Vector(50000,i*190,half+2),False,True);m.set_actor_rotation(u.Rotator(pitch=0,yaw=180,roll=0),True)
    mesh=m.get_component_by_class(u.SkeletalMeshComponent);mesh.set_component_tick_enabled(True)
    report.append({'kind':str(m.get_editor_property('kind')),'mesh':mesh.get_skeletal_mesh_asset().get_path_name(),'position':str(m.get_actor_location())})
   used=set()
   def spawn(cls,loc,rot=u.Rotator()):
    a=next(a for a in u.GameplayStatics.get_all_actors_of_class(w,cls) if a.get_name() not in used)
    used.add(a.get_name());a.root_component.set_mobility(u.ComponentMobility.MOVABLE)
    a.set_actor_location(loc,False,True);a.set_actor_rotation(rot,True);return a
   for pos,color,intensity in [(u.Vector(49400,200,550),u.LinearColor(1,.91,.8),11000),(u.Vector(50100,900,500),u.LinearColor(.55,.7,1),9500),(u.Vector(49500,1100,250),u.LinearColor(1,1,1),5000)]:
    light=spawn(u.PointLight,pos);c=light.point_light_component;c.set_intensity(intensity);c.set_light_color(color);c.set_attenuation_radius(2800)
   cam=spawn(u.CameraActor,u.Vector(49280,285,180),u.Rotator(pitch=-5,yaw=0,roll=0));cam.camera_component.set_field_of_view(62)
   pc=u.GameplayStatics.get_player_controller(w,0);pc.set_view_target_with_blend(cam,0)
   u.SystemLibrary.execute_console_command(w,'ShowHUD',pc)
   self.done=True
   if self.handle is not None:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
   (Path(u.Paths.project_saved_dir())/'Level2'/'TROOPER_PREVIEW.json').write_text(json.dumps({'status':'PREVIEW_READY','variants':report},indent=2))
   u.log('TROOPER_PREVIEW_READY')
  except Exception:
   self.done=True
   if self.handle is not None:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
   u.log_error(traceback.format_exc())
 def start(self):
  if self.editor.get_game_world():raise RuntimeError('Stop PIE before preview')
  self.handle=u.register_slate_post_tick_callback(self.tick);self.levels.editor_request_begin_play()
preview=TrooperPreview();builtins._orbit_trooper_preview=preview;preview.start()
