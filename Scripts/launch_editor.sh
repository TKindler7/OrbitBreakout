#!/bin/bash
# Unverified editor-launch wrapper; use --dry-run to inspect.
set -euo pipefail
OB_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$OB_SCRIPT_DIR/BuildSupport/workflow_common.sh"

usage() {
    cat <<'HELP'
Usage: launch_editor.sh [--engine PATH] [--project FILE] [--game] [--dry-run]

UNVERIFIED workflow wrapper: syntax and command construction have been checked,
but this wrapper has not been used to verify a running editor or playable game.

  --engine PATH  Unreal installation root or Engine directory; otherwise
                 UE_ENGINE_ROOT, then installed engine matching the .uproject.
  --project FILE Existing .uproject; defaults to this OrbitBreakout project.
  --game         Run the compiled editor executable in standalone game mode.
  --dry-run      Print the quoted command without launching or writing files.
  --help         Show this help.

Build the native module first and generate the mission before using --game.
This starts the actual UnrealEditor binary and keeps its output in this terminal.
Log: Saved/Logs/LaunchEditor.log (or LaunchGame.log with --game).
HELP
}

OB_ENGINE_ARG=''
OB_PROJECT_ARG="$OB_SCRIPT_DIR/../OrbitBreakout.uproject"
OB_DRY_RUN=0
OB_GAME=0
while [ "$#" -gt 0 ]; do
    case "$1" in
        --engine) ob_require_value "$@"; OB_ENGINE_ARG="$2"; shift 2 ;;
        --project) ob_require_value "$@"; OB_PROJECT_ARG="$2"; shift 2 ;;
        --game) OB_GAME=1; shift ;;
        --dry-run) OB_DRY_RUN=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) ob_fail "unknown option: $1 (use --help)" ;;
    esac
done
ob_resolve_paths "$OB_ENGINE_ARG" "$OB_PROJECT_ARG"
[ -x "$OB_EDITOR" ] || ob_fail "Unreal Editor executable not found: $OB_EDITOR"
OB_COMMAND=("$OB_EDITOR" "$OB_PROJECT")
OB_LOG_NAME='LaunchEditor.log'
if [ "$OB_GAME" -eq 1 ]; then OB_COMMAND+=(-game); OB_LOG_NAME='LaunchGame.log'; fi
OB_COMMAND+=(-log "-abslog=$OB_PROJECT_DIR/Saved/Logs/$OB_LOG_NAME")
if [ "$OB_DRY_RUN" -eq 0 ]; then mkdir -p "$OB_PROJECT_DIR/Saved/Logs"; fi
ob_run_command "${OB_COMMAND[@]}"
