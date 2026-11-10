# ORBIT BREAKOUT

**Emergency Departure** is an original single-player, third-person spaceship escape mission for Unreal Engine 5. The project is local to this workspace. Its brief is in `docs/PRD.md` (the source copy is `../spec/ORBIT_BREAKOUT_MASTER_BUILD_SPEC.md`).

## Current state

**November 6 boss and environment upgrade:** the current source and generator add a solo, 2,400-health Bull sentinel, telegraphed rush and shockwave attacks, half-health overdrive, and a HELIX-12 plasma repeater available before the fight. The authored roster is now **16 enemies: 15 troopers and one Bull** across six encounters. The ship receives stronger architectural framing, room landmarks, an orbital vista and an open reactor arena. See `docs/BOSS_UPGRADE.md` for tuning and verification status.

The new native editor module compiled for **arm64 and x64, with all 33 build actions passing** (`Saved/Logs/BossUpgradeBuild.log`). Applying the regenerated map, rerunning navigation and native tests, and checking runtime visuals are pending at this documentation checkpoint. Earlier test results below are historical evidence, not verification of the new boss revision.

The owned SciFi Beast06 Bull is already imported under `Content/SciFi_Beast06` and assigned to `E05_EN03`, with compatible idle, walk, attack and death animations. Its earlier saved placement and appearance were checked in the editor. Enemies disappear after 1.25 seconds and retain the earlier visual awareness, hearing, squad alerts and eight-second investigation memory. That preceding revision passed six enemy/logic checks; see `docs/PROJECT_CHECKPOINT.md`.

**The standalone app at `Builds/VisualUpgradeFinal/Mac/OrbitBreakout.app` is an older build.** It predates the imported Bull, awareness/cleanup changes, HELIX-12, solo boss mechanics and current environment revision. It passed its own launch/Main check, but needs repackaging to contain the latest work. `Builds/FirstDraft/Mac/OrbitBreakout.app` is an even earlier baseline. Historical package evidence remains in `docs/VISUAL_UPGRADE_PACKAGE_RESULTS.json` and `docs/PACKAGE_RESULTS.json`.

Project file: `OrbitBreakout.uproject`. Installed engine: UE 5.8.4 at `/Users/Shared/Epic Games/UE_5.8`; the project engine association is `5.8`.

## Play the older Mac draft

Double-click **`Builds/VisualUpgradeFinal/Mac/OrbitBreakout.app`** and choose **Continue** to resume an incomplete checkpoint, **New Mission** to begin again, or **Replay Mission** after finishing. This older visual-upgrade package was verified to launch to Main; it does not include the latest boss, weapon or environment changes. The earlier Apple silicon (`arm64`) baseline remains at `Builds/FirstDraft/Mac/OrbitBreakout.app`.

## Open the editor project

Open `OrbitBreakout.uproject`, load `/Game/OrbitBreakout/Maps/L_EmergencyDeparture` if it is not already open, press **Play**, then select **Continue** to restore an incomplete checkpoint. **New Mission** and **Replay Mission** start over and replace the mission checkpoint. Check `docs/BOSS_UPGRADE.md` for the latest revision’s validation status.

On this UE 5.8 Mac installation, quit any existing Unreal Editor instance before opening the `.uproject` in Finder. Opening the project through an already running editor has been observed to drop the full project path; a cold Finder launch works.

Leave the holding cell, collect the ARC-9, clear Security and Cargo, bypass the terminal, then take **HELIX-12** from the rack before Reactor containment. Defeat the Bull alone, clear the pod bay and hold E at the escape pod. The current roster has 15 troopers and one Bull; Cargo contains two consecutive encounters.

| Input | Action |
|---|---|
| W / A / S / D | Move relative to the camera |
| Mouse | Look |
| Left Shift | Sprint |
| Space | Jump |
| Right mouse, held | Aim over the shoulder |
| Left mouse, held | Fire |
| R | Reload |
| 1 / 2 | Select ARC-9 / HELIX-12 after acquisition |
| Q | Switch between owned weapons |
| E | Pick up a weapon or use an ammunition station |
| Hold E | Bypass the terminal for two seconds; launch the pod for one second |
| Esc / P | Pause or resume |

Menus support mouse selection and keyboard navigation. Settings include mouse sensitivity, inverted vertical look, master/effects/ambience volume, Low/Medium/High graphics, camera shake, and reduced flashes. The initial graphics preset is Medium with a 60 FPS cap; this is configuration, not a measured performance result.

## Build and authoring

The verified build route on this installation is the project-local action runner. From the `OrbitBreakout` directory:

```bash
python3 Scripts/build_project.py --executor local-actions --jobs 4 --resume
```

This uses the installed Unreal build tools and native compiler, with generated action files and logs inside the project. It has passed editor compilation, linking, and an unchanged-action resume. See `Scripts/BuildSupport/README.md` for details. Restart the editor after rebuilding its native module.

The map and original audio assets already exist; normal play does not require regenerating them. Missing original audio source files can be recreated with:

```bash
python3 Scripts/synthesize_audio.py
```

