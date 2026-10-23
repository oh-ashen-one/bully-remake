# 3_S11 — Galloway Away: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/3_S11.lua` (1452 lines). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_S11 = Galloway Away** (catalog #46, Chapter 4). **VERIFIED** by content (infiltrate the Happy Volts asylum at night, free Mr. Galloway, Ms. Phillips waiting with the car) and externally: [Bully Wiki's speech-file categories tie 3-S11 audio to Galloway Away](https://bully.fandom.com/wiki/Category:Speech_files_by_missions/Galloway_Away) and [the wiki walkthrough](https://bully.fandom.com/wiki/Galloway_Away) matches the objective chain read here (observatory secret passage → meet Ms. Philips → get onto the grounds → destroy the statue's power source → sneak in). Same S-numbering caveat as 3_S03: the id is Ch3-era but the catalog slots it in Chapter 4.

Structure: a hand-rolled nested-loop state machine (`F_GetToAsylum` → `F_GetPastGate` → `F_GetOnAsylumGrounds` → `F_Overload` running the watcher-box beat in parallel with `F_GetIntoAsylum` → `F_GetToGalloway`). All loops bail on `not MissionActive()` or `bFailedDueToViolence`.

## 2. Mission flow

Driver: `MissionSetup()` (baked intro `"3-S11"`, `MS_RunningLow02`/`MS_RunningMid` streams) → `main()` → `F_MissionSetup()` → sequential stage functions.

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **Setup** | `3_S11.DAT`; action trees `Act/Conv/3_S11.act`, `Act/Anim/Crazy_Basic.act`; anim groups `Hang_Talking`, `LE_ORDERLY`, `F_CRAZY`, `NIS_3_S11`; ped models {53, 158, 125, 153, 154, 129, 150, 63}; car model 293; asylum front gate + front door locked; POI off; adults-friendly attitude (0, 6, 4). Player starts at `_3_S11_PLAYERSTART` (area 0). |
| A1 | **Secret passage breadcrumbs** | Objective `3_S11_SECRET` + blip at `_3_S11_SECRETPASSAGE`. Two waypoint hops (`_3_S11_SECRETPASSAGE` point 1 → `_3_S11_CRUMBS` point 6) lead to the under-bridge; reaching either the crumb end or the `_3_S11_PHILLIPS_CAR_STOP` trigger early swaps the objective to `3_S11_MEETP` and the blip to `_3_S11_UNDERTHEBRIDGE` (the crumb chain is skippable if you beeline, VERIFIED). A vestigial 6-crumb blip chain (`F_Crumbs`) is defined but never driven — dead code. |
| A2 | **Meet Ms. Phillips (NIS)** | At the car-stop trigger: fade; player bike (if any) parked aside or all vehicles cleared; the watcher box (`SC_ObservTrans`) and statue lights (`AS_statueLights`) persistent entities created; Ms. Phillips (63) warps into her car (293), drives `_3_S11_PHILLIPS_CAR_PATH` to the stop trigger, engine off, exits; a 4-line exchange (M_3_S11 ids 1-4, alternating camera shots, FOV 80→40); player returned to their bike; both deleted from the scene. |
| A3 | **The gate** | Theo (53, gate orderly, double HP) posted at `_3_S11_GATE_ORDERLY`. Approaching `_3_S11_ASYLUM_GATE_WARNING2` (or hitting Theo) makes him bark a warning (id 11); the outer `_3_S11_ASYLUM_GATE_WARNING` trigger just latches flags. Population zeroed for the rest of the mission. |
| A4 | **On the grounds + the crazy meet** | Grounds populate: 4 patients (150/125/153/154, `Crazy_Basic` tree, faction 6, three walking loops) and 4 orderlies (158/53/53/158) — two socializing at the door (mutual socialize pair), two patrolling (`_3_S11_STATUE_PATH`, `_3_S11_ORDERLY_PATH1`), all with `PedSetStealthBehavior(0, cbNull)`. A global hit-handler (`RegisterGlobalEventHandler(7, cbMissionCritical)`) watches for player violence. Entering `_3_S11_PLAYER_ON_A_GROUNDS` below z=5.5 plays the patient NIS: Theo deleted, front door locked, radar zoom set (30/75/45), patient 01 runs up, two barks (ids 21, 60), then resumes his loop; objective → `3_S11_94` (the watcher box), blip at `_3_S11_WATCHERBOX`, sneak tutorial toast (`3_S11_SNEAK`). |
| A5 | **Sabotage the watcher box** | While `bPlayerOnGrounds`, each frame checks all four orderlies' vision (`PedCanSeeObject`): spotted → `F_PlayerSpotted` (busted bark + camera beat) → **`F_EjectPlayerFromAsylum`: soft fail** — fade out, interior cast deleted, grounds reset (patients/orderlies re-seated via `F_ResetCraziesAndOrderlies`), player teleported to `_3_S11_KICKEDOUT`, objectives rolled back to `3_S11_74`, Theo re-posted. Not a mission fail. **Destroying the box** (`PAnimIsDestroyed`) → lights-out sequence: door unlocked, socializing stopped, orderlies walk toward the box, everyone faces the sparks; a randomized 250-1000 ms beat chain plays spark effects (`ElectricalSpark01` at `_3_S11_SPARK08/09/10`), pops (`LiteBulbPop`), explosion effects, then 7 fires (`FireCreate` on `_3_S11_FIRE01..07`), statue lights deleted, crazies cheer (`CB_Face` + cheering flag) then scatter to `_3_S11_P_GOTO1..4` / `_3_S11_CRAZYRUN`; orderlies re-pair (01/02 wander, 03/04 attack two patients — chaos cover, **INFERRED**), all four ignore the player. Objective → `3_S11_92` (get inside), blip at `_3_S11_FDOOR_BLIP`. |
| A6 | **Inside: the cell block** | Entering `_3_S11_INSIDE_ASYLUM` (after area 38 loads) plays `F_EnteringAsylumNIS`: player crouched at `_3_S11_PLAYERCROUCH` (ped flag 2 = stealth stance, **INFERRED**), front door opened to the player only, front-desk orderly (53) patrols `_ASYLUM_PATROL_FRONTDESK` — his path-node-1 callback ends the NIS. Then: Galloway (129) spawned at `_3_S11_GALLOWAY` (untargetable, invulnerable-to-player, blipped at his trigger), two interior orderlies (53/158) spawn; a patient-orderly **struggle vignette** (`3_S11_O_P_Struggle` idle pair) plays in a cell until the player crosses `_3_S11_BREAK_OUT` → the struggle "breaks", the patient is deleted, `CELLDOOR12` locks, orderly 01 switches to the `LE_Orderly_A` tree and both orderlies patrol (`_ASYLUM_PATROL_ABLOCK`, `_3_S11_I_ORDERLY1_PATH`). **Door-peek behavior**: patrol node callbacks (5/8/11/14) check whether cell doors (`CELLDOOR/16/15/14`) are open; if so the second orderly stops, faces the door point, waits 5 s, resumes — the player hiding in cells drives this, **INFERRED**. Stealth music tier (`MS_StealthLow/Mid/High`). Spotted by front-desk or patrol orderlies → same eject soft-fail (interior state reset; `bOrderlyBreak` path re-patrols the survivors). |
| A7 | **Reach Galloway → outro** | Entering `_3_S11_GALLOWAY_TRIGGER`: fade, `CELLDOOR13` prop deleted, baked cutscene `"3-S11C"` (the reunion), then `F_NewEnding`: front gate opened, Ms. Phillips' car (293) waiting at `_3_S11_CARLEAVING`; Galloway and the player walk `_3_S11_OUTRONIS_WALK`; cruel-laugh bark; a 3-line exchange (ids 54/55/56) across two camera shots; then `F_CarDrivesAway`: Phillips + Galloway drive off (`_3_S11_NIS_CARLEAVE`, cruise 6), pass music, `M_PASS` banner (6000 = $60), gate re-locked, `MissionSucceed`. |
| A8 | **Violence fail** | Hitting Theo or any orderly (`cbMissionCritical` via global event 7, or `F_CheckOrderlyHit`) sets `bFailedDueToViolence` → all loops unwind → `M_FAIL` banner with `3_S11_VOILENCE` (sic) and a true `MissionFail`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Violence vs Theo/orderlies | global event handler 7 + hit watches | `3_S11_VOILENCE` (banner) → unkeyed `MissionFail` |
| Spotted by any orderly | `PedCanSeeObject` polls | **soft fail** (eject + full reset, retry the infiltration in place) |

