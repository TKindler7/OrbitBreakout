# Visual upgrade — packaged and launched

Updated November 6, 2026. This pass replaces the block-model guards, gives the player a held rifle, and adds industrial detail to the ship. The saved editor project now also includes the Bull sentinel and improved awareness/death cleanup. The existing app at `Builds/VisualUpgradeFinal/Mac/OrbitBreakout.app` predates these latest additions; see `PROJECT_CHECKPOINT.md`.

## Implemented

- **Held blaster:** The installed Epic `SM_Rifle` mesh, textures and materials are copied into `Content/Weapons/Rifle`. The player uses authored rifle idle, movement, aim, fire, reload and death animations through `OBRifleAnimInstance`. The simplified Manny mesh lacks the `weapon_r` bone, so its `hand_r` attachment uses the measured transform from the authored rifle pose. The muzzle follows the rifle tip.
- **Humanoid security guards:** Regular enemies use the same detailed Manny body as the player, shared rifle poses, and skeletal weapon attachment. The armor material copies preserve the original texture, normal, roughness and mask detail while adding a full-body `ArmorTint` parameter.
- **Ship environment:** Dark alloy surfaces, restrained cyan and amber lighting, ducts, vents, consoles, overhead supports, machinery, crate trim, cargo crane details and reactor fixtures replace the sparse presentation. The generated map contains 1,851 actors, including 928 dressing actors. New dressing does not add collision.
- **Heavy presentation support:** Native properties now accept a separate creature mesh, its idle/move/attack/death clips, visual scale and an attached forearm pulse emitter. This supports the requested Bull without changing the existing heavy encounter or navigation capsule.

## Bull replacement is saved

The owned [SciFi Beast06 Bull](https://www.fab.com/listings/126120a3-996e-4dc3-94f1-6550f0adac03) was downloaded through Unreal's Fab integration into `Content/SciFi_Beast06`. Mesh `SK_SciFi_Beast06_Skin1` and compatible `Idle1`, `WalkF`, `Attack1`, `Death1` animations are assigned to the reactor heavy `E05_EN03`. The mesh retains its authored materials, uses the UE4 skeleton with `lowerarm_r`, and is scaled to 1.08. Its reactor placement, facing and appearance were inspected. The level was saved at 16:03:30 UTC; `BULL_INTEGRATION_RESULTS.json` records the exact references.

`Scripts/heavy_bull.py` reapplies the assignment to the existing level. The generator also applies it whenever the imported pack is present. Runtime combat with the Bull and a new standalone package remain to be verified.

## Verification status

- The saved environment passed structural validation, 47 navigation projections and 22 navigation paths; evidence is in `docs/CONTENT_VALIDATION.json`.
- The final native editor build passed for arm64 and x64, including the hand-frame correction and armor integration; see `Saved/Logs/VisualUpgradeEditorBuild4.log`.
- All five native tests passed at 06:04:56 UTC: four save/settings logic tests and the mission progression/recovery test. The mission test took 17.11 seconds and exercised real aim/fire input against one guard. Later encounters use scripted setup and normal damage APIs; this is not a human playthrough. Results are in `docs/VISUAL_UPGRADE_TEST_RESULTS.json`.
- A separate live AI observation passed at 06:07:07 UTC. Both guards moved, enemy projectiles were observed, and normal damage reduced player health from 100 to 10 without injected damage. See `docs/AI_RUNTIME_RESULTS.json`.
- Mac build/cook/stage/package/archive finished with exit code 0 at 06:11:13 UTC in 145.63 seconds, producing `Builds/VisualUpgrade/Mac/OrbitBreakout.app`.
- A fresh launch of that exact executable was verified at 06:13:36 UTC (PID 87329). The log reported `OB_READY` with the front end active, 18 enemies, seven doors and 24 interactions. The native screen showed Main; Continue entered Cargo, where the held rifle, player and humanoid guard armor were visible. This was a launch/Continue and appearance check, not a human playthrough.
- The first packaged run reported missing Nanite usage on `M_Metal`. `Scripts/fix_material_usage.py` persisted the required flags on `M_Metal` and `M_Emissive` at 14:54:56 UTC, without changing map actors. The generator now preserves those flags too.
- The final package at `Builds/VisualUpgradeFinal/Mac/OrbitBreakout.app` completed with exit code 0 at 15:00:50 UTC in 96.06 seconds. Its exact executable was freshly launched (PID 93140), and Main was visually confirmed at 15:01:07 UTC. The log reported the front end active, 18 enemies, seven doors and 24 interactions, with no material warnings, fatal errors, ensures or streaming errors. Evidence is in `docs/VISUAL_UPGRADE_PACKAGE_RESULTS.json` and `Saved/Logs/VisualUpgradeFinalLaunch.log`.
- The final package received a launch/Main check. Earlier mission, AI and Continue-to-Cargo checks exercised the same native code; only material usage flags changed afterward. A full gameplay run of the final archive has not been performed.
- The older `Builds/FirstDraft` package is retained as baseline evidence and does not include this visual pass. A full run of the final package, combat balance and measured performance remain separate checks.

## Content scripts

Run these in the full Unreal editor while Play In Editor is stopped:

- `Scripts/player_visuals.py` creates or repairs the textured ARC-9 material, including its Nanite usage flag.
- `Scripts/security_armor.py` creates the textured armor master and material instances under `/Game/OrbitBreakout/Materials/Characters`.
- `Scripts/fix_material_usage.py` persists Nanite usage for the existing native prop materials without regenerating the map.
- `Scripts/bootstrap_content.py --repair-env-materials` backs up and refines only the generated alloy material. It leaves map actors unchanged.
- `Scripts/bootstrap_content.py --repair` backs up and regenerates the authored mission content, then validates navigation before saving.

The original editor mission save has been restored and verified unchanged, including a further check at 14:58 UTC: SHA-256 `3678cd66c5b999fee4ecc657904c70ee75beef899109bc2e99844a1f87db03e1`. Packaged-game progress is separate; its latest available save was backed up before the first visual-upgrade launch.
