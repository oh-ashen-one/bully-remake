# 5_07a — Busting In, Part II: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_07a.lua` (1034 lines, complete) + cross-references in `AreaScripts/MainMap.lua`, `SFaction.lua`, `main.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_07a = "Busting In, Part II"** (canonical Chapter 5 list, #64 in `docs/story-mode-catalog.md`, chains from #63) — **VERIFIED**: the [Bully wiki Chapter V page](https://bully.fandom.com/wiki/Chapter_V) maps script id `5-07a` to "Busting In, Part II"; the [StrategyWiki ch5 walkthrough](https://strategywiki.org/wiki/Bully/Chapter_5:_The_Fall_and_Rise_of_Jimmy_Hopkins,_Aged_15) description matches the script beat-for-beat (starts automatically after Part I; Zoe guides Jimmy through the Blue Skies industrial park; two power breakers, a gate release, a train-junction switch pair; Zoe distracts the townie crowd at the Chem-O-Lot sign; Omar — Edgar's second-in-command — carries the plant keys; beat Omar, take the key, reach the chemical-plant door).

Downstream (**VERIFIED** in the clone):

- `AreaScripts/MainMap.lua:318`: when `shared.forceRun5_07` is set and 5_06 (Part I) is complete, the area script `ForceStartMission("5_07a")` — this is the automatic Part I → Part II chain.
- `AreaScripts/MainMap.lua:2417` (`F_SetupIndustrialArea`) + `:2444` (`F_ToggleIndustrialAreaBarricade`): before 5_07a the industrial-park entrance spawns persistent barricade `in_DO_Barricade01`; after 5_07a completes it spawns `in_DO_Barricade02` instead (the gate Russell broke — the opened state persists in free roam). During the mission 5_07a sets `shared.enclaveGateRespawn = 2` + `shared.resetEnclave = true` (value 2 = barricade-02 dressing while inside; cleanup restores 1 on failure).
- `AreaScripts/MainMap.lua:2924`: while 5_07a is active, the roundhouse/turntable prop handler sets `shared.trainButton = true` — this is the engine-side source of the two "train switch" objective beats in stage 2 (the mission script only polls the shared flag plus its own trigger volumes).
- `SFaction.lua:124`: while 5_07a is *available*, `PedSetUniqueModelStatus(75, -1)` suppresses unique ped model 75 (also done for 5_06 and 6_02 — the Edgar model, so the townie boss never wanders ambient during his story window).
- No in-file `MinigameSetCompletion` / `SetFactionRespect`: the pass block is a bare `MissionSucceed(true, false, false)`. No cash or respect reward for Part II itself (**VERIFIED** absence; consistent with StrategyWiki listing the townie-respect reward on the follow-up boss mission 5-B "Showdown at the Plant").

