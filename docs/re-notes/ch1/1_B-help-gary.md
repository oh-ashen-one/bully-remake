# 1_B — Help Gary (chapter boss: Russell): recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_B.lua` (448 lines) + `SMissPass.lua` (`F_SetupMissionRequirements`) + `Library/LibObjective.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_B = "Help Gary"** (Chapter 1 boss, Russell) — **VERIFIED** by content: boss ped model 75 (`DOlead_Russell` per the opening-slice doc) with stat type `STAT_BOSS_RUSSELL`, action tree `Act/Anim/Boss_Russell.act`, music stream `MS_RussellInTheHole.rsm`, cutscenes `1-B` / `1-BB` / `1-BC`, and the fight-pit door props (`FightPit_DoorClose/Open`). Matches the canonical list entry "Help Gary (8-23, boss: Russell)". No surprise.

Setting: **area code 8** — a new area code for this chapter set: the Hole / fight pit (player transition `AreaTransitionPoint(8, POINTLIST._1_B_PLAYER)`, gate props at ~(-771, -127)). This is neither 0 (`tschool`) nor 2 (`ischool`); the pit is its own area (`ifightpit`-style interior, **INFERRED** name).

## 2. Mission flow

Driver: `MissionSetup()` → `main()`, with the fight governed by `L_ObjectiveSetParam` / `F_ObjectiveMonitor` (`Library/LibObjective.lua`) plus two threads (`T_CrowdCheering`, `T_RussellTalks`).

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Baked cutscene "1-B"** | `PlayCutsceneWithLoad("1-B", true)` inside `MissionSetup` — runs on every entry into `MissionSetup`, no attempt-count guard in this file; retry-skip **INFERRED** to be handled (or not) by the global cutscene manager. Punishment system disabled, non-mission ped generation off, `1_B.DAT` loaded, no fade-in. |
| B1 | **Arena seal + crowd** | Player transitioned into area 8 at `_1_B_PLAYER`. The persistent open-gate entity (`shared.gHoleGateOpenIndex`) is deleted and replaced with `FightPit_DoorClose` at (-771.4, -127.0, 8.8) — the pit is sealed for the fight. 15 stationary, cheap, non-targetable spectator peds spawned on crowd points (`_1_B_CROWD02..21`, see §4) running `/Global/AI/ScriptedAI/CheeringAINode`. |
| B2 | **Boss spawn + player buffs** | Russell created at `_1_B_RUSSELL` with boss stat type, damage-taken multipliers (attack classes 0 and 3 scaled to 0.2-0.5 — he takes reduced damage, **INFERRED** per-class), ignores stimuli. Player health raised to 200 if below, player ped flag 58 set (boss-fight flag, semantics **OPEN**), player weapon inventory snapshotted (restored on fail, deleted on pass), targeting of anything but Russell suppressed (`PlayerIgnoreTargeting`), death fail-display suppressed. Anim groups `Russell`, `STRAF_WREST`, `C_Wrestling` loaded; AI tree `Act/AI/AI_RUSSEL_1_B.act` loaded. |
| B3 | **Fight start** | Auto-restart on, `MS_RussellInTheHole` stream, Russell health bar shown (`1_B_BAR_RUSSELL`), single objective added (`1_B_OBJ` — beat Russell), no objective reminder. Player and Russell lock onto and face each other; fight camera shot `1_B_X`; fade in, control on, Russell's AI tree assigned, Russell attacks. |
| B4 | **Mid-fight: 50 % mark** | Objective-monitor entry `objSecondStageCutscene` (non-stopping): when Russell's health drops to ≤ 50 % of start, `gRussellAngry` is set. `F_CanRussellTalk` (read by his AI tree, **INFERRED**) then blocks further taunts. **OPEN:** whether a mid-fight cutscene was intended here — see below. |
| B5 | **Ambient layer during the fight** | `T_CrowdCheering`: looping crowd ambience (`Fight_Group_Sml_Int.rsm`) plus a random named cheer sfx from a crowd point every ~12 s + random 2-5 s (7 named character cheers at specific crowd points). `T_RussellTalks`: when the AI tree raises `F_RussellTalks` (a script callback, **INFERRED**), the crowd hushes and Russell plays one of four scripted barks (`M_1_B` ids 2, 8, 10, 12). `F_RusselGroundCheck` (tree callback, **INFERRED**): if Russell stays down on the ground for 1 s, he is forced into a rising-attack node. `F_RusselCalm` / `F_RusselUninterupt` manage a berserk flag (`RUSSELL_BESERK`) and restore an overridden stat — berserk cadence lives in the undecoded AI tree. |
| B6 | **Win** (`BeatRussell` condition, stops processing) | Russell dead → first objective completes, `mission_success = true`. Crowd hushed, fade, crowd deleted, gate swapped back to `FightPit_DoorOpen`, `PlayerSetScriptSavedData(14, 0)`, **baked cutscene "1-BC"**, then: faction respect set (11 → 100, 1 → 85, 5 → 50; faction 11 = bullies **INFERRED** from the 1_03 doc, others **OPEN**), yearbook picture 75 (Russell) unlocked + `F_UnlockYearbookReward()`, weapon snapshot deleted, `MissionSucceed(true, false, false)`. |
| B7 | **Pass side-effects** (`SMissPass.lua`, `F_SetupMissionRequirements("1_B")`) | `shared.addBusPoints = true`; if chapter ≤ 1: `ChapterSet(1)` (→ **Chapter 2**) and **force-complete `1_11x2`** (The Big Prank — the optional Halloween-night mission is auto-passed if the boss is beaten first); force-complete the level-1 classes `C_Chem_1`, `C_Art_1`, `Art1X`, `C_Wrestling_1`, `C_English_1`; `shared.updateDefaultKOPoint = true` (new KO restart default). |
| B8 | **Lose** (`PlayerDie` condition, stops processing) | Player KO → fail completion banner (`M_FAIL` with key `1_B_PLAYERKO`), fail music, fade. Then `main`'s fail branch runs `ActionSecondStageCutscene()`: control off, Russell cleared/asleep, crowd deleted, **baked cutscene "1-BB" plays** (Russell-victory / second-stage beat — **INFERRED** content; it is only invoked from this fail path in the live code), auto-restart off, weapon inventory restored, `MissionFail(true, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player knocked out | `F_PlayerIsDead()` in the objective monitor | `1_B_PLAYERKO` |

