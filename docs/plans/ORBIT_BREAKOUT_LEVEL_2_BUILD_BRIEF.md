# ORBIT BREAKOUT — LEVEL 2: DEAD SIGNAL

**Owner:** Teddy Kindler  
**Prepared:** November 13, 2026  
**Purpose:** A self-contained creative and implementation brief for Astra Ultra to build a complete second level in the existing Unreal Engine project.  
**Status:** Proposed design, ready for implementation. This document does not claim that Level 2 has been built. Numerical values are initial tuning targets.

> **Instruction to the implementing agent:** Build the complete playable level described below inside the existing Orbit Breakout project. Inspect the current project first, preserve working gameplay and owner edits, and carry implementation through saved maps, compiled gameplay, connected mission travel, testing, and a playable result. Do not stop at a plan, empty map, code scaffold, or instructions for the owner to assemble the level.

## 1. The core request

Level 1 ends with the player escaping the prison ship aboard Star Sparrow. The escape fails: the damaged craft descends onto the nearby planet and crashes through an entrance into an underground tunnel network. A cinematic shows the crash and hands control back to the player beside the wreck.

The player must fight through the tunnels against new enemies, restore the abandoned facility's power, reach an emergency signal generator, and transmit a distress call. The mission ends when someone answers. Rescue itself belongs to a later level.

**The intended feeling:** The player escaped a controlled prison and landed somewhere older, darker, and more dangerous. The tunnels should feel like a real place with a history. The goal stays clear: get a signal out and survive.

### Non-negotiable outcomes

- Underground Tunnel is the primary environment kit for a separate, fully playable Level 2.
- An authored in-engine cutscene connects the Star Sparrow escape to the underground crash.
- Paragon Wraith remains the playable character, with correctly held weapons and smooth movement while firing.
- Level 2 introduces enemies with new silhouettes or clearly differentiated appearances and genuinely different combat behavior.
- The player traverses a varied, connected tunnel mission and reaches a functioning signal-generator finale.
- Existing Level 1, the bull boss, inventory, and settings continue to work.
- The mission has checkpoints, failure recovery, an ending, working keyboard/mouse and controller inputs, and packaged map inclusion.

## 2. Existing project and verified asset context

The original local project is located at:

`/Users/teddykindler/Documents/ChatGPT/Unreal Engine Game 1/OrbitBreakout/OrbitBreakout.uproject`

Treat this as a location hint; if the workspace is elsewhere, locate the real project. Do not create a replacement blank project or edit only the separate `Publishing/OrbitBreakout` copy.

| Item | Existing project context |
| --- | --- |
| Engine | Unreal Engine 5.8 association; inspect the installed version before building |
| Level 1 map | `/Game/OrbitBreakout/Maps/L_EmergencyDeparture` |
| Gameplay source | `Source/OrbitBreakout/` |
| Player | Paragon Wraith, `/Game/ParagonWraith/Characters/Heroes/Wraith/Meshes/Wraith` |
| Regular Level 1 enemies | Sci-Fi Trooper Man 03, `/Game/SciFITrooper_Man_03/` |
| Level 1 boss | Sci-Fi Beast bull, `/Game/SciFi_Beast06/` |
| Escape craft | `/Game/StarSparrow/Meshes/Examples/SM_StarSparrow01` |
| Level 2 environment kit | `/Game/UndergroundTunnel/` |
| Player weapon system | ARC-9 blaster and HELIX-12 plasma repeater |
| Existing native classes | `AOBPlayerCharacter`, `AOBEnemyCharacter`, `AOBMissionDirector`, `UOBGameInstance`, `UOBSaveGame`, `AOBPlayerController` |
| Existing animation code | `OBRifleAnimInstance.*`, including `UOBWraithAnimInstance` |
| Content/build scripts | `Scripts/bootstrap_content.py`, `Scripts/build_project.py`, `Scripts/verify_fab_cast.py` |

The Underground Tunnel folder was found locally with **45 structural mesh assets, 232 prop mesh assets, seven Blueprint assets, and `Demonstration.umap`**. It includes tunnel pieces, pipes, tracks, and `BP_Spline_Tunnel`. These counts establish available files, not proof that every asset is suitable or loads correctly. Inspect the demonstration map, dimensions, collisions, materials, and Blueprint behavior before choosing modules. Do not overwrite the vendor demonstration map or run its presentation sequences as gameplay.

