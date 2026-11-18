# Orbit Breakout: Titanfall 2-Inspired Upgrade Plan

**Purpose:** An implementation-ready design and engineering plan for adding fast, expressive movement and tighter first-person combat to the existing Orbit Breakout Unreal project. This is a plan only; no gameplay or asset changes are included here.

**Design principle:** Borrow movement readability, momentum, responsive shooting, combat rhythm, and the contrast between a fast pilot and a powerful mech. Keep Orbit Breakout's characters, story, level identity, weapons, enemy roster, settings, controls, assets, maps, and authored mission progression. Do not copy Titanfall characters, names, story, maps, UI, audio, or proprietary assets.

## 1. Project inspection: verified baseline

The inspected workspace contains the UE 5.8 project at `OrbitBreakout/OrbitBreakout.uproject`, with saved maps `/Game/OrbitBreakout/Maps/L_EmergencyDeparture` and `/Game/OrbitBreakout/Maps/L_DeadSignal`.

| Area | Verified in project or current documentation | Planning implication |
|---|---|---|
| Player | `AOBPlayerCharacter` uses the Wraith skeletal mesh and `UOBWraithAnimInstance`; camera-relative third-person movement, sprint, jump, aim, firing, reload, interaction and controller support already exist. Current movement defaults include 450 walk / 650 sprint, 500 jump Z, and 0.25 air control. | Extend its movement and camera modes behind tunable properties. Keep its existing health, inventory, checkpoint, save and director interfaces. |
| Weapons | ARC-9 and HELIX-12 have independent ammunition, editable tuning, aiming/hip spread, reload, obstruction-aware firing, authored rifle/hand attachment, and HUD names. The HELIX rack is before Level 1's solo Bull. | Add ADS and mech weapon presentation without changing weapon identity, acquisition, balance, or save compatibility in the first movement milestone. |
| Level 1 | Six-zone Emergency Departure flow, 15 troopers and one 2,400-health Bull in the current boss revision; terminal, checkpoints, escape pod ending, menu and settings are documented. | Use the existing corridors, Cargo, Reactor, and pod bay as movement teaching, traversal, and combat arenas. Preserve all gates and objective order. |
| Level 2 | Dead Signal has seven connected areas, Wraith, thirty sci-fi troopers, hazards, checkpoints, Star Sparrow wreck, and crash/rescue sequences. The existing end-to-end campaign travel is implemented in source and has save/load and scripted mission evidence, but a normal Level 1 boss → departure → Level 2 run is explicitly still unverified. | Integrate with the existing chapter handoff; do not replace the crash/rescue sequences. Require a real campaign transition regression milestone before declaring both chapters complete. |
| Assets | Wraith, Star Sparrow, sci-fi trooper, Bull, mannequin locomotion/fire/ADS assets, rifle assets, tunnel kit and sequences are present. `SM_StarSparrow01` is used by Level 2. | Reuse present assets. No new mech mesh or cockpit asset is verified. Mech body, hands, interior and animation suitability remain assumptions pending content-browser/editor inspection. |
| Settings and input | Existing mouse sensitivity, inverted vertical look, audio, quality, camera shake and reduced flashes are documented. Keyboard/mouse and contextual PS5 controller bindings exist. Physical controller feel and some controller behaviors remain unverified. | Preserve settings and mappings. Add any new options additively and test keyboard/mouse and an actual controller separately. |
| Generation and preservation | Level 1 generator tags `OB_GENERATED` actors and has safe validation/repair procedures; Level 2 has its own saved map and builder/manifest. | Movement updates must not regenerate or overwrite authored non-generated assets/actors. Keep Level 1 and Level 2 generation paths separate. |

Source and record references: `Source/OrbitBreakout/OBPlayerCharacter.h/.cpp`, `OBCore.h/.cpp`, `OBLevel2.h/.cpp`, `OBRifleAnimInstance.cpp`, `docs/BOSS_UPGRADE.md`, `docs/LEVEL_2_IMPLEMENTATION.md`, and `docs/PROJECT_CHECKPOINT.md`.

### Not verified / assumptions to resolve before implementation

