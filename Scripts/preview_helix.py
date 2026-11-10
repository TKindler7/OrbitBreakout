"""Cosmetic PIE preview only. Front-end guard stays on; no save writes or combat."""
import unreal as u

world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
if not world:
    raise RuntimeError("Start PIE to Main first")
player = u.GameplayStatics.get_player_character(world, 0)
controller = u.GameplayStatics.get_player_controller(world, 0)
if str(controller.get_editor_property("menu_screen")) != "Main":
    raise RuntimeError("Preview requires the initial Main screen; it refuses a running mission")
player.restore_loadout(True, 24, 96)
player.restore_heavy_weapon(True, 144, True)
player.set_actor_location(u.Vector(13000, -250, 100), False, False)
player.set_actor_rotation(u.Rotator(pitch=0, yaw=0, roll=0), False)
controller.set_control_rotation(u.Rotator(pitch=-5, yaw=0, roll=0))
controller.hide_screen()
# Neither the game instance's front-end flag nor its mission snapshot is modified.
u.log("ORBIT_HELIX_COSMETIC_PREVIEW: held weapon only; gameplay disabled; no save written")
