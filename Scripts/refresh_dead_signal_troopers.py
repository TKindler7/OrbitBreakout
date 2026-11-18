"""Refresh saved native enemy presentation without regenerating level geometry."""
import unreal as u
import json,hashlib,shutil
from datetime import datetime,timezone
from pathlib import Path
root=Path(u.Paths.project_dir()).resolve()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
if editor.get_game_world():raise RuntimeError('Stop PIE before saving presentation')
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
if not levels.load_level('/Game/OrbitBreakout/Maps/L_DeadSignal'):raise RuntimeError('Cannot load chapter')
mapfile=root/'Content/OrbitBreakout/Maps/L_DeadSignal.umap'
backup=root/'Saved/Backups'/('BeforeTrooperMapRefresh_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
backup.mkdir(parents=True);shutil.copy2(mapfile,backup/mapfile.name)
actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
roster=[a for a in actors if isinstance(a,u.OBMachineEnemy)]
if len(roster)!=30:raise RuntimeError('Unexpected roster size '+str(len(roster)))
entries=[]
for a in roster:
 a.refresh_presentation()
 mesh=a.get_component_by_class(u.SkeletalMeshComponent)
 asset=mesh.get_skeletal_mesh_asset()
 if not asset or 'SK_SciFITrooper_Man_03' not in asset.get_path_name():raise RuntimeError('Trooper mesh missing')
 cap=a.get_component_by_class(u.CapsuleComponent)
 entries.append({'stable_id':str(a.get_editor_property('stable_id')),'kind':str(a.get_editor_property('kind')),'mesh':asset.get_path_name(),'radius':cap.get_scaled_capsule_radius(),'half_height':cap.get_scaled_capsule_half_height()})
if not levels.save_current_level():raise RuntimeError('Could not save updated trooper map')
report={'saved':True,'map':'/Game/OrbitBreakout/Maps/L_DeadSignal','roster_count':len(entries),'enemies':entries,'backup':str(backup),'map_sha256':hashlib.sha256(mapfile.read_bytes()).hexdigest(),'completed_utc':datetime.now(timezone.utc).isoformat()}
(root/'Saved/Level2/TROOPER_MAP_REFRESH.json').write_text(json.dumps(report,indent=2)+'\n')
u.log('TROOPER_MAP_REFRESH_SAVED')
