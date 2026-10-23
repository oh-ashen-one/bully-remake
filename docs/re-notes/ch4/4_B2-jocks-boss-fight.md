# 4_B2 — Jocks Boss Fight (Ted Thompson, football field; Chapter 4 chapter boss): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_B2.lua` (1019 lines, complete) + `SMissPass.lua` (4_B2 case) + `main.lua` (bulletin + post-completion hooks). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_B2 = Jocks Boss Fight**, the Chapter 4 chapter boss: Jimmy (in the headless mascot outfit) vs. Ted Thompson on the football field at night. **VERIFIED** by content: `gTed` = ped model 110 (Ted) with 5000 HP until the finale, `gDefenders[1]` = model 112 (Damon) with tree `Act/Anim/J_Damon.act`, jock models {231, 111, 109, 232}, mascot outfit `MascotNoHead`, boss music `MS_JockBossBattle.rsm` + `FootballGameAmbience.rsm`, football prop 331 / Ted's ball-weapon 400 with `SetWeaponLOD(331, 120)`, pass-side `SetFactionRespect(2, 100)` (faction 2 = jocks) + `SetFactionRespect(1, 100)` (nerds) + `UnlockYearbookPicture(110)`, and `SMissPass.lua`'s 4_B2 case doing the chapter-transition restart-point setup. Matches catalog entry #52 "The Big Game … (chapter boss: Ted/Jocks)" — note the catalog folds the pair: **`4_06` is "The Big Game"** (the prank mission), **`4_B2` is "Jocks Boss Fight"** (the boss itself), chained by the mission table (**INFERRED** from wiki unlock order; the table is outside the Lua).

4_B1 vs. 4_B2: **not two halves of one mission.** 4_B1 = Nerd Boss Fight (Earnest, observatory); 4_B2 = Jocks Boss Fight (Ted, field) = the actual chapter boss, since its pass leads into `Chapt4Trans` (shared `indusrtialRestartPointsAdded` setup appears in both the `SMissPass` 4_B2 case and `Chapt4Trans.MissionSetup`, VERIFIED) — mirroring 3_B → Chapt3Trans.