- Exact Wraith locomotion blend graph coverage, root-motion configuration, and which parkour clips retarget correctly. Files exist, but asset suitability was not established by this read-only inspection.
- A production-ready mech skeletal mesh, cockpit interior, pilot hands, mech animations, and mech-specific weapon art. The Star Sparrow is a ship, not a verified mech.
- Whether the current map collision and ceiling clearance support wall-running, mantle vaults, slides, and the larger mech capsule everywhere.
- A player-driven complete two-chapter run, physical controller verification, final standalone package for current Level 2, and measured performance after this plan's changes.
- The exact current controller bug/repair set: controller support exists, but the docs list physical PS5 use, held-button feel, focus, and right-stick-down behavior as not yet physically checked. Preserve the current fixes and turn those into baseline regressions; don't infer a new defect.

## 2. Intended play experience and camera contract

### Pilot movement and views

1. **Traversal / parkour:** third person is the default. The player can read feet, ledges, wall contacts, landings, the Wraith silhouette, and nearby threats. Camera stays over the right shoulder with collision and a short lag response.
2. **Precision ADS:** while aim is held, blend to first-person over approximately 0.20–0.30 seconds. Align the camera with a weapon-specific sight transform; narrow FOV modestly and reduce turn sensitivity through the existing user setting. Releasing aim blends back to third person. If the weapon cannot ADS or the player is in a forced traversal state, use a documented fallback (brief shoulder aim after landing) and never trap the camera.
3. **Mech cockpit:** entering and piloting the mech switches to a dedicated cockpit camera and mech pawn. Exit returns control to the pilot at an authored safe exit point. The cockpit exposes readable health, heat/ammo, objective, and exit prompts. No view should cut to a camera inside geometry.

Camera transitions use explicit mode state, a single transition owner, and blend curves; they must not race Level Sequence camera control, death/retry, menus, or chapter travel. Camera shake follows the existing setting; comfort settings can disable traversal bob, FOV kick and nonessential shake.

### Movement vocabulary

Sequence abilities from low-risk to high-risk: responsive sprint and slide, coyote-time jump and variable jump height, mantle/vault, wall-run on tagged surfaces, wall-kick, then optional grapple only if a verified asset/design need remains. No ability is required to finish a mission unless the level has a clear alternate route and the ability is taught first. Do not add double-jump or grapple by default; keep the silhouette and control set readable.

## 3. Unreal architecture and ownership

Keep the current native C++ gameplay classes authoritative, with Blueprint children/data assets for tuning and presentation. This avoids duplicating mission/save behavior and lets the existing authoring workflow remain usable.

| Addition / owner | Responsibility | Must not own |
|---|---|---|
| `UOBMovementComponent` or a focused `UOBParkourComponent` on `AOBPlayerCharacter` | Movement mode state machine, buffered/coyote jump, slide, mantle, wall-run/wall-kick, surface queries, cooldowns, safe cancellation, movement debug state. Start with `UCharacterMovementComponent` custom movement modes; only add custom physics for a mechanic that cannot be represented robustly by built-in modes. | Mission progression, inventory, camera mode, save format. |
| `UOBCameraModeComponent` on pilot and mech | Mode requests, smooth blend, FOV, shoulder offsets, ADS transform alignment, camera collision/obstruction, transitions and interruption cleanup. | Movement authority, mission ownership, sequence playback. |
| `AOBMechCharacter` + `UOBMechCombatComponent` | Separate possession pawn, cockpit presentation, mech locomotion, health/heat/ammo, fire and exit rules. Use explicit pilot/mech state. | Replacing `AOBPlayerCharacter`, overwriting pilot inventory/save, or owning chapter sequence. |
| `AOBMechInteractable` / `AOBMechDock` | Mission-authored enter/exit location and objective gating; director checks mech availability and encounter state. | Generic unrestricted possession anywhere. |
| Existing `AOBMissionDirector` and `AOBLevel2Director` | Remain authoritative for mission phases, encounters, doors, checkpoints, chapter handoff and cinematic start/end. Add narrow events/interfaces for movement tutorial flags and mech beats only. | Tick-by-tick parkour simulation. |
| Existing `UOBSettings` / player controller / input setup | Preserve sensitivity, inversion, shake, reduced flashes, menus, context action behavior. Extend input mapping for slide/parkour/mech actions with device-equivalent bindings. | Duplicated settings storage or direct per-level hard-coded bindings. |
| Data assets: `DA_OBParkourTuning`, weapon ADS profiles, `DA_OBMechTuning` | Designer-tunable speeds, tolerances, camera transforms, damage, heat and cooldown values. Defaults remain in safe source if data is absent. | Hidden mission state or serialized transient state. |

