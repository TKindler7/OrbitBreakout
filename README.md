# Orbit Breakout — Emergency Departure

A single-player, third-person sci-fi escape game built by Teddy Kindler with AI assistance in Unreal Engine 5.8. Fight through a spaceship, recover the HELIX-12 plasma repeater, defeat the Bull Sentinel, and reach the escape pod.

## Features

- Six encounters across security corridors, cargo, control, reactor and escape areas.
- ARC-9 blaster and HELIX-12 repeater, separate ammunition and reloads.
- Enemies with visual awareness, hearing, squad alerts and investigation memory.
- Solo 2,400-health Bull boss with rushes, jumpable shockwaves, half-health overdrive and a cinematic entrance.
- Checkpoint recovery, settings, menus and an escape sequence.

## Run locally

1. Clone this private repository and install Unreal Engine 5.8 with the platform C++ toolchain.
2. Generate project files and build the OrbitBreakoutEditor target. On the original Mac setup, `python3 Scripts/build_project.py --executor local-actions --jobs 4 --resume` is also available; its engine path is machine-specific.
3. Open `OrbitBreakout.uproject` and `/Game/OrbitBreakout/Maps/L_EmergencyDeparture`.
4. Press Play, then choose New Mission or Continue. New Mission / Replay replaces the current checkpoint.

This is a native Unreal game, not a browser game. No current standalone download is published.

## Controls

WASD move · mouse look · Shift sprint · Space jump · right mouse aim · left mouse fire · R reload · E interact · 1/2 or Q switch weapons · Esc/P pause.

## Project contents

`Source/` includes native gameplay and automation tests. `Content/` contains the saved level, materials, characters, weapons, audio and animation assets. `Config/`, `Scripts/`, `SourceAssets/`, `Build/` and `docs/` preserve project setup, content authoring and development records. Generated caches, binaries, old packaged builds and personal save games are excluded.

## Status

The native editor build passed for arm64 and x64. Ten focused boss, inventory, enemy and persistence checks passed before the entrance was added. The entrance compiled and its assets were saved; its final 11-test run was interrupted by a connection outage and remains unverified. Full mission balance testing and a fresh standalone package are still pending. See `docs/BOSS_UPGRADE.md` and `docs/BOSS_INTRO_ASSETS.json` for details.

## Asset rights

This repository is private because it includes third-party Unreal/Fab content, including SciFi Beast06 Bull. Those assets retain their original licenses; this repository does not grant redistribution rights. Do not make the complete asset repository public or distribute raw marketplace assets. The public portfolio describes the project without exposing those source assets.
