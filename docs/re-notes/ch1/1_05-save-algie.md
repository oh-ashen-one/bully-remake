# 1_05 recon: Save Algie (escort Algie: courtyard fight -> school bathroom -> locker)

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED** (read in the script), **INFERRED**, **OPEN**. No dialogue, narrative text, cutscene script or objective string text is reproduced; script/file/native names and string *keys* are citations only.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_05.lua` (1576 lines, read in full), `SMissPass.lua`, `SFaction.lua`, `main.lua`. World data lives in the install's `1_05.DAT` + `1_05b.DAT` (both `DATLoad`ed by this script); coordinates were not read.

## 1. Identification

**This is "Save Algie"** (canonical Chapter 1 list, available 8-19) — **not** "A Little Help" as the pre-existing mapping in our notes claimed. Evidence:

- The entire mission is escorting the ped spawned with model id **4** (Algie; the variable is `pedAlgie`, speech event `M_1_05`) from the school entrance, defending him from bully packs, through the school interior to a bathroom (he has a bladder emergency, a pee-dance loop, and a fail state where he wets himself), then to his locker where he retrieves books and pays the player.
- The mission pays **$5** (`MinigameSetCompletion("M_PASS", true, 500)` = 500 cents, **INFERRED**) plus faction respect shifts — the known reward profile of Save Algie.
- `main.lua`'s Halloween chain gates the Halloween arc on `IsMissionCompleated("1_05")`, consistent with Save Algie sitting early in the chapter.

Confidence: **VERIFIED** by content. Note the time-window cross-check: canonical list says Save Algie 8-19; `MissionCleanup` unlocks the school doors again only when the clock reads before 19:00, consistent.

## 2. Mission flow

Top-level chain: `main -> F_Intro -> F_Stage1 (loop) -> F_Stage2 -> F_Stage3 -> pass in F_NIS_AlgieLocker`. Background threads: `T_MonitorAlgie` (idle/whine/despawn watcher), `T_MonitorPlayerLocation` (area-change door handling), `T_Stage2_Locates` (interior triggers), `T_BathroomAntics` (stall/privacy logic), `T_MonitorAlgiePee` (pee sound anim state).

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup + cutscene 1-05** | Interactive music streams armed (`MS_FriendshipAlly*`); baked cutscene `1-05` plays (`PlayCutsceneWithLoad("1-05", true)`); `1_05.DAT` + `1_05b.DAT` loaded; control off, no auto fade-in. All peds cleared; one prefect patrol path disabled (`PATH._SGD_PREFECT2`); nerd population lowered (`AreaOverridePopulationPedType(1,2)`); nerds made hostile-social to... (`PedSetTypeToTypeAttitude(1,13,4)`, **OPEN** exact semantics); ped type 11 (bullies) mission-disabled then set hostile to player (`PedSetTypeToTypeAttitude(1,11,2)`). Player is placed at `POINTLIST._1_05_SPAWNPLAYER` in area 0. |
| S1 | **Meet Algie / objective: get to school** | Objective `1_05_MOBJ_01` added, blip at `_1_05_BLIPSCHOOL`. Algie spawned at `_1_05_SPAWNALGIE` (model 4), made **mission-critical** (KO callback -> fail), recruited as the player's ally, stats overridden (stat 6 -> 75, stat 12 -> 30). One Algie line plays. Six exterior attackers pre-spawned: first bully (model 99, infinite sprint, **invulnerable until triggered**) at `_1_05_SPAWNJOCKSNEARLIBRARY`; right pair (102, 146, both POI-pinned at `POI._POIBULLIESRIGHT`, stat-boosted) at `_1_05_SPAWNBULLIESRIGHT`; left trio (145, 146, 147; first two stat-boosted) at `_1_05_SPAWNBULLIESLEFT`. (Names say "jocks" but the model ids are the bully gang models from the opening doc.) |
| S2 | **First attacker** | Player entering `TRIGGER._1_05_TRIGGERJOCKSNEARLIBRARY` starts the first bully along `PATH._1_05_ROUTEFIRSTBULLY`; at path node 2 he attacks Algie; objective `1_05_MOBJ_00` (defend Algie) is added, bully blips appear, school blip removed. **Early aggro:** if the player hits him first (`F_HitByPlayer` = `PedIsHit` + last-hitter check), he attacks immediately. When his health drops below half he breaks off and flees along `PATH._1_05_ROUTEBULLYFLEENEW`, and reaching node 1 force-triggers the right pair. |
| S3 | **Two flank packs** | Right pair attacks when the player enters `TRIGGER._1_05_TRIGGERJOCKSNEARFRONT` **or** hits either of them; left trio on `TRIGGER._1_05_TRIGGERJOCKPACK` or being hit. One or two of each pack attack the player, one goes for Algie; a warning line fires on `TRIGGER._1_05_TRIGGERJOCKPACKWARNING`. Each triggered group gets blips; Algie plays a line when each pack is wiped. |
| S4 | **Stage 1 clear -> head inside** | When every *aggro'd* attacker is dead (first bully + whichever packs triggered — dead-pack checks are per-triggered-group, so untriggered packs are not required), `F_UnlockStage2` runs: defend-objective completed, new objective `1_05_MOBJ_02` (escort Algie into school), blip back on `_1_05_BLIPSCHOOL`, a "Timer" tutorial popup, and a **180 s mission countdown starts** (Algie's bladder). The stage advances when `bStage1BulliesDefeated` **and** the player is inside area 2 (`bPlayerIsInSchool` via `T_MonitorPlayerLocation`). While stage 1 is unresolved, all four school entrance doors (`DT_TSCHOOL_SCHOOLRIGHTFRONTDOOR`, `SCHOOLRIGHTBACKDOOR`, `SCHOOLLEFTBACKDOOR`, `SCHOOLFRONTDOORL`) are locked whenever the player is outside; they unlock on cleanup only if the clock is before 19:00. |
| S5 | **Interior setup** | Exterior peds deleted; interior spawned: two bathroom attackers (85, 102) at `_1_05B_SPAWNJOCKSBRFIRSTFLOOR` — one idling in a wall-smoking action node — plus two ambience peds (74 Eunice, 69) at `_1_05B_SPAWNBATHROOMWIERDOS`. Blip on `_1_05B_BLIPBATHROOMSECONDFLOOR`. Stall doors `TRIGGER._STALDOOR01/02` locked; locker prop `TRIGGER._NLOCK02B06` set to unusable. Ambient population is toggled off/on by corridor triggers (`_1_05B_AREASECONDFLOORBR`, `_1_05B_FIRSTFLOORAMBIENTOFF` off; `_1_05B_LOADSMOKE`, `_1_05B_FIRSTFLOORAMBIENTON` on) and hall patrol paths disabled/enabled in step. |
| S6 | **First-floor bathroom fight** | When Algie enters `TRIGGER._1_05B_BATHROOMFIRSTFLOOR` (or the player hits either bathroom ped): Algie stops following, gets his own blip and health bar, and runs to tether point `_1_05B_ALGIETETHER`; both bathroom peds are leashed to the bathroom trigger, one attacks the player, one attacks Algie. When both are dead: Algie walks `PATH._1_05B_ROUTEFIRSTFLOORSTALL`, opens/closes stall door `_STALDOOR11`, then *reopens* it — the stall is unusable (a gross-out beat) — objective flips to `1_05_MOBJ_02B` (try upstairs), blip on `_1_05B_BLIPBATHROOMSECONDFLOOR`, Algie resumes following. |
| S7 | **Second-floor bathroom: escort cut + ambush** | When Algie enters `TRIGGER._1_05B_BATHROOMSECONDFLOOR` (and no first-floor fight is running): the **countdown stops**. In-engine beat `F_CutAlgieToStall`: control off, widescreen, player auto-walks to the bathroom point, camera jumps to a hardcoded position `(-668.40, -325.41, 6.42)` looking at `(-668.86, -326.30, 6.41)` (ischool), Algie follows `PATH._1_05B_ROUTEALGIESTALL` and closes `_STALDOOR02` behind him. Objective `1_05_MOBJ_05` (guard the door / keep the bathroom clear). Then the ambush: Eunice and the other ambience ped walk out along `_1_05B_ROUTEBATHROOMWIERDOS` and despawn at node 6; two more attackers (146, 102, unarmed) spawn at `_1_05B_SPAWNSECONDFLOORAMBUSH` and walk `PATH._1_05B_ROUTESECONDFLOORAMBUSH`, turning hostile at nodes 3-4 (with lines if the player is in the room). |
| S8 | **Privacy timer -> relief** | Once both ambushers are dead, `T_BathroomAntics` polls a 12 m sphere at `_1_05B_BATHROOMCHECK`: any living ped other than player/Algie resets the 15 s "poop timer" (`gPoopDuration`) and makes Algie whine (line 27, 20 s cadence). 15 uninterrupted seconds: flush sound, `_STALDOOR02` unlocks and opens, Algie walks `_1_05B_ROUTEALGIESTALLTOSINK`, plays a hand-wash node at node 4, stage 3 launches. (The pee audio itself is an anim-node start/stop pair in `T_MonitorAlgiePee`.) |
| S9 | **Escort to locker** | Objective `1_05_MOBJ_03`, blip `_1_05B_BLIPALGIELOCKER` (interior style blip), Algie follows. Population restored. |
| S10 | **Pass scene (in-engine)** | Player entering `TRIGGER._1_05B_ALGIELOCKER` runs `F_NIS_AlgieLocker`: fade, control off, player and Algie teleported to `_1_05B_NIS_LOCKER` elements 1-2, camera hardcoded at `(-654.12, -295.78, 6.85)` -> `(-654.76, -296.53, 6.75)`, leftover ambushers deleted. Algie walks `PATH._1_05B_ROUTEALGIELOCKER`, rummages the locker (`NLOCK02B06` prop anim `SaveAlgieBooks` + Algie rummage node), receives item **405** (his books, **INFERRED**), flees briefly along `PATH._1_05B_ALGIEFLEE`, turns back, player plays a receive-money pair anim (`/Global/Player/Gifts/GetMoney`). Then: `SetFactionRespect(11, 10)` (bullies -> 10) and `SetFactionRespect(1, 60)` (nerds -> 60), completion screen `M_PASS` 500 (= $5, **INFERRED**) with respect messages `MRESPECT_NP5`/`MRESPECT_BM10`, end music, fade, `MissionSucceed(false,false,false)`. |

**Fail conditions** (all -> `MissionFail` with distinct text keys):

1. `1_05_FAIL_01` — Algie KO'd (mission-critical callback `F_MissionCritical`).
2. `1_05_FAIL_02` — the 180 s bladder countdown expires before the second-floor bathroom (`F_AlgiePissSelf`: Algie plays a wetting-himself anim, goes ambient, mission fails). Countdown is checked in both stage 1 and stage 2 loops.
3. `1_05_FAIL_03` — clock past 23:01 while outdoors (`F_CheckClockFailure`) — **dead code, never called**; note only.

**Retry/restart:** `PlayCutsceneWithLoad("1-05", true)` — the `true` is the standard "skippable/already-seen" flag, so retries skip the intro cutscene (**INFERRED**, same convention as the opening missions). `MissionCleanup` fully restores the world: doors unlocked (pre-19:00), stall/locker props reset, population and patrol paths restored, peds made ambient, anim groups unloaded, control restored.

**Chains to:** nothing explicit; `MissionSucceed` with no next-mission parameter. Next missions start from the world.

**Shared/global side effects:** sets `shared.gCloseStallDoors = true` during the mission (global stall-door behavior); calls `PlayerSetPunishmentPoints(0)` at stage 2 entry.

### Rewards / unlock side-effects

- **In-mission:** $5 (`MinigameSetCompletion("M_PASS", true, 500)`), faction respect nerds -> 60 / bullies -> 10, completion messages `MRESPECT_NP5`, `MRESPECT_BM10`. **VERIFIED**.
- **`SMissPass.lua`:** there is **no branch for `1_05`** in `F_SetupMissionRequirements` — no weapon/item/collectible grant on pass. **VERIFIED**.
- **Halloween chain:** 1_05 completion is a prerequisite for the Halloween arc: `main.lua` `F_CreateHalloweenItems` aborts unless `1_05` is done, loads `Halloween1` special entities once `1_07`+`1_08` are also done, and the `1_06_01` branch of `SMissPass` loads `Halloween1` if `1_05` is complete. **VERIFIED**.
- `SFaction.lua` `F_MissionFactionChanges` (runs after every mission pass) does not special-case 1_05; bully/nerd default attitudes are unchanged by it.

## 3. Cutscene points

| Marker | Where in the flow | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | Before S1 (`PlayCutsceneWithLoad("1-05", true)`) | baked | `fade` in at `_1_05_SPAWNPLAYER` + `letterbox` + `say` line from Algie + `banner` objective; skip on retry |
| CS-2 | S7 `F_CutAlgieToStall` | in-engine | `letterbox` on, `teleport`/`camera` to fixed shot at the stall, Algie path-follow already covers motion, `fade`/`camera` back, control on. Fully reproducible today except ped path-follow. |
| CS-3 | S10 `F_NIS_AlgieLocker` | in-engine | `fade` out, `teleport` both actors, `camera` fixed shot, locker prop anim + Algie rummage anim, short Algie `say`, `reward` (money + respect messages), `fade`. Needs prop-anim and paired "give money" anim support. |
| (dead) | `F_NIS_Outro` is listed in `NISTable` but **never defined** in the file; `F_routeAlgieOutro` (closes `TRIGGER._DT_TSCHOOL_LIBRARYL`) is likewise orphaned | — | ignore |

## 4. NPCs and assets needed

### Ped models (raw ids as scripted)

| Model id | Role | Notes |
|---|---|---|
| 4 | Algie (escortee) | `PedSetUniqueModelStatus` managed; mission-critical |
| 99 | first courtyard attacker | invulnerable until triggered |
| 102, 146 | right pair; also 102+85 first-floor bathroom, 102 second-floor ambush | stat-boosted (override stat 0 -> 362, 1 -> 100) |
| 145, 146, 147 | left trio | 147 is a sixth bully variant not in the opening doc's five — id -> model name **OPEN** |
| 85 | jock pack / bathroom | as above |
| 74 | Eunice (bathroom ambience) | exits and despawns during the ambush |
| 69 | bathroom ambience ped | same |

Per the opening doc, 102/99/85/145/146 map to `GN_Bully01`-`GN_Bully05` and 74 = `GN_Fatgirl`, 69 = `GN_Hispanicboy` (**INFERRED**, carried over). Despite variable names saying "jocks", no jock models are loaded. `PedRequestModel` calls: 102, 99, 85, 145, 146, 147 (Algie's 4 comes via unique-model status).

### Animation groups (`LoadAnimationGroup`)

`2_08WeedKiller`, `GEN_Social`, `F_Pref`, `Px_Sink`, `Px_Gen`, `NPC_Cheering`, `POI_Smoking`, `HUMIL_5-8F_A`. Action trees: `Act/Conv/1_05.act` (pee dance, piss-self, hand-wash, rummage, empty poses, pee sound nodes), `Act/Anim/GenericSequences.act` (wall smoking), `Act/Props/NLockA.act` (locker), `Act/Player.act` (GetMoney receive). Note `2_08WeedKiller` and `HUMIL_5-8F_A` have no visible live use in this file — likely vestigial loads (**OPEN**).

### Props / weapons / items

- Prop anim triggers: `TRIGGER._STALDOOR01`, `_STALDOOR02`, `_STALDOOR11` (bathroom stalls), `TRIGGER._NLOCK02B06` (Algie's locker), `TRIGGER._DT_TSCHOOL_LIBRARYL` (dead outro route).
- Door locks by name: `DT_TSCHOOL_SCHOOLRIGHTFRONTDOOR`, `DT_TSCHOOL_SCHOOLRIGHTBACKDOOR`, `DT_TSCHOOL_SCHOOLLEFTBACKDOOR`, `DT_TSCHOOL_SCHOOLFRONTDOORL`.
- Weapon models requested: 339, 398, 300, 405, 362. Live use: **405** given to Algie after the locker rummage — his school books (**INFERRED**). 300 would be given to a "stage 4" attacker in `F_Stage4SetupPeds`, but that function's peds are never initialized and it is never called — dead code (likely a cut exterior ambush with a melee weapon, **OPEN**). 339/398/362 have no in-file use; 362 also appears as a stat-override *value*, not a weapon — all **OPEN**.
- Sounds: interactive streams `MS_FriendshipAllyLow/Mid/High.rsm`; one-shot `Toilet_Flush`.

### Speech usage

~37 `SoundPlayScriptedSpeechEvent(Wrapper)` call sites, ~27 distinct line ids on event `M_1_05` (ids seen: 1,2,3,4,5,8,10,11,13,15,16,17,18,19,21,22,23,25,27,29,30,31,34,40,99). Count only; no content.

### Named world data to extract later (from `1_05.DAT` / `1_05b.DAT`)

- POINTLISTs: `_1_05_SPAWNPLAYER`, `_1_05_SPAWNALGIE`, `_1_05_SPAWNJOCKSNEARLIBRARY`, `_1_05_SPAWNJOCKPACK`, `_1_05_SPAWNBULLIESRIGHT`, `_1_05_SPAWNBULLIESLEFT`, `_1_05_BLIPSCHOOL`, `_1_05B_SPAWNJOCKSBRFIRSTFLOOR`, `_1_05B_SPAWNSECONDFLOORAMBUSH`, `_1_05B_SPAWNBATHROOMWIERDOS`, `_1_05B_BATHROOMCHECK`, `_1_05B_BLIPBATHROOMSECONDFLOOR`, `_1_05B_ALGIETETHER`, `_1_05B_BLIPALGIELOCKER`, `_1_05B_NIS_LOCKER`, `_1_05B_CAMERALOCKER`.
- PATHs: `_1_05_ROUTEFIRSTBULLY`, `_1_05_ROUTESECONDBULLY` (dead), `_1_05_ROUTEBULLYFLEENEW`, `_1_05B_ROUTEFIRSTFLOORSTALL`, `_1_05B_ROUTESECONDFLOORAMBUSH`, `_1_05B_ROUTEALGIESTALL`, `_1_05B_ROUTEALGIESTALLTOSINK`, `_1_05B_ROUTEALGIELOCKER`, `_1_05B_ALGIEFLEE`, `_1_05B_ROUTEBATHROOMWIERDOS`; ambient patrol paths referenced: `_SGD_PREFECT2`, `_HALLSPATROL_1A`, `_HALLSPATROL_2A`, `_HALLSPATROL_2B`.
- TRIGGERs: `_1_05_TRIGGERJOCKSNEARLIBRARY`, `_1_05_TRIGGERJOCKSNEARFRONT`, `_1_05_TRIGGERJOCKPACK`, `_1_05_TRIGGERJOCKPACKWARNING`, `_1_05_FRONTDOOROFF`, `_TRIGGERSTAGE4FRONTDOOR`, `_1_05_BACKDOOROFF`, `_TRIGGERSTAGE4REARRIGHT`, `_1_05B_BATHROOMFIRSTFLOOR`, `_1_05B_BATHROOMSECONDFLOOR`, `_1_05B_FIRSTFLOORGIRLSBR`, `_1_05B_SECONDFLOORGIRLSBR`, `_1_05B_AREASECONDFLOORBR`, `_1_05B_LOADSMOKE`, `_1_05B_FIRSTFLOORAMBIENTOFF`, `_1_05B_FIRSTFLOORAMBIENTON`, `_1_05B_ALGIELOCKER`, `_STALDOOR01/02/11`, `_NLOCK02B06`.
- POI: `POI._POIBULLIESRIGHT` (ped production disabled for it).
- Hardcoded cameras (ischool): stall shot and locker shot listed at CS-2/CS-3.
- **Area codes seen: 0 (tschool outdoors) and 2 (ischool interior) only.**

## 5. Framework gaps (JSON mission primitives)

Existing primitives: conditions `reach_location, trigger_enter/exit, interact, event, timer, flag, all/any`; actions `hint, banner, event, set_flag/clear_flag, log, teleport, start_mission, spawn_enemy, fade, reward, letterbox, say, camera, shake`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| S1 objective + blip + spawn escortee | `banner`, `spawn_enemy` (needs a spawn-ally variant) | **spawn_ally** with mission-critical flag, ally-follow |
| S1-S3 attack waves on trigger or on hit | `trigger_enter` | **ped-hit-by-player event** (early aggro), **attack-order** (ped attacks player *or named ped*), **ped-health-threshold event** (flee at <50%), **group-all-dead condition** over only aggro'd members, **blip-on-ped** action |
| Escort follow / stop-follow with own health bar | — | **escort-follow** (recruit/dismiss ally), **defend-NPC** fail condition (KO-critical with per-cause fail message) |
| S4 countdown with cancel | `timer` (partial) | **countdown timer with fail-on-expiry and cancel-on-event**; timer tutorial popup |
| Door lock/unlock, stall/locker prop anims | — | **door-lock** (by name), **prop-anim** (open/close/hold-open + action node on a prop trigger) |
| S5 population/patrol suppression | — | **population-override** and **patrol-path enable/disable** actions (can be a scripted `event` short-term) |
| S6-S8 leash to room, tether, privacy radius | — | **combat-leash (restrict-to-trigger)**, **tether-to-point**, **area-clear-of-hostiles condition** (sphere check excluding player/ally), **ped-reached-path-node event** (drives door open/close, ambush aggro, despawn, stage advance) |
| S8 flush/sink sequence | — | path-node events + **play-action-node** (named anim state on a ped) |
| S9-S10 locker scene + payment | `fade`, `camera`, `reward` | **paired give/receive anim**, **faction-respect action**, **completion screen** (pass card with respect deltas), weapon/item grant to NPC |
| Retry skip of intro | — | **cutscene skip on attempt > 1** (baked-cutscene placeholder) |
| Speech scheduling | `say` (single) | **timed repeat bark** helper (whine cadence) — optional, can be `timer`+`say` |
| Clock-conditional cleanup | — | low priority; `event` hook on mission end |
| Dead/dangling bits (`F_CheckClockFailure`, `F_NIS_Outro`, `F_Stage4SetupPeds`, `bAlgieDistanceCheck` set-but-never-read) | — | none; document and ignore |

## 6. Open questions

1. **Algie's follow AI** is engine-side ally behavior (`PedRecruitAlly`/`PedDismissAlly`); its follow distance, catch-up and combat cower rules are not in the Lua. `bAlgieDistanceCheck` is written all over but never read in this file — presumably consumed by engine ally logic, **OPEN**.
2. The exact meaning of `PedSetTypeToTypeAttitude(1, 13, 4)` at setup (nerds vs player, attitude 4) and whether it matters for the mission.
3. Weapon models 339/398/362: requested but unused in the live flow — leftovers from cut content (see dead `F_Stage4SetupPeds`)? Confirm against the DAT/weapon tables if ever needed.
4. What the `NISTable` registration actually does (a skip-cutscene registry? `F_NIS_Outro` is undefined, so the table is half-broken as shipped).
5. Whether the 180 s bladder timer is shown on screen from the start of stage 2 or only after the "Timer" tutorial (`TutorialStart("Timer")` then `F_StartTimer` — **INFERRED** visible countdown).
6. Fail message 3 (23:01 clock check) is dead code in this build — was it ever wired? Not needed for our port.
7. Which of the loaded anim groups are actually exercised (`2_08WeedKiller`, `HUMIL_5-8F_A` look vestigial).
