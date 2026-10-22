# 1_S01 — Hattrick vs. Galloway: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/1_S01.lua` (702 lines) + `SMissPass.lua` (`F_SetupMissionRequirements`) + `SZone.lua` (weapon id table). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_S01 = "Hattrick vs. Galloway"** — **NOT "Character Sheets"** as initially guessed. **IDENTIFICATION SURPRISE.** VERIFIED by content: the player sneaks through the school collecting **3 hidden bottles** (item 529, HUD counter icon `bagged_bottle`) — a bathroom stall, the cafeteria kitchen, and the locked **trophy case** — while math teacher **Hattrick** (ped 61, `TE_MathTeacher` per the opening-slice doc) and the **Principal** (ped 65) patrol and converse, **Edna** the cook (58) guards the kitchen, a **hall monitor** (54) searches the bathroom stalls, and a student (14, Mandy — **INFERRED** from point name `_1_S01_BROOM_GIRL` + context) is at the bathroom mirror. The bottles are delivered to **Ms. Phillips** (63, art teacher) waiting at the school bus. The reward is weapon **328 = "camera"** (**VERIFIED**: `SZone.lua` weapon table entry `{ model = 328, name = "camera", inventory = true }`). This is the Scholarship-Edition-added Chapter 1 mission given by Ms. Phillips; it does not appear in the canonical list we were given. ("Character Sheets" is a different mission; its script id was not checked here — **OPEN**, not 1_S01.)

Setting: starts in area 2 (`ischool`), ends outdoors in area 0 at the school bus (~(179.7, 3.2, 6.4)).

## 2. Mission flow

Driver: `MissionSetup()` → `main()`'s `gMissionRunning` polling loop, which ticks per-station function pointers (`gBathBottleFunction`, `gCafeBottleFunction`, `gTrophyBottleFunction`, `gReturnBottleFunction`) — the three bottles can be collected in **any order**. Threads: `T_HattrickSpeech` (patrol conversation), `T_BathroomMonitor` (stall search).