Earlier project notes live in `docs/PRD.md`, `docs/FAB_CAST_UPGRADE.md`, and `spec/ORBIT_BREAKOUT_MASTER_BUILD_SPEC.md` in the parent workspace. The original PRD's one-level scope is superseded by this brief for Level 2. Inspect source and saved assets for current behavior rather than assuming those older documents describe every later change.

A prior isolated boss-intro camera automation test crashed in recursive `SetViewTarget` / `ClientSetViewTarget` calls. Investigate the fixture before relying on it; do not blindly rerun a known crashing test or treat it as evidence the playable intro has been validated.

## 3. Scope and creative direction

**Working title:** Dead Signal.  
**Setting:** An abandoned mining and freight network beneath the surface of Aurelia, the nearby planet visible during Level 1.  
**Player identity:** Wraith is the character model; Rook remains the protagonist's callsign.  
**Run-length target:** 20–25 minutes on a first successful playthrough, excluding retries. Measure actual pacing and adjust.  
**Mode:** Offline, single-player, third-person action adventure.  
**Budget:** Use installed assets and local tools. No purchases, new subscriptions, or paid external services.

The facility's automated security survived abandonment. Its machines treat the wreck as contamination and Rook as an intruder. Damage from the crash wakes previously dormant systems, creating a credible escalation as the player restores power.

The distress device is an **emergency signal generator**: a power core and control station linked to an old surface antenna through a surviving cable shaft. This explains how an underground facility can send a long-range radio transmission. Broken wall cables and signs establish the connection before the finale.

Use the tunnel kit's worn concrete, metal, utilities, signage, and practical lighting. Add restrained sci-fi interfaces and machinery consistent with Orbit Breakout. Strong materials, geometry, and lighting should carry the visual upgrade; avoid covering generic boxes with excessive glowing strips.

## 4. Campaign continuity and loadout

Level 1's launch becomes a chapter transition rather than the entire campaign's completion. Preserve the existing launch sequence where it works, then connect it to the crash sequence and Level 2. Display a clear chapter title: **LEVEL 2 — DEAD SIGNAL**.

The player retains ARC-9 and HELIX-12 ownership when actually acquired. Carry weapon selection and ammunition across travel, then provide a clearly visible emergency supply cache near the wreck to prevent a nearly empty Level 1 finish from making Level 2 unwinnable. The crash does not arbitrarily delete weapons or create an unexplained replacement blaster.

Use the existing health maximum. Start Level 2 at full health through an explicit emergency medical injector beat after the crash. This is a predictable chapter-start reset, not random cinematic damage. Preserve sensitivity, graphics, audio, accessibility, and inversion settings.

If a developer starts Level 2 directly for testing, supply a documented default loadout with both weapons, full ARC-9 magazine and 72 reserve rounds, and full HELIX magazine with two spare magazines, using the actual implemented capacities. Direct test entry must not corrupt normal campaign progress or pretend the owner legitimately unlocked campaign content.

## 5. Opening cutscene: escape, descent, impact

### Narrative and camera beats

Target **35–45 seconds**, including the short post-impact recovery. Use a saved Level Sequence or comparably editable authored sequence with deterministic timing. Do not depend on uncontrolled rigid-body simulation for the essential crash.

| Time | Beat | Visual and sound direction |
| --- | --- | --- |
| 0–5 s | Escape becomes unstable | Star Sparrow pulls clear of the prison ship. An engine flickers and the craft lists. Continue the established launch direction and lighting. |
| 5–11 s | Failure and destination | Cockpit close-up or exterior angle; alarms, damaged navigation display, planet filling frame. Rook: “Thrusters aren't responding.” |
| 11–18 s | Descent | Two economical exterior shots establish approach to the planet and a decommissioned freight portal cut into a rocky slope. Use a short cinematic transition to imply distance and time. |
| 18–26 s | Tunnel entry and crash | The craft clips the portal frame, shears off an outer panel, scrapes the tunnel, and slides into a service chamber. Sparks, dust, a brief white flash, heavy impact, then muffled audio. |
| 26–34 s | Wreck settles | Show the recognizable Star Sparrow wreck, red emergency lights, steam, and a blocked route back. Do not bury the player or gameplay camera inside geometry. |
| 34–42 s | Recovery and purpose | Rook exits or is revealed beside the damaged cockpit, uses the medical injector, checks a dead communicator, and sees an emergency-network sign. Rook: “No signal. There has to be a relay down here.” Blend into the gameplay camera. |

