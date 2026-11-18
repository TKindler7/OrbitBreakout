import json, math, time, traceback
from pathlib import Path
import unreal as u
root=Path(u.Paths.project_dir()).resolve()
assert (root/'pilot-input-review.marker').is_file() and root.parent==Path('/tmp').resolve()
editor=u.get_editor_subsystem(u.UnrealEditorSubsystem)
levels=u.get_editor_subsystem(u.LevelEditorSubsystem)
result={}
def wait(seconds):
    start=time.monotonic()
    while time.monotonic()-start<seconds: yield

def sequence():
    while not editor.get_game_world(): yield
    while True:
        w=editor.get_game_world()
        pc=u.GameplayStatics.get_player_controller(w,0) if w else None
        if pc and str(pc.get_editor_property('menu_screen'))=='Main': break
        yield
    pc.perform_action('Continue')
    yield from wait(.5)
    while True:
        w=editor.get_game_world()
        pc=u.GameplayStatics.get_player_controller(w,0) if w else None
        p=u.GameplayStatics.get_player_character(w,0) if pc else None
        if p and p.is_gameplay_active(): break
        yield
    def key(name,value=None):
        command='Input.-key '+name if value is None else 'Input.+key '+name+' '+str(value)
        u.SystemLibrary.execute_console_command(w,command,pc)
    def press(name):
        key(name,1); yield; key(name); yield
    def check(name,condition,value):
        result[name]={'pass':bool(condition),'observed':str(value)}
    pc.perform_action('ResetSettings')
    for invert in [False,True]:
        if invert: pc.perform_action('Invert')
        pc.set_control_rotation(u.Rotator(0,0,0))
        # OnAnalogValueChanged in FSceneViewport negates the physical right Y value before PlayerController::InputKey.
        key('Gamepad_RightY',.75); yield from wait(.35); key('Gamepad_RightY')
        pitch=(pc.get_control_rotation().pitch+180)%360-180
        check('stick_down_inverted' if invert else 'stick_down_default',pitch>1 if invert else pitch< -1,pitch)
    pc.perform_action('Invert')
    pc.set_control_rotation(u.Rotator(0,0,0))
    key('Gamepad_LeftY',1); key('Gamepad_LeftThumbstick',1)
    yield from wait(.65)
    velocity=p.get_velocity()
    speed=math.hypot(velocity.x,velocity.y)
    check('L3_sprint',p.get_editor_property('sprinting') and speed>460,speed)
    movement=p.get_component_by_class(u.OBParkourMovement)
    states=[]
    key('Gamepad_RightThumbstick',1)
    for _ in range(8):
        yield
        states.append(str(movement.get_editor_property('traversal')))
    check('R3_slide',any('SLIDE' in state.upper() for state in states),states)
    key('Gamepad_RightThumbstick'); key('Gamepad_LeftThumbstick');key('Gamepad_LeftY')
    yield from wait(1.1)
    key('Gamepad_LeftTrigger',1);yield from wait(.5)
    check('L2_aim',p.get_editor_property('aiming'),p.follow_camera.field_of_view)
    sight=next(c for c in p.get_components_by_class(u.StaticMeshComponent) if c.get_name()=='PrecisionRifle')
    bounds=sight.static_mesh.get_bounding_box()
    (root/'sight-bounds.txt').write_text(str(bounds))
    before=p.magazine_ammo
    key('Gamepad_RightTrigger',1); yield from wait(.36); key('Gamepad_RightTrigger')
    yield from wait(.2)
    check('R2_fires',p.magazine_ammo<before,(before,p.magazine_ammo))
    yield from press('Gamepad_Special_Right')
    paused=str(pc.get_editor_property('menu_screen'))
    check('Options_pauses',paused=='Pause',paused)
    key('Gamepad_LeftTrigger')
    yield from wait(.12)
    if paused=='Pause':
        for _ in range(3):
            yield from press('Gamepad_DPad_Down')
            yield from wait(.09)
        yield from press('Gamepad_FaceButton_Bottom')
        menu=str(pc.get_editor_property('menu_screen'))
        check('Dpad_Cross_focus',menu=='Settings',menu)
    pc.perform_action('Resume')
    yield from wait(.22)
    check('release_aim_during_pause',not p.get_editor_property('aiming'),p.get_editor_property('aiming'))
    key('Gamepad_FaceButton_Left',1); yield; key('Gamepad_FaceButton_Left'); yield from wait(.12)
    check('Square_starts_reload',p.get_editor_property('reloading'),p.get_editor_property('reloading'))
    yield from wait(1.65)
    check('reload_completes',p.magazine_ammo==before and result['Square_starts_reload']['pass'],p.magazine_ammo)
    result['status']='PASS' if all(v['pass'] for v in result.values()) else 'FAIL'

task=sequence()
start=time.monotonic();finished=False
def tick(dt):
    global finished
    if finished:
        if not editor.get_game_world():
            u.unregister_slate_post_tick_callback(handle)
            u.EditorPythonScripting.set_keep_python_script_alive(False)
            u.SystemLibrary.execute_console_command(editor.get_editor_world(),'QUIT_EDITOR')
        return
    try:
        if time.monotonic()-start>100: raise RuntimeError('Timed out')
        next(task); return
    except StopIteration: pass
    except Exception:
        result['error']=traceback.format_exc();result['status']='FAIL'
    (root/'pad-results.json').write_text(json.dumps(result,indent=2))
    finished=True
    levels.editor_request_end_play()
u.EditorPythonScripting.set_keep_python_script_alive(True)
levels.load_level('/Game/OrbitBreakout/Maps/L_EmergencyDeparture')
handle=u.register_slate_post_tick_callback(tick)
levels.editor_request_begin_play()
