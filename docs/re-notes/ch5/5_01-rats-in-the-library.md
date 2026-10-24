# 5_01 — Rats in the Library: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_01.lua` (623 lines, complete) + cross-references in `SMissPass.lua`, `SFaction.lua`, `AreaScripts/Bdorm.lua`, `AreaScripts/Boxing.lua`, `Chapt5Trans.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_01 = "Rats in the Library"** (canonical Chapter 5 list, #55 in `docs/story-mode-catalog.md`, 8AM–9PM) — **VERIFIED**: the [Bully wiki Chapter V page](https://bully.fandom.com/wiki/Chapter_V) lists "Rats in the Library `5-01`" by script id, and the script content matches beat-for-beat (library interior, area 9; the librarian hands over a poison-spray gun; kill 20 rats before the timer; the spray slowly poisons the player too). Pass block matches the wiki's reward shape: `MinigameSetCompletion("M_PASS", true, 3000)` ($30), respect message `MRESPECT_NP15`, `SetFactionRespect(1, GetFactionRespect(1) + 15)`. Clique decode (consistent with ch4 docs): faction 1 = nerds, 13 = player; the message key carries the delta semantics (+15 nerds, **INFERRED** from key + code agreement).

Setting: entirely inside the library interior (area 9; entered via `AreaTransitionPoint(9, POINTLIST._5_01_MOVETO)`); fail/pass handling references area 0 only in cleanup. Downstream (**VERIFIED** in the clone):

- `AreaScripts/Bdorm.lua:174`: once 5_01 is complete, dorm trophy #16 exists (a bottled rat on the filing cabinet — room-trophy count +1).
- `AreaScripts/Boxing.lua:64`: once 5_01 is complete (or 5_02 is running), the boxing club's trophy case is created in its emptied/busted variant — the story beat that the trophies are already gone by "Preppies Vandalized".
- `SFaction.lua`: unique models 4/208/3/95 are suppressed while 5_01 is available; 4 and 3 are restored on completion.
- `SMissPass.lua:233`: the 5_01 pass block contains only an odd `if ChapterGet() <= 4 then ChapterSet(4)` under comments claiming to unlock the super spud gun and "set chapter to 5" — the code does neither (**OPEN**, see §6). The real 4→5 advance is `Chapt5Trans.lua:196` `ChapterSet(5)`, not this mission. This contradicts the ch4/4_02 note ("the 4→5 advance happens at 5_01") — corrected here.
- No `ChapterSet` in 5_01.lua itself.

## 2. Mission flow