If cockpit interior or an exit animation is unavailable, use exterior angles, silhouettes, a brief cut to black, and a standing recovery pose beside the wreck. Keep the essential crash beats polished instead of inventing an unsupported full cockpit. Dialogue may use subtitles and radio effects if no licensed voice recordings are available.

### Cinematic requirements

- The active cinematic suppresses gameplay movement, fire, damage, enemy attacks, and accidental interaction. Pause and skip remain available.
- Provide a visible skip hint after two seconds. Hold the designated skip button for one second; use keyboard and controller equivalents and matching prompts.
- Skipping applies the same final world state as watching: wreck transform, debris visibility, intact player, loadout, objective, chapter state, and correct camera.
- Runtime objects used by the sequence are spawned or bound reliably after map load. Never leave a second player pawn or duplicate Star Sparrow in the scene.
- Turn off temporary cinematic cameras, sound loops, movement locks, and collision changes after completion, skipping, restart, or map travel.
- Retries from the crash checkpoint start in gameplay. A fresh chapter replay can replay the cutscene.
- Use modest camera shake and a reduced-shake setting where supported; no sustained strobing.
- Save a playable post-crash checkpoint before handing over control. Also retain a safe pending-transition state until the new map is ready.

## 6. Mission layout and progression

Build a distinct Level 2 map, proposed path:

`/Game/OrbitBreakout/Maps/L_DeadSignal`

The route should take the player through seven authored areas. Alternate narrow traversal with larger combat pockets. Provide readable landmarks, cover, camera clearance, and occasional views into future spaces. Avoid a single straight tube or repeated identical rooms.

| Zone | Space and purpose | Main activity | Target time | Checkpoint |
| --- | --- | --- | --- | --- |
| T01 | Crash chamber | Recover, resupply, learn the new objective | 2 min | After crash |
| T02 | Maintenance spine | First encounter with new security machines | 3 min | No |
| T03 | Pump and drainage junction | Route around electrified water; restore access | 3 min | After junction restored |
| T04 | Freight gallery | Larger fight with flanking lanes and cover | 4 min | No |
| T05 | Auxiliary power station | Recover a power coupler and restart the network | 3 min | After power restored |
| T06 | Relay approach | Cross a short hazard sequence; meet the elite defender | 3 min | Before final arena |
| T07 | Signal-generator chamber | Bring transmitter online and defend the uplink | 4 min | Finale entry |

### T01 — Crash chamber

Recognizable Star Sparrow wreck is the centerpiece, with damaged engines and a broken side panel. The collapsed entrance explains why the player must proceed forward. A breached wall, utility lighting, and tracks lead deeper into the facility.

Give the player a quiet, safe first moment. Place the emergency cache and objective sign directly on the route, not in an obscure corner. A damaged terminal displays: **EXTERNAL COMMS OFFLINE — EMERGENCY TRANSMITTER: SECTOR 07**. A short optional log explains that the antenna shaft still reaches the surface.

Objective: **Find the emergency signal generator.** No enemies attack during the cinematic handoff or the first few seconds of control.

### T02 — Maintenance spine

Use a bend, pipe alcoves, and a small service room. Introduce two Cutter drones, then two Watchers in a separate encounter. The first Cutter's approach is visible before it attacks. Teach audio and visual cues through play, without freezing the game for a tutorial wall.

Show cover and a side passage that rejoins the main route. This space tests moving while shooting without forcing a distant precision shot.

### T03 — Pump and drainage junction

A damaged power cable electrifies a shallow flooded section. Blue-white arcs and a warning light clearly identify the dangerous water. The dry walkway and elevated route remain legible.

Reach a breaker from the dry side and hold interact for two seconds to isolate the cable, then use the accessible pump control to lower the water and unlock the forward gate. Both controls are in this junction; there is no long backtrack. A Watcher and two Cutters apply pressure before the controls become safe to use.

Water damage target: 12 health per second while in the marked live zone, no damage after isolation. Damage must be visible, escapable, and capped by a consistent interval. Never require taking damage to progress. An incidental fall has an exit or a safe reset rather than a permanent trap.