Setting: Blue Skies industrial park exteriors — area 0 only; the one `AreaTransitionPoint(0, …)` keeps the player in the main map (the plant interior is Part II's *destination*, not its playspace). Six persistent fires (`_BIKEFIRE01..06`) burn for the whole mission — the wreckage of Russell's entrance. Townies are made hostile to the player at the type level (`PedSetTypeToTypeAttitude(3, 13, 0)`, old value saved; the restore happens via the shared attitude system, not in-file — **OPEN**, see §6).

## 2. Mission flow

Driver: `MissionSetup()` (no baked cutscene — the intro is a hand-built NIS) → `main()` waits up to 4 s on the area-script barricade handshake, spawns Zoe and the persistent `DPE_RussBike` wreck, then runs a `StageFunction` state machine: `F_StageOneSetup → F_StageOne → F_StageTwoSetup → F_StageTwo → F_StageThreeSetup → F_StageThree → F_StageFourSetup → F_StageFour → F_StageFive → F_StageSixSetup → F_StageSix → F_StageSevenSetup → F_StageSeven → F_CompleteMission`. Interactive music: `MS_BikeFun_NIS.rsm` (intro) then locked `MS_BikeFunLow/Mid/High.rsm` for gameplay.

| # | Beat | Logic (functional) |
|---|---|---|
| I0 | **Intro NIS** (skippable, ~9 camera setups) | Control off, vehicles purged, widescreen. If the player arrived on a bike it's teleported to the start point. Russell (176, "burnt" spawn point) exchanges a look with Jimmy; a cop car (295 + cop 97 warped inside) follows `_5_07_COPSCHASERUSSELL` with siren on, accelerating in steps (`VehicleSetAccelerationMult` 0.3 → 2); Russell flees on foot along `_5_07_RUSSELLRUNCOPS`. Zoe then walks in and the two talk (paired action nodes from `Act/Conv/5_07a.act`, speech ids 97/96/15/1/2/3/5/6). Fire/smoke effects are pre-placed at `_5_07_BikeFire`/`_5_07_Smoke` for the scene, then re-created as six persistent `FireCreate` fires on the `_BIKEFIRE01..06` triggers for gameplay. Any-beat skip via button 7. |
| I1 | **Escort setup** | Zoe (48): untargetable, faction id 4, infinite sprint, attitude-to-player 3, health bar shown (label key `5_07A_36`), `PedSetMissionCritical(..., F_CriticalPedDied)` — her death is a mission fail. Townie crowd seeded: `F_CreateDropout` spawns 2+2+4 at `_5_07_DROPOUT_01/02/03` (random no-repeat-pair model picker over ids 41–46, hostile to player); every **second** spawned townie gets stat slot 0 = 362 / slot 1 = 100 (an armed-with-362 townie every other spawn, **INFERRED** weapon+ammo decode); all get stat 3 = 20. The first two groups stare at disabled POIs (`_5_07_POI_01/02`). Gate-switch and electric-shutoff props forced to their `NotUseable` node. |
| S1 | **Restore power (2 breakers)** | Objective `5_07A_OBJ1`, HUD counter 0/2 with the Switch icon, blips on `_TINDUST_POWER_SWITCH_01/02`; Zoe runs `_5_07_ZOEPATH1`. Each breaker detected by polling its prop anim (`PAnimIsPlaying(.../BRSwitch/Active)`), blip removed, prop set `NotUseable`. Both on → objective advances (`5_07A_OBJ4`), `_TINDUST_GATE_SWITCH` made usable, the Red Star security door unlocks, blip moves to the gate switch. One-shots: entering `_5_07_SLAUGHTERHOUSE` sends two townies down the stealth paths and Zoe comments (id 7); both breakers + player in `_5_07_FIRSTZOE` → Zoe takes `_5_07_ZOEPATH2` (lines 19, 23). Pulling the gate switch → `5_07A_OBJ5`, blip to `_TINDUST_BAR_DOOR_01`, stage 2. |
| S2 | **Office loop + train junction** | Arms when the player reaches `_5_07_OUTSIDETHEOFFICE`; Zoe runs path 3; the stage-1 townie groups are released to ambient. `_5_07_PEDATTACK02` locks slaughterhouse doors 03/04 **to the player but not to peds** (`AreaSetDoorLocked` + `AreaSetDoorLockedToPeds false`) and starts an `AreaAddMissionSpawner` over two spawn pointlists (the six townie models round-robin across the two doors; callback: each spawn attacks the player, every second one is armed as in I1). Reaching the locked door (`_5_07_DOORLOCKED01`) starts the train objective `5_07A_OBJ8`: blip on `_TINDUST_TRAIN_SWITCH_01`; when the area script's roundhouse handler has set `shared.trainButton` and the player stands in `_5_07_TRAINSWITCH`, the blip jumps to `_TINDUST_TRAIN_SWITCH_02`; the second flag + `_5_07_TRAINSWITCH02` completes the junction and re-blips Zoe. Player in `_5_07_PARTTHREE` → stage 3. |
| S3 | **Ranged ambush + bar door** | Two more townie pairs spawned (`_5_07_DROPOUT_07/08`). `_5_07_RANGED01/02` re-arm each pair with weapon 311 ×100 and raw-parameter `PedCoverSet` against the player. `_5_07_SECONDZOE` → Zoe line 12, objective `5_07A_OBJ10`, blip on `_TINDUST_BAR_DOOR_SWITCH_02`. When `_TINDUST_BAR_DOOR_01` reports open (`AreaIsDoorOpen` — the door is player-operated world geometry), the ranged pairs go ambient, five townies (`gDo10`) spawn at `_5_07_OMARCS`, Zoe takes her last path (`_5_07_ZOEPATH4`), and she comments again when she reaches the chem-entrance area object (id 13). Player in `_5_07_CHEMENTRANCE` → stage 4. |
| S4 | **Zoe's distraction** | Fade; player and Zoe teleported onto `_5_07_PLAYERDISTRACT`; fixed camera on the townie crowd; Zoe walks over, greets them (paired speech 116/4/5 + `GREET`/`GREET_HOT_GIRL` barks from two townies). After a second fade, Zoe and all five townies are fanned out on `_5_07_ZOEDISTRACT`, she plays her `ZoeDistractWait` loop tethered to the chem-entrance trigger, her health bar hides. Objective `5_07A_OBJ6`: reach `_5_07_OMARFIGHT`. **Aggro tripwire**: if the player lands a hit on any of the five distracted townies (`PedIsHit` by player within 1 s), the whole group turns hostile and Zoe drops out of her performance and goes ambient. |
| S4b | **Omar arrives** | `_5_07_SILOATTACK01`: the distraction crowd goes ambient and **Zoe is deleted from the mission** (mission-critical cleared first) — the escort is over; Omar (47) spawns at `_5_07_DROPOUT_10` element 3 with 300 health, hostile, plus two more townies. |
| S5 | **Omar fight** | Entering `_5_07_OMARFIGHT` tethers Omar and both lackeys to that trigger, letterboxes, runs a camera path (`_5_07_END_CAM`), auto-walks the player to `_5_07_PLAYERMOVETOEND`; Omar faces Jimmy, barks (id 38), then attacks (blip on Omar). Omar under 200 health → he calls the lackeys in (id 40; `decision_by_health_percent`-style one-shot). **Leash rule**: if the player leaves the fight trigger, Omar disengages and walks back to his post (`_5_07_DROPOUT_10` element 5); re-entering re-aggros him. |
| S6 | **Take the keys** | On Omar's death a permanent pickup 480 (the gate key) is created at his corpse with a blip; objective `5_07A_OBJ9`; waits on `PickupIsPickedUp`. |
| S7 | **Reach the plant door** | Blip + objective `5_07A_OBJ7` at the hardcoded roof-door point (49.926, −564.06, 32.6076); player within 1 m → `F_CompleteMission` (no fade-in after completion, `MissionSucceed(true, false, false)`). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Zoe dies (stages I1–S4) | `F_CriticalPedDied` via `PedSetMissionCritical` | `5_07A_35` (`MissionFail(true, true, …)` — fade + retry) |
| Silent edge case | Player enters `_5_07_BARRICADENIS` then leaves it while inside `_5_07_MISSIONINIT` | none — bare `MissionFail()` with `gZoeDied = true` set (cleanup teleports the player to `_5_07_A_CORONA`). Looks like a leftover "player skipped the intro barricade" guard; **OPEN** (§6) |
| Player KO / busted | engine default | — |

No checkpoints: any fail replays from I0. Cleanup additionally: restores the area-script barricade state (`enclaveGateRespawn = 1`), hides the HUD counter, strips any held key item 480 (`ItemSetCurrentNum(480, 0)`), deletes the persistent bike wreck, re-enables POIs. A `DEBUG()` dev-skip (button 0,0 → `F_CompleteMission`) is compiled in.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | I0 intro | in-engine skippable chain (~9 hardcoded cameras, paired anim nodes, vehicle + ped path chases) | `skippable_scene_chain` + `camera` + `ped_action_node` + `pursuit_car` |
| CS-2 | S4 distraction | in-engine, two fades + fixed cameras, crowd anim loop | `dance_distract_stealth` + `camera` |
| CS-3 | S5 Omar reveal | in-engine, camera path + auto-walk + letterbox | `boss_fight_camera` + `player_follow_path` |
| — | pass | none in-file (objective completes at the door; no outro scene) | `completion_banner` only if desired |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 48 | Zoe | mission-critical escort; health bar; 4 path legs; distract performance; deleted at S4b |
| 176 | Russell | intro NIS only (flees the cop car) |
| 97 | cop | intro NIS only, warped into car 295 |
| 41–46 | townies ("dropouts") | ambient crowds (POI-gawking + armed every 2nd), spawner reinforcements, ranged cover pairs, distraction crowd ×5, Omar's lackeys ×2 |
| 47 | Omar | 300 health boss; <200 calls lackeys; leash-and-return post behavior; drops key 480 |

### Animation groups / action trees / effects

`NIS_5_07` (intro), `F_Adult`, `ChLead_Idle` (S4), `Act/Conv/5_07a.act`; prop trees `Act/Props/BRSwitch.act` / `AsySwtch.act` (`Active`/`Inactive`/`NotUseable` nodes polled for breaker state). Effects: `BarrelFire`, `boilerfire2`, `SmokeStackBLK`, `SmokeStackLRG` at pointlists; six persistent fires on `_BIKEFIRE01..06`.

### Props / pickups / weapons

- Switches (all `_TINDUST_*`): `POWER_SWITCH_01/02`, `GATE_SWITCH`, `TRAIN_SWITCH_01/02`, `BAR_DOOR_SWITCH_02`, `ELECTRIC_SHUTOFF` (forced unusable, see §6). Doors: `TINDUST_REDSTAR_SECURITY_DOOR`, `SHDOOR_03/04`, `BAR_DOOR_01`.
- Key pickup 480 (Omar's corpse); `F_SetupPickups` table (health 502 ×5, ammo 301/312/316, weapon 300 ×3) is defined but **never called** (**VERIFIED** — likely moved to DAT dressing or cut; §6).
- Player-arrival bike teleported to the start point; `DPE_RussBike` persistent wreck at (146.556, −488.208, 2.421).
- Cop car model 295; townie weapons 362 (stat-granted) / 311 (cover pairs); weapon models pre-requested 301/312/316/300.

### Speech

`M_5_07A` scripted ids: 97, 96, 15, 1, 2, 3, 5, 6 (intro), 7, 19, 23 (S1), 14, 15, 17 (S2), 31 (S2 door), 12, 13 (S3), 116, 4, 5 (S4), 38, 40 (Omar) — ~22 scripted lines; ambient `GREET`, `GREET_HOT_GIRL` ×2, `SEE_SOMETHING_COOL`. Music: `MS_BikeFun_NIS.rsm`, locked interactive `MS_BikeFunLow/Mid/High.rsm`; fail stinger only (`SoundPlayMissionEndMusic(false, 10)`).

### Named world data to extract from `5_07.dat`

- POINTLISTs: `_5_07_PSTART` (3), `_5_07_ZSTART` (2), `_5_07_RUSSELLBURNT`, `_5_07_BikeFire` (2), `_5_07_Smoke` (5), `_5_07_NIS_COPCAR` (3), `_5_07_DROPOUT_01/02/03/07/08/10`, `_5_07_OMARCS` (5), `_5_07_ZOEDISTRACT` (6), `_5_07_PLAYERDISTRACT` (2), `_5_07_PLAYERMOVETOEND`, `_5_07_COVER07_02`, `_5_07_COVER08_01/03`, `_5_07_A_CORONA`, `_5_07_SPAWNER01/02`, plus the (uncalled) pickup pointlists `_5_07A_ZOE_TRIGGER`, `_5_07A_HEALTH_PICKUP_01..05`, `_5_07A_AMMO_PICKUP_01..03`, `_5_07A_WEAPON_PICKUP_01..03`.
- PATHs: `_5_07_COPSCHASERUSSELL`, `_5_07_RUSSELLRUNCOPS`, `_5_07_ZOEPATH1..4`, `_5_07_STEALTHPATH01/02`, `_5_07_END_CAM`.
- TRIGGERs: `_BIKEFIRE01..06`, `_TINDUST_*` switches/doors above, `_5_07_BARRICADENIS`, `_5_07_MISSIONINIT`, `_5_07_SLAUGHTERHOUSE`, `_5_07_FIRSTZOE`, `_5_07_ELECTRICDOOR`, `_5_07_OUTSIDETHEOFFICE`, `_5_07_PEDATTACK02/03`, `_5_07_PARTTHREE`, `_5_07_DOORLOCKED01/02`, `_5_07_TRAINSWITCH`, `_5_07_TRAINSWITCH02`, `_5_07_RANGED01/02`, `_5_07_SECONDZOE`, `_5_07_OMARFIGHT`, `_5_07_CHEMENTRANCE`, `_5_07_SILOATTACK01`.
- POIs: `_5_07_POI_01/02/06` (ped production disabled).
- Hardcoded: bike-wreck position; final door point (49.926, −564.06, 32.6076); ~9 intro camera setups (around (141, −483) and (145, −482)); distract camera (77.49, −541.10, 4.86 → 72.63, −544.70, 4.39); Zoe-distract walk target (65.1421, −549.671).
- Area codes seen: **0 only**.
- **Trigger data (DAT): YES** — `DATLoad("5_07.DAT", 2)` (note: the file is `5_07.DAT`, not `5_07a.DAT`). Original lives in `Trigger.img` per `trigger-format.md`; the research clone is Lua-only. Extraction pending; not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `camera`, `say`/`speech_sequence` equivalents, `skippable_scene_chain` (I0), `pursuit_car` + `vehicle_siren_engine_flags` + `vehicle_follow_path` (cop car), `effect_spawn_at_points` + `fire_prop_system` (bike fires), `mission_critical_npc` + `npc_health_bar` (Zoe), `npc_follow_path`/`escort_follow` (Zoe path legs — note the original drives them by player-trigger gates, not arrival events), `prop_used_poll` (breaker/gate/train switch state reads via prop anim nodes), `collect_counter` (0/2 switch counter), `door_lock_set`, `area_mission_spawner` (S2 ambush), `npc_stat_override` (every-2nd-townie arming), `area_script_shared_state` (`shared.trainButton`, `enclaveGateRespawn`, `resetEnclave`), `npc_cover_behavior` (S3 ranged pairs, raw params like 4_03), `dance_distract_stealth` (S4), `buddy_chain_retaliation` + `ped_hit_event` (distraction tripwire), `decision_by_health_percent` (Omar <200), `npc_tether_trigger` (fight ring), `drop_pickup_at_ped` + `area_gated_pickup` (S6 key), `mission_presentation_flags` (`MissionDontFadeIn*`), `inventory_item_consume_on_fail` (key strip in cleanup), `persistent_fade_entity` (`DPE_RussBike` / barricade swap), `poi_enable`, `interactive_music_stream`, `player_safe_mode`, `dismount_vehicle` (arrival-bike teleport), `ped_flag_set`, `unique_ped_suppress` (SFaction's model-75 rule), `world_state_swap` (the persistent barricade-01→02 exchange on pass).

| Beat | Covered by | New primitives needed |
|---|---|---|
| S5 Omar leash | `npc_tether_trigger` (shape only) | **boss_deaggro_reset**: while the player is outside the boss's fight trigger the boss breaks off combat and walks back to a guard post; re-entry re-aggros. A per-boss state loop, not a one-shot tether |
| S2 ambush doors | `door_lock_set` | **door_lock_peds_split**: door locked to the player *but unlocked to peds* (`AreaSetDoorLocked` + `AreaSetDoorLockedToPeds false`) so reinforcements can pour out of a room the player can't re-enter. A two-flag parameterization of `door_lock_set` would cover it |
| I1/S2 "every 2nd townie is armed" | `npc_stat_override` | none — spawn-callback logic; note the pattern for the spawner schema |

Genuinely NEW names introduced here: **boss_deaggro_reset**, **door_lock_peds_split**.

## 6. Open questions

- The silent barricade fail (`_5_07_BARRICADENIS` exit while in `_5_07_MISSIONINIT`, no fail text, Zoe-death cleanup path) — leftover guard from an earlier intro layout, or a live "don't backtrack through the intro" rule? DAT trigger placement will tell. **OPEN**.
- `_TINDUST_ELECTRIC_SHUTOFF` is forced `NotUseable` and its `F_PowerSwitchOn` branch (with objective `5_07A_OBJ3`→`OBJ4` plumbing and `gObjs[2]`) is never reachable — no stage ever polls it, and `gObjs[2]` is never added. A cut third breaker step (the "electric door" trigger `_5_07_ELECTRICDOOR` also only sets the never-read `gElectricDoor`). **OPEN** — cut content, DAT may still place the switch.
- `F_SetupPickups` (14 health/ammo/weapon pickups) is defined but never called — moved to DAT dressing, or dropped when Part II's difficulty was retuned? **OPEN**.
- Both breaker branches call `CounterSetCurrent(1)` — the HUD counter never shows 2/2. Cosmetic bug in the original, or intentional? **OPEN**.
- Townie→player attitude is set hostile in `MissionSetup` (`PedSetTypeToTypeAttitude(3, 13, 0)`) with the old value saved to `oldAttitude`, but nothing in-file restores it — the restore presumably rides the shared attitude recompute (`SFaction.lua`) on mission end. Confirm on the engine side. **OPEN**.
- Zoe's faction is set to id 4 while the townies she's distracting read as faction 3 (per the type-attitude setup) — whether 4 is a "Zoe/ally" slot or a copy-paste. **OPEN** pending the faction-table decode.
- Zoe is silently deleted when the silo-attack trigger fires (S4b) — no walk-off; if the player never enters `_5_07_SILOATTACK01` she stays in her distract loop forever. Engine-side despawn presentation TBD. **OPEN**.