Driver: `MissionSetup()` (no fade-in, interactive music off, baked intro "5-01", `DATLoad("5_01.DAT", 2)`, teleport into area 9, control off) → `main()` waits for setup, runs `T_LibraryInteriorInit` inline, starts threads `T_MonitorLoop` / `T_LibraryInteriorExitWait` / `T_PoisonCount`, then parks on `mission_done`. Only three endings: timer expiry (fail key `5_01_01`), librarian aggression (fail key `5_01_02`), all 20 rats dead (success).

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup** | `shared.gLibraryCrateShouldShow = true` (world-state flag; no consumer in the clone — **OPEN**); non-mission ped generation disabled; rat model 136 + librarian model 62 requested; rat unique-model status snapshotted and suppressed; weapons 395/310/312/343 requested (395 = the poison sprayer, destroyed in the outro — the only confirmed id); anim groups `5_01Grp`, `F_Nerds`; 20 spawn pointlists + 20 spawn triggers read into tables. |
| S1 | **Enter the library** (`T_LibraryInteriorInit`) | Exit door `_DT_LIBRARYEXITR` opened then closed and locked behind the player. Librarian (62) created at her desk point, tethered (0.25 radius), idle action node `5_01_Librarian`, stimuli ignored, untargetable, and **mission-critical with an aggression callback** (`CbPlayerAggressed` → instant fail `5_01_02`). A blip is created on her and immediately removed (dead code). Music `MS_FinalShowdownMid.rsm` at 0.35; **`MissionTimerStart(400)`** (400 s); HUD counter armed (icon "Rat", max 20, current 0); objective `5_01_Obj`; control on. |
| S2 | **Rat spawner** (`F_CreateSpawner`) | `AreaAddMissionSpawner(20, 15, -1, 0, 1000)` — quota 20, 15 concurrent, 1 s cadence — with one `AreaAddSpawnLocation` per spawn point/trigger pair, model 136 bound to each. Per-spawn callback: ped becomes a mission char, tethered to `_5_01_MAIN_ROOM_TR`, given a blip, added to the `"rats"` LibPed group. |
| S3 | **Kill counter + music ramp** (`T_MonitorLoop`) | Every 10 ms tick: librarian barks (`M_5_01` id 1) every ~12 s (±4 s jitter); the rats group is iterated — dead or ≤0-health rats increment the count and lose their blips; counter HUD updated. Kill 7 → mid-intensity stream re-fired at 0.6; kill 13 → `MS_FinalShowdownHigh.rsm` at 0.7. Count == 20 → `mission_succeeded = true`, loop breaks, `mission_done = true` after 1 s. |
| S4 | **Poison exposure** (`T_PoisonCount`) | A self-harm accumulator: +100 per 33 ms tick while the player is playing the poison-spray run-attack anim (`F_PlayerIsSpraying`), −50 otherwise (floored at 0). Crossing 30000 arms screen-effect node `5_01_PoisonEffect1` + librarian line 4; crossing 60000 arms `5_01_PoisonEffect2` + line 5. Falling below 25500 / 51000 plays the matching effect outro nodes and re-arms the flags — hysteresis band of ~15% per stage. `F_ClearDizzy` zeroes the meter and plays `5_01_PoisonEffect0` (used in cleanup and the outro). |
| S5 | **Leaving the library** (`T_LibraryInteriorExitWait`) | Once the player has entered (`bEnteredLibrary`), watch for `AreaGetVisible() ~= 9`: monitoring stops, the spawner is removed, the counter hides, 4 s later the thread ends. Because `T_MonitorLoop`'s loop condition dies with it, the loop falls through to `mission_done = true` with `mission_succeeded` still false → **generic `MissionFail()` with no key** (**VERIFIED** from control flow — exiting early is an immediate, unkeyed fail). |
| S6 | **Pass NIS** (`F_FinalCutscene`) | Fade; player teleported to `_5_01_PLAYERENDINGSTART`, librarian repositioned/untethered, mutual facing; player's weapon 395 destroyed (sprayer handed back, **INFERRED** from the hand-over pathing). `NIS_5_01` group loads; five hardcoded camera setups with FOV 70/30 alternate two-shots while scripted lines 7–12 play (librarian 7/9/11, player 8/10/12) over `Player_01/02` + `Librarian_01/02/03` nodes and the player's `_5_01_RETURNWEAPON`/`_5_01_ENDINGPATH`/`_5_01_ENDINGPATH02` path walks. Librarian goes ambient and walks `_5_01_LIBRARIANEND` out; player heading snapped to 135. `M_PASS` + $30 + `MRESPECT_NP15` + nerds +15; `MissionSucceed(false, false, false)` (first-arg false = no save prompt here, **INFERRED**). |

### Fail conditions and retry

- Timer expiry (400 s) → fail key `5_01_01`.
- Attacking the librarian (mission-critical aggression callback) → fail key `5_01_02`.
- Leaving the library interior after entering → unkeyed fail (S5).
- Player KO → generic engine fail.
- On fail cleanup: `AreaEnsureSpecialEntitiesAreCreatedWithOverride("5_01", 0)`, librarian and all recorded dead/alive rat peds deleted, garbage-pickup table swept (never populated in-file — dead table), exit door unlocked, counter hidden, `gLibraryCrateShouldShow = false`. No checkpoints: retry replays from S0. Note the fail path also forces the player out of any sitting-stool anim (`pxSitStl` get-up node) before the fail banner.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked (`5-01`, extra arg `true`) | `fade` + `letterbox` + `say` |
| CS-2 | S6 weapon hand-back | in-engine (5 fixed cameras, 6 lines, path walks) | `camera` ×5 + `speech_sequence` + `npc_follow_path` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 62 | librarian | mission-critical, untargetable, tethered to desk; barks ids 1/4/5; NIS lines 7/9/11 |
| 136 | rat ×20 | quota spawner, trigger-tethered, per-rat blip; unique model suppressed for the mission |

### Animation groups / action trees / AI

`5_01Grp`, `F_Nerds`, `NIS_5_01`; `Act/Conv/5_01.act` (`5_01_Librarian`, `5_01_Empty`, `NIS/Librarian/Librarian_01..03`, `NIS/Player/Player_01/02`); `Act/Props/pxSitStl.act` (fail-path stand-up); weapon anim branch `/Global/Weapons/WeaponActions/Melee/PoisonSpray/...` (spray-state detection); mission action-controller nodes `/Global/5_01/5_01_PoisonEffect0/1/2(+intro/cycle/outro)` (full-screen poison FX state machine).

### Props / pickups / weapons

- Door `_DT_LIBRARYEXITR` (open → closed → locked; unlocked in cleanup).
- Weapon models 395 (poison sprayer — confirmed via outro destruction), 310, 312, 343 (**OPEN** decode for the last three; probably rat-usable world weapons).
- `garbagePickupsTable` — declared, swept on cleanup, never filled (dead).

### Speech

`M_5_01` scripted ids: 1, 4, 5, 7, 8, 9, 10, 11, 12 — **9 scripted lines** (no separate 2D bark pool). Music: `MS_FinalShowdownMid.rsm` (0.35 → 0.6 at 7 kills), `MS_FinalShowdownHigh.rsm` (0.7 at 13 kills), pass/fail stingers (track 4).