Player death is not handled in-file (default engine KO → presumably mission fail via the launcher). No stage checkpoints beyond the soft-fail reset.

### Rewards / pass side-effects

- $60 banner money (6000). `shared.bAsylumPatrols` toggled false during / true after (asylum returns to ambient patrols). No respect/unlock entries; no `SMissPass.lua` case (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked `"3-S11"` | `fade` + original card |
| CS-2 | A2 Phillips car meet | in-engine, 4 lines, 4 shots | scripted vehicle arrival (pool `scripted_vehicle_arrival`) + `camera` ×2 + `speech_sequence` |
| CS-3 | A4 crazy run-up | in-engine, 2 barks, 2 shots | `camera` + `say` ×2 + `npc_move_to` |
| CS-4 | A5 lights-out | in-engine effect/beats chain (~8 s) | `effect_spawn_at_points` + `fire_prop_system` + crowd scatter — mostly systems, one fixed camera is absent (plays live) |
| CS-5 | A6 entry NIS | in-engine (patrol walk-by) | `camera` + `npc_follow_path` node cb |
| CS-6 | A7 reunion | baked `"3-S11C"` + in-engine walk/car-leave | `fade` + `npc_follow_path` pair + `vehicle_follow_path` + `completion_banner` |

Anim groups: `NIS_3_S11`, `Hang_Talking`, `LE_ORDERLY`, `F_CRAZY`. Action trees: `Act/Conv/3_S11.act` (NIS + struggle pair), `Act/Anim/Crazy_Basic.act`, `Act/Anim/LE_Orderly_A.act` (busted node + patrol tree), `Act/Props/DormTV.act` not here (that's 3_R09). Dead/vestigial: `F_Crumbs` chain, `F_CheckOrderlyHit` (registered nowhere — the global handler covers it), `F_ENDIT` (debug jump into the ending), `VISIONRANGE_*` constants unused.

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 63 | Ms. Phillips | car meet + drive-away; ignore-stimuli, untargetable, invulnerable-to-player |
| 53 / 158 | orderlies | Theo (gate, 2× HP), 4 ground (2 socializers + 2 patrollers), front-desk + 2 interior; stealth behavior 0→1; spotted → eject |
| 150, 125, 153, 154 | asylum patients ×4 | `Crazy_Basic` tree, faction 6, loop paths; scatter/cheer during lights-out |
| 129 | Galloway | cell objective + outro; untargetable |
| 125 | interior struggle patient | deleted when the struggle breaks |

### Props / vehicles / effects

- `SC_ObservTrans` (watcher box — destructible persistent entity) + `AS_statueLights` (deleted when it blows).
- 7 `FireCreate` fires (`_3_S11_FIRE01..07`); effects `ElectricalSpark01`, `BottleRocketExplosion`; 2D sounds `ElectricSparks`, `LiteBulbPop`.
- Doors: `_ASYLUM_FRONT_GATE_DOOR`, `_DT_ASYLUM_FRONT_DOOR`, `_ASYDOORS12`, `_CELLDOOR12/13/14/15/16`, `_FMDOOR` — a full lock/pathable choreography.
- Car 293 ×2 uses (Phillips arrival, departure).

### Named world data to extract from `3_S11.DAT` (DAT present, VERIFIED)

- POINTLISTs: `_3_S11_PLAYERSTART`, `_3_S11_SECRETPASSAGE`, `_3_S11_CRUMBS` (6), `_3_S11_UNDERTHEBRIDGE`, `_3_S11_PHILLIPS`, `_3_S11_PHILLIPS_CAR`, `_3_S11_PLAYER_AT_ASYLUM(2)`, `_3_S11_GATE_ORDERLY`, `_3_S11_TREEBLIP`, `_3_S11_TREECAM`, `_3_S11_WATCHERBOX`, `_3_S11_FDOOR_BLIP`, `_3_S11_KICKEDOUT`, `_3_S11_CRAZY01/04/05/06`, `_3_S11_ODERLY01..04` (sic), `_3_S11_DOORGUARDTOWATCHER`, `_3_S11_SPARK08/09/10`, `_3_S11_P_GOTO1..4`, `_3_S11_O_GOTO3`, `_3_S11_PLAYERCROUCH`, `_3_S11_DOOR_GUARD1`, `_3_S11_GALLOWAY`, `_3_S11_I_ORDERLY1/2`, `_3_S11_I_PATIENT(2)`, `_3_S11_PATH2_DOOR1`, `_3_S11_OUTRONIS(+_WALK)`, `_3_S11_CARLEAVING`, `_3_S11_PLAYER_ORDERLY`.
- PATHs: `_3_S11_PHILLIPS_CAR_PATH`, `_3_S11_CRAZY3/4/5/6_PATH1`, `_3_S11_STATUE_PATH`, `_3_S11_ORDERLY_PATH1`, `_3_S11_CRAZYRUN`, `_ASYLUM_PATROL_FRONTDESK`, `_ASYLUM_PATROL_ABLOCK`, `_3_S11_I_ORDERLY1_PATH`, `_3_S11_CRAZY_ROOM_PATH`, `_3_S11_OUTRONIS_WALK`, `_3_S11_NIS_CARLEAVE`.
- TRIGGERs: `_3_S11_PHILLIPS_CAR_STOP`, `_3_S11_PHILLIPS_REUNION`, `_3_S11_ASYLUM_GATE_WARNING(2)`, `_3_S11_PLAYER_ON_A_GROUNDS`, `_3_S11_INSIDE_ASYLUM`, `_3_S11_BREAK_OUT`, `_3_S11_GALLOWAY_TRIGGER`, `_3_S11_FIRE01..07`, door ids above.
- Hardcoded cameras: car meet (-40.88, -298.31, 10.45 → …; -39.57, -309.29, 5.25 → …; -41.48, -306.29, 5.47 → …; -43.14, -309.66, 5.64 → …), crazy meet (-143.52, -376.54, 5.99 → …; -108.26, -340.05, 5.28 → …), entry NIS (-734.67, 422.54, 2.55 → …), outro (-67.12, -327.37, 6.83 → …; -58.75, -314.06, 5.57 → …; -64.06, -317.29, 5.87 → …; -60.57, -316.59, 5.41 → …), car leave (-66.05, -315.29, 5.94 → …). Areas: **0** (grounds) and **38** (asylum interior, VERIFIED by `AreaGetVisible() == 38`).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| A1 breadcrumbs | `reach_location` chain + `dynamic_blip` | **skippable_waypoint_chain** (beeline shortcut: reaching the end trigger early completes the whole chain) |
| A3-A6 stealth | `stealth_spotter` (pool) | **vision_cone_eject_loop** (N watchers × per-frame `PedCanSeeObject` → busted bark + camera beat → fade → teleport to eject point → world-state reset of patrols/cast/objectives → resume; a *soft* fail, no mission restart) |
| A4 patrols | `npc_patrol_cycle`, `guard_search_patrol` (pool) | **socializing_guard_pair** (`PedSetWantsToSocializeWithPed` pair, broken up on alert) + **patrol_door_peek** (path-node callback × door-open predicate → stop, face door point, 5 s, resume at next node) |
| A5 sabotage | `prop_health_bar`/`prop_destroy_threshold_event` (pool) | **sabotage_set_piece** (prop destroyed → choreographed chain: door unlocks, guards converge then get distracted, randomized 250-1000 ms spark/pop/explosion beats at points, N fires lit, crowd scatter + cheer, watchers gain ignore-player) — a scripted chaos cascade primitive |
| A6 struggle vignette | `pair_anim` | **looping_struggle_pair** (two-ped idle loop + break node on player trigger, then one deleted + door locked + other joins patrol) |
| A7 ending | `scripted_vehicle_arrival` (pool, reused as departure), `npc_follow_path` pair | **escort_walk_out** (player + freed NPC walk the same path side by side — `PedFollowPath` on both; fold into `npc_lead` variants) |
| fails | keyed fail text + banner | **violence_fail_watch** (global hit event handler → any player-hit guard sets fail; distinguished from the eject soft-fail) |
| stance | `hide` | **player_stance_flag** (ped flag 2 crouch set/cleared around the interior) — trivial, fold into spawn flags |

## 6. Open questions

- Global event 7 semantics (`RegisterGlobalEventHandler(7, cbMissionCritical)` — ped-hit class, **INFERRED** from `PedGetWhoHitMeLast` use).
- Ped flag 2 (stealth crouch stance) and the unused `VISIONRANGE_*` constants (an abandoned vision-range tuning pass).
- What `shared.bAsylumPatrols` drives in the ambient asylum area script (patrols on/off around this mission).
- The exact stealth model: orderly vision distance/FOV are native defaults here; whether crouching (flag 2) reduces detection radius is engine-side.
- Whether player death during infiltration routes to the eject reset or a full fail (not handled in-file — likely engine KO → mission fail; **OPEN**).
- `3_S11_VOILENCE` spelling is verbatim in the source (sic) — keep the key as-is for data fidelity.
