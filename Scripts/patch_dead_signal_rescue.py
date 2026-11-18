"""Repair only the saved 15-second rescue sequence and its three cameras.
Editor-only, backed up before mutation. Gameplay, crash sequence and lighting are untouched.
"""
import ast
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal as u

root=Path(u.Paths.project_dir()).resolve()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
if editor.get_game_world():raise RuntimeError('Stop PIE before rescue camera repair')
world=editor.get_editor_world()
if '/Game/OrbitBreakout/Maps/L_DeadSignal' not in world.get_path_name():raise RuntimeError('Load saved L_DeadSignal')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
source_path=root/'Scripts'/'build_dead_signal.py'
source=source_path.read_text()
shots=[
 dict(name='RescueCam01_Call',start=0,end=6,cam_from=(21800,10050,390),cam_to=(21700,9920,360),look_from=(21170,9500,205),look_to=(21170,9500,205),fov=62,tags=('L2RescueCamera',)),
 dict(name='RescueCam02_Carrier',start=6,end=11,cam_from=(21680,8850,300),cam_to=(21580,8940,280),look_from=(21160,9500,225),look_to=(21160,9500,225),fov=58,tags=()),
 dict(name='RescueCam03_Response',start=11,end=15,cam_from=(21590,9770,260),cam_to=(21510,9740,245),look_from=(21135,9500,230),look_to=(21135,9500,230),fov=54,tags=()),
]
def xyz(p):return [float(p.x),float(p.y),float(p.z)]
def mix(a,b,t):return tuple(x+(y-x)*t for x,y in zip(a,b))
roster=actors.get_all_level_actors()
owned=[a for a in roster if u.Name('OB_L2_GENERATED') in a.tags]
existing={a.get_actor_label():a for a in owned}
for shot in shots:
 if shot['name'] not in existing:raise RuntimeError('Missing owned rescue camera '+shot['name'])
# Inspect actual visible architecture, including noncolliding decorative beams.
geometry=[]
for a in roster:
 if not isinstance(a,u.StaticMeshActor) or a.get_editor_property('hidden'):continue
 component=a.static_mesh_component
 if not component.is_visible() or not component.static_mesh:continue
 center,extent=a.get_actor_bounds(False)
 geometry.append((a.get_actor_label(),xyz(center),xyz(extent)))
probes=[]
for shot in shots:
 for alpha in [0,.25,.5,.75,1]:
  p=mix(shot['cam_from'],shot['cam_to'],alpha)
  overlaps=[label for label,c,e in geometry if all(abs(p[i]-c[i])<e[i]+12 for i in range(3))]
  probes.append(dict(camera=shot['name'],alpha=alpha,position=p,visible_mesh_bounds_within_12cm=overlaps))
  if overlaps:raise RuntimeError('Camera clearance failed: '+str(probes[-1]))
# Native BeginCinematic(true) places Wraith at Transmitter + (-165,0,65), yaw0.
console=existing['L2_Transmitter']
expected_player=console.get_actor_location()+u.Vector(-165,0,65)
if any(abs(a-b)>1 for a,b in zip(xyz(expected_player),(21135,9500,175))):raise RuntimeError('Transmitter moved; reframe Wraith before applying')
if not levels.save_current_level():raise RuntimeError('Could not save baseline')
stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
backup=root/'Saved'/'Backups'/('DeadSignalRescue_'+stamp)
files=['Content/OrbitBreakout/Maps/L_DeadSignal.umap','Content/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Rescue.uasset','Scripts/build_dead_signal.py']
for rel in files:
 src=root/rel;dst=backup/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
crash_path=root/'Content/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Crash.uasset'
crash_hash=hashlib.sha256(crash_path.read_bytes()).hexdigest()
# Import only construction helpers, never call build() or regenerate actors.
tree=ast.parse(source,filename=str(source_path));assert isinstance(tree.body[-1],ast.Try);tree.body.pop()
ns={'__name__':'ob_rescue_camera_helpers'};exec(compile(tree,str(source_path),'exec'),ns)
seq=ns['clean_sequence']('LS_DeadSignal_Rescue',15)
for shot in shots:actors.destroy_actor(existing[shot['name']])
for shot in shots:ns['shot'](seq,**shot)
existing['Mission_DeadSignal'].set_editor_property('rescue_sequence',seq)
if not u.EditorAssetLibrary.save_loaded_asset(seq,False):raise RuntimeError('Could not save rescue sequence')
if not levels.save_current_level():raise RuntimeError('Could not save rescue camera actors')
# Keep regeneration reproducible without touching the crash block.
start=source.index("    rescue=clean_sequence('LS_DeadSignal_Rescue',15)")
end=source.index('    for seq in [crash,rescue]:',start)
lines=["    rescue=clean_sequence('LS_DeadSignal_Rescue',15)"]
for s in shots:
 lines.append("    shot(rescue,%r,%s,%s,%r,%r,%r,%r,%s,%r)"%(s['name'],s['start'],s['end'],s['cam_from'],s['cam_to'],s['look_from'],s['look_to'],s['fov'],s['tags']))
source=source[:start]+'\n'.join(lines)+'\n'+source[end:]
ast.parse(source);source_path.write_text(source)
if hashlib.sha256(crash_path.read_bytes()).hexdigest()!=crash_hash:raise RuntimeError('Unexpected crash sequence change')
report=dict(status='SAVED_AWAITING_VISUAL_CHECK',saved=True,backup=str(backup),sequence=seq.get_path_name(),duration_seconds=15,camera_cuts=3,shots=shots,clearance_probes=probes,expected_native_rescue_player=xyz(expected_player),crash_sequence_sha256_unchanged=True,gameplay_actors_unchanged=True,scope='Only rescue cameras, rescue sequence and matching builder shot declarations changed; existing lighting retained.',completed_utc=datetime.now(timezone.utc).isoformat())
(root/'Saved'/'Level2'/'RESCUE_CAMERA_PATCH_RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('DEAD_SIGNAL_RESCUE_CAMERA_PATCH_SAVED '+json.dumps(report))
