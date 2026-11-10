# Orbit Breakout build status

Last reviewed: November 5, 2026. **First playable draft; evidence and remaining polish below.** The current app is `Builds/FirstDraft/Mac/OrbitBreakout.app`.

## Verification state

| Gate | Status | Evidence / remaining work |
|---|---|---|
| Local project and tool discovery | Confirmed | `OrbitBreakout.uproject`, source/configuration, installed UE 5.8.4 and Xcode discovered |
| Gameplay implementation | Compiled; final regression checks passed | Final collision fix passes mission automation and real AI movement/projectile-damage observation. Prior standalone package completed all six encounters and ending without agent progression commands |
| Editor C++ compilation | Passed: Mac Development, arm64 + x64 | UE 5.8.4; permanent `Scripts/build_project.py --executor local-actions --jobs 4 --resume` route rebuilt all 18 project translation units and four link/metadata actions, exit 0; details below |
| Permanent build runner and resume | Passed on this installation | Real export/compile/link/metadata run: 22 successful actions, zero failures; unchanged action-graph resume: 18 compile actions reused and four link/metadata actions rerun successfully |
| Game native compilation/link | Passed | Earlier local-action build compiled/linked arm64+x64. Subsequent normal editor packaging produced the current arm64 app, executable and target receipt |
| Game target app finalization/receipt | Passed through normal editor packaging | Earlier sandboxed shell finalization failed at an engine-directory write. Normal Unreal Editor packaging subsequently built the app/receipt successfully |
| Audio source creation/import | Saved | Sixteen original PCM cues and 16 saved SoundWave assets; see `AUDIO.md`; in-game mix/listening still pending |
| Generator Python syntax | Passed | `compile(source, filename, 'exec')` on `Scripts/bootstrap_content.py` |
| Backup and latent control flow | Passed with isolated Python fixtures | Map/sidecar/external package copies and hashes; successful nav wait/save; nav failure/timeout cleanup; previous keep-alive restoration. These are not Unreal navigation tests |
| Static authored spawn geometry | Passed, limited scope | All 18 enemy and three player checkpoint capsules checked against 427 authored collision meshes; axis-aligned box intersections evaluated; rotated/curved shapes and runtime navigation not validated |
| Unreal content generation | Passed in full editor | Saved `Content/OrbitBreakout/Maps/L_EmergencyDeparture.umap`, materials, 16 audio assets and manifest. Latest observed generation marker: 2026-11-06 04:40:23 UTC |
| Structural map validation | Passed in engine | `CONTENT_VALIDATION.json`: 862 actors, 18 enemies across six encounters, one heavy, five station zones, no structural errors |
| Safe generator rerun | Passed on preceding 862-actor generation | `GENERATOR_RERUN_RESULTS.json`: identical before/after map SHA-256 and size, 862 actors, 22 paths and 47 projections. This check preceded the final sign-facing repair |
| Editor navigation | Passed with larger agent | 22 complete paths and 47 projections passed. Editor confirmed `RecastNavMesh-OrbitAgent` radius 44 cm / height 224 cm. Full player collision/traversal remains unverified |
| Editor playthrough | Not yet performed | New Mission to Mission Complete using normal inputs, no progression shortcuts |
| Save/settings logic tests | Passed in engine | Four native tests passed, zero errors/warnings; `LOGIC_TEST_RESULTS.json` |
| Mission progression/recovery automation | Passed in editor | `MISSION_TEST_RESULTS.json`: one test passed in 16.281441 seconds, zero test errors; includes actual aim/fire input for one guard, scripted later positioning/damage, real timers/world reloads and natural ending |
| Checkpoint, menus and persistence | Automated recovery and completed-save preservation passed | CP01/CP02 disk reads, death/retry, pause, menu routing and holds checked in editor. Completed standalone save remained byte-identical across app restart. Packaged incomplete-checkpoint Continue and deliberate death/retry remain unverified |
| Packaged Mac build | Final corrected app created and freshly launched | `Builds/FirstDraft/Mac/OrbitBreakout.app`, arm64 Development; 665 packages cooked, exit 0 at 05:32:44 UTC, Main reached 05:33:29. Prior package completed the full mission before the final trigger-collision correction |
| Enemy AI and projectile collision | Passed after regression fix | `AI_RUNTIME_RESULTS.json`: real movement, spawned bolts and player health 100→40 over six seconds without injected damage |
| Visual/audio/performance review | Initial viewport inspection and audio correction | Ramp orientation, excessive lighting and sign facing corrected in saved map; alarm looping asset defect fixed. Listening, resolution/preset and measured frame-time review remain pending |
| Human playtest feedback | Not collected | Standalone gameplay progression was observed in logs; player identity, reactions and a representative first-run duration were not recorded |

