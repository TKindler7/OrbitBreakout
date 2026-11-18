# Level 2 — Dead Signal implementation

Updated November 13, 2026, after the final trooper mission smoke and focused rescue/navigation follow-ups. The converted troopers compile for arm64 and x64, all five targeted native tests pass, and all thirty saved actors have been refreshed. The post-conversion scripted smoke passed 114/114 checks in 360.83 seconds, including actual death/retry and 41 live presentation audits covering all thirty troopers. The report has no assertion errors or warnings; its separate handled Python navigation ensure was diagnosed, and an isolated corrected-binding probe passed afterward. A rescue-camera defect was repaired in a camera-only patch, and the coordinator visually verified the actual sequence at 1, 8 and 12 seconds. The full mission was not repeated after those camera/harness changes. The original 67-assertion result remains historical evidence in `Saved/Level2/PRE_TROOPER_RUNTIME_SMOKE.json`. The active project is `OrbitBreakout.uproject` in this directory's parent; the separate Publishing copy is not the target.

## Implemented

- A saved `/Game/OrbitBreakout/Maps/L_DeadSignal` chapter, constructed from the installed Underground Tunnel kit. Seven connected areas, collision, navigation, cover, the Star Sparrow wreck, controls, hazards and lighting exist in the actual map. Both editable cinematic `.uasset` files have been saved and reopened in the editor; this is no longer a script-only deliverable.
- `OBLevel2.h/.cpp` supplies an independent mission director, hold interactions and electrical/pressure hazards. It intentionally bypasses the Level 1 director's initialization and Tick implementation.
- Thirty authored security troopers: seventeen along the route including the Relay Warden, and thirteen in the finale. `OBMachineEnemy.h/.cpp` retains Cutter lunges, Watcher bursts, Bulwark directional shielding and exposed cores, and the Warden's charged shot/pulse pattern. The compiled source and refreshed map use the installed textured Sci-Fi Trooper mesh, authored rifle and role-specific equipment in place of the primitive machine bodies; no external art purchases are required. All thirty passed active mesh, rifle attachment and bounded-muzzle checks during the completed scripted mission smoke.
- Level 1's existing nine-second Star Sparrow departure calls a guarded chapter handoff. Chapter two uses a 42-second crash sequence and a 15-second rescue-response sequence, with subtitles and existing sound cues. Hold C / Circle for one second after the first two seconds to skip. Both paths use the same finalizer to restore the wreck, player, camera and objective.
- Wraith, its authored rifle/socket, ARC-9, HELIX-12, the Level 1 Bull and existing sensitivity/inversion settings are retained. `OBRifleAnimInstance.cpp` now has a separate Level 2 trooper branch for aim, recoil, death and the Cutter dash; its existing player, Level 1 enemy and Wraith configuration branches are unchanged. The shared file is therefore no longer byte-identical to the original backup. The player's existing obstruction-aware trace also damages these enemies.
- Keyboard E and contextual controller Square operate mission controls. Square reloads when no control is in reach. Damage, movement out of range, lost sight, pause, death and cinematic entry cancel partial hold progress. Circle is a separate cinematic skip input.
- Level 2 HUD includes its objective marker, labeled uplink meter, Warden health/attack cue, controller prompts and save warnings. Completion offers Replay Level 2, Main menu and Quit; it does not expose an unavailable third chapter.
- Both chapter maps are included in `Config/DefaultGame.ini` packaging settings.

## Trooper presentation conversion

The native implementation uses `/Game/SciFITrooper_Man_03/SkeletalMesh/SK_SciFITrooper_Man_03`, the installed paintable Top 3 / Bottom 3 materials and `/Game/Weapons/Rifle/Meshes/SM_Rifle`. The material instances retain authored normal, surface and wear textures, with subdued paint and no body-wide emissive glow. The old 24 primitive body components remain only for serialized map compatibility and are hidden with collision disabled. Small muzzle/status lights and attack telegraphs still use simple shapes.

