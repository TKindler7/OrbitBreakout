# ORBIT BREAKOUT

## Complete product requirements and autonomous implementation specification

**Document version:** 1.0  
**Prepared:** November 5, 2026  
**Owner:** Teddy Kindler  
**Deliverable:** An original, playable single-player third-person spaceship escape level in Unreal Engine 5  
**Working project name:** `OrbitBreakout`  
**Working mission name:** `Emergency Departure`  
**Intended use:** Attach this entire Markdown file to a new Codex chat in a local project workspace and write **“build this.”**

This file is the complete brief. The implementing agent must be able to work from this file without access to the conversation that produced it. Product choices have defaults. Numeric values are initial tuning values, not claims that the game has already been built or tested. This document requests implementation when the owner supplies it with a build instruction; its existence alone does not mean a project has been created.

## Contents

1. [Instructions to the implementing agent](#1-instructions-to-the-implementing-agent)
2. [Product vision](#2-product-vision)
3. [Scope, priorities, and exclusions](#3-scope-priorities-and-exclusions)
4. [First-run experience and complete mission flow](#4-first-run-experience-and-complete-mission-flow)
5. [Story, setting, and presentation copy](#5-story-setting-and-presentation-copy)
6. [Player controls and input behavior](#6-player-controls-and-input-behavior)
7. [Camera, aiming, and character presentation](#7-camera-aiming-and-character-presentation)
8. [Player health, damage, and survival](#8-player-health-damage-and-survival)
9. [Player weapon: ARC-9 security blaster](#9-player-weapon-arc-9-security-blaster)
10. [Ammunition, medicine, and supply safety](#10-ammunition-medicine-and-supply-safety)
11. [Enemy system](#11-enemy-system)
12. [Encounter design and roster](#12-encounter-design-and-roster)
13. [Space planning and level construction standards](#13-space-planning-and-level-construction-standards)
14. [Z01 — Holding Cells](#14-z01--holding-cells)
15. [Z02 — Security Corridor](#15-z02--security-corridor)
16. [Z03 — Cargo Processing](#16-z03--cargo-processing)
17. [Z04 — Security Junction](#17-z04--security-junction)
18. [Z05 — Reactor Passage](#18-z05--reactor-passage)
19. [Z06 — Escape-Pod Bay](#19-z06--escape-pod-bay)
20. [Mission state, objectives, and progression](#20-mission-state-objectives-and-progression)
21. [Checkpoints, saves, and recovery](#21-checkpoints-saves-and-recovery)
22. [User interface specification](#22-user-interface-specification)
23. [Art direction and asset plan](#23-art-direction-and-asset-plan)
24. [Audio and effects](#24-audio-and-effects)
25. [Unreal environment discovery and project setup](#25-unreal-environment-discovery-and-project-setup)
26. [Runtime architecture and ownership](#26-runtime-architecture-and-ownership)
27. [Content organization and editable project structure](#27-content-organization-and-editable-project-structure)
28. [Content generation and reproducibility](#28-content-generation-and-reproducibility)
29. [Performance and platform goals](#29-performance-and-platform-goals)
30. [Build, package, and launch requirements](#30-build-package-and-launch-requirements)
31. [Implementation milestones and completion gates](#31-implementation-milestones-and-completion-gates)
32. [Autonomous working procedure](#32-autonomous-working-procedure)
33. [Functional acceptance tests](#33-functional-acceptance-tests)
34. [Automated checks and diagnostic tools](#34-automated-checks-and-diagnostic-tools)
35. [Playtesting and tuning procedure](#35-playtesting-and-tuning-procedure)
36. [Risk register and concrete fallback decisions](#36-risk-register-and-concrete-fallback-decisions)
37. [Definition of done and deliverable inventory](#37-definition-of-done-and-deliverable-inventory)
38. [Future editing boundaries](#38-future-editing-boundaries)
39. [Quick requirement cross-reference](#39-quick-requirement-cross-reference)
40. [Final execution directive](#40-final-execution-directive)

---

## 1. Instructions to the implementing agent

When the owner says “build this,” implement the game described here in the selected local workspace. Create the project, gameplay, playable level, assets, user interface, configuration, tests, and documentation. Run the available build and verification steps, repair defects, and deliver the most complete verified result the environment supports. A proposal, scaffold, collection of code snippets, or instructions for the owner to assemble everything is not the requested deliverable.

Work through the implementation milestones in this document during the same task as far as available tools, dependencies, and usage allowance permit. Do not stop after the first milestone simply to ask whether to continue. Make normal design and engineering decisions using the defaults in this file. Keep a progress record so work can resume accurately after an interruption.

The owner is a full-time student. Minimize the amount of setup, repetitive editor work, and technical troubleshooting they must perform. The owner's primary involvement should be launching a build, playing it, and giving creative feedback. Never claim that this eliminates the need for human playtesting or that one prompt guarantees a finished game on every machine.

### 1.1 What success means

The owner receives an actual Unreal project containing a connected, complete mission. They can start a new game, move a visible third-person character, collect a blaster, shoot enemies, survive several encounters, activate a terminal, use checkpoints, enter an escape pod, see a short ending, and replay. The game should look like a deliberately designed sci-fi prototype, with coherent lighting and readable rooms.

The preferred final deliverable also includes a locally runnable desktop build for the development machine's operating system. Editor playability and packaged playability are separate claims; verify both where possible.

### 1.2 Decisions already made

| Decision | Default |
|---|---|
| Engine | Unreal Engine 5, compatible stable version already installed |
| Perspective | Third-person, over the right shoulder when aiming |
| Mode | Offline single-player |
| Platform | Desktop, initially the development machine's OS |
| Controls | Keyboard and mouse |
| Genre | Compact action adventure / spaceship escape shooter |
| Tone | Stylish, adventurous, tense, readable, non-gory |
| Mission length | Aim for 10–15 minutes on a first successful run; measure rather than promise |
| Level count | One mission, six connected zones |
| Weapon count | One player energy blaster |
| Enemy roster | Security Trooper and Heavy Sentinel, sharing an AI foundation |
| Combat style | Movement, shooting, line-of-sight cover, visible enemy projectiles |
| Interaction | One context-sensitive interact button |
| Special ability | Terminal bypass, represented as a short hold interaction |
| Main objective | Escape the prison deck and launch an escape pod |
| Save model | One local mission checkpoint slot plus separate settings |
| Art | Original ship assembled from simple modular geometry and permitted installed assets |
| Asset budget | Zero paid purchases by default |
| Networking | None |
| Coding approach | C++ gameplay foundations with editable properties and lightweight Blueprint/data wrappers when supported |
| Production approach | Complete a working mission first; then improve its presentation |

These defaults supersede earlier informal suggestions, including Blueprint-only construction, a large stealth system, and any guarantee about a two-week schedule or subscription cost. A roughly two-week personal project is an aspiration. Actual delivery depends on the machine, tools, iteration, and the owner's available time. Do not infer a subscription, unlimited compute, paid API authorization, or permission to buy assets from this document.

### 1.3 Authorized local work and boundaries

Inside the selected project directory, create and edit source, configuration, scripts, assets, maps, local documentation, and build outputs. Use installed development tools and Unreal automation. Read the relevant setup information and build logs. Run local tests and local builds. Preserve unrelated files and existing owner edits.

Do not purchase assets, subscribe to services, spend API credit through a newly configured service, publish the game, upload private project files, create accounts, or change system-wide security settings. Do not install a large engine or toolchain without the owner authorizing that installation. Do not accept account agreements on their behalf. These boundaries do not prevent implementing the local project with existing tools.

If a genuine external prerequisite is missing, complete all independent local work that can be done honestly, record what remains unverified, and request the specific missing action. Do not represent generated text or unexecuted scripts as saved Unreal assets.

### 1.4 Completion reporting

At handoff, state where the project is, how to open it, how to play, what was actually tested, whether a packaged build exists, and any remaining blockers. Clearly separate “implemented,” “compiled,” “played in editor,” and “tested in packaged build.” Do not say “done” when the project is only a scaffold or when the mission cannot be completed.

---

## 2. Product vision

The player is a captured field agent aboard the **Helios Ark**, a corporate research and detention vessel. A power surge opens their cell. They seize a security blaster, push through a cargo checkpoint, reroute a security terminal, survive the reactor passage, and launch a maintenance escape pod while the ship enters emergency lockdown.

The fantasy is resourceful escape: an outnumbered agent using a stolen weapon and the ship's infrastructure to survive. The level should feel like a short playable scene from a larger original adventure, with a beginning, escalation, climax, and release.

The mission emphasizes readable action over tactical simulation. The player should understand the controls within the first minute, fire their first shot shortly afterward, and always have a visible next destination. The ship should seem larger than the playable route through windows, inaccessible doors, signage, ambient machinery, and glimpses of distant compartments.

The first level must stand on its own. Do not build a campaign selection system, skill tree, multiplayer lobby, economy, or long cinematic prologue before the escape mission works.

### 2.1 Four design pillars

1. **Responsive action.** Movement starts promptly, aiming is predictable, hits register visibly, and controls remain consistent across combat, interaction, death, and menus.
2. **A ship with a clear route.** Rooms have distinct purposes and silhouettes. Lighting, signs, doors, and objectives guide the player without a minimap.
3. **Escalation through combinations.** Introduce shooting, then multiple enemies, then terminal activation, then a tougher enemy. Reuse understandable systems in different spaces.
4. **Easy continuation.** The owner can later change damage values, encounter counts, room props, colors, dialogue, and checkpoints without rewriting the whole project.

### 2.2 Player and use context

The target player is comfortable with basic third-person games but should not need shooter expertise. A first-time player may use a laptop, external mouse, and headphones. Small-screen readability and predictable performance matter more than elaborate rendering effects.

The owner is also a beginner editor. Organization, sensible editable values, safe regeneration scripts, and a short guide to changing the game are product features, not optional cleanup.

---

## 3. Scope, priorities, and exclusions

### 3.1 P0: Required for a complete playable mission

- A valid Unreal project with real saved content and a configured starting map.
- A visible controllable third-person player, camera collision, movement, sprint, jump, aim, fire, reload, and interact.
- Health, damage, death, and restart from checkpoint.
- A single energy blaster with ammunition, collision-aware aiming, feedback, and reload behavior.
- Security Trooper enemies that navigate, acquire the player, attack visibly, take damage, die, and clear encounters.
- One Heavy Sentinel using the same reliable foundation with distinct presentation and values.
- Six connected zones with collision, readable exits, encounter boundaries, and navigable enemy routes.
- A terminal that controls the required security gate.
- Health and ammunition pickups, plus supply stations that prevent ammunition dead ends.
- A mission state system that controls objectives, doors, encounters, checkpoint restoration, and the ending.
- A start menu, HUD, pause menu, death screen, success screen, and functioning buttons.
- Three defined checkpoint snapshots, including the mission start.
- A complete escape-pod ending, even if represented with simple camera movement and animated geometry.
- A documented build route and honest verification evidence.

### 3.2 P1: Expected presentation pass after P0 works

- Coherent materials, room-specific color accents, emissive signage, and a space backdrop.
- Basic firing, impact, damage, pickup, door, terminal, and launch sounds generated locally or sourced from permitted installed assets.
- Simple particles, muzzle effects, hit markers, damage direction, and restrained camera feedback.
- A visible planet or stars outside selected windows.
- Adjustable mouse sensitivity, sound levels, graphics preset, and camera shake.
- Local checkpoint persistence between launches.
- A package for the local desktop platform, when installed tools support it.
- Visual inspection at laptop-sized and larger resolutions.

P1 is part of the intended completed delivery. If an external dependency blocks one item, finish the other items and identify the blocked item explicitly. Do not silently remove it from the requirements.

### 3.3 P2: Optional only after all required work is stable

- Gamepad input and prompt switching.
- A small shoulder-swap option.
- A second visual skin for the same trooper.
- A short optional lore screen.
- More sophisticated hand placement or aim animation.
- Additional decorative ship movement outside windows.

Do not implement P2 at the expense of mission reliability, packaging, or owner documentation.

### 3.4 Explicitly outside this first build

Multiplayer, battle royale, online accounts, matchmaking, user-generated worlds, building mechanics, open world travel, multiple missions, procedural maps, inventory grids, weapon attachments, a stealth visibility simulation, disguises, camera networks, skill trees, crafting, dialogue choices, voice acting, destructible ship hulls, zero gravity, vehicles, a second playable character, real-money systems, analytics collection, and store publication are outside scope.

There is no mandatory wall-climbing, crouch-cover attachment, dodge-roll animation system, melee combo system, ragdoll dependency, or sophisticated boss phase system. Enemies can be intimidating through timing, numbers, and room layout.

---

## 4. First-run experience and complete mission flow

The application opens to a simple title menu over an in-engine ship view or a static view of the detention deck. The title is `ORBIT BREAKOUT`, with the subtitle `Emergency Departure`.

“New Mission” starts the holding cell sequence. If a compatible incomplete checkpoint exists, “Continue” resumes that checkpoint. If a completed save exists, show “Replay Mission” in place of Continue. Never present a Continue button that loads an invalid state.

The opening lasts no more than eight seconds. A quiet alarm sounds, a light flickers, the door unlatches, and a message reads: “Containment power interrupted.” The player can move as soon as the brief fade ends. The camera begins behind the character, looking toward the opened exit.

The mission path is:

```text
Title menu
    |
New Mission / Continue
    |
Z01 Holding Cells -- take blaster --> Z02 Security Corridor
    |                                      |
    |                               defeat first 3 troopers
    |                                      |
    +------------------------------> CP01 at Cargo entrance
                                           |
                                  Z03 Cargo Processing
                                  3 troopers, then 2
                                           |
                                  Z04 Security Junction
                                  bypass terminal, then 3
                                           |
                                  CP02 before Reactor
                                           |
                                  Z05 Reactor Passage
                                  2 troopers + 1 sentinel
                                           |
                                  Z06 Escape-Pod Bay
                                  final 4 troopers
                                           |
                                  board and launch pod
                                           |
                                  mission complete / replay
```

An experienced player may finish faster than ten minutes. Do not artificially increase enemy health, insert repeated waves, slow doors excessively, or add empty corridors just to stretch time. First-run duration is a playtest outcome. Target roughly one minute for onboarding, two minutes for the first corridor, three minutes for cargo, two minutes for security, two minutes for reactor, and two minutes for the finale, including movement and learning.

---

## 5. Story, setting, and presentation copy

### 5.1 Characters and world

The playable agent uses the call sign **Rook**. They wear a charcoal utility suit with a teal identification light and a small illuminated wrist device. Their face does not need a custom model. An installed mannequin, helmeted silhouette, or original simple character is acceptable if movement and presentation are coherent.

The Helios Ark is operated by a fictional organization called **Asterion Systems**. Its interior combines clean utility panels, reinforced cargo spaces, restricted laboratories behind closed doors, and a warm reactor core. These are working fictional names; the project does not claim name or trademark clearance for commercial release.

Security Troopers are clearly synthetic guards. This allows readable destruction, sparks, and power-down effects without gore. The Heavy Sentinel is a bulkier security unit with an amber core. Avoid recognizable franchise helmets, armor shapes, logos, sound effects, or story elements.

### 5.2 Narrative delivery

Story appears through short objective text, environmental labels, and optional subtitle-like system messages. No voice service, external account, or prerecorded cinematic is required.

Use these lines as defaults:

| Trigger | Copy |
|---|---|
| Opening | “Containment power interrupted.” |
| First objective | “Find a way off the detention deck.” |
| Blaster pickup | “Security blaster acquired.” |
| First armed encounter | “Security units responding.” |
| Cargo entrance | “Escape pods: forward maintenance deck.” |
| Locked gate | “Transit gate locked. Bypass the security terminal.” |
| Terminal complete | “Override accepted. Security response incoming.” |
| Security encounter cleared | “Transit access restored.” |
| Reactor entry | “Reactor instability detected. Proceed to evacuation.” |
| Sentinel sighting | “Heavy security unit ahead.” |
| Final bay cleared | “Escape pod ready.” |
| Pod boarding | “Launch sequence initiated.” |
| Ending | “Signal lost. Agent status: escaped.” |

Messages remain legible long enough to read and do not stack over important combat prompts. Use a queue with a maximum of one narrative message at a time. Objective changes can replace earlier objective text immediately. Death and success screens suppress queued chatter.

### 5.3 Urgency without a frustrating timer

The ship becomes more unstable through lighting, audio, and scenery. There is no lethal self-destruct countdown in the baseline mission. If a display shows a countdown for atmosphere, it must not imply an actual gameplay deadline; prefer “EVACUATION ACTIVE” instead. The player can learn and explore without losing solely because they read a prompt slowly.

---

## 6. Player controls and input behavior

Use conventional keyboard and mouse controls. Input mapping must live in one maintainable place and work in both editor play and a packaged build. Use the installed engine's supported input system; Enhanced Input is the preferred project approach, subject to actual version verification.

| Action | Input | Behavior |
|---|---|---|
| Move | W/A/S/D | Camera-relative movement |
| Look | Mouse | Rotate camera; vertical angle clamped |
| Jump | Space | Single grounded jump |
| Sprint | Hold Left Shift | Faster movement while not aiming, firing, or interacting |
| Aim | Hold right mouse button | Shoulder camera and slower movement |
| Fire | Hold left mouse button | Automatic blaster fire at a fixed cadence |
| Reload | R | Reload from reserve if allowed |
| Interact | E | Pick up weapon, use terminal, board pod, activate supply station |
| Pause | Escape | Toggle pause while alive in gameplay |
| Confirm UI | Left click / Enter | Activate focused button |
| Navigate UI | Tab / arrow keys | Move focus through actionable items |

The prompt for a hold action must say “Hold E,” while an instant action says “E.” Avoid showing the same button as both a tap and a hold on the same object.

### 6.1 Input precedence

State precedence is `Success / Ending`, then `Dead`, then `Paused`, then `Interacting`, then normal gameplay. A higher-priority state blocks lower-priority actions. Menus consume relevant input so clicking Resume does not also fire the weapon. Releasing a held button during pause must not produce a stuck firing or sprinting state after resume.

Escape during the short pod sequence opens a minimal pause overlay with Resume and Skip Ending. Escape on the death or success screen does not reactivate gameplay. Editor Escape behavior may be reserved by Unreal; verify the actual editor route and document any difference from the packaged build.

### 6.2 Movement initial values

| Parameter | Initial value |
|---|---:|
| Normal movement speed | 450 cm/s |
| Sprint speed | 650 cm/s |
| Aim movement speed | 260 cm/s |
| Interaction movement | Disabled during terminal hold and boarding |
| Jump velocity | 500 cm/s |
| Air control | 0.25 |
| Ground acceleration | 2,000 cm/s² |
| Ground braking | 2,000 cm/s² |
| Walkable slope | Approximately 45 degrees or template-safe equivalent |

Keep the installed template's tested capsule dimensions unless necessary. Do not shrink collision just to fit badly designed doorways. The character cannot sprint while aiming or shooting. Starting aim cancels sprint immediately. Firing during sprint exits sprint and fires without requiring an extra click. Reloading permits ordinary movement and jumping but cancels sprint; a player can cancel reload by starting sprint, receiving no new ammunition until a later completed reload.

There is no stamina bar. Do not add fatigue mechanics to constrain a small level unnecessarily. Jumping is useful for movement expression, not mandatory precision platforming.

---

## 7. Camera, aiming, and character presentation

The camera normally sits behind and slightly above the player. Aiming moves it smoothly toward the right shoulder, narrows the field of view modestly, and rotates the player to match their aim direction.

| Camera value | Initial setting |
|---|---:|
| Normal arm distance | 320 cm |
| Aim arm distance | 220 cm |
| Normal field of view | 85 degrees |
| Aim field of view | 70 degrees |
| Aim blend time | 0.15–0.20 seconds |
| Pitch range | Roughly -65 to +65 degrees |
| Aim lateral offset | About 55 cm to the right |
| Shake strength default | 35% of authored maximum |

Tune offsets against the actual character mesh, walls, and a laptop screen. Keep the crosshair clear of the character's shoulder. Use camera collision so walls do not obscure the player for long periods or allow the camera to leave the ship shell. Do not use motion blur by default.

### 7.1 Third-person shot resolution

1. Trace from the camera through the reticle to identify the desired target point, up to weapon range.
2. Trace from the weapon muzzle toward that point to determine the actual unobstructed hit.
3. Apply damage only to the actual valid hit from the muzzle-side check.
4. Draw the tracer and impact at the actual path and hit point.

This prevents the camera from shooting around a wall while the muzzle remains behind it. Ignore the firing character and the weapon's own components. Stop on world collision. A decorative tracer is not a second damaging projectile. A single shot must cause at most one direct damage event to a target.

If the muzzle is blocked at very short distance, show a small blocked-shot cue and impact on the obstacle. Do not teleport the muzzle outside geometry or allow damage through cover. The weapon may lower visually near a wall if that is easy; collision correctness takes priority.

### 7.2 Animation minimum

The player visibly idles, moves, turns, and jumps. Firing must show an attached weapon, muzzle feedback, and some readable recoil or upper-body response. A full custom animation suite is unnecessary. Use compatible installed template animation assets when available. If a custom aim pose cannot be created reliably, use a stable simplified pose and record the limitation, while preserving functional aim and combat.

Synthetic enemies can use rigid body-part movement and procedural bobbing instead of borrowed humanoid animations. Avoid enemies sliding featurelessly across the floor with no orientation or movement cue.

---

## 8. Player health, damage, and survival

The player begins with 100 health. There is no armor layer, shield regeneration, hunger, bleeding, or limb damage. Medical pickups restore health up to the maximum. Health persists through the current run until changed by damage, pickups, or checkpoint restoration.

| Value | Default |
|---|---:|
| Maximum health | 100 |
| Standard trooper bolt damage | 10 |
| Heavy bolt damage | 18 |
| Medical pickup healing | 35 |
| Minimum health restored at CP01/CP02 | 100 |
| Fall below safe world boundary | Respawn through death/checkpoint flow |
| Full-screen damage flash | Brief, low opacity; removable with reduced effects |

A damage event displays a directional indicator, a small flash near the HUD, and a short impact sound. Avoid large camera jerks that disorient the player. Keep the health bar readable at low health. At or below 25 health, change the bar and show a restrained warning; do not rely on red alone or add continuous intense flashing.

There is no general invulnerability window that causes clearly landed bolts to silently fail. Spawn safety comes from safe checkpoint placement, a brief spawn grace period of two seconds, and enemies that have not yet activated. During grace, show a subtle shield-like visual or short “Checkpoint restored” notification; disable grace immediately when the player fires or enters an active encounter.

### 8.1 Death behavior

When health reaches zero, process death once. Stop player firing, cancel reload and interaction, clear temporary prompts, disable player-controlled movement, and block further player damage events. Enemies stop attacking a dead player. After a brief 0.75-second fade or power-down, show:

- `MISSION INTERRUPTED`
- `Retry Checkpoint`
- `Restart Mission`
- `Main Menu`

Retry reloads the mission from the saved in-memory checkpoint snapshot. It must not reload into the middle of a projectile volley. Restart Mission returns to CP00 and resets mission progression. Main Menu preserves the last valid checkpoint.

Death during terminal interaction must not complete the terminal. Death and pod boarding in the same frame resolve through a single authoritative state change; entering Ending is permitted only if the player was alive when the validated boarding action completed.

---

## 9. Player weapon: ARC-9 security blaster

Create one original compact energy weapon named **ARC-9**. The visual can be built from a few bevelled or simple geometric pieces with a cyan energy strip. It must be attached visibly to the character and point broadly toward the firing direction. Do not copy a recognizable real or fictional weapon design.

### 9.1 Weapon tuning

| Parameter | Initial value |
|---|---:|
| Direct body damage | 20 |
| Firing cadence | 5 shots per second |
| Magazine capacity | 24 |
| Initial reserve on pickup | 96 |
| Maximum reserve | 144 |
| Reload duration | 1.4 seconds |
| Maximum range | 6,000 cm |
| Aim spread half-angle | 0.4 degrees |
| Hip-fire spread half-angle | 1.8 degrees |
| Ammo pickup | 48 reserve rounds |
| Empty click cooldown | At least 0.4 seconds |

The baseline uses a magazine and reserve ammunition. Do not simultaneously introduce an overheating meter. Headshot multipliers are outside the required build; body damage must be consistent across the default enemy hit region. A 60-health trooper requires three body hits, while the 240-health sentinel requires twelve.

### 9.2 Firing and reload rules

- Fire only when the weapon is acquired, the player is alive, gameplay is active, ammunition exists in the magazine, and no blocking interaction or reload is active.
- Holding fire produces one shot per cadence interval. Do not tie shot count to frame rate.
- Each accepted shot consumes exactly one magazine round.
- When the magazine reaches zero and reserve exists, automatically start a reload after the final shot's interval. Manual reload uses the same path.
- If both magazine and reserve are empty, show “Find ammo” near the ammo display, with a restrained dry-fire sound when attempting to shoot.
- Reloading with a full magazine does nothing. Reloading with no reserve does nothing and provides a short message.
- Ammunition transfer occurs only when the reload completes. Transfer `min(magazine deficit, reserve)`.
- Cancelled reloads transfer no ammo. Death, restart, and map unload clear reload timers.
- Holding fire through a completed automatic reload may resume firing only if the button remains physically held and gameplay has focus.

### 9.3 Weapon feedback

Player shots are cyan-white, narrow, and short-lived. Enemies use orange-red bolts. The reticle expands slightly during firing, returns toward rest, and shows a brief hit marker only on confirmed enemy damage. A kill marker may differ in shape or brightness. A wall impact must not display an enemy hit marker.

Use a muzzle pulse, a small impact particle or decal, a brief light if performance allows, and a short original or permitted sound. Recoil should feel crisp but should not create cumulative camera drift that makes the first level difficult to test. All transient effects must expire and release resources.

---

## 10. Ammunition, medicine, and supply safety

Small pickups collect automatically when the player overlaps them after acquiring the weapon, except medical pickups can work before acquisition. Pickups use visible shapes and labels: medical is a white cross inside a teal casing; ammunition is an amber cartridge with stacked-bar markings. This gives meaning beyond color.

Medical pickups are not consumed at full health. Ammo pickups are not consumed at maximum reserve. A successful pickup updates the HUD immediately, plays one feedback cue, and cannot be collected twice through repeated overlap events.

No random drops are required. Author fixed pickups for reproducible tuning and tests. The initial encounter layout supplies enough ammunition for the mandatory enemies even when the player misses frequently.

### 10.1 Recovery stations

Place a wall-mounted ammo station at the safe side of every combat area: Security Corridor entry, Cargo entry, Security Junction entry, Reactor entry, and Escape Bay entry. Each remains reachable without crossing a permanently closed door. It uses an instant E interaction, replenishing reserve up to at least 72 rounds when the player's reserve is below 72; it does not empty or alter the magazine. A short three-second cooldown prevents accidental repeated audio.

Stations can be reused. Their purpose is preventing a resource dead end, not an economy. They do not heal the player, damage enemies, provide invulnerability, or complete objectives. Include a consistent sign: `SECURITY AMMUNITION`.

An empty player can retreat to a station, reload, and continue. Review the layout so combat locking never strands the player away from that area's station. This is an explicit acceptance requirement.

### 10.2 Pickup placement baseline

| Zone | Medical pickups | Ammo pickups | Station |
|---|---:|---:|---|
| Z01 Holding Cells | 0 | 0 | No |
| Z02 Security Corridor | 1 | 1 | Entry alcove |
| Z03 Cargo Processing | 2 | 2 | Inside entry safety area |
| Z04 Security Junction | 1 | 1 | Before terminal alcove |
| Z05 Reactor Passage | 2 | 2 | Safe entrance |
| Z06 Escape-Pod Bay | 2 | 2 | Arrival bulkhead |

These are starting placements. Move them based on tests, but preserve at least one reachable medical pickup in each combat zone and guaranteed access to ammunition recovery.

---

## 11. Enemy system

Use one shared enemy foundation with editable tuning for the trooper and sentinel. A small explicit AI state machine is sufficient. A behavior tree is acceptable if it improves reliability in the installed toolchain, but it is not required simply because Unreal supports it.

Required states: `Dormant`, `Idle`, `Acquire`, `Approach`, `Attack`, `Reposition`, and `Dead`. Encounter activation moves an enemy out of Dormant. Idle enemies can perform a short patrol or face a plausible guard direction. They must never damage the player before their encounter activates.

### 11.1 Perception and pursuit

Use distance and line-of-sight checks at a controlled interval, initially 0.2 seconds. Troopers acquire within 1,800 cm and roughly a 120-degree forward field. An encounter can alert its own members after one sees the player, but each shooter still needs its own unobstructed firing solution. Do not let a shared alert become permission to shoot through walls.

Keep the last seen location for up to three seconds. When the player is occluded, stop initiating shots, move toward a reachable last seen position, and reacquire normally. Existing bolts can continue along their original paths. Do not track the player's exact position indefinitely through geometry.

An enemy approaches until it has a comfortable firing distance, initially 600–1,200 cm for a trooper. It stops or strafes between authored reachable side points. Avoid complex dynamic cover selection. If a movement request fails, choose another nearby valid point, then retry after a delay. Never issue path requests every frame.

### 11.2 Attack fairness

Before each burst, show a visible 0.45-second charge at the muzzle or chest, accompanied by an optional sound. At the end of charge, lock a target point based on the player's then-current position. Fire three bolts over a short burst, with modest spread around that point. Do not perfectly steer the burst to follow the player after it begins.

Bolts are visible moving projectiles with collision, not invisible hitscan attacks dressed up after damage. They do not home. They stop on world geometry and damage a valid player once. Team collision rules prevent enemies from damaging each other. Security machinery can be damaged only if explicitly designated as a hazard.

Initial projectile speed is 1,800 cm/s, with a maximum life of four seconds. Use swept collision or an equivalent tested method to avoid tunneling. An unobstructed close-range hit can hurt; wall collision must still protect the player.

### 11.3 Enemy tuning

| Parameter | Security Trooper | Heavy Sentinel |
|---|---:|---:|
| Health | 60 | 240 |
| Move speed | 260 cm/s | 190 cm/s |
| Damage per bolt | 10 | 18 |
| Bolts per burst | 3 | 3 |
| Time between bolts | 0.18 seconds | 0.25 seconds |
| Telegraph duration | 0.45 seconds | 0.8 seconds |
| Recovery after burst | 1.6 seconds | 2.2 seconds |
| Desired range | 600–1,200 cm | 800–1,400 cm |
| Physical scale | Normal robot silhouette | About 1.25× visual height, validated collision |
| Color accent | Orange visor / white shell | Amber core / dark broad shell |

Use an encounter-level attack budget of at most two enemies beginning active bursts at once. Others move, aim, or wait. Stagger acquisition so a newly entered room does not instantly fire every enemy's first burst simultaneously. The sentinel uses one attack slot like a trooper. It gains no hidden damage immunity.

Acquire a slot when the charge telegraph begins. Hold it through the burst and for 0.25 seconds after the last bolt is emitted, then release it; the rest of recovery does not occupy a slot. Existing bolts can remain in flight after release. Death, a cancelled attack, or encounter shutdown releases the slot immediately. Pausing freezes the slot's timers. This limit applies to telegraph/emission sequences, not to the total number of projectiles already in flight.

### 11.4 Death and cleanup

At zero health, emit one death notification, disable attacks and navigation, disable blocking collision that could obstruct the route, show a short sparks/power-down effect, and remove or settle the visual. Corpses are nonblocking and can disappear after eight seconds. Encounter tracking uses enemy identity and death state, not whether a corpse still exists.

Do not count arbitrary destruction or level unload as a player kill. A validation tool can explicitly resolve a bugged enemy during development, but the normal mission cannot rely on this. If an enemy falls below the world boundary, mark it resolved for the encounter and log the placement defect so the gate does not remain locked; fix the underlying location before delivery.

---

## 12. Encounter design and roster

There are **18 mandatory enemies: 17 Security Troopers and 1 Heavy Sentinel**. This is the authoritative baseline roster.

| Encounter ID | Zone | Activation | Composition | Completion effect |
|---|---|---|---|---|
| E01_CORRIDOR | Z02 | Armed player crosses corridor trigger | 3 troopers | Open Cargo access and enable CP01 |
| E02_CARGO_A | Z03 | Player leaves entry safe area | 3 troopers | Schedule second cargo group |
| E03_CARGO_B | Z03 | 2 seconds after E02 completion | 2 troopers | Open Security Junction access |
| E04_SECURITY | Z04 | Successful terminal bypass | 3 troopers | Open Reactor transit gate and enable CP02 |
| E05_REACTOR | Z05 | Player passes safe entrance | 2 troopers + 1 sentinel | Open Escape Bay access |
| E06_POD_BAY | Z06 | Player crosses bay threshold | 4 troopers | Enable escape-pod boarding |

No enemies appear behind the player without a visible arrival door and a warning. Prefer enemies placed in authored locations or arriving from side service doors. A delayed group can emerge from a door with a light and sound before attacking. Do not materialize units directly in the player's view.

### 12.1 Encounter accounting

Each encounter has a stable ID, a list of expected enemy IDs, an activation state, and a resolved set. Activate once, spawn or enable each expected enemy once, and mark cleared only when every expected member is resolved. A second activation event must not duplicate enemies. Restoring a checkpoint must not mix actors from the previous attempt with the new roster.

An empty or malformed roster is a validation error. It must not quietly become a cleared encounter in a normal build. Development validation should catch missing spawn references before a playthrough.

### 12.2 Difficulty tuning rules

Start with the values in this file. If the game is too hard, first improve visibility, telegraph duration, enemy attack staggering, and medical placement. Then adjust damage or health. If it is too easy, first improve enemy positioning and sightlines. Avoid turning every enemy into a high-health target.

The first corridor should be forgiving. The final bay should require movement between cover but should not require perfect aim. An average new player should be able to recover from mistakes. Any claim about first-time success rate requires actual human playtest evidence.

---

## 13. Space planning and level construction standards

Use one persistent mission map named `L_EmergencyDeparture`. Keep the six zones physically connected with short, authored transitions. A separate `L_FrontEnd` map is acceptable for the main menu. A small `L_CombatTest` map is encouraged for development and is excluded from the player's normal flow.

The geometry can be assembled from reusable floor, wall, doorway, beam, pipe, crate, light-strip, railing, and console modules. Use the editor to save actual actors and assets. Runtime generation may supplement decorative details, but the owner must be able to open and edit the main layout.

### 13.1 Scale and readability

Use Unreal centimeters consistently. Initial module sizes: 400 cm wide wall panels, 400 cm ceiling height in normal corridors, 600–800 cm ceiling height in larger spaces. Corridors should generally be 400–600 cm wide. Main door openings should be at least 240 cm wide and 300 cm tall. Validate these dimensions against player and enemy collision.

Most cover is 110–140 cm tall, with full-height structures breaking long sightlines. The game has no crouch system, so half-height cover is primarily a movement obstacle and firing position, not a guarantee of safety for the entire standing character. Include full-height pillars or crates where true line-of-sight protection is needed.

The critical route should not require jumping. Raised platforms connect through ramps or stairs that navigation can traverse. Put railings on elevated edges and blockers behind decorative windows. Windows do not open into playable vacuum.

### 13.2 Approximate layout anchors

These coordinates are a starting guide, not an immutable architectural drawing. Values are in centimeters, with floors near Z=0. Add short connectors and real openings; do not place sealed room boxes next to one another and call them connected.

| Zone | Approximate center X,Y | Interior footprint | Ceiling | Main character |
|---|---|---|---|---|
| Z01 Holding Cells | 0, 0 | 1,600 × 1,200 | 400 | Tight, blue, interrupted power |
| Z02 Security Corridor | 3,000, 0 | 3,200 × 700 | 450 | Long sightline broken by supports |
| Z03 Cargo Processing | 6,500, 1,000 | 3,600 × 2,800 | 800 | Broad, layered, crates and gantry |
| Z04 Security Junction | 10,000, 1,000 | 2,000 × 1,800 | 500 | Terminal focal point and locked gate |
| Z05 Reactor Passage | 13,500, 0 | 3,200 × 1,600 | 700 | Warm core, machinery, offset route |
| Z06 Escape-Pod Bay | 17,000, 0 | 3,600 × 2,400 | 800 | Space vista, pod silhouette, final arena |

The route may bend to make the ship feel substantial. Allow enough space between zone footprints for connectors. Test the complete collision and navigation path; the table is not a substitute for that test.

### 13.3 Shared environmental grammar

Teal floor-edge strips indicate the usable route. Amber markings indicate security restrictions or machinery. White illuminated labels identify the next destination. Locked gates combine a red/amber status light with text or a lock symbol. An unlocked gate changes symbol and opens visibly.

Decorative doors have no interaction prompt and are labeled with plausible destinations such as `LABORATORY — SEALED`. Do not place identical interactive-looking consoles everywhere unless they are actually usable. Keep the player's actionable objects distinct in shape and light treatment.

---

## 14. Z01 — Holding Cells

### Purpose

Establish identity, controls, and escape direction without combat pressure. Teach movement and interaction through the level itself.

### Layout

The player starts in a 500 × 500 cm cell looking toward a sliding door. A bench, a small wall light, and a transparent or barred cell front establish the prison setting. The adjoining detention hall includes two visibly sealed cells, an open equipment alcove, and a single clear exit toward security.

The floor strip points toward the equipment alcove. The blaster sits in a visible rack with a cyan pulse, a recognizable silhouette, and the prompt `E — Take ARC-9`. Place it at a reachable height, approximately 100 cm, and within the normal interaction distance from the path.

### Sequence

1. Fade in. Display the initial objective.
2. After a short power flicker, open the cell door fully.
3. Show `WASD — Move` and `Mouse — Look` as compact prompts.
4. After the player moves a few meters, fade those prompts.
5. When the player approaches the rack, show the pickup prompt.
6. On E, acquire the weapon exactly once, attach it, set ammo to 24/96, and show aiming/shooting hints.
7. Unlock the exit to the Security Corridor.

A player who ignores the rack sees `Take the security blaster` at the exit. The exit cannot permanently close behind an unarmed player. The weapon object remains until successfully acquired. Jumping onto scenery cannot bypass the armed-player gate.

### Done checks

The player never starts inside collision, can find the exit without rotating the camera wildly, can acquire the weapon by pressing E once, and cannot trigger E01 without having the weapon. Restarting New Mission restores this exact initial state.

---

## 15. Z02 — Security Corridor

### Purpose

Teach the relationship between aiming, firing, enemy telegraphs, damage, and recovery. Provide the first satisfying combat success.

### Layout

Use a corridor with three offsets, a full-height support on one side, and two cargo carts. The first enemy stands at medium range with a clear silhouette against a brighter panel. The second and third occupy deeper positions, separated so they do not all overlap visually.

Place the ammo station in an entry alcove. Put one medical pickup behind the first safe support and one ammo pickup near the far side. The next gate is clearly labeled `CARGO PROCESSING`.

### Encounter

E01 activates only after weapon acquisition and entering the corridor. The nearest trooper provides the first telegraph. Delay the other two units' first attack opportunities so the player has time to understand the first bolt. All three eventually participate, with the shared two-attacker budget.

Do not require the player to search distant rooms for a remaining guard. Every enemy should either engage or remain visibly reachable in this corridor. On the last defeat, play a small completion cue, open Cargo access, and update the objective to `Reach the forward transit gate`.

### Checkpoint transition

Crossing the safe threshold into Cargo after E01 is cleared commits CP01. Show `Checkpoint saved`. This checkpoint starts inside the cargo entry alcove, before E02 activates, with earlier zones cleared and the weapon equipped.

---

## 16. Z03 — Cargo Processing

### Purpose

Expand combat from a corridor to a room with multiple movement choices. Give the ship a memorable large interior without building a maze.

### Layout

Build a cargo hall with a central arrangement of containers, an overhead gantry, a large sealed freight door, and a window looking toward a planet or external ship structure. The player enters near one corner. The exit lies diagonally across the room and is visible from the entry after a few steps.

Create two ground-level paths around the central containers and one short accessible raised platform. The platform is optional and reached by a ramp. Railings and support beams make it read as working ship infrastructure. Keep enough navigation clearance around container corners for the sentinel-sized collision shape even though the sentinel does not appear here.

Place the two medical pickups on different sides, both reachable during combat. Put ammo along the movement route, not exclusively at enemy spawn points. The ammo station sits inside the entry bulkhead, reachable after the encounter activates.

### Encounter

E02 activates when the player exits the safe entry area. Three troopers occupy a near lane, a far lane, and a side position. After all three are defeated, wait two seconds and signal a service door. E03's two troopers enter from that door and become active. Register both enemies before presenting the second-wave cue so a spawn failure cannot create a false cleared state.

The cargo exit opens after both groups are cleared. Do not lock the player in a small entrance vestibule or close access to ammunition recovery.

### Optional environmental object

One volatile energy canister is an optional P2 feature. If implemented, it has a clear warning symbol, explodes once, checks line of sight before applying radial damage, cannot destroy required terminals or gates, and cannot set off an uncontrolled chain. Its absence must not change the encounter solution or the mandatory tests.

---

## 17. Z04 — Security Junction

### Purpose

Provide a short change of activity and a story reason for the locked transit gate. The terminal bypass is the agent's one gadget-like mechanic.

### Layout

A reinforced gate blocks the obvious forward route. A nearby alcove contains a console with a bright wrist-device-compatible reader. The console is visible from the gate and marked `TRANSIT CONTROL`. Three cover structures provide positions for the reinforcement encounter. A side service door establishes where reinforcements will arrive.

### Terminal interaction

Within 200 cm and with an unobstructed view, show `Hold E — Bypass security (2 sec)`. Holding E starts progress. Releasing E during active gameplay, moving out of range, losing valid interaction focus, dying, or exiting the mission cancels and resets progress. Pause is an exception: it freezes progress. On resume, continue only if E is still physically held and the target remains valid; otherwise cancel and reset progress. The terminal cannot be activated again after success.

The player must be alive, grounded, not reloading, and not in another interaction. Beginning the hold stops movement and firing. Keep the camera controllable within a modest range so the player is not disoriented. Damage can cancel the hold if a stray active attack somehow reaches the player, but the authored encounter begins only after success.

### Security response

On successful completion, record the terminal as used and activate E04. The forward gate remains locked while the response is active. Display `Override accepted. Clear the security response.` After the three troopers are defeated, open the gate and change its status to `TRANSIT ACCESS RESTORED`.

This two-stage rule is authoritative: terminal completion alone does not open the gate. Terminal completion plus E04 cleared does. The terminal must never ask the player to perform the bypass again during the same attempt after E04 begins.

### Checkpoint transition

The safe corridor beyond the open gate commits CP02. CP02 restores the completed terminal, cleared E01–E04, and an unstarted Reactor encounter. The player resumes on the safe side of Reactor entry with full health and baseline checkpoint ammunition.

---

## 18. Z05 — Reactor Passage

### Purpose

Increase spectacle and introduce the Heavy Sentinel while reusing learned combat rules.

### Layout

The route bends around a contained reactor column. Use a bright amber inner core, dark structural ribs, and a glass or force-field-like enclosure that blocks projectiles. The core is decorative and cannot be destroyed. Pipes, cables, and illuminated service labels imply scale.

The passage is wider than an ordinary corridor, with two full-height shielding structures providing real cover. Avoid a single narrow funnel that causes all enemies to stack. The sentinel starts beyond the core, framed in a doorway or broad lane so its size and charge telegraph are visible.

Provide two medical pickups, two ammo pickups, and the entrance supply station. One medical pickup should be reachable without running directly into the sentinel's firing line.

### Encounter

E05 activates after the player leaves CP02's safe area. Two troopers occupy side positions; the sentinel starts slightly deeper. Its first attack uses the full 0.8-second telegraph. It moves slowly, fires a three-bolt burst, and recovers visibly. There is no shield puzzle, mandatory weak point, invulnerable phase, or surprise instant-kill attack.

On all three defeats, open the evacuation passage. Increase alarm intensity and show a broad view of the escape bay or a clear sign toward it. Do not apply random environmental damage merely to create urgency.

---

## 19. Z06 — Escape-Pod Bay

### Purpose

Deliver the final action beat and visible payoff for reaching the destination.

### Layout

The pod sits at the far side in an obvious launch cradle. A large observation window shows space. Three or four substantial cover objects create a zigzag route across the bay. Use bright evacuation markings leading toward the pod. The player can see the pod from the entrance; the objective marker confirms it.

Two medical pickups sit on opposite sides, not both behind the same firing line. The ammo station remains at the arrival bulkhead. The room should allow movement without navigation obstacles trapping the enemies.

### Encounter

E06 activates on entering the bay. Four troopers engage with the two-attacker budget. Two begin in cover-separated frontal positions, and two begin farther back or arrive from a visible service door. No second sentinel is required.

Before all enemies are defeated, the pod prompt reads `Secure the bay before launch` without a hold meter. After E06 clears, show `Hold E — Launch escape pod (1 sec)` at the boarding hatch.

### Ending

On a valid completed boarding interaction, enter Ending once. Stop combat, clear projectiles, disable player damage, and transfer the camera to a stable authored position inside or just outside the pod. Close the pod hatch, play a mechanical sound, and move the pod or the exterior scenery to imply departure. An eight-to-twelve-second sequence is sufficient.

Show the ship receding against space, then fade to `MISSION COMPLETE`. A simple silhouette and moving camera are acceptable. Do not require a full exterior ship model or movie file. Provide `Replay Mission`, `Main Menu`, and `Quit`.

The ending can be skipped after one second. Skipping reaches the same completed state and save result as watching it. It must not leave the player trapped in a hidden pawn or return control to the bay.

---

## 20. Mission state, objectives, and progression

The mission has one authoritative runtime coordinator. UI, doors, checkpoints, encounter actors, and the ending receive state from that coordinator. Do not implement independent overlapping copies of progression inside unrelated widgets and level scripts.

### 20.1 Mission phase model

| Phase | Entry condition | Main objective | Exit condition |
|---|---|---|---|
| P00_CELL | New Mission | Leave the cell and take the security blaster | Weapon acquired |
| P01_CORRIDOR | Blaster acquired | Clear the security corridor | E01 cleared |
| P02_CARGO | E01 cleared | Fight through cargo processing | E02 and E03 cleared |
| P03_TERMINAL | Cargo cleared | Bypass the transit terminal | Terminal completed |
| P04_SECURITY | Terminal completed | Clear the security response | E04 cleared |
| P05_REACTOR | E04 cleared | Cross the reactor passage | E05 cleared |
| P06_POD_BAY | E05 cleared | Secure the escape-pod bay | E06 cleared |
| P07_BOARD_POD | E06 cleared | Launch the escape pod | Boarding hold completed |
| P08_ENDING | Valid boarding | Launching | Sequence completes or is skipped |
| P09_COMPLETE | Ending resolved | Mission complete | Replay or return to menu |

Checkpoint commits are triggered at the specific safe thresholds, not automatically on every phase change. The phase can advance before the player reaches the next checkpoint. Death then restores the last committed checkpoint, including earlier combat if necessary.

### 20.2 Door dependencies

| Door ID | Unlock condition | Notes |
|---|---|---|
| D00_CELL | Opening sequence power fault | Always opens again on New Mission |
| D01_DETENTION_EXIT | Weapon acquired | Prevents an unarmed encounter |
| D02_CARGO_ENTRY | E01 cleared | Safe CP01 threshold beyond it |
| D03_SECURITY_ENTRY | E02 and E03 cleared | Both cargo groups required |
| D04_REACTOR_GATE | Terminal completed and E04 cleared | CP02 beyond it |
| D05_EVAC_ACCESS | E05 cleared | Includes sentinel |
| D06_POD_HATCH | E06 cleared | Boarding requires player alive |

Doors use visible movement over approximately 0.6 seconds. Their blocking collision must match their actual position. An opening door cannot leave an invisible blocking wall. Do not close a required gate through the player. The simplest solution is to keep cleared progression doors open for the rest of the attempt.

### 20.3 Event rules

Use explicit events such as `WeaponAcquired`, `EncounterActivated`, `EnemyResolved`, `EncounterCleared`, `TerminalCompleted`, `CheckpointCommitted`, `PlayerDied`, and `PodBoarded`. Events include stable relevant IDs. Validate state before transitions and ignore already-handled events safely.

Avoid per-frame searches for every actor in the world. Register mission actors once, validate them, and communicate through owned references, interfaces, or delegates. Rebuild registrations on map reload. A destroyed UI widget must not remain subscribed to mission events.

### 20.4 Objective presentation

Show one primary objective at a time in the upper left. A small marker can point to the next required doorway or interactable. During combat, mark the destination while clearly stating that the room must be secured first. Do not draw labels on every enemy or turn the scene into a wall of markers.

After a prolonged period with no progression, a gentle hint may show the destination sign or nearest required terminal. This is optional. Mission completion cannot depend on a hint timer.

---

## 21. Checkpoints, saves, and recovery

Use a fixed checkpoint model, with snapshots at safe authored locations. Do not attempt arbitrary mid-combat world serialization for this first level. There are three checkpoint IDs: `CP00_START`, `CP01_CARGO`, and `CP02_REACTOR`.

### 21.1 Authoritative checkpoint snapshots

| Snapshot field | CP00_START | CP01_CARGO | CP02_REACTOR |
|---|---|---|---|
| Spawn | Cell start | Cargo entrance safety alcove | Reactor entrance safety alcove |
| Mission phase | P00_CELL | P02_CARGO | P05_REACTOR |
| Player health | 100 | 100 | 100 |
| Weapon owned | No | Yes | Yes |
| Magazine | 0 | 24 | 24 |
| Reserve | 0 | 96 | 96 |
| Completed encounters | None | E01 | E01, E02, E03, E04 |
| Terminal complete | No | No | Yes |
| Pending encounters | E01–E06 | E02–E06 | E05–E06 |
| Opening sequence | Play | Skip | Skip |

On CP01/CP02 commit, immediately set health to 100, magazine to 24, and reserve to the greater of current reserve and 96, capped at 144. Store those actual normalized ammo values in the snapshot. On retry, restore the snapshot, including its stored reserve. The table shows the minimum guaranteed reserve, not a rule to discard extra reserve at commit. CP00 always begins unarmed.

### 21.2 World restoration

The reliable baseline is to reload the mission map, wait until actors register, and apply the snapshot before enabling the player. Mark earlier encounters cleared without spawning their enemies. Restore required door and terminal state. Reset uncompleted encounter rosters and all pickups within the current and later zones. Disable pickups and interactive rewards in earlier cleared zones to prevent restoration anomalies.

Doors in completed zones stay open. The player may walk back, but previous weapon racks and one-shot triggers cannot re-grant progression. Ammo stations remain functional in accessible areas. All active projectiles and pending attack timers from the previous attempt disappear on reload.

CP01 triggers only after E01 is cleared; CP02 triggers only after E04 is cleared and the terminal is complete. Re-crossing a committed threshold must not repeatedly reset health or overwrite newer progression. A later checkpoint cannot be replaced by an earlier one merely by walking backward.

### 21.3 Save files

Use the engine's local save mechanism. A proposed data contract follows; adapt field types to Unreal's supported serialization:

```text
MissionSave
  SchemaVersion: integer = 1
  MissionId: stable identifier = EmergencyDeparture
  CheckpointId: CP00_START | CP01_CARGO | CP02_REACTOR
  MissionCompleted: boolean
  PlayerHealth: number
  MagazineAmmo: integer
  ReserveAmmo: integer
  WeaponOwned: boolean
  CompletedEncounterIds: stable identifier array
  CompletedInteractionIds: stable identifier array
  CreatedWithProjectVersion: short version string

UserSettings
  SchemaVersion: integer = 1
  MouseSensitivity: number
  InvertVerticalLook: boolean
  MasterVolume: number
  EffectsVolume: number
  AmbienceVolume: number
  GraphicsPreset: Low | Medium | High
  CameraShakeScale: number
  ReducedFlashes: boolean
```

Derive door states and phase from a validated checkpoint definition and mission completion, rather than blindly trusting arbitrary saved actor transforms. Clamp numeric values. Ignore unknown optional fields. If the mission ID or schema is unsupported, preserve the file, show a concise explanation, and offer New Mission. Do not crash on a corrupt save.

### 21.4 Storage failure and overwrite behavior

Keep the in-memory checkpoint working even if saving to disk fails. Show `Checkpoint reached — disk save unavailable` and record the error. Do not display `Saved` after a failed disk write. Continuing after application restart requires a valid stored checkpoint.

Starting New Mission when an incomplete save exists asks the player inside the game's menu whether to replace that progress. This is a normal game UI confirmation, not a reason for the implementing agent to stop coding. Settings persist independently when mission progress resets.

Once New Mission, Restart Mission, or Replay is confirmed where required, initialize the in-memory CP00 snapshot, set `MissionCompleted` to false, and immediately replace stored mission progress with that CP00 snapshot before normal play begins. Do not wait until CP01 to replace the old run. If the disk write fails, follow the storage-failure behavior above and state that the old disk save may still exist; do not claim it was replaced. A first-ever New Mission also creates CP00 persistence through this same path.

On success, mark the mission completed and retain the last checkpoint data for diagnostics. The front end offers Replay Mission rather than loading a completed world. Replay initializes CP00 explicitly. A normal Quit or Main Menu action never invents a new checkpoint from the player's current location.

---

## 22. User interface specification

Use a small set of reusable UI styles and a single HUD root. Avoid hundreds of separately ticking bindings. Update health, ammo, objectives, and prompts from events when practical. UI must work at 1280×720 and 1920×1080; check a 16:10 laptop aspect ratio as well if available.

### 22.1 Visual style

| Token | Suggested value | Use |
|---|---|---|
| Background panel | #101923 at about 85% opacity | Menus and compact information panels |
| Primary text | #F1F5F9 | Labels, objectives, buttons |
| Secondary text | #A8BCCB | Supporting instructions |
| Teal accent | #48DAD0 | Player, route, interactions |
| Amber accent | #FFB454 | Security, ammo, warnings |
| Damage accent | #F16D6D | Damage and health warning |
| Success accent | #8DE1AC | Checkpoint and completion feedback |

These are design defaults, not mandatory copied brand colors. Use an installed legible sans-serif font with permission for the build. Do not download a font just for decoration if a working bundled alternative exists.

### 22.2 HUD layout

| Element | Placement | Content |
|---|---|---|
| Objective | Upper left, safe margin | One short heading and at most two lines |
| Health | Lower left | Bar, numerical value, medical icon |
| Ammunition | Lower right | Large magazine count, smaller reserve count, reload indicator |
| Crosshair | Screen center | Small readable reticle with confirmed-hit feedback |
| Interaction | Near lower center | Input label, action text, optional progress ring/bar |
| System message | Upper center below safe margin | Temporary story or checkpoint message |
| Damage direction | Near reticle | Short directional wedge, restrained opacity |

At 1080p, body text should generally be 20–24 px equivalent or larger; scale with resolution. Keep a safe margin of approximately 4% from screen edges. Use anchors and layout containers rather than fixed pixel positions alone. Test long objective text for wrapping.

### 22.3 Main menu

Buttons in order: Continue or Replay Mission when applicable, New Mission, Settings, Controls, Quit. Show a clear selected/focused state. Hover, focus, click, and keyboard activation should agree. The background cannot capture mouse input through the menu.

New Mission transitions into the mission without exposing test maps. Continue loads the validated checkpoint route. Quit exits the packaged application; in editor testing it should end play safely rather than closing the whole editor unexpectedly.

### 22.4 Pause menu

Pause stops player movement, AI decisions, projectiles, reload progress, encounter delays, and terminal holds. It does not stop menu interaction. Buttons: Resume, Retry Checkpoint, Restart Mission, Settings, Controls, Main Menu. Confirm Restart Mission and Main Menu if progress since the last checkpoint will be lost, explaining that checkpoint recovery remains available.

When resuming, restore gameplay input and hide the cursor. Clicking a menu item must not send a weapon shot into the world. Settings reached from pause must return to the pause menu, not accidentally unpause the mission behind the settings page.

### 22.5 Settings and controls

Required settings: mouse sensitivity, invert vertical look, master volume, effects volume, ambience volume, Low/Medium/High graphics preset, camera shake amount, and reduced flashes. A fullscreen/resolution selector is useful only if it can be implemented and tested safely; otherwise use the engine's normal window behavior and document it.

Provide reasonable defaults, a Reset Defaults button, and a Back button. Save settings locally. Sliders apply immediately and remain valid on reopening the menu. Do not show settings that have no effect. Controls can be a static readable table for the baseline; full key rebinding is optional.

### 22.6 Accessibility baseline

All required objectives have visible text. Essential enemy attacks have visible telegraphs. Pickups, gate states, and interaction objects use shape and text in addition to color. Camera shake can be reduced to zero. Reduced flashes disables bright full-screen effects and aggressive flicker. Use short smooth fades rather than rapid full-screen flashing.

Do not claim full accessibility compliance. The required baseline improves readability and comfort but still needs varied human testing.

---

## 23. Art direction and asset plan

The intended look is a stylized industrial spaceship with large simple forms, selective bright color, and clear silhouettes. Detail is concentrated on places the player looks: the first weapon rack, the cargo vista, the security gate, the reactor core, and the escape pod.

### 23.1 Asset sourcing hierarchy

1. Original project geometry, materials, icons, and simple effects.
2. Installed Unreal template assets and content permitted for the project.
3. Existing owner-provided assets with a known usable license.
4. External free assets only if access, license, and download scope are clear and they materially help.

Paid assets are not required. Do not make the build depend on an asset-store checkout, marketplace account access, image-generation subscription, or custom outsourced animation. If an optional source is unavailable, continue with original geometry. Maintain an asset ledger with origin, license information, project usage, and attribution requirements where applicable. Do not call an asset public domain merely because it was downloadable for free.

### 23.2 Minimum asset inventory

| Group | Required items | Simplest acceptable construction |
|---|---|---|
| Architecture | Floors, walls, ceilings, doorway frames, beams | Reused primitive-based modules with materials |
| Doors | Cell door, transit gate, service door, pod hatch | Sliding panels with collision and state light |
| Cover | Tall crate, cargo stack, support pillar | Original geometric forms with clear collision |
| Machinery | Console, ammo station, reactor shell | Geometric body, emissive panels, readable labels |
| Player | Visible agent silhouette and weapon | Installed compatible character plus original color treatment |
| Enemies | Trooper and sentinel | Shared synthetic design with distinct scale and accent |
| Pickups | Medical kit and ammo cartridge | Icon-shaped geometry plus emissive accent |
| Ending | Escape pod, cradle, distant ship silhouette | Low-complexity original geometry |
| Environment | Space backdrop, stars, planet | Original materials/meshes or permitted installed content |
| UI | Crosshair, health icon, ammo icon, interaction prompt | Simple original shapes and text |

### 23.3 Materials

Create a small master material set: painted metal, dark structural metal, matte plastic, emissive panel, transparent window if supported reliably, and simple effect material. Use instances for zone color variation. Avoid creating a unique shader for every crate.

Ship floors should separate walkable surfaces from holes and nonwalkable machinery. Walls are dark enough to support contrast but not so dark that enemies disappear. Emissive lighting marks the route but should not bloom into unreadable white patches.

### 23.4 Lighting progression

Holding Cells use cool dim light with a clear exit. Security uses white overhead utility lights and amber enemy accents. Cargo includes broader soft illumination and a space-facing window. The Security Junction has a distinct console light and gate status. Reactor uses warm amber against cooler side passages. The Escape Bay combines emergency amber with a bright cool space vista.

All combat spaces need enough fill light to see enemies and pickups. Do not rely on expensive rendering features that the local machine cannot support. Choose a tested lighting path and record it. Visual design should still read correctly on the Low preset.

### 23.5 Screenshots that should look intentional

After gameplay works, inspect at least five representative views: cell exit, aiming down Security Corridor, Cargo's main combat floor, the sentinel near Reactor, and the escape-pod approach. Correct floating props, exposed voids, repeated obvious texture errors, unreadable signs, and missing ceilings or walls. Add detail only where it improves one of these views or gameplay comprehension.

---

## 24. Audio and effects

Audio supports action clarity and atmosphere. It does not require a soundtrack, voice actor, or external service. Original short synthesized sounds are acceptable and can be generated locally, imported through the engine, and listed as original project assets.

### 24.1 Required cues

| Cue | Desired character | Trigger |
|---|---|---|
| Player blaster | Short crisp electronic pulse | Accepted player shot |
| Enemy charge | Rising electrical tone | Telegraph begins |
| Enemy bolt | Lower, harsher pulse | Enemy projectile fired |
| Metal impact | Brief tick/spark | Shot hits structure |
| Enemy damage | Short mechanical crackle | Valid enemy damage |
| Enemy defeat | Power-down burst | Enemy death once |
| Player damage | Restrained impact | Player health reduced |
| Reload | Mechanical click plus energy rise | Reload begin/complete |
| Pickup | Short positive cue | Pickup consumed |
| Door | Servo slide | Door movement |
| Terminal | Quiet loop and success chime | Hold and completion |
| Checkpoint | Subtle two-note cue | Checkpoint commit |
| Ship ambience | Low hum | Local ambience bed |
| Emergency | Intermittent alarm | Reactor and final bay |
| Pod launch | Mechanical latch and rising engine | Ending |

No audio cue should play every frame because an overlap or state is repeatedly evaluated. Limit simultaneous repeated effects and attenuate distant shots. Master Volume must affect all sound categories. Sound must pause or behave deliberately in menus, with no endless terminal loop continuing after a cancelled interaction.

### 24.2 Visual effects minimum

Use brief muzzle pulses, enemy charge glow, visible enemy bolts, impact sparks, pickup disappearance feedback, terminal progress, and pod engine glow. Prefer simple mesh/material effects when complex particle-system creation is unreliable. Every effect has a lifetime or an explicit owner responsible for cleanup.

Optional particles and camera shake never determine whether damage, mission state, or a door transition occurs. Turning off effects must not break gameplay.

---

## 25. Unreal environment discovery and project setup

Begin by inspecting the selected workspace and applicable project instructions. Determine whether an existing OrbitBreakout project or related project is present. If the user provided an existing project, extend it carefully. If the workspace is empty, create a new `OrbitBreakout` directory there. Do not place the project in a guessed unrelated folder.
### 25.1 Record the actual environment

- Host operating system and CPU architecture.
- Installed Unreal Engine versions and the chosen engine path.
- Available compiler/toolchain and its compatibility with that engine.
- Available engine templates and usable character assets.
- Working editor automation routes, if any.
- Available free disk space and obvious build constraints.
- Target platform selected for the first local package.

Record concise results in `docs/ENVIRONMENT.md`. Do not copy credentials, personal files, or unrelated machine data into the repository. A macOS development machine implies macOS as the first package target unless the owner requests otherwise; it does not imply that a Windows binary can be verified locally.

### 25.2 Engine and template choice

Use a compatible stable installed Unreal 5 release. Do not upgrade an existing project or choose a preview release just to obtain the newest features. Pin the actual engine association and document it.

Prefer the standard Third Person template as the movement and animation foundation where available. Epic documents that this template includes a controllable character, movement, jumping, geometry, and animation assets; it is a starting foundation, not the complete shooter specified here. [Epic: Third Person Template](https://dev.epicgames.com/documentation/en-us/unreal-engine/third-person-template-in-unreal-engine)

Do not assume the exact same template asset names or paths across engine versions. Discover them. If the standard template is unavailable, construct the minimum equivalent character and camera foundation within the project, and report which assets differ.

### 25.3 Missing engine or compiler

If Unreal is missing, create the source/configuration/documentation and reproducible setup work that can be produced without it, but mark editor-generated assets, compilation, map creation, navigation, and playability unverified. Give the owner the exact prerequisite and the next command or editor action after installation. Do not stop at a single sentence if useful independent project work can continue.

If a compiler is missing or incompatible, verify the engine's actual requirements before recommending installation. Do not invent toolchain versions. Use an available verified Blueprint authoring route if it can genuinely produce the complete project with less setup; otherwise prepare the C++ project and explain the compile blocker. Never replace the requested Unreal game with a browser game to conceal the missing prerequisite.

### 25.4 Automation route

Use installed Unreal command-line tools, editor Python, a verified editor bridge, or supported native UI control as appropriate. Do not assume an Unreal MCP integration is connected. Inspect available capabilities and choose a working route.

Editor Python can create and arrange supported assets and actors, but it is an editor automation facility rather than the runtime gameplay language. Keep shipped gameplay in Unreal runtime code and supported assets. Verify version-specific APIs before using them. [Epic: Scripting the Unreal Editor Using Python](https://dev.epicgames.com/documentation/en-us/unreal-engine/scripting-the-unreal-editor-using-python)

Do not fabricate `.uasset` or `.umap` files by writing text and renaming it. If editor automation cannot author a particular Blueprint graph reliably, implement that behavior in a native class with editable properties and use a simple data-only Blueprint child where supported. A written script is not proof that its resulting assets exist; run it and inspect the saved result.

---

## 26. Runtime architecture and ownership

The architecture below defines responsibilities, not an obligation to create a huge framework. Combine small responsibilities when sensible, but preserve clear ownership, testable state, and editable tuning.

### 26.1 Proposed runtime classes

| Class / component | Responsibility |
|---|---|
| `AOBGameMode` | Mission startup, player spawn policy, death/retry flow |
| `UOBGameInstance` | Front-end transitions, pending checkpoint selection, local session/save services |
| `AOBPlayerController` | Input context, menu focus, pause and HUD ownership |
| `AOBPlayerCharacter` | Movement, camera, aiming, visual weapon attachment |
| `UOBHealthComponent` | Health, validated damage, one-time death event |
| `UOBWeaponComponent` | Ammo, cadence, reload, shot resolution |
| `UOBInteractionComponent` | Candidate selection, range/visibility checks, hold progress |
| `IOBInteractable` | Interaction label, eligibility, start/cancel/complete contract |
| `AOBEnemyCharacter` | Synthetic enemy appearance, health, weapon origin, death presentation |
| `AOBEnemyController` | Perception, movement, attack-state timing |
| `AOBEnemyProjectile` | Moving bolt, collision, damage-once, expiration |
| `AOBEncounterVolume` | Stable encounter ID, authored roster, activation and resolution |
| `AOBMissionDirector` | Mission phase, gate dependencies, objective events, checkpoint validation |
| `AOBDoor` | Visible door movement and collision state |
| `AOBTerminal` | Bypass interaction and completion event |
| `AOBPickup` | Health/ammo reward and consume-once state |
| `AOBAmmoStation` | Repeatable safe reserve replenishment |
| `AOBCheckpoint` | Safe threshold and snapshot commit request |
| `AOBEscapePod` | Boarding interaction and ending sequence |
| `UOBMissionSave` | Validated mission checkpoint persistence |
| `UOBHUDRoot` and menu widgets | Presentation and buttons, not duplicate mission logic |

Names may change to match an existing project, but maintain equivalent responsibilities and update documentation. Do not introduce a general-purpose service locator, plugin framework, online subsystem, or elaborate dependency-injection system for this small game.

### 26.2 Native code and editor-friendly data

Put reliable gameplay behavior in C++ when the toolchain supports it. Expose designer-facing values with clear names, units, categories, and bounds. Use data assets, structs, or editable class defaults for combat settings and encounters. The owner should be able to adjust health or damage without searching for scattered literal numbers.

Keep optional Blueprint wrappers thin. They may assign meshes, materials, sounds, and tuning. The project should remain understandable if a wrapper has no complex event graph. Prefer a few composable components over one enormous character class that also owns menus, doors, saves, and every mission event.

### 26.3 Runtime and editor separation

If custom content generation needs editor-specific C++ functionality, place it in an editor-only module or appropriately guarded editor code. The packaged runtime must not import editor-only modules, Python execution, asset editing libraries, or machine-specific script paths.

List dependencies based on actual code and engine version. Candidate modules include the engine core, input, AI/navigation, UMG/Slate, and project-specific runtime code. Do not blindly copy a dependency list that fails on the installed release. Compile a minimal module before authoring the entire mission.

### 26.4 Collision contract

| Object | Blocks movement | Blocks player shot | Blocks enemy bolt | Interaction |
|---|---|---|---|---|
| Ship shell / cover | Yes | Yes | Yes | No |
| Open doorway | No in opening | No in opening | No in opening | Door state only |
| Player | Normal pawn collision | Ignore self | Receives enemy bolt | Own interaction source |
| Enemy | Pawn collision while alive | Receives shot | Ignore same team | No |
| Pickup | No | Usually no | No | Overlap reward |
| Console/station | Solid body if appropriate | Body may block | Body may block | Focus trace candidate |
| Decorative effect | No | No | No | No |
| Dead enemy | No route-blocking collision | No required hit response | No | No |

Use collision channels or object responses consistently. Document any custom channels and include them in project configuration so a fresh checkout behaves the same way. Fix collision through real configuration, not by globally ignoring all obstacles.

---

## 27. Content organization and editable project structure

Use a structure similar to this:

```text
OrbitBreakout/
  OrbitBreakout.uproject
  README.md
  Config/
    DefaultEngine.ini
    DefaultGame.ini
    DefaultInput.ini                 # only where used by the chosen input path
  Source/
    OrbitBreakout/
      Core/
      Player/
      Combat/
      AI/
      World/
      UI/
      Save/
    OrbitBreakoutEditor/             # only if custom editor code is needed
  Content/
    OrbitBreakout/
      Maps/
      Characters/
      Enemies/
      Weapons/
      World/
      UI/
      Materials/
      Audio/
      FX/
      Data/
      Generated/
  Scripts/
    bootstrap_project.*
    build_content.py
    validate_content.py
    build_local.*
    package_local.*
  docs/
    PRD.md
    BUILD_STATUS.md
    ENVIRONMENT.md
    ARCHITECTURE.md
    EDITING_GUIDE.md
    TEST_PLAN.md
    TEST_RESULTS.md
    ASSET_LEDGER.md
    KNOWN_ISSUES.md
  Builds/                            # local distributable output, normally ignored
```

Do not create empty folders just to match the tree. Use existing template folders where necessary and document them. Source-control generated caches, logs, intermediate build files, and binaries only when there is a concrete reason; normally ignore them. Track actual game source and content. If large binary assets need Git LFS, verify availability and explain the repository configuration rather than pretending ordinary text diffs describe those assets.

### 27.1 Naming

Use descriptive stable IDs and asset names. Suggested prefixes: `L_` maps, `BP_` Blueprint wrappers, `WBP_` widget Blueprints, `M_` materials, `MI_` material instances, `SM_` meshes, `DA_` data assets, `S_` sounds. Do not name final required content `TestThing`, `NewBlueprint23`, or `Untitled`.

### 27.2 Owner editing guide requirements

Explain, with exact project locations, how to change player health, weapon damage, enemy health, enemy counts, room geometry, colors, pickup placement, objective copy, checkpoint position, and the ending camera. Include what must be rebuilt or regenerated for each change.

Provide a short “change one thing” example: adjust the blaster damage, play the combat test map, verify the new hit count, and revert or retain it. Explain the difference between changing a shared default and changing one placed actor instance.

---

## 28. Content generation and reproducibility

Automate repetitive construction so the owner does not need to manually place every wall or wire every gate. The generator should produce actual reusable content, save it, and validate references.

### 28.1 Generation sequence

1. Verify the project and native classes load.
2. Create or locate base materials and mesh resources.
3. Create required lightweight asset wrappers and data definitions.
4. Create the mission map and optional front-end/test maps.
5. Place architecture, collision, doors, lights, signs, and scenery.
6. Place player starts, checkpoint starts, mission director, terminals, pickups, stations, encounters, enemy spawn markers, and pod.
7. Assign stable IDs and references.
8. Build or prepare navigation and validate required reachable paths.
9. Save packages and maps through Unreal's asset APIs.
10. Run content validation and open the saved map for inspection.

### 28.2 Rerunning safely

Use a manifest of generated assets and actor IDs. Rerunning normal generation updates known generated content or skips already-correct content. It must not duplicate every enemy, light, and wall. Do not delete manually edited content silently.

Provide a clearly named explicit rebuild mode only for generated content, with a backup or recoverable copy before replacing a saved map containing owner edits. The default path should preserve existing hand edits. Never make launching the game automatically regenerate the level and overwrite changes.

Generated JSON, tables, or configuration can describe room placement, but they do not replace the resulting `.umap` and assets. Save and inspect the actual editor result. If regeneration needs a compiler step before native classes are exposed, document that order and enforce it in the bootstrap route.

### 28.3 Idempotency validation

Run content generation a second time in the intended safe mode. Verify stable mission actor counts, unique IDs, no duplicate player starts designated as primary, no doubled lights, and no broken references. If the generator is a one-time bootstrap by design, it must detect an existing generated project and stop safely with an explanation rather than overwrite it.

### 28.4 Deterministic configuration

Use authored spawn lists, fixed room placement, and controlled visual variation. Do not depend on random enemy spawn positions for the acceptance route. If decorative randomization is used, store a seed and keep it separate from gameplay. A failed test must be reproducible.

---

## 29. Performance and platform goals

Measure on the actual development hardware. The target is responsive play at 1080p or a sensible laptop resolution with a Medium preset, ideally around 60 frames per second on supported hardware. This is a target, not a verified claim. If the host cannot sustain it, lower unnecessary visual cost, measure again, and report the actual resolution, preset, hardware, and frame-time observations.

### 29.1 Initial implementation budgets

- At most five active enemies at once in the baseline, with the authored encounters using no more than four simultaneously.
- At most two enemies beginning active bursts concurrently.
- Limited projectile lifetime and bounded transient effects.
- AI perception around five checks per second per active enemy rather than every rendered frame.
- Shared materials, static scenery, and restrained shadow-casting lights.
- No runtime asset scanning or repeated full-world actor queries in ordinary frame updates.
- No permanent tick on every pickup, sign, door, and UI element where events or timers suffice.
- No mandatory hardware ray tracing, expensive real-time reflections, or high-cost fog.

Use built-in engine scalability where appropriate. The Low preset can reduce shadows, post-processing, and effects, but cannot remove attack telegraphs, objective readability, or necessary collision. If lights are baked, ensure the workflow and generated lighting are valid; if dynamic, manage their cost intentionally.

### 29.2 Navigation requirements

Build a continuous navigable route for each enemy's encounter space. Keep enemy spawn markers on valid ground and outside cover collision. Validate ramps, door openings, and wide enough turning spaces. Avoid runtime navigation rebuilds every time a decorative object moves.

A gate can block enemy access intentionally, but an active enemy cannot be trapped on the wrong side while counted as required for opening that gate. Use local encounter routes and test the furthest spawn positions.

### 29.3 Stability and cleanup

Restart the mission repeatedly and watch for duplicated widgets, accumulating enemies, repeated delegate subscriptions, audio loops, or projectile counts that never return to zero. Stop timers when actors die or maps unload. Use engine-supported ownership so references do not outlive their targets.

Shipping gameplay must work with the network disconnected. No remote authentication, cloud save, live model call, or telemetry endpoint is required to play this level.

---

## 30. Build, package, and launch requirements

Set the editor starting map and game default map intentionally. Ensure the packaged build includes its front-end and mission maps, all referenced gameplay assets, input configuration, and runtime dependencies. A project opening in the editor is not proof that the packaged game starts correctly.

Epic describes packaging as building, cooking, staging, and bundling the project for a target platform. Use the installed engine's supported packaging route and inspect its logs. [Epic: Packaging Unreal Engine Projects](https://dev.epicgames.com/documentation/en-us/unreal-engine/packaging-your-project)

### 30.1 Local scripts

Provide a straightforward local build script and, when possible, a local package script appropriate to the host OS. Resolve the engine path through documented configuration or discovery; do not hard-code a different user's machine path. Quote paths correctly, including spaces.

Scripts should fail visibly on missing prerequisites, preserve useful logs, and return a failure status for failed builds. They must not print a success message simply because a subprocess launched. Use bounded retries for known transient editor readiness issues; investigate actual build errors instead of repeating the same failing command indefinitely.

### 30.2 Package target

Package for the current development OS first. If on macOS, deliver a local macOS application when the engine/toolchain supports it. If on Windows, deliver the appropriate local executable and its required data directory. Do not deliver only the executable if its cooked content lives beside it.

Do not claim cross-platform validation for a target that was not built and launched. Store signing, notarization, distribution certificates, console access, and public publishing are outside this local prototype delivery.

### 30.3 Launch validation

Close the running test instance, launch the packaged app from its output location, and confirm the title menu appears. Start a mission, verify mouse capture and keyboard input, fire the weapon, trigger an enemy, and confirm UI assets and sounds load. Then perform the complete packaged mission test if the environment allows it.

A successful cook followed by a black-screen launch is a failed delivery check. Missing fonts, missing map references, editor-only code dependencies, and unbound input are defects to fix before calling the package ready.

---

## 31. Implementation milestones and completion gates

Work in this order. Maintain `docs/BUILD_STATUS.md` with the current milestone, evidence, unresolved issues, and next concrete action. These are internal work stages, not requests to ask the owner for approval between every step.

### M0 — Environment and executable project shell

Discover the environment, select the engine, create or reuse the project, establish the source organization, compile the minimal runtime, and open an actual map. Gate: a real project loads without missing required modules. If unavailable, record the exact external blocker and continue independent file work.

### M1 — Movement and camera

Create the player, input mapping, movement, sprint, jump, shoulder aiming, collision-aware camera, and a visible character. Gate: the player can navigate a test space and aim near walls without leaving the world or losing control.

### M2 — Complete combat loop in a test room

Implement health, ARC-9 fire/reload/ammo, one trooper, enemy projectiles, death, retry, and minimal HUD. Gate: the player can kill the enemy, be killed by the enemy, reload correctly, and retry without duplicates or stale timers.

### M3 — Interaction, progression, and checkpoints

Implement terminal, doors, pickups, station, encounter accounting, mission director, save snapshot definitions, and checkpoint reload. Gate: a small test chain can require combat, unlock a terminal, restore from checkpoint, and finish without inconsistent state.

### M4 — Entire mission in simple geometry

Create all six connected zones, exact encounter rosters, gate dependencies, CP00/CP01/CP02, final bay, pod boarding, and a minimal ending. Gate: the whole mission is completable from a fresh start with plain materials and all core systems active. This is the earliest acceptable playable prototype, not the final stopping point if work can continue.

### M5 — Presentation and menus

Apply consistent materials, lighting, signage, sound, effects, front end, pause, controls, settings, death screen, and completion screen. Gate: required UI works at test resolutions and the five key level views look intentional. No required button is decorative.

### M6 — Recovery, quality, and local packaging

Exercise saves, reloads, deaths, menu transitions, low-ammo recovery, navigation failures, and full mission completion. Fix blockers. Package for the host platform and test launch. Gate: evidence matches the actual verified capabilities; remaining issues are explicit.

### M7 — Owner handoff

Complete the editing guide, asset ledger, test report, launch instructions, and known issues. Include exact output paths. Gate: a new reader can open, play, change a tuning value, and understand what remains without reading the original chat.

### 31.1 Scope discipline while building

If complexity grows, simplify presentation and implementation technique before cutting the full mission loop. Use fewer decorative assets, simpler animation, and basic camera movement for the ending. Preserve movement, combat, objectives, checkpoints, and a complete escape outcome. Do not replace a broken core mechanic with a note saying the owner can implement it later.

---

## 32. Autonomous working procedure

Read this document fully before implementing so later requirements do not contradict early architecture. Then establish the project and complete the milestones. Write compact progress updates focused on finished behavior and the next uncertainty being resolved.

### 32.1 Make routine choices

Choose simple names, local code structure, material parameters, panel shapes, exact prop transforms, and audio synthesis details without repeatedly asking the owner. If a value is missing, choose the smallest maintainable solution consistent with the stated player experience, record the decision, and continue.

When a choice would fundamentally change the engine, perspective, platform, paid spending, multiplayer scope, or use of the owner's existing work, request clarification. A build instruction authorizes this game's local implementation; it does not authorize an unrelated product or external publication.

### 32.2 Validate incrementally

Compile the smallest viable gameplay module early. Verify one instance of a system before replicating it across six rooms. Run the mission after changes to progression and checkpoints. Keep a known working state or recoverable version before risky engine or map edits.

Do not write thousands of lines against guessed engine APIs and only try compilation at the end. Inspect installed headers or official documentation where needed. Do not install a random third-party automation bridge merely because its name suggests Unreal support.

### 32.3 Avoid user assembly work

Prefer automatically creating widgets, assigning references, saving maps, and arranging actors. If a manual editor step is unavoidable because the available tools cannot perform it, make the exact step minimal, explicit, and tied to a verified limitation. State which file or asset is prepared and what the owner must do. Never imply a missing manual step has already been completed.

### 32.4 Interruption and continuation

After meaningful milestones, update the status file with current paths, engine version, completed systems, failed checks, and the next command/action. On resumption, read that record and inspect current files before rewriting anything. Do not regenerate the whole project blindly or discard owner modifications made since the previous run.

If context or usage limits interrupt work, leave a truthful resumable state. A large specification increases clarity; it does not override runtime limits or guarantee all tasks finish in one model response.

---

## 33. Functional acceptance tests

Execute relevant tests in the engine using supported automation or direct play. Tests that require a visible interaction must be observed or explicitly marked unverified. A static code check cannot prove a camera, enemy, or button feels correct in play.

Use `PASS`, `FAIL`, or `NOT RUN`, with environment, evidence, and concise notes. An implementation checkbox is not test evidence. Fix failing required tests before claiming the vertical slice is complete.

| ID | Scenario | Expected result |
|---|---|---|
| A01 | Open project from a fresh editor launch | Correct project and saved map load; no required missing assets/modules |
| A02 | New Mission with no save | Player begins in cell, unarmed, 100 health, correct objective |
| A03 | Move, look, sprint, jump | Inputs behave consistently; capsule stays on valid surfaces |
| A04 | Aim beside a wall and beneath low structure | Camera collision prevents persistent clipping or exterior escape |
| A05 | Attempt to leave detention unarmed | Exit directs player to blaster; no unrecoverable trap |
| A06 | Acquire ARC-9 repeatedly | Weapon appears once; ammo initializes once; rack does not duplicate rewards |
| A07 | Fire magazine continuously | Exactly 24 accepted shots before reload; cadence independent of frame rate |
| A08 | Reload a partial magazine | Correct deficit transferred from reserve after completion |
| A09 | Cancel reload by sprinting or dying | No free ammo; timers cleared or safely cancelled |
| A10 | Fire with muzzle behind cover but camera sees target | Cover blocks damage; tracer agrees with actual hit |
| A11 | Shoot standard trooper in body | Three valid 20-damage hits defeat 60-health enemy |
| A12 | Shoot sentinel in body | Twelve valid hits defeat 240-health enemy unless tuning changed and documented |
| A13 | Observe enemy attack | Visible charge precedes moving non-homing bolts |
| A14 | Hide fully behind solid cover | New enemy attacks require line of sight; bolts hit cover |
| A15 | Let enemy see player, then break sight | Enemy stops omniscient tracking and uses last seen behavior |
| A16 | Receive damage and collect medicine | Health changes correctly and clamps at maximum |
| A17 | Cross a pickup repeatedly / at full capacity | No duplicate reward; full-capacity pickup remains available |
| A18 | Run out of all ammunition in each combat zone | Reachable station restores enough reserve to continue |
| A19 | Activate encounter twice | No duplicate roster, waves, or clear events |
| A20 | Complete E01 | Cargo gate opens; CP01 available at threshold |
| A21 | Clear Cargo first group | Second group arrives once after warning/delay |
| A22 | Clear both Cargo groups | Security Junction gate opens and objective advances |
| A23 | Begin terminal hold and release early | Progress resets; no partial completion |
| A24 | Complete terminal hold | E04 activates once; gate stays locked until enemies cleared |
| A25 | Clear E04 | Reactor gate opens; CP02 can commit |
| A26 | Kill troopers while sentinel remains alive | Reactor exit remains locked |
| A27 | Clear complete E05 roster | Evacuation access opens |
| A28 | Reach pod while E06 enemies remain | Boarding blocked with understandable message |
| A29 | Clear E06 and complete boarding | Ending starts once; combat is disabled |
| A30 | Watch ending or skip it | Same complete state, usable menu, completed save |
| A31 | Retry from CP00, CP01, and CP02 | Correct loadout, objective, doors, pickups, and encounter states |
| A32 | Re-cross old checkpoint after newer one | No downgrade or free repeated healing |
| A33 | Die during reload/interaction/burst | No stale gameplay timers after retry |
| A34 | Pause during projectile flight and terminal hold | Gameplay freezes; menu remains usable |
| A35 | Resume while mouse click released | No unintended shot or stuck input |
| A36 | Main Menu then Continue | Loads last committed checkpoint, not arbitrary live position |
| A37 | Relaunch application with valid save | Continue works and settings persist |
| A38 | Missing, corrupt, or incompatible save | Safe explanation and New Mission; no crash |
| A39 | Full-health or maximum-ammo state | Pickups and UI obey capacity limits |
| A40 | Restart mission three consecutive times | One player/HUD/roster; no accumulating audio or actors |
| A41 | Reach every required interactable | Range, focus, prompt, and collision agree |
| A42 | Test 720p, 1080p, and available laptop ratio | Essential UI readable and within bounds |
| A43 | Change every exposed setting | Each setting has observable effect and persists |
| A44 | Low graphics and reduced flashes | Essential telegraphs, signs, and navigation remain visible |
| A45 | Rerun content bootstrap in safe mode | No duplicated content or silent loss of owner edits |
| A46 | Launch packaged build | Main menu, maps, input, character, UI, and audio load |
| A47 | Full packaged mission run | Cell-to-pod completion without a debug shortcut |
| A48 | Play with network disconnected | No login/network dependency prevents mission play |

If a fallback modifies architecture, retain equivalent behavior tests. Do not mark A47 passed using only an editor run. Do not mark A37 passed using only an in-memory retry.

---

## 34. Automated checks and diagnostic tools

Prefer a small meaningful test set focused on state transitions and failure-prone calculations. Do not add hundreds of shallow tests that merely repeat literal constants.

### 34.1 Logic checks

- Health clamps correctly and emits death once.
- Ammo transfer handles full magazines, empty reserves, partial reserves, and cancellation.
- Encounter resolution ignores repeated deaths and does not clear while a member remains active.
- Checkpoints cannot regress from CP02 to CP01.
- Door conditions match the authoritative dependency table.
- Terminal completion cannot occur twice or after death.
- Save validation rejects unknown mission IDs and invalid required state.
- Mission completion occurs once whether the ending is watched or skipped.

### 34.2 Content checks

- All required maps exist and are configured for the appropriate launch/cook route.
- Exactly one primary mission director and intended player start exist.
- Every mission actor has a unique stable ID.
- Every encounter has a nonempty valid roster matching the baseline, unless a documented tuning revision changes it.
- Required references resolve to actual saved assets and actors.
- Door IDs and checkpoint IDs match known definitions.
- Every enemy spawn is on safe ground with a valid navigation route in its encounter area.
- Every combat zone has its required reachable ammo station.
- Pod boarding and the success UI are connected to the mission coordinator.
- Runtime assets do not depend on editor-only scripts or modules.

### 34.3 Development-only conveniences

Optional debug commands can set a checkpoint, show mission phase, show encounter membership, display collision/nav paths, or refill health for diagnosis. Keep them disabled or inaccessible in ordinary release play. Document when a test used a shortcut. A debug-assisted traversal is useful diagnostic evidence but is not a successful normal mission playthrough.

---

## 35. Playtesting and tuning procedure

Complete at least one uninterrupted developer run from New Mission to Mission Complete without cheats. Also perform deliberate deaths at CP01 and CP02, use all menus, and exhaust ammunition once to exercise recovery. Capture actual mission duration when possible.

Then assess experience separately from technical completion. Questions for the owner or another human tester:

1. Was it obvious where to go after leaving the cell?
2. Did aiming hit what the crosshair suggested?
3. Could you recognize an enemy attack before taking damage?
4. Did each room feel different enough to remember?
5. Did you understand why the transit gate was locked?
6. Did retries restore a sensible state without excessive repetition?
7. Did the sentinel feel tougher for readable reasons?
8. Did you run out of ammunition, and could you recover?
9. Was the escape pod clearly the final destination?
10. Did the ending feel like completing a mission?

The implementing agent should not invent answers from a human tester. If only automated or agent-controlled play is available, label the experience assessment accordingly. The owner can provide human feel feedback after the first build; its absence does not prevent delivering the verified technical result.

### 35.1 Tuning order

Fix blocking defects first, then control feel, then shot correctness, then enemy readability, then difficulty, then route clarity, then presentation. Change a small related set of values at a time. Record meaningful tuning changes with their reason so the baseline tables do not silently become misleading.

If first-run length is under ten minutes but the mission feels complete, report the measured duration. Add useful encounter or environmental variety only after feedback; do not inflate the mission automatically. If a first run is too long because the player is lost, improve route guidance rather than calling it more content.

---

## 36. Risk register and concrete fallback decisions

| Risk | Early signal | Response |
|---|---|---|
| Unreal absent | No usable installed editor/toolchain | Prepare independent source/setup work; request exact installation prerequisite; mark runtime unverified |
| Engine API mismatch | Minimal compile or asset script fails | Inspect installed headers/docs, adapt to pinned version, rerun smallest test |
| Blueprint graph automation unreliable | Asset creation works but behavior graphs fail | Move behavior to native classes and use simple wrappers |
| Editor scripting unavailable | No verified way to create/save maps | Try installed command-line/editor facilities; report specific required editor step if no route exists |
| Imported character incompatible | Animation skeleton mismatch or missing references | Use discovered compatible template asset or simple original synthetic avatar |
| Laptop rendering too slow | Consistent high frame times during play | Reduce lights/shadows/post-processing, keep low-cost materials, measure again |
| Navigation deadlock | Active enemy cannot reach or see player | Fix spawn, clearance, or local paths; validate encounter before presentation work |
| Checkpoint inconsistent | Gate/objective/enemy state disagrees after retry | Restore from canonical snapshot through one coordinator, reload map cleanly |
| Missing packaged assets | Editor works but package is blank/broken | Verify map cook list, references, runtime-only dependencies, and launch configuration |
| Audio sources unavailable | No permitted usable sound pack | Generate simple original local effects and import them |
| Ambitious ending stalls work | Large cinematic or exterior asset dependency | Use simple pod movement, ship silhouette, short camera sequence, and success screen |
| Task interrupted | Usage/context/tool availability ends | Preserve BUILD_STATUS, known working state, exact next actions, and verification limits |

Fallbacks preserve the intended playable result. A fallback is not permission to claim a system exists when it has not been created or tested.

---

## 37. Definition of done and deliverable inventory

The technical vertical slice is complete when the player can perform the entire mission loop, required state recovery works, the actual project loads, core acceptance tests pass, and remaining verification limitations are clearly stated. The intended final delivery also includes the presentation pass and local package where supported.

### 37.1 Required project deliverables

- Real `OrbitBreakout.uproject` and valid project configuration.
- Runtime source and required editor-generation source, if used.
- Real saved mission map, required assets, UI, tuning data, and optional test map.
- One complete playable first level with the required combat, interaction, checkpoint, and ending behavior.
- Build and content-generation scripts that match the actual environment.
- Local package and launch instructions if packaging is supported and verified.
- Copy of this specification at `docs/PRD.md`, with any accepted implementation changes noted.
- Accurate README, environment report, architecture summary, editing guide, tests/results, asset ledger, known issues, and build status.

### 37.2 Evidence requirements

For build claims, keep the actual command/result and useful log location. For visual claims, inspect the running game or rendered screenshots. For playability claims, complete the relevant in-engine behavior. For performance claims, name the hardware, resolution, preset, and measurement conditions. For package claims, launch the package rather than inferring success from an output directory.

Screenshots should show real game output, not concept art presented as implementation evidence. If screenshot capture is unavailable, say so and describe which other checks were completed.

### 37.3 Final handoff format

The final reply to the owner should be short and practical:

1. Project and packaged-build locations.
2. The exact way to open and play.
3. Main controls.
4. What is verified and what remains blocked or unverified.
5. The most useful next playtest feedback to collect.

Do not make the owner read a long engineering postmortem before launching their game. Put details in the project documentation.

---

## 38. Future editing boundaries

After the first complete build, the owner may request more rooms, better art, new weapons, abilities, or additional missions. Preserve the working mission while adding those changes incrementally. Keep stable checkpoint and encounter IDs where possible; when save compatibility changes, bump the schema and provide a safe fallback.

Potential later directions include richer agent gadgets, optional stealth paths, a second weapon, additional synthetic enemy roles, a longer ship route, improved character animation, and controller support. These are future possibilities, not hidden requirements for this first task.

Before expanding, fix any unresolved input, aiming, save, navigation, or packaged-build defects. New content should reuse the reliable systems established here rather than accumulate separate incompatible copies.

---

## 39. Quick requirement cross-reference

| Owner intention | Concrete implementation | Verification |
|---|---|---|
| “Single player” | Offline local runtime; no accounts or server | A48 |
| “Fight your way out of a spaceship” | Six linked zones, 18 enemies, terminal gate, pod ending | A20–A30, A47 |
| “Unreal Engine” | Actual UE5 project, source, saved maps and content | A01, A46 |
| “First level” | One complete mission with a beginning and ending | A02, A29–A30, A47 |
| “Do it all for me” | Automated creation, wiring, build, testing, documented minimal setup | M0–M7, deliverable inventory |
| “Then continue editing” | Editable tuning, organized content, safe bootstrap, editing guide | A45 and owner editing exercise |
| Student time constraints | Clear defaults, short play sessions, checkpoints, simple handoff | A31, A36–A37 |
| Controlled spending | No paid assets or new paid services required | Asset ledger and actual dependency list |

---

## 40. Final execution directive

If this file is supplied with the instruction **“build this,”** treat it as the complete implementation brief for Orbit Breakout's first mission. Inspect the project environment, create the working Unreal project, implement the full mission, verify it, and deliver the actual files and supported playable build. Use the defaults in this document to make progress without asking the owner to redesign the game at each step.

Keep the product coherent: one agent, one blaster, one ship route, readable synthetic enemies, a security bypass, reliable retries, and a visible escape. Make the result easy to open, play, and improve. Be precise about what exists and what has been tested. Continue through the necessary implementation work until the requested result is achieved or a concrete external prerequisite prevents further progress.

**End of specification.**

