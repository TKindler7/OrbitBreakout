# Orbit Breakout pilot upgrade

## Playing the upgrade

Open OrbitBreakout.uproject in Unreal Engine 5.8 and press Play. The original main menu supports New Mission and Continue. The new options are under Settings. Teal marks identify optional vaults and wall-run surfaces. Every required objective remains reachable on foot.

| Action | Keyboard and mouse | DualSense binding |
|---|---|---|
| Sprint, then slide while moving forward | Hold Shift, press Left Ctrl | Hold L3, press R3 |
| Vault or mantle a marked ledge | Move toward its face and press Space | Approach and press Cross |
| Wall-run and kick away | Sprint and jump alongside a marked wall, then press Space | Sprint and jump alongside it, then press Cross |
| Aim through the equipped sight | Hold right mouse | Hold L2 |
| Fire in pilot or loader mode | Left mouse | R2 |
| Board or exit the loader during the Warden encounter | E | Square |

The loader is in the Warden bay after auxiliary power comes online. It has its own directional pulse cannon, armor and heat limit. Exiting returns Wraith to his boarding point with his existing loadout. The original loader model has an ochre armored cab, a smoked canopy, a salvage jaw, a pulse tool and piston-driven legs. The separate cockpit assembly keeps its frame and instrument lights around the screen edges. The walker animates its hips, knees and feet from movement speed.

## Milestone evidence

| Milestone | Implementation | Verification |
|---|---|---|
| M0 · Baseline | Recorded checksums for the original content, source, configuration and two existing save files. Audited Wraith, its native animation instance, its Muzzle_01 socket and the existing rifle and tunnel assets. | All 2,836 original content files match the baseline. Both map packages and both saves match individually. |
| M1 · Movement | Added jump buffering, coyote time, variable jump height and a momentum-dependent slide with capsule clearance. Walk and sprint remain 450 and 650 cm/s. | Native collision tests cover slide entry, crouched exit under a ceiling, standing when clear, buffered landing and a single coyote jump. |
| M2 · Traversal | Added marked mantles, timed wall-runs, directional wall-kicks, landing lockout and Wraith pose changes. Per-level parkour can be disabled on the director. | Native sweeps reject unmarked cover and blocked mantles, clear a marked ledge, kick away from the wall and reject an immediate repeat run. Hand-played route chaining remains open. |
| M3 · Camera and ADS | Traversal stays third person. Holding aim blends to first person. ARC-9 and HELIX-12 use separate sight data assets. Added motion intensity, wall roll and ADS sensitivity settings. | Native blend checks exercise the 68° ARC sight, 62° HELIX sight and return to 85°. Simulated mouse aim and fire kill a real guard with the existing traces. Exact visual alignment, reload clipping and comfort across both levels need human review. |
| M4 · AI | Troopers use bounded vertical perception and projected navigation destinations. Bull attack selection accounts for height. Existing sight, cover, attack and encounter rules remain in place. | Automated awareness, cover, Bull telegraph/sweep and solo-gate tests. Enemy vault links were deferred because there is no validated trooper vault animation for these obstacles. |
| M5 · Tunnel-service loader | Added a separate possessed pawn, cockpit, heat-limited cannon, armor, bounded movement and ejection in the existing Warden encounter. | Isolated tests cover exclusive possession, firing heat, armor depletion, ejection and pilot inventory. The campaign fixture also boards and exits at the actual dock. The original meshes and motion replace the first release’s graybox. See the follow-up results below. |
| M6 · Map integration | Added optional routes in Holding, Cargo, Reactor, Junction and Freight and a loader in the Warden bay using actors installed by each director in its own map. | The original maps are byte-identical. The campaign test ran the Bull, pod departure, tunnel crash, checkpoint reopen, Warden, generator overload/repair, all thirteen finale enemies and rescue. |
| M7 · Regression and release | Extended the campaign regression through both chapters. Added an opt-in packaged campaign test that requires an isolated save directory. | See release results below. Physical DualSense, uninterrupted human play and subjective camera comfort remain open. |