| Existing role | Saved visual implementation | Preserved behavior |
| --- | --- | --- |
| Cutter | Charcoal with ochre markings, 0.96 mesh scale | Marked lunge, now using the existing `MM_Dash` animation |
| Watcher | Gunmetal with muted red markings, 1.00 mesh scale | Committed rifle bursts |
| Bulwark | Gray with ochre markings, 1.08 mesh scale; imported chest armor and forearm shield | Frontal protection, flanking and timed exposure |
| Relay Warden | Black/steel with muted red markings, 1.17 mesh scale; reinforced chest armor and larger shield | Charged shot/pulse pattern and existing exposure windows |

The rifle follows the authored weapon/right-hand bone transform, with projectiles and flash sourced at its muzzle. Heavy troopers use the vendor's contoured `SM_Armor_full_2` as a forearm breach shield, painted with the same uniform material as their chest armor. The shield lowers during the existing vulnerability window; this remains directional damage protection, not a new precision weak-point hitbox. Standing capsules range from 90 to 112 cm half-height and 34 to 44 cm radius; skeletal and equipment meshes do not add collision. Ground placement traces architecture only, and dormant/dead actors suspend skeletal animation after their appropriate lifecycle transition.

After the final build/reload, all thirty actors were refreshed and the map saved without regeneration. The coordinator independently viewed the final compiled four-role lineup and confirmed humanoid silhouettes, ground contact, rifle grip, subdued role colors/scales and contoured heavy shields. `TROOPER_PREVIEW.json` records the four preview actors. This focused inspection does not establish every combat animation; mission-state coverage comes from the separate full smoke.

Encounter IDs, enum names, health values, attack slots, finite waves, checkpoints and save schema remain stable. The approach acceptance radius is now 45 cm to clear path corners before the separate attack-range check. The previously stalled north-entry Cutters reached Windup, Lunge and Recovery in both final smoke finale runs, with first attacks at 6.67–6.68 seconds. Three player-facing objective/reinforcement strings now say “security trooper(s).” The antenna maintenance log and Relay Warden name are unchanged.

## Ordered mission and recovery

| Milestone | Required actions | Checkpoint restored on retry |
| --- | --- | --- |
| Crash | Watch or skip; explicit medical recovery | Crash chamber; intro does not replay |
| Junction | Clear security, isolate live cable, drain water | Dry junction walkway; cable and pump completed |
| Auxiliary power | Clear freight and station, collect/insert coupler, restart | Power station; previous encounters resolved |
| Warden | Defeat the single elite; key awarded automatically | Safely beyond Warden gate; finale starts from Stage A |
| Uplink | Both stabilizers, opening fight, transmitter, overload repair, remaining hostiles | Finale-entry checkpoint until completion |

Machine encounter IDs are 201 (Cutters), 202 (Watchers), 203 (junction), 204 (freight), 205 (freight reinforcement after two deaths), 206 (power) and 207 (Warden). Gates are 203, 204, 206 and 207.

Finale scheduling is finite and idempotent: 210 = four opening enemies; 211 = four at transmission start; 212 = three at 30 seconds; 213 = two after repair. At most five are alive in the finale and four own attack slots. Reinforcements wait for capacity and a hidden, adequately distant entry location. Progress clamps to 50 seconds once, requires a two-second repair at A, then resumes to 75 seconds. Completion waits for the entire finite defense to be cleared. The generator has no escort health bar.

Water deals 3 damage per 0.25-second interval until isolation. Vents cycle through a 1-second warning, 1.5-second active interval and 2-second safe gap; contact damage is 15 with a 1-second repeat interval. Dry routes and waiting pockets are authored into the map.

## Save and replay isolation

Schema 2 adds explicit chapter identity and stable checkpoint keys to the existing save class. Schema 1 snapshots migrate to Level 1, preserving its checkpoint and ammunition fields. The transition writes a pending Level 2 handoff before travel and consumes it only after the destination exists. ARC/HELIX ownership, exact magazines, reserves and selected weapon transfer. The crash does not grant an unearned HELIX; its nearby cache supplies reserve ammunition.

