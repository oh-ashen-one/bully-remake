# 3_G3 — Lola's Race: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_G3.lua` (991 lines) + `Library/BikeRace_util.lua` (the race library it drives). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_G3 = Lola's Race** (catalog #40). **VERIFIED** by content: `G`-series "girl mission" numbering (1_G1 Diary, 2_G2 Carnival Date), Lola (model 25) as countdown/reward ped, Johnny Vincent (23) plus greasers Lucky (26)/Ricky (28) as rival racers, the kiss reward beat (`Player/Social_Actions/MakeOut` nodes), greaser respect −20 on a win, and the `3_G3_UNLOCK` completion message. One-lap bike race through the poor district / rail yards of New Coventry with live hazards (moving train cars, falling trees) and a greaser crowd.

Uses the shared `BikeRace_util.lua` library (`SetParam_Race/Player/Racers/Shortcuts/HighlightedNodes`, `RaceSetup`, `L_RaceStart`, `L_RaceIsOver`, `L_RacePlayerNotFirst`, `L_RaceStayOnBike`, `L_RaceTooFarBehind`) — same engine as 2_04 (see `2_04-race-the-vale.md`) and 3_R08.

## 2. Mission flow

Driver: `MissionSetup()` (plays baked intro `"3-G3"` unless `shared.g3G3TrainTest` debug is set) → `main()` → `F_MissionPlacement()` → race → win/lose branch.

