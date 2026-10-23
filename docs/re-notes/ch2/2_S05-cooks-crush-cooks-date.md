# 2_S05 — Cook's Crush + Cook's Date (two-stage Edna mission): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/2_S05.lua` (1503 lines) + `SMissPass.lua` (no 2_S05 case — **VERIFIED** absence) + `SZone.lua` (item/weapon ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_S05 = the Edna missions "Cook's Crush" (stage 1) and "Cook's Date" (stage 2)** — **VERIFIED** by content: anim group `"2_S05_CooksCrush"`, Edna (model 58, the cook per 1_S01's doc) as mission-critical quest giver, stage 1 = a three-item fetch (perfume from the staff room, candy box from a townie, a sedative bottle from downtown trash cans — ids below), stage 2 = defending Edna's dinner date with Dr. Watts (model 106, `watts` var — chemistry teacher, **INFERRED**) from waves of mockery while the player watches from a tree. The stage is chosen by an **external** `F_SetStage(nStage)` call before `main()` (the launcher/mission table owns it — **OPEN** how the two halves are offered as separate missions).

**IDENTIFICATION SURPRISE:** neither name appears in the canonical Chapter 2 list we were given — the list is incomplete; this one script implements both Edna missions, split by the stage parameter. Cutscenes: `"2-S05"` (stage-1 intro), `"2-S05B"` (stage-1 outro), `"2-S05C"` (stage-2 outro) — all baked.

Setting: stage 1 = school (area 2) + staff room (**area code 23** — `AreaGetVisible() == 23`; **INFERRED** = `ischool` staff lounge; new code) + downtown business area (area 0). Stage 2 = the rich area outdoors (dinner table at ~(447, 196, 8.4)).

## 2. Mission flow

Driver: `main()` waits for `nStage`, then runs stage 1 (`F_SetupMission` → `F_GetPerfume` → `F_GetDowntownStuff` → `F_ReturnItemsToEdna`) or stage 2 (`F_MeetEdnaInRichArea` → `F_EdnaWattsDinner`), each ending in its own `MissionSucceed`. Threads: `T_MissionFail` (hit-watch on Edna/Watts), `T_PedCreation`, dinner threads (`T_AnnoyanceMonitor`, `T_PlayerInTreeCheck`, `T_DialogueSpeech`), `T_DinnerFail`.

### Stage 1 — the fetch (Cook's Crush)

| # | Beat | Logic (functional) |
|---|---|---|
| C0 | **Baked "2-S05"** | Only when `nStage == 1`. DAT loaded, counter HUD 0/3 (text `2_S05_EDNA_ITEMS`), music `MS_Searching*`, trash-can props `_2_S05_TRASHCAN1/2/3` created targetable. |
| C1 | **Meet Edna** | Player to area 2 `_2_S05_PLAYERSTART`; existing ambient Edna (`shared.gEdnaID`) replaced by the mission Edna (58) at `_2_S05_OUTSIDECAFE` — mission-critical, stimuli-ignored. |
| C2 | **Item 1: perfume (staff room)** | Objective `2_S05_O1`. Two teachers (106 Watts + 64) converse in the lounge (alternating `CONVERSATION_NEGATIVE_*` barks). Player approaching adds objective `2_S05_O2` (get them away — **paraphrase**). They leave when the **school fire alarm** (`shared.gSchoolFAlarmOn`) trips **or** both wander out of `_2_S05_TEACHERAREA`; the staff door `DT_ischool_Staff` then unlocks. Player entering **area 23** spawns the **perfume pickup 490 = "Perfume"** (VERIFIED name) with a blip; lingering 30 s inside spawns a teacher who walks in (`_2_S05_TEACHERENTERPATH`) and adds **150 punishment points**; bumping a teacher also +150 (once). Pickup → counter 1/3, door relocks. If the player somehow already has 478, objective text swaps. |
| C3 | **Item 2: candy (downtown)** | Objective `2_S05_O3`. At `_2_S05_FATTY`: **Fatty** (155 — obese townie, **INFERRED** from model/context) with two friends (6, 9); spawned only once the player enters `_AMB_BUSINESS_AREA`. Approach → bark (13). Branches: **bribe** — his required gift is id 23; on gift received he hands over **478 = "ChocBox"** (VERIFIED name) and the counter ticks; **violence** — damaging him flips him and both friends hostile (line 15, blip turns red); a registered social "mug" callback (action 30) makes him drop the 478 as a pickup; **shortcut** — already owning 478 auto-completes. |
| C4 | **Item 3: sedative (trash cans)** | Objective `2_S05_O4`. Of the three trash-can props, **one random can** holds **522 = "DrugBttl"** (VERIFIED name): destroying the right can spawns the pickup; destroying any can wakes the **hobo** (157, armed with 327 = bottle, VERIFIED name): he breaks the bottle (`Anims/Break`), attacks, then pukes (`Drink/Puke/Load`). Wrong cans are removed from the table when destroyed. Got 522 → counter 3/3. |
| C5 | **Return to Edna** | Objective `2_S05_O5`; Edna respawned at the cafe (model **221 if chapter == 2, else 58** — a dressed-up Edna variant, **INFERRED**). Player within 7 m: fade, **baked "2-S05B"**, then pass banner + pass music and `MissionSucceed(true, false, false)`. Punishment zeroed at hand-in. |

