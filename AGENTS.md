# AGENTS.md — Bully Remake (UE 5.8 port of Bully: Scholarship Edition)

Machine-oriented companion to `README.md`. Read this first if you are a coding agent
asked to set up, build, run, or recreate this project on someone's machine.

## What this is

A one-to-one fan port of *Bully: Scholarship Edition* to Unreal Engine 5.8, in the
OpenMW shape: **this repo contains only original code** (MIT). No game assets are or
may ever be committed — a CI blocklist enforces it. All content is converted locally
from the user's own purchased, installed PC copy at build time.

Current `main` state (as of `23977e5`, 2026-10-26): playable Chapter 1 story mode —
title screen (New Story / Continue), autosave, the first ~20 min of the chapter
(arrival → day_one → 1_02B/1_02C → Chemistry class → 1_03 → 1_04), original cutscenes
with recorded voice + subtitles, campus life (bell schedule, truancy, prefects,
factions), minigames (lockpick, slingshot, tree climb), the whole outdoor map with
townspeople and drivable parked cars, AI NPC voice chat, melee combat, missions for
all six chapters authored as data. A free-play sandbox mode (`-BRSandbox`) drops you
in town facing a vehicle lineup with the full weapon arsenal.

## Hard requirements (stop and report if missing — do not substitute)

1. **An installed PC copy of Bully: Scholarship Edition** on this machine. Locate the
   game root (must contain `Stream/`). Point the importers at it with `BULLY_INSTALL`,
   `--install`, or a git-ignored `tools/importers/local_config.json`:
   `{"install": "/abs/path/to/Bully Scholarship Edition"}`. Read-only; never copy,
   commit, upload, or redistribute game content; never bypass DRM.
2. **Unreal Engine 5.8** — set `UE_ROOT` if it is not at
   `/Users/Shared/Epic Games/UE_5.8`. Plugins: EnhancedInput, PythonScriptPlugin,
   ModelContextProtocol; Interchange glTF import.
3. **macOS + Xcode** — the `tools/ue/*.sh` wrappers and `tools/play_story.sh` are
   Mac-specific (Metal, `swiftc` window helper). Windows/Linux are unported.
4. **Python 3** (stdlib + NumPy; Pillow for previews; pytest for `tools/tests`;
   `faster-whisper` only if you want `--transcribe` subtitle matching).
5. Optional NPC-chat providers: an OpenAI-compatible LLM endpoint + Fish Audio key —
   see `unreal/BullyRemake/Config/NPCChat.json` (`api_key_file` paths live outside the
   repo). Missing keys degrade to silent subtitles, not errors. `whisper-cli` +
   `~/.cache/whisper/ggml-*.bin` enables push-to-talk voice input.

The wrappers used to hard-require a personal GPU-slot coordinator at
`~/.cache/gpu-slot/bin/gpu_slot.sh`. They no longer do: `_common.sh` now falls back to
`tools/ue/gpu_slot_fallback.sh` (a passthrough) when it is absent. `GPU_SLOT` still
overrides for machines that do run a coordinator.

## Full fresh-checkout recipe (dependency order)

