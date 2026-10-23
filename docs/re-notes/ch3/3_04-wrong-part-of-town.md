# 3_04 — Wrong Part of Town: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_04.lua` (2092 lines, complete) + `SMissPass.lua` (`3_04` branch). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_04 = "Wrong Part of Town"** (canonical Chapter 3 list, mission #39, 8AM–11PM) — **VERIFIED** by the loaded animation group `"3_04WrongPtTown"` (mission title embedded in the group name) and by content: greasers Johnny (model 23) and Gary (130) hold Cornelius (9) in New Coventry; the player then escorts Algie (4) to Chad (32) and Lola (25), and the back half is a greaser bike chase through the poor district with the player riding on the back of Chad's bike. Catalog cross-check: `docs/story-mode-catalog.md` #39. No surprise.

Dead/loose ends in the file (never fire from the live path): `F_routeJohnnyLeave` (registered nowhere; the J-crew cleanup is driven by `bCleanupJCrew` from the stage-1 poll instead), `F_routeChaseBreakOff01/03` + `bDeleteExtraGoon01/03` + the `pedExtraGoon01-03`/`vehicleExtraGoon01-03` table entries (a cut "chasers break off" beat — the extra goons are never spawned on the live path, **INFERRED** cut), `F_StartAtStage2/3/4` debug entry points (`bDebugFlag`/`gDebugLevel`), one duplicated `F_WaitForSpeech` definition (the later one wins), and a `T_SaveCorn` thread that is defined but never created.

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → `F_Stage1()` … `F_Stage4()`, each stage = setup + a poll-every-frame loop over a `StageN_Objectives()` function; two in-engine skippable cutscene threads (`T_CutsceneCornelius`, `T_CutsceneBikeEscape`) and three service threads (`T_MonitorCornBullies`, `T_MonitorBikers`, speech threads). No checkpoints: any fail replays from the top.

