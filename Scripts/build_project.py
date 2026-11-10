#!/usr/bin/env python3
"""Build Orbit Breakout on macOS with stock Unreal tools or a local action DAG.

The standard executor is the default. The explicit local-actions executor keeps
UBT's header generation and action export, then invokes the exported native
commands directly. Its reflection host is version-sensitive (initially UE 5.8).
It does not change the sandbox or engine installation. The local executor keeps
its generated tooling, cache data, logs and declared outputs in the project.
The standard executor uses Unreal's normal cache locations and permissions.

This route was verified with UE 5.8.4 on this Mac, including native compilation,
linking, metadata and unchanged-graph resume. Re-run and verify it before claiming
it has been tested with a different engine or machine.
"""
from __future__ import annotations

import argparse
from concurrent.futures import FIRST_COMPLETED, ThreadPoolExecutor, wait
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys


def version_key(path: Path) -> tuple[int, ...]:
    return tuple(int(part) for part in re.findall(r"\d+", path.name))


def newest(paths, description: str) -> Path:
    candidates = list(paths)
    if not candidates:
        raise RuntimeError(f"Could not locate {description} in the selected Unreal installation.")
    return max(candidates, key=version_key)


def project_file(value: str | None) -> Path:
    path = Path(value).expanduser() if value else Path(__file__).resolve().parents[1]
    if path.is_dir():
        projects = list(path.glob("*.uproject"))
        if len(projects) != 1:
            raise RuntimeError(f"Select one .uproject explicitly with --project: {path}")
        path = projects[0]
    if not path.is_file() or path.suffix != ".uproject":
        raise RuntimeError(f"Project file does not exist: {path}")
    return path.resolve()


def engine_dir(value: str | None, project: Path) -> Path:
    configured = value or os.environ.get("UE_ENGINE_ROOT")
    if configured:
        path = Path(configured).expanduser().resolve()
        path = path / "Engine" if (path / "Engine").is_dir() else path
    else:
        association = json.loads(project.read_text()).get("EngineAssociation", "")
        path = Path("/Users/Shared/Epic Games") / ("UE_" + association) / "Engine"
    if not (path / "Build/BatchFiles/Mac/Build.sh").is_file():
        raise RuntimeError("Unreal Engine not found. Supply --engine /path/to/UE_x.y or set UE_ENGINE_ROOT.")
    return path.resolve()


def checked_project_path(path: Path, project_root: Path) -> Path:
    result = path.resolve()
    if not result.is_relative_to(project_root):
        raise RuntimeError(f"Declared build output must remain inside the project: {result}")
    return result


def run_logged(command: list[str], log: Path, cwd: Path | None = None, env=None) -> int:
    print(shlex.join(command), flush=True)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w") as output:
        result = subprocess.run(command, cwd=cwd, env=env, stdout=output, stderr=subprocess.STDOUT)
    print(f"{'PASS' if result.returncode == 0 else 'FAIL'} log: {log}", flush=True)
    if result.returncode:
        print(log.read_text(errors="replace")[-14000:], flush=True)
    return result.returncode


def discover_dotnet(engine: Path) -> dict[str, Path | str]:
    host = "mac-arm64" if platform.machine() == "arm64" else "mac-x64"
    roots = [p.parent for p in (engine / "Binaries/ThirdParty/DotNet").glob(f"*/{host}/dotnet")]
    sdk_root = max(roots, key=lambda p: version_key(p.parent)) if roots else None
    if sdk_root is None:
        raise RuntimeError(f"Bundled .NET SDK for {host} is missing.")
    sdk_version = newest((sdk_root / "sdk").glob("*"), ".NET compiler SDK")
    refs_version = newest((sdk_root / "packs/Microsoft.NETCore.App.Ref").glob("*"), ".NET reference assemblies")
    references = newest((refs_version / "ref").glob("net*"), ".NET target framework")
    runtime = newest((sdk_root / "shared/Microsoft.NETCore.App").glob("*"), ".NET runtime")
    return {"root": sdk_root, "dotnet": sdk_root / "dotnet",
            "compiler": sdk_version / "Roslyn/bincore/csc.dll", "references": references,
            "tfm": references.name, "runtime": runtime.name,
            "native_rid": "osx-arm64" if host == "mac-arm64" else "osx-x64"}


