#!/bin/bash
# OWNER: sm-campus
# Headless campus-life smoke: `-game -nullrhi` (no GPU, no window, no audio device). The -BRCampusSmoke harness
# (UBullyCampusSubsystem::TickSmoke) runs one school morning through the real schedule with the real cast: 8:43 on the
# front lawn -> students head to class (8:45 dockers) -> the 9:00 bell -> late for class (truancy, trouble at the
# chase line) -> the grounds clear -> a prefect spots, chases and busts Jimmy -> sent to class (clock to the end of the
# period, the class-end bell) -> students stream back out -> hitting a student raises trouble and the student reacts
# -> ambient barks + a two-student conversation -> the 1_02B heat contract (held until hidden, drains, reaches 0).
# Each check logs `BR_CAMPUS_SMOKE <check> PASS|FAIL: ...`, then `BR_CAMPUS SMOKE PASS|FAIL (n/m checks)`; the engine
# quits itself. Needs the campus import (python3 -m tools.importers campus) for the trigger points, voices and bell.
#
#   tools/ue/run_campus_smoke.sh [--out DIR] [--seconds-limit 900]
#
# Same engine-safety envelope as the other headless runners: GPU-slot wrapper (label bully-campus), window watchdog
# that SIGTERMs the engine if any window appears (an empty window_watch.txt is the no-window evidence), no retry,
# SIGTERM -> 60 s -> SIGKILL. Exit code 0 only on PASS.
set -uo pipefail
source "$(dirname "$0")/_common.sh"

OUT=""
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-campus
LIMIT=900
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --seconds-limit) LIMIT="$2"; shift 2 ;;
    *) br_die "unknown argument $1" ;;
  esac
done
[ -f "$BR_REPO/unreal/BullyRemake/Config/Campus.json" ] || br_die "missing Config/Campus.json"
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
[ -n "$OUT" ] || OUT="$HOME/bully-remake-evidence/missions/smoke_campus"

br_preflight
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
rm -f "$OUT/ue_run.log" "$OUT/sequence.log" "$OUT/window_watch.txt"
echo "campus smoke -> $OUT"
START=$(date +%s)
"$GPU_SLOT" capture --label "$LABEL" -- \
  "$UE_CMD" "$BR_UPROJECT" "$MAP" -game -nullrhi -unattended -nosplash -nocrashreports -NoSound -notraceserver -NoVSync \
  -stdout -FullStdOutLogOutput -forcelogflush -abslog="$OUT/ue_run.log" -ResX=640 -ResY=360 -windowed \
  -BRCampusSmoke ${BR_EXTRA_ARGS:-} \
  > "$OUT/stdout.log" 2>&1 &
WRAP=$!
br_window_watch "$WRAP" "$OUT/window_watch.txt" &
WATCH=$!
stop_engine() {
  local epid
  epid=$(pgrep -P "$WRAP" | head -1)
  [ -n "$epid" ] || epid=$WRAP
  kill -TERM "$epid" 2>/dev/null
  for _ in $(seq 60); do kill -0 "$epid" 2>/dev/null || break; sleep 1; done
  kill -0 "$epid" 2>/dev/null && kill -KILL "$epid" 2>/dev/null
}
trap 'stop_engine; exit 130' INT TERM
( sleep "$LIMIT"; echo "time limit $LIMIT s reached: stopping the engine" >&2; stop_engine ) &
TIMER=$!
wait "$WRAP"
RC=$?
pkill -P "$TIMER" 2>/dev/null; kill "$TIMER" 2>/dev/null
wait "$WATCH" 2>/dev/null
br_reap_crash_reporters
echo "engine exit $RC after $(( $(date +%s) - START ))s"
sed -E 's/^\[[^]]*\]\[[^]]*\]//' "$OUT/ue_run.log" | grep -E 'BR_CAMPUS|BR_CLOCK' > "$OUT/sequence.log" || true
grep -E 'BR_CAMPUS_SMOKE|BR_CAMPUS SMOKE|BR_CAMPUS (cast|bell|BUSTED|truancy|class|prefect|conversation)' "$OUT/sequence.log" | tail -${BR_TAIL:-60}
if [ -s "$OUT/window_watch.txt" ]; then echo "WARNING: engine windows were seen on screen:"; cat "$OUT/window_watch.txt"; else echo "window watch: no on-screen window owned by the engine"; fi
grep -q "BR_CAMPUS SMOKE PASS" "$OUT/sequence.log" && exit 0
exit 1
