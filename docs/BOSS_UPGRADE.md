# Boss, weapon and environment upgrade

This November 6 revision makes Reactor a solo Bull fight and places a stronger weapon immediately before it. It updates the original one-weapon and escort-roster design in response to the owner's new request. All work remains in the local editable project.

## Current verification

| Check | Status / evidence |
|---|---|
| Native editor compilation | **PASS** — arm64 and x64; 33/33 actions, zero failed or blocked; `Saved/Logs/BossUpgradeBuild.log` |
| Saved map regeneration and structural validation | **PASS** — 2,168 actors, 16 enemies, one 2,400 HP Bull, pre-boss HELIX rack; `BOSS_UPGRADE_CONTENT_RESULTS.json` |
| Navigation, including HELIX approach and boss dodge routes | **PASS** — 23 routes and 49 projected points; `CONTENT_VALIDATION.json` |
| New boss, weapon and solo-gate automation checks | **PASS** — 4 checks; `BOSS_UPGRADE_TEST_RESULTS.json` |
| Existing isolated enemy/logic regression checks | **PASS** — 6 checks, 10 total with zero failures |
| Runtime held weapon and visual inspection | **PASS** — HELIX held pose and HUD in a cosmetic PIE preview; Cargo, Reactor and rack inspected in editor. Normal gameplay pickup is covered by inventory/core tests, not an end-to-end input playthrough. |
| Full mission playthrough and balance feedback | **PENDING** |
| Standalone package containing this revision | **NOT BUILT** — `Builds/VisualUpgradeFinal/Mac/OrbitBreakout.app` predates these changes |

The new map was saved after navigation validation. The owner’s existing mission save retained SHA-256 `6c67f3ad71246e12533dd8179ee855dfde6ca9971dbfeb73c7166fcac7f8f43e`. Earlier checkpoint/package documents describe older revisions. Native test warnings are unloaded hand sockets in isolated test pawns, not material or combat failures. Unreal also displayed a system memory-pressure warning during visual checks; performance was not benchmarked.

## Ship and arena

The generator adds broader portal frames, hull shoulders, matte walkway panels and distinct room signage. Larger landmarks distinguish Detention, Security, Cargo, Control and Departures. Cargo and the escape bay overlook Aurelia, with cloud variation, an atmospheric rim and orbital structures. Lighting uses restrained blue fill and warm reactor accents.

The reactor machinery moves to the side of its room, leaving the center open for rushes and sideways movement. Cover remains at the perimeter; floor markings identify the arena. Decorative additions do not block navigation. The generator validates the HELIX approach and both reactor dodge routes alongside the existing traversal paths.

`E05_EN03` is the only enemy in Reactor. Escorts `E05_EN01` and `E05_EN02` are removed. The complete mission has **16 enemies: 15 troopers and one Bull**, distributed as 3, 3, 2, 3, 1 and 4 across encounters 1–6. The exit still opens only when the Bull is defeated.

## HELIX-12 plasma repeater

The `I05_HelixRifle` rack sits at `(12200, 280, 0)`, on the safe side of the Reactor encounter trigger. Use **E** to claim it; it equips automatically. Reactor activation waits until it is acquired. Use **1** for ARC-9, **2** for HELIX-12 or **Q** to switch.

| Property | HELIX-12 |
|---|---|
| Damage | 36 per hit |
| Shot interval | 0.13 seconds |
| Magazine / reserve capacity | 36 / 180 |
| Ammunition on acquisition | Full magazine plus 144 reserve |
| Reload | 1.7 seconds |
| Recovery station supply | Raises selected HELIX reserve to at least 108 |

The weapon retains the authored rifle grip and poses, with an extended shroud, energized rails, capacitor and different tracer presentation. Each gun retains separate ammunition. Switching cancels pending reload callbacks and cannot bypass firing cadence. Repeating the acquisition does not duplicate ammunition. The HUD identifies the selected gun and its ammunition.

## Bull sentinel

The owned SciFi Beast06 Skin1 mesh and compatible idle, movement, attack and death animations remain assigned. Its forearm emitter uses the imported skeleton's `lowerarm_r` bone.

| Mechanic | Behavior |
|---|---|
| Health | **2,400**, increased from 240 |
| Pulse burst | Existing charged three-bolt ranged attack; cover blocks shots |
| Bull rush | 1.2-second lane warning, then a committed straight rush up to 1,050 cm at 950 cm/s; 28 damage |
| Shockwave | 1.35-second warning, then a low expanding ring up to 720 cm at 600 cm/s; 24 damage |
| Overdrive | Begins at 50% health or below; special recovery falls from 2.15 to 1.35 seconds and ranged recovery from 2.2 to 1.45 seconds |
| Telegraphs | Ground lane/ring plus boss HUD health and attack instructions; warning duration stays unchanged in overdrive |

The rush uses capsule sweeps, stops at blockers and does not home after its warning. Its lane is conservatively wider than the combined player and boss collision radii. Jump above the low shockwave or use solid cover; the wave checks both height and line of sight. Each special can damage the player at most once. Death or cancellation removes the telegraphs and releases the attack slot. The boss remains damageable throughout the fight and adds no reinforcements.

## Checkpoint compatibility

Schema version 1 remains supported. Additive `bHelixOwned` and `HelixReserveAmmo` fields default to false and zero when absent, leaving the new rack available for older checkpoints. New interaction kind `HeavyWeapon` is appended after the existing enum values, preserving their serialized ordinals.

Claiming HELIX after checkpoint 2 records ownership through ordinary gameplay. Checkpoint recovery restores the acquired weapon with a full magazine and at least 108 reserve rounds, while ARC ammunition remains separate. **New Mission / Replay replaces the existing mission checkpoint.** The isolated tests use transient worlds or in-memory archives and do not access that slot.

## Verification targets

Run the isolated `OrbitBreakout.Boss`, `OrbitBreakout.Weapons`, `OrbitBreakout.Upgrade`, `OrbitBreakout.Enemies` and `OrbitBreakout.Logic` filters after restarting the rebuilt editor. The new tests cover actual swept collision, warning time, jump/cover avoidance, damage-once behavior, overdrive and death cleanup; inventory/reload separation and memory save roundtrip; and HELIX-before-boss activation plus solo death-to-gate progression.

The existing `OrbitBreakout.Mission.PIE.ProgressionAndRecovery` test requires no pre-existing mission save. Do not rename, move or replace owner progress to force it to run. Its previous pass is historical and is not a full-playthrough result for this revision.
