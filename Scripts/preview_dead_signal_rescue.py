"""Isolated PIE rescue-camera preview; never calls mission completion or checkpoint APIs.
Run from editor mode. Then use builtins._orbit_rescue_preview.seek(8) / seek(12).
Stop PIE to discard all preview state. Saved sequence/map assets remain untouched.
"""
import builtins
import hashlib
import json
import time
import traceback
from pathlib import Path
import unreal as u

class RescuePreview:
 def __init__(self):
  self.editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
  self.levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
  self.handle=None;self.ready=False;self.failed=False;self.sequence_player=None
  self.report={'status':'STARTING','scope':'PIE-only visual sequence sampling; no mission finalizer/checkpoint calls','samples':[]}
  self.saved=Path(u.Paths.project_saved_dir()).resolve()
  self.hashes=self.save_hashes()
 def save_hashes(self):
  return {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (self.saved/'SaveGames').glob('OB_*.sav')}
 def write(self):
  self.report['save_hashes_before']=self.hashes;self.report['save_hashes_now']=self.save_hashes()
  self.report['save_files_unchanged']=self.hashes==self.report['save_hashes_now']
  (self.saved/'Level2'/'RESCUE_VISUAL_PREVIEW.json').write_text(json.dumps(self.report,indent=2)+'\n')
 def seek(self,seconds):
  if not self.ready:raise RuntimeError('Preview is not ready')
  seconds=max(0.0,min(14.95,float(seconds)))
  params=u.MovieSceneSequencePlaybackParams()
  params.set_editor_property('time',seconds)
  params.set_editor_property('position_type',u.MovieScenePositionType.TIME)
  params.set_editor_property('update_method',u.UpdatePositionMethod.JUMP)
  self.sequence_player.set_playback_position(params)
  self.sequence_player.pause()
  camera=self.sequence_player.get_active_camera_component()
  if not camera:raise RuntimeError('Rescue sequence has no active camera at requested time')
  self.pc.set_view_target_with_blend(camera.get_owner(),0)
  position=camera.get_world_location();rotation=camera.get_world_rotation()
  sample={'seconds':seconds,'camera':camera.get_owner().get_actor_label(),'position':[position.x,position.y,position.z],'rotation':[rotation.pitch,rotation.yaw,rotation.roll],'fov':camera.field_of_view,'player_position':str(self.player.get_actor_location())}
  self.report['samples'].append(sample);self.report['status']='READY_FOR_SCREENSHOT';self.write()
  u.log('RESCUE_PREVIEW_SAMPLE '+json.dumps(sample))
  return sample
 def tick(self,dt):
  if self.ready or self.failed:return
  try:
   world=self.editor.get_game_world()
   if not world:return
   directors=u.GameplayStatics.get_all_actors_of_class(world,u.OBLevel2Director)
   player=u.GameplayStatics.get_player_character(world,0)
   if not directors or not player:return
   self.player=player;self.pc=u.GameplayStatics.get_player_controller(world,0)
   # Freeze mission evaluation without calling FinishCinematic, CompleteMission or Save.
   for director in directors:director.set_actor_tick_enabled(False)
   for actor in u.GameplayStatics.get_all_actors_of_class(world,u.LevelSequenceActor):
    seqplayer=actor.get_editor_property('sequence_player')
    if seqplayer:seqplayer.stop()
   for machine in u.GameplayStatics.get_all_actors_of_class(world,u.OBMachineEnemy):
    machine.set_actor_tick_enabled(False);machine.set_actor_hidden_in_game(True);machine.set_actor_enable_collision(False)
   player.set_actor_tick_enabled(False);player.set_actor_enable_collision(False);player.set_actor_hidden_in_game(False)
   move=player.get_component_by_class(u.CharacterMovementComponent);move.stop_movement_immediately();move.disable_movement()
   controls=u.GameplayStatics.get_all_actors_of_class(world,u.OBLevel2Interactable)
   console=next(a for a in controls if str(a.get_editor_property('stable_id'))=='L2_Transmitter')
   player.set_actor_location(console.get_actor_location()+u.Vector(-165,0,65),False,True)
   player.set_actor_rotation(u.Rotator(pitch=0,yaw=0,roll=0),True)
   mesh=player.get_component_by_class(u.SkeletalMeshComponent);mesh.set_component_tick_enabled(True)
   self.pc.set_ignore_move_input(True);self.pc.set_ignore_look_input(True)
   # Match the native ending's carrier confirmation color in this transient world.
   for actor in u.GameplayStatics.get_all_actors_of_class(world,u.PointLight):
    if actor.get_actor_label()=='T07_GeneratorCarrier':actor.point_light_component.set_light_color(u.LinearColor(.08,1,.3,1))
   sequence=u.load_asset('/Game/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Rescue')
   settings=u.MovieSceneSequencePlaybackSettings()
   settings.set_editor_property('auto_play',False);settings.set_editor_property('pause_at_end',True)
   self.sequence_player,self.sequence_actor=u.LevelSequencePlayer.create_level_sequence_player(world,sequence,settings)
   if not self.sequence_player:raise RuntimeError('Could not create rescue preview player')
   self.sequence_player.play();self.sequence_player.pause()
   self.ready=True
   if self.handle is not None:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
   # Console toggle only affects this PIE HUD and does not alter project settings.
   u.SystemLibrary.execute_console_command(world,'ShowHUD',self.pc)
   self.seek(1)
  except Exception:
   self.failed=True
   if self.handle is not None:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
   self.report['status']='ERROR';self.report['error']=traceback.format_exc();self.write();u.log_error(self.report['error'])
 def start(self):
  if self.editor.get_game_world():raise RuntimeError('Stop PIE before isolated rescue preview')
  self.handle=u.register_slate_post_tick_callback(self.tick)
  self.levels.editor_request_begin_play()

preview=RescuePreview();builtins._orbit_rescue_preview=preview;preview.start()
