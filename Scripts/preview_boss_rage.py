"""Preview a paused Rage frame in the editor; --restore returns to idle. Never saves."""
import sys
import unreal as u

if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
    raise RuntimeError("This preview is editor-only")
boss = next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
            if a.get_class().get_name() == "OBEnemyCharacter"
            and str(a.get_editor_property("stable_id")) == "E05_EN03")
mesh = boss.get_component_by_class(u.SkeletalMeshComponent)
restore = "--restore" in sys.argv
clip = boss.get_editor_property("heavy_idle_animation") if restore else u.load_asset("/Game/SciFi_Beast06/Animations/Anim_SciFi_Beast06_Rage")
mesh.override_animation_data(clip, restore, restore, 0.0 if restore else .45, 1.0)
mesh.set_update_animation_in_editor(True)
mesh.set_position(0.0 if restore else .45, False)
u.log("ORBIT_BOSS_RAGE_PREVIEW " + ("restored idle" if restore else "Rage at 0.45 seconds"))
