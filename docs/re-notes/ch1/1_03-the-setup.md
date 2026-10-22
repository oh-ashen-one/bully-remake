# 1_03 — The Setup: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_03.lua` (1517 lines) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_03 = "The Setup"** — **VERIFIED** by the loaded animation group `"1_03The Setup"` and by content: the player chases a bully (Davis, ped model 99) across the school grounds to the auto-shop area, fights his backup in waves, and knocks him off a barrel stack to take his slingshot. Matches the canonical Chapter 1 list entry "The Setup (8-19)". No surprise.

Setting is entirely outdoors: the distance-fail check fails the mission if `AreaGetVisible() ~= 0` (area 0 = `tschool`). The arena at the end is the auto-shop yard (`TRIGGER._1_03_AUTOSHOPAREA`, garage doors `_SCGRDOOR*`).

## 2. Mission flow

Driver: `main()` → `MissionSetup()` → `MissionInit()` → `ChaseOneSetup()`, then a `gMainLoop` function pointer that switches `ChaseOneLoop → ChaseTwoLoop`, plus ~8 concurrent threads (`T_ChasePartTwo`, `T_RunFights`, `T_WaveMonitor`, `T_DoorMonitor`, `T_Chase2_MeetPeter`, `T_Taunt`, `T_BarrelTutorial`, `L_MonitorTriggers`).

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **Baked cutscene "1-03"** | `PlayCutsceneWithLoad("1-03", true, true, true)` in `MissionSetup`. Loads `1_03.DAT`, `tschool_garagedoors.DAT`, `tbarrels.DAT`. Prefect/hall-patrol spawners disabled, chase music streams armed (`MS_BikeChase*`), POI disabled, all ambient peds cleared, auto-shop population trigger activated, greaser population trigger deactivated, nav nodes inside the auto-shop trigger switched off. Retry-skip behavior is not explicit in this file — **INFERRED** the engine's attempt count handles it (pattern from 1_01/1_02); **OPEN**. |
| A1 | **Scripted intro run** (`ChaseOneSetup`) | Control off, widescreen. Player placed at `POINTLIST._1_03_START`; Davis (model 99) spawned at `_1_03_DAVISSTART` — invulnerable, min-health 10, infinite sprint, ignores stimuli/player, own action tree `Act/Anim/1_03_Davis.act`. All wave bullies, props, pickups and doors are created up front (see §4). Peter (model 134) posted at `_1_03_PETER` playing a fist-shake idle. Then the player is auto-run along `PATH._1_03_PURSUE` while Davis flees along `PATH._1_03_SCHOOLFLEE` (Davis taunts at path node 2). When the player's path ends: widescreen off, control on, sprint tutorial, objective added (chase Davis, key `1_03_INSTRUC02`), Davis gets a map blip. |
| A2 | **Chase leg 1** (`ChaseOneLoop`) | Fail-watch only (see fails below). When Davis reaches the end of `_1_03_SCHOOLFLEE` (`cbSchoolFlee`) → `ChaseTwoSetup`. |
| A3 | **Chase leg 2, fountain** | Davis flees along `PATH._1_03_FLEETOFOUNTAIN`. En route he shoulder-slams Peter at path node 1 (paired action nodes on both peds); when the player enters `TRIGGER._1_03_PETERTRIGGER`, Peter plays his reaction end-anim and is released to ambient. At the last node, `bNISBragTime` is set: Davis stops at `_1_03_DAVISBRAG`, faces a waiting bully (gCSBully, model 146 at `_1_03_BULLY1`) and plays a winded idle until the player enters `TRIGGER._1_03_FIGHT1` **or hits Davis or his buddy** (hit event registration) — either breaks the pose. |
| A4 | **Fight 1 + garbage-can tutorial** | On the brag break, Davis flees toward the garage (`PATH._1_03_FLEEFROMFOUNTAIN`, speech cue), gCSBully attacks the player, and a thread shows a garbage-can pickup tutorial (`TUT_GBIN1`) with a blip over `_1_03_BARRELBLIP` until gCSBully dies. Chase music switches to the high-intensity stream. |
| A5 | **Chase leg 3, garage run** | When the player enters `TRIGGER._1_03_DAVISRUN`, Davis flees `PATH._1_03_FLEETOGARAGE`; on arrival he is teleported to `_1_03_DAVISINGARAGE`, put to sleep facing the player. When the player enters `TRIGGER._1_03_FIGHT2`-equivalent garage trigger (see doors below) Davis wakes, taunts, and flees `PATH._1_03_FLEEGARAGE` to the barrel yard. Doors: the front gate and both garage doors close and lock behind Davis as he passes (per-door `OnExit` callback); the player re-opens each by stepping into its corona point (`_1_03_GATECORONA`, `_1_03_DOOR1CORONA`, `_1_03_DOOR2CORONA`). Opening garage door 1 starts the garage fight wave; opening door 2 starts the final fight wave. |
| A6 | **Three fight waves** (`T_RunFights`/`T_WaveMonitor`) | Trigger-gated: `_1_03_FIGHT3` → barrel-yard bully (model 85, hiding behind barrels, ambush anim then attack); `_1_03_FIGHT4` → garage bully (model 145, drops item 362 on KO); `_1_03_FIGHT5` → two guards (models 146+147, talking idle, 147 drops 362). A hack opens all doors once Davis reaches the FIGHT3 trigger. Wave deaths set flags (`bWave1Dead`, `bWave2Dead`, `bLastWaveIsDead`). |
| A7 | **Ladder drop + barrel perch** | `_1_03_LADDER_DROP` trigger: Davis plays a kick anim and the barrel-ladder prop (`_1_03_BARELLAD`) plays its destroyed state — the direct route up is removed. When the chase objective completes, Davis is moved on top of the barrel stack (`_1_03_BARRELSTOP`), barrels get collision on, taunt/chicken bark threads start (timed random taunts while perched). |
| A8 | **Boss fight: knock Davis down** (`Chase2_HandleBarrelFight`) | Once all three waves are dead (or the player lands a first hit), Davis is given a slingshot (weapon 303, 6 ammo), a cover behavior anchored at `_1_03_BARRELSTOP`, and a health bar appears. Scripted damage model: Davis has 200 health; the 1st/2nd/3rd player hits override his health to 70%/40%/9% of max via a hit-event callback (real damage is zeroed each hit — **hit count, not DPS, drives the fight**). After hit 3 he becomes invulnerable/untargetable and `bDavisDefeated`/`bBattleOver`/`bMissionPassed` are set. A boss tutorial thread nags the player to pick up and throw projectile weapons (bricks 311 / throwables 315) until Davis has been hit once. |
| A9 | **In-engine ending cutscene** (`CutsceneDavisBegs`) | Widescreen, control off, all surviving attackers flee, FOV 90 camera on Davis: he wobbles (barrels rumble), falls off the stack (fall anim + barrels destroyed prop state), pleads (speech id 11). Fade; player moved to `_1_03_PLAYERCS2`; FOV 30 close-up; player plays a pick-up-slingshot anim (`PlayerPickupSlingshot`); pass music; minigame-style completion banner (`M_PASS`, tag `1_03_SLINGSHOT`, plus respect message `MRESPECT_BM5`); player is **given weapon 303 (slingshot)**; two slingshot tutorial pop-ups (`TUT_RSPX02/03`, ~4.75 s each); fade out, doors removed, nav around the barrels re-enabled. |
| A10 | **Pass** | `PlayerSetScriptSavedData(15, 1)`, `SetFactionRespect(11, 20)` (faction 11 = bullies, **INFERRED** from Davis context), camera reset, `MissionSucceed(true, false, false)`. No chained mission start in the Lua; the next story mission (1_04) is picked up from the world mission table — **OPEN** (table lives outside the scripts). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player falls behind Davis | 3D distance > 35 m starts a 20 s fail timer with an on-screen countdown warning (`1_03_D13`, ticks once per second from 20); closing the gap resets both | `1_03_D11` |
| Player leaves the chase route | Entering `_1_03_FAILTRIGGER01/02/04` or leaving area 0 (indoors) fails instantly with a hint line (`1_03_D10`) | `1_03_D11` |
| Generic | any other failure path | `MissionFail(true, true)` no key |