### Stage 2 — the dinner defense (Cook's Date)

| # | Beat | Logic (functional) |
|---|---|---|
| D0 | **Setup** | No intro cutscene (stage 2 skips `"2-S05"`); punishment system **disabled**; `MS_Candidate.rsm` stream; speech trees muted; debug-start transitions to `_2_S05_DINCLIMBPOINT`. |
| D1 | **Take the vantage (30 s)** | Objective `2_S05_O6` + blips on the climb point; `MissionTimerStart(30)`; player must be inside `_2_S05_TREE` **and in a stealth prop** (`PlayerIsInStealthProp` — the tree, **INFERRED**). Timeout → fail `2_S05_FAILTIME`. |
| D2 | **Dinner NIS (in-engine)** | Population cleared; Edna (221) and Watts (106) spawn at the dinner table, made untargetable and friendly; camera rides paths `_2_S05_CINDIN1/2`; Watts walks `_2_S05_DINWATTSSIT` and sits (last-node callback); seating props (`pxSitStl` chairs around the table) disabled by prop flags; one Edna line (18). Objective `2_S05_ODATE1`. |
| D3 | **Annoyance defense (4 waves + dialogue opera)** | **Annoyance meter = Edna's health bar**: max 1000, quiet mode; disturbers inside `_2_S05_DISTURBRADIUS` add their rate (3-4 each, hard-capped at 8/tick), the meter decays 15/tick when clear. Waves walk in along `_2_S05_DINANNOYPATH1-9` from `_2_S05_DINANNOYSPAWN` and **gawk** at the table (face Edna, `Disturb/Gawk` loop, a jeer line at path node 2 from a 16-entry model→line table, some cross-mission): girls (67/14/38/39), then nerds (9/11 + reinforce 4/5/6/11 after 4 s), preps (30/34 + reinforce 32/40/31/30/34 after 5 s), jocks (20/15/13 + reinforce 17/13/16/15/20 after 5 s) — reinforcements only while the meter isn't full. Hitting a disturber below his threshold (80-90% HP) makes him flee (`_2_S05_DINPEDFLEE`) and clears his contribution; **stinkbombs (309) are special-cased** (a stinkbombed ped stops gawking). Between waves, a **queued two-hander dialogue** between Edna and Watts plays (~29 lines: ids 22-32, 82-89). A mascot gag exists (`F_GoMascotGo`, model 88, `MascotDance`) but is **never called** — dead code (**INFERRED** cut content). |
| D4 | **Fail or pass** | Meter full → Edna rage quits (lines 75/76, both stand, she storms off along the flee path, disturbers scatter, Watts line 77) → fail `2_S05_FAILMAD`. Leaving the tree → 30 s re-entry timer with blip (`T_PlayerInTreeCheck`), expiry fails. Hitting Watts/Edna → `2_S05_HITTEACH`; Edna mission-critical → `2_S05_FAILEDNA`. **Pass:** the dialogue queue drains after the jocks wave → `2_S05_ODATE1` completes → **baked "2-S05C"** → pass music, `MissionSucceed(false, true, true, 5000)`. A money pickup (462, `_2_S05_DOLLAR` — **INFERRED** a cash tip; 462 not in the `SZone` table read) is created after the dinner. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Edna hit/KO'd (mission-critical) | `cbFail` / `T_MissionFail` | `2_S05_FAILEDNA` |
| Hit Watts or Edna at the dinner | hit-watch thread | `2_S05_HITTEACH` |
| Vantage timer (30 s) expires | `MissionTimerHasFinished` | `2_S05_FAILTIME` |
| Out of the tree 30 s during dinner | `T_PlayerInTreeCheck` | (no key — `MissionFail(false)`) |
| Annoyance meter maxed | `T_AnnoyanceMonitor` | `2_S05_FAILMAD` |