Checkpoint milestones restore full health and adequate ammunition. The crash checkpoint retains the transferred magazine/reserve amounts; the cache is optional resupply. Finale activation/overload/timer state stays transient until completion, so death consistently resets both stabilizers and all thirteen enemies.

A direct Level 2 launch or the menu's separate replay uses `OB_DeadSignal_Development`, retains the campaign snapshot in memory and restores it when returning to the menu. Its documented test loadout is ARC 24 + 72 reserve, HELIX 36 + 72 reserve. The normal campaign slot remains `OB_EmergencyDeparture`; settings remain separate in `OB_Settings`.

## Saved content and reproducible tooling

| Asset | Purpose |
| --- | --- |
| `/Game/OrbitBreakout/Maps/L_DeadSignal` | Complete authored chapter map |
| `/Game/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Crash` | 42-second opening, six camera cuts, the existing scene's single Star Sparrow actor |
| `/Game/OrbitBreakout/Level2/Cinematics/LS_DeadSignal_Rescue` | 15-second response, three camera cuts |
| `/Game/OrbitBreakout/Level2/Materials/` | Chapter-owned accent materials; vendor materials remain in their original folders |

`Scripts/build_dead_signal.py` generates only `OB_L2_GENERATED` actors and backs up previously generated map/sequence assets before rebuilding. It preserves untagged owner actors and does not rewrite the vendor demonstration map. `docs/LEVEL_2_LAYOUT_MANIFEST.json` records the exact controls, enemy identities, encounters and checkpoint positions.

`Scripts/validate_dead_signal.py` validates the saved map without regenerating it. It reads the manifest, requests a navigation build, checks 34 points and ten local routes, saves the map, and records the saved file's size, modification time and SHA-256. Its callback unregisters before saving to prevent nested Slate ticks from running a second save.

A navigation lock after the initial generation was resolved by reopening the editor and map normally. No engine lock flags were forcibly removed. The initial north-door path sample was accidentally inside a raised cover block; `NORTH_DOOR_SAMPLE_DIAGNOSTIC.json` records the old/new projections, real cover bounds and paths. The corrected test endpoint is walkable arena floor. Geometry was not removed and navigation tolerances were not loosened to make the test pass. Earlier reports were archived.

The first visual inspection found an impact camera inside a slope mesh and overexposed gameplay. `Scripts/patch_dead_signal_presentation.py` backed up and repaired only the authored presentation: the impact camera is outside the slope bounds, the interior crash cut starts at 24 seconds, the opaque collapse cap sits at the exterior portal, outdoor fill lights illuminate descent, orbital key lights are reduced, and the chapter exposure is 5.5 EV100. The resulting map and sequences were saved. The coordinator observed a readable ship during descent, Star Sparrow visibly entering the arch with sparks and warm light around 24–26 seconds, and a clearly visible warm-lit wreck/Wraith/crash chamber around 30 seconds.

An independent image during the completed smoke exposed a rescue view dominated by black space and white crossed beams. `Scripts/patch_dead_signal_rescue.py` subsequently backed up and changed only the rescue cameras, rescue sequence and matching builder shot declarations. The patch saved at 21:34:56 UTC retains three cuts over 15 seconds, relocates the shots into the room east of the central shaft and frames the native Wraith/transmitter position. All fifteen sampled camera-clearance points were clear; the crash sequence hash and gameplay actors were unchanged. In actual PIE sequence previews at 1, 8 and 12 seconds, the coordinator independently confirmed Wraith and the transmitter with no black-frame/crossed-beam defect. The preview made no mission-finalizer/checkpoint calls and left both campaign and development save files unchanged. These focused checks follow the 114-check run; the complete mission was not rerun after the camera-only patch.

## Verification evidence

The table distinguishes functional checks, source/build evidence and visual inspection. The final smoke's `PASS` status does not include editor-log review or camera-image quality.