Use `EnhancedInput` actions/mapping contexts if already used by the current actual input path; the project plugin is enabled, but do not assume the player currently uses it without checking. Otherwise retain current input binding style for the first pass and introduce Enhanced Input as its own isolated migration. Avoid a project-wide input rewrite bundled with movement physics.

### State model and invariants

Pilot modes: `Grounded`, `Falling`, `Sliding`, `Mantling`, `WallRunning`, `WallKicking`, `AimADS`, `Dead`, `InMech`, `Cinematic`, `Menu`. ADS is a camera/weapon stance layered over locomotion, not a movement mode. The movement component owns traversal mode; camera component owns view mode; director owns cinematic/gameplay permission. A mode transition must clear incompatible held inputs, timers, movement root motion and stale camera requests.

Required invariants: one possessed gameplay pawn; no fire during transition/death/menu/cinematic; no interaction hold completing after a movement/camera/possession interruption; parkour never bypasses a locked story gate; no camera cut changes checkpoint or inventory; retry restores the authored checkpoint and loadout; Level 2 chapter identity/save schema remains backward-compatible.

## 4. Movement physics and tuning plan

Treat the following as starting ranges for graybox tuning, not measured final values. Preserve current movement until each mechanic has an isolated test and can be enabled per level.

| Property | Starting range | Notes |
|---|---:|---|
| Walk / sprint | existing 450 / 650 cm/s | Keep familiar input response; tune animation and acceleration before raising sprint speed. |
| Ground acceleration / braking | 2,000–3,200 cm/s² | Existing acceleration/braking are 2,000; responsive starts, controllable stops. Avoid ice-like drift. |
| Air control | 0.30–0.45 | Current 0.25. Keep modest; enable enough steering to land on obvious routes. |
| Jump Z velocity | 500–560 cm/s | Current 500. Test reachable height and ceilings with Wraith's capsule. |
| Coyote / jump buffer | 0.08–0.12 s / 0.10–0.15 s | Small forgiveness window; consume buffer once and clear on death/cinematic. |
| Slide entry speed | >= 500 cm/s | Crouch/slide requires forward intent and grounded state; no instant speed exploit. |
| Slide speed / duration | 650–850 cm/s / 0.55–0.85 s | Preserve momentum by friction curve; cap and validate collision capsule clearance. Sprint release or jump ends slide. |
| Mantle reach | 90–150 cm horizontal, 40–130 cm vertical | Capsule sweep, ledge normal/surface tags, head clearance, navigation-safe landing. Use authored animation only after root-motion review. |
| Wall-run | 0.6–1.2 s; 500–700 cm/s | Require wall angle and forward speed; camera roll <= 6–8 degrees; stamina/cooldown prevents infinite loops. |
| Wall-kick | 350–550 cm/s lateral impulse plus jump | Direction away from detected wall; avoid forced aim cancellation beyond transition policy. |
| Mech walk / burst | 300–450 / 500–650 cm/s | Heavier acceleration, slower turning, deliberate step timing. No pilot parkour while possessed. |

Implement sprint/slide/jump first. Use swept capsule movement and a short floor probe. Mantle: forward sphere/capsule sweep, top-down/downward landing probe, capsule clearance test, target slope/collision check, then short controlled motion; abort cleanly if target becomes invalid. Wall-run: left/right capsule traces, surface normal, walkable/allowed tag and facing/velocity thresholds; project gravity along wall and retain forward momentum; detach on obstruction, timer, low speed, loss of surface, jump, damage (tunable), or director state change. Never set actor transform every frame through geometry. Use built-in movement networking/collision paths even though this project is single-player, so behavior remains deterministic and testable.

Animation: audit Wraith's existing locomotion and aim graph, then layer a small state machine for slide, mantle and wall contact. Prefer in-place clips plus code-driven motion initially. Use root motion only for short authored vaults after checking retargeting, collision sweep, and interruption. Do not retarget new animation assets before validating skeleton/retarget chain.

## 5. Camera, ADS, and weapon presentation

