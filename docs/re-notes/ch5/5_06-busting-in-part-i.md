# 5_06 — Busting In, Part I: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_06.lua` (464 lines, complete). `SMissPass.lua` **has** a `5_06` branch: `shared.enclaveGateRespawn = 2` on pass (**VERIFIED**, SMissPass read). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_06 = "Busting In, Part I"** (canonical Chapter 5 list, mission #63, 8AM–11PM) — **VERIFIED** by content: the player fetches Russell (model 75) from his house in the Vale, follows him on bikes across the map, and Russell rams the chemical-plant barricade gate (`BarrGate`) which explodes open; the script then latches `shared.forceRun5_07 = true`, chaining directly into `5_07a` (Part II — the plant showdown; the [walkthrough](https://www.bazicenter.com/features/ign_guides/bully_ignpdf.pdf) titles it "Showdown At The Plant"). Note script id ≠ catalog row (#63). The conv/action trees borrowed from `5_07a` (`Act/Conv/5_07a.act`, `NIS_5_05`-style garage NIS anims under `/Global/5_07a/...`) confirm the two halves were authored together (**VERIFIED**).

Dead/loose ends in the file: **`F_StageThree` + `F_StageFourSetup` are a complete, older ending path that is unreachable** — `StageFunction` is never assigned `F_StageThree` (the live flow's `F_StageTwo` advances straight to `F_StageThreeSetup`, which does the whole ending inline). The dead path contains a cop-and-worker arrival beat (models 97 + 123, bike 275, camera path `_5_06_BARRICADECAM`), skippable text beats, and **literal placeholder `TextPrintString` dialogue** about stealing the cop's bike to ram the barricade — dev leftovers, not shipped content (**VERIFIED** dead, **INFERRED** pre-final draft of the ending). Also: `gDebug = false` debug entry into stage 3; `Russell, idBike` declared as locals shadowed by globals; `F_DeleteUnusedVehicles` duplicated from 5_05.

Cross-file handshakes: `shared.forceRun5_07 = true` (consumer: campaign/`main.lua` mission dispatch, **OPEN** exact mechanism — schema-side this is our `start_mission` chain); `shared.enclaveGateRespawn` — set to 3 in `F_StageThreeSetup`, spin-waited until an external consumer clears it (5 s timeout fallback), and set to 2 by the `SMissPass` pass branch — a three-state gate-respawn handshake with the industrial-area script (**OPEN** producer/consumer; same `area_script_shared_state` family as ch3/4).

## 2. Mission flow

Driver: `main()` runs a `StageFunction` pointer loop; the follow-leash and got-away fail checks live in the `main()` loop itself, not the stages. No checkpoints — fail replays from the top; baked `5-06` intro replay on retry is not skipped in-file (**OPEN**, engine-side per ch1 pattern).

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup** | Baked cutscene `5-06`; `MissionDontFadeIn`; `DATLoad("5_06.DAT", 2)`; `Act/Conv/5_07a.act` loaded; player transitioned to `_5_06_PLAYERSTART` (area 0); bike 273 + Russell 75 requested; interactive music `MS_RunningLow02`/`MS_RunningMid`. |
| S1 | **Get Russell** | Pete (model 134) spawns beside the player, is made ambient, and wanders off (a walk-off cameo, **INFERRED** story continuity); blip + objective to Russell's house. Player stepping into `_5_06_NISRECRUITRUSSELL` (2 m radius) → recruit NIS: control off, fade, `5-06_Garage_NIS.rsm` preloaded, Harley sound bank (`OBJECTS_3\HrlyLeev.bnk`) loaded, player dismounted, vehicles within 4 m purged, Russell spawned invulnerable/stimuli-ignored, garage geometry `RI_GarDoorClose` instanced on, gate prop `_5_06_RUSSELLGATE` created, two bikes spawned (player 276, Russell's motorcycle 275). Beats: player knock anim + one line; Russell mounts up; the gate plays `RSGrDoor/GarageOpens`; FOV 80→30→80 camera cuts; Russell's reply (speech id 127). Blip moves to the player's bike. |
| S2 | **Follow Russell (mounted leash)** | Russell waits until the player is on any vehicle (or a 10 s grace expires), force-mounted if needed (4 s `PedEnterVehicle` retry → `PedPutOnBike`), then flees-by-bike along `_5_06_MAINBIKEPATH` with a 6-stat override bundle (bike handling/speed slots 9, 24, 25, 26, 27, 29 — meanings **OPEN**), `HarleyLeaves` 2D sting, locked `MS_BikeFastHigh.rsm`, objective + character blip on Russell. Main-loop enforcements: **warn at > 50 m** (`5_06_08`), **fail at > 75 m** or if the player leaves area 0 (`gGotAway` → `5_06_09`); fail too if Russell dies, dismounts, or goes invalid. Entering `_5_06_BRIDGE` spawns the plant barricade: `BarrGate` persistent entity + geometry at (150.2, −483.2, 2.1) playing `GateStanding`. Russell reaching `_5_06_BIKETRIGGER` stops him; when the player also arrives (or 3 s elapse) → fade, player teleported to `_5_06_RUSSELLNIS` if more than 10 m away, Harley bank unloaded, stage 3. |
| S3 | **The ram (ending NIS)** | `F_StageThreeSetup` is the entire ending, inline: `5-06_HarleyLeavesExplode_NIS.rsm` preloaded; ambient vehicles suppressed and all vehicles cleared (twice); `shared.enclaveGateRespawn = 3` then a spin-wait for the area script to clear it (5 s timeout); camera beats on the gate (FOV 90/80); player and Russell one-liners (ids 18, 12); Russell rides `_5_06_RUSSELLBIKE` with a crash callback at node 3; the script waits for the crash node **or** `_5_06_BARRICADECRASH` trigger **or** an 8 s timeout (triple fallback); then **five `BigExplosion` effects + one `CarDestroyed`** at `_5_06_EXPLOSIONS` points 1–6, and `BarrGate` plays `GateExplodes`. Fade, control back, ambient vehicles restored, gate entity deleted, **`shared.forceRun5_07 = true`**, `MissionDontFadeInAfterCompetion`, `MissionSucceed(true, false, false)` — no banner, no respect: the mission hands off straight into Part II. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player falls > 75 m behind Russell during S2 | `main()` loop distance check (`DistanceBetweenCoords2d`) | `5_06_09` (warn at > 50 m via `5_06_08`) |
| Player leaves area 0 during S2 | `AreaGetVisible()` check in `main()` | `5_06_09` |
| Russell dead / dismounted / invalid | S2 validity check | `5_06_09` (via `gGotAway`) |

