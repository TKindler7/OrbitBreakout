#!/bin/bash
# Unverified stock RunUAT packaging wrapper; use --dry-run to inspect.
set -euo pipefail
OB_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$OB_SCRIPT_DIR/BuildSupport/workflow_common.sh"

usage() {
    cat <<'HELP'
Usage: package_game.sh [--engine PATH] [--project FILE]
                       [--configuration Development|Shipping] [--output PATH] [--dry-run]

UNVERIFIED workflow wrapper: syntax and command construction have been checked;
no packaged application has been produced or validated through this wrapper.
Runs the installed engine's stock RunUAT.sh BuildCookRun for macOS.

  --engine PATH         Unreal installation root or Engine directory; otherwise
                        UE_ENGINE_ROOT, then engine matching the .uproject.
  --project FILE        Existing .uproject; defaults to this OrbitBreakout project.
  --configuration NAME Development (default) or Shipping.
  --output PATH         Archive destination; defaults to project/Builds/Mac.
  --dry-run             Print the quoted command without running UAT or writing files.
  --help                Show this help.

Generate and validate the mission first. This builds, cooks, stages and archives
the game; installing Xcode and compiling the editor alone does not verify packaging.
Stock UAT manages its own build and logging paths and needs normal write access.
HELP
}

OB_ENGINE_ARG=''
OB_PROJECT_ARG="$OB_SCRIPT_DIR/../OrbitBreakout.uproject"
OB_DRY_RUN=0
OB_CONFIGURATION='Development'
OB_OUTPUT=''
while [ "$#" -gt 0 ]; do
    case "$1" in
        --engine) ob_require_value "$@"; OB_ENGINE_ARG="$2"; shift 2 ;;
        --project) ob_require_value "$@"; OB_PROJECT_ARG="$2"; shift 2 ;;
        --configuration) ob_require_value "$@"; OB_CONFIGURATION="$2"; shift 2 ;;
        --output) ob_require_value "$@"; OB_OUTPUT="$2"; shift 2 ;;
        --dry-run) OB_DRY_RUN=1; shift ;;
        --help|-h) usage; exit 0 ;;
        *) ob_fail "unknown option: $1 (use --help)" ;;
    esac
done
case "$OB_CONFIGURATION" in Development|Shipping) ;; *) ob_fail 'configuration must be Development or Shipping' ;; esac
ob_resolve_paths "$OB_ENGINE_ARG" "$OB_PROJECT_ARG"
[ -f "$OB_UAT" ] || ob_fail "RunUAT.sh not found: $OB_UAT"
if [ -z "$OB_OUTPUT" ]; then OB_OUTPUT="$OB_PROJECT_DIR/Builds/Mac"; fi
OB_OUTPUT="$(python3 - "$OB_OUTPUT" <<'PY'
from pathlib import Path
import sys
print(Path(sys.argv[1]).expanduser().resolve())
PY
)"
ob_run_command /bin/bash "$OB_UAT" BuildCookRun "-project=$OB_PROJECT" -noP4 \
    -platform=Mac "-clientconfig=$OB_CONFIGURATION" -build -cook -stage -pak -archive \
    "-archivedirectory=$OB_OUTPUT" -utf8output