- Add `ADS` start/end using current aim input. Request camera mode; do not directly teleport the spring arm. Blend camera location/rotation/FOV and weapon sight alignment together; trace from screen center for aim, then reconcile the muzzle trace to retain obstruction handling.
- Keep aim trace and muzzle trace distinction explicit: screen-center target trace determines target point; muzzle-to-target trace determines actual obstruction/hit. Continue to respect `WeaponRange`, spread, fire cadence, reload, ammo, damage, and enemy damage APIs.
- First-person arms/weapon option: initially test whether current Wraith full-body mesh and rifle attachment clip safely in the close camera. If not, use a hidden-body plus dedicated first-person arms only after compatible existing assets are confirmed; do not create an unverified duplicate weapon mesh path by assumption.
- Preserve ARC-9/HELIX-12 profiles and selection. Author ADS transforms per profile. Test the held weapon at standing, moving, slide end, landing, recoil, reload, close wall, and shoulder/door occlusion.
- Recoil adds brief camera/weapon motion but leaves the existing camera-shake setting authoritative. Reduced-flashes setting suppresses bright ADS acquisition, muzzle flashes and mech warning effects without removing needed hazard readability.

## 6. AI and encounter response

Retain the existing trooper/Bull classes, AI navigation, awareness/hearing, squad alert and cleanup behavior, plus Level 2 Cutter/Watcher/Bulwark/Warden behaviors. Movement creates new player locations and vertical routes; it must not make enemies omniscient or silently grant them parkour.

1. Extend perception/nav queries to include a bounded vertical reach and optional `NavLinkProxy` traversal for selected troopers. Troopers can take simple stairs/ramps and vault only explicit low cover where a tested animation exists.
2. Keep existing sight, hearing, investigation report, alert, cover and attack-slot rules. Fire uses actual line of sight; don't let a trooper shoot through floor/cover because the player wall-runs above it.
3. Give AI a `CombatTargetPosition`/last seen location and tactical response to fast traversal: suppress the last valid location, move to authored elevated cover/NavLink, or reposition after reacquiring sight. No tracking through occluders.
4. For the Bull: preserve 2,400 HP, charged three-bolt pulse, rush and shockwave. Add clear vertical/side lane telegraph checks for a wall-running or sliding player. Do not enlarge attack hit volumes until manual balance shows need. Maintain its solo-gate condition and collision-swept non-homing rush.
5. Level 2's thirteen-enemy finale and finite spawn/attacker limits remain. Only add rare parkour links to a few eligible troopers after nav and spawn visibility regressions; keep reinforcement bounds and no-teleport behavior.

## 7. Mech design and mission integration

### Mech gameplay slice

Design a short authored sequence where the player enters an available mech, pilots in cockpit view through a combat space, uses one clearly differentiated heavy weapon, survives a limited encounter, then exits or the sequence returns the player to a safe authored state. Mech can be a mission asset/reward only once the actual mesh, interior and animation plan is verified. Star Sparrow remains a ship and cinematic vehicle; do not relabel it as the mech.

Define an `EOBControlMode` (`Pilot`, `Mech`, `Cinematic`) and possession handoff transaction: validate gate/state and exit transform; save pilot transform/rotation and desired camera; possess mech; switch input/camera and HUD; on exit, ensure safe floor/clear capsule, restore pilot and camera, release controls. On death, pause, retry, level travel or sequence, run one idempotent cleanup and persist only intended mission facts. Do not save transient mech possession unless save migration and retry tests explicitly support it.

Mech combat should initially reuse projectile/damage interfaces and existing enemy health/death resolution; add a distinct `EOBWeaponType` only if serialized enum migration is append-only and old save reads are tested. Prefer a component-level mech weapon profile so pilot inventory/save fields do not change. Mech damage/armor/heat is separate from pilot health/ammo; dismounting does not heal or refill the pilot.

### Level 1: Emergency Departure

