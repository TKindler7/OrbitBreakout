"""Read-only map/loadout checks; run in Unreal after the boss upgrade repair."""
import json
from pathlib import Path
from datetime import datetime, timezone
import unreal as u


def main():
    actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    enemies = [a for a in actors if a.get_class().get_name() == "OBEnemyCharacter"]
    boss_room = [a for a in enemies if a.get_editor_property("encounter_id") == 5]
    pickups = [a for a in actors if a.get_class().get_name() == "OBInteractable"
               and str(a.get_editor_property("stable_id")) == "I05_HelixRifle"]
    triggers = [a for a in actors if a.get_class().get_name() == "OBZoneTrigger"
                and a.get_editor_property("zone") == 5 and not a.get_editor_property("checkpoint")]
    errors = []
    if len(enemies) != 16:
        errors.append("Expected 15 guards and one boss")
    if len(boss_room) != 1 or not boss_room[0].get_editor_property("heavy"):
        errors.append("The reactor must contain only the Bull boss")
    boss = boss_room[0] if len(boss_room) == 1 else None
    if boss:
        if boss.get_editor_property("sentinel_health") != 2400:
            errors.append("Boss health must be 2400")
        if not boss.get_editor_property("heavy_skeletal_mesh"):
            errors.append("Fab Bull mesh missing")
        for name in ("heavy_idle_animation", "heavy_move_animation", "heavy_attack_animation", "heavy_death_animation"):
            if not boss.get_editor_property(name):
                errors.append("Missing " + name)
    if len(pickups) != 1 or len(triggers) != 1:
        errors.append("Expected one HELIX pickup and one reactor trigger")
    else:
        pickup, trigger = pickups[0], triggers[0]
        if pickup.get_editor_property("kind") != u.OBInteractionKind.HEAVY_WEAPON:
            errors.append("HELIX pickup uses the wrong interaction")
        box = trigger.get_component_by_class(u.BoxComponent)
        boundary = trigger.get_actor_location().x - box.get_scaled_box_extent().x
        if pickup.get_actor_location().x >= boundary:
            errors.append("HELIX pickup must precede the boss activation boundary")
    report = {
        "checked_utc": datetime.now(timezone.utc).isoformat(),
        "status": "FAIL" if errors else "PASS", "errors": errors,
        "actor_count": len(actors), "enemy_count": len(enemies),
        "reactor_enemies": [str(a.get_editor_property("stable_id")) for a in boss_room],
        "boss_health": boss.get_editor_property("sentinel_health") if boss else None,
        "boss_mesh": boss.get_editor_property("heavy_skeletal_mesh").get_path_name() if boss and boss.get_editor_property("heavy_skeletal_mesh") else None,
        "pickup_location": str(pickups[0].get_actor_location()) if pickups else None,
        "scope": "Saved editor actor properties and pickup placement; native combat is checked separately by isolated automation. No mission save access.",
    }
    (Path(u.Paths.project_dir()) / "docs" / "BOSS_UPGRADE_CONTENT_RESULTS.json").write_text(json.dumps(report, indent=2) + "\n")
    if errors:
        raise RuntimeError("; ".join(errors))
    u.log("ORBIT_BOSS_UPGRADE_CONTENT_OK " + json.dumps(report))


if __name__ == "__main__":
    main()