Stage 1 fails use `MissionFail(true, true, key)` (fade + retry); the dinner fail thread uses `MissionFail(false, true, key)`. Retry-skip of the baked cutscenes not explicit — **OPEN**.

### Rewards / pass side-effects

None in `SMissPass.lua` (**VERIFIED** absence). In-script: the 462 money pickup after stage 2 (**INFERRED** reward); items 490/522 zeroed in cleanup; stage-1 pass is a standalone `MissionSucceed`.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | C0 `"2-S05"` | baked | `fade` + `letterbox` + 1-2 original `say` beats (Edna's request) |
| CS-2 | C5 `"2-S05B"` | baked | `fade` + `completion_banner` |
| CS-3 | D2 dinner setup | in-engine, ~6 s | `camera` path pair + `npc_follow_path` + seat `prop` flags + 1 `say` |
| CS-4 | D4 Edna rage | in-engine, ~5 s | 2 `say` + `pair_anim`-ish stand-up nodes + flee paths; no letterbox needed |
| CS-5 | D4 `"2-S05C"` | baked | `fade` + `completion_banner` |

Cutscene anim groups: `2_S05_CooksCrush`, `Hobos` (hobo set), `Px_Ladr`, `Px_Tree` (stage-2 climb/vantage props).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 58 / **221** | **Edna** (stage-1 / chapter-2-dressed variant) | mission-critical; 221 **INFERRED** date outfit |
| 106 | **Dr. Watts** | teacher in the lounge (C2) + dinner date (D2) |
| 64 | second teacher (lounge) | grapple-paired with 106 for the convo pose |
| 151 | staff-room teacher (table entry) | spawned via `_2_S05_TEACHERSPAWN` variant paths |
| 155 | **Fatty** (candy holder) | required gift 23; mug callback drops 478 |
| 6, 9 | Fatty's backup | turn hostile on the violence branch |
| 157 | hobo (trash-can guard) | bottle 327; attack → puke |
| 88 | mascot | **dead code** (`F_GoMascotGo` never called) |
| 67/14/38/39, 9/11/4/5/6, 30/34/32/40/31, 20/15/17/13/16/12 | dinner disturbers (girls/nerds/preps/jocks) | path-in, gawk, jeer line, flee under threshold |

### Animation groups / action trees

`LoadAnimationGroup`: `2_S05_CooksCrush`, `Hobos`, `Px_Ladr`, `Px_Tree`.
`LoadActionTree`: `Act/Conv/2_S05.act` (drink/break/puke/gawk/mascot/date nodes); social overrides `Mission/2_S05WantGift.act` (24) and gift give flows.

### Props / pickups / weapons

- Items: **490 Perfume**, **478 ChocBox**, **522 DrugBttl** (all VERIFIED names), 462 (dollar pickup, **OPEN**), 475 check not present here.
- Props: trash cans `_2_S05_TRASHCAN1/2/3` (one random holds 522); dinner chairs `pxSitStl` ×16 (prop-flag disabled); tree stealth prop (`_2_S05_TREE`/`_TREE2`).
- Weapons: 327 bottle (hobo), 309 stinkbomb (player tool special-cased), 320/… none granted.
- Staff door `DT_ischool_Staff`.

### Speech

`M_2_S05` ids: 13, 15, 16, 18, 22-32, 33, 50, 51, 55, 58, 68, 69, 75, 76, 77, 82, 84, 85, 87, 88, 89, 92 — **31 distinct scripted lines**, plus cross-mission jeers (`JEER` 0, `M_2_06` 19, `M_2_08` 49, `M_4_G4` 10, `M_3_02` 7, `M_2_G2` 23, `M_4_04` 12), `GIFT_RECEIVE`, and the `CONVERSATION_NEGATIVE_*` lounge pair. Music: `MS_Searching*` (stage 1), `MS_Candidate.rsm` (stage 2).

### Shared-state contract

`shared.gEdnaID` (ambient Edna swap), `shared.gSchoolFAlarmOn` (fire alarm empties the lounge), `shared.quit_minigame`-adjacent flags not used here.

### Named world data to extract from `2_S05.DAT`

- POINTLISTs: `_2_S05_PLAYERSTART`, `_2_S05_OUTSIDECAFE`, `_2_S05_PERFUME`, `_2_S05_PLAYERKICKEDOUT` (1-2), `_2_S05_TEACHERSPAWN`, `_2_S05_FATTY` (1-3), `_2_S05_DRUGCANS`, `_2_S05_BUM`, `_2_S05_DINCLIMBPOINT`, `_2_S05_DINNEREDNA` (1-2), `_2_S05_DINANNOYSPAWN` (1-6), `_2_S05_DOLLAR`, `_2_S05_ENDPOS` (1-2), `_2_S05_SURPRISE`, `_2_S05_BPLAYERSTART` (debug), `_2_S05_STAFFROOMTEACHERS` (1-2).
- PATHs: `_2_S05_TEACHERENTERPATH`, `_2_S05_CINDIN1`, `_2_S05_CINDIN2`, `_2_S05_DINWATTSSIT`, `_2_S05_DINPEDFLEE`, `_2_S05_DINANNOYPATH1`..`9`, `_2_S05_SURPRISE`.
- TRIGGERs: `_2_S05_TEACHERAREA`, `_2_S05_DRUGALLEY`, `_2_S05_TREE`, `_2_S05_TREE2`, `_2_S05_DISTURBRADIUS`, `_2_S05_TRASHCAN1/2/3` (props), `_AMB_BUSINESS_AREA`, `_2_S05_TRASHCAN*` PAnim anchors.
- Hardcoded coords: dinner NIS camera (444.48, 197.45, 10.13 → 450.30, 197.41, 9.73); 16 chair prop positions around (445.3-450.0, 192.5-198.0, 8.38); stage-1 end camera (-638.12, -276.95, -0.39).
- Area codes seen: **2**, **23** (staff room, INFERRED), **0**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| Two halves | mission framework | **multi_stage_mission_param** (external `F_SetStage` picks which half runs; separate cutscene set, pass and reward per stage) |
| C2 staff room | `pickup_spawner`, `door_prop_control`, `conversation_beat_machine` (lounge pair), `punishment_points_add` (2_S06 doc) | **global_fire_alarm_hook** (`shared.gSchoolFAlarmOn` empties the lounge — same family as 1_S01's `gAlarmOn`) |
| C3 candy branch | `give_item_scene` (required gift 23), `ped_hit_event` → hostility flip, `drop_pickup_at_ped` (mug drop), `player_has_item` shortcut | **bribe_or_beat_branch** is just composition — no new primitive |
| C4 trash cans | `object_destroyed` | **randomized_loot_prop** (one random prop among N contains the pickup; wrong destroys trigger a guard NPC's attack + gag) |
| D1/D3 vantage | `stealth_prop_entered` (ch1), `countdown_fail_timer` | — |
| D3 meter | `health_bar(name)` (quiet mode) | **annoyance_meter** (NPC health bar repurposed as a patience meter: fill = Σ per-disturber rates inside a trigger (capped), fixed decay, fail at max; meter hidden from normal HUD) |
| D3 disturbers | `spawn_npc` bundles, `npc_follow_path` + node cb (jeer at node 2), `npc_flee` at HP threshold | **crowd_gawk_disturbers** (path-in → face target → gawk loop → contribute to meter → flee when hurt; weapon-specific reaction exception — stinkbomb) |
| D3 dialogue | `conversation_beat_machine` | — (the `tblDatalogue` FIFO + mutual-exclusion speaker check is that primitive) |
| D2 seating | — | **seat_prop_disable** (prop flags on chair instances by position) |

## 6. Open questions

- How the launcher splits stages (two mission-table entries pointing at one script? `F_SetStage` caller not in this file).
- What item 462 is (name not in the `SZone` range read) and whether stage 1's pass has any persistent effect (none visible).
- Gift id 23 (Fatty's required gift) mapping to the gift table.
- `PedSetStealthBehavior` modes and `PlayerIsInStealthProp` internals (engine-side, undecoded — as in 1_S01/2_S06).
- The unused `F_StartTimer`/`F_StopTimer` 5 s timer pair and `F_GoMascotGo` — dead code, presumably cut beats.
- Whether wrong-can hobo aggro can soft-lock the drug fetch (cans removed from the table when destroyed, the right can persists until smashed — looks safe).