- **Holding Cell / Security:** introduce sprint/jump and a generous low vault before the first combat gate. Keep ARC-9 pickup and opening flow.
- **Cargo Processing:** teach slide under a low obstacle and a short optional wall-run across an obvious gap; bypass route rejoins before checkpoint/encounter trigger. Do not skip required enemies or terminal.
- **Security Junction / terminal:** ADS precision beat with existing troopers behind existing cover; allow normal cover-level movement and preserve hold-to-bypass cancellation when damage/movement/range rules say cancel.
- **Reactor Passage:** HELIX pickup remains before encounter. Use broad perimeter cover and central Bull arena for slide/wall dodge, keeping the original 1.2 s rush and 1.35 s shockwave telegraphs readable. Mech is not required for this original boss fight.
- **Escape Pod Bay:** retain four troopers, hold-E pod, ending and checkpoint semantics. A brief authored mech assist/teaser is optional only if it does not delay departure or alter the established ending.

### Level 2: Dead Signal

- Keep the 42-second crash and 15-second rescue sequence, wreck restoration, Wraith, Star Sparrow, hazards, objective order, checkpoints and existing save isolation.
- First parkour tutorial begins only after crash control is restored. Junction low vault/slide routes may route around a hazard visually, but breaker and pump objectives remain mandatory.
- Freight gallery can teach wall-run with visible landing zone; preserve encounters 204/205, spawn visibility guard, checkpoint positions and route controls.
- Relay Warden and finale remain combat/mission gates. A mech may be staged in a visually clear bay before or after the Warden, but do not spawn one as a new dependency in the thirteen-hostile finale without dedicated balance/spawn tests.
- Rescue sequence remains the chapter's ending. Any future mech beat is additive and concludes before transmitter/rescue unless a separately approved story revision changes that flow.
- Verify campaign path from Level 1's Bull → pod → Star Sparrow departure → Dead Signal intro → playable control → checkpoint save/quit/reopen → rescue completion. Existing tests do not close this whole verification gap.

## 8. Controls and settings

Retain all existing controls and controller fixes. Suggested additive mapping (final buttons must be checked against current PS5 layout to avoid conflict):

| Action | Keyboard/mouse default | Controller behavior |
|---|---|---|
| Move/look | WASD / mouse | left/right stick |
| Sprint / parkour run | Left Shift | L3 hold or current sprint mapping |
| Jump / wall-kick | Space | current jump button; wall-kick uses contextual jump |
| Slide | Left Ctrl or C (pick the free existing binding after inspection) | Circle tap/hold only if not reserved for skip/menu; otherwise unused face button |
| Aim / ADS | RMB hold | current aim trigger |
| Fire | LMB | current fire trigger |
| Reload / contextual use | R / E | preserve current Square contextual priority and reload fallback |
| Mech enter/exit | E at authored dock | preserve contextual interaction button |
| Weapon select/cycle | existing 1/2/Q | preserve existing D-pad/shoulder selection |

Add settings only if needed: parkour camera motion (On/Reduced/Off) defaults Reduced; wall-run camera roll (0–8 degrees) with 0 available; ADS sensitivity multiplier (0.5–1.0, default 0.8) multiplied by current sensitivity; hold/toggle sprint and ADS only if compatible with input behavior. Persist through existing settings save path. Never reset user's sensitivity/inversion/audio/quality/shake/reduced-flash values as part of adding defaults. Ensure menu focus and controller input restore after ADS, possession, death and cinematic.

## 9. Milestones and dependencies

