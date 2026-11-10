#!/bin/bash
# Unverified full-editor content-generation wrapper; use --dry-run to inspect.
set -euo pipefail
OB_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$OB_SCRIPT_DIR/BuildSupport/workflow_common.sh"

usage() {
    cat <<'HELP'
Usage: generate_content.sh [--engine PATH] [--project FILE] [--validate | --repair] [--dry-run]

UNVERIFIED workflow wrapper: syntax and command construction have been checked,
but this wrapper has not been used for an end-to-end content generation run.
Starts the full macOS Unreal Editor and runs Scripts/bootstrap_content.py.
The editor remains open while asynchronous navigation validation completes.
Inspect the editor log for ORBIT_BOOTSTRAP_COMPLETE before assuming success.

  --engine PATH  Unreal installation root or its Engine directory; otherwise
                 UE_ENGINE_ROOT, then installed engine matching the .uproject.
  --project FILE Existing .uproject; defaults to this OrbitBreakout project.
  --validate     Validate existing mission without regenerating it.
  --repair       Explicitly back up and replace generator-owned mission actors.
                 Untagged actors are preserved by the generator.
  --dry-run      Print the quoted command without starting Unreal or writing files.
  --help         Show this help.

Build the native editor module before generation. Existing missions are preserved
unless --repair is explicitly selected. Logs: Saved/Logs/GenerateContent.log.
HELP
}

OB_ENGINE_ARG=''
OB_PROJECT_ARG="$OB_SCRIPT_DIR/../OrbitBreakout.uproject"
OB_DRY_RUN=0
OB_GENERATE_MODE=''
while [ "$#" -gt 0 ]; do
    case "$1" in
        --engine) ob_require_value "$@"; OB_ENGINE_ARG="$2"; shift 2 ;;
        --project) ob_require_value "$@"; OB_PROJECT_ARG="$2"; shift 2 ;;
        --validate|--repair)
            [ -z "$OB_GENERATE_MODE" ] || ob_fail 'choose only one of --validate or --repair'
            OB_GENERATE_MODE="$1"; shift ;;
        --dry-run) OB_DRY_RUN=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) ob_fail "unknown option: $1 (use --help)" ;;
    esac
done
ob_resolve_paths "$OB_ENGINE_ARG" "$OB_PROJECT_ARG"
[ -x "$OB_EDITOR" ] || ob_fail "Unreal Editor executable not found: $OB_EDITOR"
OB_GENERATOR="$OB_PROJECT_DIR/Scripts/bootstrap_content.py"
[ -f "$OB_GENERATOR" ] || ob_fail "content generator not found: $OB_GENERATOR"
OB_PYTHON_COMMAND="$OB_GENERATOR"
if [ -n "$OB_GENERATE_MODE" ]; then OB_PYTHON_COMMAND="$OB_PYTHON_COMMAND $OB_GENERATE_MODE"; fi
if [ "$OB_DRY_RUN" -eq 0 ]; then mkdir -p "$OB_PROJECT_DIR/Saved/Logs"; fi
ob_run_command "$OB_EDITOR" "$OB_PROJECT" "-ExecutePythonScript=$OB_PYTHON_COMMAND" \
    -log "-abslog=$OB_PROJECT_DIR/Saved/Logs/GenerateContent.log"