On fail: fail music, text cleared, `MissionFail(true, true, msg)` (fade + retry allowed). Cleanup restores population triggers, POI, weather, door props, barrel collision state (restored if failed, left cleared if passed), and clears `shared.g1_03_BoughtSlingshot`. There is no partial checkpointing: a retry replays from A0/A1.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | A0, before control | baked (`1-03`) | fade in on player at `_1_03_START` + letterbox + one `say` beat of original VO, or straight to A1's scripted run |
| CS-2 | A1 scripted run | in-engine (paths `_1_03_PURSUE`/`_1_03_SCHOOLFLEE`) | keep as-is: it is already our `camera`-follow + path-walk pattern, control off + letterbox |
| CS-3 | A3 brag pose / A5 door beats | in-engine micro-scenes | no cut; just NPC anims triggered by our `trigger_enter` |
| CS-4 | A9 Davis fall + pickup | in-engine, ~15 s | letterbox + fixed `camera` at Davis + timed prop/anims + `fade`; the fall can be one scripted anim + shake; then `reward` (slingshot) + tutorial `banner`s |
| CS-5 | pass camera in `main` | engine camera set | trivial, fold into CS-4 outro |

Cutscene-only anim groups: `NIS_1_03` (plus `POI_Smoking`, `Cheer_Cool2`, `GEN_Social`, `Hang_Talking`, `NPC_Adult` for ambient/talk poses).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 99 | Davis (chase target / boss) | unique-ified; custom action tree `Act/Anim/1_03_Davis.act`; slingshot 303 w/ 6 ammo in boss phase |
| 134 | Peter (bystander gag) | idle → slammed → released ambient |
| 146 | gCSBully (brag buddy, fight 1) + final-wave guard 1 | drops 362 (gCSBully), guard 1 no drop |
| 85 | barrel-yard ambusher (wave 1) | |
| 145 | garage fighter (wave 2) | drops 362 |
| 147 | final-wave guard 2 | drops 362 |
| 102 | loaded in `MissionInit` bully model list but never spawned in the live path | dead load (**INFERRED**) |

