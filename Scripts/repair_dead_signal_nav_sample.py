"""Record why the north-door validation sample was on cover, then revalidate.
No layout regeneration, cover removal, nav-limit relaxation, or mission-save use.
"""
import json
import shutil
from datetime import datetime,timezone
from pathlib import Path
import unreal as u
ROOT=Path(u.Paths.project_dir()).resolve()
OUT=ROOT/'Saved'/'Level2'
report=OUT/'NAV_VALIDATION_RESULT.json'
if report.exists():
    shutil.copy2(report,OUT/('NAV_VALIDATION_BEFORE_SAMPLE_FIX_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'.json'))
world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
if '/Game/OrbitBreakout/Maps/L_DeadSignal' not in world.get_path_name():
    raise RuntimeError('Load saved L_DeadSignal before this scoped validation repair')
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
cover=next(a for a in actors if a.get_actor_label()=='T07_DefenseCover3_Collision')
o,e=cover.get_actor_bounds(False)

def project(p):
    return u.NavigationSystemV1.project_point_to_navigation(world,u.Vector(*p),None,None,u.Vector(110,110,260))

def values(p):
    return [p.x,p.y,p.z] if hasattr(p,'x') else str(p)

old=project((20700,10600,110));new=project((20700,10200,110));start=project((20650,11720,110))
data={'cover_bounds':{'center':values(o),'extent':values(e)},'old_source':[20700,10600,110],'old_projection':values(old),'new_source':[20700,10200,110],'new_projection':values(new),'start_projection':values(start),'explanation':'Old target was inside intended cover and projected onto its raised top. New target is walkable arena floor; geometry and nav query extent unchanged.'}
for name,target in [('old',old),('new',new)]:
    path=u.NavigationSystemV1.find_path_to_location_synchronously(world,start,target,None,None)
    data[name+'_path']={'valid':bool(path and path.is_valid() and not path.is_partial()),'partial':bool(path and path.is_partial()),'points':[values(p) for p in path.path_points] if path else []}
power=next((a for a in actors if a.get_actor_label()=='T05_PowerLabel'),None)
data['power_label_tag_present']=bool(power and u.Name('L2PowerLabel') in power.tags)
if power and not data['power_label_tag_present']:
    power.set_editor_property('tags',list(power.tags)+[u.Name('L2PowerLabel')])
    data['power_label_tag_added']=True
(OUT/'NORTH_DOOR_SAMPLE_DIAGNOSTIC.json').write_text(json.dumps(data,indent=2)+'\n')
u.log('DEAD_SIGNAL_NORTH_DOOR_DIAGNOSTIC '+json.dumps(data))
if not data['new_path']['valid']:
    raise RuntimeError('Actual north-door floor route is still incomplete; inspect diagnostic before editing geometry')
u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
p=ROOT/'Scripts'/'validate_dead_signal.py'
exec(compile(p.read_text(),str(p),'exec'),globals())
