#!/bin/bash
# OWNER: sm-cutscenes
# Headless import of the baked-cutscene actors (Full-tier cutscene playback) into /Game/Imported/Characters/<ACTOR>.
#
#   python3 -m tools.importers cutscene build            # manifests + audio (imported/cutscenes/<id>/)
#   python3 -m tools.importers cutscene actors           # actor glTFs (imported/models/CS_*.gltf) + cutscenes/actors.json
#   tools/ue/run_import_cutscene_actors.sh [--only CS_PLAY,CS_TE_SECRETARY] [--imported DIR]
#
# One UnrealEditor-Cmd pythonscript commandlet (-nullrhi -unattended -nosplash) inside the shared GPU slot (label
# bully-cutscene-import), no retries; each actor goes through tools/ue/import_character.py. Log + per-actor reports in
# unreal/BullyRemake/Saved/BullyImport/.
set -uo pipefail
source "$(dirname "$0")/_common.sh"

IMPORTED="$BR_REPO/imported"
ONLY=""
while [ $# -gt 0 ]; do
  case "$1" in
    --only) ONLY="$2"; shift 2 ;;
    --imported) IMPORTED="$2"; shift 2 ;;
    *) br_die "unknown argument $1" ;;
  esac
done
[ -f "$IMPORTED/cutscenes/actors.json" ] || br_die "$IMPORTED/cutscenes/actors.json missing: run python3 -m tools.importers cutscene actors first"

br_preflight
OUT="$BR_REPO/unreal/BullyRemake/Saved/BullyImport"
mkdir -p "$OUT"
STAMP=$(date +%Y%m%d-%H%M%S)
LOG="$OUT/cutscene_actors_$STAMP.log"
export BR_IMPORTED="$IMPORTED" BR_CS_ONLY="$ONLY" BR_CS_REPORT_DIR="$OUT/cutscene_actors_$STAMP"
echo "cutscene actor import -> /Game/Imported/Characters/CS_* only=${ONLY:-all} log=$LOG"
START=$(date +%s)
"$GPU_SLOT" capture --label "${BR_GPU_LABEL:-bully-cutscene-import}" -- \
  "$UE_CMD" "$BR_UPROJECT" -run=pythonscript -script="$BR_REPO/tools/ue/import_cutscene_actors.py" \
  -unattended -nosplash -nullrhi -nopause -nocrashreports -NoSound -notraceserver -stdout -FullStdOutLogOutput \
  > "$LOG" 2>&1
RC=$?
br_reap_crash_reporters
echo "engine exit $RC after $(( $(date +%s) - START ))s"
grep -E 'BR_CS_ACTORS' "$LOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -40
if [ $RC -eq 0 ] && ! grep -q 'BR_CS_ACTORS_DONE ok=[0-9]* failed=0' "$LOG"; then RC=3; fi
exit $RC
