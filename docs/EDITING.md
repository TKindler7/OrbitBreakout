# Editing Orbit Breakout

The current mission is authored by `Scripts/bootstrap_content.py`; its native gameplay lives in `Source/OrbitBreakout/`. After generation, the map and placed actors are ordinary Unreal content you can edit. Consult `BUILD_STATUS.md` before assuming the map or a playable build already exists.

## Keep changes recoverable

Make a recoverable local copy or source-control checkpoint before substantial changes. Normal generator reruns preserve an existing mission and validate it. Explicit `--validate` has the same preserve behavior and refuses to create a missing mission. Both check structural data and existing navigation without saving the map. They do not synchronize changed script coordinates into the existing map.

Every generated actor carries the `OB_GENERATED` tag and sits in an `OB_Generated/...` Outliner folder. **Explicit repair deletes and recreates tagged actors, including any edits made to those actors.** It leaves untagged actors and existing material assets alone. Moving an actor to another Outliner folder does not remove its ownership tag. Before replacement, the generator saves current edits to this mission if it is loaded and creates a timestamped backup under `Saved/Backups/EmergencyDeparture_<UTC timestamp>/`. It copies the map, existing sidecars/build data, and any external actor/object packages, and verifies SHA-256 hashes before it allows deletion. `BACKUP_MANIFEST.json` records each file. This protects the pre-repair map, while a separate local/source-control checkpoint remains useful for source and other assets.

To recover a backup, close Unreal Editor, preserve the current version, then copy the backup's `Content/` files back into the corresponding project locations. Reopen the project and run validation. Do not mix actor packages from different backup snapshots. Backups do not include unchanged shared material/audio assets.

Use one of these approaches for a custom prop:

- Add a new untagged actor under your own Outliner folder to keep it independent of generation.
- Edit a tagged actor directly and avoid repair unless that edit has also been incorporated into the script or preserved separately.
- For a permanent procedural change, edit the script, back up the map, then explicitly repair generated actors.

Do not remove generation tags from required enemies, doors, checkpoints or the mission director just to preserve them: repair could then create duplicates beside them. Keep their stable IDs and ownership consistent.

Existing materials and audio imports are reused by name. Repairing actors does not overwrite edited material assets. Audio synthesis also preserves existing WAV files unless `--replace` is explicitly supplied. Replacing WAV files requires an Unreal reimport to change already imported SoundWave assets.

## What to change

| Change | Exact location | Apply / verify |
|---|---|---|
| Walk, sprint, aim speed | `OBPlayerCharacter.h`: `WalkSpeed`, `SprintSpeed`, `AimSpeed` | Recompile C++; verify camera-relative movement and shooting restrictions |
| Blaster damage, cadence, magazine, reserve, reload | `OBPlayerCharacter.h`: `ShotDamage`, `ShotInterval`, `MagazineCapacity`, `MaxReserveAmmo`, `ReloadDuration` | Recompile; verify hit count, reload, pickups and checkpoint ammo assumptions |
| Player maximum health | `OBPlayerCharacter.h`: `MaxHealth`; also inspect checkpoint/pickup logic in `OBCore.cpp` | Recompile and test damage, healing and all checkpoint restores; current baseline is 100 and some core paths may still contain literal baseline values |
| Enemy health, damage, charge and speed | Placed `OBEnemyCharacter` Details under `Orbit|Tuning`; shared defaults in `OBEnemyCharacter.h`, movement values in `.cpp` | Instance edits need a map save; shared native edits need compilation; verify both trooper and sentinel |
| Enemy placement | Map actors in `OB_Generated/Encounters`; persistent generator source `ENEMIES` in `bootstrap_content.py` | Preserve unique StableId and EncounterId; keep capsule above the floor; test paths and line of sight |
| Enemy counts | `ENEMIES`, generator `validate_saved()`, and `OBCore.cpp` `ActivateEncounter()` / `ValidateContent()` expected counts | Update all coupled expectations and stable IDs together; compile/repair as required; recheck clearing and gates |
| Room geometry | Map actors in `OB_Generated/Architecture`, `Cover`, `Fixtures`; script `ZONES`, `shell()`, `connector()` and detail functions | Save the map or repair after backup; verify door widths, camera, navmesh and full route |
| Room colors and glow | `/Game/OrbitBreakout/Materials/MI_*`; parent `M_Metal`, `M_Emissive`, `M_Window` | Edit Color/Roughness/EmissiveStrength where exposed; save assets; check readability in the game viewport |
| Pickups and ammo stations | Placed `OBInteractable` actors; script `populate_gameplay()` | Preserve `Kind`, `Zone` and unique `StableId`; save map; verify pickup and recovery reachability |
| Medical/ammo reward and station refill | `OBCore.cpp`: `CheckPickups()` / `TryInteract()` | Recompile; verify capacity clamps, refill cooldown and no empty-ammo dead end |
| Objective and system-message copy | `OBCore.cpp`: `GetObjectiveText()`, transition/interaction methods | Recompile; verify each phase and UI text fitting |
| Checkpoint position | Placed `Mission_EmergencyDeparture`: Start/Cargo/Reactor checkpoint location; script `START`, `CARGO_CP`, `REACTOR_CP` | Save or repair; test death/retry and fresh app Continue from each checkpoint |
| Checkpoint trigger boundary | `CP_Z03`, `CP_Z05` Details: Zone, Checkpoint and TriggerExtent | Preserve sequence requirements; test crossing once, retreating, and spawning at the checkpoint |
| Ending camera and departure | `OBCore.cpp`: `BeginEnding()`, ending branch of `Tick()` | Recompile; test full ending, pause, skip after one second, complete/save/replay |
| Pod appearance | Script `bay_details()` or attached `OB_POD_VISUAL` actors | Keep moving parts attached to `I06_EscapePod` and movable; fixed cradle remains separate |
| Sound design | `Scripts/synthesize_audio.py`, `SourceAssets/Audio/`, `/Game/OrbitBreakout/Audio/` | Preserve/reimport sources deliberately; audition with volume settings; see `AUDIO.md` |
| Controls | `Config/DefaultInput.ini`; direct controller menu bindings in `OBUI.cpp` | Restart editor/game as needed; update Controls screen copy to match |