| # | Beat | Logic (functional) |
|---|---|---|
| H0 | **Baked cutscene "1-S01"** | `PlayCutsceneWithLoad("1-S01", true)` in `MissionSetup`. `1_S01.DAT` loaded, POI disabled, `shared.gEdnaOverride` set, models 57/63/54 made unique, anim groups `Hang_Talking`/`F_Adult`/`NPC_Adult`/`WeaponUnlock` loaded, camera+bottle models (327, 328) preloaded. |
| H1 | **Setup** | Player transitioned to area 2 (`_1_S01_PLAYER_START`), ambient peds cleared, interactive music `MS_InTrouble*`, punishment zeroed. Objective `1_S01_42` (find the bottles, **paraphrase**) + **HUD counter 0/3** with bottle icon. Bottle pickups created: bathroom bottle at `_1_S01_2ND_BOTTLE_BATH`, cafeteria bottle at `_1_S01_3RD_BOTTLE_CAFE`; area blips for all three stations. The trophy-case bottle does **not** exist yet — it spawns when the case glass breaks. Six trophy-glass props (`DPI_TrophyGlsA/B/C`, hardcoded coords ~(-627..-629, -293.9, 6.5-7.2)) registered for tracking. |
| H2 | **Bathroom bottle** | Entering `_1_S01_BATHROOMSETUP` spawns Mandy (14) at `_1_S01_BROOM_GIRL` playing a mirror loop. If the player enters `_1_S01_BATHROOM`, or Mandy is hit, or the fire alarm is on (`shared.gAlarmOn`): she flees along `PATH._1_S01_BATHROOM_GIRL_PATH` (bark), objective `1_S01_02` added, and a **hall monitor** (54) spawns at `_1_S01_BROOM_PREFECT` patrolling `PATH._1_S01_BROOM_PREFECT_PATH2`; at path node 4 he starts a stall-by-stall search (`T_BathroomMonitor` along `_1_S01_BROOMSTALLS`, opening stall doors `_STALDOOR06/07`, barking `WARNING_COMING_TO_CATCH`/`CHASE`/`STEALTH_INVESTIGATING`/`STEALTH_CONFUSION`); if his stealth behavior spots the player he attacks (bust attempt). Picking up the bottle: counter +1, objective complete. |
| H3 | **Cafeteria bottle** | Entering `_1_S01_CAFE`: objective `1_S01_01` added; a kitchen **guard** (50) spawns at `_1_S01_EDNA_GUARD` patrolling `_1_S01_EDNA_GUARD_PATH` with perception stat overrides; **Edna** (58) spawns at `_1_S01_EDNA`, tethered to `_1_S01_KITCHEN`, patrolling `_1_S01_EDNA_PATH`, with stealth behavior: if she sees the player inside the kitchen trigger she yells (`M_4_06` 8) and sends the guard at the player. Edna gossips on an 8 s ambient timer (`CONVERSATION_GOSSIP*`). Bottle pickup: counter +1, objective complete. |
| H4 | **Trophy-case bottle** | Entering `_1_S01_TROPHYSETUP` (or glass already broken): the **Principal** (65) and **Hattrick** (61) spawn at `_1_S01_CRABBLE` elements 1/2, facing each other, running a scripted multi-beat conversation (speech ids 1,2,3,5 then 27-30,4 then 8-11 — content not reproduced; functionally: Hattrick is sniffing around for something). Hattrick then patrols `_1_S01_HATTRICKPATH01` → convo → `PATH02` → convo → `PATH03` toward the cafeteria (node 23 despawn both). Breaking the case glass (`_1_S01_GLASS` PAnim destroyed count > 0) spawns the bottle pickup at `_1_S01_BOTTLE3_CASE`; if Hattrick is within 10 m he **hears it** (bark `M_4_06` 27) and hurries down path 01 to investigate. If Hattrick's stealth behavior spots the player **after at least one bottle is held or glass was heard breaking** (`Cb_HattrickSeePlayer`): conversation cut, Hattrick attacks the player (line 36) and goes ambient, Principal goes ambient. Bottle pickup: counter +1, a static bottle geometry instance (`1S01_bbagbottle`) appears in the case spot, objective `1_S01_03` completes. |
| H5 | **All three → return objective** | At 3 bottles: first objective completes, counter HUD hidden, objective `1_S01_56` added (deliver to Phillips, **paraphrase**). Ms. Phillips (63) spawns outdoors at `_1_S01_PHILLIPS`, stationary, faction 9, invulnerable-ish flags, mission-critical — **hitting her fails the mission** (`CbPlayerAggressed`, key `1_S01_04`); her required gift is id 17 (the bottle bundle, **INFERRED**). Outdoor punishment alert levels reset, patrol paths re-enabled. |
| H6 | **Handoff NIS at the bus** | Player within ~3.5 m of Phillips (plus `PedGetFlag(gPlayer, 1)` — player-state flag, meaning **OPEN**): invulnerable scene guard, control off, widescreen, bike dismount; a **hobo** (87) walks by on `PATH._1_S01_HOBO_PATH` as a comic beat; fixed camera at (179.7, 3.2, 6.4); the give anim (`GIVE_B/GiveMissPhillips1_S01`) plays while four alternating player/Phillips lines (53, 18, 19, 99) are sequenced by speech-end polling. Then the **bus doors open** (`_BUSDOORS`), Phillips walks `PATH._1_S01_PHILLIPS_PATH` to the bus, player plays a `Success` anim with a camera offset, pass banner (`M_PASS`, tag `1_S01_REWARD`), pass music. |
| H7 | **Pass + reward** | `MissionSucceed(false, false, false)`; the player is equipped with weapon **328 (camera)** in-script, and `SMissPass.lua` (`F_SetupMissionRequirements("1_S01")`) grants it again if missing — **the camera is the mission reward** (VERIFIED id→name). Cleanup: bus doors closed, Phillips and hobo deleted, item 529 cleared, POI re-enabled, model uniqueness restored. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player attacks Ms. Phillips | `CbPlayerAggressed` mission-critical callback | `1_S01_04` |
| Generic | `gMissionFail` with no message set | `MissionFail()` no key |

Notably, **being spotted is not a Lua-level fail**: Hattrick, the hall monitor and Edna's guard attack the player (bust attempts); whether a successful bust fails or ejects the mission is punishment-system behavior — **OPEN**. No explicit retry/cutscene-skip logic in this file (**INFERRED** global attempt handling for `1-S01`).

### Rewards / pass side-effects