def prepare_export_host(engine: Path, project_root: Path, logdir: Path) -> tuple[Path, Path]:
    sdk = discover_dotnet(engine)
    work = project_root / "Intermediate/BuildSupport"
    work.mkdir(parents=True, exist_ok=True)
    source = Path(__file__).resolve().parent / "BuildSupport/ProjectBuildHost.cs"
    binary = work / "ProjectBuildHost.dll"
    compile_command = [str(sdk["dotnet"]), str(sdk["compiler"]), "-nologo", "-debug:portable", "-target:exe", "-out:" + str(binary)]
    compile_command += ["-r:" + str(p) for p in sorted(sdk["references"].glob("*.dll"))]
    compile_command.append(str(source))
    result = run_logged(compile_command, logdir / "export-host-compile.log")
    if result:
        raise RuntimeError("Could not compile the optional UBT action-export host.")
    runtime = {"runtimeOptions": {"tfm": sdk["tfm"],
        "framework": {"name": "Microsoft.NETCore.App", "version": sdk["runtime"]},
        "configProperties": {"System.Globalization.Invariant": True}}}
    (work / "ProjectBuildHost.runtimeconfig.json").write_text(json.dumps(runtime, indent=2) + "\n")
    native_dir = engine / "Binaries/DotNET/UnrealBuildTool/runtimes" / sdk["native_rid"] / "native"
    for library in native_dir.glob("*.dylib"):
        destination = work / library.name
        if not destination.exists() or destination.stat().st_mtime_ns != library.stat().st_mtime_ns:
            shutil.copy2(library, destination)
    return sdk["dotnet"], binary


def response_files(action: dict) -> list[Path]:
    cwd = Path(action["WorkingDirectory"])
    result = []
    for argument in shlex.split(action["CommandArguments"]):
        if argument.startswith("@"):
            path = Path(argument[1:])
            result.append(path if path.is_absolute() else cwd / path)
    return result


def action_fingerprint(action: dict, environment: dict) -> str:
    fields = {key: action.get(key) for key in ("Type", "CommandPath", "CommandArguments", "CommandVersion", "WorkingDirectory", "ProducedItems")}
    fields["Environment"] = environment
    fields["Responses"] = [(str(p), p.read_text()) for p in response_files(action)]
    return hashlib.sha256(json.dumps(fields, sort_keys=True).encode()).hexdigest()


def dependency_paths(action: dict) -> list[Path] | None:
    """Read clang's make dependency rule, including escaped spaces and continuations."""
    dependency = action.get("DependencyListFile")
    if not dependency:
        dependency = next((p for p in action.get("ProducedItems", []) if p.endswith(".d")), None)
    if not dependency or not Path(dependency).is_file():
        return None
    content = Path(dependency).read_text().replace("\\\r\n", "").replace("\\\n", "")
    # Ignore optional -MP phony rules after the first dependency rule.
    first_rule = content.splitlines()[0] if content.splitlines() else ""
    separator = re.search(r"(?<!\\):(?:\s|$)", first_rule)
    if not separator:
        return None
    try:
        names = shlex.split(first_rule[separator.end():], comments=False, posix=True)
    except ValueError:
        return None
    cwd = Path(action["WorkingDirectory"])
    names = [name.replace("$$", "$") for name in names]
    return [(Path(name) if Path(name).is_absolute() else cwd / name) for name in names] or None


def can_resume(action: dict, actions: dict, state: dict, fingerprint: str, rebuilt: set[int]) -> bool:
    # Only compilation has a complete compiler-generated dependency list. Linking,
    # metadata and post-build operations always run so external libraries and
    # unlisted packaging/metadata inputs cannot silently become stale.
    if action["Type"] != "Compile" or set(action.get("PrerequisiteActions", [])) & rebuilt:
        return False
    record = state.get(fingerprint)
    if not record or not record.get("success"):
        return False
    outputs = [Path(p) for p in action.get("ProducedItems", [])]
    dependencies = dependency_paths(action)
    if not outputs or not dependencies:
        return False
    dependencies += response_files(action)
    dependencies.append(Path(action["CommandPath"]))
    for prerequisite in action.get("PrerequisiteActions", []):
        dependencies += [Path(p) for p in actions[prerequisite].get("ProducedItems", [])]
    try:
        oldest_output = min(path.stat().st_mtime_ns for path in outputs)
        newest_input = max(path.stat().st_mtime_ns for path in dependencies)
        # Also reject output replacement after the previously successful action.
        same_outputs = all(record["outputs"].get(str(path)) == path.stat().st_mtime_ns for path in outputs)
        return same_outputs and oldest_output >= newest_input
    except (FileNotFoundError, KeyError, OSError):
        return False


