# 2_05 — Tad's House: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_05.lua` (1251 lines, complete), `Library/LibTriggerNew.lua` + `Library/LibPed.lua` (the `L_*`/`F_MonitorTriggers` helpers it uses), `SGlFunc.lua` (`F_RainBeGone`), `SMissPass.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_05 = "Tad's House"** (canonical Chapter 2 list, 8-23) — **VERIFIED** by `LoadAnimationGroup("2_05TadsHouse")` and by content: Russell (75) and the player egg Tad's house (six window/shutter stations `_TADSHUD..05`, eggs = projectile 312), Tad (31) runs around closing shutters while prep patrols walk stealth routes, then a fight out of the yard. Matches the canonical entry exactly. No surprise.

Note: the DAT loaded is **`2_05_new.DAT`** (not `2_05.DAT`) — **VERIFIED** in `MissionSetup`. Setting is Tad's property in the rich area; area references are all outdoors (gate trigger `_AMB_RICH_AREA`); no area-code constants beyond that appear.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → `MissionInit()` (baked cutscene + spawn), then a `MissionStageRun` pointer loop (`Stage_01GoToHouse → Stage_02GetInYard → Stage_03EggHouse → Stage_04LeaveYard`) with a 3-state mission enum (RUNNING/PASS/FAIL), plus threads `T_RussellBikeMonitor`, `T_MakeRussellAlly`, `T_TriggerMonitor`, `T_StayInYard`, `T_MonitorEggCount`, `T_TadBehaviorMonitor`, `T_ManAimLoop`, `T_SpawnFinalGuys`.

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Baked cutscene "2-05"** | `PlayCutsceneWithLoad("2-05", true, true)` in `MissionInit`. Player placed at `_2_05_PLAYERSTART`. No attempt-count check — retry skip **OPEN**. |
| T1 | **Ride to Tad's with Russell** (`Stage_01GoToHouse`) | Russell created (health 800) with his bike (vehicle 274) at `_2_05_RUSSELLBIKE`, mission-critical; he rides ahead along `PATH._2_05_RUSSELBIKEPATH` via `PedFleeOnPathOnBike` with heavy stat overrides. Player is **given eggs**: weapon 312 + 24 ammo. Objective `2_05_29`, blip on `_2_05_GATECORONA`. Stage advances when the player enters `_AMB_RICH_AREA`. Russell supervision (`T_RussellBikeMonitor`): if he falls ≥ 80 m behind he is warped to `_2_05_RUSSELLWAITSPOT` and his bike is respawned at its point; if he is off his bike for any other reason, or the player takes his bike → fail `2_05_FAIL01`; at `TRIGGER._2_05_RUSSELLOFFBIKE` (or the ally trigger below) he dismounts and walks to the wait spot. `T_MakeRussellAlly`: player in `_2_05_RECRUITRUSSELL` or `_2_05_TADSYARD` → Russell becomes a recruited ally (follow + jump). Hitting Russell (`PedGetWhoHitMeLast == gPlayer`) fails the mission (`2_05_FAIL01`); if he was on the bike he plays a fall-off-bike node first. |
| T2 | **Get into the yard** (`Stage_02GetInYard`) | Trigger-monitor thread on (yard enter/exit + gate enter/exit callbacks), population overridden, POI system disabled, ambient vehicles overridden. Advance when the player is at `_2_05_FRONTGATE` or `_2_05_BACKGATE` **and** either Tad gate prop (`_TRICH_TADGATES`/`_TRICH_TADGATES01`, same props as 2_03) is open — i.e. the yard's gates open by proximity (world door behavior) and the stage just waits for the player to be through one. |
| T3 | **Egg six windows** (`Stage_03EggHouse`) | Russell's mission-critical flag dropped (but if he is somehow dead → fail `2_05_FAIL01`). Music `MS_FightingPrepsLow.rsm` (high-intensity variant armed). Instruction NIS (CS-2). Threads armed: stay-in-yard watchdog, egg-economy monitor, Tad behavior monitor, manual-aim tutorial (`MANAIMREMX` shows while the player is in the yard). Setup: initial patrol prep (34) spawned at `_2_05_PATROLA` walking `PATH._2_05_PATROLA` with **stealth behavior**; Tad created invulnerable/non-targetable at `_2_05_TADSTART`; the six room triggers (`_2_05_ROOM1..6`) registered with LibTriggerNew for **projectile-312 impacts** (`RegisterTriggerEventHandler(trigger, 2, ...)` — event type 2 = projectile impact, **INFERRED** from `L_TriggerOnImpactOld`); a HUD counter (`WindowEgg` icon) set 0/6; radar clamped to 30 m; blips on every un-egged shutter prop. |
| T3a | **Room egged** (`F_RoomEgged`) | First impact per room only: a splat/break 3D sound pair at the shutter (`Chrybmb_Exp` + `ToiletExp` — names cited verbatim, odd pairing **VERIFIED**), the room is queued for Tad, and its linked reinforcement patrol is queued (rooms 1, 2, 4, 5 queue patrols 1-4; rooms 3 and 6 queue none). |
| T3b | **Tad closes shutters** (`T_TadBehaviorMonitor`/`F_TadProcessRoom`) | Per queued room: Tad line (id 11), counter +1, that shutter's blip cleared; Tad is teleported to his slot at `_2_05_TADWINDOWLOC[i]` then runs `PATH._2_05_TOWINDOWi` to the window; on the last node he plays the close-shutters node (with a 1900 ms force-close fallback driving the shutter prop to `Close_StayClosed`) and the room is marked egged. When no rooms are queued: queued reinforcements spawn after a 1.5 s yell (line 12 on the first one) — peds 30/32/40/30 from `_2_05_FRONTDOOR`/`_2_05_BACKDOOR` onto paths `_2_05_PATROL1..4`, stealth behavior, 40% chance KO-drop of item 362. When both queues are empty and at least one window has been egged: Tad taunts (13) every 35 s. |
| T3c | **Stealth patrols** | Every patrol ped runs `PedSetStealthBehavior(1, F_BreakStealth, F_BreakStealth)` — a spotter behavior whose break callback plays a per-model spotted line (ids 18/19/21/600 by prep model; the initial patrol model 34 gets the generic `FIGHT_INITIATE` event instead). What happens after the bark is engine stealth→combat default, not scripted here — **INFERRED**. |
| T3d | **Egg economy** (`T_MonitorEggCount`) | Start: 24 eggs. At 0 eggs: spawn an egg-stash pickup (312) at `_2_05_LIFTSPOT` (`EggsBute`), blip it, switch the objective to the restock entry (`2_05_64`), decrement a reserve counter (starts at 2); pickup restores the egging objective (`2_05_09`) and shutter blips. Zero eggs **with reserves exhausted** → fail `2_05_02`. |
| T3e | **Stay in the yard** (`T_StayInYard`) | Leaving `_2_05_TADSYARD` before all six rooms are egged prints nag text (`2_05_61`, or `2_05_64` while a stash is out); if the player also leaves `_2_05_TADSBLOCK` (or strays into `_2_05_TREE01/02`) → 3 s grace → fail `2_05_62`. |
| T4 | **Fight out** (`Stage_04LeaveYard`) | At 6/6: counter off; prep-to-player attitude (types 5 vs 13) set hostile; Russell's attitude set and he moves to `_2_05_PATROL_B`; Tad's last line (22); objective `2_05_56` (leave the yard); a final three-prep wave (33/35/30) spawns at `_2_05_FRONTDOOR`, runs `PATH._2_05_FIGHTPATH`, and attacks once clear of the door trigger `_TADFRONTDOORR`; Tad is deleted; **all** surviving patrol peds are pulled off stealth and attack the player. |
| T5 | **Pass** | Player exits `_2_05_TADSYARD` → widescreen, `SetFactionRespect(5, 0)` — prep respect set to **exactly 0**, not a decrement (**VERIFIED**; harsher than 2_03/2_04's −10), pass banner `M_PASS` value 1500 (**INFERRED** money), respect message `MRESPECT_PM15`, pass music, `MissionSucceed(false, false, false)`. `F_RainBeGone()` (shared weather-clear helper, `SGlFunc.lua`) runs on pass — **INFERRED** the cutscene/mission could leave weather dirty. No chained start — **OPEN**. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Russell hit by player / dies | mission-critical callback + bike monitor | `2_05_FAIL01` |
| Player takes Russell's bike / Russell off bike outside script | `T_RussellBikeMonitor` | `2_05_FAIL01` |
| Out of eggs, stash reserves exhausted | `T_MonitorEggCount` | `2_05_02` |
| Abandoning the yard before 6/6 | `T_StayInYard` | `2_05_62` |

Retry: `MissionFail(false, true, msg)` — retry allowed, no fade; there is **no** mid-mission checkpoint (unlike 2_04), so a retry replays from T0/T1.

### Pass effects

**No 2_05 branch in `SMissPass.lua`** (**VERIFIED** by absence). In-script only: prep respect forced to 0, banner value 1500.

### Dead code in the file (not reached from `main`)

`F_BikeAttackCheck` + `F_CreateFrontAttack`/`F_CreateRearAttack`/`F_CreateGardenAttack`: two bike-mounted attackers (models 35/33 on bikes 273) spawned at `_2_03_BIKE01/02`, `_2_03_BIKEA01/02`, `_2_03_BIKEB01` when the player hits `_2_05_REAREXIT`/`_2_05_FRONTEXIT`/`_2_05_TREE01/02`/`_2_05_WALLBREAK01/02`. **VERIFIED** never called from any live path in this file or the libraries — a vestigial escape-chase beat. Note the **_2_03_-prefixed point names** would have to live in `2_05_new.DAT` (only that DAT is loaded): copy-paste naming from 2_03's data, **INFERRED**.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | T0 intro | baked (`2-05`) | `fade` in at `_2_05_PLAYERSTART` + `letterbox` + one `say` beat |
| CS-2 | T3 instruction NIS | in-engine, ~5 s + skippable | camera on `PATH._2_05_INTRO` looking at `_2_05_WINDOW1` (z − 3), speed (3,2,2), `letterbox`, objective `banner`; skippable — reuse the ch1 1_11x2 **skippable in-engine cutscene** gap |
| CS-3 | T3b shutter closes | in-engine anim beats | no camera: `prop_set_state` on the shutter + Tad `npc_follow_path` + anim node |
| CS-4 | T5 pass | engine UI | `completion_banner` + respect line |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 75 | Russell | health 800, bike escort, recruited ally, mission-critical until T3 |
| 31 | Tad | invulnerable, non-targetable; shutter-closing scheduler; deleted at T4 |
| 34 | initial patrol | `_2_05_PATROLA` path, stealth; generic spotted bark |
| 30, 32, 40, 30 | reinforcement patrols 1-4 | stealth paths `_2_05_PATROL1..4`; 40% drop 362 |
| 33, 35, 30 | final wave ×3 | front-door spawn, `_2_05_FIGHTPATH`, then attack |
| 35, 33 | (dead code) bike attackers ×2 | only in the uncalled bike-attack functions |
| 274 | Russell's bike | respawned if Russell is warped |
| 273 | bikes | loaded (`tblVehicleModels`); live use only in dead code |

### Animation groups / action trees

`LoadAnimationGroup`: `2_05TadsHouse` (contains the shutter-close and fall-off-bike nodes). `LoadActionTree`: `Act/Conv/2_05.act`. Shutter prop tree: `Act/Props/tadshud.act` (`Close_StayClosed` node).

### Props / pickups / weapons

- Six shutter props `_TADSHUD`..`_TADSHUD05` (blippable, anim-driven close).
- Tad gate props `_TRICH_TADGATES`/`_TRICH_TADGATES01` (shared with 2_03).
- Weapon/projectile **312 = eggs** (24 starting ammo; stash refills ×2).
- Pickup **312** at `_2_05_LIFTSPOT` (`EggsBute`).
- Item 362 KO drops (40% on patrols) — contents **OPEN**.

### Speech

`SoundPlayScriptedSpeechEvent("M_2_05", id, ...)` ids used: 11, 12, 13, 22 (Tad), 18, 19, 21, 600 (per-model spotted barks) — **8 distinct scripted lines**, plus generic `FIGHT_INITIATE` events. Music: `MS_FightingPrepsLow.rsm` + high-intensity `MS_FightingPreps.rsm`; pass/fail stingers.

### Named world data to extract from `2_05_new.dat`

- POINTLISTs: `_2_05_PLAYERSTART`, `_2_05_RUSSELL`, `_2_05_RUSSELLBIKE` (2), `_2_05_RUSSELLWAITSPOT`, `_2_05_TADSTART`, `_2_05_TADWINDOWLOC` (6), `_2_05_LIFTSPOT`, `_2_05_GATECORONA`, `_2_05_WINDOW1`, `_2_05_PATROLA`, `_2_05_FRONTDOOR`, `_2_05_BACKDOOR`, `_2_05_PATROL_B`; dead-code bike points `_2_03_BIKE01/02`, `_2_03_BIKEA01/02`, `_2_03_BIKEB01`.
- PATHs: `_2_05_RUSSELBIKEPATH`, `_2_05_PATROLA`, `_2_05_PATROL1..4`, `_2_05_TOWINDOW1..6`, `_2_05_INTRO`, `_2_05_FIGHTPATH`.
- TRIGGERs/props: `_2_05_ROOM1..6`, `_TADSHUD.._TADSHUD05`, `_2_05_TADSYARD`, `_2_05_TADSBLOCK`, `_2_05_FRONTGATE`, `_2_05_BACKGATE`, `_2_05_RECRUITRUSSELL`, `_2_05_RUSSELLOFFBIKE`, `_2_05_EGGSTASHAREA`, `_2_05_TREE01/02`, `_2_05_REAREXIT`, `_2_05_FRONTEXIT`, `_2_05_WALLBREAK01/02`, `_TADFRONTDOORR`, `_TRICH_TADGATES`, `_TRICH_TADGATES01`, `_AMB_RICH_AREA` (shared).
- Area codes seen: none numeric in this file (all references by name; outdoors rich area, **INFERRED** code 0).

## 5. Framework gaps (beat → primitives)

Reuses ch1 vocabulary: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `mission_critical_npc` fail, `ped_hit_event` (who-hit-me-last variant), `npc_follow_path` + `ped_reached_path_node(last)`, `recruit_ally` (ch1 2_S04), `collect_counter` (ch1 1_S01/2_S04), `pickup_spawner`, `player_has_weapon/ammo` conditions, `stealth_spotter`/`spotted_by` (ch1 1_S01/1_04), `npc_bark_timer`/`bark_loop`, `population_override`, `poi_enable`, `door_prop_control`, `dynamic_blip`, `radar_set_range` (ch1 1_04), `completion_banner`, `set_faction_respect`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| T1 Russell bike escort | `escort_follow` (partial) | **npc_ride_vehicle_path** (NPC drives a vehicle along a path); **vehicle_leash_respawn** (≥80 m behind → warp NPC + respawn vehicle); **player_takes_npc_vehicle** fail condition |
| T3 room egging | `projectile_impact_count` (ch1 1_04, per-zone) | **projectile_impact_in_trigger(trigger, projectile_id) → event** (per-room, once-only latch) — a generalization; here 6 armed triggers |
| T3b Tad scheduler | `npc_follow_path`, `prop_set_state` | **npc_task_queue**: FIFO of investigate tasks + reinforcement spawns + idle barks when empty, one consumer NPC. The real logic is a tiny work-queue, not combat AI |
| T3b shutter | `prop_set_state` | **anim_with_timeout_fallback** (play node; if no done-callback in 1.9 s, force prop state) — small, may fold into prop_set_state |
| T3c stealth patrols | `stealth_spotter` | nothing new — per-model spotted bark table is data |
| T3d egg economy | `pickup_spawner`, `player_has_weapon/ammo` | **ammo_out_refill** pattern: on weapon-ammo 0 → spawn stash pickup + swap objective + fail after N refills exhausted (bounded `pickup_spawner` with reserve counter) |
| T3e stay-in-yard | `zone_exit_countdown` (ch1 1_04) | nothing new (nag text + 3 s fail is exactly it) |
| T4 final wave | `wave_group` (ch1 1_03) | **npc_spawn_walk_then_attack** (spawn at door, walk path, attack once past a trigger) — small; may fold into spawn_npc flags |
| T5 pass | `set_faction_respect` | nothing new (but note the set-to-absolute-0 variant) |
| CS-2 | `camera`, `letterbox`, `banner` | ch1's skippable in-engine cutscene primitive |

Genuinely NEW names introduced here: **npc_ride_vehicle_path**, **vehicle_leash_respawn**, **player_takes_npc_vehicle**, **projectile_impact_in_trigger**, **npc_task_queue**, **ammo_out_refill**, **npc_spawn_walk_then_attack**. (The manual-aim tutorial `MANAIMREMX` is a `hint`/tutorial-popup call, no new primitive.)

## 6. Open questions

- What engine behavior follows a stealth break (the callback only barks; escalation to combat presumed engine-side, **INFERRED**).
- Exact shutter/window/patrol geometry pending `2_05_new.dat` extraction; whether the `_2_03_BIKE*` points exist there (dead code suggests the DAT may still carry them).
- Whether Tad's shutter close can be interrupted (Tad is invulnerable + non-targetable, so **INFERRED** no).
- The `ToiletExp` second impact sound at every shutter (name cited; purpose **OPEN** — likely a sloppy shared splat).
- `PedFleeOnPathOnBike` semantics (flee-mode flags on a scripted escort ride) and Russell's stat overrides (ids 24/26-29/35/37 undecoded).
- Retry cutscene skip (no attempt check in file) — **OPEN**.
