# 2_04 — Race the Vale: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_04.lua` (749 lines, complete) + `Library/BikeRace_util.lua` (imported, not re-read in full) + `SMissPass.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_04 = "Race the Vale"** (canonical Chapter 2 list, 8-23) — **VERIFIED** by content: `ImportScript("Library/BikeRace_util.lua")`, a one-lap bike race (`PATH._2_04_RACE`) against three prep racers (models 30/31/32) through the rich area (barrier set `RL_rich1`, gate trigger `_AMB_RICH_AREA`, shortcuts `_2_04_RICHSHORTCUT01..05`), Petey (134) riding along, and a race owner/starter (model 86 — the bike-shop owner, **INFERRED** from the `OWNER` point name and his role) running the countdown and the podium beat. Anim groups (`NIS_2_04`, `3_R08RaceLeague`, `Cheer_*`) corroborate; none embeds the title, so the id rests on content — unambiguous. No surprise.

Setting is entirely outdoors; every scripted area reference is **0** (`visibleArea = 0` in the race params, no area transitions).

## 2. Mission flow

Driver: `MissionSetup()` → `main()` as a **linear blocking sequence** (no stage-function loop): spawn entities → (first attempt only) ride-to-the-beach segment → race setup → camera-path intro → `RaceControl()` blocks until win/lose → win NIS or fail. Threads: `T_Petey`, `T_GetBackOnBikeCheck`, `T_ChangeBeachPositions`, `T_Countdown`.

**Retry checkpoint (unusual):** `PlayerGetScriptSavedData(23)` is a **mid-mission checkpoint flag**, not an attempt counter. If set: the intro cutscene "2-04" is skipped *and* the whole ride-to-the-beach segment is skipped — the player, Peter and both bikes spawn directly at `_2_04_RESTARTPLAYER`. The flag is written (`PlayerSetScriptSavedData(23, 1)`) only once the race setup is reached. A retry therefore restarts at the race, not the mission start — the first mission in our recon with a real mid-mission resume point.