| Check | Recorded result | Evidence and limits |
| --- | --- | --- |
| Native editor module | **Passed after final trooper changes** | `Saved/Logs/Level2TrooperFinalBuild.log` and `Saved/Logs/Builds/20261113T212243.450326Z/summary.json`: final incremental build completed six actions, zero failed/blocked, arm64 and x64. The preceding trooper/animation compilation also passed; the final build includes the contoured shield revision. |
| Saved map and sequences | **Passed for existence, save and reopen** | Generation, subsequent normal editor reopen, presentation-patch asset saves and post-patch map save recorded. Actual cinematic quality remains a runtime inspection item. |
| Editor navigation | **Passed** | `Saved/Level2/NAV_VALIDATION_RESULT.json`, completed 20:50:29 UTC: all 34 projections and ten local paths passed, `save_returned_success=true`, no errors. Routes are tested with authored gates closed; runtime gate opening/full player traversal are separate checks. |
| Save migration and serialization | **Passed targeted automation after conversion** | `TROOPER_AUTOMATION_RESULTS.json`: legacy Level 1 migration and chapter/loadout round trip. These tests do not claim every quit/reopen checkpoint path has been manually exercised. |
| Exact loadout transfer | **Passed targeted automation after conversion** | `TROOPER_AUTOMATION_RESULTS.json`: ARC/HELIX ownership, magazines, reserves and selection checked by native test. |
| Machine combat rules | **Passed targeted automation after conversion** | `TROOPER_AUTOMATION_RESULTS.json`: shield direction, exposed cores, death collision/cleanup, retry reset and bounded announced-reinforcement investigation with standing capsules; no controller/camera fixture required. |
| Mission rules | **Passed targeted automation after conversion** | `TROOPER_AUTOMATION_RESULTS.json`: finite 4+4+3+2 scheduling, five-alive/four-attacker caps, duplicate resolution, overload at 50 seconds, stop at 75 seconds, spatial spawn guard, hazard intervals, recovery and common cinematic finalization. Isolated world state tests do not replace a complete playable run. |
| Runtime mission smoke | **114/114 functional checks passed after conversion; scoped follow-ups** | `Saved/Level2/RUNTIME_SMOKE.json`, completed 21:31:01 UTC: `PASS`, 360.83 seconds, zero report errors/warnings. Includes native death/reload, the restarted thirteen-enemy finale, completion save and 41 live trooper audits covering thirty unique actors. This report predates the subsequent camera-only patch and Python harness binding change; both have separate focused evidence below. |
| Protected existing content | **Historical hashes; scoped source preservation** | `Saved/Level2/PRESERVATION_CHECKS.json` records the pre-conversion Level 1 map and `OBRifleAnimInstance.cpp/.h` as byte-identical to the BeforeDeadSignal backup. The current `.cpp` intentionally differs: a comparison with `Saved/Backups/BeforeTrooperConversion_20261113T211253Z/` shows only the added machine branch/state and action sampling. Existing player, Level 1 enemy and Wraith configuration bodies are unchanged. This is source preservation evidence, not a new runtime regression pass. |
| Trooper presentation | **Compiled, saved, previewed and checked while active** | `Saved/Level2/TROOPER_ASSET_AUDIT.json` records asset inspection. `TROOPER_MAP_REFRESH.json`, completed 21:24:50 UTC, records all thirty expected meshes/capsules and successful map save. The coordinator visually confirmed four grounded variants, rifle attachments, armor and final contoured shields. Final smoke checks confirm every trooper while active; comprehensive combat animation quality remains a manual check. |
| Python navigation diagnostic | **Handled ensure diagnosed; corrected binding probe passed** | `NAV_PYTHON_CONTEXT_PROBE.json`, completed 21:31:55 UTC: two valid projections and a complete two-point path using the actual PIE-owned navigation object, with no new ensure/error in the marked log span. The future smoke harness now uses that object-bound call and passes syntax validation. `docs/LEVEL_2_HARNESS_NAV_CONTEXT.md` preserves the original handled ensure and exact follow-up scope. |
| Rescue visual presentation | **Camera-only repair saved; all three shots visually checked** | `RESCUE_VISUAL_VALIDATION_RESULT.json`, completed 21:36:55 UTC: `PASS_SAMPLED_SHOTS`. It consolidates the patch/preview evidence: three cuts/15 seconds, fifteen clear sampled camera positions, unchanged crash sequence/gameplay actors, actual PIE samples at 1/8/12 seconds, unchanged campaign/development saves and the coordinator's readable Wraith/transmitter observations. The full mission was not repeated after this patch. |
| Freight reinforcement placement | **Saved; geometry and focused runtime passed** | `Saved/Level2/FREIGHT_SPAWN_PATCH_RESULT.json`, completed 21:07:43 UTC: reinforcement 205 moved behind the transfer-bend wall, with a complete 1,947 cm path and verified occlusion from gallery center/north-lane samples. `FREIGHT_RUNTIME_SMOKE.json`, completed 21:09:26 UTC: five checks passed, no warnings/errors; activation 1.327 seconds after the native two-death trigger, attack windup 9.51 seconds after activation, campaign hash unchanged. |
| Standalone application/package | **Not verified; finalization blocked** | Standalone native compilation/linking completed, but Xcode PostBuildSync exited 66. Its generated workspace exists; sandboxed `xcodebuild` reported that it was not a workspace and logged service/access errors. No current Level 2 packaged application has been validated. |