## Preservation

The original input file's SHA-256 is reproduced exactly after removing the two new Slide action mappings. The original engine, game and editor configuration files also passed their checksums. The verification copies used separate saves. Neither chapter required regeneration. The four new data assets are in Content/OrbitBreakout/Tuning and can be edited in Unreal.

| Protected file | SHA-256 |
|---|---|
| L_EmergencyDeparture.umap | `02a6a5cb56fbed47b79634eba30320f8e0f222d42ac53793f5b4dfa4a3e37b06` |
| L_DeadSignal.umap | `3c05c761b4f5809abae92f85d8fd7e8de2ffccbb8f300f5a2ba74bbad1419afd` |
| OB_DeadSignal_Development.sav | `b5506e5ec16907f65ba29940f9b1030b73d4b0728e06cfa072d6ae38b3dcff42` |
| OB_EmergencyDeparture.sav | `d458fb28fbef96e37bc310a93892ad1fdbbdf9e20f46275e119cf9ae3c7cda67` |

## First pilot build

- **Editor.** The 18 focused native tests and the separate two-chapter campaign test all returned Success on UE 5.8.4. A combined editor run hit an engine MassEntity assertion when starting PIE after the transient-world tests. The two suites ran to completion in fresh editor processes. Six tests recorded warnings, including meshless transient actors, crowd-manager initialization and an expected missing settings slot in the fresh profile.
- **Dead Signal navigation and recovery.** A separate rendered PIE run passed all 114 checks with zero warnings. It walked five route segments using movement input and checked paths through the opened mission gates, the 30-trooper roster, interactions, damage interruption, forced death and checkpoint retry, both passes through the defense, and the unskipped rescue. The harness uses additional positioning, increased test-pawn health and injected damage for combat. Evidence is in Saved/Upgrade/tunnel-navigation.json.
- **Existing save.** A new editor process opened a copy of the original campaign save, accepted Continue at Main and restored Wraith at Cargo checkpoint 1 with 24 rounds and ADS released. The actual map exposed three marked mantle surfaces, two wall-run surfaces and the Wraith muzzle socket.
- **Packaged Mac game.** UAT built the Apple silicon Development target from an isolated copy of the project source and configuration and cooked 1,127 packages with zero errors and 42 warnings. Warnings included localized text initialized from strings in the original maps. Explicitly running the package step before archiving was necessary on this Mac. The delivered 1.8 GiB app passes code-signature verification and contains the current cooked containers for both chapters.
- **Packaged campaign.** The app started at Main with a fresh temporary profile inside its macOS application sandbox. The campaign test exercised disk saves, Continue, death and retry, the boss and pod, the full tunnel crash, the Warden loader dock and all thirteen signal-defense enemies. The rescue cinematic played, completion went to disk and the test returned Success. The same driver uses simulated aim/fire on one guard, position changes and direct damage for later encounters.
- **Frame timing.** The packaged signal defense at 1280 × 720 on an Apple M5 Pro produced 4,498 automation tick samples with median 16.67 ms and 95th percentile 16.67 ms. These are frame intervals during the scripted defense at the game's frame cap. Uncapped headroom, GPU-only timing and a full human combat run remain unmeasured.
- **Evidence files.** Saved/Upgrade/editor-focused.json, editor-campaign.json, packaged-campaign.log, build-cook-archive.log, final-package.log and the two protected-file checksum reports.

## Loader release

The Mac Development app is in `Builds/LoaderRelease/Mac/OrbitBreakout.app`. Choose Continue to load available progress or New Mission to start at the holding cell. In Dead Signal, restore auxiliary power, approach the ochre loader in the Warden bay and press E or Square. Its pulse cannon has a separate heat meter. The existing on-foot Warden route is still available.

