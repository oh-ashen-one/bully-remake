# 3_B — Fighting Johnny Vincent (Chapter 3 boss fight): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_B.lua` (1619 lines) + `SMissPass.lua` (3_B case) + `Library/BikeRace_util.lua` (not used here; chase logic is in-file). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_B = Fighting Johnny Vincent, the Chapter 3 chapter boss.** (Fan wikis also call this mission "Rumble"; the catalog's #43 "Rumble (chapter boss: Johnny Vincent)" conflates it with 3_06, which is the separate pre-boss mission "The Rumble" — see `3_06-the-rumble.md`.) **VERIFIED** by content: boss ped model 23 with dedicated action tree `Act/Anim/G_Johnny.act`, greaser faction wrap-up (`SetFactionRespect(4, 100)` on pass, `UnlockYearbookPicture(23)`), boss music chain `MS_3B_JohhnyV_NIS.rsm` → `MS_FightingJohnnyVincentBikeRide.rsm` → `MS_FightingJohnnyVincentFight.rsm` → `MS_FightingJohnnyVincentBossFight.rsm` → `MS_3-B_ENDTAG.rsm`, and the `SMissPass.lua` 3_B case performing the Chapter 3→4 transition (see Rewards). Matches catalog entry #43 "Rumble (chapter boss: Johnny Vincent)".

Structure: three gameplay stages behind a persistent stage checkpoint (`shared.g3BMissionState`, values 0-3) so a fail restarts at the current stage, not the intro:

| Stage | Name in file | Content |
|---|---|---|
| 0 | `STAGE_START` | Baked intro `"3-B"` + in-engine alley NIS: Johnny throat-grabs Jimmy, cops raid, greaser busts, both flee on bikes |
| 1 | `STAGE_COP_CHASE` | Bike escape: a cop car pursues through the junkyard while greasers jump/run across the route |
| 2 | `STAGE_BIKE_BATTLE` | Junkyard arena: Johnny circles on his bike swinging a lead pipe while 6 perimeter greasers egg the player; Petey must survive-walk to the crane; magnet capture |
| 3 | `STAGE_JOHNNY_BATTLE` | Dismounted boss fight vs Johnny on foot |

