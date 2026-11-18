"""Apply the crash-only animation repair in a full editor or Python commandlet.
Backs up saved map/crash sequence before edits. Does not regenerate the chapter,
rescue sequence, roster, controls, collisions, navigation or checkpoint positions.
"""
import ast
import hashlib
import json
import math
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve()
MAP='/Game/OrbitBreakout/Maps/L_DeadSignal'
MAP_FILE=ROOT/'Content/OrbitBreakout/Maps/L_DeadSignal.umap'
CRASH_FILE=ROOT/'Content/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Crash.uasset'
RESCUE_FILE=ROOT/'Content/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Rescue.uasset'
REPORT_FILE=ROOT/'Saved/Level2/CRASH_MOTION_PATCH_RESULT.json'
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
if editor.get_game_world():raise RuntimeError('Stop PIE before crash repair')
world=editor.get_editor_world()
if not world or MAP not in world.get_path_name():
 world=u.EditorLoadingAndSavingUtils.load_map(str(MAP_FILE))
if not world or MAP not in world.get_path_name():raise RuntimeError('Could not load saved Dead Signal map')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
roster=list(actors.get_all_level_actors())
owned={a.get_actor_label():a for a in roster if u.Name('OB_L2_GENERATED') in a.tags}
for name in ['StarSparrow_Wreck','CrashEngineFire','Mission_DeadSignal','T01_CrashChamber_Floor']:
 if name not in owned:raise RuntimeError('Missing generated actor '+name)
ship=owned['StarSparrow_Wreck'];light=owned['CrashEngineFire']
def xyz(p):return [float(p.x),float(p.y),float(p.z)]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
old_location=xyz(ship.get_actor_location());old_light=xyz(light.get_actor_location())
rotation=ship.get_actor_rotation();scale=xyz(ship.get_actor_scale3d())
mesh_bounds=ship.static_mesh_component.static_mesh.get_bounds()
mesh_origin=xyz(mesh_bounds.origin);mesh_extent=xyz(mesh_bounds.box_extent)
source=ROOT/'Scripts/build_dead_signal.py';tree=ast.parse(source.read_text(),filename=str(source))
assert isinstance(tree.body[-1],ast.Try);tree.body.pop()
namespace={'__name__':'dead_signal_crash_repair_helpers'};exec(compile(tree,str(source),'exec'),namespace)
old_scale=tuple(scale)
placement=namespace['crash_wreck_placement'](mesh_origin,mesh_extent)
scale=placement['scale'];new_location=list(placement['position'])
new_light=[new_location[0]-450,new_location[1],new_location[2]]
center,extent=owned['T01_CrashChamber_Floor'].get_actor_bounds(False)
if abs(center.z+extent.z)>.1:raise RuntimeError('Crash floor height changed; recompute grounded camera endpoint')
# Visible mesh bounds matter here: the wreck deliberately has no collision.
p,y,r=map(math.radians,[rotation.pitch,rotation.yaw,rotation.roll])
cp,sp,cy,sy,cr,sr=math.cos(p),math.sin(p),math.cos(y),math.sin(y),math.cos(r),math.sin(r)
axes=[(cp*cy,cp*sy,sp),(sr*sp*cy-cr*sy,sr*sp*sy+cr*cy,-sr*cp),(-(cr*sp*cy+sr*sy),cy*sr-cr*sp*sy,cr*cp)]
def inside_wreck(point,location,margin=12):
 offset=[point[i]-location[i] for i in range(3)]
 local=[sum(offset[j]*axis[j] for j in range(3))/scale[i] for i,axis in enumerate(axes)]
 return all(abs(local[i]-mesh_origin[i])<=mesh_extent[i]+margin/scale[i] for i in range(3))
geometry=[]
for actor in roster:
 if actor==ship or not isinstance(actor,u.StaticMeshActor) or actor.get_editor_property('hidden'):continue
 component=actor.static_mesh_component
 if not component.is_visible() or not component.static_mesh:continue
 c,e=actor.get_actor_bounds(False);geometry.append((actor.get_actor_label(),xyz(c),xyz(e)))
pitch=math.radians(-8.)
endpoint=(-320*math.cos(pitch)-65*math.sin(pitch),35.,98.-320*math.sin(pitch)+65*math.cos(pitch))
probes=[]
for index in range(27):
 alpha=index/26.;ease=alpha*alpha*(3-2*alpha)
 point=tuple(a+(b-a)*ease for a,b in zip((-380,480,260),endpoint))
 overlaps=[label for label,c,e in geometry if all(abs(point[i]-c[i])<=e[i]+12 for i in range(3))]
 if inside_wreck(point,new_location):overlaps.append('StarSparrow_Wreck (proposed oriented bounds)')
 probes.append({'alpha':alpha,'position':point,'visible_geometry_within_12cm':overlaps})
 if overlaps:raise RuntimeError('Recovery camera clearance failed: '+str(probes[-1]))