After the final trooper build/reload, `Saved/Level2/TROOPER_AUTOMATION_RESULTS.json` records five passed, zero failed, zero skipped, with no test errors or warnings. The following tests also passed in the earlier pre-conversion run recorded in `AUTOMATION_RESULTS.json`:

- `OrbitBreakout.Level2.Machines.ShieldsAndCleanup`
- `OrbitBreakout.Level2.Mission.FiniteWavesRecoveryAndHazards`
- `OrbitBreakout.Level2.Save.ChapterAndLoadoutRoundTrip`
- `OrbitBreakout.Level2.Save.LegacyMigration`
- `OrbitBreakout.Level2.ExactCampaignLoadout`

`NAV_VALIDATION_RESULT.json` and `TROOPER_MAP_REFRESH.json` retain the earlier navigation-save and actor-refresh hashes. The final camera-only save at 21:34:56 UTC is recorded in `RESCUE_VISUAL_VALIDATION_RESULT.json`: `L_DeadSignal.umap` is 2,600,314 bytes with SHA-256 `3c05c761b4f5809abae92f85d8fd7e8de2ffccbb8f300f5a2ba74bbad1419afd`; `LS_DeadSignal_Rescue.uasset` is 20,808 bytes with SHA-256 `e359376123e44a708cbd03b41215b616608b850631b861ce9393a3e152ef2319`. The crash sequence was unchanged. A presentation save is not a repeat of the full navigation suite.

## Runtime scope and remaining checks

The preliminary PIE run is preserved as `Saved/Level2/PRELIMINARY_RUNTIME_SMOKE.json`. It verified direct-map development-session isolation, the Wraith mesh, all 30 machines, the 42-second opening's camera targets, post-crash checkpoint/health restoration, one 1,288.76 cm movement-input traversal, Cutter/Watcher attack states, and measured 12 damage during 1.2 seconds in live water. It stopped at the cable breaker because the test harness attempted to read a non-reflected `hold_target` property. That preliminary result is a **failed harness run**, not a completed level pass. The campaign save SHA-256 was unchanged across that run.

Both completed scripted smokes used explicit teleports between test positions, injected native point damage to clear enemies, real-time calls into hold behavior with temporarily suspended pawn ticks, and increased observation health. They establish mission-state integration, not normal weapon aim, physical controller input, combat balance or a manual end-to-end run. Their Slate tick timings include editor overhead and are not representative game/GPU performance measurements.

The final post-conversion run demonstrated:

