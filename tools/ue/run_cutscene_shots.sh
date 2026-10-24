#!/bin/bash
# OWNER: sm-cutscenes
# Rendered pass of one ORIGINAL baked cutscene (imported/cutscenes/<id>/cutscene.json): `-game -RenderOffScreen` (Slate
# makes no native window), UBullyCutsceneSubsystem plays the cutscene once shaders are idle and grabs a HighResShot at
# each requested cutscene time (holding the cutscene clock there for a few frames so streaming/animation settle), then
# quits itself. Full tier when the actors are imported (tools/ue/run_import_cutscene_actors.sh), else audio tier over
# the default camera (--audio forces that).
#
#   tools/ue/run_cutscene_shots.sh <cutscene_id> [--times 3,10,25] [--out DIR] [--res 1280x720] [--seconds-limit 600] [--audio]
#
# Same engine-safety envelope as the other runners: GPU-slot wrapper (FIFO-waits if both slots are busy), window
# watchdog that SIGTERMs the engine if any window appears (empty window_watch.txt = no-window evidence), -NoSound, no
# retry, SIGTERM -> 60 s -> SIGKILL. Output: <out>/<id>_NN_tSSS.S.png + the run log. Exit code is the engine's.
set -uo pipefail
source "$(dirname "$0")/_common.sh"

CID=""
OUT=""
RES=1280x720
TIMES=""
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-cutscene-shots
LIMIT=600
AUDIO=""
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --res) RES="$2"; shift 2 ;;
    --times) TIMES="$2"; shift 2 ;;
    --seconds-limit) LIMIT="$2"; shift 2 ;;
    --audio) AUDIO="-BRCutsceneAudioTier"; shift ;;
    -*) br_die "unknown argument $1" ;;
    *) [ -z "$CID" ] || br_die "only one cutscene id"; CID="$1"; shift ;;
  esac
done
[ -n "$CID" ] || br_die "usage: run_cutscene_shots.sh <cutscene_id> [--times a,b,c] [--out DIR] [--res WxH]"
[ -f "$BR_REPO/imported/cutscenes/$CID/cutscene.json" ] || br_die "imported/cutscenes/$CID/cutscene.json missing: python3 -m tools.importers cutscene build $CID"
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
[ -n "$OUT" ] || OUT="$HOME/bully-remake-evidence/cutscenes/shots_$CID"
RX=${RES%x*}; RY=${RES#*x}
TARG=""
[ -n "$TIMES" ] && TARG="-BRCutsceneShotTimes=$TIMES"

br_preflight
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
rm -f "$OUT/ue_run.log" "$OUT/window_watch.txt" "$OUT/${CID}"_[0-9][0-9]_*.png
echo "cutscene shots: $CID -> $OUT at ${RX}x${RY} times=${TIMES:-default}"
START=$(date +%s)
"$GPU_SLOT" capture --label "$LABEL" -- \
  "$UE_CMD" "$BR_UPROJECT" "$MAP" -game -RenderOffScreen -ResX="$RX" -ResY="$RY" -windowed \
  -unattended -nosplash -nocrashreports -NoSound -notraceserver -NoVSync -stdout -FullStdOutLogOutput -forcelogflush \
  -abslog="$OUT/ue_run.log" -BRNoAmbient -BRCutsceneShots="$CID" -BRShotDir="$OUT" -BRShotResX="$RX" -BRShotResY="$RY" \
  $TARG $AUDIO ${BR_EXTRA_ARGS:-} \
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
kill "$TIMER" 2>/dev/null; pkill -P "$TIMER" 2>/dev/null
wait "$WATCH" 2>/dev/null
br_reap_crash_reporters
echo "engine exit $RC after $(( $(date +%s) - START ))s"
grep -E 'BR_CUTSCENE' "$OUT/ue_run.log" | sed -E 's/^\[[^]]*\]\[[^]]*\]//' | tail -40
for f in "$OUT/${CID}"_[0-9][0-9]_*.png; do [ -f "$f" ] && echo "$(basename "$f") $(sips -g pixelWidth -g pixelHeight "$f" 2>/dev/null | awk '/pixel/{printf "%s ", $2}')"; done
if [ -s "$OUT/window_watch.txt" ]; then echo "WARNING: engine windows were seen on screen:"; cat "$OUT/window_watch.txt"; else echo "window watch: no on-screen window owned by the engine"; fi
exit $RC
