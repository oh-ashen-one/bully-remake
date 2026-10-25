#!/bin/bash
# OWNER: sm-playloop
# Rendered story-mode evidence (offscreen: `-game -RenderOffScreen`, Slate makes no native window): PNGs WITH the UI of
# (00) the title screen offering Continue, (01) the free-roam gold star + guidance HUD ("Go to the yellow star: ...",
# waypoint diamond) from 18 m, (02) the star from 5 m. It continues from the save written by run_story_smoke.sh (slot
# BullyStorySmoke), so run `tools/ue/run_story_smoke.sh --only new` first. GPU run (shared slot); keep it short.
#
#   tools/ue/run_story_shots.sh [--out DIR] [--res 1920x1080] [--seconds-limit 900]
#
# Same engine-safety envelope as the other runners: GPU-slot wrapper (label bully-story-shots), window watchdog that
# SIGTERMs the engine if any window appears (empty window_watch.txt = no-window evidence), no retry,
# SIGTERM -> 60 s -> SIGKILL.
set -uo pipefail
source "$(dirname "$0")/_common.sh"

OUT=""
RES=1920x1080
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-story-shots
LIMIT=900
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --res) RES="$2"; shift 2 ;;
    --seconds-limit) LIMIT="$2"; shift 2 ;;
    *) br_die "unknown argument $1" ;;
  esac
done
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
[ -f "$BR_REPO/unreal/BullyRemake/Saved/SaveGames/BullyStorySmoke.sav" ] || br_die "no smoke save: run tools/ue/run_story_smoke.sh --only new first"
[ -n "$OUT" ] || OUT="$HOME/bully-remake-evidence/story-mode/smoke_story/shots"
RX=${RES%x*}; RY=${RES#*x}

br_preflight
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
rm -f "$OUT/ue_run.log" "$OUT/window_watch.txt" "$OUT"/[0-9][0-9]_*.png
echo "story shots -> $OUT at ${RX}x${RY}"
START=$(date +%s)
"$GPU_SLOT" capture --label "$LABEL" -- \
  "$UE_CMD" "$BR_UPROJECT" "$MAP" -game -RenderOffScreen -ResX="$RX" -ResY="$RY" -windowed \
  -unattended -nosplash -nocrashreports -NoSound -notraceserver -NoVSync -stdout -FullStdOutLogOutput -forcelogflush \
  -abslog="$OUT/ue_run.log" -BRNoAmbient -BRStorySmoke=shots -BRShotDir="$OUT" ${BR_EXTRA_ARGS:-} \
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
( sleep "$LIMIT" & SP=$!; trap 'kill $SP 2>/dev/null; exit 0' TERM; wait $SP
  echo "time limit $LIMIT s reached: stopping the engine" >&2; stop_engine ) &
TIMER=$!
wait "$WRAP"
RC=$?
kill "$TIMER" 2>/dev/null
wait "$WATCH" 2>/dev/null
br_reap_crash_reporters
echo "engine exit $RC after $(( $(date +%s) - START ))s"
sed -E 's/^\[[^]]*\]\[[^]]*\]//' "$OUT/ue_run.log" | grep -E 'BR_STORY|BR_STAR' | tail -30
for f in "$OUT"/[0-9][0-9]_*.png; do [ -f "$f" ] && echo "$(basename "$f") $(sips -g pixelWidth -g pixelHeight "$f" 2>/dev/null | awk '/pixel/{printf "%s ", $2}')"; done
if [ -s "$OUT/window_watch.txt" ]; then echo "WARNING: engine windows were seen on screen:"; cat "$OUT/window_watch.txt"; else echo "window watch: no on-screen window owned by the engine"; fi
exit $RC
