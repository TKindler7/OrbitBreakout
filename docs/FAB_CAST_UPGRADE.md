# Fab cast upgrade — November 10, 2026

The playable character uses Paragon Wraith and his authored rifle, materials, and combat animations. Fifteen regular enemies use Sci-Fi Trooper Man 03 with 97 saved Manny-to-Trooper retargeted animations. The heavy sentinel remains the Sci-Fi Beast bull. Star Sparrow 01 replaces the escape-pod silhouette and travels with the existing departure sequence; the interaction point is a small boarding console.

Moving fire now blends recoil above the spine while preserving leg locomotion. Aim-facing rotation interpolates rather than snapping on each shot. Shots and muzzle effects use Wraith’s authored Muzzle_01 socket. HELIX attachments use that same coordinate space.

Validation: native editor build passed; saved map contains 15 troopers, one bull and one ship. All 114 referenced character animation clips loaded with matching skeletons. Six automation checks passed: moving recoil/skeleton compatibility, HELIX switching, HELIX memory persistence, enemy awareness/cover, death disappearance, and boss telegraphs/sweeps/jump/cover. See FAB_CAST_AUTOMATION_RESULTS.json.

Limitation: the separate existing boss-intro camera automation crashed with recursive SetViewTarget/ClientSetViewTarget calls in its isolated fixture. It is not counted as passing. A complete mission playthrough has not been performed for this update.

Scripts/verify_fab_cast.py saves and checks the cast in the level. Scripts/playtest_fab_cast.py performs a temporary runtime preview without starting a mission or saving a checkpoint.