Fail presentation: fail stinger 10, Russell + his bike deleted, `MissionFail(false, true, "5_06_09")`. Full replay from the top.

### Pass-side effects

- In-file: `shared.forceRun5_07 = true` (chain into Part II), `shared.enclaveGateRespawn = 3` (gate-open world state, then managed externally), `MissionDontFadeInAfterCompetion` (seamless handoff), no banner/respect/money.
- `SMissPass.lua` branch for `5_06` (**VERIFIED**): `shared.enclaveGateRespawn = 2`. The in-file set to 3 vs. the pass-hook set to 2 implies a state machine in the industrial area script (0/2/3 — exact semantics **OPEN**).
- `SFaction.lua`: Russell's unique model (75) is suppressed while `5_06` (or `5_07a`, or `6_02`) is available — `unique_ped_suppress` pattern, keeps him out of free-roam during the chain (**VERIFIED** in SFaction).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | S0 intro | baked (`5-06`) | `fade` in at `_5_06_PLAYERSTART` + `letterbox` + one `say` beat |
| CS-2 | S1 recruit Russell | in-engine, ~12 s | `inline_cutscene_chain`: `fade` + `camera` FOV cuts ×3 + 2 `say` beats + gate `prop_set_state` + NPC `PedPutOnBike` |
| CS-3 | S2 bridge barricade spawn | live gameplay, no camera | `world_state_swap`-adjacent: persistent entity + geometry + standing prop state on `trigger_enter` |
| CS-4 | S3 the ram | in-engine, ~10 s | `camera` beats + `say` ×2 + `npc_ride_vehicle_path` to crash node + `effect_spawn_at_points` (explosions ×6) + `prop_set_state` (`GateExplodes`) |
| CS-5 | S3 handoff | none (no fade-in after completion) | `mission_presentation_flags` + `start_mission` chain (schema exists) |

Music: `MS_RunningLow02`/`MS_RunningMid` (setup/S1), `MS_BikeFastHigh.rsm` locked (S2), preloaded one-shot NIS streams for the garage and the explosion; fail stinger 10, no pass stinger (hands off to 5_07a).

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 75 | Russell | unique-model suppressed while the mission is available (SFaction); invulnerable during NIS; crash-proof by script fallbacks |
| 134 | Pete | spawn-and-release cameo at mission start |
| 97, 123 | cop + worker | **dead path only** (`F_StageThree`) |

### Props / world systems

- Vehicles: 273 (setup request), 275 (Russell's motorcycle — also the dead path's cop bike), 276 (player bike). `VehicleOverrideAmbient(0,0,0,0)` + `AreaClearAllVehicles` during the finale; `F_DeleteUnusedVehicles` purge helper.
- Geometry/props: `RI_GarDoorClose` geometry instance (garage), `_5_06_RUSSELLGATE` PAnim (`RSGrDoor.act` `GarageOpens`), `BarrGate` persistent entity + geometry at the plant gate (`GateStanding` → `GateExplodes`).
- Effects: `BigExplosion` ×5 + `CarDestroyed` ×1 at `_5_06_EXPLOSIONS` points.
- Audio: `OBJECTS_3\HrlyLeev.bnk` bank load/unload; `HarleyLeaves` 2D; two preloaded NIS streams.
- Speech: `M_5_06` ids 12, 16, 18, 127 only. Tiny category.

