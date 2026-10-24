# 5_04 — The Gym Is Burning: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_04.lua` (1881 lines, complete). No `SMissPass.lua` branch exists for `5_04` — all pass effects are in-file (**VERIFIED**, SMissPass read). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_04 = "The Gym Is Burning"** (canonical Chapter 5 list, mission #56, 8AM–9PM) — **VERIFIED** by content: the gym interior (area 13) is on fire, three jocks are trapped, the player puts out banner fires with the extinguisher and frees them, and the culprit hiding in the changeroom (the dropout arsonist, `DO_MODEL = 46`, `idDO` = "dropout", Gurney per the story) shoulder-charges the player and escapes. Catalog cross-check: `docs/story-mode-catalog.md` #56. Note script id ≠ catalog row (5_04 is the *fourth* ch5 script but the #56 mission; Cheating Time is engine `3_S03`).

Script-id side effects in shared files (**VERIFIED**): `SFaction.lua` suppresses Burton's unique model (55) while `5_04` is *available* and restores it once completed (mission-availability-driven unique-ped gating, same `unique_ped_suppress` pattern as ch2/3 docs). No `SMissPass.lua` pass branch.

Major structural finding: **the file contains a fully scripted second half (a mounted town chase of the fleeing dropout with ambushes and cop cars) that is unreachable in normal play** — `F_Changeroom` sets `gTheMissionWasPassed = true` at the end of the changeroom NIS, so `main()`'s loop exits to the pass banner while the player is still inside area 13, before the `AreaGetVisible() == 0` check in `F_Stage01InTheGym` can advance to `F_Stage02OutOfTheGymSetup`. Supporting evidence that stage 2 is cut/unshipped: `F_Stage02OutOfTheGym` (the stage poll) is an empty body, `F_CreateFirstCops`/`F_CreateSecondCops` are empty (their `F_FirstCops`/`F_SecondCops` counterparts do spawn cars), `F_CreateFourthAmbushPeds`/`F_CleanupFourthAmbushPeds`/`F_Ambush05`/`F_DOEscape` are empty, and `F_DOStop02`'s health check has an empty then-branch (**INFERRED** cut content — matches the shipped game ending the mission in the gym; the chase code is documented in §2-S2 for completeness since it is real, wired logic).