# Capture all gameplay identities/positions to establish the deliberately narrow scope.
def gameplay_snapshot():
 return sorted((a.get_actor_label(),xyz(a.get_actor_location())) for a in actors.get_all_level_actors()
  if a.get_class().get_name() in ['OBMachineEnemy','OBLevel2Interactable','OBZoneTrigger','OBDoor'])
before_gameplay=gameplay_snapshot();rescue_hash=sha(RESCUE_FILE)
stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
backup=ROOT/'Saved/Backups'/('DeadSignalCrashMotion_'+stamp)
for path in [MAP_FILE,CRASH_FILE,ROOT/'Scripts/build_dead_signal.py']:
 destination=backup/path.relative_to(ROOT);destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,destination)
source=ROOT/'Scripts/build_dead_signal.py';tree=ast.parse(source.read_text(),filename=str(source))
assert isinstance(tree.body[-1],ast.Try);tree.body.pop()
namespace={'__name__':'dead_signal_crash_repair_helpers'};exec(compile(tree,str(source),'exec'),namespace)
ship.set_actor_scale3d(u.Vector(*scale))
ship.set_actor_location(u.Vector(*new_location),False,False)
light.set_actor_location(u.Vector(*new_light),False,False)
for actor in roster:
 if u.Name('OB_L2_GENERATED') in actor.tags and actor.get_actor_label().startswith('CrashCam'):
  actors.destroy_actor(actor)
crash=namespace['crash_sequence'](ship)
owned['Mission_DeadSignal'].set_editor_property('crash_sequence',crash)
if gameplay_snapshot()!=before_gameplay:raise RuntimeError('Unexpected gameplay actor change; do not save')
if not u.EditorAssetLibrary.save_loaded_asset(crash,False):raise RuntimeError('Could not save crash sequence')
if not u.EditorLoadingAndSavingUtils.save_map(world,MAP):raise RuntimeError('Could not save repaired crash map')
if sha(RESCUE_FILE)!=rescue_hash:raise RuntimeError('Unexpected rescue sequence change')
keys=namespace['crash_motion_keys'](tuple(new_location),(rotation.roll,rotation.pitch,rotation.yaw),tuple(scale))
speed_samples=[]
for seconds in [22,24,24.6,25,26,27,27.2,42]:
 i=min(range(len(keys)),key=lambda i:abs(keys[i][0]-seconds));time,pos,_,_=keys[i]
 speed=math.dist(pos,keys[i+1][1])/(keys[i+1][0]-time) if i+1<len(keys) else 0
 speed_samples.append({'seconds':time,'position':pos,'speed_cm_per_second':speed})
report={'status':'SAVED_AWAITING_RUNTIME_VISUAL_CHECK','completed_utc':datetime.now(timezone.utc).isoformat(),'backup':str(backup),
 'changes':['Restored uniform spacecraft proportions and placed wreck clear of recovery cameras','Replaced rigid final slide/stop with explicit per-frame deceleration','Reduced interior camera travel','Matched final camera position/rotation/FOV to grounded pawn and held40.5–42s'],
 'old_wreck_location':old_location,'new_wreck_location':new_location,'old_engine_light':old_light,'new_engine_light':new_light,
 'prior_recovery_endpoint_inside_wreck_bounds':inside_wreck((-360,120,240),old_location,0),
 'prior_default_gameplay_camera_inside_wreck_bounds':inside_wreck(endpoint,old_location,0),
 'new_recovery_camera_endpoint':endpoint,'new_recovery_rotation':[0,-8,0],'new_recovery_fov':85,'stationary_camera_hold_seconds':[40.5,42],
 'wreck_stops_at_seconds':27.2,'ship_motion_key_count':len(keys),'ship_speed_samples':speed_samples,'recovery_clearance_probes':probes,
 'gameplay_actor_positions_unchanged':True,'rescue_sequence_sha256_unchanged':True,
 'map_sha256':sha(MAP_FILE),'crash_sequence_sha256':sha(CRASH_FILE),'rescue_sequence_sha256':rescue_hash,
 'native_dependency':'Companion director fix must ground actual crash-recovery pawn atfloor+capsulehalfheight+2 and manage recovery visibility/pose; this asset patch does not edit native behavior.',
 'limits':['Clearance checks use actual visible-mesh bounds, not triangle intersections.','Saved assets and analytic motion checks do not establish visual quality or continuous runtime handoff.']}
REPORT_FILE.parent.mkdir(parents=True,exist_ok=True);REPORT_FILE.write_text(json.dumps(report,indent=2)+'\n')
u.log('DEAD_SIGNAL_CRASH_MOTION_PATCH_SAVED '+json.dumps(report))
