#!/bin/bash
# Shared path discovery for the unverified macOS workflow wrappers.
# This file is sourced; it does not launch Unreal or change machine settings.

ob_fail() { printf 'Orbit Breakout: %s\n' "$*" >&2; exit 2; }

ob_require_value() {
    [ "$#" -ge 2 ] && [ -n "$2" ] || ob_fail "$1 requires a value"
}

ob_resolve_paths() {
    command -v python3 >/dev/null 2>&1 || ob_fail 'python3 is required for path and project discovery'
    OB_PROJECT="$(python3 - "$2" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1]).expanduser().resolve()
if p.suffix != '.uproject' or not p.is_file():
    raise SystemExit('Project must be an existing .uproject file: ' + str(p))
print(p)
PY
)" || exit 2
    OB_PROJECT_DIR="$(dirname "$OB_PROJECT")"
    OB_ENGINE="$(python3 - "$1" "$OB_PROJECT" <<'PY'
from pathlib import Path
import json, os, sys
requested = sys.argv[1] or os.environ.get('UE_ENGINE_ROOT', '')
project = Path(sys.argv[2])
if requested:
    root = Path(requested).expanduser().resolve()
    engine = root if root.name == 'Engine' else root / 'Engine'
    if not (engine / 'Build/BatchFiles/RunUAT.sh').is_file():
        raise SystemExit('Engine path does not contain Unreal Build/BatchFiles: ' + str(engine))
else:
    association = str(json.loads(project.read_text()).get('EngineAssociation', ''))
    candidates = []
    for root in Path('/Users/Shared/Epic Games').glob('UE_*'):
        engine = root / 'Engine'
        version_path = engine / 'Build/Build.version'
        if not version_path.is_file():
            continue
        try:
            version = json.loads(version_path.read_text())
            version_name = f"{version['MajorVersion']}.{version['MinorVersion']}"
        except (OSError, ValueError, KeyError):
            continue
        if association == version_name or association == root.name.removeprefix('UE_'):
            candidates.append(engine.resolve())
    if len(candidates) != 1:
        raise SystemExit(f'Expected one installed engine matching EngineAssociation={association!r}; '
                         'pass --engine or set UE_ENGINE_ROOT.')
    engine = candidates[0]
print(engine)
PY
)" || exit 2
    OB_EDITOR="$OB_ENGINE/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
    OB_UAT="$OB_ENGINE/Build/BatchFiles/RunUAT.sh"
}

ob_print_command() {
    printf '%q ' "$@"
    printf '\n'
}

ob_run_command() {
    ob_print_command "$@"
    if [ "$OB_DRY_RUN" -eq 1 ]; then return 0; fi
    exec "$@"
}