Objective: **Isolate the damaged cable.** Then **Drain the junction and open the service gate.** Save the completed junction state at its checkpoint.

### T04 — Freight gallery

A broad rail tunnel opens into a freight loading pocket with machinery, broken carts, two approach lanes, and a short raised catwalk. Establish the exit before combat so the player's direction stays clear.

Initial encounter: three Watchers and two Cutters. One Bulwark emerges from a readable maintenance door after the first two enemies die. Maximum four enemies actively attack at once; others reposition or wait for an attack slot. Spawn reinforcements outside the camera view with an audible door cue, never behind the player within melee distance.

The Bulwark shields the main lane. Side cover and a clearly reachable catwalk angle let the player hit its exposed side or rear. Include one optional supply alcove that rewards exploration without hiding an objective-critical item.

### T05 — Auxiliary power station

The emergency transmitter has power but cannot sustain an uplink. Restart the auxiliary network using one replacement power coupler from a nearby maintenance cabinet. This is a short local objective, not a sprawling fetch quest.

Show the empty coupler socket and cabinet marker before the player searches. The coupler is a persistent mission item, uses the normal interact input, and cannot be dropped or lost. Protecting the coupler cabinet requires defeating two Watchers and one Cutter. Inserting it and holding the restart control for three seconds completes the objective.

Power returning changes the space: work lights come on, a fan starts, tunnel signage turns from red to amber, and a distant antenna status panel responds. This is a tangible reward and a preview of the final objective. Save power-restored state and refill enough ammunition for the remaining mission.

### T06 — Relay approach

A short service tunnel has intermittent pressure vents. Jets have a one-second hiss/light warning, a 1.5-second active phase, and a two-second safe gap. Provide a safe waiting pocket and a readable route; no mandatory precision platforming. Pressure-vent damage target: 15 per contact with a one-second repeat interval.

At the exit, introduce **one Relay Warden**, an elite Bulwark variant, in a larger antechamber. Give it a short five-second introduction with an optional skip, or a continuous in-world reveal that preserves player control. Do not repeat the bull or copy its full boss fight. The Warden guards a transmitter access key that is awarded automatically on defeat. It cannot fall out of bounds or require pixel hunting.

The final arena checkpoint is committed after the Warden is defeated, so a finale retry does not repeat this encounter or the vents.

### T07 — Signal-generator chamber

The room contains a central generator, two side stabilizers, a transmitter console on a modest raised platform, a cable shaft leading toward the surface antenna, and three readable enemy-entry doors. Use circular movement and several cover positions. No place should be both mandatory and exposed to every spawn lane.

The finale is fully specified in Section 9. End with a confirmed response from a rescue contact rather than another combat wave.

## 7. New enemy roster

These are design requirements, not claims that matching Fab enemy models are already installed. Inspect available content. Prefer suitable existing licensed meshes; if none exist, build cohesive original machine silhouettes from permitted meshes and simple components. Do not buy or download a new pack without authorization. Keep the mechanical differences even when art must use a fallback.

| Enemy | Role and appearance | Starting health | Attack and fair counterplay |
| --- | --- | --- | --- |
| Cutter drone | Low, compact maintenance machine with exposed moving arms; amber light | 70 | Rushes along the ground, pauses with a 0.65 s windup, then lunges. Deals 12 damage. Miss leaves 0.9 s recovery. Shoot during approach or sidestep the telegraphed lunge. |
| Watcher | Upright automated sentry with a distinct sensor head; red scanning light | 130 | Uses cover and fires a short visible projectile burst after 0.8 s aiming cue. Deals 8 per hit, 1.8 s burst cooldown. Break line of sight or flank while it reacquires. |
| Bulwark | Broad armored maintenance/security unit with an obvious forward shield; white spotlight | 260 | Shield reduces frontal damage by 80%; sides/rear remain vulnerable. Fires a slow, telegraphed arc projectile for 18 damage. After firing, exposes a bright weak point for 2 s. |
| Relay Warden | Single elite Bulwark, larger shoulder assembly and a unique silhouette/light pattern | 650 | Alternates shield advance, two marked floor pulses, and a slow charged shot. Pulse warning 1.2 s; 20 damage. Exposes its core for 3 s after the second pulse. No unavoidable room-wide damage. |

