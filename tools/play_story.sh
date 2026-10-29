#!/bin/bash
# OWNER: sm-playloop
# Player launcher for story mode: builds the editor target if the sources are newer than the built module, then
# launches the game WINDOWED (never fullscreen) on the Bullworth map with -BRStory (title screen: New Story / Continue)
# through the shared GPU slot wrapper. This is an ATTENDED session, so a window is expected -- do not use it from an
# unattended/agent run (those use the headless tools/ue/run_*_smoke.sh wrappers).
#
#   tools/play_story.sh [--dry-run] [--no-build] [--res 1920x1080] [--slot NAME] [-- extra engine args...]
#   "Play Bully Story Mode.command"      (double-clickable wrapper at the repo root)
#
#   --dry-run   print the build decision and the exact launch command, run nothing
#   --no-build  skip the build-if-needed step
#   --res WxH   window size (default 1920x1080; still windowed)
#   --slot NAME save slot (default BullyStory; passed as -BRStorySlot=NAME)
#
# GPU slot class: `capture` (one of the 2 shared slots; no exclusive GPU gate) -- `perf` is for frame-time measurement
# only and would kill the game after 15 min. The capture class kills its command after a max hold (default 40 min),
# which would end a play session; the launcher raises it to 4 h (override: BR_PLAY_MAX_HOLD=<seconds>, 0 = unlimited).
# Progress is autosaved after every mission pass, so a forced stop loses at most the current mission.
set -uo pipefail
source "$(dirname "$0")/ue/_common.sh"

RES=1920x1080
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-story-play
DRY=0
BUILD=1
SLOT=""
EXTRA=()
while [ $# -gt 0 ]; do
  case "$1" in
    --dry-run) DRY=1; shift ;;
    --no-build) BUILD=0; shift ;;
    --res) RES="$2"; shift 2 ;;
    --slot) SLOT="$2"; shift 2 ;;
    --) shift; EXTRA=("$@"); break ;;
    *) br_die "unknown argument $1 (see the header of tools/play_story.sh)" ;;
  esac
done
case "$RES" in [0-9]*x[0-9]*) ;; *) br_die "--res wants WxH, got $RES" ;; esac
RX=${RES%x*}; RY=${RES#*x}
UE_APP="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor.app"
EDITOR_BUILD="$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
MODULE="$BR_REPO/unreal/BullyRemake/Binaries/Mac/libUnrealEditor-BullyRemake.dylib"
MAX_HOLD="${BR_PLAY_MAX_HOLD:-14400}"

[ -d "$UE_APP" ] || br_die "UnrealEditor.app not found at $UE_APP"
[ -x "$GPU_SLOT" ] || br_die "launch wrapper not found at $GPU_SLOT (GPU_SLOT env or tools/ue/gpu_slot_fallback.sh)"
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "the Bullworth level is missing: import the game content first (python3 -m tools.importers world --preset all, then tools/ue/run_import.sh)"

# build-if-needed: any source/config/uproject file newer than the built module, or no module at all
NEED_BUILD=0
if [ ! -f "$MODULE" ]; then NEED_BUILD=1
elif [ -n "$(find "$BR_REPO/unreal/BullyRemake/Source" "$BR_REPO/unreal/BullyRemake/BullyRemake.uproject" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.cs' -o -name '*.uproject' \) -newer "$MODULE" -print -quit 2>/dev/null)" ]; then NEED_BUILD=1; fi

if [ "$BUILD" = 1 ] && [ "$NEED_BUILD" = 1 ]; then
  BUILD_CMD=("$EDITOR_BUILD" BullyRemakeEditor Mac Development -project="$BR_UPROJECT")
  if [ "$DRY" = 1 ]; then echo "[dry-run] build needed: ${BUILD_CMD[*]}"
  else
    echo "building BullyRemakeEditor (sources changed)..."
    "${BUILD_CMD[@]}" || br_die "build failed"
  fi
else
  echo "build: $([ "$NEED_BUILD" = 1 ] && echo 'needed but skipped (--no-build)' || echo 'up to date')"
fi

# Launch through `open` on the .app bundle (LaunchServices): this gives the game a proper
# macOS app environment. Invoking the raw Mach-O inside the bundle can die in early startup
# (observed 2026-10-29: UBT ValidatePlatforms 'MainVersion' SDK failure, exit 1).
LAUNCH=(/usr/bin/open -W -n "$UE_APP" --args "$BR_UPROJECT" "$MAP" -game -windowed -ResX="$RX" -ResY="$RY" -BRStory -BRTownLife -nosplash -nocrashreports -notraceserver)
[ -n "$SLOT" ] && LAUNCH+=("-BRStorySlot=$SLOT")
[ ${#EXTRA[@]} -gt 0 ] && LAUNCH+=("${EXTRA[@]}")
WRAPPED=(env "GPU_SLOT_CAPTURE_MAX_HOLD=$MAX_HOLD" "$GPU_SLOT" capture --label "$LABEL" -- "${LAUNCH[@]}")

if [ "$DRY" = 1 ]; then
  echo "[dry-run] launch (windowed ${RX}x${RY}, nothing started):"
  printf '  %q' "${WRAPPED[@]}"; echo
  exit 0
fi

br_preflight
echo "launching story mode (windowed ${RX}x${RY}); Esc = menu (Save & Quit), progress autosaves after every mission."
exec "${WRAPPED[@]}"