Item 362 = KO drop on marked bullies — contents **OPEN** (likely a pickup; script gives no name).

### Animation groups / action trees

`LoadAnimationGroup`: `1_03The Setup`, `NIS_1_03`, `POI_Smoking`, `Hang_Talking`, `GEN_Social`, `Cheer_Cool2`, `NPC_Adult`, `TSGate`, `SCgrdoor`, `Sbarels1`, `Area_School`, `Px_Rail`, `DO_Grap`.
`LoadActionTree`: `Act/Conv/1_03.act` (all mission conv nodes), `Act/Anim/1_03_Davis.act` (Davis AI), `Act/Props/SBarels1.act` (barrel stack), `Act/Props/barrelLad.act` (ladder).

### Props / pickups / weapons

- Doors (PAnim props): `_TSCHOOL_AUTOSHOPFGATE` (front gate, slides), `_SCGRDOOR01`, `_SCGRDOOR02` (garage doors, roll up), plus a plain `_SCGRDOOR` created closed/locked.
- Barrel stack `_TBARRELS_SBARELS1` (collision on/off, rumble, destroy states) and ladder `_1_03_BARELLAD`.
- Garbage cans `_1_03_GARBAGECAN02/05/06/07` (created non-targetable — throwable props for the tutorial).
- Pickups: bricks (model 311, respawning, from point list `_1_03_PICKUPBRICKS`, respawn attribute "BrickBute" — brick **INFERRED**); four permanent weapon pickups (model 315) at `_1_03_TCLID02/04/05/06` — throwable, exact item **OPEN**.
- Weapons referenced: 303 slingshot (Davis, then player reward), 311 brick (**INFERRED**), 315 throwable (**OPEN**).

### Speech

`SoundPlayScriptedSpeechEvent("M_1_03", id, ...)` ids used: 3, 5, 11, 12, 14, 21, 22, 23, 32, 33, 41, 42 — **12 distinct scripted lines** (jumbo/supersize categories), plus hit-reaction line on hit 1. Music: interactive streams `MS_BikeChaseLow/Mid/High`; pass/fail stingers via `SoundPlayMissionEndMusic`.

### Special entities

`AreaEnsureSpecialEntitiesAreCreatedWithOverride("1_03", 4)` at pass (variant 4 = post-mission state), override 1 on fail — a persistent world-state swap for the auto-shop corner (**INFERRED**: the barrel stack/garage dressing).

### Named world data to extract from `1_03.dat` (+ `tschool_garagedoors.dat`, `tbarrels.dat`)