Setting: starts in an alley (**area 43** on first start, VERIFIED by `AreaTransitionPoint(43, POINTLIST._PLAYER_FIRSTSTART)`), chase and battle in the junkyard area 0. Note `F3_StageThreeSetup` writes `shared.g3BMissionState = STAGE_BIKE_BATTLE` (not STAGE_JOHNNY_BATTLE) — likely a bug or a deliberate "retry stage 3 from stage 2" choice (**OPEN**).

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → `MissionInit()` (reads `shared.g3BMissionState` to pick the stage; only `STAGE_START` re-does the area transition). Stage functions `F0..F3` run sequentially; `F_MissionFail(why)` / `F_MissionPass()` flip globals the main loop polls.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup** | Interactive music off; punishment off; **weapon inventory snapshot** (`PedSaveWeaponInventorySnapshot`; restored on fail, deleted on pass); player HP topped to 200 if lower; player ped flag 58 (cheap-KO immunity, per 2_B doc) set; rain forced off (`F_RainBeGone`); control off. |
| B1 | **Stage 0: cops NIS (first attempt only)** | Baked cutscene `"3-B"` plays first. Then in-engine: player's bike cleared, all vehicles/explosions/projectiles cleared. Johnny (23) spawns at `_JOHNNY_FIRSTSTART`, locks on and plays a throat-grab node (`G_Johnny/Cinematic/ThroatGrab`) until the player's matching receive-node plays (2 s safety timeout force-plays it), then throws Jimmy belly-up. Cops spawn (`_3B_COPS` point list, model 97), two blocking cop cars (295) drive `_3B_BLOCKCOPCAR01/02PATH` with sirens; two posted greasers (29, 28) flee and get chased/busted by cops (punishment points 600 — a scripted bust vignette). Johnny runs to his bike (`_3_B_JOHNNYTOBIKE`), mounts, flees `_JOHNNYFLEE01`; the chase cop car follows `_3B_COPCARALLEYCHASE`; player get-up anim, then player follows `_3B_PLAYERFLEECOPALLEY` (node-2 callback = reached own bike). → stage 1. |
| B2 | **Stage 1: junkyard bike chase** | Player mounted on bike 273 at `_PLAYERBIKEFIRSTSTART`; chase car (cop 97 in car 295, cheap/asleep/ignore-stimuli) warped to `_3_B_COPCAR_WARP`, siren on, follows `_3_B_COPCARCHASEPLAYER`; player punishment 800; vehicle camera preset `JunkyardChase`. A trigger-chain encounter script (`TriggerHandler` 0→6) fires on `_GREASERS_JET01..05` and `_JOHNNYRAMBLE02`: onlooker greasers flee, crate-jumpers (27/21/28) vault crate stacks, last jumpers leap, last runners sprint — pure spectacle, all untargetable. `_3B_COPSSPEEDUP` trigger boosts the cop car to cruise 25 and arms a **distance-adaptive pursuit speed** (12/17/22/25 at <8/8.5/20/25 m). **Arrest fail**: cop driver within a small box of the player, or the player stays off the bike for a 5-count countdown (`3B_BIKEWARN` text, `BIKE_REENTRY_DELAY` 5000 ms) → `F1_Arrested`: control off, busting bark, fall-off-bike anim unless on foot, player HP zeroed, fail key `3_B_FAIL01`. Reaching `_3_B_STAGE_2_TRIGGER` with handler 6 fades out → stage 2. |
| B3 | **Stage 2: bike battle + crane** | Baked `"3-BB"` transition cutscene; player teleported to `_3_B_STAGE2_TELEPORT`, dismounted. Johnny re-spawned on bike 282 with weapon **418 (lead pipe)**, 500 HP, health bar (`3_B_JOHNNY_HEALTH`), stat overrides (33→100, 34→0, 24→50), ped flag 107, damage-taken ×0.2 (type 3) / ×0.3 (type 0), ignore stimuli. 6 perimeter greasers (24/22/27/26/28/29) at `_3_B_STAGE2_THUG_A..F` with 20 HP, blips, egg/stink-bomb weapons (312 ×5, last one 309), attack the player from fixed posts. **Petey thread**: Petey (134, invulnerable, untargetable) walks path chain `_3_B_START → _PATH_A … → _PATH_F` toward the crane; he crouches at each path end and only advances to the next leg when the greaser guarding that leg is dead (`F2_UpdatePeterPos` on kill-watch thread). Reaching the crane (`_AREA_AROUND_BUTTON` trigger) plays his crane NIS (bark id 46, magnet hum emitter on, crane + button persistent entities `JY1d_animlight`/`DPE_buttonOFF` spawned, magnet blip). **Johnny behavior**: `F2_JohnnyChoosePath` circle-strafes the arena — four region triggers (`_3_B_BIKE_REGION_NE/NW/SE/SW`) each select the next quarter-path (counter-clockwise rotation); near the magnet he instead picks `_3_B_JV_MAGNET_PATH_SW/NE`. Charge attack every 6-10 s: stop, bark id 18, ride at the player's position, swing pipe right+left (`LeadPipe/SwingRight/LeftExec` in `Act/Vehicles.act`), resume circling; if dismounted he walks back and re-mounts. Controller vibration pulses inside `_3_B_MAGNET`, stronger once the magnet is on. |
| B4 | **Magnet capture → stage 3** | Once all 6 greasers are dead **and** the magnet is on, Johnny entering `TRIGGER._3_B_MAGNET` triggers `JohnnyMagnetized`: Johnny put to sleep, pipe weapon destroyed, `Magnetized` action node plays while the lead pipe flies to the crane as a persistent entity following `_3_B_LEAD_PIPE_PATH` (speed 3.5) — the crane disarms him. Failsafe: if he's somehow off the bike, just destroy the pipe. → stage 3. |
| B5 | **Stage 3: ground boss fight** | Boss music 3, player bark id 13, Johnny made targetable and attacks on foot (`PedAttackPlayer` 3). His AI tree reads `F_FightCondition` (knockdown flag) / `F_DodgeCondition` (avoid flag) — same undecoded-tree pattern as 2_B. Johnny dead → pass path. |
| B6 | **Pass** | 2.5 s hold, end music `MS_3-B_ENDTAG.rsm`, baked cutscene `"3-BD"`, health bar off, `SetFactionRespect(4, 100)` (faction 4 = greasers, VERIFIED via 3_G3's `PedGetTypeToTypeAttitude(4, 13)` greaser-player usage), `UnlockYearbookPicture(23)` + `F_UnlockYearbookReward()`, weapon snapshot deleted, saved-data slot 14 = 0, `MissionSucceed(true, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player dead | health poll in every stage loop | `M_FAIL` banner (snapshot restored) |
| Arrested in stage 1 | cop-driver proximity box, or off-bike 5 s countdown | `3_B_FAIL01` |

Retry resumes at `shared.g3BMissionState` (0-2); stage 3 maps back to stage 2 on retry (see §1 note). Stage 0's alley NIS is the only non-replayable intro.

### Rewards / pass side-effects (`SMissPass.lua` 3_B case + in-script)

- `ChapterSet(3)` if chapter ≤ 3 (enters Chapter 4, 0-indexed — VERIFIED) + `AreaLoadSpecialEntities("Christmas", false)` (winter dressing **removed**; mirrors the ch2→3 load).
- Force-completes (with success-count increments): `C_Chem_3`, `C_Art_3`, `C_Wrestling_3`, `C_English_3`, `C_Photography_3`; `HUDPhotographySetColourUpgrade(true)`.
- In-script: greaser respect 100, yearbook picture 23, saved-data 14 = 0.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B1 opening | baked `"3-B"` + in-engine alley sequence | `fade` + original establishing card; in-engine: `letterbox` + pair-anim throat grab + `npc_follow_path` flee beats + scripted cop cars |
| CS-2 | B2→B3 transition | baked `"3-BB"` | `fade` + short junkyard flyover card |
| CS-3 | B3 Petey crane beat | in-engine (~5 s, 2 camera shots) | `camera` pair + `say` + magnet-on `flag` |
| CS-4 | B4 magnet capture | in-engine scripted anim | `prop_set_state` + pipe `prop_path_anim` + boss stun beat |
| CS-5 | B6 victory | baked `"3-BD"` | `fade` + `completion_banner` + original outro card |

Cutscene-only anim groups: `NIS_3_B`, `3_BFightJohnnyV`, `P_Striker`, `Boxing` (loaded for the whole mission). Dead code: `T_DebugController`-style debug prints only; `T_MagnetVibrate` is live (vibration feedback).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 23 | **Johnny Vincent** (boss) | tree `Act/Anim/G_Johnny.act`; stage 2: 500 HP, pipe 418, damage-taken ×0.2/×0.3, stat overrides 33/34/24; health bar; circle-strafe + charge pattern |
| 134 | Petey | crane helper: invulnerable, untargetable, crouch/advance legs gated on guard deaths |
| 97 | cops | alley bust vignette + chase car driver (cheap, asleep, ignore stimuli) |
| 29, 28 | alley greasers | stationary until the scripted bust |
| 29/24/21/27 | onlookers (stage 1 dressing) | flee on approach; crate-jumpers/runners untargetable |
| 24/22/27/26/28/29 | perimeter eggers ×6 | 20 HP, weapons 312 (eggs) / 309 (stink bomb on the last), `G_Ranged_A` tree, stat-tuned (3/2→100, 13→0, dodge freq 5) |

### Animation groups / action trees

`LoadAnimationGroup`: `3_BFightJohnnyV`, `P_Striker`, `Boxing`, `NIS_3_B`.
`LoadActionTree`: `Act/Conv/3_B.act` (Petey crouch/stand/crane, player fall-off-bike), `Act/Anim/G_Ranged_A.act` (eggers), plus referenced `Act/Anim/G_Johnny.act`, `Act/Vehicles.act` (bike swing + `Magnetized` nodes), `Act/Player.act`.

### Props / pickups / weapons

- Crane set: `JY1d_animlight`, `DPE_buttonOFF` persistent entities (area 43 coords), `MagnetHum` sound emitter.
- Lead pipe 418: Johnny's stage-2 weapon; the captured pipe is re-created as a persistent entity and flown along `_3_B_LEAD_PIPE_PATH`.
- Vehicles: 273 player bike, 282 Johnny's bike (entity flag 41 + forced bounding-sphere update), 295 cop cars ×3.
- Crate pickups: `DPE_CrateBrk` loot table overridden to `PickupListCrateHEALTH` during stage 2 (reverted in cleanup).

### Speech

`M_3_B` ids seen: 5, 9, 13, 18, 27, 28, 46; `M_3_B_2D` ids 3, 4; cop `CHASE`/`BUSTING` events. Roughly **9 distinct scripted lines** + barks. Music: 5-stream chain listed in §1.

### Named world data to extract from `3_B.DAT` + `3_B_BIKE_STUFF.DAT` (**two DAT files**, VERIFIED — both loaded)

- POINTLISTs: `_PLAYER_FIRSTSTART`, `_JOHNNY_FIRSTSTART`, `_JOHNNYBIKEFIRSTSTART`, `_PLAYERBIKEFIRSTSTART`, `_3B_ALLEYGREASER`, `_3B_COPS`, `_3B_COPSPAWN`, `_3B_BLOCKCOPCAR01/02`, `_3B_COPCHASECARSTART`, `_3_B_COPCAR_WARP`, `_3B_GREASERFLEETO/2`, `_3_B_JOHNNYTOBIKE`, `_GREASECHEER03..06`, `_CRATES_CHEERER/02`, `_CRATES_JUMPER01..03`, `_LAST_JUMPER01/02`, `_LAST_RUNNER01/02`, `_LAST_RUNNER_POINT`, `_JUMP_CRATES_2/3`, `_LAST_JUMP01/02`, `_3_B_GREASER_FLEE_POINT`, `_3_B_STAGE2_TELEPORT`, `_3_B_JOHNNY_SPAWN`, `_3_B_JOHNNY_BIKE_SPAWN`, `_3_B_STAGE2_THUG_A..F`, `_3_B_HELPER_SPAWN`, `_3B_PETERCRANESYNCH`, `_3B_MAGNETBLIP`.
- PATHs: `_3B_BLOCKCOPCAR01/02PATH`, `_3B_COPCARALLEYCHASE`, `_3_B_COPCARCHASEPLAYER`, `_JOHNNYFLEE01`, `_3B_PLAYERFLEECOPALLEY`, `_3_B_START`, `_3_B_PATH_A..F` (Petey legs), `_3_B_JV_BIKE_PATH_NW/SW/NE/SE` (circle-strafe), `_3_B_JV_MAGNET_PATH_SW/NE`, `_3_B_LEAD_PIPE_PATH`.
- TRIGGERs: `_3B_COPSSPEEDUP`, `_GREASERS_JET01..05`, `_JOHNNYRAMBLE02`, `_3_B_STAGE_2_TRIGGER`, `_3_B_BIKE_REGION_NE/NW/SE/SW`, `_3_B_MAGNET`, `_3_B_REGION_S`, `_AREA_AROUND_BUTTON`.
- Hardcoded cameras: alley (-582.79, -594.27, 8.51 → …), bike-flee (-585.07, -622.29, 7.18 → …), Petey crane (-745.02, -626.37, 6.70 / -731.67, -630.01, 6.10 / -745.90, -625.62, 4.38).
- Area codes seen: **43** (alley start), **0** (junkyard).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| B0 setup | `mission framework` | `weapon_inventory_snapshot` (2_B pool — reuse); **rain_force_off** (fold into `weather_force`) |
| stage resume | `script_checkpoint` (2_04 doc) | reuse; note 3_B's variant stores stage in `shared.*` and swaps whole stage functions — same primitive |
| B1 alley NIS | `letterbox`, `camera`, `pair_anim`, `npc_follow_path`, `spawn_npc` bundles | **scripted_bust_vignette** (cops chase/bust specific peds: `PedSetPunishmentPoints` + `PedAttack` cop + flee — set-dressing only) |
| B2 chase | `vehicle_follow_path` (2_04 pool), `follow_cam preset` (`vehicle_camera_preset`) | **pursuit_car** (driver ped warped in, siren on, path-follow, **distance-banded cruise speeds** after an arming trigger); **arrest_fail** (proximity box to driver OR off-bike countdown → control off, busted bark, fall anim, HP-zero fail with keyed text) |
| B2 encounters | `trigger`-chained beat machine | **trigger_chain** (ordered one-shot triggers gated on handler index — generalizes ch1 sequence triggers; spectacle spawns: `npc_jump_entrance` reuse for crate jumpers) |
| B3 circle-strafe boss | `boss_profile`, `damage_taken_multiplier` (2_B pool) | **region_path_selector** (which-of-4-region-triggers → next quarter path; heading-quadrant fallback inside an inner radius; magnet override path pair when flag set); **mounted_boss_cycle** (charge at player pos → swing pair → resume circling, 6-10 s cadence; auto-remount loop if dismounted) |
| B3 Petey legs | `npc_follow_path` + node cb, `npc_stat_override` | **ally_advance_on_guard_kills** (ally walks a path chain; crouches at each leg end; advances only when the guard assigned to that leg dies; end beat = interact anim + flag) |
| B4 capture | `prop_set_state` | **trap_capture_beat** (boss enters trigger while flag set → sleep + weapon-destroy + prop path anim of the seized weapon + proceed) |
| fails | health poll + keyed fail text | none new (`countdown_fail_timer` pool covers the off-bike countdown shape) |
| B6 pass | `completion_banner`, `yearbook_unlock` (2_B pool), `chapter_set`, `force_complete` (pool) | **camera_photo_upgrade** (`HUDPhotographySetColourUpgrade` — fold into a photo-system unlock primitive with the 2_B save-level calls) |

## 6. Open questions

- `G_Johnny.act` tree internals for stage 3 (what `F_FightCondition`/`F_DodgeCondition` actually gate; nothing in this Lua ever sets `bJohnnyKnockDown`/`bJohnnyCanAvoid` — the tree or a missing thread may; flagged dead-code-adjacent).
- `shared.g3BMissionState = STAGE_BIKE_BATTLE` inside `F3_StageThreeSetup`: bug or deliberate retry-to-stage-2; who seeds `shared.g3BMissionState` on first launch (mission table outside the Lua — presumably the 3_08 "Tenements"-era chain or the launcher sets STAGE_START).
- Cop driving mode 3 semantics (`VehicleSetDrivingMode`) and ped flag 107 on Johnny.
- How stage 2's eggers interact with the magnet capture timing if killed before Petey arrives (scripted order is guards-then-crane; both gates must be true).
- The chase's relationship to the baked `"3-B"` intro and the mission table chain 3_08 → 3_B → `Chapt3Trans`.