| Milestone | Work | Depends on | Exit gate |
|---|---|---|---|
| M0 — Baseline and asset audit | Record source/map/save hashes; inspect actual input implementation, Wraith animation graph/socket transforms, collision/ceilings, Level 2 handoff; identify candidate mech art or explicitly mark art gap. No content edits. | None | Baseline checklist and per-map protected content list; no unresolved asset claim. |
| M1 — Movement foundation | Component/state model, acceleration/braking tuning, coyote/buffer jump, sprint and slide; debug visualization and per-level enable flags. | M0 | Existing mission gates, damage, saves and control response unchanged; mechanics pass isolated functional checks. |
| M2 — Pilot animation and third-person traversal | Wraith animation audit, vault/mantle, wall-run/kick, collision and landing; tutorial routes in selected generated/owned map actors only. | M1; compatible clips and map clearances | Each mechanic can be interrupted/retried safely; optional paths rejoin without sequence or gate bypass. |
| M3 — Camera modes and precision ADS | Camera mode owner, smooth third-to-first ADS and return, weapon sight profiles, obstruction traces, camera settings. | M0 camera/input audit; M1 movement state events | All transitions, weapon traces, settings and menus pass; no camera overlap/cut defects in both maps. |
| M4 — AI vertical/traversal response | Bounded nav links, tactical reacquisition and Bull vertical telegraph validation. | M2 map traversal; current AI regression baseline | No omniscience, nav deadlocks or combat gate changes; current enemy behavior tests remain passing. |
| M5 — Mech asset and playable slice | Confirm/obtain authorized local mesh and cockpit resources; mech pawn, possession, HUD, combat profile, dock and cleanup. | M0 asset gate; stable input/camera; M3 camera framework | Mech entry, combat, exit, death, retry and save behavior work; no missing cockpit/animation assets. If art is absent, stop at a documented graybox prototype and label it. |
| M6 — Level integration | Place authored tutorial/mech beats in Level 1 then Level 2; preserve story gates, encounter counts, sequence tracks, checkpoints, generator boundaries. | M2, M3; M5 for mech; M4 for AI links | Both maps validate; generated and hand-authored content preserved; mission order and serialized state unchanged. |
| M7 — Regression, balance and package | Keyboard/mouse/controller, full campaign, checkpoint save/reopen, performance, camera comfort, build/cook/package. Tune from observed play. | M1–M6 | All acceptance criteria below met; report editor vs packaged evidence separately. |

Keep M2/M3 decoupled enough for parallel implementation only after M0 documents shared interfaces. M5 must not block the parkour/ADS upgrade if no mech asset exists; the team can complete pilot movement while the mech stays explicitly unimplemented.

## 10. Verification plan and tests

Do not infer success from compile or a scripted state test alone. Add focused native automation and a human play checklist; do not overwrite the user's save slot to run progression tests.

### Unit / native automation

- Movement state transitions: sprint, jump buffer/coyote once, slide start/stop/blocked capsule, mantle clearance/failure, wall-run valid/invalid surface, detach causes, wall-kick direction and cooldown.
- Physics envelopes: no NaN velocity; speed/vertical caps; stable landing; slope/ceiling; no transform teleport through blocking geometry; no infinite wall-run or repeated jump exploit.
- Camera: ADS blend both directions, sensitivity/inversion settings preserved, obstruction collision, mode interruption by death/menu/cinematic/Level Sequence, reduced shake/flash behavior.
- Weapons: target trace and muzzle obstruction, ARC/HELIX separate profiles and ammo, reload cancellation, fire cadence, current damage/health limits, aiming while jumping/sliding and weapon switch.
- AI: existing perception/hearing/investigation and squad behavior unchanged; elevated player detection only with line of sight; no shots through cover; nav-link timeout/fallback; Bull telegraphs damage at most once and retain solo-gate rule.
- Mech: exclusive possession, cannot enter when gate/cinematic/dead/occupied, exit-floor validity, no lost pilot, independent mech/pilot health-ammo, fire/damage interfaces, retry/death cleanup, save migration and checkpoint recovery.
- Mission and persistence: Level 1 six-zone sequence, terminal, Bull, pod and ending; Level 2 crash/freight/Warden/finale/rescue, finite spawn limits, chapter handoff, old schema read, quit/reopen each checkpoint. Keep existing isolated tests and do not relabel scripted tests as manual play.
- Controls: keyboard/mouse and controller mapping parity, contextual Square/reload priority, menu navigation/focus, right-stick inversion and vertical direction, held/released actions across pause/death/travel.

### Human play scenarios

1. Fresh Level 1 run: learn each ability from geometry without a tutorial wall of text; complete all six zones with standard health/ammo.
2. Cargo route: test sprint → slide → jump/vault → optional wall-run landing under combat; repeat after death/checkpoint.
3. Reactor: use ARC and HELIX; aim/ADS transitions; evade Bull rush and shockwave with ground and wall routes; verify fair telegraph readability.
4. Pod bay: hold/release/cancel interactions under movement, damage, pause and controller; finish ending unchanged.
5. Level 2: cinematic restores control correctly; test live water/vent, full freight routes, Warden, finale and rescue; verify camera never overlaps Wraith/Star Sparrow geometry.
6. Mech slice (if asset gate passes): enter/exit at dock, operate cockpit, fight authored enemies, sustain damage, exit/retry and confirm pilot progress/loadout.
7. Settings matrix: normal/reduced/off camera movement, existing shake/reduced-flash settings, low graphics, sensitivity/inversion, controller and keyboard/mouse.
8. Package check: cook both maps and required asset references, launch fresh package, complete campaign path and reopen a checkpoint. Report measured frame-time on representative hardware; do not claim from editor Slate timings.