| # | Beat | Logic (functional) |
|---|---|---|
| W0 | **Baked cutscene "3-04"** | `PlayCutsceneWithLoad("3-04", true)` in `MissionSetup`; `MissionDontFadeIn`, `DATLoad("3_04.DAT", 2)`. No in-file attempt-count skip — **OPEN** (ch1 pattern suggests engine handles retry skip). |
| W1 | **Setup** | Anim groups (`3_04WrongPtTown`, `NIS_3_04`, `G_Johnny`, cheer/spectator/nerd sets), conv tree `Act/Conv/3_04.act`; unique models 32/4/25/9 suppressed for the mission (`unique_ped_suppress`); ped models {9,23,130,22,28,29,27,21,4,32,25} and bike models {281,273,282} loaded; nerd-faction attitude toward faction 13 (player-side, **INFERRED**) set to 4; two named ambient sound emitters (`Poor_AutoBodyShop`, `Poor_TennmentsDay01`) disabled; player max health recorded. |
| W2 | **Stage 1: the shakedown tableau** | Player transitioned to `_3_04_SPAWNPLAYER` (area 0). `F_LoadBullyScene` stages a live scene: Cornelius stationary with doubled health and a blip; Johnny grapple-holding him (`PedSetGrappleTarget`) in a looping shove anim; Gary + four greasers (22/28/29/27) in a cheer ring (`Cheer_Cool3` nodes 11-14); Johnny, Gary and all greasers doubled health + infinite sprint. Objective `3_04_MOBJ_01`. Approach trigger `_3_04_CORNSCREAM2` fires the ambient bark chain (`T_CornScream`). |
| W3 | **Stage 1 interrupt: save Cornelius** | Entering `_3_04_CORNELIUSCUT`, or the player striking any of the seven scene peds (`T_MonitorCornBullies`, who-hit-me-last filter), starts `F_CutCornelius01`: fade + fixed FOV-40 cameras + `MS_Confrontation_NIS.rsm`, skippable cutscene thread (Johnny/Gary speeches and walk-off along `PATH._3_04_JOHNNYLEAVE`). Afterwards: Cornelius cowers, is recruited as ally and made mission-critical (fail key 1); greasers 03/04 attack him with blips; Johnny/Gary/greasers 01-02 deleted. Objective `3_04_MOBJ_02`. Cornelius has a **zone-ally toggle**: while the player stays inside `_3_04_CORNELIUSCUT` he stays recruited; leaving dismisses him and shows his health bar (`3_04_HEALTH_CORN`), re-entering re-recruits. When both attacker greasers are dead → `F_CutCornelius02`: Cornelius thanks the player (speech 12), ally/critical cleared, he walks `PATH._3_04_CORNLEAVE` and is deleted at node 2. **Fail:** Cornelius dies any time after the save starts (key 1). Also, the player KO'ing any tableau ped before the cut = immediate generic fail. |
| W4 | **Stage 2: meet Algie, Chad and Lola** | Interactive stream `MS_Running{Low02,Mid,High}.rsm`. Algie spawned stationary/ignore-stimuli/ignore-attacks with min-health pinned at current (`PedSetMinHealth`), plus Lola and Chad (Chad faces Lola; flag 129 on Algie). Objective `3_04_MOBJ_03`, blip on Algie. Bikes 281/273 lazy-created when the player enters `_3_04_LOADBIKES`. On `_3_04_ALGIEFLEE` or the player hitting Algie: scripted beat — control off, widescreen, dismount, player runs `PATH._3_04_CHASEALGIE`, bark (speech 15), Algie's `OhFace` reaction anim under two fixed cameras, both walk to Lola's spot → stage 3. |
| W5 | **Stage 3 setup: baked cutscene "3-04B" + chase build** | Punishment points zeroed, ambient vehicles and population overridden to a near-empty mix (`AreaOverridePopulation(3,0,…,1,1,0)` — slot semantics **OPEN**), POI disabled, player force-dismounted. Stage-2 peds deleted, models/anim groups reloaded around `PlayCutsceneWithLoad("3-04B", true)`. Player repositioned `_3_04_SPAWNPLAYERSTG3`; Chad respawned at 2× health (invulnerable/untargetable/ignore-attacks); Algie, Norton (29, stationary — a temp NIS actor deleted mid-scene), greasers 01/02 on bikes 282 at `_3_04_SPAWNGPED01/02`; player's last bike deleted (anti-chase cheese, **INFERRED**). Skippable in-engine intro (`T_CutsceneBikeEscape`: Algie/Norton/greaser speeches 14/16/21/22, fixed cameras + FOV moves, player walked along `_3_04_STG3INTROJIMMY`). |
| W6 | **Stage 3: the bike chase** | `F_SetupBikeChase`: Chad + Algie mounted; **the player is bolted to the back of Chad's bike** (`PlayerFixToBackOfVehicle`, take-damage-while-attached on, camera rotation-locked relative to the bike, camera change disabled) and force-equipped with weapon 306 (super slingshot, per 1_09 notes, **INFERRED**) — the player shoots pursuers while Chad drives. Chad follows `PATH._3_04_BIKEESCAPE` (speed stat 24 → 40); 2.5 s later Algie peels off on `PATH._3_04_ALGIELEAVELOLAS` (speed 80) with a blip; player healed to full; punishment system disabled for the whole chase. Two starter chasers (health pinned to 40, chaser stat bundle — speed window ±10 around Chad, damage-taken multiplier class 3 → 0.05, move-out-of-way disabled on Chad) attack Chad armed alternately with 312/301. Objective `3_04_MOBJ_04`; objective reminders disabled. |
| W7 | **Chase escalation (all path-node driven)** | Chad's path-node callbacks run the show: nodes 10/16/25/32/33/40/41/46 of route 1 (and further nodes on routes 02/03/04) call `F_LaunchBikeDude(n)` — spawn a greaser+bike 282 at `_3_04_BIKEATTACKERS` node n, mount, arm (every 4th gets 312, else 301), chaser stats, attack Chad — but only while ≤2 chasers are live in the table (concurrency cap; `T_MonitorBikers` culls chasers who lost their bike or fell >50 m behind the player, making them flee ambient, and fires a one-time Jimmy bark on the first cull). Route 1 node 45 = `BikeJump`; node 44/46 = camera cut 2 + reset. Route 2: node 21 = all chasers wipe out (forced off bikes); node 24 = "lost them" dialogue (speeches 27/28) + camera cut 3 (area peds cleared, player disarmed, camera on `PATH._3_04_CAMBIKE03`) + Algie respawned at `_3_04_STG3WARPALGIE` to rejoin; node 32 = **hill ambush cutscene** (`F_CutEscape`: greasers 01/04 on bikes 03/04 at the hill points, speeches 30/31/33, Algie rerouted to `PATH._3_04_ALGIETRAPTRAP`, Chad to route 03, stream up to `MS_BikeFastHigh`). Route 4 nodes 1-28 launch up to 12 more chasers (spawn ordinals 11-22); node 29 = camera cut 4; node 37 = stage 4. Camera cuts 1/5 also exist (route positions **OPEN** — set from nodes 44/route2-10 respectively; cut 1's trigger not seen in the read path). |
| W8 | **Stage 4: walk Algie home** | Chase stream stopped, population/ambient vehicles reverted. Earnest (10) and Cornelius (9) spawned stationary/ignore-stimuli at the school-side points, both mission-critical (fail keys 3 and 1); player unbolted from the bike with a no-damage fall anim if still mounted. Algie respawned with his bike, recruited as ally, mission-critical (fail key 2), health bar `3_04_HEALTH_ALGIE`. Chad wanders off (leave speeches 34/35). Blip at `_3_04_STG4_OBJECTIVE`; objective `3_04_MOBJ_05`; punishment system + POI back on. |
| W9 | **Outro NIS + pass** | Player entering `_3_04_STG4_RETURN`: invulnerable player + nerds, fade, widescreen, player/Algie repositioned (`_3_04_OUTRO_JIMMY/_ALGIE`), mutual target locks, FOV-30 fixed camera, four sequential speech+anim beats (Cornelius 37, Earnest 38, Algie 39, Jimmy 40), camera swap, nerds walk off, `bMissionPassed`. Pass block: greaser respect −5 (faction 4), nerd respect +5 (faction 1), banner `M_PASS` + respect messages `MRESPECT_NP5`/`MRESPECT_GM5`, pass stinger 8, nerd trio deleted, `MissionSucceed(false,false,false)`, fade back, control on. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Cornelius KO'd after the save beat | `PedIsDead` poll / mission-critical callback | `3_04_FAIL_01` |
| Algie KO'd (stage 4 escort) | mission-critical callback | `3_04_FAIL_02` |
| Earnest KO'd (stage 4) | mission-critical callback | `3_04_FAIL_03` |
| Player hits Earnest or Cornelius (100 damage worth) in stage 4 | who-hit-me-last poll; everyone made ambient first | `3_04_FAIL_04` |
| Player KO's any of the 7 tableau peds in stage 1 before the cut | `T_MonitorCornBullies` death check | generic (`MissionFail`, no key) |