## Corrections made during source review

- Unreal Python boolean property names corrected to `checkpoint` and `heavy`; the engine strips the native `b` prefix.
- Structural class checks use `MathLibrary.class_is_child_of`; `unreal.load_class` returns an Unreal Class object, not a Python type accepted by `isinstance`.
- Cargo reinforcement E03_EN02 moved away from the raised platform. A static geometry check reproduced the old overlap and confirms the new location clears authored boxes.
- Sentinel spawn moved from Z=110 to Z=116 to clear its 112 cm capsule half-height.
- Reactor housing changed to transparent material so the inner emissive core can be seen.
- Pod visuals attached as movable children of the runtime pod actor; departure now has a visual assembly to translate.
- Final launch aperture widened and sealed with transparent collision, preserving a view from the ending camera while blocking player access to vacuum.

Earlier corrections above have compiled and the generated map has been inspected in the editor. Remaining behavior claims still require the corresponding runtime checks.

Additional corrections made during actual generation/inspection:

- Corrected the reflected text-alignment enum and used named rotation/color fields to avoid Unreal Python constructor ordering mistakes.
- Corrected the cargo ramp rotation and enemy/label orientation.
- Set point lights explicitly to lumens and fixed post-process exposure after an overbright viewport result.
- Deferred `RebuildNavigation` until the asynchronous asset-loading lock clears; the resulting navigation report passed and was saved.
- Added explicit supported-agent dimensions of 44 cm radius / 224 cm height for the heavy enemy. The loaded Recast actor and rebuilt navigation were verified at these dimensions.
- Added arrival-facing room names, dark sign backing, next-room exit labels, and a raised evacuation sign; corrected text-facing orientation and regenerated the saved map.
- Finished outstanding asynchronous asset loading before starting the navigation wait/rebuild timer.
- Set `S_Alarm` to non-looping in the saved SoundWave and generator; this removed orphaned alarm warnings in the final mission test.
- Fixed all seven invisible zone-trigger boxes inheriting BLOCK for the custom `EnemyBolt` channel. Saved responses and generator now explicitly ignore it; structural validation checks it and the default Trigger collision profile overrides it for future/reset volumes.

## Generator safety and validation implementation

Repair now creates a timestamped SHA-256-verified copy under `Saved/Backups` before deleting any generated actors. `--validate` loads an existing mission, runs structural checks and editor navmesh queries, and never regenerates or saves the map. A missing map fails validation instead of being created.

Generation uses the installed engine's `EditorPythonScripting.set_keep_python_script_alive()` and `register_ticker_callback()` APIs so the editor can tick while navigation settles. After a minimum settling period and one second without active/locked navigation, it projects the three checkpoint locations and queries paths for all 18 enemies plus four cargo/ramp/reactor/bay routes. It then saves generated assets/map and emits `ORBIT_NAVIGATION_OK` and `ORBIT_BOOTSTRAP_COMPLETE`. Failure or a 180-second timeout writes `FAIL`, logs `ORBIT_BOOTSTRAP_FAILED`, and releases the runner; normal validation ends with `ORBIT_VALIDATION_COMPLETE`.

The Python success/failure/cleanup paths passed isolated fixtures, and the actual editor generation completed with a saved map and a passing navigation report. Runtime opening doors, player collision and AI behavior still require play testing even when editor navigation queries pass.

## Exact next work

1. Complete a full run of the corrected package and exercise deliberate packaged death/retry and incomplete-checkpoint Continue. The preceding package completed the mission; the corrected package has passed fresh startup and editor mission/AI regression checks. Completed-save preservation across restart is verified.
2. Collect player feedback and assess traversal readability, aim/reload/pause controls, difficulty and pacing. The completed log does not establish a representative first-run duration or human reaction.
3. Measure presentation/performance and update this file with evidence and any remaining defects.

## Saved content and runtime evidence