Tune these against actual weapon damage, magazine sizes, movement speed, and playtest time. Basic enemies should not become bullet sponges. A clean HELIX burst should feel meaningfully stronger than ARC-9 fire.

### Shared AI and animation requirements

- Enemies can patrol or idle, investigate a sound, detect the player through valid sight, chase, attack, search after losing sight, and return or hold position. They do not track through walls indefinitely.
- Nearby enemies can receive an alert within a defined local radius. Never wake the entire level from one shot.
- Use navigation and valid collision. Check the actual kit's collision before building navmesh. Ground drones must not float, tunnel through walls, or climb inaccessible geometry.
- Allow deliberate door-based reinforcements; reserve camera-hidden spawn points that cannot appear directly on top of the player.
- Preserve shared attack-slot limits. The Warden fights alone; regular enemies do not enter its arena until it is defeated and the player has moved onward.
- Movement, attack, hit reaction, and death have coherent animation or mechanical component motion. Avoid skating or T-poses.
- Death disables attacks and collision immediately, then a short spark/dissolve or shutdown effect removes the body within two seconds. Stop timers and unregister AI references. No gore.
- If a fight-critical enemy becomes unreachable, detect and recover its navigation or relocate it safely at its entry point with an appropriate cue. Never silently unlock the objective while a visible enemy still fights.

## 8. Player combat, camera, and controls

Retain the responsive third-person shooting foundation. Use Wraith's compatible combat locomotion and weapon socket. Recoil affects the upper body while legs continue locomotion; moving diagonally, strafing, reversing, and firing must not twist the entire character or snap facing on every shot. Check muzzle direction, hand contact, and cover collisions in actual play.

Use existing weapon switching and reload behavior. If the character's compatible reload clip is unavailable, use a restrained supported presentation; do not force an incompatible skeleton animation. HELIX attachments must follow the same gun coordinate frame as the base weapon.

The narrow tunnels need a collision-aware camera: retract smoothly near walls, preserve aim direction, and avoid moving through ceilings or inside Wraith. Avoid sudden left/right shoulder switches. If the gun is blocked by nearby cover, do not allow projectiles to originate beyond the obstruction.

Support keyboard/mouse and the connected PS5 controller through Unreal's actual input path. Reuse established mappings where present; default controller mapping if absent:

| Action | Controller |
| --- | --- |
| Move / look | Left stick / right stick |
| Aim / fire | L2 / R2 |
| Jump | Cross |
| Interact / reload | Square: contextual interaction when valid; reload otherwise; hold Square for timed objectives |
| Sprint | L3 |
| Switch weapon | Triangle |
| Pause | Options |
| Skip cinematic | Hold Circle, with a dedicated cinematic binding |

**Right-stick down must look down by default.** Keep optional invert-Y separate for controller and mouse if supported. Apply sensible dead zones and time-scaled stick look; menu focus and game viewport focus must work. Never let an interaction accidentally reload or a skip input trigger a gameplay action on the next frame.

## 9. Signal-generator finale

### Stage A — Start the uplink

The player arrives with the Warden's access key and restored auxiliary power. Show the inactive console, antenna cable shaft, and two stabilizers. Initial objective: **Activate both signal stabilizers.**

Each stabilizer uses a 1.5-second hold interaction. Partial hold progress resets if interrupted; a fully activated stabilizer remains active. Activate in either order. The first activation starts a small, finite opening fight: two Watchers and two Cutters, arriving through announced entry doors. Both stabilizers must be active and the opening fight cleared before the transmitter console can begin the uplink.

### Stage B — Defend the signal

Hold the central console for two seconds to begin a **75-second transmission**. The HUD shows a labeled uplink progress bar. The player is then free to move, shoot, and use cover; they do not have to remain glued to the console.

| Uplink moment | Event |
| --- | --- |
| Start | Wave 1: two Watchers and two Cutters |
| At 30 seconds of uplink progress | Wave 2: one Bulwark and two Cutters |
| At 50 seconds | A scripted overload pauses transmission; one stabilizer flashes amber and gains a clear objective marker |
| After overload repair | Final wave: two Watchers; transmission resumes from 50 seconds |

No more than four enemies actively attack at once and no more than five are alive in the arena. Queue scheduled reinforcements until space is available. Waves spawn once; overload never repeats. There are **13 total finale enemies**, including the four in Stage A. Repair requires a two-second hold at the marked stabilizer. The player does not restart from zero after repair.