Structure: function-pointer stage machine (`gStageFunction`, polled in `main()`): intro → three "defensive line" waves (KO all 3 defenders to advance) → Ted flee/taunt finale. Long-stage design: stages 2/3 rebuild the line from the sideline bench. Persistent threads: `T_MonitorPeds` (win/fail watch), `T_TedGetBalls` (Ted's pass-and-attack cycle), `T_ForcePlayerGrapple` (defender interception).

Setting: football field, **area 0**, night (clock forced 22:00), `WeatherSet(2, true)` (weather type 2 — **INFERRED** overcast/rain). Baked cutscenes `"4-B2"` (intro) and `"4-B2B"` (outro). Auto-restart explicitly disabled on both pass and fail (`MissionSetAutoRestart(false)`).

## 2. Mission flow

Driver: `MissionSetup()` (world state) → `main()` → `MissionInit()` (loads/spawns) → `F_Setup()` (Petey + music) → stage-function loop. `T_MonitorPeds` ends the mission: player dead → fail; Ted dead or invalid → all jocks flee + pass.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup** | `shared.bFootBallFieldEnabled = false` (area-script gate); music/punishment off; clothing backup + `MascotNoHead` outfit; clock 22:00; baked `"4-B2"`; weather 2 forced; ambient ped generation off; `4_B2.DAT` layer 2. `MissionInit`: models (football 331, ball-weapon 400, Petey 134, Ted 110, Damon 112, jocks 231/111/109/232), trees `J_Damon`/`Conv/4_B2`, anim groups `NPC_AggroTaunt`/`NIS_4_B2`/`SGIRL_S`; vehicles + bikes purged; weapon snapshot then **full weapon strip**; player HP to max; flag 58; football LOD boosted; persistent set dressing (crowd prop, stadium wall, cooler, "bad" LED scoreboard) + `NOGO_tschoolSOP` geometry on; POI off; population zeroed; peds cleared. |
| B1 | **Cast** | Ted: tethered to `_TED`, stat 34 → 0, **5000 HP** (effectively invulnerable), hostile to player, flag 68, infinite sprint. Defensive line: Damon (112, 200 HP) + jocks 231/232 at `_DEFENSIVELINE01`, each: disarmed, infinite sprint, hostile, **drop health item 362 on KO**, `J_Damon` tree, focus/lock player, stat 34 → 0, **`PedGuardPed` → guarding Ted**, blip, tethered to `_TETHERD101..103`. Sideline bench: 7 jocks stationary/invulnerable/cheap/untargetable (spectators until promoted). Boss fight cam `"4B2"` second-targeted on Ted. |
| B2 | **Stage 1 intro** | Ambience on; widescreen; FOV 40; camera flies `_INTROCAM` looking at `_INTROLOOKAT`; the 3 defenders strike `IntoPose`; grapple tutorial message (9.5 s); sideline passer A arms a football (331) and **passes to Ted** (`PedPassBall`/`PedReceiveBall`, 4 s window); Ted swaps to weapon 400, stat 11 → 100, attacks the player. Control on; objective `4_B2_Obj1`; threads `T_ForcePlayerGrapple`, `T_MonitorPeds` started. |
| B3 | **Stage 1: first line** | `T_TedGetBalls` arms (3 s late start): whenever the player is >6 m from Ted and ≥2 s since last cycle — pause combat, a random sideline passer (A or B, 50/50) throws Ted a football (speech roulette: Ted line / passer line / taunt), Ted catches (4 s watch), clears stray balls within 100 m, swaps to weapon 400 and **charges the player**. Defenders intercept: `T_ForcePlayerGrapple` — player within 6 m of Ted → nearest live defender is put to sleep and force-run at the player (`StartRun` node), Ted barks id 11. KO accounting: each KO'd defender → `F_TetherDefenders`: **1 KO** → survivors re-tethered to wider zones (`_TETHERD104/105`), Petey congratulates; **2 KOs** → tethers cleared (free pursuit); **3 KOs** → stage advance. |
| B4 | **Stage 2: second line** | Beat-NIS: Damon knocked down at line 1, `GetUpEasy`, cruel-laugh + line id 12, back to idle. Sideline jocks 1-3 promoted: 2-3 become the new defenders (tethers `_TETHERD201..203`); **sideline 1 becomes a free attacker who respawns 10 s after each KO** (`F_CreateReinforcement` — random model from {231,111,109,232}, disarmed, attacks, alternately drops health 362, flag 68; Ted bark id 13 on each respawn). Ted retreats to `_TED02` (re-tethered there), passers re-cast (A = bench slot 4, B = slot 6). Same KO-ladder (wider tethers `_TETHERD204/205`, then free, then advance). Ted's ball cycle keeps running through stages 1-3 (gated by `gPauseThread`/`gTedCanAttack`). |
| B5 | **Stage 3: third line** | Damon `GetUpHard` beat at 100 HP (mocking bark + line id 12); bench slots 5-6 → defenders 2-3 (tethers `_TETHERD301..303`); sideline 4 becomes attacker #1 (10 s→25 s respawn) and a **second reinforcement slot opens** (25 s respawn each); passer A only (slot 7). Damon's flag 58 cleared and `FIGHT_BEATEN` bark fired on his final KO. Same KO-ladder → advance. |
| B6 | **Stage 4: Ted's stand** | Fade; Ted's HP/ max set to **5** (one solid hit); everyone else deleted/cleared; player moved to `_PLAYEREND`; Ted disarmed, taunts (`TAUNT` xtralarge, player `PLAYER_TAUNT_FLEE` reply), then **flees between `_TEDFLEEPOINTS` 1-4** (random, no immediate repeat, anti-stuck reroll). Health bar on (`4_B2_01`); objective 1 completed, objective 2 added (beat Ted). Loop: on reaching a point Ted faces the player and taunts every 3 s; **player within 7 m → Ted bolts to the next point** (`INDIGNANT` bark). Petey bark id 1; sideline 7 (last jock) attacks the player during the chase. Ted dead → `T_MonitorPeds`: everyone else flees, pass. |
| B7 | **Pass** | 2 s hold; defenders/sidelines/Petey deleted; models released; baked `"4-B2B"`; auto-restart off; **jock respect 100 + nerd respect 100**; yearbook 110; `MissionSucceed(true, false, false)`. Cleanup: outfit forced to `"Uniform"` on success (backup restored on fail); weather released; snapshot restored+deleted; field re-enabled; population/POI restored. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player dead | `T_MonitorPeds` (10 ms poll) | `M_FAIL_DEAD` after a Damon/Ted victory bark (`VICTORY_TEAM`), `M_FAIL` banner, fail music track 10 |

`MissionSetAutoRestart(false)` on both outcomes; retry = mission relaunch from the intro (no stage checkpoint).

### Rewards / pass side-effects (`SMissPass.lua` 4_B2 case + in-script)

- In-script: jock + nerd respect 100; yearbook picture 110.
- `SMissPass` 4_B2: force-completes (with success-count increments) `C_Chem_4`, `C_Art_4`, `C_Wrestling_4`, `C_English_4`, `C_Photography_5`; and the **industrial-area restart-point block** (`AddKORestartPoint`/`AddArrestRestartPoint` + interior variants 20/16/54 + camera positions, guarded by `shared.indusrtialRestartPointsAdded`) — duplicated in `Chapt4Trans.MissionSetup`, which is the actual chapter roll (see `chapt4trans-chapter-transition.md`).
- `main.lua`: post-4_B2 world hook at line ~702 (`IsMissionCompleated("4_B2")` gate — what it enables needs a follow-up read, **OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0 opening | baked `"4-B2"` | `fade` + original establishing card |
| CS-2 | B2 field intro | in-engine (camera paths + poses + pass) | `camera` path/look-at pair + `letterbox` + **pass_ball beat** + tutorial hint |
| CS-3 | B4/B5 line rebuilds | in-engine (Damon get-up beats) | `camera` fixed + `ped_action_node` get-up + `say` + respawn juggling |
| CS-4 | B6 Ted's stand intro | in-engine (taunt + flee) | `say` pair + `npc_move_to` + health bar on |
| CS-5 | B7 victory | baked `"4-B2B"` | `fade` + `completion_banner` + original outro card |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 110 | **Ted Thompson** (boss) | 5000 HP → 5 HP finale; stat 34 → 0; stat 11 → 100 when charging; flag 68; flee/taunt final phase with random point selection |
| 112 | **Damon** (line captain) | 200 HP stages 1-2, 100 HP stage 3; flag 58 (cheap-KO immunity) until stage 3; scripted get-up beats between stages |
| 231, 232, 111, 109 | line defenders + bench | defenders: `J_Damon` tree, `PedGuardPed(Ted)`, drop 362 on KO, tether ladder; bench: cheap/invulnerable spectators until promoted |
| (random of above) | reinforcement attackers | 10 s (stage 2) / 25 s (stage 3) respawn timers; alternate health-drop |
| 134 | Petey | invulnerable sideline commentator (congratulations barks; final-phase bark id 1) |

### Animation groups / action trees

`NPC_AggroTaunt`, `NIS_4_B2`, `SGIRL_S`; `Act/Anim/J_Damon.act` (defenders: poses, run-tackle, get-ups), `Act/Conv/4_B2.act` (poses, get-up variants, taunt, blank).

### Props / pickups / weapons

- Football pickup 331 (LOD boosted to 120 for the whole mission, restored in cleanup); Ted's charge weapon 400 (**INFERRED**: thrown/explosive ball — he swaps to it after each catch).
- Health drop 362 on defender KOs (defenders + alternating reinforcements).
- Persistent set dressing: `tschl_uvCrowd`, `scBoss_wall`, `SC_GatCooler`, `SC_JocksLEDbad`; geometry `NOGO_tschoolSOP`.

### Speech

`M_4_B2` ids 1, 5, 6, 8, 9, 11, 12, 13 (+ `M_4_03` id 18 recycled in the pass roulette); event barks `FIGHT_INITIATE`, `LAUGH_CRUEL`, `BIKE_SEE_TRICK_FAIL`, `FIGHT_BEATEN`, `TAUNT`, `INDIGNANT`, `VICTORY_TEAM`, `CONGRATULATIONS` (Petey), `PLAYER_TAUNT_FLEE`. Roughly **12 distinct scripted lines** + barks. Music: `MS_JockBossBattle.rsm` + `FootballGameAmbience.rsm` ambience, end music track 10.

### Named world data to extract from `4_B2.DAT` (trigger data **VERIFIED present** — `DATLoad("4_B2.DAT", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_PLAYERSTART` (2 elements), `_PLAYEREND`, `_TED`, `_TED02`, `_TED03`, `_TEDFLEEPOINTS` (4), `_DEFENSIVELINE01/02/03` (3 each), `_SIDELINE` (7), `_RESPAWN`, `_PETEY`.
- PATHs: `_INTROCAM`, `_INTROLOOKAT`.
- TRIGGERs: `_TETHERD101..105`, `_TETHERD201..205`, `_TETHERD301..305` (per-stage tether ladder), `_FOOTBALLFIELD` (pickup-clear zone).
- Hardcoded cameras: stage-2 (-26.83, -67.17, 1.99 → …), stage-3 (-27.11, -82.09, 1.56 → …), stage-4 (-34.67, -98.40, 3.77 → … and -25.98, -106.35, 3.52 → …).
- Area codes seen: **0** only.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| B0 setup | `mission framework`, `weapon_inventory_snapshot` + `weapon_strip`, `temp_outfit_swap`, `weather_force`, `population_override`, `player_buff` (pool) | **area_script_gate** (`shared.bFootBallFieldEnabled` — mission suppresses an area script's ambient behavior; generalizes `area_script_shared_state`) |
| B1 cast | `spawn_npc` bundles, `npc_stat_override`, `npc_tether_to_zone`, `npc_drop_item_on_ko` (pool) | **npc_guard_ped** (`PedGuardPed` — defender bodyguard assignment; distinct from static `guard`/`npc_scene_guard`) |
| boss fight cam | — | **boss_fight_camera** (shared with 4_B1: second-target + `FollowCamSetFightShot` preset) |
| B2/B3 pass cycle | — | **boss_pass_catch_cycle** (sideline passer arms prop → `PedPassBall`/`PedReceiveBall` timed catch → boss swaps to special weapon + charges; pause-gated, distance-gated, with speech roulette) |
| defender interception | — | **force_grapple_intercept** (player within radius of boss → nearest live defender is slept and force-run at the player; boss bark) |
| B3-B5 KO ladder | `wave_group` (pool, partial) | **ko_tether_ladder** (per-stage: 1 KO → survivors re-tethered wider, 2 KOs → untethered, 3 KOs → stage advance; per-stage tether trigger sets) |
| reinforcements | `continuous_wave_spawner` (pool, partial) | **respawn_slot_timers** (named attacker slots with independent respawn delays — 10 s/25 s — and alternating drop tables) |
| B4/B5 line rebuilds | `ped_action_node`, `spawn_npc` | reuse; promotion pattern = bench list → defender list (data-driven cast table) |
| B6 finale | `npc_health_bar` (pool) | **flee_taunt_boss_phase** (boss reduced to 1-hit HP; flees between N random points with no-repeat reroll; taunts on arrival every 3 s; player proximity → next flee) |
| B7 pass | `completion_banner`, `yearbook_unlock`, `set_faction_respect`, `force_complete`, `set_restart_points` (pool) | none new |
| football LOD | — | **weapon_lod_override** (`SetWeaponLOD` raise/restore for a mission-critical prop weapon; minor) |

## 6. Open questions

- Weapon 400 semantics (Ted's post-catch weapon: thrown football? explosive?) and what stat 11/34 actually modulate.
- Ped flags 68 (Ted + reinforcements), 106 (cleared on sidelines), 58 (Damon until stage 3) meanings.
- Whether `T_TedGetBalls`' passers running dry in stage 4 is intentional (gPasserA/B set false → Ted stops getting balls — consistent with the flee phase).
- `F_FaceRightWay`/`F_FaceRightWayClear` and `F_DamonStandUp` are defined but never called — **dead code** (VERIFIED); possibly remnants of a scrapped facing correction.
- Mission-table wiring: exact `4_06` → `4_B2` → `Chapt4Trans` chain and bulletin `BB_25` gating; what `main.lua`'s post-4_B2 branch (line ~702) unlocks.
- `PedGetMaxHealth`/`PedSetMaxHealth` interplay on Ted's 5000→5 transition (does the finale expect damage carryover?).
