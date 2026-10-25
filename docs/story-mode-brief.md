# Bully remake — Story Mode push (shared brief, 2026-10-24)

## Goal (owner's words, paraphrased)
Hari wants to launch the Bully remake and **play story mode naturally — at minimum the first ~20 minutes of
Chapter 1** (arrival → day_one → 1_02B → 1_02C → 1_03 → 1_04), like the original: chapters, missions, cutscenes.
Chapters 1–2 must be genuinely playable by a human; Kimi K3's other work (Ch3 missions, Ch4–6 recon) is integrated
as-is so nothing is wasted. The coordinator (main Opus session) merges your branch into `story-mode`, then `main`.

## Where things are
- Repo: PUBLIC `oh-ashen-one/bully-remake`. Base = `origin/kimi-backend` (44b8930), Kimi K3's campaign work.
- Read first: `HANDOFF.md` (the `kimi-backend` section), `docs/campaign-spine.md`, `docs/story-mode-catalog.md`,
  the mission schema comment block in `unreal/BullyRemake/Source/BullyRemake/MissionSubsystem.h`, and the per-mission
  recon notes `docs/re-notes/ch1/*.md` (what the ORIGINAL does, beat by beat).
- Missions: `unreal/BullyRemake/Config/Missions/*.json`; campaign table `Config/Campaign.json`; boot chain
  `arrival` → `day_one` (BootMission in `Config/DefaultGame.ini`, currently empty).
- Imported game content (git-ignored, cloned per worktree): `imported/`, `unreal/BullyRemake/Content/Imported/`.
  Original install: `$HOME/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/Bully Scholarship Edition`
  (env `BULLY_INSTALL`). Read-only; never modify it.
- Evidence root (not committed): `~/bully-remake-evidence/story-mode/<your-branch>/`.

## Ownership
You own ONLY your worktree `~/bully-remake-wt/<branch>` and branch `<branch>`. Sibling agents work in parallel on
`sm-playloop`, `sm-humandrive`, `sm-visual`, `sm-cutscenes` and will touch some of the same files
(MissionSubsystem.cpp/.h, mission JSONs). Keep diffs focused: no reformatting, no renames, no drive-by refactors,
append new code in clearly named sections. Never touch `~/bully-remake` (Kimi's checkout), other worktrees,
`main`, or `kimi-backend`. Never force-push, rebase shared branches, or `git stash` (shared stash stack).
Commit often (end each message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`) and
`git push -u origin <branch>` before you report.

## Build & test
- Build (≈30 s incremental):
  `"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" BullyRemakeEditor Mac Development -project="$PWD/unreal/BullyRemake/BullyRemake.uproject"`
  Gotcha (HANDOFF): an incremental build can leave `Binaries/Mac/UnrealEditor.modules` pointing at a stale dylib —
  confirm with `nm` that your new symbols are in the dylib the manifest names; if not, delete the numbered dylibs +
  the manifest and rebuild.
- Headless mission smoke (no GPU, no window): `tools/ue/run_mission_smoke.sh <id> --out ~/bully-remake-evidence/story-mode/<branch>/smoke_<id>`
  Campaign smoke: `tools/ue/run_campaign_smoke.sh`. Rendered stills: `tools/ue/run_mission_shots.sh` (offscreen, GPU).
  Always use these wrappers (or wrappers written in the same style) — they go through the shared GPU slot.
- Regression bar before you report: everything you touched re-PASSes, plus `day_one`, `arrival`, `1_02B`,
  `1_03` smokes and the campaign smoke.

## Engine / machine safety (mandatory, owner rules)
- Every Unreal launch goes through `~/.cache/gpu-slot/bin/gpu_slot.sh` (the tools/ue wrappers do this). 2 capture
  slots shared by FOUR agents — runs queue; keep runs short, never hold a slot idle, never run more than one engine
  yourself at a time. Prefer `-nullrhi` headless; rendered runs only when you need pixels.
- No visible windows, ever (the wrappers' window watchdog enforces it). Never fullscreen. No editor GUI, no Unreal
  MCP server, no Insights/TraceServer or any network-listening tool (firewall dialogs hang unattended runs).
- Stop an engine: kill its driver script first, then SIGTERM, wait 60 s, SIGKILL only as a last resort. Never launch
  while an Unreal process is in `E`/`Z` state. After 2 crashes of the same thing, stop and report — no relaunch loops.
  Never touch engines you didn't start (other sessions run other Unreal projects on this Mac).
- Hari does the final hands-on feel playtest; you verify headlessly / offscreen. Don't open the game for him.

## Public-repo content rule
The repo contains original code only. NEVER commit game assets, decoded audio/anim/cutscene data, raw decompiler
output, or the original game's dialogue/subtitle text. Anything derived from the install is produced by an importer
at build time into `imported/` / `Content/Imported/` (git-ignored). Code must degrade gracefully when that imported
content is absent (fall back to today's behavior). Keep the UE project loadable in a clean clone.

## Unreal routing
Owner rule: for Unreal work read the router `~/.config/devin/brain/sources/oh-ashen-one--agents-md/ba35669bd13c6d1d514b3c1b77d1ed4dfbc7ca53/.devin/brain/library/unreal-engine/SKILL.md`.
This task is source edits + headless builds; do NOT start an editor or its MCP server.

## Report back (≤300 words)
What you changed (files), what you verified (exact commands + PASS lines + evidence paths), what is still
broken or stubbed, the pushed branch + head SHA. Be blunt about anything unverified.