- POINTLISTs: `_1_03_START`, `_1_03_DAVISSTART`, `_1_03_PURSUE`*, `_1_03_PETER`, `_1_03_BULLY1`, `_1_03_BULLY3`, `_1_03_BULLY4`, `_1_03_GUARD1`, `_1_03_GUARD2`, `_1_03_DAVISBRAG`, `_1_03_DAVISINGARAGE`, `_1_03_BARRELSTOP`, `_1_03_OFFBARRELS`, `_1_03_DAVISFALLFROM`, `_1_03_PLAYERCS2`, `_1_03_BARRELBLIP`, `_1_03_GATECORONA`, `_1_03_DOOR1CORONA`, `_1_03_DOOR2CORONA`, `_1_03_PICKUPBRICKS`, `_1_03_TCLID02/04/05/06`.
- PATHs: `_1_03_SCHOOLFLEE`, `_1_03_PURSUE`, `_1_03_FLEETOFOUNTAIN`, `_1_03_FLEEFROMFOUNTAIN`, `_1_03_FLEETOGARAGE`, `_1_03_FLEEGARAGE`.
- TRIGGERs: `_1_03_FIGHT1`, `_1_03_FIGHT3`, `_1_03_FIGHT4`, `_1_03_FIGHT5`, `_1_03_LADDER_DROP`, `_1_03_DAVISREGISTERHIT`, `_1_03_PETERTRIGGER`, `_1_03_DAVISRUN`, `_1_03_FRONTGATE`, `_1_03_GARAGE02`, `_1_03_GARAGE03`, `_1_03_KEEPPEDTRIGGER`, `_1_03_FAILTRIGGER01/02/04`, `_1_03_AUTOSHOPAREA` (population), `_GREASERS` (population), door props `_TSCHOOL_AUTOSHOPFGATE`, `_SCGRDOOR`, `_SCGRDOOR01`, `_SCGRDOOR02`, `_TBARRELS_SBARELS1`.
- Hardcoded coords (cameras/pass): (127.9, 25.0, 7.1), (128.4, 24.3, 6.8), (128.7, 24.3, 7.2), nav-radius re-enable at (134.7, 27.6, 6.1).
- Area codes seen: **0 only** (all outdoors; mission fails if area ≠ 0).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| A0 cutscene + retry skip | `fade`, `letterbox` | **cutscene_skip_on_retry** (attempt-count-aware gate) |
| A1 scripted player run | `camera`, `teleport` | **player_follow_path** (scripted locomotion, control off, node-done event) |
| A2/A3/A5 chase legs | `trigger_enter`, `flag` | **npc_follow_path** with **ped_reached_path_node(node|last)** events; **npc_distance_leash** fail condition (distance > R for T seconds with countdown HUD) |
| A3 brag pose / hit interrupt | `event` | **ped_hit_event** (attacker/victim filter) driving interrupts; **ped_action_node** state query |
| A4/A6 fight waves | `spawn_enemy`, `trigger_enter` | **wave_group** (tagged spawns + **all_dead(group)** condition + per-wave callbacks); **npc_drop_item_on_ko** |
| A5 door choreography | — | **door_prop_control** (open/close/lock/pathable states), **corona_interact** (step-in-to-open), door-exit event when a specific NPC passes |
| A7/A8 boss fight | `spawn_enemy` | **boss_hit_count_health** (scripted health override per hit N, real damage zeroed), **npc_cover_behavior** (tethered shooter), **health_bar(name)**, **npc_invulnerable/untargetable toggles**, timed random **bark_loop** |
| A8 projectile nag | `hint` | **player_has_weapon/ammo** condition; **pickup_spawner** (respawning / permanent mission pickups) |
| A9 ending | `fade`, `letterbox`, `camera`, `say`, `shake` | **prop_set_state** (barrels rumble/destroy, ladder destroy), **completion_banner** (pass screen + respect line), **give_weapon** |
| A10 pass | `reward` | **set_faction_respect**, **world_state_swap** (special-entity override) |
| fails | `timer`, `event` | **leave_area_fail** (area-code check), countdown-warning variant of `timer` |

## 6. Open questions

- `Act/Anim/1_03_Davis.act` internals: how Davis's chase speed is modulated so the player can keep up (stat overrides 1/10/11 are set but their meaning is undecoded), and the cover behavior parameters in `PedCoverSet`.
- Exact fail-trigger placement vs. shortcut routes (coords pending DAT extraction).
- What pickup 362 and weapon 315 actually are (ids only in script).
- Cutscene "1-03" content/duration and whether retry skips it (no explicit attempt check in this file).
- How the mission is offered in the world (start trigger/blip) — mission table is outside the Lua.
- The `T_DoorMonitor`/`bDoorOpened` handshake: who sets `bDoorOpened` (no writer found in this file — presumably `LibTriggerNew.lua`'s `L_MonitorTriggers`; **OPEN** but behavior is clear: step into corona → door opens).