- The complete opening sequence without skipping (41.71 seconds observed), the Wraith mesh and thirty-trooper roster, and post-crash recovery/checkpoint.
- All thirty Sci-Fi Trooper meshes, native animation instances, standing capsule envelopes, rifle/bone attachments and hidden legacy bodies. Forty-one live audits across the route and retry cover thirty unique actors, confirming visible mesh/rifle and finite muzzle positions within the expected bounds.
- Movement-input traversal on the crash route and through all four opened mission gates, with six valid local runtime navigation queries. The remaining journey between test positions used logged teleports.
- Actual water damage before isolation (12 health over 1.2 seconds), zero afterward, and bounded vent damage (30 over a five-second observation).
- Breaker, pump, coupler/socket, auxiliary restart, Warden key and finale checkpoint progression.
- Native player death and map reload from the overloaded finale, restoration to standard 100 health, preservation of power/key progress, and reset of stabilizers, overload, timer and dormant finale roster.
- The repeated finale: overload clamped at 50 seconds, repair resumed from 50, exactly thirteen finale machines activated and resolved, and the complete rescue sequence without skipping (14.97 seconds, three camera targets recorded).
- Completion menu, valid completed development-session save, updated development save file and unchanged campaign SHA-256. The protected campaign hash was `e7b6fa92be43971567265228de80fe4e7f38d63a9eda37b9b0d1c93ea2c5f8e6` before and after.

The original `PRE_TROOPER_RUNTIME_SMOKE.json` passed 67 assertions in 404.41 seconds but contained three warnings: `L2_E211_04` did not reach an observed attack phase in either finale run, and `L2_E212_03` did not attack in the completion run before diagnostic damage. `CUTTER_APPROACH_PROBE.json` and `CUTTER_TOLERANCE_PROBE.json` then recorded the north-entry corner stall and a controlled tolerance comparison. After the 45 cm approach correction and standing trooper conversion, both actors reached Windup, Lunge and Recovery in both final smoke finale runs. First attacks occurred at 6.67 seconds for `E211_04` in both runs, and 6.68/6.67 seconds for `E212_03`. Those specific observation failures did not recur; the final report contains no enemy-observation warnings.

The final report's empty error/warning arrays describe its assertions, not the complete editor log. A handled editor ensure occurred at 21:25:49.548 UTC in the Python `NavigationSystemV1.project_point_to_navigation` class-function wrapper's world-context handling. The recorded stack contains editor/Python navigation context calls, not native machine combat. Navigation queries and actual movement continued. The separate PIE-owned navigation-object probe passed at 21:31:55 UTC with two valid projections, a complete path and no new ensure/error in its log span. The future harness was then changed only to use those object-bound navigation calls and passed syntax validation. The original full report is preserved unchanged and was not rerun solely for that binding change. See `docs/LEVEL_2_HARNESS_NAV_CONTEXT.md`. The five targeted native tests remain independently clean, with no test warnings or errors.

After that full smoke, `Scripts/patch_dead_signal_freight_spawn.py` backed up the map, builder and manifest, then moved only freight reinforcement `L2_E205_01` from `(11950,3500,110)` to `(12500,3950,110)` and added its maintenance-bay sign. The builder and manifest now match the saved map. Real visibility traces confirmed that the transfer-bend west wall hides the new position from gallery center and north-lane samples; the old location was exposed. South-lane and forward samples can still see the new location, and the native distance/visibility guard is unchanged. The candidate projects to floor and has a complete path to the freight fight. This patch followed a roughly 55-second activation delay in the full smoke. The full 67-assertion run predates this small placement/sign change.

The subsequent focused saved-map freight regression passed all five checks in 21.47 seconds, with no warnings or errors. The real encounter-204 two-death condition queued reinforcement 205, which activated after 1.327 seconds and progressed through Search, Approach and Windup; the first attack telegraph occurred 9.51 seconds after activation. The campaign save hash remained unchanged. This verifies prompt arrival from the tested gallery-center position. Prerequisites were set only in transient PIE, observation health was increased, and enemy damage was injected, so it is not a normal-health weapon-combat test.