## Shared defaults versus a placed instance

Changing one placed enemy's Details affects that saved actor. Changing a C++ default affects the native default and new instances; an existing placed actor with an override can keep the old value. Reset the instance property to its default when you want it to inherit. Verify in a fresh run rather than relying on a value changed during Play In Editor, since PIE changes normally disappear when play ends.

The player is spawned by `AOBGameMode` using `AOBPlayerCharacter`; there is no placed player actor whose combat values can simply be saved in the map. For the current implementation, change the native defaults and recompile. A future thin Blueprint child can expose persistent defaults, but it must also be assigned as the game mode's default pawn class.

Health, weapon, interactions, save and UI responsibilities are combined into a small number of classes. The project does not currently contain separate health/weapon/interaction components or UMG widget assets. The Canvas HUD/menu implementation is `OBUI.cpp`.

## Baseline mission contracts

| Encounter | Zone | Units |
|---|---|---|
| E01 | Security Corridor | 3 troopers |
| E02 | Cargo Processing | 3 troopers |
| E03 | Cargo reinforcements after E02 | 2 troopers |
| E04 | Security Junction after terminal bypass | 3 troopers |
| E05 | Reactor Passage | 2 troopers + 1 sentinel |
| E06 | Escape-Pod Bay | 4 troopers |

Gate IDs D00–D06 are unique. D01 requires the blaster; D02 requires E01; D03 requires E02 and E03; D04 requires terminal success and E04; D05 requires E05; D06 requires E06. Checkpoints restore canonical progress: CP00 is the unarmed cell start, CP01 clears E01, and CP02 clears E01–E04 plus the terminal. Do not manually change phase state to make a gate appear fixed; test the dependency and restoration path instead.

Baseline checkpoint capsule centers are CP00 `(-600, 0, 100)`, CP01 `(5070, 0, 100)`, CP02 `(12000, 0, 100)`, in centimeters. The saved map's director properties are authoritative after manual edits. Main doorway openings are 300 cm wide by 320 cm tall, with a wider final launch aperture. Keep the critical route walkable without jumping and keep the ammunition station reachable during every encounter.

## Change one thing: blaster damage

After a working baseline has been compiled and verified, change `ShotDamage` in `OBPlayerCharacter.h` from 20 to 30, rebuild the editor target, and start a New Mission. There is no separate combat-test map in the current project, so use E01 in the main mission. A 60-health trooper should require two registered body hits rather than three, assuming default health and no intervening tuning. Verify hit feedback and enemy clearing, then retain or revert the value and rebuild. Record actual results; this example is an expected calculation, not a completed playtest.

## Configuration boundaries

`DefaultEngine.ini` sets the starting map, native game mode/instance, dynamic navigation and the `EnemyBolt` collision channel. `DefaultInput.ini` defines gameplay mappings. `DefaultGame.ini` defines package cooking for `/Game/OrbitBreakout` and `/Game/Characters`. Keep editor-only generation APIs out of runtime source, and do not confuse a successful editor asset load with cooked package availability.

Gameplay changes require C++ compilation; map placement/material edits require saved Unreal assets; generator changes require a deliberate generation decision. Packaging must be repeated after changes when verifying the standalone game. Keep mission save schema changes compatible, or increment/migrate the schema deliberately instead of silently interpreting old progress differently.
