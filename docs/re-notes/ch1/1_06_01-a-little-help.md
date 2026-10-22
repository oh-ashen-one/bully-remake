# 1_06_01 recon: A Little Help (meet the hobo: radio fetch -> fighting-move tutorial)

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED** (read in the script), **INFERRED**, **OPEN**. No dialogue, narrative text, cutscene script or objective string text is reproduced; script/file/native names and string *keys* are citations only.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_06_01.lua` (1058 lines, read in full), `SMissPass.lua`, `main.lua`. World data lives in the install's `1_06.DAT` (`DATLoad`ed by this script); coordinates not read except the few hardcoded ones noted below.

## 1. Identification

**This is "A Little Help"** (canonical Chapter 1 list, available 8-23) — **not** "Save Algie" as the pre-existing mapping claimed; our notes had 1_05 and 1_06_01 swapped. Evidence:

- The script spawns Gary (model 130) and Pete (model 134) as allies who lead the player to a hobo ped (model 87) living behind the school (`_BUSDOORS`, a closed gate, a "fighting area"). The hobo asks for a transistor radio (item 476 — the script's own variable is `totalRadios`), and then teaches fighting moves through a button-sequence sparring tutorial.
- The script is a **multi-mission template**: `F_SetStage(nCurrentMission)` is called from outside, and `F_SetupActions` defines move tables for stages 1,2,3,4,7,8,9,10,11,12,13,14 — this one file backs the whole hobo-lesson chain (`1_06_01` .. presumably `1_06_05`). Stage 1 (what `1_06_01` runs) teaches exactly one move, string keys `1_06_MOVE01` / `1_06_MOVEUNLK01`.
- `SMissPass.lua` on pass: loads `Halloween1` special entities if `1_05` is done (and `1_11_Dummy` is not), and makes collectible type 1 available. Combined with `main.lua` re-asserting `CollectiblesSetTypeAvailable(1, true)` whenever `1_06_01` is completed and the mission's own item-476 radio counting, **collectible type 1 = transistor radios** (**INFERRED**, strong).

Confidence: **VERIFIED** by content. "Save Algie" is 1_05 (see `1_05-save-algie.md`).

## 2. Mission flow

Top level: `MissionSetup` (no fade-in, button-history input routed to action trees, `_BUSDOORS` made ped-pathable) -> `main` waits for `bStageLoaded` (external `F_SetStage`) -> mission-1-only intro cutscene -> per-stage function pointer `gMissionFunction` is swapped through the flow -> `MissionCleanup`.

| # | Beat | Logic (functional) |
|---|---|---|
| H0 | **Intro cutscene 1-06** (mission 1 only) | Baked `PlayCutsceneWithLoad("1-06", true)`. `1_06.DAT` loaded; anim groups + ped models 134/130/87 + weapon model 379 loaded. Player/Gary/Pete placed at `POINTLIST._BEGGINING` elements 3/1/2 (area 0); Gary and Pete recruited as allies, both get one shared health bar HUD widget (`1_06_GARY`/`1_06_PETE` labels), both made mission-critical with KO callbacks, infinite sprint. The player's carried radio count is snapshotted. |
| H1 | **Follow Gary and Pete** | Objective `1_06_OBJ01`; blip on `_1_06BEGIN` plus a blip on each ally. A speech thread plays five alternating Gary/Pete lines en route. The hobo-area gate (a persistent closed-gate entity `1_06_GateClosed` at hardcoded `(165.967, 18.8144, 7.31457)`, owned via `shared.hoboGateIndex`) is deleted so the area can be entered. |
| H2 | **Arrive: swap to hobo scene** | Player entering `TRIGGER._FIGHTINGAREA`: control off, fade, Gary and Pete deleted (criticality removed first), their blips removed, objective 1 completed, models 130/134 released, radio collectibles enabled. Baked cutscene `1-06B` plays. Music switches to `MS_Searching*` streams. Then: hobo spawned at `_1_06BEGIN` (model 87, mission-critical with "spares-the-player" flag 3rd arg true), player placed at `_AFTERCUTSCENE`, hobo put asleep/stationary, two social actions against him disabled, control restored. |
| H3 | **Fetch the radio** | Objective `1_06_OBJ02`; tutorial popup `TUT_TRANX01` (4.5 s) explaining transistors. A small blip-state machine: blip on `_STAIRCORONA` at ground level; if the player enters `TRIGGER._LOWCORONA` *while playing the ladder-climb anim state* the blip moves to `_TRANSMITTERBLIP` (high variant, style 29); if on `TRIGGER._HIGHGROUND` the blip points at the transmitter directly. So the radio sits up a ladder. Every 7 s the hobo yells encouragement if the player is within ~5 m. `_BUSDOORS` is held open. Condition to advance: `ItemGetCurrentNum(476) > 0`. |
| H4 | **Hand over the radio** | Objective `1_06_OBJ03`, blip retargeted to the hobo. When the player comes within ~2 m: control off, widescreen, both face each other, radio count decremented, the player plays a give-item anim (`/Global/1_06/1_06_Give/Give/GiveHobo_1_06`), hobo thanks line, objective completed, fade. Hobo deleted (position remembered) — **INFERRED** this fade hides the swap to the "trainer" instance. (For repeat missions — `nCurrentMission ~= 1` — the whole H0-H2 arc is skipped and the mission starts directly at this hand-over at `_1_06BEGIN` element 5.) |
| H5 | **Fight-tutorial setup** | Control off, widescreen, population zeroed, **game clock paused**, HUD components 0/4/5/11 hidden. Player teleported to `_1_06BEGIN` element 2, all peds cleared, hobo respawned with min-health 100 and 5000 health, player disarmed (weapon removed, HUD weapons locked, unequip loop), the closed-gate persistent entity recreated, pickups inside `_FIGHTINGAREA` removed. Camera: look-at `(157.27, 21.93, 7.03)` then path `PATH._FIGHTCUT`. Hobo gets a dedicated action tree `Act/Anim/Hobo_Blocker.act`, is leashed to `TRIGGER._RINGAREA` with tether-to-centre, attitude set hostile, and is ordered to attack. Player lock-on to the hobo. Punishment system disabled. |
| H6 | **Button-sequence move lesson** | For the stage's move table: the prompt widget (HUD component 21) shows the sequence (`ButtonHistoryAddSequence` / `...TimeInterval`), 10 s per attempt, pass/fail/correct-button callbacks; right/wrong 2D sounds (`RightBtn`). For moves with `condition == 1` the player must first enter the grapple-hold give state. Each **passed** sequence must then be *performed*: the loop waits until the player is playing the required action state, and counts a success; 3 successes (`gMaxSucceedCount`) advance to the next move with a widescreen beat and a new hobo instruction line. A failed sequence just re-prompts (with a hobo taunt line every 5 s). Stage 1's table: one move, sequence = a single press of button code 6 (the light-attack button, **INFERRED**), condition 2 = wait for return to `/Global/Player` default. |
| H7 | **Pass + end scene (in-engine)** | When the last move is done: hobo line 32, fade, `F_FirstMissionEndCutscene`: hobo deleted and respawned, lock-on released, control off, gate entity removed, player moved to `_1_06BEGIN` element 5, fixed camera at `(154.24, 22.28, 8.48)` -> `(157.37, 21.67, 7.47)`, the move-unlock string key is printed (`1_06_MOVEUNLK01`), end music, hobo farewell line, player auto-walks `PATH._ENDCUTSCENEPATH` for 7 s, fade, `MissionSucceed(true, false, false)`. `MissionCleanup` teleports the player to `POINTLIST._ENDING` if they never moved, restores HUD/clock/weapons/camera, and re-creates the closed-gate entity for the world. |
| H8 | **(stage 8 only) gag ending** | The final lesson (stage 8, `gLastStage = 8`) ends with a scripted UFO-abduction joke: preloaded stream `1-06_HoboNIS.rsm`, hobo at `_ENDHOBONIS`, camera at `(171.96, 15.02, 7.10)`, a `RaceBeam` effect at `_BEAMEFFECT`, gravity off, hobo floats upward 0.1/frame for 5 s, fade, hobo deleted, gate restored. Not needed for mission 1; listed for completeness. |

**Fail conditions** (`MissionFail(true, true, key)` — first arg **INFERRED** as "allow retry"):

1. `1_06_GARYKO` / `1_06_GARYHIT` — Gary KO'd (polled dead-check in main loop + mission-critical callback).
2. `1_06_PETEKO` / `1_06_PETEHIT` — same for Pete.
3. `1_06_HOBOKO` — hobo dies (health watcher thread `T_MonitorHoboHealth`; effectively unreachable in the lesson because his min-health is 100, so it's a guard against stray damage before/after). On this fail the hobo and player play a failure anim pair and a hobo insult line plays before the fail screen.

**Retry/restart:** baked cutscenes use `PlayCutsceneWithLoad(..., true)` — skippable on retry (**INFERRED**, standard flag). Crucially, `MissionCleanup` for mission 1 when *not* completed **zeroes item 476 and re-hides/uncollects collectible type 1** — a failed attempt takes the fetched radio away so the fetch beat replays.

**Chains to:** nothing explicit. Later hobo lessons reuse this script with a different `nCurrentMission` via `F_SetStage` (called from outside this file — the dispatch wrapper is **OPEN**, presumably `1_06_02`..`1_06_05` stubs).

### Rewards / unlock side-effects

- **In-mission:** the taught fighting move; unlock banner key `1_06_MOVEUNLK01` is printed, but **how the move is actually persisted to the player is not in the Lua** (no unlock call — presumably the action tree/flag side; **OPEN**).
- **`SMissPass.lua` (`1_06_01` branch), VERIFIED:** if `1_05` is complete and `1_11_Dummy` is not: `AreaLoadSpecialEntities("Halloween1", true)` (Halloween prep chain; `main.lua` separately loads `Halloween1` once 1_05+1_07+1_08 are all done). Plus `CollectiblesSetTypeAvailable(1, true)` — **transistor radio collectibles (item 476) become available in the world** (**INFERRED** that type 1 = radios, from the mission's own item-476 counting and `main.lua`'s re-assertion on load).
- No weapon/item grant, no outfit, no chapter set, no force-completed missions for this one.

## 3. Cutscene points

| Marker | Where in the flow | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | Before H1 (`1-06`) | baked | `fade` in at `_BEGGINING`, `letterbox` + Gary/Pete `say` exchange, skip on retry |
| CS-2 | H2 (`1-06B`) | baked | `fade` + `letterbox` + hobo `say` lines at the bus; can merge into the H2 fade |
| CS-3 | H4 radio hand-over | in-engine | `letterbox`, `camera` two-shot, give-anim + `say`, `fade` — fully reproducible once paired anims exist |
| CS-4 | H5 arena reveal (`PATH._FIGHTCUT`) | in-engine | `camera` path playback (new) or a fixed shot + `shake`; then control on |
| CS-5 | H7 end scene | in-engine | fixed `camera` + `banner` (move-unlocked text) + `say` + player auto-walk (path follow) + `fade` |
| CS-6 | H8 UFO gag (stage 8 only) | in-engine | out of scope for 1_06_01; stub with `fade` if we ever port later lessons |

## 4. NPCs and assets needed

### Ped models (raw ids as scripted)

| Model id | Role |
|---|---|
| 130 | Gary (escort ally, mission-critical) — matches `Nemesis_Gary` in the opening doc |
| 134 | Pete (escort ally, mission-critical) — model name **OPEN** (not in opening doc) |
| 87 | The hobo (quest giver / sparring partner) — model name **OPEN** |

### Animation groups (`LoadAnimationGroup`)

`DO_Striker`, `DO_StrikeCombo`, `Boxing`, `NPC_AggroTaunt`, `Hobo_Cheer`, `NPC_Adult`, plus `MINI_React` for mission 1 only. Action trees: `Act/Conv/1_06.act` (give anim, failure pair, hobo idle cycle, hobo fly), `Act/Anim/Hobo_Blocker.act` (trainer AI tree — internals undecoded), `Act/Player.act`.

### Props / weapons / items

- Item **476**: transistor radio (fetch quest item + world collectible type 1). **VERIFIED** by script variable naming.
- Weapon model **379**: loaded, never visibly used in-file — **OPEN** (possibly the hobo's prop weapon inside his action tree).
- Persistent entity `1_06_GateClosed` at `(165.967, 18.8144, 7.31457)` — closed gate blocking the hobo yard; deleted/created across mission boundaries via `shared.hoboGateIndex`.
- `TRIGGER._BUSDOORS` — bus door prop, opened/closed and made ped-pathable.
- `EffectCreate("RaceBeam", ...)` — only in the stage-8 gag ending.
- Sounds: interactive streams `MS_SearchingLow/Mid/High.rsm`; UI `RightBtn` (+ a wrong-button sound via the failed callback path); preloaded `1-06_HoboNIS.rsm` (stage 8).

### Speech usage

~26 `SoundPlayScriptedSpeechEvent` call sites on event `M_1_06_01` (ids seen: 1,2,3,4,5,12,13,14,17,18,19,22,23,24,25,28,30,31,32,33,34,128) plus two one-shot events `THANKS_JIMMY` and `SEE_SOMETHING_CRAP`. Count only; no content.

### Named world data to extract later (from `1_06.DAT`)

- POINTLISTs: `_BEGGINING` (elements 1=Gary, 2=Pete, 3=player), `_1_06BEGIN` (1=hobo, 2=player fight pos, 5=player end pos), `_AFTERCUTSCENE`, `_STAIRCORONA`, `_TRANSMITTERBLIP`, `_ENDING`, `_ENDHOBONIS`, `_BEAMEFFECT`.
- PATHs: `_FIGHTCUT` (camera path), `_ENDCUTSCENEPATH` (player walk).
- TRIGGERs: `_FIGHTINGAREA`, `_RINGAREA`, `_LOWCORONA`, `_HIGHGROUND`, `_BUSDOORS`, `_FINALPOSCHECK`.
- Hardcoded: gate position (above); cameras at H5/H7/H8 (above).
- **Area codes seen: 0 (outdoors) only.**

## 5. Framework gaps (JSON mission primitives)

Existing primitives: conditions `reach_location, trigger_enter/exit, interact, event, timer, flag, all/any`; actions `hint, banner, event, set_flag/clear_flag, log, teleport, start_mission, spawn_enemy, fade, reward, letterbox, say, camera, shake`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| H1 follow two allies with shared health bar | `spawn_enemy` (as spawn-ally variant), `banner` | **escort-follow / spawn_ally** (shared with 1_05), **defend-NPC** fail (KO-critical, per-NPC fail message) |
| H3 radio fetch | `reach_location` (partially) | **has-item condition** (count of item id), **contextual blip retarget** (blip moves by player state), **player-anim-state condition** (on-ladder), ladder traversal itself (world/movement feature) |
| H4 hand-over | `interact` (near-ped) | **give-item-to-NPC** (decrement item, paired give/receive anim, resume) |
| H5 arena setup | `teleport`, `fade`, `camera` | **disarm-player + weapon-HUD lock**, **pause-game-clock**, **HUD component show/hide by id**, **population-override**, **persistent world entity create/delete** (the gate), **camera-path playback**, **lock-on**, **combat-leash (restrict-to-trigger)** + tether (shared with 1_05), **min-health floor** per ped |
| H6 move lesson | — | **button-sequence prompt** (sequence + per-button timing windows, pass/fail/correct callbacks, timeout, retry) — the core new primitive; **count-successes-then-advance** (N=3) gating; hobo taunt on repeat failure = `timer`+`say` |
| H7 pass scene | `fade`, `camera`, `banner`, `say` | **player auto-walk along path**, **cutscene skip on attempt > 1** |
| Cleanup take-back | — | **on-fail item/collectible rollback** (mission-scoped) |
| Side effects | `reward` | **collectible-type-availability unlock**, **special-entities load** (Halloween1), **move-unlock grant** (mechanism TBD, see open questions) |

## 6. Open questions

1. **Move persistence:** nothing in the Lua grants the taught move to the player (only a banner key is printed). Where does the unlock live — an engine-side mapping from `1_06_MOVEUNLK01`, or the action tree? Decide our own mechanism (a `grant_move` reward action).
2. **Button code mapping:** codes 6,7,8,9,10..19 appear in sequences; their controller/keyboard mapping is engine-side and unknown. We only need code 6 for mission 1.
3. **`Act/Anim/Hobo_Blocker.act`** internals (attack cadence, block behavior) are undecoded — same class of gap as the 1_02 tutorial trees.
4. **How `F_SetStage` is called** for each hobo lesson (wrapper scripts / mission table not read). For our port, mission 1 only.
5. Whether stage numbers 5 and 6 (absent from `F_SetupActions`) were cut moves or live in another file.
6. The exact radius of `PlayerIsInAreaObject(gHobo, 2, 2, 0)` (the args look like area/size/flag; semantics **INFERRED** as "player within ~2 m of hobo").
7. Weapon 379's use, if any.
