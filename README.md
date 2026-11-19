# Orbit Breakout

A two-chapter, single-player sci-fi game built by Teddy Kindler in Unreal Engine 5.8. Wraith escapes the detention ship in the Star Sparrow, crashes in the underground tunnel, and fights to bring a rescue transmitter online.

## Play on Mac

[Download the Apple silicon Mac build](Releases/OrbitBreakout-Mac.zip), unzip it, and open OrbitBreakout.app. This download uses the original controller look speed. The project source includes the newer, 35% slower right-stick tuning for Wraith and the loader. The archive SHA-256 is `d7a8b792f0cc657ef6db527c265cbb338cbe47e0e5b8ef3015263fda70755f84`.

## Open the Unreal project

1. Install Git LFS and clone the private repository. Run `git lfs pull` from the checkout to download the asset packs.
2. Install Unreal Engine 5.8 and the C++ toolchain for your platform. Generate project files and build the OrbitBreakoutEditor target. The Mac build helper is in `Scripts/build_project.py`.
3. Open `OrbitBreakout.uproject` and press Play. Choose **New Mission** or **Continue**.

## Play

Move with WASD or the left stick. Sprint with Shift or L3, then press Ctrl or R3 to slide. Jump with Space or Cross to traverse marked ledges and walls. Hold the right mouse button or L2 to aim down the sights. In the tunnel Warden bay, approach the ochre loader and press E or Square to board. Use the same button to exit.

## Project contents

`Source` contains gameplay and native tests. `Content` contains the two maps and Unreal assets, including Wraith, the troopers, the Bull, the Star Sparrow, the underground tunnel and the loader. `ArtSource`, `SourceAssets`, and `Scripts` preserve the original loader geometry, audio sources and authoring tools. Machine-generated build caches and local player saves are excluded.

The milestone report covers the packaged campaign, all seven checkpoint restarts, the Warden fight and known test limits. Project plans and verification results are included in `docs/plans` and `docs/verification`. [Milestone verification](docs/PILOT_UPGRADE_VERIFICATION.md)

## Asset rights

This repository is private because it includes third-party Unreal/Fab content, including SciFi Beast06 Bull. Those assets retain their original licenses; this repository does not grant redistribution rights. Do not make the complete asset repository public or distribute raw marketplace assets. The public portfolio describes the project without exposing those source assets.
