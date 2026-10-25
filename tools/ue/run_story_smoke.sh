#!/bin/bash
# OWNER: sm-playloop
# Headless story-mode smoke (two engine runs, one after the other, never concurrent): `-game -nullrhi` (no GPU, no
# window, no audio device), fixed-rate game time. The -BRStorySmoke harness (UBullyStorySubsystem) exercises the real
# player flow with simulated key presses:
#   run 1 (new):      title screen (no Continue) -> New Story -> the mission smoke driver plays arrival + day_one ->
#                     autosave written -> free roam: the 1_02B gold star is up and the HUD points at it -> state mutated
#                     (clock 11:15 paused, +$13, items, faction respect, collectible, flag, ammo) and written through the
#                     Save & Quit code path.
#   run 2 (continue): title offers Continue first -> restore -> chapter / completions / attempts / wallet / items / clock /
#                     ammo / respect / flags / respawn spot all equal the slot, 1_02B star back, free-roam guidance shown.
# Both runs log BR_STORY <check> PASS/FAIL + a BR_STORY SMOKE summary and quit the engine themselves. The smoke uses its own
# save slot (BullyStorySmoke) and deletes it at the start of run 1, so a player's save (slot BullyStory) is never touched.
#
#   tools/ue/run_story_smoke.sh [--out DIR] [--only new|continue] [--seconds-limit 900]
#
# Same engine-safety envelope as the other headless runners: GPU-slot wrapper (label bully-story), window watchdog that
# SIGTERMs the engine if any window appears (empty window_watch.txt = no-window evidence), no retry,
# SIGTERM -> 60 s -> SIGKILL. Exit code: 0 only if every run that was started PASSed.
set -uo pipefail
source "$(dirname "$0")/_common.sh"

OUT=""
ONLY=""
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-story
LIMIT=900
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --only) ONLY="$2"; shift 2 ;;
    --seconds-limit) LIMIT="$2"; shift 2 ;;
    *) br_die "unknown argument $1" ;;
  esac
done
[ -f "$BR_REPO/unreal/BullyRemake/Config/Campaign.json" ] || br_die "missing Config/Campaign.json"
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
[ -n "$OUT" ] || OUT="$HOME/bully-remake-evidence/story-mode/smoke_story"
case "$ONLY" in ""|new|continue) ;; *) br_die "--only takes new or continue" ;; esac

run_one() {   # run_one <new|continue>
  local MODE="$1" DIR="$OUT/$1"
  mkdir -p "$DIR"
  rm -f "$DIR/ue_run.log" "$DIR/sequence.log" "$DIR/window_watch.txt"
  br_preflight
  echo "story smoke ($MODE) -> $DIR"
  local START=$(date +%s)
  "$GPU_SLOT" capture --label "$LABEL" -- \
    "$UE_CMD" "$BR_UPROJECT" "$MAP" -game -nullrhi -unattended -nosplash -nocrashreports -NoSound -notraceserver -NoVSync \
    -stdout -FullStdOutLogOutput -forcelogflush -abslog="$DIR/ue_run.log" -ResX=640 -ResY=360 -windowed -BRNoAmbient \
    -BRStorySmoke="$MODE" ${BR_EXTRA_ARGS:-} \
    > "$DIR/stdout.log" 2>&1 &
  local WRAP=$!
  br_window_watch "$WRAP" "$DIR/window_watch.txt" &
  local WATCH=$!
  stop_engine() {
    local epid
    epid=$(pgrep -P "$WRAP" | head -1)
    [ -n "$epid" ] || epid=$WRAP
    kill -TERM "$epid" 2>/dev/null
    for _ in $(seq 60); do kill -0 "$epid" 2>/dev/null || break; sleep 1; done
    kill -0 "$epid" 2>/dev/null && kill -KILL "$epid" 2>/dev/null
  }
  trap 'stop_engine; exit 130' INT TERM
  # watchdog: TERM-able without orphaning its sleep (an orphan would hold the caller's pipe open for $LIMIT s)
  ( sleep "$LIMIT" & SP=$!; trap 'kill $SP 2>/dev/null; exit 0' TERM; wait $SP
    echo "time limit $LIMIT s reached: stopping the engine" >&2; stop_engine ) &
  local TIMER=$!
  wait "$WRAP"
  local RC=$?
  kill "$TIMER" 2>/dev/null
  wait "$WATCH" 2>/dev/null
  br_reap_crash_reporters
  echo "engine exit $RC after $(( $(date +%s) - START ))s"
  sed -E 's/^\[[^]]*\]\[[^]]*\]//' "$DIR/ue_run.log" | grep -E 'BR_STORY|BR_CAMPAIGN|BR_STAR|BR_MISSION_SMOKE|BR_HUD (banner|hint|objective)' > "$DIR/sequence.log" || true
  grep -E 'BR_STORY|BR_MISSION_SMOKE (PASS|FAIL)' "$DIR/sequence.log" | tail -${BR_TAIL:-40}
  if [ -s "$DIR/window_watch.txt" ]; then echo "WARNING: engine windows were seen on screen:"; cat "$DIR/window_watch.txt"; else echo "window watch: no on-screen window owned by the engine"; fi
  grep -q "BR_STORY SMOKE $MODE PASS" "$DIR/sequence.log" && return 0
  return 1
}

FAIL=0
if [ -z "$ONLY" ] || [ "$ONLY" = new ]; then run_one new || FAIL=1; fi
if [ "$FAIL" = 0 ] && { [ -z "$ONLY" ] || [ "$ONLY" = continue ]; }; then run_one continue || FAIL=1; fi
[ "$FAIL" = 0 ] && echo "STORY SMOKE: PASS" || echo "STORY SMOKE: FAIL"
exit $FAIL