| # | Beat | Logic (functional) |
|---|---|---|
| G0 | **Setup** | Garage deactivated; POI disabled; interactive music off; ambient vehicles cleared/overridden; population forced to a lone slot (index 11 = 1); `3_G3.DAT` loaded; 11 train-car persistent entities created (some parked dressing, 8 with paths/speeds/proximity radii); 3 tree props registered with hidden greaser peds (models 29/22/28, asleep + ignore-stimuli); race/player/racer/shortcut/highlight tables pushed to the race library; greaser→player attitude set friendly (4, 13, 0) for the race. |
| G1 | **Pre-race NIS** | Unused vehicles near the player deleted; widescreen; camera rides `PATH._3_G3_PATHCAM` looking along `_3_G3_PATHCAMLOOK` (speed 5) while each of the 3 racers plays a `TRASH_TALK_TEAM` bark; fade through black; control back. |
| G2 | **The race** | 1 lap on `PATH._3_G3_RACE` (jump nodes 7/14/15/36/42), soundtrack `MS_BikeRace01.rsm`, 3 racers on bike 273 with rubber-band params (sprint/normal caps 0.83-0.92, catch-up 14.5-10 m @1.3-1.6×, slow-down 25 m @0.35×, shortcut odds 30/40/**100 for Johnny**, aggressiveness 0.65-0.8). Lola is the countdown ped (`_3_G3_LOLACOUNTDOWN`) and is **mission-critical** (`cbCritFail`). Six shortcut paths (`_3_G3_SCUT01..10`) with start/end node windows. During the race: tree monitors (player within 100 m spawns the hidden greaser; entering the tree trigger plays the greaser's push node + the tree's `TreeFall/Damage/Fall` prop node, then the greaser wakes and attacks; tree prop deletes/resets at 75 m radius) and train monitors (proximity 30-65 m starts the car rolling at 20-35 speed with a looped `TrainLoop` sound; node-1 callback stops it with `TrainStop`). At the `_3_G3_MOVELOLA` trigger, Lola teleports to the finish straight with 4 spawned cheerers (22/29/26/27) playing a cheer node. A water watchdog fails the race if the player's bike stays below z=0 for two consecutive 1 s polls (warn text then fail — the course skirts water, **INFERRED**). |
| G3 | **Win** | Race-over check via `L_RaceIsOver`; win requires `RaceGetPositionInRaceOfRacer(gPlayer) == 1`. Race-end NIS (`M_RACEWIN` banner, `RaceVictory` bike anim if not on a 276), then `F_EndingCinematic_new`: racers deleted, attitude restored mid-scene, greaser crowd (Johnny/Lucky/Ricky/Lola) at `_3_G3_ECS_*` points; Johnny bark (M_3_G3 id 10), Lola bark (id 12), greasers walk off, then the player-Lola makeout (`GrappleAttempt/Kisses` nodes, mutual lock, Lola emotion 8 toward player) under `M_PASS` + `3_G3_UNLOCK` banner with respect toasts (`MRESPECT_GM20`); Lola becomes ambient/wanders after. Then greaser respect **−20** (`SetFactionRespect(4, GetFactionRespect(4) - 20)`), cutscene peds deleted, `MissionSucceed`. |
| G4 | **Lose** | `L_RacePlayerNotFirst` (a rival finished first), `L_RaceStayOnBike` timeout, or `L_RaceTooFarBehind` → `EndMission`, fail music; keyed reasons `RACING_L_NOBIKE` / `RACE_TOOFAR` if set, else a generic `GKART_YOULOSE` banner; countdown ped + cheerers made ambient. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Rival finishes first | `L_RacePlayerNotFirst` | generic lose (`GKART_YOULOSE`) |
| Off the bike too long | library `get_on_delay` countdown (`L_RaceStayOnBike`) | `RACING_L_NOBIKE` |
| Too far behind the pack | library distance gate (`L_RaceTooFarBehind`, warn then fail distance) | `RACE_TOOFAR` |
| Bike in water ≥ ~2 s | `F_WaterMonitor` z<0 twice | (unkeyed `MissionFail`) |
| Lola (countdown ped) hit | `PedSetMissionCritical(..., cbCritFail)` | `3_G3_FAILHIT` |
| Player incapacitated | `PedSetMissionCritical(gPlayer, cbCritPlayer)` sets `shared.gPlayerIncapacitated` | engine-driven |

No stage checkpoints; a fail replays from the baked intro (no `IsMissionFromRestart` handling in-file — **OPEN** whether the outer launcher skips it).

### Rewards / pass side-effects

- Greaser respect −20 (in-script), respect toast `MRESPECT_GM20` on the banner, `3_G3_UNLOCK` completion message key (what it unlocks is data-side — **OPEN**, plausibly the next story star).
- No `SMissPass.lua` case for 3_G3 (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked `"3-G3"` | `fade` + original card |
| CS-2 | G1 pre-race trash talk | in-engine camera path pair + 3 barks | `camera_rail_pair` + `say` ×3 |
| CS-3 | G3 race-end | in-engine (`M_RACEWIN` + victory anim) | `completion_banner` + player anim beat |
| CS-4 | G3 reward scene | in-engine, ~15 s, 2 camera shots + makeout | `camera` ×2 + `say` ×2 + `pair_anim` (kiss) + ambient-ify |
| CS-5 | trees/trains mid-race | live props | none — hazard systems |

Anim groups: `NPC_Love`, `F_Girls`, `F_Greas`, `GEN_Social`, `NPC_Adult`, `NPC_Cheering`, `3_G3`, `Cheer_Girl3`, `Cheer_Cool2`, `NIS_3_G3`. Action trees: `Act/Conv/3_G3.act` (push/cheer/cutscene nodes), `Act/Anim/Race.act`, plus `Act/Props/TreeFall.act` and `Act/Player.act` (makeout). Dead code: `F_EndingCinematic` (the old text-based ending, superseded by `_new`), `F_LolaMakeKissable`, `F_TrainTest` (debug harness gated on `shared.g3G3TrainTest`), `F_CSPlayIntro` never called... (actually intro is `PlayCutsceneWithLoad("3-G3")` in MissionSetup — VERIFIED).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 25 | **Lola** | countdown ped → finish-line cheerer → kiss reward; mission-critical (hit = fail) |
| 23 | Johnny Vincent (racer) | fastest profile, shortcut odds 100 |
| 26, 28 | Lucky/Ricky (racers) | mid profiles |
| 29/22/28 | tree-trap greasers ×3 | asleep+ignore until their tree falls, then attack |
| 22/29/26/27 | finish-line cheerers ×4 | stationary cheer node, ambient after |

### Props / hazards

- 11 `TrainCarA/B/C` persistent entities; 8 path-driven (paths `_3_G3_CAR1A/1B/2A/2B/3A/3B/4A/4B`), proximity-triggered with `TrainLoop`/`TrainStop` 2D sounds.
- 3 tree props (`TRIGGER._3_G3_TREE1/3/4` as both prop id and fall trigger) with `TreeFall.act` damage node.
- Player/racer bikes all model 273; bike 276 excluded from the victory anim (a moped check, **INFERRED**).

### Named world data to extract from `3_G3.DAT` (DAT present, VERIFIED)

- POINTLISTs: `_3_G3_PLAYER`, `_3_G3_LOLACOUNTDOWN`, `_3_G3_CHEERINGPED1..5`, `_3_G3_TREEPED1/3/4`, `_3_G3_ECS_LOLA/JOHNNY/LUCKY/RICKY/PLAYER/PLAYERENDBIKE`.
- PATHs: `_3_G3_RACE` (course, ~86 nodes; jump nodes 7/14/15/36/42), `_3_G3_SCUT01/02/05/08/09/10`, `_3_G3_PATHCAM`, `_3_G3_PATHCAMLOOK`, `_3_G3_CAR1A..4B` (trains), `_3_G3_ECS_LOLA` (walk-off).
- TRIGGERs: `_3_G3_TREE1/3/4`, `_3_G3_MOVELOLA`.
- Hardcoded camera coords: ending scene (447.93, -452.50, 3.68 → …; 449.79, -450.43, 3.80 → …) plus the dead `F_EndingCinematic` set.
- Area codes: **0** throughout.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| G2 race core | `race_controller` (2_04 doc: laps/path/rubber-band/shortcuts/highlights/countdown ped/soundtrack/win-lose reasons) | reuse; this file adds a **countdown-ped-as-NPC** (mission-critical Lola + relocate-to-finish on trigger + cheer squad spawn) — **race_rail_persona** |
| trees | `prop_set_state` | **boobytrap_prop_pair** (armed prop + hidden ped: proximity spawns ped asleep; trigger → ped push anim + prop damage anim + ped wakes hostile; prop delete/rearm on a 75 m player radius) |
| trains | `effect_spawn_at_points` (sounds) | **proximity_prop_mover** (persistent entity + path; player within R → follow path at speed S with looped sound; node callback → stop + end sound) |
| water | — | **vehicle_z_fail** (bike z<0 sustained ~2 s → warn then fail; course-specific watchdog) |
| G3 reward | `pair_anim`, `completion_banner`, `set_faction_respect` | **social_reward_kiss** (mutual lock + `MakeOut` nodes + emotion state + respect delta −20 with toast; closest existing: `social_override_set` — this is the playable variant) |
| G1 intro | `camera_rail_pair` (ch2trans pool) | **racer_trash_talk** (per-racer `TRASH_TALK_TEAM` bark during the flyover — a `say` variant; can fold into camera beat) |
| fails | race library reasons | fold `RACING_L_NOBIKE`/`RACE_TOOFAR` into `race_controller` fail keys |

## 6. Open questions

- What `3_G3_UNLOCK` unlocks (text/data side; likely just the pass banner's unlock line).
- Exact `BikeRace_util` distance thresholds (`nWarningDistance`/`nFailDistance`, `get_on_delay`, `finish_delay`) — library globals, not read line-by-line this pass.
- Whether the baked `"3-G3"` intro is double-played with anything the launcher does.
- Tree prop ids double as trigger ids (`TRIGGER._3_G3_TREE1` used with both `PAnimSetActionNode` and `PlayerIsInTrigger`) — one DAT entity serving both roles; confirm in extraction.
- The `F_WaterMonitor` 10 s startup delay and 3 s warn gap: exact fail cadence (two consecutive sub-0 reads 1 s apart) is read, but whether it also catches the player on foot is **OPEN** (polls the bike's position only).