For content validation, use the **full editor's Output Log command field** after loading the project:

```text
py "/Users/teddykindler/Documents/ChatGPT/Unreal Engine Game 1/OrbitBreakout/Scripts/bootstrap_content.py" --validate
```

Running the script without a flag also preserves an existing mission and validates it. `--validate` fails if the map does not exist. Neither route replaces actors or saves the map. Look for `ORBIT_VALIDATION_COMPLETE` and inspect `docs/CONTENT_VALIDATION.json`; Python exceptions or `ORBIT_BOOTSTRAP_FAILED` indicate failure.

Use `--repair` only to apply intended generator changes. It saves current mission edits, makes a timestamped SHA-256-verified backup under `Saved/Backups`, then replaces actors carrying `OB_GENERATED`. This includes hand edits to generated actors. Existing materials and untagged actors are preserved. `--validate` and `--repair` are mutually exclusive; see `docs/EDITING.md`.

The generator finishes outstanding asset loading before its navigation timer and waits for navigation locks to clear before requesting a rebuild. It then waits for navigation to settle, checks checkpoint/enemy projections and encounter, ramp, HELIX approach and reactor dodge routes, saves the map, and emits `ORBIT_NAVIGATION_OK` followed by `ORBIT_BOOTSTRAP_COMPLETE`. The process uses editor ticks and a 180-second timeout rather than blocking the editor with sleeps. `PASS_STRUCTURE_AND_NAVIGATION` covers editor queries, not player collision, combat, or a complete mission.

`Builds/FirstDraft/Mac/OrbitBreakout.app` was produced successfully through Unreal Editor’s normal **Platforms → Mac → Package Project** workflow. All 665 packages cooked, and build/cook/stage/package/archive completed with exit code 0. Evidence is in `docs/PACKAGE_RESULTS.json` and `Saved/Logs/FirstDraftFinalPackaging.log`. The final corrected app reached its Main screen without error/fatal/ensure log entries. Complete standalone mission progression was recorded in the preceding package; the final collision fix was then covered by editor mission and AI regression checks before repackaging. The packaged executable is `arm64`; the earlier universal native compilation is separate build evidence.

The first visual-upgrade package completed with exit code 0 at 06:11:13 UTC in 145.63 seconds. The exact app under `Builds/VisualUpgrade/Mac` freshly launched at 06:13:36 UTC and Continue entered Cargo. A native screen check showed the held rifle and humanoid armor; it did not establish a full playthrough. A missing Nanite usage warning on `M_Metal` was repaired at 14:54:56 UTC, along with `M_Emissive`, and the map stayed unchanged.

The final `Builds/VisualUpgradeFinal/Mac` package completed with exit code 0 at 15:00:50 UTC in 96.06 seconds. Its exact executable launched freshly, Main was visually confirmed, and the launch log contained no material warnings, fatal errors, ensures or streaming errors. See `docs/VISUAL_UPGRADE_PACKAGE_RESULTS.json` and `Saved/Logs/VisualUpgradeFinalLaunch.log`. The final archive's check covers launch/Main; the previous gameplay checks apply to the same native code before this material-only repair.

`Scripts/package_game.sh` remains an unverified stock BuildCookRun wrapper. The earlier sandboxed native build reached compilation/linking but could not complete engine-directory metadata writes; the normal editor packaging workflow resolved that finalization blocker. Use a packaged app under `Builds/`, not the intermediate native executable in `Binaries/`.

## Automated checks

The new boss/weapon revision compiled successfully. Its isolated `OrbitBreakout.Boss`, `OrbitBreakout.Weapons` and `OrbitBreakout.Upgrade` tests, plus existing `OrbitBreakout.Enemies` and `OrbitBreakout.Logic` checks, are pending execution at this documentation checkpoint. They use transient worlds or memory archives without accessing the mission save slot. See `docs/BOSS_UPGRADE.md`.

**Historical checks:** the preceding visual upgrade passed all five native logic/mission tests at 06:04:56 UTC; those results are in `docs/VISUAL_UPGRADE_TEST_RESULTS.json`. The mission test completed in 17.11 seconds. The earlier baseline logic results remain in `docs/LOGIC_TEST_RESULTS.json`.

`OrbitBreakout.Mission.PIE.ProgressionAndRecovery` passed after the final collision fix in 16.28 seconds; results and warning review are in `docs/MISSION_TEST_RESULTS.json`. Run the `StartsWith:OrbitBreakout.Mission.PIE` filter from the editor automation runner with **no existing mission save**. Do not start PIE manually: the test creates and ends its own single-client PIE session.

The test refuses existing progress, drives normal mouse aim/fire and weapon traces against one guard, then uses authored interaction/trigger positions and normal damage APIs for the remaining encounters. Real world ticks drive delayed reinforcements, pause, death/retry, checkpoint travel and the nine-second ending; the Director API checks hold cancellation/completion. It cleans up only the save it creates. Do not move, rename or replace an existing mission save to force this test to run. Use the isolated checks while owner progress exists. This verifies mission state and one firing path; it does not establish human movement/aiming, combat balance, or packaged behavior.

