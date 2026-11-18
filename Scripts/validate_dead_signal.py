"""Validate the saved Dead Signal map after reopening the editor.

No actor generation, deletion, cinematic editing, save-slot access or engine
settings changes. Uses the saved layout manifest and the same geometry checks as
the authoring script. Nav rebuilding is requested after actual editor ticks,
with bounded retries while normal asset compilation releases its load lock.
"""
import ast
import json
import hashlib
import shutil
import time
import traceback
from datetime import datetime, timezone
from pathlib import Path
import unreal as u

_ROOT=Path(u.Paths.project_dir()).resolve()
_OUT=_ROOT/'Saved'/'Level2'
_OUT.mkdir(parents=True,exist_ok=True)
_PREVIOUS_RESULT=_OUT/'NAV_VALIDATION_RESULT.json'
if _PREVIOUS_RESULT.exists():
    shutil.copy2(_PREVIOUS_RESULT,_OUT/('NAV_VALIDATION_PRIOR_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')+'.json'))
_MANIFEST=json.loads((_ROOT/'docs'/'LEVEL_2_LAYOUT_MANIFEST.json').read_text())
_MAP=_MANIFEST['map']
_LEVELS=u.get_editor_subsystem(u.LevelEditorSubsystem)
_EDITOR=u.get_editor_subsystem(u.UnrealEditorSubsystem)
if _EDITOR.get_game_world():
    raise RuntimeError('Stop PIE before validating the saved map')
if _MAP not in _EDITOR.get_editor_world().get_path_name():
    if not _LEVELS.load_level(_MAP):
        raise RuntimeError('Could not reopen saved Dead Signal map')
_WORLD=_EDITOR.get_editor_world()

# Import only definitions/initialization from the builder. Its terminal try/build
# block is deliberately removed from the AST; no generator function is invoked.
_SOURCE=_ROOT/'Scripts'/'build_dead_signal.py'
_TREE=ast.parse(_SOURCE.read_text(),filename=str(_SOURCE))
if not isinstance(_TREE.body[-1],ast.Try):
    raise RuntimeError('Builder entrypoint changed; review validation import')
_TREE.body.pop()
_OBL2_VALIDATOR={'__name__':'ob_dead_signal_validation_functions'}
exec(compile(_TREE,str(_SOURCE),'exec'),_OBL2_VALIDATOR)
_OBL2_VALIDATOR['MACHINES']=_MANIFEST['machines']
_KEEP=u.EditorPythonScripting.get_keep_python_script_alive()
u.EditorPythonScripting.set_keep_python_script_alive(True)
_STATE={'status':'FINISHING_ASSET_COMPILATION','map':_MAP,'started_utc':datetime.now(timezone.utc).isoformat(),'frames':0,'build_attempts':0,'last_attempt':-1000.,'handle':None,'idle_since':None,'last_report':0.,'processing':False,'finished':False}


def _write():
    serial={k:v for k,v in _STATE.items() if k not in ('handle','idle_since','last_attempt','last_report','started','processing')}
    (_OUT/'NAV_VALIDATION_RESULT.json').write_text(json.dumps(serial,indent=2)+'\n')