Dead/loose ends: `F_MonitorHealth` (player-KO check, never registered in `L_ObjectiveSetParam`), `gJocksFree[5]` mapped from `TRIGGER._5_04_BANNERS_06` — no fire is ever created on banner 06 (initial fires: 02/07/08/10/11/12; dynamic add: 03), `F_AttackerFlee`/`F_PedFlee` (flee-stat helpers, the latter unreferenced), `F_WaitingForExtinguisher` (duplicates the stage-1 poll's extinguisher block, never called), the `gTimeForRespawn`/`gCurrentRespawn`/`gCurrentRespawnedFires` fire-respawn globals (never consumed — a fire-respawn mechanic was planned, **INFERRED**). One cross-chapter curiosity: the intro plays one line from the `M_4_04` speech bank (id 51) amid `M_5_04` lines (**VERIFIED**, likely a bank-reference slip in the original).

## 2. Mission flow

Driver: `main()` polls a `gStageFunction` pointer each frame; parallel work runs in `LibObjective`'s `T_ObjectiveMonitor` (declarative success/fail conditions), `LibTrigger`'s `L_MonitorTriggers` (19 registered trigger callbacks), plus `F_MonitorAlpha` (fire overlay) and `T_ExtinguisherCheck` (equipment watchdog) threads. No checkpoints: fail replays from the top; on fail the player is dumped to `_5_04_PSTART_TEST` (area 0).

| # | Beat | Logic (functional) |
|---|---|---|
| F0 | **Baked cutscene "5-04"** | `PlayCutsceneWithLoad("5-04", true)`; `MissionDontFadeIn`; `DATLoad("5_04.DAT", 2)`; player teleported to `_5_04_BURTON` if not already within 15 m (mission starts facing Burton's spot, **INFERRED**); six exterior smoke/fire effects at `_5_04_OUTSIDESMOKE` points dress the burning gym from outside; `shared.gGymHasBurnt = true` (world-state flag; reset to false on cleanup unless passed — consumed elsewhere, **OPEN** consumer). Weapon models pre-requested: 303, 310, 311, 326, 331, 346, 349 (most are stage-2-only). Four `PGymLights` geometry instances toggled on. |
| F1 | **Into the burning gym** | Area transition to 13 at `_5_04_COP_END` point 3; gym + pool doors locked to the player but not to peds; population fully cleared/overridden; `EffectSetGymnFireOn(true)`; player flag 109 set (fire-related? **OPEN**). Props loaded via `L_PropLoad`: door barricade `_5_04_DOOR_HACK`, 13 banner PAnims, 4 gym lights, hoop, wall. `F_SetupFires` creates **6 damageable fires** on banner anchors (`FireCreate(trigger, health 1000, FIREDAMAGE 40, radius 100, scale 115, "GymFire")`, damage radius 1, prop health bars hidden); each live fire contributes +0.1 to a target fire-overlay alpha that a lerp thread drives via `EffectSetGymnFireAlpha`. `F_CreatePeds`: the dropout (46) kneels at `_5_04_GYM_DOSTART` (hidden in plain sight, **INFERRED** — he is never flagged to the player); three jocks spawn coughing — index 1 model 13 plays `KirbyInit`-family anims (Kirby), index 2 model 14 `CoughingMandy` (Mandy), index 3 model 20 `CoughingCasey` (Casey) — invulnerable, untargetable, stimuli/attacks ignored, attitude-to-player 4, health 150, flag 111 cleared. |
| F1i | **Intro NIS (Kirby pinned)** | `F_Intro`: camera path `_5_04_JOCK_CAM` on Kirby, player NIS-safe; Kirby's taunt/defeat barks (one line from the `M_4_04` bank — see §1), then the gym wall prop (`_5_04_GYMWALL`, `GymWLad.act`) plays `Damage/Fall`, Kirby is moved into it, gravity disabled, `KirbyFall` anim — he is pinned under the fallen wall. Lamp 3 drops (`F_DropLamp(3)` → `AGymLght.act` `Damage/Fall`; destroyed lamps queue for `PAnimDelete`). Objective `5_04_11` + **210 s countdown** (`GYM_TIME`) start; `MS_BikeChaseMid.rsm` plays. |
| F2 | **Extinguisher gate** | `T_ExtinguisherCheck` thread: if the player doesn't have weapon 326 (extinguisher) equipped, a blip marks it (first a hardcoded gym point, then re-marked at the player's position whenever unequipped again — the re-mark at *player* position reads like an original bug, **OPEN** intent). Once equipped: objective 1 completes; objectives `5_04_35` (fires) and `5_04_36` (jocks) appear with blips on all three jocks. |
| F3 | **Put out fires / free the jocks** | Fires are extinguished by damaging them to 0 (`FireGetHealth` poll in `F_FiresOut`); each fire-out drops the overlay alpha by 0.1 and optionally spawns a `SmokeStackLRG` at the anchor. Fire→rescue mapping: banner 07 → Mandy (stands, thanks line, walks `_5_04_JOCKPATH01`); banners 02+11 → the wall pinning Kirby becomes usable (`GymWLad` → `OnGround/Useable`, corona blip); banner 03 (added dynamically, see F3b) → Casey walks `_5_04_JOCKPATH03`; banner 06 → dead slot (no such fire). Freed jocks get `F_ResetJock`: stimuli/attacks re-enabled, vulnerable, targetable, **mission-critical with `CbPlayerAggressed`** (health ≤ 145 → fail `5_04_07` — don't beat up the victims). |
| F3b | **Casey under the hoop** | Player within 14 m of Casey's spot → the gym hoop prop falls (`GymHoop.act` `Damage/Fall`), Casey plays the pinned fall anim, corona blip appears. `gFinishedGymHoop` (external prop-interaction hook, **INFERRED** area/prop script reports the lift) → Casey crawls out (`CrawlCycle` until the anim ends), gravity restored, lamp 2 drops as a hazard, **a new fire spawns on banner 03** (alpha +0.2), Casey coughs free. Same pattern for Kirby via `gFinishedGymWall`: crawl-out, then walks `_5_04_JOCKPATH02`. |
| F3c | **Mandy waits for the player** | Mandy path-node callback (`CbMandyPath`, node 7) → she faces the player and loops freak-out barks every 10 s (50/50 line pick while Kirby or Casey still trapped). Jocks reaching their escape-path end (`CbJock01Path` node 7) are deleted. |
| F4 | **All out → Mandy's tip** | `F_FiresOut` true when all fires dead **and** Kirby and Casey freed (if fires die first, objective 2 completes and the stage waits on the rescues; objective 3 completes once all three walk off). → `F_JockConv` NIS: timer stopped, widescreen, player teleported to `_5_04_PLAYERMANDYCONV`, five speech beats (ids 22–26) with camera cuts, `GS_Female_A` tree swap on Mandy. Afterwards: objective `5_04_12` (check the changeroom), **new 45 s timer**, `gCanAccessBathrooms = true`, Mandy exits via `_5_04_BURTON_OUT`. |
| F5 | **Changeroom: the culprit bolts** | Bathroom access is fenced by two triggers: `_5_04_PREVENT01` prints a turn-back message; `_5_04_PREVENT02` hard re-routes — control off, message, fade, teleport to `_5_04_PREVENT_RETURN`, face heading −92.9° (no fail). Entering `_5_04_CHANGEROOM` → `F_Changeroom`: player healed +20 if ≤ 30 hp; the dropout plays `GurneyCrouchOut`, both walk scripted paths, shoulder-charge beat (`RunShoulder`, infinite sprint, blip on him), fade, camera cut to the gym exit, `MS_BikeChaseHigh.rsm`; he flees `_5_04_DO_ESCAPE_GYM` (node 7 → gym door closes behind him + a 15 s exit timer arms — moot, see below); gym/pool doors unlock. **`gTheMissionWasPassed = true` — the mission passes here.** The armed 15 s timer (`gExitTimer`) would fail `5_04_06` if the player were still inside after it, but the pass fires first (**INFERRED** vestige of the open-ended escape). |
| F6 | **Pass** | Fixed camera outside, player `PlayerScratch` anim, completion banner `M_PASS` (3000 ms) + respect message `MRESPECT_JP15`, pass stinger 8, **faction 2 (jocks) +15 respect**, punishment points zeroed, `MissionSucceed(false, false, false)`. `shared.gGymHasBurnt` stays true. |

### Stage 2 (present but unreachable — cut mounted chase, **INFERRED**)

Had it run: leaving area 13 would spawn the dropout at `_5_04_DO_START` with his bike (282), a player bike (273), a little-boy NPC (69) whose bike gets stolen (`BIKE_STOLEN` speech event — the dropout takes his own bike, or **the player's bike if the player took his**), and two football jocks (15, 18; weapon 331, `PedPassBall` at the player when approached within 10 m of `_5_04_ANGRY_JOCKS_01`). The dropout flees `_5_04_SCHOOL_ESCAPE` with a 13-entry stat-override bundle, then three escape legs (`_5_04_BIKE_ESCAPE_01/02/03`) with per-leg stops; `F_MonitorDO` enforces a two-tier distance leash (warn > 50/55 m via `5_04_05`, fail > 65/75 m → `5_04_06`) and triggers the last stand at `_5_04_DO_GOAL` (dismount, `PedAttackPlayer`). Ambush chain: A1 a dropout fells a tree (`TreeFall.act` `Damage/Fall`) then flees; A2 two mounted dropouts on bikes; A3 three on foot set hostile; A4 empty; A5 three more mounted. Six cop cars (295) at `_5_04_COP_CAR_01-06`. Dropout health at 200; health-gated taunts (> 75 %, > 50 %); bark loop every 8–10 s pre-discovery (`F_DOChat`). KO of the dropout → `F_CompleteMission` (all ambushers force-fled to ambient, punishment zeroed) — this pass path is unreachable; the changeroom pass fires first.

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Leave the gym (area ≠ 13) before finding the culprit | `objDiscoverDO` fail condition `F_LeaveGym` → `F_FailMission` | generic (`gMissionFailedMessage` unset) |
| 210 s fire timer expires | `objTimer` fail `F_TimerFinished` → `F_CopsArrived` NIS (three cop models 83/97/82 spawn at `_5_04_COP_END`, camera path `_5_04_JOCK_CAM`) → fail | `5_04_04` |
| 45 s changeroom timer expires (same `objTimer`) | same | `5_04_04` |
| A freed jock's health driven ≤ 145 | `CbPlayerAggressed` mission-critical callback | `5_04_07` |
| (Stage 2, dead) lose the dropout / let him escape | `F_MonitorDO` distance bands, `gExitTimer` | `5_04_05` (warn) / `5_04_06` |

Fail presentation: control off, NIS-safe, fail stinger 8, `M_FAIL` banner with the keyed reason, doors unlocked, fade, teleport to `_5_04_PSTART_TEST` (area 0), `MissionFail(false, false)`. No checkpoints — full replay; retry skip of baked `5-04` not handled in-file (**OPEN**, engine-side per ch1 pattern).

### Pass-side effects

- In-file only: jocks (faction 2) +15 respect, `MRESPECT_JP15` message, banner, stinger 8, punishment points zeroed, `shared.gGymHasBurnt` latched true.
- **No `SMissPass.lua` branch for `5_04`** (**VERIFIED**). `SFaction.lua` re-enables Burton's unique model on completion. (Fan wikis list no item/clothing grant for this mission — consistent with the in-file-only set.)

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | F0 intro | baked (`5-04`) | `fade` in at `_5_04_BURTON` + `letterbox` + one `say` beat |
| CS-2 | F1i Kirby pinned | in-engine, ~6 s | `letterbox` + `camera` path + `say` ×2 + `prop_set_state` (wall fall) + pinned NPC anim |
| CS-3 | F3b hoop/wall lifts ×2 | in-engine micro-beats | `prop_set_state` + crawl-out `ped_action_node` + lamp-drop hazard |
| CS-4 | F4 Mandy's tip | in-engine, ~15 s | `inline_cutscene_chain`: `fade` + 4 camera cuts + 5 `say` beats + teleport-to-conversation-point |
| CS-5 | F5 changeroom charge | in-engine, ~8 s | `letterbox` + scripted path pair + `say` ×2 + fixed `camera` + music swap |
| CS-6 | F0-fail cops arrive | in-engine fail NIS | `fail_taunt_scene` (3_R09): spawn authority trio + `camera` path + keyed fail banner |
| CS-7 | F6 pass | in-engine, ~3 s | fixed `camera` + `pair_anim`-adjacent solo anim + `completion_banner` + `set_faction_respect` |

Cutscene-adjacent anim groups: `NIS_5_04`, `Area_Asylum`, `MINICHEM`, `1_03The Setup`, `F_Nerds`, `GEN_Social`, `2_S04CharSheets`, `F_Adult`, `Ambient3`. Conv tree `Act/Conv/5_04.act`. Music: `MS_BikeChaseMid.rsm` (gym), `MS_BikeChaseHigh.rsm` (chase NIS), stinger id 8 both ways.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 46 | the dropout arsonist (Gurney; `idDO`) | kneeling in the gym, invulnerable until the escape; stage-2 chase target at 200 hp with a 13-stat override bundle |
| 13 | Kirby (gymjocks[1]) | pinned under the gym wall; freed by fire pair 02+11 → wall lift |
| 14 | Mandy (gymjocks[2]) | freed by fire 07; delivers the changeroom tip (F4 NIS) |
| 20 | Casey (gymjocks[3]) | pinned under the hoop; freed by hoop lift → fire 03 spawns |
| 83, 97, 82 | cops (fail NIS) | `_5_04_COP_END` trio |
| 69 | little boy (stage 2, dead) | bike-theft victim at `_5_04_DO_BIKE` |
| 15, 18 | angry jocks (stage 2, dead) | football pass attack (`PedSetWeapon` 331 + `PedPassBall`) |
| 41–45 | dropout ambushers (stage 2, dead) | `F_GetDOModel` round-robin avoiding the last three picks; mounted pairs get bikes (273) |

### Props / world systems

- PAnim sets: 13 banners (fire anchors), 4 gym lights (`AGymLght.act` fall + `GymLightSmash` effect + deferred `PAnimDelete`), gym wall (`GymWLad.act`: Fall / OnGround/Useable), gym hoop (`GymHoop.act` Fall), door barricade `_5_04_DOOR_HACK`.
- Fires: `FireCreate`/`FireGetHealth`/`FireDestroy` ×6 (+1 dynamic), `EffectSetGymnFireOn/Alpha` global overlay, `SmokeStackLRG` aftermath puffs.
- Doors: `_DT_GYM_DOORL`, `_DT_POOL_DOORL` (locked player-only during stage 1; unlocked on the escape and on fail).
- Geometry: 4× `PGymLights` instances.
- Speech: `M_5_04` ids 2–45 used; one stray `M_4_04` id 51; ambient banks `DEFEAT_INDIVIDUAL`, `VICTIMIZED`, `BIKE_STOLEN`.
- Extinguisher: weapon 326 equip watchdog (see F2).

### Named world data to extract from `5_04.DAT` (present — `DATLoad("5_04.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_5_04_BURTON`, `_5_04_PSTART_TEST`, `_5_04_COP_END`, `_5_04_GYM_DOSTART`, `_5_04_GYM_JOCK_01/02/03`, `_5_04_PLAYERMANDYCONV`, `_5_04_PREVENT_RETURN`, `_5_04_INGYM_END`, `_5_04_OUTSIDESMOKE` (6), `_5_04_DO_START`, `_5_04_DO_BIKE`, `_5_04_PBIKE`, `_5_04_ANGRY_JOCKS_01`, `_5_04_AMBUSH_01`, `_5_04_AMBUSH_05-09`, `_5_04_AMBUSH_05/06/14/15/16_BIKE`, `_5_04_AMBUSH_14-16`, `_5_04_COP_CAR_01-06`.
- PATHs: `_5_04_JOCK_CAM`, `_5_04_JOCKPATH01/02/03`, `_5_04_BURTON_OUT`, `_5_04_PLAYERCHARGED`, `_5_04_CHANGEROOMCHARGE`, `_5_04_DO_ESCAPE_GYM`, `_5_04_SCHOOL_ESCAPE`, `_5_04_BIKE_ESCAPE_01/02/03`, `_5_04_AMBUSH_02/05`.
- TRIGGERs: `_5_04_DOOR_HACK`, `_5_04_BANNERS_01-13`, `_5_04_LIGHT_01-04`, `_5_04_GYMHOOP`, `_5_04_GYMWALL`, `_DT_GYM_DOORL`, `_DT_POOL_DOORL`, `_5_04_DO_NEAR_BIKE`, `_5_04_BIKE_STOP_01/02`, `_5_04_COP_START_01/02`, `_5_04_AMBUSH_01-04`, `_5_04_LASTAMBUSH`, `_5_04_DO_STOP_01`, `_5_04_CREATE_FOURTH_PEDS`, `_5_04_DO_CHAT`, `_5_04_CHANGEROOM`, `_5_04_DO_ESCAPE`, `_5_04_PREVENT01/02`, `_5_04_OTHERROUTE`, `_5_04_DO_GOAL`.
- Hardcoded coords in-file: changeroom-NIS camera, pass camera, wall/hoop coronas, extinguisher point, Kirby/Casey move targets.
- Area codes seen: **0** (exterior) and **13** (gym interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1–ch4 set | New primitives needed |
|---|---|---|
| F0 baked cutscene + world flag | `fade`, `letterbox`, `world_state_swap` (`shared.gGymHasBurnt`) | none new |
| F1 area lockdown | `population_override` + `clear_ambient_peds` (3_05), `door_lock_set` | none new — the player-locked/ped-unlocked gym doors are exactly `door_lock_peds_split` (5_07a doc) |
| F1 fires | `fire_prop_system` (2_R04: `FireCreate`/health/out events bound to props) — same calls, tuned for extinguishing instead of protecting | **global_fire_overlay** (`EffectSetGymnFireOn` + `EffectSetGymnFireAlpha`: one room-wide fire-intensity overlay whose level is the smoothed sum of per-fire contributions — 0.1 per live fire, lerped on a thread; per-fire-out events decrement it) |
| F2 extinguisher gate | `weapon_equipped` condition + `hint_when_unarmed_in_zone` (3_05) + `dynamic_blip` (1_G1) | none new (composite; note the re-mark-at-player-position oddity) |
| F3/F3b pinned jocks | `prop_set_state`, `corona_interact`, `mission_critical_npc`, `npc_follow_path` node events | **pinned_prop_rescue** (an NPC is trapped under a PAnim prop: prop plays its fall anim onto a gravity-disabled NPC → prop enters a "usable" state with a corona blip → player interaction reports through an external prop-finished hook → NPC plays crawl-out, walks an escape path, then re-arms as a mission-critical ped with a hurt-the-victim fail threshold) |
| F5 bathroom fence | `soft_fail_eject_reset` (2_S06) is fail-adjacent; this is gentler | **zone_return_teleport** (entering a forbidden trigger: control off + keyed message + fade + teleport to a return point + forced heading, mission continues — no fail, no objective rollback) |
| F5 culprit charge | `npc_follow_path` node events, `player_follow_path` (2_S06) | none new |
| F4/F1i/CS beats | `inline_cutscene_chain` (4_04), `conversation_beat_machine` | none new |
| fails | `countdown_fail_timer` (3_01A), `fail_taunt_scene` (3_R09 — cops-arrive NIS), `mission_critical_npc` threshold fail | none new |
| stage 2 chase (dead) | `npc_flee_path_mounted` (2_02) incl. the steal-the-player's-bike variant, `mounted_chaser_spawner` (3_04), two-tier `npc_distance_leash` (2_02), `vehicle_area_purge` (2_07), `scripted_vehicle_arrival` | none new — document only; do not build |
| pass | `completion_banner`, `set_faction_respect`, `punishment_points_add` (zeroing) | none new |

## 6. Open questions

- `PedSetFlag(gPlayer, 109, ...)` and `PedSetFlag(jock, 111, false)` flag semantics (109 also seen in other fire/burn contexts? not surveyed).
- Consumer of `shared.gGymHasBurnt` (charred gym dressing / gym-closed state in free roam — presumably an area script; confirm when area scripts are surveyed).
- `FIREDAMAGE = 40` + `FireCreate` args (health 1000, radius 100, scale 115) units; how extinguisher spray damages a fire engine-side.
- Who calls the prop-finished hooks (`gFinishedGymHoop`/`gFinishedGymWall` setters) and the lamp-drop triggers (`F_DropLamp` call sites 2/3 are in-file; 1/4 presumably fire-damage-driven externally) — area/prop scripts, same `area_script_shared_state` pattern as ch3/4.
- The re-mark-at-player-position extinguisher blip (F2) — original bug or "you dropped it" marker.
- Whether retry skips baked `5-04` (no attempt check in-file).
- The `M_4_04` bank line in the intro — bank reuse or original typo.
- All trigger/point geometry (pending DAT extraction); stage-2 DAT content exists but can be deprioritized with the chase confirmed cut.