`docs/CONTENT_VALIDATION.json` records `PASS_STRUCTURE_AND_NAVIGATION`, **862 actors**, encounter sizes 3/3/2/3/3/4, one heavy, no structural errors, **47 successful projections and 22 non-partial paths**. The latest read-only validation at 05:29:41 UTC also checks the corrected trigger responses; `saved_after_navigation` is false because that validation does not rewrite the already saved map. The map and 16 SoundWave `.uasset` files exist on disk.

The first navigation pass at 2026-11-06 04:03:47 UTC used the smaller default agent and 823 actors. That historical pass is superseded by the regenerated map. The editor subsequently reported `RecastNavMesh-OrbitAgent` at radius **44.0** and height **224.0**, rebuilt navigation for that agent, and emitted `ORBIT_NAVIGATION_OK` / `ORBIT_BOOTSTRAP_COMPLETE` at 04:32:18 UTC. The later signage-facing repair emitted the same success markers at **04:40:23 UTC**. Evidence is retained in `OrbitBreakout-backup-2026.11.06-04.41.20.log` under the editor log directory. All these UTC timestamps fall on November 5 Pacific time.

`docs/GENERATOR_RERUN_RESULTS.json` records a passing normal rerun on the preceding 862-actor generation. Before and after, the map was 2,015,976 bytes with SHA-256 `d5df13b2f9e262266008124610ffe14afc959f02781e2bca898eb4000522f14d`; the run validated 22 paths and 47 projections without rewriting the map. This establishes the preservation route on that generation. The final sign-facing repair subsequently changed the map, so this hash is not claimed to describe the current map bytes.

Initial editor logs record `OB_READY checkpoint=0 frontEnd=1 enemies=18 doors=7 interactions=24` and then `frontEnd=0`; menu, player and HUD visuals were observed. New Mission was first invoked through the native controller action. These startup observations alone do not establish physical controls or a full player run.

### Save/settings logic

`docs/LOGIC_TEST_RESULTS.json` records four successful native tests with zero warnings or errors: `Checkpoint.ContinueEligibility`, `Checkpoint.ValidateBoundaries`, `Persistence.MemoryRoundTrip`, and `Settings.ClampAndPreserve`. Their combined execution time was 0.02764 seconds.

### Mission progression, recovery and firing

`docs/MISSION_TEST_RESULTS.json` records **`OrbitBreakout.Mission.PIE.ProgressionAndRecovery`: Success**, duration **16.281441 seconds**, zero test errors, after the final trigger-collision correction. The earlier pre-correction pass took 15.942827 seconds.

The test starts and stops its own single-client PIE session; it must be run from the editor with no mission save present, without manually starting PIE. It refuses existing progress and removes only its own test save. The final implementation uses normal mouse aim/fire input and the actual weapon trace to defeat one corridor guard and verifies firing stops after release. Remaining enemies receive normal player-instigated damage calls, and the pawn is positioned at authored trigger/interaction locations. The test checks all six encounters, both checkpoint disk snapshots, death/retry, cargo reinforcements, pause freezing world time, menu routing, hold cancellation/completion and the natural nine-second ending.

This is an automated mission-state and firing-path result, not a full manual traversal, combat-balancing result, measured mission length, or packaged-game verification.

The initial test build passed 6/6 actions at `Saved/Logs/Builds/20261106T042341.828901Z/`. Its initial manually-started-PIE setup was incompatible with the automation runner ending PIE before tests, so the harness was changed to own its session; the resulting build passed at `20261106T043536.791882Z`. The subsequent actual-input firing addition compiled at `20261106T044208.620403Z`. Historical build logs remain available.

The final run's warning review is preserved with its results: five crowd-navigation warnings occur during world teardown, the missing settings save uses first-run defaults, and Unreal's Mac CoreAudio sample-rate query warns while the mixer continues at 48 kHz. The looping alarm asset was corrected before the successful run; its orphaned-alarm warnings are absent from the final result.

## Enemy projectile regression and live AI check

An edge-position probe found that all seven invisible zone-trigger boxes inherited BLOCK for the new `EnemyBolt` channel, absorbing shots before they reached the player. The failed observation remains in `Saved/Automation/AI_EdgePosition.json`. The saved boxes now ignore that channel, the generator persists and validates the response, and `DefaultEngine.ini` overrides the Trigger profile for future/reset volumes. No C++ gameplay change was required.