The combat regression fixed seven invisible zone triggers that were absorbing enemy bolts. Their saved collision responses, generator and default Trigger profile now ignore `EnemyBolt`. The historical visual-upgrade run in `docs/AI_RUNTIME_RESULTS.json` passed at 06:07:07 UTC: both guards moved, projectiles were observed, and normal enemy damage reduced player health from 100 to 10 with no injected damage. The original editor mission save was restored afterward and its SHA-256 verified unchanged.

## Owned Fab assets

The Fab library review identified these three owned packs. The Bull is imported and integrated; the other two remain candidates.

| Pack | Integration / candidate use |
|---|---|
| [Underground Tunnel](https://www.fab.com/listings/6c9e3f18-0b5e-4240-9aea-eecb515175c1) | Selected industrial meshes for Cargo Processing and Reactor Passage; preserve the ship layout and gameplay collision |
| [SciFi Beast06 Bull](https://www.fab.com/listings/126120a3-996e-4dc3-94f1-6550f0adac03) | Imported under `Content/SciFi_Beast06`; Skin1 and compatible animations are assigned to the solo reactor Bull `E05_EN03` |
| [3D Platformer Game Bundle](https://www.fab.com/listings/1898b906-d481-4a05-8515-f2b4d3b28378) | Possible later cargo zipline; its additional movement mechanics are outside this first mission's scope |

The two art packs list UE 5.8 and Mac support. The platformer bundle lists Windows support. The imported Bull skeleton and animation compatibility have been inspected; its forearm emitter uses `lowerarm_r`. The tunnel demo scene expects baked lighting; importing selected meshes does not require adopting the demo map or changing the game's lighting globally.

## Project map

| Location | Purpose |
|---|---|
| `Source/OrbitBreakout/OBCore.h/.cpp` | Mission progression, gates, interactions, pickups, checkpoint/settings saves, game mode |
| `Source/OrbitBreakout/OBPlayerCharacter.h/.cpp` | Rook, movement, camera, health, ARC-9 / HELIX-12 inventory and interaction input |
| `Source/OrbitBreakout/OBEnemyCharacter.h/.cpp` | Trooper/Bull presentation, awareness, navigation, ranged attacks and boss specials |
| `Source/OrbitBreakout/OBEnemyProjectile.h/.cpp` | Swept enemy bolt collision and lifetime |
| `Source/OrbitBreakout/OBRifleAnimInstance.h/.cpp` | Shared rifle animation poses and authored hand-frame attachment |
| `Source/OrbitBreakout/OBUI.h/.cpp` | Native Canvas HUD, menus and player controller |
| `Scripts/bootstrap_content.py` | Deterministic saved map, architecture, encounters, materials and audio import |
| `Scripts/synthesize_audio.py` | Original deterministic PCM sound synthesis |
| `Scripts/player_visuals.py`, `Scripts/security_armor.py` | Textured ARC-9 and full armor tint materials |
| `Scripts/fix_material_usage.py` | Persists native prop-material Nanite usage without regenerating map content |
| `Content/Characters/Mannequins/` | Installed engine-template character assets |
| `Content/Weapons/Rifle/` | Installed Epic rifle mesh and texture assets |
| `Content/SciFi_Beast06/` | Imported owned Bull mesh, materials and animations |
| `SourceAssets/Audio/` | Original editable audio sources |
| `Config/` | Maps, renderer, navigation, collision, input and cook settings |
| `docs/` | Brief, implementation contracts, audio notes, editing and build status |

`DefaultEngine.ini` selects the mission map, `OBGameMode`, and `OBGameInstance`, configures dynamic navigation, and defines `EnemyBolt` on `ECC_GameTraceChannel1`. `DefaultInput.ini` supplies keyboard/mouse mappings. `DefaultGame.ini` includes the mission map and the OrbitBreakout, Characters and Weapons content directories in cooking. Do not remove required cook entries just because assets load in the editor.

Mission saves use slot `OB_EmergencyDeparture`; settings use `OB_Settings`. Schema version 1 gains additive HELIX ownership and reserve-ammunition fields; older checkpoints default to an unclaimed weapon rack. Checkpoint 2 restores an acquired HELIX with a full magazine and at least 108 reserve rounds. New Mission replaces the previous mission checkpoint. Disk save failures should be reported in the UI while the in-memory checkpoint remains available. Save-file paths depend on the runtime/editor environment; inspect the active platform's project saved directory rather than assuming a packaged app uses the editor's directory.

The older material-corrected package passed a fresh launch/Main check. The earlier visual package passed Continue-to-Cargo using the same native code. Do not treat `Binaries/` or an editor module as a standalone game. Earlier baseline mission completion is historical evidence, not a full-playthrough result for this visual revision. A newly packaged boss/weapon revision, full mission playthrough, player feedback and measured performance remain separate checks. During those earlier runs, the editor save was restored and verified unchanged, and the packaged save was backed up before launching the first visual upgrade. This revision’s save-integrity check is recorded separately with its verification results.
