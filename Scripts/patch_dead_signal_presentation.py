"""Scoped visual repair: exposure, two outside fills and crash camera positions.
Requires editor mode in L_DeadSignal. Does not regenerate gameplay or architecture.
"""
import ast
import hashlib
import json
import shutil
from datetime import datetime,timezone
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
if editor.get_game_world():raise RuntimeError('Stop PIE before presentation patch')
world=editor.get_editor_world()
if '/Game/OrbitBreakout/Maps/L_DeadSignal' not in world.get_path_name():raise RuntimeError('Load L_DeadSignal first')
actors=u.get_editor_subsystem(u.EditorActorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
levels.save_current_level()
stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')
backup=root/'Saved'/'Backups'/('DeadSignalPresentation_'+stamp)
for rel in ['Content/OrbitBreakout/Maps/L_DeadSignal.umap','Content/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Crash.uasset','Content/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Rescue.uasset']:
    src=root/rel;dst=backup/rel;dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
source=root/'Scripts'/'build_dead_signal.py';tree=ast.parse(source.read_text(),filename=str(source));assert isinstance(tree.body[-1],ast.Try);tree.body.pop()
ns={'__name__':'ob_dead_signal_presentation_functions'};exec(compile(tree,str(source),'exec'),ns)
owned=[a for a in actors.get_all_level_actors() if u.Name('OB_L2_GENERATED') in a.tags]
lookup={a.get_actor_label():a for a in owned}
post=lookup['DeadSignalExposure'];settings=post.get_editor_property('settings')
for key,val in [('override_auto_exposure_min_brightness',True),('override_auto_exposure_max_brightness',True),('auto_exposure_min_brightness',5.5),('auto_exposure_max_brightness',5.5)]:settings.set_editor_property(key,val)
post.set_editor_property('settings',settings)
lookup['CollapsedPortal'].set_actor_location(u.Vector(-3150,0,480),False,False)
lookup['OrbitalSun'].get_component_by_class(u.PointLightComponent).set_intensity(1800000)
lookup['OrbitalRim'].get_component_by_class(u.PointLightComponent).set_intensity(800000)
for name in ['ExteriorDescentFill','ExteriorPortalFill']:
    if name in lookup:actors.destroy_actor(lookup[name])
ns['light']('ExteriorDescentFill',(-8000,-2300,6500),(.48,.65,.85),12000,3000000,True)
ns['light']('ExteriorPortalFill',(-4700,-200,2100),(.72,.65,.52),5000,1000000)
# All these cameras and sequences are generator-owned and backed up above.
for a in owned:
    if a.get_actor_label().startswith(('CrashCam','RescueCam')):actors.destroy_actor(a)
ship=next(a for a in owned if u.Name('L2Wreck') in a.tags)
crash,rescue=ns['sequences'](ship)
director=lookup['Mission_DeadSignal'];director.set_editor_properties(dict(crash_sequence=crash,rescue_sequence=rescue))
if not levels.save_current_level():raise RuntimeError('Presentation patch map save failed')
for seq in [crash,rescue]:
    if not u.EditorAssetLibrary.save_loaded_asset(seq,False):raise RuntimeError('Presentation sequence save failed')
result={'saved':True,'backup':str(backup),'exposure_ev100':5.5,'changes':['Moved impact camera out of slope mesh bounds','Corrected descent look-at targets to actual ship positions','Interior crash shot begins at24 seconds','Collapsed entrance cap moved aft to exterior portal','Added exterior descent and portal illumination','Reduced orbital key/rim intensity'],'runtime_visual_verification':'pending','sequences':ns['REPORT']['sequences'],'map_sha256':hashlib.sha256((root/'Content/OrbitBreakout/Maps/L_DeadSignal.umap').read_bytes()).hexdigest()}
(root/'Saved'/'Level2'/'PRESENTATION_PATCH_RESULT.json').write_text(json.dumps(result,indent=2)+'\n')
u.log('DEAD_SIGNAL_PRESENTATION_PATCH_SAVED '+json.dumps(result))