`docs/AI_RUNTIME_RESULTS.json` records **PASS at 2026-11-06 05:28:44 UTC**, with no errors. After scripted setup, six game seconds of unmodified AI/nav/projectile behavior reduced the stationary player's health from **100 to 40**; two guards moved horizontally **181.796 cm** and **621.719 cm**, and enemy bolts were observed. The observer did not inject damage or change enemy states, destinations or attack timings. This validates the regression fix, not human play or difficulty balance. The full native mission test then passed again, and read-only structural/navigation validation passed all 22 paths and 47 projections.

## Owned asset review

The signed-in Fab library contains **Underground Tunnel**, **SciFi Beast06 Bull**, and **3D Platformer Game Bundle**. They have not been imported. The first pack is a candidate for industrial dressing in Cargo/Reactor; the Bull is a possible sentinel appearance after animation/rig integration; a zipline is a later optional traversal feature. The two art packs advertise UE 5.8/Mac support; the movement bundle advertises Windows. See the linked inventory in `README.md`.

## Verified editor build evidence

On November 5, 2026 (Pacific time), the persisted build helper completed `OrbitBreakoutEditor`, Mac, Development, `arm64+x64` using installed UE 5.8.4 and Xcode's Apple clang 21 / macOS SDK 27.2. Command, run from this project:

```sh
python3 Scripts/build_project.py --executor local-actions --jobs 4 --resume
```

The first verification run exported and completed four up-to-date target link/metadata actions. To exercise the persisted helper's native compile path, the 18 project `.o` files were moved to `Intermediate/BuildSupport/VerificationObjects-20261106` and the same command was run again. No gameplay source or engine installation files were changed. All 18 translation units, the two architecture links, universal merge, and metadata completed: **22 passed, zero failed or blocked, process exit 0**. The existing precompiled-header outputs were reused. Compiler/link action logs contained no `error:` or `warning:` diagnostics.

Evidence is in `Saved/Logs/PermanentBuildVerification.log` and `Saved/Logs/Builds/20261106T032104.970905Z/` (including `summary.json`, per-action logs and exported action graph). `file Binaries/Mac/libUnrealEditor-OrbitBreakout.dylib` confirms a universal Mach-O dynamic library with x86_64 and arm64 slices; `Binaries/Mac/UnrealEditor.modules` references that library with build ID `55116800`.

An immediate unchanged-graph check used `python3 Scripts/build_project.py --executor local-actions --actions-only --jobs 4 --resume`: **18 compile actions reused, four link/metadata actions rebuilt, zero failures, exit 0**. Its console evidence is `Saved/Logs/PermanentResumeVerification.log`.

These build results establish native build and incremental-runner behavior. Subsequent editor loading, test execution and generation evidence is recorded below; the native build alone does not establish those results or a packaged game.

## Historical game target finalization blocker

The game target was attempted separately, without launching the editor or running packaging:

```sh
python3 Scripts/build_project.py --target OrbitBreakout --executor local-actions --configuration Development --architecture arm64+x64 --jobs 4 --resume
```

Header generation, both precompiled headers, all game translation units, both architecture links, the universal merge, runtime-library copies and version-file generation succeeded. At that earlier build, `file Binaries/Mac/OrbitBreakout` confirmed a Mach-O executable with arm64 and x86_64 slices. The later normal editor package rebuilt the current executable as arm64.

Action export reported a non-fatal installed-engine warning about missing `Engine/Source/ThirdParty/Apple/MetalShaderConverter/include/metal_irconverter_ext`; native compilation and linking still succeeded.

The overall command exited **1**: **28 actions passed, one failed and one was blocked**. `ApplePostBuildSync` failed with `UnauthorizedAccessException` at `/Users/Shared/Epic Games/UE_5.8/Engine/Intermediate/ProjectFiles`; the dependent `OrbitBreakout.target` metadata action was blocked. Inspection of the installed `ProjectFileGenerator.cs` shows that project generation writes `PrimaryProjectName.txt` and `PrimaryProjectPath.txt` to that engine path even for a game project. The engine installation was not modified and the app-bundle finalization step was not bypassed.

Evidence for this earlier failed shell route: `Saved/Logs/GameTargetBuildVerification.log` and `Saved/Logs/Builds/20261106T035122.439361Z/` (per-action logs, `summary.json`, and exported graph). The intermediate executable alone was not a packaged distribution. The later normal editor packaging workflow succeeded as recorded below.

