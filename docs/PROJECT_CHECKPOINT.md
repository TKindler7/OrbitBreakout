# November 6 morning project checkpoint

Open `OrbitBreakout.uproject` in Unreal Engine 5.8.4. The main map is `/Game/OrbitBreakout/Maps/L_EmergencyDeparture`.

Saved work:
- SciFi Beast06 Bull imported from the owned Fab library and assigned to reactor heavy sentinel `E05_EN03`, with matching idle/walk/attack/death animations, original materials and 1.08 scale. Saved map and appearance verified in the editor.
- Defeated enemies hide after 1.25 seconds, retaining mission bookkeeping before actor cleanup.
- Active guards react to nearby players, movement, shots and same-encounter squad reports; eight-second last-known-position memory. Visual targeting still requires line of sight.
- Held rifle, detailed humanoid guards and industrial environment from the preceding visual pass are included.

Verification: native editor build passed all 18 actions for arm64 and x64 (`Saved/Logs/EnemyAwarenessBuild.log`). Six in-engine focused enemy/logic tests passed (`ENEMY_BEHAVIOR_TEST_RESULTS.json`). The enemy tests use isolated worlds and do not reset mission saves. Test warnings include absent meshes in those isolated fixtures and an unrelated Fab authorization-pending response during login.

The editable project is the current version. `Builds/VisualUpgradeFinal/Mac/OrbitBreakout.app` is the preceding packaged build and does not include the Bull or latest enemy behavior. Bull runtime combat and repackaging remain for the next session.

Editor mission-save SHA-256 at checkpoint: `3678cd66c5b999fee4ecc657904c70ee75beef899109bc2e99844a1f87db03e1` (unchanged).