def execute_actions(action_file: Path, project_root: Path, logdir: Path, jobs: int, resume: bool) -> int:
    data = json.loads(action_file.read_text())
    actions = {action["Id"]: action for action in data["Actions"]}
    if len(actions) != len(data["Actions"]):
        raise RuntimeError("The exported action graph contains duplicate IDs.")
    for action in actions.values():
        for output in action.get("ProducedItems", []) + action.get("DeleteItems", []):
            checked_project_path(Path(output), project_root)
        if not set(action.get("PrerequisiteActions", [])).issubset(actions):
            raise RuntimeError(f"Action {action['Id']} references unknown prerequisites.")
    shutil.copy2(action_file, logdir / "Actions.json")
    environment = dict(os.environ)
    environment.update(data.get("Environment", {}))
    compiler_environment = dict(data.get("Environment", {}))
    for name in ("PATH", "CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "OBJC_INCLUDE_PATH",
                 "LIBRARY_PATH", "SDKROOT", "DEVELOPER_DIR", "MACOSX_DEPLOYMENT_TARGET", "DYLD_LIBRARY_PATH"):
        if name in environment:
            compiler_environment[name] = environment[name]
    fingerprints = {i: action_fingerprint(action, compiler_environment) for i, action in actions.items()}
    state_file = project_root / "Intermediate/BuildSupport/action-state.json"
    try:
        state = json.loads(state_file.read_text()) if state_file.exists() else {}
    except (ValueError, OSError):
        state = {}
    pending, completed, failed, rebuilt, skipped = set(actions), set(), set(), set(), set()
    running = {}

    def run(action):
        for output in action.get("ProducedItems", []):
            checked_project_path(Path(output), project_root).parent.mkdir(parents=True, exist_ok=True)
        for stale in action.get("DeleteItems", []):
            checked_project_path(Path(stale), project_root).unlink(missing_ok=True)
        command = [action["CommandPath"]] + shlex.split(action["CommandArguments"])
        if action["Type"] == "WriteMetadata":
            command.append("-Log=" + str(logdir / f"metadata-{action['Id']}.log"))
        label = re.sub(r"[^\w.-]", "_", action["StatusDescription"])
        log = logdir / f"{action['Id']:03d}_{label}.log"
        with log.open("w") as output:
            result = subprocess.run(command, cwd=action["WorkingDirectory"], env=environment, stdout=output, stderr=subprocess.STDOUT)
        return result.returncode, log

    def persist_state():
        state_file.parent.mkdir(parents=True, exist_ok=True)
        temp = state_file.with_suffix(".tmp")
        temp.write_text(json.dumps(state, indent=2, sort_keys=True) + "\n")
        temp.replace(state_file)

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        while pending or running:
            progressed = False
            for identifier in sorted(pending.copy()):
                action = actions[identifier]
                prerequisites = set(action.get("PrerequisiteActions", []))
                if prerequisites & failed:
                    failed.add(identifier)
                    pending.remove(identifier)
                    progressed = True
                    print(f"BLOCKED {identifier} {action['StatusDescription']}", flush=True)
                elif prerequisites <= completed and len(running) < jobs:
                    pending.remove(identifier)
                    progressed = True
                    if resume and can_resume(action, actions, state, fingerprints[identifier], rebuilt):
                        completed.add(identifier)
                        skipped.add(identifier)
                        print(f"UP-TO-DATE {identifier} {action['StatusDescription']}", flush=True)
                    else:
                        state.pop(fingerprints[identifier], None)
                        persist_state()
                        running[pool.submit(run, action)] = identifier
                        print(f"BUILD {identifier} {action['StatusDescription']}", flush=True)
            if not running:
                if pending:
                    if progressed:
                        continue
                    raise RuntimeError(f"Unresolved or cyclic action dependencies: {sorted(pending)}")
                break
            done, _ = wait(running, return_when=FIRST_COMPLETED)
            for future in done:
                identifier = running.pop(future)
                try:
                    returncode, log = future.result()
                except Exception as error:
                    failed.add(identifier)
                    print(f"FAIL {identifier}: {error}", flush=True)
                    continue
                if returncode:
                    failed.add(identifier)
                    print(f"FAIL {identifier} exit={returncode} log={log}", flush=True)
                    print(log.read_text(errors="replace")[-14000:], flush=True)
                else:
                    produced = [Path(p) for p in actions[identifier].get("ProducedItems", [])]
                    if not all(p.is_file() for p in produced):
                        failed.add(identifier)
                        print(f"FAIL {identifier}: command succeeded but declared outputs are missing", flush=True)
                        continue
                    completed.add(identifier)
                    rebuilt.add(identifier)
                    state[fingerprints[identifier]] = {"success": True, "outputs": {str(p): p.stat().st_mtime_ns for p in produced}}
                    persist_state()
                    print(f"PASS {identifier}", flush=True)
    summary = {"succeeded": len(completed), "rebuilt": len(rebuilt), "resumed": len(skipped), "failed_or_blocked": len(failed)}
    (logdir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("BUILD_RESULT " + json.dumps(summary), flush=True)
    return int(bool(failed))


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--project", help=".uproject or directory containing one; defaults to this project")
    parser.add_argument("--engine", help="Unreal installation root or its Engine directory")
    parser.add_argument("--target", help="Defaults to <project name>Editor")
    parser.add_argument("--configuration", default="Development", choices=("Debug", "DebugGame", "Development", "Test", "Shipping"))
    parser.add_argument("--architecture", default="arm64+x64", help="Mac target architecture (default: arm64+x64)")
    parser.add_argument("--executor", choices=("standard", "local-actions"), default="standard")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--resume", action="store_true", help="Reuse successful local compile actions only when fingerprints/dependencies match")
    parser.add_argument("--export-only", action="store_true", help="Export headers/actions without executing native actions (local-actions only)")
    parser.add_argument("--actions-only", action="store_true", help="Execute the existing Actions.json without rerunning header generation")
    parser.add_argument("--dry-run", action="store_true", help="Print the plan without writing files or starting build processes")
    parser.add_argument("ubt_args", nargs=argparse.REMAINDER, help="Additional UBT arguments after --")
    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    if platform.system() != "Darwin":
        raise RuntimeError("This project helper currently supports macOS only.")
    if args.jobs < 1:
        raise RuntimeError("--jobs must be positive.")
    if args.export_only and args.actions_only:
        raise RuntimeError("--export-only and --actions-only are mutually exclusive.")
    if args.executor != "local-actions" and (args.resume or args.export_only or args.actions_only):
        raise RuntimeError("--resume, --export-only and --actions-only require --executor local-actions.")
    project = project_file(args.project)
    root = project.parent
    engine = engine_dir(args.engine, project)
    target = args.target or project.stem + "Editor"
    extra = args.ubt_args[1:] if args.ubt_args[:1] == ["--"] else args.ubt_args
    build_args = [target, "Mac", args.configuration, "-Project=" + str(project),
                  "-architecture=" + args.architecture, "-NoHotReloadFromIDE", "-MaxParallelActions=" + str(args.jobs)]
    actions = root / "Intermediate/Build/Actions.json"
    if args.dry_run:
        if args.executor == "standard":
            print(shlex.join(["/bin/bash", str(engine / "Build/BatchFiles/Mac/Build.sh")] + build_args + extra))
        else:
            sdk = discover_dotnet(engine)
            print(f"Engine: {engine}\nProject: {project}\nBundled .NET: {sdk['dotnet']}\nTarget: {target} Mac {args.configuration} {args.architecture}")
            print(f"{'Use existing' if args.actions_only else 'Generate headers and export'} {actions}")
            print(f"{'Export only' if args.export_only else 'Execute native action DAG'}; jobs={args.jobs}; resume={args.resume}")
        return 0
    work = root / "Intermediate/BuildSupport"
    work.mkdir(parents=True, exist_ok=True)
    with (work / "build.lock").open("a+") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise RuntimeError("Another build_project.py process already owns this project.")
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        logdir = root / "Saved/Logs/Builds" / stamp
        logdir.mkdir(parents=True)
        if args.executor == "standard":
            command = ["/bin/bash", str(engine / "Build/BatchFiles/Mac/Build.sh")] + build_args
            return run_logged(command + ["-Log=" + str(logdir / "UnrealBuildTool.log")] + extra, logdir / "standard-build.log")
        if not args.actions_only:
            dotnet, host = prepare_export_host(engine, root, logdir)
            (root / "Intermediate/Build").mkdir(parents=True, exist_ok=True)
            command = [str(dotnet), str(host), str(engine / "Binaries/DotNET/UnrealBuildTool"), str(root / "Saved/BuildCache")]
            command += build_args + ["-rootdirectory=" + str(engine.parent),
                "-XmlConfigCache=" + str(root / "Intermediate/Build/XmlConfigCache.bin"),
                "-NoUBTMakefiles", "-NoUBA", "-Log=" + str(logdir / "UnrealBuildTool.log")] + extra
            result = run_logged(command, logdir / "export-actions.log")
            if result:
                return result
        if not actions.is_file():
            raise RuntimeError(f"No action graph was exported: {actions}")
        if args.export_only:
            shutil.copy2(actions, logdir / "Actions.json")
            return 0
        return execute_actions(actions, root, logdir, args.jobs, args.resume)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (RuntimeError, OSError, ValueError) as error:
        print(f"BUILD ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