### Installed UE 5.8 Xcode workflow investigation

The installed source does not support disabling modern Xcode for a Mac game through project configuration. `Platform/Apple/AppleExports.cs` marks legacy Xcode unsupported in UE 5.8; `UseModernXcode()` now returns `!CreatingAppOnWindows()`, which selects the alternate path only on an actual Windows host. `ProjectFiles/Xcode/XcodeProject.cs` likewise creates the legacy generator only in that Windows case. No `bUseModernXcode` project override remains in these paths.

`AppleToolChain.PostBuild()` schedules app finalization for a Mac GUI executable. `BuildWithStubXcodeProject()` always generates the Xcode project before building it, and `ProjectFileGenerator.cs` writes the two engine metadata files unconditionally. The AutomationTool Apple staging and packaging paths also call this generator, so merely skipping the native rebuild would not remove the same write requirement. `UE_BUILD_FROM_XCODE=1` only suppresses the post-build work on the assumption that Xcode is already performing it; setting it independently would not construct a valid application bundle and is not a packaging solution.

The normal **Platforms → Mac → Package Project** operation in the running Unreal editor subsequently completed successfully. No ineffective legacy setting, permission change, or fabricated receipt was added to bypass the earlier shell limitation. `Scripts/package_game.sh` itself has not been verified.

## Successful final Mac package

The corrected draft completed normal editor packaging at **2026-11-06 05:32:44 UTC** (November 5 Pacific time), using UE 5.8.4, Development configuration and **Apple silicon (`arm64`)**. All **665 packages cooked**, and build/cook/stage/package/archive completed with **exit code 0**.

**Deliverable:** `Builds/FirstDraft/Mac/OrbitBreakout.app`, 917,281,937 bytes (approximately 875 MiB). This is the current package; the earlier app under `Builds/Mac` is superseded. Evidence: `docs/PACKAGE_RESULTS.json` and `Saved/Logs/FirstDraftFinalPackaging.log`.

**Fresh final-app launch passed at 05:33:29 UTC.** The process executable was confirmed inside the final app path. Its runtime reached Main with 18 enemies, seven doors and 24 interactions, with no error/fatal/ensure entries. The completed standalone save remained byte-identical across graceful quit/restart; the original editor CP0 save was restored separately. The retained final runtime log is `Saved/Logs/FirstDraftFinalStandalone.log`.

The earlier package, built at 05:18:19 UTC, independently completed New Mission, both checkpoint disk saves, all six encounters and the escape ending, reaching `OB_MISSION_COMPLETE` at **05:21:53.954 UTC**. No agent input, damage or progression commands were injected into that packaged session. This completion **predates the final trigger-collision correction** and is retained as earlier evidence, not claimed as a full run of the current package. See `Saved/Logs/FirstDraftStandalone.log` and `Saved/Automation/PACKAGE_BEFORE_COLLISION_FIX.json`.

After that correction, the full mission automation, real AI damage/navigation observation and structure/navigation checks passed before the final package was built and freshly launched. A full run of this corrected package, deliberate packaged death/retry and incomplete-checkpoint Continue remain to be checked. No player identity/reactions, representative first-run duration or measured frame rate is claimed.

An earlier packaging attempt was canceled after a missing `GameFeatureData` AssetManager rule; the standard rule was added before the successful runs. The original sandboxed shell finalization limitation is historical; normal editor packaging succeeded without fabricated receipts or permission changes.

## Evidence conventions

Generator `PASS_STRUCTURAL` means roster/reference checks passed. Final `PASS_STRUCTURE_AND_NAVIGATION` adds editor spawn projection/path queries; neither certifies a complete played mission. `OB_READY`, `OB_ENCOUNTER_STARTED`, `OB_ENCOUNTER_CLEARED`, `OB_CHECKPOINT`, `OB_ENDING_STARTED`, and `OB_MISSION_COMPLETE` are runtime diagnostic markers. Logs produced using test commands or forced progression must be labeled as automated state tests, not an uninterrupted player run.

Keep failed build logs and material diagnostics long enough to explain fixes. Do not invent successful tests, screenshots, frame rates, human feedback, package paths, or completion times. Root will replace this checkpoint with actual build and play evidence as verification progresses.
