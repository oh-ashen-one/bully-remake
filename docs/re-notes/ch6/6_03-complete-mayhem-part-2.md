# 6_03 — Complete Mayhem, part 2 (retake the school): recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap6/6_03.lua` (1611 lines, complete) + handshake reads in `scripts/AreaScripts/{GymAndPool,Gdorm,Library,PrepHouse,SchoolHallways,MainMap}.lua`. No `SMissPass.lua` branch (**VERIFIED**). Structure in our own words; script/native/string-key names are citations only.

## 1. Identification

**6_03 = "Complete Mayhem" (part 2 of 2)** — the school-riot act: with Russell as an ally, Jimmy hunts the four clique leaders (free order) and then meets Edgar in the main building for the Gary reveal. One canonical mission (#65, 8AM–8PM) split across two scripts; see the 6_02 doc for part 1 and the seam evidence (shared `6_02.DAT` — there is **no** `6_03.DAT`; opening cutscene `"6-02B"`). Walkthrough cross-check: leaders at Gym (Nerds), Girls' dorm (Greasers), Library (Jocks), Harrington House (Preppies), then the main-building scene where prefects confiscate Jimmy's weapons and Gary appears on the roof — all **VERIFIED** in-file.

Boss/id mapping (**VERIFIED** from spawn code; names per roster): Nerd boss 215 (Earnest) + lackeys 8/11 on the gym garage roof (area 13); Greaser boss 217 (Johnny) + 21/22 at the girls' dorm (area 35); Jock boss 216 (Ted) + 12/18 in the library (area 9); Preppy boss 218 (Derby) + 32/35 at Harrington House (area 32). Edgar = 91, Gary = 130, Russell = 176, prefects 49/50, bully NIS extras 99/85/145.

Dead/loose ends (**VERIFIED** in-file): `F_UpdateCops` is dead twice over — never spawned as a thread, and its `while mission == "running"` guard reads a nonexistent global (`mission`, not `gMissionStage`), so it would never loop anyway; `gGaryTextTable` (five taunt keys) is unused; `CB_PLAYEREND`/`CB_RUSSELLEND`/`CB_PREFECTEND` counters are written but only `nPlayerEnd`'s sibling pattern in 6_02 is consumed — here nothing reads them; `F_NISDoorOpening` is a dead tree-query hook (`bGateOpen` global nil → always 1); `gPreviousRussellHealth` is snapshot-restored around NIS segments (**VERIFIED** live). `T_MonitorBarriers`' `barriersVisible = barriersVisible and false` is a no-op assignment (barricades never un-show — harmless, player can't re-enter after).

## 2. Mission flow

Driver: same function-pointer stage poller as 6_02 (`F_Stage3Setup` → `F_Stage3` → `F_Stage4Setup` → `F_Stage5Setup` → `F_Stage5`), plus two service threads (`T_LeaveingFail`, `T_MonitorBarriers`) and up to four boss monitor threads. No checkpoints.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup + baked cutscene "6-02B"** | Continues 6_02's DAT; player transitioned to `_INDUS_POINTS,12`; school gate PAnims created; ~18 named school/gym/pool/dorm doors locked en masse; punishment system disabled; riot music (`MS_StreetFightLargeLow_Boxing`, locked stream at 0.85); 12 unique models suppressed (incl. 48 = Gary and the four leaders' ambient variants — **INFERRED** to stop ambient duplicates); player's weapon inventory snapshotted (`PedSaveWeaponInventorySnapshot`); global rumble attitude on (`PedSetGlobalAttitude_Rumble(true)`); male-only ped generation; POI disabled; `shared.g6_03_{Nerds,Greasers,Preppies,Jocks}Alive = true` + `shared.g6_03_AreaReady = false` published for the area scripts. |
| B1 | **Gates NIS: the alliance** (`F_SideSchoolNIS`) | In-engine scene at the school side gate: Russell + two townies + Edgar spawn; if the main map is visible, the two school-gate geometry sets are swapped closed→open (`GeometryInstance` ×4) and the shared persistent gate entities from `shared.gSchoolGates` are deleted, with nav holes cut at both gates. Jimmy/Russell dialogue beats; Russell walks to the gate and plays a long headbutt-antic action node (the gate-ramming gag, ~6.5 s); Edgar + townies run off to their fight destinations; Russell is recruited as ally, made **untargetable**, and his health is multiplied ×7. Objective `6_03_OBJ00` + four leader blips. |
| B2 | **Riot overworld** (`F_UpdateGlobalHatred`) | Attitude matrix rewrite: cliques 4/2/5 hostile to player type, everyone hostile to everyone (13 rows); population override to a riot mix (`14,0,3,3,0,3,3,…`), all peds cleared — the school becomes a brawling free-for-all where regular students fight each other and mostly ignore the player. Parking gate held open every frame (`F_UpdateGates`). |
| B3 | **Four leader fights, free order** (`F_Stage3` + `T_Monitor*Boss`) | The area scripts own the handshake (**VERIFIED** they read `MissionActiveSpecific("6_03")` + the `shared.g6_03_*Alive` flags — `GymAndPool.lua:8`, `Gdorm.lua:4`, `Library.lua:4`, `PrepHouse.lua:15`; **INFERRED** they flip the flag false and set `g6_03_AreaReady` when the player enters the dressed interior). On a flag flip, `F_UpdateBosses` spawns that clique's monitor thread. Each monitor: waits for its area code, spawns boss + 2 armed lackeys (nerd lackeys carry 301/299, greasers 303 ×2, jocks 300 ×2, preppies 302 ×1), swaps music to the high-intensity layer, runs the faction NIS (`F_Execute*NIS`: leader/lackey cheer or taunt beats, 2–3 dialogue exchanges with the player, camera path + look-path pan, Russell frozen/unfrozen around it via `F_EnableRussell`), locks the area doors, then releases everyone to attack (boss + lackeys vs player, Russell attacks all three). All three dead → doors unlock, peds cleared, `F_LeaderKOd` prints a remaining-count hint if any leader is left, that objective completes, blips refresh (`F_RestoreAllBlips`: one blip left → `6_03_OBJLAST` "final leader" objective). Nerd/Jock NIS segments dress the area with scripted fires (`F_CreateFire` on five/four triggers; cleaned up on thread exit). |
| B4 | **Regroup at the school** (`F_Stage4Setup/5Setup`) | All four objectives complete → stage 4: residual NIS peds deleted, blip + objective `6_03_OBJ02C` at the school entrance (`_OBJECTIVELOCS,5`). |
| B5 | **End NIS: busted, saved, Gary** (`F_Stage5`) | Player at the entrance → fade, transition to area 2, full NIS: population zeroed, clock set to 22:00, Edgar + 2 prefects + 3 sleeping bullies staged; Edgar cocky speech + laugh; Jimmy/Russell walk-up; prefect 1 approaches and plays a bust vignette on the player (`PlayerBusted/BustThatBitch` grapple + weapon put-away node) — Jimmy's punishment points set to 3, Edgar/Russell zeroed; Russell intercedes (two lines), both prefects play scared and are walked off along paths; Russell departs; Gary spawns at an elevated point, taunts (his `GaryCrazy/GoOff` beat), then runs off along `PATH._GARYPATH` with the player auto-chasing on `_6_03_ENDNISJIMMYRUNS`; fades with camera-linked audio. `gMissionStage = "passed"`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Russell dies | `PedSetMissionCritical(gRussell, true, F_RussellDied)` → `bRussellDied` | `6_03_RUSSDIED` |
| Player leaves the school grounds | warn ring `_6_03_WARNFAIL01-03` (warning text + a decorative cop car + 2 cops spawn at the back/front entrance, stimulus-ignored, one invulnerable) then fail ring `_6_03_FAIL01-03` | `6_03_FAILLEAVE` |
| Any other failure | generic | `MissionFail(true, true)` |

Every fail restores the weapon inventory snapshot first (`PedRestoreWeaponInventorySnapshot`); fail stinger 10. Full replay, no checkpoints. Cleanup is exhaustive: all ~18 doors unlocked, patrol paths re-enabled, population reverted, unique models re-enabled, rumble attitude off, punishment re-enabled, fires and persistent entities removed, the four `shared.g6_03_*Blip` mirrors nilled, attitudes reset to default.

### Pass-side effects

- In-file: `MissionSucceed(false, false, false)` — no banner, no rewards; the chain into `6_B` (Final Showdown) is engine mission-table behavior (**OPEN**, same standing question as 6_02→6_03).
- `SMissPass.lua`: no branch (**VERIFIED**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-1 | B0 | baked (`6-02B`) | `fade` + `letterbox` + `say` |
| CS-2 | B1 gate alliance | in-engine, ~25 s | `inline_cutscene_chain` + `world_entity_swap` (gate geometry + persistent-entity purge + `nav_block_radius` cut) + `ped_action_node` headbutt gag |
| CS-3 | B3 leader NIS ×4 | in-engine, ~15–25 s each | `inline_cutscene_chain`: `camera_path_cycle`-style pan (path + look-path, cite 3_G3), `speech_sequence`, cheer `ped_action_node`s, door lock/unlock |
| CS-4 | B5 end NIS | in-engine, ~60 s | `inline_cutscene_chain` + `scripted_bust_vignette` (prefect grapple + `weapon_strip` via the PutAway node + `punishment_points_add`) + `npc_flee_path` prefect walk-offs + Gary reveal |

Anim groups: `NIS_6_03`, `NPC_Adult`, `NPC_AggroTaunt`, `Russell`, `IDLE_DOUT_D`, plus per-fight `4_04_FUNHOUSEFUN` (nerds), `CHEER_COOL1` (greasers), `CHEER_COOL3` (jocks). Action tree `Act/Conv/6_02.act` (shared with 6_02). Music: `MS_StreetFightLarge{Low,High}_Boxing.rsm` locked streams.

## 4. NPCs and assets needed

### Peds (model ids as scripted)

| Model id | Role | Notes |
|---|---|---|
| 176 | Russell (ally) | recruited/dismissed per NIS; untargetable; health ×7, snapshot-restored around segments (`F_EnableRussell`); health bar shown during fights; KO = keyed fail |
| 91 | Edgar | NIS actor ×2 (gate + end) |
| 46, 42 | townie backup | gate NIS, run off to fight |
| 215 / 217 / 216 / 218 | Earnest / Johnny / Ted / Derby | area-boss, one fight each, free order |
| 8, 11 / 21, 22 / 12, 18 / 32, 35 | lackeys ×2 per boss | weapons 301/299, 303, 300, 302 respectively; stat-forced (`GameSetPedStat` 362/100) |
| 50, 49 | prefects (end NIS) | bust vignette actors |
| 99, 85, 145 | bully extras (end NIS) | asleep-staged background |
| 130 | Gary | reveal + rooftop run-off |
| 83, 97 + car 295 | warn-ring cops + cruiser | decorative blockers, stimulus-ignored |

### Named world data to extract (6_03 reads **`6_02.DAT`** — flagged, not extracted; no 6_03.DAT exists)

- POINTLISTs: `_SIDESCHOOLPOINTS`, `_6_03_DODEST`, `_6_03_GATEXYZ`, `_GARAGEROOF`, `_GIRLSDORML`/`_GIRLSDORM`, `_LIBPEDS`, `_HARRINGTONHOUSE`, `_6_03_MOVETOPLAYER`, `_6_03_SCHOOLENTRANCE`, `_6_03_ENDNIS`, `_6_03_ENDNISBULLIES`, `_6_03_ENDNISMOVETO`, `_OBJECTIVELOCS`, `_6_02_COPSBACKENTRANCE`, `_6_02_COPSFRONTENTRANCE`.
- PATHs: `_RUSSELLTOGATE`, `_6_03_CAM_GYM(+LOOK)`, `_6_03_CAM_GDORM(+LOOK)`, `_6_03_CAM_HARR(+LOOK)`, `_6_03_CAM_LIB(+LOOK)`, `_6_03_ENDEDGARPATH`, `_6_03_ENDRUSSPATH`, `_GARYPATH`, `_6_03_ENDNISJIMMYRUNS`.
- TRIGGERs: `_TSCHOOL_FRONTGATE`, `_TSCHOOL_PARKINGGATE`, `_6_03_WARNFAIL01-03`, `_6_03_FAIL01-03`, `_GIRLSTRIG`, `_INDUSTRIALGATES`, `_6_03_NISNERD01-05`, `_6_03_NISJOCK01-04` (fire dressing), ~18 door ids (`DT_TSCHOOL_*`, `GYMPOOL_DOORL/R`, `DT_GYM_DOORL`, `GDORM_*`, `ESCDOORL`, `DT_LIBRARYEXITR`, `DT_PREPTOMAIN`…).
- Hardcoded coords in-file: gate geometry instances (2 opened + 2 closed), nav-cut radii, all NIS camera pairs, dumpster entity, barricade instances.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by existing set | New primitives needed |
|---|---|---|
| B0 door lockdown | `door_lock_set` | none new |
| B0 riot world-state | `population_override`, `global_rumble_attitude_toggle` (exists in pool), `faction_attitude_matrix_override`, `weapon_inventory_snapshot` | none |
| B1 gate swap | `world_entity_swap`, `geometry_instance_toggle`, `nav_block_radius` | none |
| B1/B3 Russell ally | `recruit_ally`, `npc_health_bar`, `ped_set_invulnerable`-family | **ally_health_snapshot** (save/restore an ally's current health around scripted segments that dismiss/re-recruit him, plus scripted max-health multiplier; keeps an escortee's damage persistent across NIS boundaries without making him targetable mid-scene) |
| B3 boss handshake | `area_script_shared_state`, `mission_active_query`, `area_mission_spawner` | **faction_leader_gauntlet** (N free-order area bosses: an area-script handshake arms the fight; boss + 2 armed lackeys spawn with a faction intro NIS; all three KO'd → doors unlock, area purged, that objective completes, remaining-leader hint + blip refresh, last-leader objective variant; each fight fully self-contained so order never matters) |
| B3 fires | `fire_prop_system` | none |
| B3 music layers | `interactive_music_stream` (locked variant = data flag) | none |
| warn/fail rings | `zone_blocker_npc`, `leave_area_fail`, `countdown_fail_timer` | none — warn ring = text + `zone_blocker_npc` cop dressing; fail ring = plain trigger fail |
| barricades | `geometry_instance_toggle` | none |
| B5 bust vignette | `scripted_bust_vignette`, `weapon_strip`, `punishment_points_add`, `npc_grapple_hold_scene` | none |
| end chase-out | `player_follow_path`, `npc_flee_path`, `camera_linked_audio_fade` | none |

## 6. Open questions

- The exact area-script writes that flip `shared.g6_03_*Alive` / `g6_03_AreaReady` (reads verified in the four area scripts; writes presumed there — confirm when those get their own notes).
- Population override tuple `(14,0,3,3,0,3,3,0,0,0,0,0,0)` — slot semantics (14 = dropout-heavy riot mix, **INFERRED**).
- Lackey weapon ids (301 spud gun? 299/300/302/303) — id-table confirmation pending.
- `PedOverrideStat(gRussell, 39, 80)` (stamina? speed?) at gate-NIS spawn.
- How 6_03→6_B chaining is declared (engine table).
- Whether the boss monitor's area-code waits (`AreaGetVisible() == 13/35/9/32`) can wedge if the player enters through a non-handshake door; the `AreaIsLoading()` OR-guard suggests known flakiness.
- All geometry pending `6_02.DAT` extraction (shared with 6_02).