The generator is not a second health bar. Enemies attack the player; the finite combat, exposed repair interaction, and shifting positions create the challenge. This avoids an opaque escort-style failure condition. Supply one medicine pickup and an ammo cache placed away from the repair point so repositioning matters.

At 75 seconds, finish the current finite encounter if enemies remain. Display **Signal established — clear remaining hostiles**. No additional waves spawn. Once the uplink is complete and all finale hostiles are dead, disable combat spawning and play the ending.

### Stage C — Someone answers

Target 12–18 seconds. Rook returns to or faces the console. A green carrier light replaces the alarm. An overhead view follows the cable toward the surface antenna, then cuts back to the exhausted player.

Suggested subtitle exchange:

- **Rook:** “Emergency channel. This is Rook. My ship is down. I'm beneath the Aurelia freight tunnels. Does anyone read?”
- **Rescue:** “Rook, we read you. Your signal is weak. Hold your position. We're coming.”
- **Rook:** “Copy. I'll be here.”

End on **LEVEL 2 COMPLETE — SIGNAL RECEIVED**, with Continue (if a later level actually exists), Replay Level 2, and Main Menu. Do not expose a broken Continue button or claim a rescue mission exists. Save completion before showing the result screen. The end sequence is skippable to the same completed state.

## 10. Objectives, state, and progression rules

Use one authoritative, ordered mission state machine, with separate flags for actions that may complete in either order. Proposed states:

`CrashIntro → ReachJunction → RestoreJunction → CrossFreight → RestoreAuxPower → DefeatWarden → ActivateStabilizers → TransmitSignal → ClearFinalHostiles → SignalReceived`

Relevant persisted flags include crash-intro completion, junction isolation/drainage, coupler pickup/insertion, auxiliary power, Warden defeat/key award, each stabilizer activation, and Level 2 completion. Finale checkpoint behavior is defined below. Keep transient cinematic locks, attack slots, timer handles, and spawned runtime actor pointers out of the save file.

Objective markers should guide the current actionable target rather than reveal every later item. Distinguish locked, available, and completed controls through appearance and prompt copy. Keep one main objective visible, with a concise contextual instruction when needed.

An interaction should validate mission state, range, view, actor validity, and line of sight. Holding interact must cancel safely on leaving range, damage if the current design interrupts holds, death, pause, travel, or entering a cinematic. Grant pickups and progression events once. Ensure every critical gate has a valid completion path.

## 11. Checkpoints, campaign saves, and recovery

Extend the current save format with a schema version, explicit chapter/map identity, stable checkpoint IDs, and mission flags. Existing Level 1 saves with no chapter field should migrate to Level 1. Do not reinterpret a Level 1 checkpoint integer as a Level 2 checkpoint.

Checkpoints:

1. **Crash site:** after the intro, loadout restored and player safely positioned.
2. **Pump junction cleared:** power isolated, water drained, gate opened.
3. **Auxiliary power restored:** coupler inserted, relevant earlier fights cleared.
4. **Finale entry:** Warden defeated/key awarded, both stabilizers reset to inactive, all finale waves and uplink progress reset.

A death during the finale reloads the finale-entry checkpoint and restarts Stage A consistently. Do not accidentally retain a partial timer with missing enemies or leave previously activated controls unusable. Save after clearing a milestone, never in the middle of a dangerous animation, fall, attack, or cinematic.

Store loadout and mission state with stable IDs. On retry, reconstruct doors, water hazards, lights, pickups, enemies, and objectives from that state. Health and ammunition get the existing checkpoint replenishment treatment, tuned so no checkpoint creates an impossible low-ammo loop.

Travel from Level 1 must be idempotent: a skipped launch, repeated completion callback, failed map load, or quit/reopen cannot duplicate the chapter transition or delete the last working save. Validate the destination map before committing a completed handoff. If saving fails, keep session progress and show a short accurate warning; do not report disk persistence as successful.

## 12. Environment, lighting, audio, and performance

### Visual composition