No timers, no countdowns. Fail music stinger 8. Retry = full mission replay.

### Pass-side effects

- In-file: greasers −5 / nerds +5 faction respect; banner + two respect messages.
- `SMissPass.lua` `3_04` branch: **grants weapon 307 if the player doesn't have one** (`ItemGetCurrentNum(307) == 0 → GiveWeaponToPlayer(307)`) — **VERIFIED** in code; per the fan wikis Wrong Part of Town unlocks the bottle-rocket launcher, so 307 = bottle-rocket launcher is **INFERRED** (consistent with the mission's bike-chase shooting theme).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | W0 intro | baked (`3-04`) | `fade` in at `_3_04_SPAWNPLAYER` + `letterbox` + one `say` beat |
| CS-2 | W3 Cornelius rescue | in-engine, skippable, ~30 s | `inline_cutscene_chain`: `letterbox` + 2 fixed `camera` + `speech_sequence` + paired `ped_action_node` + NPC walk-off `npc_follow_path` |
| CS-3 | W4 Algie spot | in-engine micro-scene | control off + `player_follow_path` + 2 `camera` cuts + one `say` + `ped_action_node` reaction |
| CS-4 | W5 pre-chase | baked (`3-04B`) + in-engine skippable thread | `inline_cutscene_chain` with `camera` FOV/cuts + `speech_sequence`; mount NPCs on bikes during it |
| CS-5 | W7 hill ambush + 5 chase cam cuts | in-engine, mid-gameplay | **chase_camera_cuts** (new, below); the ambush itself is a mini `inline_cutscene_chain` that keeps the world simulating |
| CS-6 | W9 nerd send-off | in-engine, ~15 s | `letterbox` + fixed `camera` ×2 + `speech_sequence` (4 lines) + `pair_anim`-style mutual lock |

Cutscene-adjacent anim groups: `3_04WrongPtTown`, `NIS_3_04`, `G_Johnny`, `SNERD_I`/`SNERD_S`, `Cheer_Cool1/3`, `NPC_Spectator`, `NPC_Adult`, `Cheer_Nerd1`.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 23 | Johnny Vincent (tableau villain) | grapple-hold on Cornelius, shove loop, 2× health, infinite sprint; deleted after W3 |
| 130 | Gary | cheer ring, 2× health; deleted after W3 |
| 9 | Cornelius (victim → escortee at school) | 2× health, zone-ally toggle, health bar; mission-critical keys 1/… |
| 4 | Algie | stationary meet (min-health pin, flag 129), chase companion (speed-80 bike path, warp-respawn mid-chase), stage-4 escort ally with health bar; mission-critical key 2 |
| 32 | Chad | 2× health, invulnerable driver of the chase bike; own speed-stat overrides (stat 24 = pace); wanders off in W8 |
| 25 | Lola | stationary meet dressing in W4 only |
| 29 | Norton | stationary temp actor for the stage-3 intro cutscene; deleted mid-scene; reused as hill-ambush greaser model |
| 22, 28, 29, 27, 21 | greasers (cheer ring, attackers, chasers) | chaser spawns round-robin `tableGreaserModels` {27,29,21,28,22}; health pinned 40; weapons 312/301 alternating by spawn ordinal |