| # | Beat | Logic (functional) |
|---|---|---|
| R0 | **Baked cutscene "2-04"** | First run only. Garage deactivated, POI disabled, no auto fade-in. |
| R1 | **Spawn cast** (`F_CreateMissionEntities` + `F_CreateBeachPeds`) | First run: player at `_2_04_PLAYER`; Peter (134) + his bike (273) at `_2_04_PETEYSPAWN`; player bike at `_2_04_PLAYERBIKE`. Retry: everything at `_2_04_RESTARTPLAYER` elements 2/3/4. Beach cast: owner (86) at `_2_04_OWNER`, three preps (30/32/31) at `_2_04_PREP1/2/3` with three bikes; owner, all three racers and Peter are **mission-critical** with distinct fail callbacks (`2_04_FAILPETE` / `2_04_FAILOWNER` / `2_04_FAILRACER`). |
| R2 | **Get on the bike** (`F_GetBike`) | Objective (`2_04_INSTRUC01`) held until `PedIsOnVehicle(gPlayer)`. Watchdog thread `T_GetBackOnBikeCheck`: if the player dismounts during R2-R3, text nags alternate and a blip jumps onto the player's (last) bike; remounting re-points the blip at the destination. |
| R3 | **Ride to the beach** (`F_GetToBeach` + `T_Petey`) | Objective (`2_04_INSTRUC02`). Peter is put on his bike, non-targetable, focused on the player, follows `PATH._2_04_PETEPATH` with a health bar and blip; 5 s in he stops at `_2_04_CROWDCHEER` element 3 and waits for the race flag. Completion needs both: player inside `_AMB_RICH_AREA`, then within 10 m of `_2_04_PETER` while on a vehicle. On completion: whole beach cast made invulnerable, fade out, Peter's follow flag cleared (his thread deletes him — he is re-created as a cheerer). |
| R4 | **Race setup** (`F_RaceSetup` + `F_InitTables` + `F_BarrierInit`) | Checkpoint flag 23 written here. Beach peds/racers cleared and rebuilt by the race utility. Race params: 1 lap on `PATH._2_04_RACE`, player start `_2_04_PLAYERRICH` (area 0), 3 racers with rubber-band AI (catch-up: 30 m / 1.3× speed; slow-down: 15 m / 0.35×; aggressiveness 0.5; no shortcuts/tricks/shooting), 5 shortcut sub-paths declared (node ranges 7-11, 38-44, 46-54, 58-63, 63-70) for the player to discover, 32 highlighted course nodes, soundtrack `MS_BikeRace01.rsm`. `AreaLoadSpecialEntities("RL_rich1", true)` raises the race barrier dressing; ambient vehicles zeroed; population overridden. Beach cheerers created: Peter (134) + models 25 + 67 cheering at `_2_04_CROWDCHEER` (stationary, ignore attacks/stimuli). `T_ChangeBeachPositions` re-seats the crowd at `_2_04_BEACHCHEERCHANGE` (8 slots incl. the owner) once the player passes `_2_04_CHANGEBEACH` mid-race. |
| R5 | **In-engine race intro** (`F_NISRace`) | Widescreen, control off, vehicles near player deleted; camera flies `PATH._2_04_INTROPATH` looking along `_2_04_INTROLOOK` (speed 5); three trash-talk barks (generic event `TRASH_TALK_TEAM`, one per racer) spaced ~1.2 s apart; sound/music fade with the camera; interactive music stopped. Fade back to the player. |
| R6 | **Countdown + race** (`RaceControl`) | Countdown ped (owner) plays the `Race/Countdown` node; player flagged mission-critical — but `cbCritPlayer` only sets `shared.gPlayerIncapacitated` and clears text (no fail; it feeds the race utility, **INFERRED**). `RaceControl()` (BikeRace_util) runs HUD, positions and finish logic and returns win + optional fail-reason string. HUD hidden afterwards. |
| R7 | **Win NIS** (`F_EndNIS`) | Owner and player invulnerable, timer stopped, racers deleted. Fade, widescreen, control off, player dismounted and placed at `_2_04_NIS_END` (element 2), owner at element 1; mutual face + target-lock; FOV 40 camera (329.24, 277.05, 7.52 → 329.70, 276.16, 7.47); owner line (`M_2_04` 19); FOV 80 second camera (324.56, 269.23, 7.75 → 325.43, 269.67, 7.96); second line (21). Pass banner `M_PASS` with value **1500** (INFERRED: money, $15.00 if cents — same value used by 2_05), respect message `MRESPECT_PM10`, pass music. Owner then walks `_2_04_NIS_END_WALK` and is deleted; camera/FOV reset. `SetFactionRespect(5, respect - 10)` (preps, **INFERRED**), `MissionSucceed(true, false, false)`. No chained start — **OPEN** (world mission table). |
| R8 | **Lose** | Fail music; if `RaceControl` gave a reason string it is used (`MissionFail(false, true, reason)`), else a generic you-lose banner (`GKART_YOULOSE`) then `MissionFail(false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Peter dies | mission-critical callback | `2_04_FAILPETE` |
| Race owner dies | mission-critical callback | `2_04_FAILOWNER` |
| Any racer dies | mission-critical callback | `2_04_FAILRACER` |
| Race lost / race-specific | `RaceControl` return | reason string or `GKART_YOULOSE` banner |

Retry resumes at the race (checkpoint flag 23, see above) — `F_CreateMissionEntities` respawns at `_2_04_RESTARTPLAYER`. Cleanup releases `RL_rich1`, reverts population/vehicles/POI, makes the whole cast ambient, and teleports the owner to a hardcoded spot (337.08, 274.95, 7.31) — **INFERRED** his day-job position at the bike shop.

### Pass effects

**No 2_04 branch in `SMissPass.lua`** — no clothing/weapon/class unlocks, no force-completes, no chapter set (**VERIFIED** by absence). In-script only: prep respect −10, pass banner value 1500.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | R0 intro | baked (`2-04`), first run only | `fade` + `letterbox` + one `say` beat; or skip straight to R2 |
| CS-2 | R5 race intro | in-engine (camera paths `_2_04_INTROPATH`/`_2_04_INTROLOOK` + 3 barks) | **camera path playback** (already a named gap from ch1 `chapt1trans`) + `say` ×3 + `fade`; path data is in `2_04.dat` (reference only) |
| CS-3 | R6 countdown | in-engine (one action node) | trivial: owner plays countdown node, then control on |
| CS-4 | R7 podium | in-engine, 2 fixed cameras + 2 lines | two fixed `camera` setups + `say` ×2 + `completion_banner` + `fade` |
| CS-5 | crowd move (R4) | teleport dressing | `npc_move_to`-equivalent re-seat on `trigger_enter`; no camera |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 134 | Peter | ride-along, then cheerer; mission-critical (`2_04_FAILPETE`) |
| 86 | race owner / countdown ped / podium host | mission-critical (`2_04_FAILOWNER`); created at `_2_04_OWNER` |
| 30, 32, 31 | prep racers 1-3 | mission-critical (`2_04_FAILRACER`); rubber-band race AI |
| 25, 67 | beach cheerers | stationary cheer loop |
| 38, 142, 9, 68, 139 | loaded in `tblLoadModels` but never explicitly spawned here | presumably used inside BikeRace_util crowd/racer logic — **OPEN** |
| 273 | bicycle (×6: player, Peter, 3 racers, spare) | vehicle model |
| 25 | cheerer — **not** in the load list | loaded elsewhere (race util?) — **OPEN** |

### Animation groups / action trees

`LoadAnimationGroup`: `NIS_2_04`, `3_R08RaceLeague`, `Cheer_Girl3`, `Cheer_Nerd3`, `Cheer_Posh3`, `Cheer_Cool2`, `3_G3`. `LoadActionTree`: `Act/Conv/2_04.act` (cheer/podium/idle nodes), `Act/Anim/Race.act` (countdown).

### Props / pickups / weapons

None — no weapons, pickups or destructible props anywhere in the script. World dressing: `RL_rich1` special-entity set (race barriers) on during the race, off in cleanup.

### Speech

Mission-scoped: `M_2_04` ids 19, 21 (podium, supersize) — **2 scripted lines**. Plus three generic `TRASH_TALK_TEAM` events (race intro) and countdown barks. Music: `MS_BikeRace01.rsm` during the race; pass/fail stingers (`SoundPlayMissionEndMusic`, bank 8).

### Special entities

`AreaLoadSpecialEntities("RL_rich1", true/false)` — barrier dressing swap (**world_state_swap**, same family as ch1 1_03's override).

### Named world data to extract from `2_04.dat`

- POINTLISTs: `_2_04_PLAYER`, `_2_04_PETEYSPAWN` (2), `_2_04_PLAYERBIKE`, `_2_04_RESTARTPLAYER` (4), `_2_04_PETER`, `_2_04_OWNER`, `_2_04_PREP1/2/3`, `_2_04_PREP1BIKE/2BIKE/3BIKE`, `_2_04_CROWDCHEER` (3), `_2_04_BEACHCHEERCHANGE` (8), `_2_04_NIS_END` (2), `_2_04_PLAYERRICH`, `_SV_BOYSDORMMAIN` (referenced as race player startPosition — shared point).
- PATHs: `_2_04_PETEPATH`, `_2_04_RACE`, `_2_04_RICHSHORTCUT01..05`, `_2_04_INTROPATH`, `_2_04_INTROLOOK`, `_2_04_NIS_END_WALK`.
- TRIGGERs: `_2_04_CHANGEBEACH`, `_AMB_RICH_AREA` (shared ambient rich-area volume).
- Hardcoded coords: owner post-mission spot (337.0773, 274.9468, 7.3130); podium cameras (329.2388, 277.0464, 7.5217)→(329.7032, 276.1627, 7.4670) and (324.5582, 269.2296, 7.7510)→(325.4314, 269.6697, 7.9601).
- Area codes seen: **0 only**.

## 5. Framework gaps (beat → primitives)

Reuses ch1 vocabulary: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `mission_critical_npc` fail, `npc_follow_path`, `completion_banner`, `set_faction_respect`, `population_override`, `world_state_swap`, `health_bar(name)`, `cutscene_skip_on_retry` (here checkpoint-driven instead).

| Beat | Covered by | New primitives needed |
|---|---|---|
| checkpoint resume | `set_flag`-adjacent | **script_checkpoint**(key): mid-mission saved-data flag that both skips the intro cutscene *and* swaps the spawn table on retry — generalizes ch1 `cutscene_skip_on_retry` |
| R2/R3 ride-to-beach | `reach_location` | **player_mounted** condition (on any/last vehicle); **vehicle_follow_path** (Peter rides a path on a bike); nag-blip ping-pong = ch1 `dynamic_blip` |
| R4 race | — | **race_controller** (laps, course path, rubber-band params per racer, shortcuts, highlighted nodes, countdown ped, soundtrack, position HUD, win/lose + reason string). This is the one big new subsystem; BikeRace_util is its spec. |
| R4 crowd | `spawn_npc` | **crowd_ring** (ch1 1_B) reuse + re-seat on trigger |
| R5 intro | `camera`, `say` | **camera path playback** (already named in ch1 `chapt1trans`) + audio-fade-with-camera |
| R6 countdown | — | small: named countdown node + control handoff (fold into race_controller) |
| R7 podium | `camera`, `say`, `fade`, `completion_banner` | nothing new |

Genuinely NEW names introduced here: **script_checkpoint**, **player_mounted**, **vehicle_follow_path**, **race_controller**. The race controller dwarfs everything else in the file.

## 6. Open questions

- `BikeRace_util.lua` internals (`RaceControl`, `CreateCountdownPed`, shortcut/highlight semantics, fail-reason strings) — not fully re-read for this pass; it is the implementation spec for `race_controller`.
- Pass banner value 1500: money vs score (**INFERRED** money; eggs in 2_03 price-check against 150 units).
- Roles of loaded-but-unspawned models 38/142/9/68/139 and where cheerer model 25 is loaded.
- Whether the countdown ped created in `F_CreateBeachPeds` and the one `CreateCountdownPed` uses are the same entity (**INFERRED** yes, via `tblRaceInfo.race.countdown_ped.id`).
- How the mission is offered in the world (start trigger/blip outside the Lua) — **OPEN** as usual.