The coordinator visually inspected the repaired impact around 24 seconds: Star Sparrow entered the tunnel portal with visible sparks/warm light and no black frame. The rescue sequence's timing, camera targets and completion behavior passed in the full smoke before its final camera-only repair. The final three shot compositions were then visually checked in PIE at 1, 8 and 12 seconds and showed Wraith/transmitter without the former black-frame/crossed-beam defect. This is sampled visual evidence, not a second full mission or uninterrupted playback run after the repair.

Still unverified at this documentation checkpoint:

- Full combat animation/death quality, dash entry/exit smoothness and ordinary weapon/close-cover muzzle behavior. The post-conversion mission smoke and all thirty live trooper attachment/muzzle checks have passed, but they do not replace manual combat inspection.
- Full player-driven traversal and combat with ordinary health/ammunition, including a complete retry run.
- Physical PS5 controller use, held-button feel, menu focus and right-stick down looking down in the actual device path.
- Full Level 1 boss → Star Sparrow departure → Level 2 travel in a normal campaign session, plus quit/reopen at every checkpoint.
- Uninterrupted end-to-end playback after the final camera-only rescue revision. Its three shots have focused PIE visual checks; the earlier full run verified timing, bindings and completion behavior.
- Wraith animation quality during every strafe/backward/diagonal firing combination and close-cover muzzle behavior.
- Representative crash, freight and finale frame times; the brief's 20–25-minute first-play pacing has not been measured.
- A newly cooked, staged, launched desktop package containing both chapters.

## Editor entry and packaging

Open the real `OrbitBreakout.uproject`, load `/Game/OrbitBreakout/Maps/L_DeadSignal`, then Play for a protected developer chapter session. Normal campaign entry is through Level 1's escape; direct chapter play does not establish that campaign travel has been playtested. Existing applications under `Builds/` predate this chapter unless a new package is explicitly verified.

At handoff, the coordinator verified `L_DeadSignal` open in the editor, PIE stopped and the editor showing All Saved. The campaign save hash was independently rechecked and remained unchanged.

Both maps are listed in `Config/DefaultGame.ini`. `/Game/OrbitBreakout` is always cooked, covering the Level 2 sequences and original assets; referenced Underground Tunnel, Wraith, Star Sparrow and other vendor assets are dependencies. Packaging inclusion is configured, not proof that cooking or launching has succeeded.

Build, navigation, presentation and runtime evidence are separate. Useful runtime log markers begin with `OB_LEVEL2_` and record actions, scheduling, activation, checkpoints and completion. Subsequent changes require appropriately scoped validation. The final rescue-camera repair and navigation-context probe have their own focused records and do not replace the preserved full-mission report.

### Standalone app finalization investigation

The November 13 game-target build recorded in `Saved/Logs/Level2GameBuild.log` completed 61 native actions, then failed the Xcode PostBuildSync step with exit code 66. Xcode reported that `Intermediate/ProjectFiles/OrbitBreakout_Mac_OrbitBreakout.xcworkspace` was not a workspace file. The same log contains denied Xcode/CoreSimulator log/cache access and unavailable service connections. No signing, entitlement, security or build-setting changes were made to work around those failures.

A read-only audit confirmed that the workspace XML parses, its referenced `ProjectFilesMac/OrbitBreakout (Mac).xcodeproj` exists, the shared OrbitBreakout scheme exists, and `plutil -lint` validates the project file. This rules out an absent reference or syntactically broken project file; it does not establish why Xcode refuses this workspace in the execution environment. The alternative Xcode connector's initial workspace-list request returned no result and was interrupted after approximately 584 seconds. No connector build was attempted.

Consequently a current standalone `.app`/packaged playthrough is **not verified**. The earlier game build predates the final AI investigation fix and trooper conversion. The editor module has since compiled successfully with the final trooper changes, but that is not standalone app finalization. Both maps remain listed for cooking. The coordinating editor/runtime validation proceeds independently; an old packaged build must not be described as the new chapter release.
