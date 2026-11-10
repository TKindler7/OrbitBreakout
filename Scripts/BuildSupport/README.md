# macOS build helpers

Run these commands from the `OrbitBreakout` directory. Python 3.10 or later is required. Engine discovery uses `--engine`, then `UE_ENGINE_ROOT`, then the project's `EngineAssociation` under `/Users/Shared/Epic Games`. Both an installation root and its `Engine` directory are accepted.

```sh
python3 Scripts/build_project.py --dry-run
python3 Scripts/build_project.py
```

The default route invokes the installed engine's normal `Build.sh`. It requires normal access to Unreal's build/cache locations. `--project`, `--target`, `--configuration`, `--architecture`, and `--jobs` select the build. Add further UnrealBuildTool arguments after `--`.

## Optional local action executor

```sh
python3 Scripts/build_project.py --executor local-actions --dry-run
python3 Scripts/build_project.py --executor local-actions --jobs 4 --resume
```

This route preserves UnrealBuildTool's header generation, toolchain selection and exported action dependencies, then runs those native commands directly. It avoids requiring the UBA action executor. `ProjectBuildHost.cs` sets only process-local Unreal cache directories, and reflects into the installed UBT API; it is version-sensitive. Bundled .NET SDK, runtime, reference assemblies, and native dependencies are discovered from the selected engine. No engine installation or global configuration is changed.

The persisted helper was verified with UE 5.8.4 on November 5, 2026 (Pacific time): it exported and rebuilt all 18 project translation units for arm64/x64, completed four link/metadata actions, and exited successfully with zero failures. Existing precompiled headers were reused. A second unchanged-graph run reused all 18 compile actions and successfully reran the four link/metadata actions. See `docs/BUILD_STATUS.md`, `Saved/Logs/PermanentBuildVerification.log`, and `Saved/Logs/PermanentResumeVerification.log` for evidence. Existing original working copies in `Intermediate/BuildHost` remain intact. This does not verify a different Unreal version or machine.

`--resume` reuses only compile actions previously completed by this script. It verifies command/response/environment fingerprints, compiler dependency files, all input/output modification times, and rebuilt prerequisite actions. Linking and metadata always rerun. The first run does not adopt older build outputs without a recorded successful fingerprint. Missing or changed inputs force compilation.

`--export-only` generates headers and `Intermediate/Build/Actions.json` without native compilation. `--actions-only` executes the existing graph without exporting it again; use that only while the target, source-file roster, headers and build settings still match the graph. After changing reflected headers or adding files, run the normal export/build route.

Each invocation keeps its own logs and action graph snapshot under `Saved/Logs/Builds/<UTC timestamp>/`. Successful local-action state lives in `Intermediate/BuildSupport/action-state.json`; per-project locking prevents two copies of this script building concurrently. A failure or missing declared output returns a nonzero status, and failed prerequisites block their dependents.

## Content, editor and packaging wrappers

```sh
Scripts/generate_content.sh --dry-run
Scripts/generate_content.sh --validate --dry-run
Scripts/launch_editor.sh --dry-run
Scripts/launch_editor.sh --game --dry-run
Scripts/package_game.sh --configuration Development --dry-run
```

These wrappers have been checked for shell syntax and argument construction. Their end-to-end Unreal operations are unverified. Remove `--dry-run` to invoke the corresponding operation. Generate content only after the native editor module compiles; package only after the mission exists and validates. `--game` uses the editor executable in standalone mode and is not a packaged application. The packaging wrapper uses stock `RunUAT.sh BuildCookRun` and archives by default to `Builds/Mac`.