### Named world data to extract from `5_01.dat`

- POINTLISTs: `_5_01_MOVETO`, `_5_01_LIBRARIAN`, `_5_01_PLAYERENDINGSTART`, `_5_01_RATSPAWN01..20`.
- TRIGGERs: `_5_01_RATSPAWNT01..20`, `_5_01_MAIN_ROOM_TR`, `_DT_LIBRARYEXITR`.
- PATHs: `_5_01_RETURNWEAPON`, `_5_01_ENDINGPATH`, `_5_01_ENDINGPATH02`, `_5_01_LIBRARIANEND`.
- Hardcoded: 5 fixed camera position/look pairs and one heading (135) in the outro.
- Area codes seen: **9** (library interior).
- **Trigger data (DAT): YES** — `DATLoad("5_01.DAT", 2)`; original lives in `Trigger.img` per `trigger-format.md`. Not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `camera`, `speech_sequence`, `collect_counter` (1_S01 — counter icon/max, kill increment), `countdown_fail_timer` (S1 400 s), `area_mission_spawner` (3_R09 — exactly this `AreaAddMissionSpawner` + per-spawn callback shape), `npc_tether_to_zone` / `tethered_npc_spawn` (rat tether to main-room trigger), `blip_on_npc` (per-rat blips + dead-blip sweep), `unique_ped_suppress` (rat model), `door_lock_set` (exit door), `mission_critical_npc` + `ped_hit_event` (librarian aggression fail), `meter_threshold_escalation` (3_XM — the 7/13-kill music ramp is a threshold table on the kill counter), `geometry_instance_toggle` (`F_SetSecondFloorInvisible` batch — **dead in-file**, defined but never called; presumably the library area script's culling hook, **OPEN**), `area_script_shared_state` / `world_state_swap` (`gLibraryCrateShouldShow`), `special_entity_set_load` (fail cleanup override), `interactive_music_stream`, `weapon_strip` (395 destroyed at outro), `player_action_state` (spray-anim detection — the 4_02 cannon-mount pattern), `respect_delta` (3_S10), `completion_banner`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| S2/S3 quota extermination | `area_mission_spawner` + `collect_counter` + `blip_on_npc` | none — quota + tether + per-kill blip is composition |
| S4 poison self-exposure | `player_action_state` (spray detection only) | **spray_exposure_meter**: accumulator fed while the player performs a named weapon action (+rate/tick, −decay/tick, floor 0); rising thresholds arm full-screen post-effect action nodes (and fire barks), lower release thresholds play the outro node and re-arm — hysteresis; mission cleanup must be able to hard-reset it |
| S1/S3 music ramp | `meter_threshold_escalation` (kill-count variant) | none (parameter variant: thresholds fire music-stream swaps, not spawner changes) |
| S5 early-exit fail | `leave_area_fail` | none — but note the semantics differ: exit unwinds the monitor thread into an *unkeyed* generic fail, not a keyed area-fail message |
| S1 librarian protection | `mission_critical_npc`, `ped_hit_event` | none |

Genuinely NEW names introduced here: **spray_exposure_meter**. (`MissionPlayActionNode`/`MissionStopActionController` full-screen post-effect control is the engine-side substrate the primitive drives; the `Counter*` icon parameter is a data note on `collect_counter`.)

## 6. Open questions

- `SMissPass.lua:233`: the 5_01 pass block's comments promise a super-spud-gun unlock and chapter 5, but the code is only `if ChapterGet() <= 4 then ChapterSet(4)` — a no-op or decompile artifact. Which is it, and where did the spud-gun upgrade actually move (the crate pickup-list override sits under the 4_03 block)? **OPEN**; resolve before authoring 5_01's on_pass in the remake (the real chapter advance is `Chapt5Trans.lua:196`).
- `shared.gLibraryCrateShouldShow` has no reader anywhere in the clone (5_01.lua sets/clears it; nothing consumes it). Presumably read by engine-side library-interior spawning not in the dump. **OPEN**.
- `F_SetSecondFloorInvisible` (~80 `GeometryInstance` toggles for the library's upper floor) is defined but never called in 5_01.lua — called from the area script, or dead? **OPEN**.
- Weapon ids 310/312/343 loaded at setup — which world weapons are these, and are they for the player or for ambient peds? **OPEN** (pending `default.ide`/weapon-table cross-check; 395 = poison sprayer is the only confirmed id).
- The S5 early-exit fail path shows no message (unkeyed `MissionFail()`) — is that the shipped behavior, or is a fail string assigned via data? **OPEN**.
- `MissionEndFollowCamera` is written in two places and never read — vestigial follow-camera mode select. **OPEN** (cosmetic).
