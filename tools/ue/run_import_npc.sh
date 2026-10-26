#!/bin/bash
# OWNER: slice-npc (scripted NPCs)
# Imports the peds + gesture clips the scripted NPCs need (Config/NPCScripts/*.json) into /Game/Imported/Characters/<Ped>/.
# Per ped: `python3 -m tools.importers ped <Ped> --anims <locomotion + every gesture clip>` (needs BULLY_INSTALL or
# tools/importers/local_config.json) writes imported/models/<Ped>.gltf, then run_import_character.sh imports the mesh
# (idempotent: unchanged hashes are skipped) and the missing AnimSequences. Gestures are C_Player clips, so a ped that is
# already imported only gains Anims/<CLIP> assets. Same engine-safety envelope as the other importers (GPU-slot wrapper,
# -nullrhi, no retries).
#
#   tools/ue/run_import_npc.sh [--peds GN_Boy01,GN_Boy02] [--dry-run]
#   tools/ue/run_import_npc.sh --cast [--skip-existing]   # the Chapter 1 story cast: every import=true row of
#                                                         # Config/PedModels.json (tools/ue/ped_cast.py); --skip-existing
#                                                         # leaves peds whose SK_<Ped> is already imported alone
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
PEDS_ARG=""
DRY=0
CAST=0
SKIP_EXISTING=0
while [ $# -gt 0 ]; do
  case "$1" in
    --peds) PEDS_ARG="$2"; shift 2 ;;
    --dry-run) DRY=1; shift ;;
    --cast) CAST=1; shift ;;
    --skip-existing) SKIP_EXISTING=1; shift ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
  esac
done
PLAN="$(python3 "$HERE/npc_plan.py")" || exit 2
PEDS="$(echo "$PLAN" | sed -n 's/^PEDS //p')"
CLIPS="$(echo "$PLAN" | sed -n 's/^CLIPS //p')"
[ -n "$PEDS_ARG" ] && PEDS="${PEDS_ARG//,/ }"
if [ "$CAST" = 1 ]; then
  PEDS="$(python3 "$HERE/ped_cast.py" | sed -n 's/^PEDS //p')" || exit 2
fi
if [ "$SKIP_EXISTING" = 1 ]; then
  KEEP=""
  for M in $PEDS; do
    [ -f "$REPO/unreal/BullyRemake/Content/Imported/Characters/$M/SK_$M.uasset" ] && echo "skip $M (already imported)" || KEEP="$KEEP $M"
  done
  PEDS="${KEEP# }"
fi
echo "peds: $PEDS"
echo "clips: $CLIPS"
[ "$DRY" = 1 ] && exit 0
cd "$REPO" || exit 2
RC=0
OK_PEDS=""
for M in $PEDS; do
  echo "== $M: glTF with locomotion + gesture clips"
  # shellcheck disable=SC2086
  python3 -m tools.importers ped "$M" --anims $CLIPS --root-motion inplace > "$REPO/imported/ped_$M.json" || { echo "ped export failed for $M" >&2; RC=3; continue; }
  OK_PEDS="$OK_PEDS,$M"
done
OK_PEDS="${OK_PEDS#,}"
[ -n "$OK_PEDS" ] || exit $RC
# one engine run for all of them (each ped still lands in its own /Game/Imported/Characters/<Ped>)
echo "== UE import (one engine run): $OK_PEDS"
BR_GPU_LABEL=bully-npc "$HERE/run_import_character.sh" --batch "$OK_PEDS" --anims all || { echo "UE import failed (see BR_CHAR_IMPORT_MODEL lines)" >&2; RC=3; }
exit $RC
