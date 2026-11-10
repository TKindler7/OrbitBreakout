"""Apply the owned SciFi Beast06 Bull to the authored heavy; run in Unreal Editor."""
import json
from pathlib import Path
import unreal as u

MESH = "/Game/SciFi_Beast06/Mesh/SK_SciFi_Beast06_Skin1"
ANIMS = "/Game/SciFi_Beast06/Animations/Anim_SciFi_Beast06_"


def apply_to_enemy(actor):
    mesh = u.load_asset(MESH)
    if not mesh:
        raise RuntimeError("SciFi Beast06 Bull is missing; import it from the owned Fab library first.")
    clips = {name: u.load_asset(ANIMS + clip) for name, clip in {
        "heavy_idle_animation": "Idle1", "heavy_move_animation": "WalkF",
        "heavy_attack_animation": "Attack1", "heavy_death_animation": "Death1",
        "heavy_intro_animation": "Rage",
    }.items()}
    skeleton = mesh.get_editor_property("skeleton")
    for name, clip in clips.items():
        if not clip or clip.get_editor_property("skeleton") != skeleton:
            raise RuntimeError("Missing or incompatible Bull animation: " + name)
    actor.set_editor_properties(dict(
        heavy=True, heavy_skeletal_mesh=mesh, **clips,
        heavy_intro_sound=u.load_asset("/Game/OrbitBreakout/Audio/S_BossIntro"),
        heavy_mesh_offset=u.Vector(0, 0, -112),
        heavy_mesh_rotation=u.Rotator(pitch=0, yaw=-90, roll=0),
        heavy_mesh_scale=u.Vector(1.08, 1.08, 1.08),
        heavy_weapon_socket=u.Name("lowerarm_r"),
        heavy_emitter_offset=u.Vector(0, 0, -16),
        heavy_emitter_rotation=u.Rotator(pitch=0, yaw=0, roll=0),
        heavy_emitter_scale=u.Vector(1.5, 1.5, 1.5),
    ))
    return {"mesh": mesh.get_path_name(), "skeleton": skeleton.get_path_name(),
            "animations": {name: clip.get_path_name() for name, clip in clips.items()},
            "scale": 1.08}


def main():
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem)
    if world.get_game_world():
        raise RuntimeError("Stop PIE before saving the Bull assignment.")
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    targets = [a for a in actors.get_all_level_actors()
               if a.get_class().get_name() == "OBEnemyCharacter"
               and str(a.get_editor_property("stable_id")) == "E05_EN03"]
    if len(targets) != 1:
        raise RuntimeError("Open L_EmergencyDeparture: expected exactly one E05_EN03 sentinel.")
    actor = targets[0]
    report = apply_to_enemy(actor)
    component = actor.get_component_by_class(u.SkeletalMeshComponent)
    # Persist an editor preview too; BeginPlay applies the same native configuration.
    component.set_skeletal_mesh_asset(u.load_asset(MESH))
    component.set_relative_location(u.Vector(0, 0, -112), False, False)
    component.set_relative_rotation(u.Rotator(pitch=0, yaw=-90, roll=0), False, False)
    component.set_relative_scale3d(u.Vector(1.08, 1.08, 1.08))
    component.override_animation_data(u.load_asset(ANIMS + "Idle1"), True, True, 0.0, 1.0)
    component.set_update_animation_in_editor(True)
    if not u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Could not save the level with the Bull assignment.")
    report.update(actor=actor.get_path_name(), stable_id="E05_EN03", encounter=5,
                  saved=True, scope="Saved editor assignment; no mission progress was reset.")
    path = Path(u.Paths.project_dir()) / "docs" / "BULL_INTEGRATION_RESULTS.json"
    path.write_text(json.dumps(report, indent=2) + "\n")
    actors.set_selected_level_actors([actor])
    u.log("ORBIT_BULL_SAVED " + json.dumps(report))


if __name__ == "__main__":
    main()