That is the only fail path. No timer, no escortee. `MissionSetAutoRestart(true)` is set at fight start and switched off in both end branches; retry replays from B0. `CB_PlayerDead` exists but is never registered (dead code). The crowd/ambience is rebuilt on every attempt by `MissionSetup`.

Cleanup (`MissionCleanup`) restores: punishment system on, open-gate entity restored, anim groups unloaded, crowd deleted, ped generation on, pickup respawn distance restored, camera defaults, **player moved to `_1_B_BOOKIE`** (the pit's bookie spot — post-fight world position), HUD components 0/11/12 restored, Russell AI attributes reverted to `Default`, `1_B.DAT` unloaded.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0, before arena | baked (`1-B`) | `fade` in at the pit + `letterbox` + 1-2 original `say` beats (Gary leads Jimmy to the pit), or straight into B1 |
| CS-2 | B4, 50 % health | none fired in live Lua (only the anger flag) — the `1-BB` slot exists | optional: 2-3 s `letterbox` + `shake` + Russell roar bark + crowd hush, then resume |
| CS-3 | B6 win | baked (`1-BC`) | `letterbox` + fixed `camera` on Russell down + original short VO/`say` beat, `fade` to rewards |
| CS-4 | B8 lose | baked (`1-BB` from fail branch) | fade + fail `banner`; the cutscene is optional for us |

Cutscene-only anim groups: none loaded beyond the fight set (`NIS_*` absent here); the crowd uses the shared `AI.act` cheering node.

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 75 | Russell (boss) | `STAT_BOSS_RUSSELL`, damage-taken multipliers, boss action tree + AI tree, health bar |
| 35, 37, 38, 23, 25, 29, 12, 19, 14, 99, 130, 85, 6, 10, 5 | 15 crowd spectators | stationary, cheap, non-targetable; 99 = `GN_Bully02`, 85 = `GN_Bully03`, 130 = Gary (opening-slice doc ids); cheer sfx table places named characters at points: Gary at `_1_B_CROWD15`, Lola at `09`, Gord at `03`, Cornelius at `20`, Trent at `14`, Algie at `21` — the model↔character join for the rest is **OPEN** |

### Animation groups / action trees

`LoadAnimationGroup`: `Russell`, `STRAF_WREST`, `C_Wrestling`.
`LoadActionTree`: `Act/AI/AI_RUSSEL_1_B.act`; trees referenced by string: `/Global/BOSS_Russell` (`Act/Anim/Boss_Russell.act`), `/Global/AI/ScriptedAI/CheeringAINode` (`Act/AI/AI.act`), ground-recovery nodes under `/Global/HitTree/...`.

### Props / pickups / weapons

- Persistent geometry swap: `FightPit_DoorClose` ↔ `FightPit_DoorOpen` at ~(-771.0/-771.4, -127.04, 8.80), area 8 — mission-scoped arena seal.
- `PickupSetIgnoreRespawnDistance(true)` during the fight (pit pickups behave mission-scoped, **INFERRED**).
- No weapons given to the player; inventory snapshot/restore guards the player's loadout.

### Speech

Scripted: Russell barks `M_1_B` ids 2, 8, 10, 12 (**4 distinct lines**, random-weighted). Crowd: 7 named cheer sfx entries played positionally + ambience loop `Fight_Group_Sml_Int.rsm`. Music: `MS_RussellInTheHole.rsm` + pass/fail stingers. Everything else lives in the baked cutscenes.

### Special entities

`CreatePersistentEntity` / `DeletePersistentEntity` on the fight-pit gate (world-state swap, mission-scoped). No `AreaLoadSpecialEntities` here (that mechanism is used by `SMissPass` for Halloween elsewhere).

### Named world data to extract from `1_B.dat`

- POINTLISTs: `_1_B_PLAYER`, `_1_B_RUSSELL`, `_1_B_BOOKIE`, `_1_B_CROWD02`, `_1_B_CROWD03`, `_1_B_CROWD04`, `_1_B_CROWD08`, `_1_B_CROWD09`, `_1_B_CROWD10`, `_1_B_CROWD11`, `_1_B_CROWD12`, `_1_B_CROWD13`, `_1_B_CROWD14`, `_1_B_CROWD15`, `_1_B_CROWD16`, `_1_B_CROWD19`, `_1_B_CROWD20`, `_1_B_CROWD21`.
- PATHs / TRIGGERs: none referenced in the Lua.
- Hardcoded coords: gate entities (-771.439, -127.039, 8.801) closed / (-770.009, -127.039, 8.801) open.
- Area codes seen: **8** (fight pit) — new for the chapter docs; add to the area-code table.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| B0/B6/B8 cutscenes | `fade`, `letterbox` | **cutscene_skip_on_retry** (attempt-aware gate) |
| B1 arena seal | — | **geometry_swap** (persistent entity open/closed state, mission-scoped, restored on cleanup) |
| B1 crowd ring | `spawn_enemy` (stretch) | **crowd_ring**: spawn N stationary non-targetable spectators with cheer idle + positional random barks + ambience loop |
| B2 boss setup | `spawn_enemy` | **boss_profile**: named stat block, per-class damage-taken multipliers, ignore-stimuli, **health_bar(name)**; **player_buff** (min health, targeting lock, inventory snapshot/restore) |
| B4 phase change | — | condition **npc_health_pct_below(target, pct)** (non-terminal, sets flag / fires event) |
| B5 taunts/berserk/ground-recovery | — | **boss_bark_loop** (event-driven scripted barks with hush-the-crowd side effect); berserk flag + rising-attack-on-grounded belong to the boss AI brain, not JSON |
| B6 win chain | `reward`, `event` | conditions **npc_ko(target)**; reward extensions: **set_faction_respect**, **unlock_collectible** (yearbook), **force_complete_mission** (1_11x2, level-1 classes), **chapter_set**, **set_restart_points** (KO default, bus points) |
| B8 lose | `banner`, `fade` | condition **player_ko** with mission fail + optional fail cutscene hook |

## 6. Open questions

- `Act/AI/AI_RUSSEL_1_B.act` and `Boss_Russell.act` internals: attack cadence, berserk trigger, unblockables, how `F_RusselGroundCheck`/`F_RusselCalm`/`F_RusselTalks` are wired as tree callbacks. The Lua gives flags and multipliers only.
- `1-BB` placement: only invoked from the fail branch in the live code; the 50 % condition (`SecondStageCutscene`) sets `gRussellAngry` but plays nothing. Either a mid-fight cutscene was cut/mis-wired, or `1-BB` is purely the fail cutscene — **OPEN** (content of `Cuts.img` not inspected).
- Meaning of player ped flag 58, `PlayerSetScriptSavedData(14, 0)`, and stat override id 13 (set to 10 by `F_RusselCalm`).
- Faction ids 1 and 5 in the win block (11 = bullies inferred from 1_03 context; the other two unverified).
- Retry behavior of the `1-B` intro cutscene (no attempt-count guard in this file).
- How the mission is offered/chained in the world (start trigger for 1_B, and what precedes it after `1_10`) — mission table lives outside the Lua.