Weapon **312** (chaser thrown weapon ×100) and **301** (chaser weapon ×100) — **OPEN** ids (312 already flagged in 2_07/1_09 notes as the attackers' thrown weapon, eggs **INFERRED** there). Weapon **306** = player chase weapon (super slingshot, per 1_09 notes, **INFERRED**). Reward weapon **307** — see §2 pass-side effects.

### Vehicles

- 281 = Algie's bike; 273 = Chad's bike (the chase vehicle — the player is a passenger, never drives); 282 = greaser chase bikes (spawned per launch node); all bikes' world instances created lazily via `_3_04_LOADBIKES` / `_3_04_SETUPCHASERS`.

### Speech

Scripted `M_3_04` ids used: 1, 2, 4-8, 10-12, 14-16, 21, 22, 24-28, 30-40 — **~28 distinct lines** (very large category) plus generic `FIGHT_WATCH` barks, one `BOISTEROUS`/`COMPLAIN` ambient, and one out-of-bank line (`M_2_R11` id 10 in chase-cam shot 2 — shared bank reuse, **OPEN** why). Music: `MS_Running{Low02,Mid,High}.rsm`, `MS_BikeFast{Low,Mid,High}.rsm`, `MS_Confrontation_NIS.rsm`; stingers id 8.

### Named world data to extract from `3_04.DAT` (present — `DATLoad("3_04.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_3_04_SPAWNPLAYER`, `_3_04_DEBUGSTAGE2`, `_3_04_SPAWNPLAYERSTG3`, `_3_04_STG4_SPAWNPLAYER/SPAWNEARNEST/SPAWNCORNELIUS/SPAWNALGIE/SPAWNALGIEBIKE/SPAWNCHAD/OBJECTIVE`, `_3_04_SPAWNEARNESTLIB`, `_3_04_SPAWNCORNELIUSSTG1`, `_3_04_SPAWNJOHNNYSTG1`, `_3_04_SPAWNGARYSTG1`, `_3_04_SPAWNGOON1-4STG1`, `_3_04_STG3CUT_TEMPNORTON`, `_3_04_SPAWNALGIESTG2/STG3`, `_3_04_SPAWNCHADSTG2`, `_3_04_SPAWNLOLASTG2`, `_3_04_BIKEALGIE`, `_3_04_BIKECHAD`, `_3_04_SPAWNGBIKE01/02`, `_3_04_HILLBIKE01/02`, `_3_04_HILLPED01/02`, `_3_04_STG3EXTRAGOON01-03`, `_3_04_SPAWNGPED01/02`, `_3_04_SETUPCHASERS`, `_3_04_BIKEATTACKERS` (≥22 nodes), `_3_04_STG3WARPALGIE`, `_3_04_CORNCUTJIMMY`, `_3_04_CAMSTG3INTRO/2/3`, `_3_04_OUTRO_JIMMY/_ALGIE`.
- PATHs: `_3_04_CORNLEAVE`, `_3_04_JOHNNYLEAVE`, `_3_04_CHASEALGIE`, `_3_04_BIKEESCAPE` (≥46 nodes), `_3_04_BIKEESCAPE02/03/04`, `_3_04_ALGIELEAVELOLAS`, `_3_04_ALGIETRAPTRAP`, `_3_04_CAMBIKE03`, `_3_04_STG3INTROJIMMY`, `_3_04_STG3INTROGB1/02`, `_3_04_HILL01/02`.
- TRIGGERs: `_3_04_CORNSCREAM2`, `_3_04_CORNELIUSCUT`, `_3_04_LOADBIKES`, `_3_04_ALGIEFLEE`, `_3_04_STG4_RETURN`.
- Hardcoded coords in-file: stage-3 intro cam (508.4, -435.8, 6.0 …), chase-cam shots 1/2/4/5, Cornelius-cut cameras (239.4, -118.9 … / 238.9, -132.0 …), outro cameras (286.7, -74.0 … / 280.1, -75.3 …).
- Area codes seen: **0 only**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1/ch2 set | New primitives needed |
|---|---|---|
| W0/W5 baked cutscenes | `fade`, `letterbox`, `cutscene_skip_on_retry` (1_03/1_04) | none new |
| W1 world setup | `unique_ped_suppress` (2_S04), `population_override` (1_02B), `poi_enable` | **ambient_emitter_toggle** (enable/disable named world sound emitters, mission-scoped, restored on cleanup); **ambient_vehicle_override** (`VehicleOverrideAmbient` + revert — ped population override already exists, ambient traffic is a separate channel) |
| W2 tableau | `spawn_npc` flag bundles, `ped_action_node`, `crowd_ring` (1_B) | **npc_grapple_hold_scene** (A grapple-holds B in a looped shove anim as a standing tableau until interrupted — Johnny/Cornelius) |
| W2/W3 interrupts | `ped_hit_event` (who-hit-me-last variant, 2_05), `mission_critical_npc` | none new (KO-any-tableau-ped fail = `mission_critical_npc` group variant) |
| W3 zone ally | `recruit_ally` (2_S04), `health_bar(name)` (2_09) | **ally_zone_recruit_toggle** (recruit/dismiss ally + health-bar show/hide + attacker retarget, driven by player trigger enter/exit on a leash zone) |
| W5/W6 player on bike | `dismount_vehicle` (2_R03), `vehicle_camera_preset` (2_R03) partially | **player_rides_npc_vehicle** (player fixed to a seat of an NPC-driven vehicle: `PlayerFixToBackOfVehicle`, take-damage-while-attached toggle, camera rotation-locked relative to the vehicle, camera-change lock, forced passenger weapon + heal-to-full on mount; unbolt restores camera + plays a no-damage dismount anim). Distinct from `player_takes_npc_vehicle` (2_05), where the player drives |
| W7 chase spawner | `continuous_wave_spawner` (2_07), `npc_ride_vehicle_path` (2_05), `npc_follow_path` node events (1_03) | **mounted_chaser_spawner** (chase-leader path-node callbacks spawn mounted chaser pairs at indexed points; concurrency cap on live chasers; model round-robin; weapon alternation by spawn ordinal; chaser stat bundle: speed window relative to leader, health pin, damage-taken multiplier; monitor culls dismounted/out-of-range chasers to ambient-flee + one-time player bark) |
| W7 camera cuts | `camera`, `vehicle_camera_preset` | **chase_camera_cuts** (path-node-triggered cinematic shots *during live gameplay*: control off, player invulnerable, fixed or camera-path shot with audio focus on camera, optional player disarm for the shot, timed reset to the chase camera) |
| W7 bike jump / wipeout | node events (1_03) | **vehicle_jump_action** (`BikeJump` impulse at a path node — small action); **chaser_wipeout_event** (force all live chasers off their bikes at a node — fold-able into `mounted_chaser_spawner` as a scripted beat) |
| W6/W8 heal + disarm | `give_weapon`, `weapon_strip` (2_B) | **player_heal_full** (small action; the "strip for camera shot" arm/disarm reuses `weapon_strip`-adjacent forced-equip) |
| W8 escort | `recruit_ally`, `escort_follow` (2_G2), `mission_critical_npc` | none new |
| W9 outro | `speech_sequence`, `camera`, `letterbox`, `completion_banner`, `set_faction_respect` | none new |
| fails | `mission_critical_npc` (hit + KO variants) | none new — five keyed variants is data |
| pass | `give_weapon` | none new (`SMissPass` "grant if not owned" = `give_weapon` with a `player_has_weapon` guard condition, 2_07) |

## 6. Open questions

- `AreaOverridePopulation(3, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0)` slot meanings (per-class spawn counts; decoding belongs to a population-format note, not this mission).
- Weapon/item ids 301, 312, 306 (chase context) and 307 (reward) — id-level confirmation pending a weapon-table note; 312 also open from 2_07.
- Chase-cam shot 1's trigger: `bBikeCameraCut01` is never set in the live read path (only 2/3/4/5 and resets are) — dead beat or set from the DAT side, **OPEN**.
- The `pedExtraGoon01-03` / `F_routeChaseBreakOff*` family: spawned nowhere on the live path — cut "chasers break off" beat, or DAT-driven, **OPEN**.
- `M_2_R11` speech bank reuse inside a 3_04 camera shot (cross-mission bank, id 10).
- `PedSetFlag(ped, 20/108/113/117/125/129)` semantics — engine flag numbers, same status as other chapters' notes.
- Whether retry skips baked `3-04`/`3-04B` (no attempt check in-file).
- Exact route-node indices vs. the 22 `_3_04_BIKEATTACKERS` launch points, all trigger geometry, and the school-return trigger bounds (pending DAT extraction).