- Use the Underground Tunnel kit as the visible architecture throughout; avoid simply placing its demonstration map behind a new start point.
- Each zone has a distinct landmark: wreck, cable spine, flood pit, freight crane, power turbines, antenna conduits, signal core.
- Palette: cold industrial ambient light, amber navigation and maintenance lights, red enemy alerts, green confirmed signal. Lights need believable fixtures.
- Combat spaces are readable at normal display brightness. Dark mood must not hide hazards, enemy windups, exits, or interactable controls.
- Use decals, damaged panels, cable runs, occasional steam, grime, and sparse debris to create history. Avoid dense clutter in navigation lanes and repeated identical props at identical intervals.
- Major passages should generally give at least 250 cm clear width, expanding to 350–450 cm where the third-person camera needs it. Check the actual kit dimensions and capsule before scaling. Combat chambers need larger turning and flanking space.
- Ground the Star Sparrow wreck in debris with authored collision. Keep its identity recognizable. The crash opening and cave-in must make spatial sense.

### Audio

Use restrained ambient rumbles, pipe ticks, distant ventilation, and water. Introduce different enemy signatures: Cutter motor, Watcher acquisition chirp, Bulwark charge. Hazards and spawn doors need directional warnings. Music escalates during freight combat and the uplink, then clears for the rescue response. Subtitles include speaker labels and remain readable over effects.

### Performance

Target a stable playable 60 fps at the machine's reasonable medium settings, and report measured performance rather than promise a frame rate. Avoid unrestricted dynamic shadow-casting lights, oversized fog volumes, constant full-scene physics, and excessive simultaneous particles. Use static geometry instancing where appropriate, sensible LODs, collision simplification, and distance culling for incidental effects. Profile the crash and finale separately. If 60 fps is not reached, report measured settings and the bottleneck.

## 13. Implementation architecture and editing boundaries

Inspect how `AOBMissionDirector`, `UOBGameInstance`, map reload, current menu flow, and save validation work before adding Chapter 2. Existing code currently has Level 1-specific zones and completion behavior; extend chapter routing explicitly rather than forcing Level 2 into the old numeric zone assumptions.

Prefer a dedicated Level 2 director or a clear chapter-specific state component/data asset behind a shared interface. Reuse tested player, damage, enemy awareness, inventory, interaction, and UI foundations. New enemy types should be configurable archetypes rather than duplicated copies of the entire base AI.

Create an organized Level 2 content area for the playable map, sequences, mission data, enemy variants, UI additions, and original effects. Keep vendor assets in their existing folders. Use saved Level Sequences with editable camera cuts and bindings. Separate mission-critical collision from decorative debris.

Preserve existing work from other active sessions, including controller mapping and look-direction fixes. Inspect changes before replacing source files. Do not revert the player to a mannequin, switch troopers back to older meshes, remove HELIX, or remove the existing bull fight while connecting chapter travel.

If using generation scripts, make them repeatable and scoped to Level 2-generated actors/assets, with stable tags and IDs. Never delete every actor in the current map. Save the resulting `.umap` and `.uasset` files; a script that has not run is not a completed Unreal level.

Suggested deliverables:

- Saved `L_DeadSignal` map and any required cinematic maps/sublevels.
- Opening crash and ending transmission sequences, with working skip behavior.
- Three new enemy archetypes plus the Relay Warden variant.
- Signal generator, stabilizers, coupler, breakers, hazard controls, and mission routing.
- Updated campaign travel, save migration, controller prompts, menus, packaging map list, and tests.
- Scoped Level 2 construction/validation scripts if used.
- `docs/LEVEL_2_IMPLEMENTATION.md`, a playtest record, and an honest list of remaining issues.

No website or portfolio publishing is part of this build request. Do not upload licensed Fab source assets to a public repository. Build locally and use the project's existing source-control practices for permitted files.

## 14. Build order and completion gates

### Gate 1 — Inspect and protect the existing game

Identify the active project, engine, working map, current source changes, asset licenses, and current save/input behavior. Record a recoverable local baseline using available project/source-control mechanisms. Verify that the intended project launches before large changes.

### Gate 2 — Complete the connected greybox

Create all seven spaces, valid navigation, gates, objectives, checkpoints, campaign travel, and a complete start-to-signal loop. Use temporary shapes only during implementation. Finish the actual route rather than polishing one chamber while the rest is missing.

### Gate 3 — Implement enemies and mission mechanics

Add the new enemy behaviors, the Warden, electrical/vent hazards, power coupler, and the bounded uplink encounter. Validate ammo and health availability and all recovery paths.

