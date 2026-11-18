"""Move only freight reinforcement205 behind the transfer-bend wall.
Run in editor after PIE stops. Verify architectural occlusion and floor navigation
before saving; preserve a pre-patch map, builder and manifest. No enemy/door logic
or encounter counts are changed.
"""
import json
import math
import shutil
from datetime import datetime,timezone
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
if editor.get_game_world():raise RuntimeError('Stop PIE before freight spawn patch')
world=editor.get_editor_world()
if '/Game/OrbitBreakout/Maps/L_DeadSignal' not in world.get_path_name():raise RuntimeError('Load saved L_DeadSignal first')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
roster=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=='OBMachineEnemy']
targets=[a for a in roster if int(a.get_editor_property('encounter_id'))==205]
if len(targets)!=1:raise RuntimeError('Expected exactly one freight205 machine')
machine=targets[0]
if u.Name('OB_L2_GENERATED') not in machine.tags:raise RuntimeError('205 is not generator-owned')
original=machine.get_actor_location()
new=u.Vector(12500,3950,110)

def xyz(p):return [float(p.x),float(p.y),float(p.z)]
def project(p):
    raw=u.NavigationSystemV1.project_point_to_navigation(world,p,None,None,u.Vector(90,90,80))
    if hasattr(raw,'x'):return raw
    if isinstance(raw,tuple) and not any(x is False for x in raw):return next((x for x in raw if hasattr(x,'x')),None)
    return None

def sight(source,destination):
    raw=u.SystemLibrary.line_trace_single(world,source,destination,u.TraceTypeQuery.ECC_VISIBILITY,False,roster,u.DrawDebugTrace.NONE,True)
    hit=raw if isinstance(raw,u.HitResult) else next((v for v in raw if isinstance(v,u.HitResult)),None) if isinstance(raw,tuple) else None
    if hit is None:return {'blocked':False}
    parts=hit.to_dict()
    actor=parts['hit_actor']
    return {'blocked':bool(parts['blocking_hit']),'hit_actor':actor.get_actor_label() if actor else None,'impact':xyz(parts['impact_point'])}

start=project(u.Vector(12500,3950,10));end=project(u.Vector(10800,3000,10))
if start is None or end is None:raise RuntimeError('Candidate or freight floor has no nav projection')
path=u.NavigationSystemV1.find_path_to_location_synchronously(world,start,end,None,None)
if not path or not path.is_valid() or path.is_partial():raise RuntimeError('Candidate cannot navigate into the freight fight; map unchanged')
samples={}
for label,pos in [('gallery_center',(9700,3000,240)),('gallery_north_lane',(10000,3800,240)),('gallery_south_lane',(10000,2300,240)),('gallery_forward',(11000,3000,240))]:
    samples[label]={'old':sight(u.Vector(*pos),original+u.Vector(0,0,45)),'new':sight(u.Vector(*pos),new+u.Vector(0,0,45))}
if not samples['gallery_center']['new']['blocked']:
    raise RuntimeError('Transfer-bend wall does not occlude candidate from gallery center; map unchanged')
if not levels.save_current_level():raise RuntimeError('Could not save baseline before backup')
stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
backup=root/'Saved'/'Backups'/('DeadSignalFreightSpawn_'+stamp)
for rel in ['Content/OrbitBreakout/Maps/L_DeadSignal.umap','Scripts/build_dead_signal.py','docs/LEVEL_2_LAYOUT_MANIFEST.json']:
    src=root/rel;dst=backup/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
machine.set_actor_location(new,False,False)
# A readable industrial bay sign identifies the source of the arrival cue.
name='T04_BulwarkMaintenanceSign'
sign=next((a for a in actors.get_all_level_actors() if a.get_actor_label()==name and u.Name('OB_L2_GENERATED') in a.tags),None)
if not sign:
    sign=actors.spawn_actor_from_class(u.TextRenderActor,u.Vector(11540,3850,430),u.Rotator(pitch=0,yaw=180,roll=0))
    sign.set_actor_label(name);sign.set_editor_property('tags',[u.Name('OB_L2_GENERATED')]);sign.set_folder_path(u.Name('DeadSignal/Signage'))
comp=sign.text_render;comp.set_text('AUTOMATED MAINTENANCE // BAY04');comp.set_world_size(24);comp.set_text_render_color(u.Color(r=255,g=190,b=95,a=255));comp.set_horizontal_alignment(u.HorizTextAligment.EHTA_CENTER)
if not levels.save_current_level():raise RuntimeError('Freight location/sign could not be saved')
builder=root/'Scripts'/'build_dead_signal.py';source=builder.read_text()
old="machine('BULWARK',205,1,(11950,3500,110))"
replacement="machine('BULWARK',205,1,(12500,3950,110))"
if old in source:source=source.replace(old,replacement)
elif replacement not in source:raise RuntimeError('Unexpected205 builder declaration; update source to match saved map')
line="    text('T04_BulwarkMaintenanceSign','AUTOMATED MAINTENANCE // BAY04',(11540,3850,430),180,24,(255,190,95))"
if "text('T04_BulwarkMaintenanceSign'" not in source:source=source.replace('    '+replacement,'    '+replacement+'\n'+line)
builder.write_text(source)
manifest_path=root/'docs'/'LEVEL_2_LAYOUT_MANIFEST.json';manifest=json.loads(manifest_path.read_text())
entries=[e for e in manifest['machines'] if e['encounter']==205]
if len(entries)!=1:raise RuntimeError('Unexpected manifest205 roster')
entries[0]['position']=[12500,3950,110]
manifest_path.write_text(json.dumps(manifest,indent=2)+'\n')
report={'saved':True,'backup':str(backup),'stable_id':str(machine.get_editor_property('stable_id')),'old_location':xyz(original),'new_location':xyz(new),'new_floor_projection':xyz(start),'connected_to_freight':True,'path_length_cm':path.get_path_length(),'path_points':[xyz(p) for p in path.path_points],'architectural_sightlines':samples,'limits':'Occlusion verified from representative positions; forward/low-lane angles may see the bay. Native spawn visibility/distance guard remains unchanged. Runtime arrival timing must be observed separately.','builder_and_manifest_updated':True,'completed_utc':datetime.now(timezone.utc).isoformat()}
(root/'Saved'/'Level2'/'FREIGHT_SPAWN_PATCH_RESULT.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('DEAD_SIGNAL_FREIGHT_SPAWN_PATCH_SAVED '+json.dumps(report))