### Named world data to extract from `5_06.DAT` (present — `DATLoad("5_06.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_5_06_PLAYERSTART`, `_5_06_RUSSELL`, `_5_06_NISRECRUITRUSSELL`, `_5_06_PLAYERBIKE`, `_5_06_RUSELLBIKESTART` (sic), `_5_06_RUSSELLNIS`, `_5_06_EXPLOSIONS` (6), `_5_06_COPEVENT` (dead path), `_5_06_RUSSELLWALKTO` (cleanup eject).
- PATHs: `_5_06_MAINBIKEPATH`, `_5_06_RUSSELLBIKE`, `_5_06_BARRICADECAM` + `_5_06_BIKEPATH` + `_5_06_WORKERPATH` (dead path).
- TRIGGERs: `_5_06_BRIDGE`, `_5_06_BIKETRIGGER`, `_5_06_BARRICADECRASH`, `_5_06_BARRICADEDOOR` (dead path), `_5_06_RUSSELLGARAGE` (cleanup).
- Hardcoded coords in-file: `BarrGate` (150.232, −483.16, 2.05389, heading 125), `RI_GarDoorClose` (480.175, 351.333, 19.6935), all S1/S3 cameras, the dead path's cop walk target.
- Area codes seen: **0** only (leaving area 0 mid-chase = fail).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1–ch4 set | New primitives needed |
|---|---|---|
| S0/S1 setup + recruit NIS | `fade`, `letterbox`, `camera` (FOV cuts), `say`, `prop_set_state` (gate), `world_state_swap` (garage geometry), `vehicle_area_purge` (2_07), `dismount_vehicle` | none new |
| S1 Pete cameo | `spawn_npc` + `npc_despawn`-adjacent (made ambient + wander) | none new |
| S2 follow-the-leader | `npc_flee_path_mounted` (2_02), two-tier `npc_distance_leash` (2_02: warn 50 m / fail 75 m, instantaneous), `vehicle_tail` family (2_S02), `npc_stat_override` (2_09), `blip_on_npc` | none new (the leader's wait-for-player-to-mount gate = `player_mounted` condition (2_04) with a 10 s timeout; note as data) |
| S2 barricade pre-spawn | `geometry_prop_instance` / `world_state_swap` on `trigger_enter` | none new |
| S3 crash NIS | `npc_ride_vehicle_path` / `npc_follow_path` node events (1_03), `anim_with_timeout_fallback`, `effect_spawn_at_points` (2_S06), `prop_set_state`, `camera`, `say` | none new (the node-trigger-timeout triple fallback = `anim_with_timeout_fallback` extended with a trigger-or-node race; data) |
| S3 gate handshake | `area_script_shared_state` (3_05): `shared.enclaveGateRespawn` write + spin-wait with timeout | none new |
| S3 chain to Part II | schema `start_mission` action + `mission_presentation_flags` (`MissionDontFadeInAfterCompetion`) | none new (`shared.forceRun5_07` = the original's flag-driven equivalent; our chain is cleaner) |
| fails | `leave_area_fail` (1_02B), `npc_distance_leash` | none new |
| pass hook | `area_script_shared_state` (`SMissPass` writes `enclaveGateRespawn = 2`) | none new |

**No new primitives from 5_06** — the whole mission composes from the ch1–ch4 chase/NIS/effect set. (The dead `F_StageThree`/`F_StageFourSetup` path would add nothing either: cop arrival = `scripted_vehicle_arrival`, skippable text beats = `say`.)

## 6. Open questions

- `shared.enclaveGateRespawn` state machine: 3 (in-file, during the ram) vs 2 (SMissPass, after pass) vs cleared-by-area-script (the spin-wait) — semantics live in the industrial area script; confirm during area-script survey.
- Consumer and exact semantics of `shared.forceRun5_07` (mission-dispatch chain into `5_07a` — presumably `main.lua`; our `start_mission` replaces it).
- Russell's 6-stat bike bundle (slots 9, 24, 25, 26, 27, 29) — belongs to the stat-table note; 25 at 125 is the only > 100 value.
- Why `Act/Conv/5_07a.act` is the NIS tree for a `5_06` mission (shared authoring of the two-parter — cosmetic, but confirms the pair).
- Whether retry skips baked `5-06` (no attempt check in-file).
- Whether any reward is attached at all (nothing in-file; fan wikis credit the two-parter's rewards to Part II — **OPEN** but consistent).
- All trigger/point geometry (pending DAT extraction); the `_5_06_MAINBIKEPATH` route defines the chase's map span.