- **Weapon 328 = camera** (VERIFIED). This is the camera unlock for the yearbook/photography systems.
- No chapter change, no force-completed missions, no outfit, no collectible flags in `SMissPass.lua` for this id (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | H0, before control | baked (`1-S01`) | `fade` + `letterbox` + 1-2 original `say` beats (Phillips explains the errand), or a title card |
| CS-2 | H4 teacher conversations | in-engine (path + speech beats) | keep as ambient NPC behavior: `npc_follow_path` + timed `say` at waypoints; interruptible on alert |
| CS-3 | H6 bus handoff | in-engine (~15 s) | `letterbox` + fixed `camera` + give pair-anim + 4 `say` beats + hobo walk-by (`spawn` + `npc_follow_path`) + `banner` |

Cutscene-only anim groups: none beyond the talk sets (`Hang_Talking`, `F_Adult`, `NPC_Adult`) and `WeaponUnlock` (reward grant anim, **INFERRED**).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 61 | Hattrick (math teacher) | patrol conversation + investigate + attack-on-spot; `TE_MathTeacher` per opening doc |
| 65 | Principal Crabblesnitch | conversation partner, follows Hattrick beats, despawn at cafeteria |
| 58 | Edna (cook) | kitchen tether, stealth spotter, gossip barks |
| 50 | kitchen guard | patrol, high perception stats (overrides 3→12, 2→70), attacks on Edna's call |
| 54 | hall monitor | bathroom patrol → stall search → attack-on-spot |
| 14 | bathroom student (Mandy, **INFERRED**) | mirror loop, flees when player enters / is hit / alarm |
| 63 | Ms. Phillips (art teacher) | mission-critical recipient at the bus |
| 87 | hobo | handoff-scene walk-by only |
| 57 | unique-ified, never spawned (**OPEN** why) | |

### Animation groups / action trees

`LoadAnimationGroup`: `Hang_Talking`, `F_Adult`, `NPC_Adult`, `WeaponUnlock`, `Px_Sink` (mirror sink anim, bathroom dressing).
`LoadActionTree`: `Act/Conv/1_S01.act` (all mission conv nodes incl. `1_S01_Bathroom_Loop`, `TalkingLoops`, `GiveMissPhillips1_S01`, `Success`, `ClearNode`).

### Props / pickups / weapons

- Item **529 = bagged bottle** (VERIFIED by counter icon `bagged_bottle` + context) — 3 pickups, `PermanentButes`.
- Trophy case: PAnim glass `_1_S01_GLASS` + six `DPI_TrophyGlsA/B/C` pool props (hardcoded coords); static geometry `1S01_bbagbottle` toggled on collection.
- Stall doors `_STALDOOR06`, `_STALDOOR07` (PAnim open during the search).
- Bus doors `TRIGGER._BUSDOORS` (opened for the ending).
- Weapons: 327 ("bottle", melee prop — preloaded; also used by 2_S05's hobo), **328 = camera (reward)**, 426 digital camera exists elsewhere — not here.
- `LoadModels({54, 63, 87, 14, 50, 58, 65, 61, 529})`.

### Speech

`M_1_S01` scripted ids: 1, 2, 3, 4, 5, 8, 9, 10, 11, 18, 19, 27, 28, 29, 30, 36, 53, 99 — **18 distinct lines**; plus cross-mission barks `M_4_01` 7, `M_4_06` 8/27, `CONVERSATION_GOSSIP`/`_CHAPTER_1`, and ambient stealth barks (`WARNING_COMING_TO_CATCH`, `STEALTH_PURSUIT`, `CHASE`, `STEALTH_INVESTIGATING`, `STEALTH_CONFUSION`). Music: `MS_InTrouble*` interactive streams.

### Special entities

None via `AreaLoadSpecialEntities`. Global flags touched: `shared.gEdnaOverride`, `shared.gAlarmOn` (fire alarm modifies the bathroom beat — Mandy leaves without the objective add).

### Named world data to extract from `1_S01.dat`

- POINTLISTs: `_1_S01_PLAYER_START`, `_1_S01_2ND_BOTTLE_BATH`, `_1_S01_3RD_BOTTLE_CAFE`, `_1_S01_BOTTLE3_CASE`, `_1_S01_BATHROOM_BLIP`, `_1_S01_CAFE_BLIP`, `_1_S01_BOTTLE3_BLIP`, `_1_S01_BROOM_GIRL`, `_1_S01_BROOM_PREFECT`, `_1_S01_EDNA`, `_1_S01_EDNA_GUARD`, `_1_S01_CRABBLE` (elements 1-2), `_1_S01_PHILLIPS`, `_1_S01_HOBO`, `_1_S01_PLAYERENDING`.
- PATHs: `_1_S01_BATHROOM_GIRL_PATH`, `_1_S01_BROOM_PREFECT_PATH2`, `_1_S01_BROOMSTALLS`, `_1_S01_EDNA_PATH`, `_1_S01_EDNA_GUARD_PATH`, `_1_S01_HATTRICKPATH01`, `_1_S01_HATTRICKPATH02`, `_1_S01_HATTRICKPATH03`, `_1_S01_PHILLIPS_PATH`, `_1_S01_HOBO_PATH`.
- TRIGGERs: `_1_S01_BATHROOMSETUP`, `_1_S01_BATHROOM`, `_1_S01_CAFE`, `_1_S01_KITCHEN`, `_1_S01_TROPHYSETUP`, `_1_S01_BOTTLE_TRIG_CASE`, `_1_S01_GLASS` (prop), `_1_S01_PLAYERBLOCKING`, `_STALDOOR06`, `_STALDOOR07`, `_BUSDOORS`.
- Hardcoded coords: trophy glass props ~(-627.1..-629.4, -293.856, 6.56-7.18); bottle geometry (-627.619, -293.51, 6.55437); bus scene camera (179.7372, 3.151522, 6.393147 → 179.32288, 4.061574, 6.403118); bus walk target (166.04, 18.9094, 6.294).
- Area codes seen: **2** (bottles) and **0** (bus/Phillips).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| H1 collect 3 | `interact` (per pickup) | **collect_counter** (HUD icon + n/max, increment on pickup event), **pickup_spawner** (permanent mission pickups + area blips) |
| H2 stall search | — | **guard_search_patrol**: path-node callbacks, open-door props per node, bark sequence, give-up node |
| H2/H3/H4 stealth guards | — | **stealth_spotter**: cone/behavior with on-spot callback, conditional arming (Hattrick only after ≥1 bottle / glass heard — condition **flag + item_count**), alert cascade (bark → send guard → attack) |
| H3 kitchen guard pair | — | **npc_tether_trigger**, stat overrides (**npc_stat_override**), call-for-help link between two NPCs |
| H4 glass break | `event` | **prop_destroyed_event(prop, count)** → spawn pickup at point; **npc_heard_event** (distance check from noise) |
| H4 patrol conversation | `say` | **conversation_beat_machine**: ordered speech lines across two NPCs pinned to path progress, interruptible |
| H4/H6 geometry | — | **geometry_instance_toggle** (bottle appears), **door_prop_control** (bus doors, stall doors) |
| H5/H6 recipient | `banner` | **give_item_scene** (pair anim + required gift), **mission_critical_npc** aggression fail, condition **player_near_ped + has_items**, hobo walk-by = `spawn` + `npc_follow_path` |
| H7 reward | `reward` | **give_weapon** (camera) — also in 1_03's table |

## 6. Open questions

- Stealth behavior internals (`PedSetStealthBehavior` cone sizes, reaction times, what "attack (bust)" does to the player vs. the mission) — engine-side, undecoded.
- The `PedGetFlag(gPlayer, 1)` gate at the handoff (some player-state check — **OPEN**).
- Why the mission starts the player with `ItemSetCurrentNum(529, 1)` (one bottle "held" as the example/bundle?) vs. the counter starting at 0 — likely the quest-item bundle representation, **OPEN**.
- Hattrick's patrol timing budget (8 s bathroom linger, path node indices 3/8/23 as beat markers) — coordinates pending DAT extraction.
- Fire-alarm interaction (`shared.gAlarmOn` short-circuits the bathroom beat) — owned by the alarm system, **OPEN** scope.
- Whether a bust during the mission fails it (no arrest override set here, unlike 1_02B) — punishment system owns this, **OPEN**.
- How the mission is offered (Ms. Phillips in the art room, hours — mission table outside the Lua) and where "Character Sheets" actually lives (not 1_S01).
