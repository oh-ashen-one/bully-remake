#!/bin/bash
# OWNER: campaign-spine
# Headless campaign-spine smoke: `-game -nullrhi` (no GPU, no window, no audio device), fixed 60 Hz game time. The
# -BRCampaignSmoke harness (UMissionStarSubsystem) boots the day_one chain through the mission smoke driver (no quit
# on done), then runs the design doc's verification checks (docs/campaign-spine.md): the post-boot star set, starting
# 1_02B by walking into its star and the 1_02B -> 1_02C chain, clock windows (23:00 despawn / paused stability /
# 08:00 return), and the pass-side effects of force-completing 1_09 and 1_B. Each check logs BR_CAMPAIGN <name>
# PASS/FAIL plus a BR_CAMPAIGN SMOKE summary, and the engine quits itself like the other harnesses.
#
#   tools/ue/run_campaign_smoke.sh [--out DIR] [--seconds-limit 900]
#
# Same engine-safety envelope as the other headless runners: GPU-slot wrapper (label bully-campaign), window
# watchdog that SIGTERMs the engine if any window appears (an empty window_watch.txt is the no-window evidence),
# no retry, SIGTERM -> 60 s -> SIGKILL. Exit code is the engine's (the harness quits the engine itself on PASS/FAIL).
set -uo pipefail
source "$(dirname "$0")/_common.sh"

OUT=""
MAP=/Game/Imported/Maps/Bullworth
LABEL=bully-campaign
LIMIT=1800
while [ $# -gt 0 ]; do
  case "$1" in
    --out) OUT="$2"; shift 2 ;;
    --seconds-limit) LIMIT="$2"; shift 2 ;;
    -*) br_die "unknown argument $1" ;;
    *) br_die "unknown argument $1" ;;
  esac
done
[ -f "$BR_REPO/unreal/BullyRemake/Config/Campaign.json" ] || br_die "missing Config/Campaign.json"
[ -f "$BR_REPO/unreal/BullyRemake/Content/Imported/Maps/Bullworth.umap" ] || br_die "level missing: run tools/ue/run_import.sh first"
[ -n "$OUT" ] || OUT="$HOME/bully-remake-evidence/missions/smoke_campaign"

br_preflight
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
rm -f "$OUT/ue_run.log" "$OUT/sequence.log" "$OUT/window_watch.txt"
echo "campaign smoke -> $OUT"
START=$(date +%s)
"$GPU_SLOT" capture --label "$LABEL" -- \
  "$UE_CMD" "$BR_UPROJECT" "$MAP" -game -nullrhi -unattended -nosplash -nocrashreports -NoSound -notraceserver -NoVSync \
  -stdout -FullStdOutLogOutput -forcelogflush -abslog="$OUT/ue_run.log" -ResX=640 -ResY=360 -windowed -BRNoAmbient \
  -BRCampaignSmoke ${BR_EXTRA_ARGS:-} \
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
sed -E 's/^\[[^]]*\]\[[^]]*\]//' "$OUT/ue_run.log" | grep -E 'BR_CAMPAIGN|BR_CLOCK|BR_STAR|BR_MISSION_SMOKE' > "$OUT/sequence.log" || true
grep -E 'BR_CAMPAIGN|BR_MISSION_SMOKE' "$OUT/sequence.log" | tail -${BR_TAIL:-60}
if [ -s "$OUT/window_watch.txt" ]; then echo "WARNING: engine windows were seen on screen:"; cat "$OUT/window_watch.txt"; else echo "window watch: no on-screen window owned by the engine"; fi
exit $RC