## 11. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Wraith animation set does not support wall-run/mantle or ADS close-up | Audit before coding; use in-place/basic poses and conservative camera first; do not promise custom clips. |
| Existing narrow ship corridors/collision make high-speed parkour frustrating | Add short optional routes, keep broad authored landing zones, test capsule sweeps, preserve a ground route. Disable individual ability per zone. |
| First-person Wraith mesh clips camera or weapon | Start with camera proof scene and ADS weapon transform; use dedicated arms only if compatible assets are verified. |
| Movement bypasses encounter/terminal gates or checkpoints | Keep progression in directors and triggers; add route-rejoin markers and gate invariants; parkour never completes an objective. |
| More player speed breaks enemy accuracy/readability or projectile fairness | Preserve visibility, attack slots and telegraphs; tune enemy lead only from current visible velocity; manually test Bull. |
| Mech art unavailable or incompatible | M0 hard asset gate. Complete pilot upgrade independently; do not call a Star Sparrow a mech. Graybox is not final acceptance. |
| Possession/camera transitions race Level Sequences, respawn or travel | One control-mode transaction and idempotent cleanup; focused tests for all interruption points; directors remain owner. |
| Input rewrite regresses known controller fixes/settings | Keep current mappings and settings code as baseline; additive bindings; real hardware acceptance. |
| Level 1 generator overwrites authored changes or Level 2 map is accidentally rebuilt | Separate builders/manifests; snapshot hashes; only touch tagged actors; back up before any planned regeneration. |
| Perf and packaging risk on Mac/UE 5.8 | Profile in target device; test both architecture/build path in use; current Level 2 package is not verified, so package work is explicit acceptance scope. |

## 12. Acceptance criteria

The upgrade is accepted only when all applicable criteria below are evidenced:

1. Existing Wraith, Star Sparrow, sci-fi troopers, Bull, weapons, maps, settings, controller mappings/fixes, save compatibility and story flow remain present and loadable.
2. Third-person parkour is responsive and readable; sprint, slide, jump forgiveness, vault/mantle and wall-run/kick (if enabled) behave with collision sweeps, safe interruption, accessible fallbacks and no required route softlocks.
3. Holding aim enters first-person precision ADS smoothly and aligns ARC-9 and HELIX-12; releasing aim returns to third-person parkour view. Obstruction-aware weapon firing and independent ammunition behavior remain correct.
4. Mech uses an actual verified mech asset, has a usable cockpit view and distinct controllable combat loop, and safely returns pilot control. If the project has no authorized compatible mech asset, this criterion is explicitly pending; the pilot upgrade can still be accepted as a separate slice.
5. Troopers, Bull, Level 2 machine archetypes, mission directors, objectives, checkpoint positions, encounter counts and the order of all authored beats remain intact unless a separately approved content change specifies otherwise.
6. Both saved maps retain existing non-generated actors/assets and Level 2 cinematics. Validation and safe regeneration procedures do not destroy owner content.
7. Keyboard/mouse and physical controller pass the same mission and transition checks; current sensitivity, inversion, camera shake, reduced flashes, quality and audio settings persist.
8. Fresh and existing saves load, retries restore the correct checkpoint/loadout, and no stale movement/aim/mech state survives pause, death, cinematic, chapter travel or restart.
9. Automated movement/combat/mission tests pass, followed by an uninterrupted human campaign playthrough from Level 1 through Level 2 rescue; results distinguish editor and packaged behavior.
10. A current package (if target packaging is supported) cooks both maps, launches, and completes the mission; measured performance and any remaining limitations are reported honestly.

## 13. Recommended implementation order

First deliver a **pilot movement + camera/ADS slice** in a disposable test map or isolated test area, then tune and integrate into Level 1, then Level 2. Treat mech as a separate gated slice after the art audit. This order preserves the existing game while gaining the most distinctive Titanfall-inspired feel early: momentum-driven traversal, responsive camera control, and a clear shift from third-person movement to first-person precision.