### Gate 4 — Author presentation

Replace temporary architecture with Underground Tunnel modules, add the Star Sparrow crash, finished lighting, effects, sound, subtitles, objective prompts, and signal-received ending. Fix camera behavior in narrow spaces and animation during moving fire.

### Gate 5 — Test, save, package, and hand off

Build successfully, save all required maps/assets, run targeted tests, play the complete chapter, and verify an appropriate desktop package if the local toolchain supports it. Capture actual observations and remaining limitations. Editor playability and packaged playability are separate claims.

Proceed through these gates autonomously within authorized local scope. Ask only for genuinely missing access, assets, or a consequential decision that cannot reasonably be inferred. If interrupted, leave a precise progress note with the next executable step.

## 15. Acceptance checklist

The level is complete only when these behaviors are demonstrated or clearly marked unverified with a concrete blocker.

### Campaign and cutscenes

- [ ] Completing Level 1 launches the correct Star Sparrow and transitions once into Level 2.
- [ ] The cutscene visibly establishes descent to the planet, tunnel impact, wreck, and recovery.
- [ ] Watch and skip paths produce equivalent mission state, loadout, camera, and collision.
- [ ] Pausing, quitting, retrying, and skipping do not leave input or camera locks behind.
- [ ] A direct Level 2 developer launch works without overwriting legitimate campaign progress.

### Traversal, combat, and presentation

- [ ] All seven zones are connected and traversable with Wraith's capsule and camera.
- [ ] Actual Underground Tunnel assets form the playable environment.
- [ ] Wraith moves and fires forward, backward, sideways, and diagonally without full-body recoil or aim snapping.
- [ ] Weapons stay held correctly and shots cannot pass through close cover by spawning beyond it.
- [ ] New enemies have readable appearances, different behaviors, valid navigation, fair telegraphs, and working death cleanup.
- [ ] The Warden fights alone and awards its key exactly once.
- [ ] Water and vents warn before damage and offer safe routes; no fall permanently traps the player.
- [ ] Keyboard/mouse and a connected PS5 controller can complete the mission. Right-stick down looks down by default.

### Objectives, finale, and saves

- [ ] Every objective updates once and every gate opens from the intended action.
- [ ] Coupler progress, junction drainage, power, and Warden defeat restore correctly from checkpoints.
- [ ] Either stabilizer can be activated first; interrupted holds reset predictably.
- [ ] All finale waves are finite, spawn once, obey alive/attack limits, and cannot appear on the player.
- [ ] Uplink pauses once at 50 seconds, repair resumes from 50 seconds, and no new wave spawns after completion.
- [ ] Death at every finale stage resets consistently to the finale-entry checkpoint.
- [ ] Completion requires a successful uplink and cleared remaining hostiles, then plays the rescue response.
- [ ] No broken Continue button or nonexistent third level is exposed.
- [ ] Existing Level 1 saves migrate safely and Level 1 remains playable.
- [ ] Quit/reopen and retry work at every checkpoint; save failures produce an accurate warning.

### Delivery evidence

- [ ] Native build passes with the intended engine version.
- [ ] Saved maps and sequences exist, reopen, and contain their actual intended assets.
- [ ] Targeted automation covers mission transitions, save migration, skip finalization, bounded wave scheduling, and finale retry cleanup where practical.
- [ ] A full successful Level 2 playthrough and a retry run are recorded in the playtest notes.
- [ ] Performance is measured in the crash, freight fight, and final uplink.
- [ ] A desktop package includes both chapter maps and sequences, if packaging is available; otherwise the exact blocker is stated.
- [ ] Final handoff explains what was built, where to open/play it, what passed, and what remains unverified.

## 16. Copy-and-send prompt

Use this message when attaching this file to Astra Ultra in the project workspace:

> Build Level 2 of my existing Unreal Engine game, Orbit Breakout, using the attached build brief. Underground Tunnel is already imported. Connect Level 1's Star Sparrow escape to the crash cutscene, build the complete tunnel mission with new enemies, and finish with the working signal-generator defense and rescue response. Preserve my existing Wraith player, weapons, Level 1 boss, settings, and controller improvements. Make reasonable decisions, implement the full level, save the real Unreal assets, test it, and leave it ready for me to play. Report honestly what you verified and anything still blocked.
