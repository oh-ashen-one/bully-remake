#!/bin/bash
# OWNER: builder-B6 (proof)
# Re-place the door-link actors (door_links.json) into the saved map, without re-importing anything
# (M6: the M5 re-import ran while the editor module was unbuilt and saved a map with no doors).
#
#   tools/ue/run_place_doors.sh                          # hand-made tools/ue/door_links.json only (default)
#   tools/ue/run_place_doors.sh --generated              # hand-made + imported/door_links_game.json (ExitLocs.dat doors)
#   tools/ue/run_place_doors.sh --links A.json --links B.json   # exactly these files, merged in order, first id wins
#
# The generated file comes from `python3 -m tools.importers exits` (git-ignored, derived from the owner's install).

# pythonscript commandlet, -nullrhi (no GPU, no window), wrapped in the shared GPU slot like run_import.sh.
set -uo pipefail
source "$(dirname "$0")/_common.sh"
LINKS=()
GENERATED=0
while [ $# -gt 0 ]; do
  case "$1" in
    --links) [ $# -ge 2 ] || br_die "--links needs a file"; LINKS+=("$(cd "$(dirname "$2")" 2>/dev/null && pwd)/$(basename "$2")"); shift 2 ;;
    --generated) GENERATED=1; shift ;;
    *) br_die "unknown argument: $1 (use --links FILE ... or --generated)" ;;
  esac
done
if [ $GENERATED -eq 1 ]; then
  LINKS=("$BR_REPO/tools/ue/door_links.json" "$BR_REPO/imported/door_links_game.json" ${LINKS[@]+"${LINKS[@]}"})
fi
for f in ${LINKS[@]+"${LINKS[@]}"}; do [ -f "$f" ] || br_die "door links file missing: $f"; done
if [ ${#LINKS[@]} -gt 0 ]; then
  BR_DOOR_LINKS="$(IFS=:; echo "${LINKS[*]}")"
  export BR_DOOR_LINKS
  echo "place_doors: link files: $BR_DOOR_LINKS"
fi
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
br_preflight
OUT="$BR_REPO/unreal/BullyRemake/Saved/BullyImport"
mkdir -p "$OUT"
LOG="$OUT/place_doors_$(date +%Y%m%d-%H%M%S).log"

START=$(date +%s)
"$GPU_SLOT" capture --label bully-proof -- \
  "$UE_CMD" "$BR_UPROJECT" -run=pythonscript -script="$BR_REPO/tools/ue/place_doors.py" \
  -unattended -nosplash -nullrhi -nopause -nocrashreports -NoSound -notraceserver -stdout -FullStdOutLogOutput \
  > "$LOG" 2>&1
RC=$?
br_reap_crash_reporters
echo "place_doors: engine exit $RC after $(( $(date +%s) - START ))s; log $LOG"
grep -E 'BR_DOORS' "$LOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -15
if [ $RC -eq 0 ] && ! grep -q 'BR_DOORS_DONE OK' "$LOG"; then RC=3; fi
exit $RC