```sh
export BULLY_INSTALL="/abs/path/to/Bully Scholarship Edition"
export UE_ROOT="/Users/Shared/Epic Games/UE_5.8"   # if different

# 1. convert content locally (CPU; output -> imported/, git-ignored)
python3 -m tools.importers world --preset all
python3 -m tools.importers ped PLAYER --anims '*' --root-motion inplace --head B_Buzz
python3 -m tools.importers audio extract FeetAll FeetSCHL
python3 -m tools.importers audio extract Ambs --names 'NewSchoolOutdoor_F|Birds01'
python3 -m tools.importers audio footsteps
python3 -m tools.importers townlife && python3 -m tools.importers exits
python3 -m tools.importers vehicle bike SK8Board GoCart taxicab Policecar Limo Truck
python3 -m tools.importers prop slingshot spudg BROCKETLAUNCHER BROCKET EGGPROJ stinkbomb marble W_Itch
for m in GN_Boy01 GN_Boy02 GN_Asiangirl GN_Greekboy NDH3_Bucky PFH1_Seth; do
  python3 -m tools.importers ped "$m" --anims IDLE PLAYERWALK PLAYERRUN RUNFAST \
    IDLEJUMP JOGJUMP_L JOGJUMP_R FALL FALL_LAND PLAY_LAND_HURT --root-motion inplace
done
python3 -m tools.importers cutscene build --chapter1
python3 -m tools.importers cutscene actors --chapter1
python3 -m tools.importers speech --transcribe            # optional: needs faster-whisper
python3 -m tools.importers speech --extra-only --transcribe
python3 -m tools.importers campus --transcribe
python3 -m tools.importers minigame
python3 -m tools.importers validate

# 2. build the editor module (CPU; BEFORE the level import or door actors get skipped)
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" \
  BullyRemakeEditor Mac Development \
  -project="$PWD/unreal/BullyRemake/BullyRemake.uproject"

# 3. headless Unreal imports (each goes through the slot wrapper; -nullrhi, no window)
tools/ue/run_import.sh --scenes all
tools/ue/run_import.sh --models bike,SK8Board,GoCart,taxicab,Policecar,Limo,Truck,\
wheel_van,wheel_cargreen,Police_wheel,slingshot,spudg,BROCKETLAUNCHER,BROCKET,EGGPROJ,\
stinkbomb,marble,W_Itch --sfx --townlife
tools/ue/run_import_character.sh
tools/ue/run_import_ambient.sh
tools/ue/run_import_npc.sh --cast
tools/ue/run_import_npc.sh --peds PRH1_Gord,PRH1a_Tad,PRGirl_Pinky,GRH1_Lefty,GRH1a_Vance,\
GRGirl_Lola,JKH1a_Kirby,JKH2_Dan,JKGirl_Mandy,NDH2_Thad,NDGirl_Beatrice,PF2nd_Max,GN_SexyGirl,GN_WhiteBoy
tools/ue/run_import_cutscene_actors.sh
tools/ue/run_lighting.sh
```

## Launch

- Story mode (title screen): `tools/play_story.sh` (or double-click
  `Play Bully Story Mode.command`). Windowed 1920x1080; Esc = menu/Save & Quit;
  progress autosaves per mission.
- Sandbox showcase: same binary + `-BRSandbox -BRTownLife` instead of `-BRStory`
  (town spawn facing bike/skateboard/go-kart/car; all weapons; wheel/D-pad cycles).
- In-game: WASD/mouse, Shift sprint, Space jump, E interact, T talk to NPC,
  hold Tab voice input, G teleport menu (50 spots incl. `town`), H horn.

## Verification wrappers (headless, no window)

`tools/ue/run_mission_smoke.sh <id>`, `run_story_smoke.sh`, `run_campus_smoke.sh`,
`run_campaign_smoke.sh`, `run_mission_shots.sh`, `run_story_shots.sh` — all write
logs/reports under `unreal/BullyRemake/Saved/`. Imports log to `Saved/BullyImport/`.

## Repo map

- `tools/importers/` — Python converters: world scenes/placements/textures, `ped`
  skinned characters + HXD/AGR clips, `vehicle`/`prop`, `audio` (XACT/.SEA),
  `cutscene`, `speech`, `campus`, `minigame`, `townlife`, `trigger`, `exits`.
- `tools/ue/` — bash wrappers around UnrealEditor-Cmd commandlets (`_common.sh` shared).
- `unreal/BullyRemake/` — the UE 5.8 project; C++ in `Source/BullyRemake/`
  (missions, combat, campus, town life, sandbox, NPC chat, minigames); data-driven
  content in `Config/*.json`.
- `docs/` — architecture, CREDITS, re-notes (format reverse-engineering), story-mode docs.
- `HANDOFF.md` — rolling developer log (verify current state, don't trust stale lines).

## House rules for agents

- Work on a task branch; do not push to `main` without the repo owner's say-so.
- Never commit `imported/`, `Content/Imported/`, `Saved/`, `Binaries/`, or anything
  decoded from the game — `.gitignore` + CI blocklist back this up.
- The documented source install is the Steam PC edition via CrossOver on macOS;
  other install paths are untested — flag, don't guess.
- Claims must match evidence: build success ≠ headless pass ≠ rendered check ≠
  hands-on play. Report which one you actually did.
