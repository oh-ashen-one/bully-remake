#!/bin/bash
# OWNER: slice-weapons
# Rendered evidence for the rides and the arsenal: the town sandbox start in a real renderer (`-game -RenderOffScreen`,
# no native window) with UBullyShowcaseShots driving it: every ride mounted and driven (3/4 view + chase camera), every
# weapon held / at its release frame / just after its impact on a passive target. Waits for compilation to drain, then
# shoots and quits itself.
#
#   tools/ue/run_showcase_shots.sh [--out DIR] [--res 1920x1080] [--seconds-limit 1500]
#
# Same engine-safety envelope as run_mission_shots.sh: GPU-slot wrapper, window watchdog (empty window_watch.txt =
# no-window evidence), no retry, SIGTERM -> 60 s -> SIGKILL. Output: <out>/NN_<name>.png + the run log.
set -uo pipefail
source "$(dirname "$0")/_common.sh"

OUT=""
RES=1920x1080
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-showcase-shots
LIMIT=1500
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --res) RES="$2"; shift 2 ;;
    --seconds-limit) LIMIT="$2"; shift 2 ;;
    *) br_die "unknown argument $1" ;;
  esac
done
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
[ -n "$OUT" ] || OUT="$HOME/bully-remake-evidence/showcase"
RX=${RES%x*}; RY=${RES#*x}

br_preflight
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
rm -f "$OUT/ue_run.log" "$OUT/window_watch.txt" "$OUT"/[0-9][0-9]_*.png
echo "showcase shots -> $OUT at ${RX}x${RY}"
START=$(date +%s)
"$GPU_SLOT" capture --label "$LABEL" -- \
  "$UE_CMD" "$BR_UPROJECT" "$MAP" -game -RenderOffScreen -ResX="$RX" -ResY="$RY" -windowed \
  -unattended -nosplash -nocrashreports -NoSound -notraceserver -NoVSync -stdout -FullStdOutLogOutput -forcelogflush \
  -abslog="$OUT/ue_run.log" -BRNoAmbient -BRSandbox -BRShowcaseShots="$OUT" -BRShotResX="$RX" -BRShotResY="$RY" \
  ${BR_EXTRA_ARGS:-} \
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
grep -E 'BR_SHOWCASE|BR_VEHICLE|BR_WEAPON |BR_SFX|BR_ANIM clip missing' "$OUT/ue_run.log" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -40
for f in "$OUT"/[0-9][0-9]_*.png; do [ -f "$f" ] && echo "$(basename "$f") $(sips -g pixelWidth -g pixelHeight "$f" 2>/dev/null | awk '/pixel/{printf "%s ", $2}')"; done
if [ -s "$OUT/window_watch.txt" ]; then echo "WARNING: engine windows were seen on screen:"; cat "$OUT/window_watch.txt"; else echo "window watch: no on-screen window owned by the engine"; fi
exit $RC
