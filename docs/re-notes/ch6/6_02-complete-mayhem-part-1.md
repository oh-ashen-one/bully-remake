# 6_02 — Complete Mayhem, part 1 (Wonder Meats): recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap6/6_02.lua` (439 lines, complete). No `SMissPass.lua` branch (**VERIFIED**). Structure in our own words; script/native/string-key names are citations only.

## 1. Identification

**6_02 = "Complete Mayhem" (part 1 of 2)** — the find-Russell-at-Wonder-Meats act. Canonical mission lists treat Complete Mayhem as one mission (#65, 8AM–8PM, final chapter's penultimate mission); the engine splits it across two scripts: `6_02` (rescue Russell from the police cordon at the Wonder Meats plant) and `6_03` (retake the school from the four clique leaders). **VERIFIED** as a direct continuation pair: `6_03` co-loads `6_02.DAT`, opens with the baked cutscene `"6-02B"` and repositions the player to another `_INDUS_POINTS` element — a seamless handoff, not two separate missions. Walkthrough cross-check ([StrategyWiki Ch.5](https://strategywiki.org/wiki/Bully/Chapter_5:_The_Fall_and_Rise_of_Jimmy_Hopkins,_Aged_15)): Complete Mayhem begins with Russell hiding from the police in the Wonder Meats factory after his Chem-O-Lot stunt — exactly this script.

Dead/loose ends (**VERIFIED** in-file): the file carries a big block of copy-pasted-but-unused globals from `6_03` (`gBikePeds`, `gEdgar`, `gGary*` fields, `bNerdBoss/bGreaserBoss/bJockBoss/bPreppyBoss`, `gNerdThread`…); `F_RunRescueRussellNIS` is an empty stub; `gStageEvent`, `gTriggerActivated` are never read; `CB_RussellPath`'s `bRussellReachedEnd` is set but never consumed (Russell's escape is ended by the player hitting the gates trigger, not by Russell finishing his path — **INFERRED** the pursuit outlasts him by design). `gCop1/gCop2` in the final NIS leak to globals. `F_PlaySpeechWait` is defined and unused here (used in 6_03's copy).

## 2. Mission flow

Driver: a **function-pointer stage poller** — `main()` loops `gStageUpdateFunction()` every frame (`F_SetupStage1` → `F_Stage1` → `F_Stage2A` → `F_Stage3`) until `gMissionStage` flips to `"passed"`/`"failed"`. No checkpoints.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup + baked cutscene "6-02"** | Russell's unique model (176) suppressed for the duration; the school front doors lock and both school gates are forced closed via shared overrides (`shared.gOverrideSchoolGates`, `gParkingGateOpen`, `gFrontGateOpen`); interactive music armed (`MS_RunningLow02` with `MS_SearchingMid` as the mid layer); POI groups off; a dumpster persistent entity (`DPE_Dumpster`) is created near the plant; `shared.enclaveGateRespawn = 2`; cop models 83/97 + cop car 295 loaded. Player is teleported to the industrial estate (`F_MagicalJasonsByRobertoTransition(0, _INDUS_POINTS, 1)` — a dev-named transition helper) facing east. |
| B1 | **Infiltrate the cordon** (`F_SetupStage1`) | Two cops patrol fixed paths at Wonder Meats (`PATH._INDUS_COPA/B`), two siren-blaring cop cars with drivers wait at `_COPS`, two more foot cops stand by — all with attitude 0 (hostile) toward player types 3/13. Objective 1 + point blip toward the plant interior. |
| B2 | **Find Russell** (`F_Stage1`) | First entry into `_FACTORYENTRANCE` spawns Russell cowering at `_INDUS_POINTS,6` (action node `RussellHide/Cower_Child`); objective 1 completes, objective 2 + blip appear, music switches to the Searching low/mid/high stack. When the player gets within 5 m of Russell **and** is inside `_6_02_WONDERMEATS`: Russell plays a rise/reaction beat + one line, then a second line 3 s later, and bolts along `PATH._6_02_RUSSELLRUNAWAY` at max gait with infinite sprint and a health bar on screen; objective 3; all ambient vehicles are cleared and ambient traffic is zeroed; the stage-1 cops are ambient-ized and a **final chase set** spawns (2 foot cops + 2 siren cars at `_6_02_CHASECOPS`/`_OUTSIDECOPS`). |
| B3 | **Escape pursuit** (`F_Stage2A`) | Re-entering `_FACTORYENTRANCE` sends two foot cops attacking the player. (Russell's own survival is watched every frame in `main()`.) |
| B4 | **Gates NIS: Russell's tackle** (`F_Stage3` → `F_RunFinalNIS`) | Player in `_INDUSTRIALGATES` → scripted escape vignette: any vehicle the player is riding is deleted, control off, both actors repositioned; the player auto-runs `PATH._6_02_PLAYERRUN` (path-end callback gates the beat); at the end Russell grapples a NIS cop (`WonderNIS/Grapple` vs `Receive`); camera cut to the alley; both peds put to sleep/frozen while a searching cop plays a look-around + "target lost" bark and walks off along `_6_02_COPGOFOLLOW` (stimulus-ignore + ignore-lists keep everyone calm); final alley talk — Russell pledge beat (his NIS03 anim + one line, player reply), control back. `gMissionStage = "passed"`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Russell dies (any time after spawn) | per-frame `PedIsDead(gRussell)` poll in `main()` | `6_02_RUSSKO` (fail stinger 10) |
| Any other failure | generic | `MissionFail()` |

Full replay (no checkpoints); cleanup deletes Russell, ambient-izes/deletes all cops and cars, unlocks the school front doors, releases the gate overrides implicitly via `MissionCleanup` scope (**OPEN** — the shared gate-override flags are set but never cleared in-file; presumably cleared by the next mission/area script), stops the interactive stream, unloads `NIS_6_03`-era groups.

### Pass-side effects

- In-file: `MissionSucceed(true, false, false)` (completion banner) only; the continuation into 6_03 is engine-side mission-table chaining (**OPEN** — no Lua declares it; see 6_03's setup for the receiving half).
- `SMissPass.lua`: no branch (**VERIFIED**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-1 | B0 | baked (`6-02`) | `fade` + `letterbox` + one `say` beat |
| CS-2 | B2 Russell reveal | in-engine, ~4 s | `inline_cutscene_chain`: `ped_action_node` (cower→rise) + `speech_sequence` + `npc_flee_path` start |
| CS-3 | B4 gates tackle + alley hide | in-engine, ~15 s | `inline_cutscene_chain` with `player_follow_path`, paired grapple (`pair_anim`), `npc_asleep_staging`, `npc_ignore_list`, `npc_ignore_stimuli`, two `camera` cuts, cop walk-off (`npc_follow_path`) |

Anim groups: `6_02CompMayh`, `NIS_6_02`; action tree `Act/Conv/6_02.act`. Music: `MS_RunningLow02`/`MS_Searching{Low,Mid,High}.rsm`; fail stinger 10.

## 4. NPCs and assets needed

### Peds (model ids as scripted)

| Model id | Role | Notes |
|---|---|---|
| 176 | Russell | unique-model-suppressed globally for the mission; cower spawn → scripted flee path → NIS actor; death = keyed fail |
| 83 / 97 | cops (foot + drivers) | two patrol-path pairs stage 1; chase set stage 2; NIS pair |
| 72, 66, 70, 73 | loaded but never spawned in-file (**INFERRED** cut content — townies/Edgar-era beats) | |
| 130 (Gary), 91 (Edgar) | declared in unused globals only | copy-paste residue |

Vehicles: 295 cop cars ×4 (siren + engine flags; `PedWarpIntoCar` drivers). Persistent entity: `DPE_Dumpster`. No weapons given.

### Named world data to extract from `6_02.DAT` (present — `DATLoad("6_02.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_INDUS_POINTS` (multi-element: player start, NIS positions 3/4/5/9, end-NIS pair, 6_03's stage-5 start at 12), `_COPS`, `_6_02_CHASECOPS`, `_6_02_OUTSIDECOPS`, `_6_02_COPSNIS`, `_6_02_ENDNIS`, `_OBJECTIVELOCS` (shared with 6_03).
- PATHs: `_INDUS_COPA/B`, `_6_02_RUSSELLRUNAWAY`, `_6_02_PLAYERRUN`, `_6_02_COPGOFOLLOW`.
- TRIGGERs: `_FACTORYENTRANCE`, `_6_02_WONDERMEATS`, `_INDUSTRIALGATES`, `_DT_TSCHOOL_SCHOOLFRONTDOORL`, `_TSCHOOL_SCHOOLFRONTDOORR`.
- Hardcoded coords: three NIS camera pairs, dumpster placement, FOV 30 shots.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by existing set | New primitives needed |
|---|---|---|
| B0 school lockdown from afar | `door_lock_set` (3_05-era), `area_script_shared_state` (3_05), `unique_ped_suppress` | none new — the shared gate-override flags are `area_script_shared_state` data |
| B0/B2 music | `interactive_music_stream`, `music_intensity_by_distance` | none |
| B1 police cordon | `npc_patrol_cycle`, `pursuit_car`, `vehicle_siren_engine_flags`, `conditional_faction_attitude` | none |
| B2 Russell reveal + flee | `presence_gated_npc` (spawn on zone entry), `npc_within_radius`, `npc_flee_task_watch`, `npc_health_bar`, `npc_stat_override` (infinite sprint) | none — composition |
| B2/B4 objective blip swaps | `dynamic_blip` | none |
| B3 trigger-attack | `npc_spawn_walk_then_attack`, `trigger_chain` | none |
| B4 escape NIS | `inline_cutscene_chain`, `player_follow_path`, `pair_anim`, `npc_asleep_staging`, `npc_ignore_list`, `npc_ignore_stimuli`, `npc_dismounted_event` (vehicle delete variant: `dismount_vehicle`) | none new — but note the **ally-grapples-pursuer** beat is a two-ped `pair_anim` on *attacker* roles, a new use not a new primitive |
| ally death fail | `mission_critical_npc` (expiry variant) | none — here it's a raw per-frame poll; fold into `mission_critical_npc`'s KO event |
| fail cleanup | `weapon_inventory_snapshot` (not used here), door unlocks | none |

## 6. Open questions

- Where the 6_02→6_03 chain is declared (engine mission table; not in Lua). 6_03's `"6-02B"` cutscene + shared DAT make the seam invisible in-game.
- The stage-1 cop hostility uses attitude slots 3/13 vs value 0 — the usual attitude-matrix decode question (same standing question as 3_04/4_04/4_06).
- Models 72/66/70/73 loaded, never spawned — cut beats (Edgar escort? more townies?).
- Whether `shared.gOverrideSchoolGates` etc. are cleared anywhere (6_03? area scripts?) — no clearer found in this file.
- All trigger/point geometry pending `6_02.DAT` extraction.
