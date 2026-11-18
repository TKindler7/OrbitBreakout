"""Watch the real crash and gameplay handoff in PIE, sampling views and screenshots.
No mission-state shortcuts. The direct-map development checkpoint can be written by
its normal finalizer; campaign save hashes are checked. Stop PIE after inspection.
"""
import builtins
import hashlib
import json
import math
import time
import traceback
from datetime import datetime,timezone
from pathlib import Path
import unreal as u

class CrashMotionPreview:
 def __init__(self):
  self.editor=u.get_editor_subsystem(u.UnrealEditorSubsystem);self.levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
  self.saved=Path(u.Paths.project_saved_dir()).resolve();self.output=self.saved/'Level2'/'CrashMotionFrames'
  self.output.mkdir(parents=True,exist_ok=True)
  self.report_path=self.saved/'Level2'/'CRASH_MOTION_PREVIEW.json'
  self.patch=json.loads((self.saved/'Level2'/'CRASH_MOTION_PATCH_RESULT.json').read_text())
  if not self.patch.get('status','').startswith('SAVED'):raise RuntimeError('Apply and save crash asset patch before preview')
  self.handle=None;self.started=time.monotonic();self.done=False;self.busy=False;self.world=None;self.director=None
  self.targets=[1.,5.2,11.2,18.2,22.3,24.6,27.4,33.8,34.3,35.2,36.5,40.5,41.8]
  self.next_sample=0;self.finish_at=None;self.before=self.campaign_hash()
  self.report={'status':'RUNNING','started_utc':datetime.now(timezone.utc).isoformat(),'scope':'Actual native42-second crash plus gameplay handoff; screenshots require independent visual inspection','samples':[],'errors':[],'campaign_hash_before':self.before}
 def campaign_hash(self):
  p=self.saved/'SaveGames'/'OB_EmergencyDeparture.sav'
  return hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None
 def write(self):self.report_path.write_text(json.dumps(self.report,indent=2)+'\n')
 def stop(self):
  if self.handle is not None:u.unregister_slate_post_tick_callback(self.handle);self.handle=None
 def sample(self,label,requested):
  pc=u.GameplayStatics.get_player_controller(self.world,0);manager=pc.player_camera_manager
  pos=manager.get_camera_location();rot=manager.get_camera_rotation();target=pc.get_view_target()
  player=u.GameplayStatics.get_player_character(self.world,0)
  wreck=next(a for a in u.GameplayStatics.get_all_actors_of_class(self.world,u.StaticMeshActor) if u.Name('L2Wreck') in a.tags)
  screenshot=self.output/(label+'.png')
  task=u.AutomationLibrary.take_high_res_screenshot(1280,720,str(screenshot))
  sample={'label':label,'requested_seconds':requested,'actual_cinematic_seconds':self.director.get_cinematic_elapsed(),'cinematic_active':self.director.is_cinematic_active(),
   'view_target':target.get_name() if target else None,'camera_position':[pos.x,pos.y,pos.z],'camera_rotation':[rot.pitch,rot.yaw,rot.roll],'fov':manager.get_fov_angle(),
   'player_position':[player.get_actor_location().x,player.get_actor_location().y,player.get_actor_location().z],
   'player_hidden':player.get_editor_property('hidden'),'gameplay_active':player.is_gameplay_active(),
   'wreck_location':[wreck.get_actor_location().x,wreck.get_actor_location().y,wreck.get_actor_location().z],
   'screenshot_requested':str(screenshot),'screenshot_task_created':bool(task)}
  self.report['samples'].append(sample);self.write();u.log('CRASH_MOTION_PREVIEW_SAMPLE '+json.dumps(sample))
  return sample
 def tick(self,dt):
  if self.done or self.busy:return
  self.busy=True
  try:
   if time.monotonic()-self.started>100:raise RuntimeError('Crash preview timed out')
   self.world=self.editor.get_game_world()
   if not self.world:return
   if not self.director:
    ds=u.GameplayStatics.get_all_actors_of_class(self.world,u.OBLevel2Director)
    if not ds:return
    self.director=ds[0]
   if not u.GameplayStatics.get_player_character(self.world,0):return
   elapsed=self.director.get_cinematic_elapsed()
   if self.next_sample<len(self.targets) and self.director.is_cinematic_active() and elapsed>=self.targets[self.next_sample]:
    requested=self.targets[self.next_sample];self.sample('crash_%05.1f'%requested,requested);self.next_sample+=1
   if not self.director.is_cinematic_active() and elapsed>=41.5:
    if self.finish_at is None:self.finish_at=time.monotonic()
    if time.monotonic()-self.finish_at>=1.:
     sample=self.sample('handoff_after_1s',43.)
     self.report['campaign_hash_after']=self.campaign_hash();self.report['campaign_unchanged']=self.report['campaign_hash_after']==self.before
     self.report['technical_checks']={
      'native_intro_finished':not sample['cinematic_active'],
      'gameplay_active':sample['gameplay_active'],
      'player_visible':not sample['player_hidden'],
      'player_grounded':abs(sample['player_position'][2]-98.)<1.,
      'camera_matches_grounded_endpoint':math.dist(sample['camera_position'],self.patch['new_recovery_camera_endpoint'])<12.,
      'camera_fov_matches':abs(sample['fov']-85.)<1.,
      'wreck_at_saved_final':math.dist(sample['wreck_location'],self.patch['new_wreck_location'])<.5,
      'campaign_unchanged':self.report['campaign_unchanged']}
     self.report['status']='TECHNICAL_PASS_VISUAL_REVIEW_PENDING' if all(self.report['technical_checks'].values()) else 'TECHNICAL_FAIL'
     self.report['completed_utc']=datetime.now(timezone.utc).isoformat();self.done=True;self.stop();self.write()
     u.log('CRASH_MOTION_PREVIEW_FINISHED '+json.dumps(self.report['technical_checks']))
  except Exception:
   self.done=True;self.stop();self.report['status']='ERROR';self.report['errors'].append(traceback.format_exc());self.write();u.log_error(self.report['errors'][-1])
  finally:self.busy=False
 def start(self):
  if self.editor.get_game_world():raise RuntimeError('Stop PIE before native crash preview')
  self.write();self.handle=u.register_slate_post_tick_callback(self.tick);self.levels.editor_request_begin_play()

preview=CrashMotionPreview();builtins._orbit_crash_motion_preview=preview;preview.start()