def _finish(status,error=None):
    _STATE['finished']=True
    if status=='PASS':
        _STATE.pop('error',None);_STATE.pop('traceback',None)
    _STATE['status']=status
    _STATE['finished_utc']=datetime.now(timezone.utc).isoformat()
    if error:_STATE['error']=error
    _write()
    if _STATE['handle'] is not None:
        u.unregister_slate_post_tick_callback(_STATE['handle'])
        _STATE['handle']=None
    u.EditorPythonScripting.set_keep_python_script_alive(_KEEP)
    if status=='PASS':
        p=_OUT/'BUILD_RESULT.json'
        report=json.loads(p.read_text()) if p.exists() else {'map':_MAP}
        if report.get('error') or report.get('traceback'):
            shutil.copy2(p,_OUT/('BUILD_RESULT_PRIOR_'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S_%fZ')+'.json'))
        report.pop('error',None);report.pop('traceback',None)
        report.update(status='PASS_STRUCTURE_AND_NAVIGATION',navigation=_STATE['navigation'],saved_after_navigation=True,finished_utc=_STATE['finished_utc'])
        p.write_text(json.dumps(report,indent=2)+'\n')
        u.log('DEAD_SIGNAL_NAVIGATION_PASS: saved map projections and local routes verified')
    else:
        u.log_error('DEAD_SIGNAL_NAVIGATION_'+status+': '+str(error))


def _tick(dt):
    if _STATE['processing'] or _STATE['finished']:return
    _STATE['processing']=True
    try:
        _STATE['frames']+=1
        now=time.monotonic();elapsed=now-_STATE['started']
        _STATE['elapsed_seconds']=round(elapsed,2)
        if _EDITOR.get_editor_world()!=_WORLD:
            raise RuntimeError('Editor world changed during validation')
        if elapsed>180:
            raise RuntimeError('Navigation still locked/queued after180 seconds; inspect LogNavigation lock flags and remaining asset compilation. No forced lock removal performed.')
        if _STATE['frames']<24 or elapsed<2:
            return
        busy=u.NavigationSystemV1.is_navigation_being_built_or_locked(_WORLD)
        _STATE['navigation_busy']=busy
        # RebuildNavigation reports an exact lock reason when the editor is
        # still finishing assets. Unlike the old generator, don't require an
        # empty dirty-tile queue before requesting the first build.
        if _STATE['build_attempts']==0 or (busy and _STATE['build_attempts']<3 and now-_STATE['last_attempt']>20):
            _STATE['status']='REQUESTING_NAVIGATION_BUILD'
            _STATE['build_attempts']+=1;_STATE['last_attempt']=now
            _write()
            u.SystemLibrary.execute_console_command(_WORLD,'RebuildNavigation')
            _STATE['idle_since']=None
            return
        if busy:
            _STATE['status']='WAITING_FOR_NORMAL_NAVIGATION_UNLOCK'
            _STATE['idle_since']=None
            if now-_STATE['last_report']>5:
                _STATE['last_report']=now;_write()
            return
        if _STATE['idle_since'] is None:
            _STATE['idle_since']=now;return
        if now-_STATE['idle_since']<2:return
        _STATE['navigation']=_OBL2_VALIDATOR['validate_nav'](_WORLD)
        if _STATE['navigation']['errors']:
            raise RuntimeError('; '.join(_STATE['navigation']['errors']))
        # Saving can pump Slate; unregister before it and keep the processing
        # guard set so a nested tick cannot run a second validation/save.
        if _STATE['handle'] is not None:
            u.unregister_slate_post_tick_callback(_STATE['handle'])
            _STATE['handle']=None
        if not _LEVELS.save_current_level():
            raise RuntimeError('Validated navigation could not be saved')
        saved_map=_ROOT/'Content'/'OrbitBreakout'/'Maps'/'L_DeadSignal.umap'
        if not saved_map.is_file():
            raise RuntimeError('Save returned success but the map file is missing')
        _STATE['map_on_disk']={'path':str(saved_map),'bytes':saved_map.stat().st_size,'modified_utc':datetime.fromtimestamp(saved_map.stat().st_mtime,timezone.utc).isoformat(),'sha256':hashlib.sha256(saved_map.read_bytes()).hexdigest()}
        _STATE['save_returned_success']=True
        _finish('PASS')
    except Exception as exc:
        _STATE['traceback']=traceback.format_exc()
        _finish('FAIL',str(exc))
    finally:
        _STATE['processing']=False


_write()
u.AutomationLibrary.finish_loading_before_screenshot()
_STATE['started']=time.monotonic()
_STATE['status']='WAITING_FOR_EDITOR_TICKS'
_write()
_STATE['handle']=u.register_slate_post_tick_callback(_tick)
u.log('DEAD_SIGNAL_NAVIGATION_VALIDATION_STARTED: no actors regenerated')