- **Art and integration.** Five original static meshes and eight material instances are in `Content/OrbitBreakout/Loader`. The editable Blender generator, FBX source and Unreal importer are in `ArtSource/Loader`. All five mesh assets resolve their reimport paths relative to the project, and their stored FBX hashes match the source files. Unreal’s import check measured the hull up to 132 cm above its pivot. The cockpit has its own camera-attached frame, consoles and status lights. The exterior uses articulated hips, knees and ankles.
- **Warden.** A packaged campaign test boarded and exited at the authored dock with simulated Square. It reboarded, aimed at the live Warden, fired the normal pulse cannon with simulated R2, and regained Wraith with the original health and ammunition. It then completed all thirteen finale enemies and the rescue. One repeat run lost the stationary loader to the Warden. The test driver now moves during marked pulses and reserves its heat budget for the exposed core. Other encounters use scripted positioning and direct damage.
- **Cold Continue.** The driver copied real checkpoint files during play and started a separate Mac game process for each one. Main → Continue restored Cell, Cargo, Reactor, Crash, Junction, Power and Finale. The checks covered location, control, and released traversal, ADS and loader state. They also checked the chapter-specific phase, inventory or gate flags. One earlier launch crashed in the engine allocator while AppKit was activating the app, before a game log existed. The same Crash checkpoint loaded successfully on retry. All seven cold launches in the final release run passed on their first attempt.
- **Controller bindings.** Eleven rendered PIE checks passed using simulated controller input on a copy of the Cargo save. The default right-stick down input lowered the view and inversion reversed it. L3 reached 650 cm/s, R3 entered a slide, L2 aimed at 68°, and R2 fired two rounds. Options paused, D-pad and Cross opened Settings, releasing L2 during pause cleared aim, and Square reloaded from 22 to 24 rounds. A physical DualSense was unavailable.
- **Camera.** The ARC-9 and HELIX-12 sight transforms use the measured rifle bounds to leave the stock beyond the near plane and the top rail under the reticle. The first-person rifle uses the existing ARC material. The HELIX adds its capacitor. Cover clearance includes the rifle tip. Focused tests exercise cover, reload, landing, the three wall-roll motion levels, and a zero-roll override.
- **Final build.** Unreal 5.8.4 built the Apple silicon Mac Development app and cooked 1,140 packages with zero errors and 42 warnings. The 18 focused editor tests passed. Five reported warnings. The packaged campaign passed in a fresh profile and the live Warden fight lasted approximately 20 seconds after the first shot window.
- **Final frame timing.** The packaged signal defense at 1280 × 720 on an Apple M5 Pro logged 2,421 samples, with a 29.80 ms median and 40.31 ms 95th percentile. The first pilot build recorded 16.67 ms for both. The runs did not control other system activity or capture a GPU profile, so the cause of the difference is unresolved.
- **Evidence.** Build, campaign, focused test and individual cold-start logs are in `Saved/Upgrade/LoaderRelease`. The in-game captures are `docs/LOADER_COCKPIT.png` and `docs/PILOT_ARC_SIGHT.png`. The exterior studio render is `docs/LOADER_MODEL.png`.

## Hands-on acceptance still open

- Play from a fresh slot through rescue without automation. Chain the Cargo movement lanes under fire, try the Reactor side wall against the Bull and compare the ground routes.
- On an actual DualSense, check R3 while holding sprint, Square interaction versus reload, inversion, menu focus and releasing held controls across pause, death and travel.
- Assess comfort and readability for both sights at doors, landings and reloads, with different wall roll, shake and reduced-flash settings.
- Assess cockpit readability, cannon feedback and difficulty during a human Warden fight.

## DualSense menu verification

The subsequent Mac Development build passed the packaged `OrbitBreakout.Package.GamepadFocus` test. From the main menu, simulated D-pad and left-stick events moved the selection, Cross opened Controls, and Circle returned to the main menu. This test exercised the game viewport and menu handlers. A physical-controller playthrough remains open.
