"""Import the original intro cue and assign the existing Bull rage animation."""
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve()
RAGE_PATH = "/Game/SciFi_Beast06/Animations/Anim_SciFi_Beast06_Rage"
SOUND_PATH = "/Game/OrbitBreakout/Audio/S_BossIntro"


def main():
    if u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError("Stop PIE before applying entrance assets")
    sound = u.load_asset(SOUND_PATH)
    if not sound:
        task = u.AssetImportTask()
        task.set_editor_properties(dict(filename=str(ROOT / "SourceAssets/Audio/S_BossIntro.wav"),
            destination_path="/Game/OrbitBreakout/Audio", destination_name="S_BossIntro",
            automated=True, replace_existing=False, save=True))
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        sound = u.load_asset(SOUND_PATH)
    if not sound:
        raise RuntimeError("Mechanical roar import failed")
    if "--audio-only" in sys.argv:
        u.log("ORBIT_BOSS_INTRO_AUDIO_READY " + sound.get_path_name())
        return
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    targets = [a for a in actors.get_all_level_actors()
               if a.get_class().get_name() == "OBEnemyCharacter"
               and str(a.get_editor_property("stable_id")) == "E05_EN03"]
    if len(targets) != 1:
        raise RuntimeError("Expected exactly one Bull in L_EmergencyDeparture")
    boss = targets[0]
    rage = u.load_asset(RAGE_PATH)
    mesh = boss.get_editor_property("heavy_skeletal_mesh")
    if not rage or not mesh or rage.get_editor_property("skeleton") != mesh.get_editor_property("skeleton"):
        raise RuntimeError("Rage animation does not match the assigned Bull skeleton")
    boss.set_editor_properties(dict(heavy_intro_animation=rage, heavy_intro_sound=sound, boss_intro_seconds=2.4))
    # Keep the saved editor appearance idle. The rage is a one-shot runtime intro.
    component = boss.get_component_by_class(u.SkeletalMeshComponent)
    component.override_animation_data(boss.get_editor_property("heavy_idle_animation"), True, True, 0.0, 1.0)
    if not u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level():
        raise RuntimeError("Could not save entrance assignment")
    report = dict(saved=True, checked_utc=datetime.now(timezone.utc).isoformat(),
                  actor=boss.get_path_name(), animation=rage.get_path_name(),
                  original_clip_seconds=rage.get_play_length(), intro_seconds=2.4,
                  sound=sound.get_path_name(), boss_health=boss.get_editor_property("sentinel_health"),
                  scope="Targeted actor/asset save. No mission save access or map regeneration.")
    (ROOT / "docs/BOSS_INTRO_ASSETS.json").write_text(json.dumps(report, indent=2) + "\n")
    u.log("ORBIT_BOSS_INTRO_ASSETS_SAVED " + json.dumps(report))


if __name__ == "__main__":
    main()
