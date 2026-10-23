# 4_02 — Stronghold Assault: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_02.lua` (1409 lines, complete) + cross-references in `SMissPass.lua`, `SFaction.lua`, `AreaScripts/MainMap.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_02 = "Stronghold Assault"** (canonical Chapter 4 list, #45 in `docs/story-mode-catalog.md`, 8AM–9PM) — **VERIFIED**: the [Bully wiki Chapter IV page](https://bully.fandom.com/wiki/Chapter_IV) lists "Stronghold Assault 4-02" by script id; the [Stronghold Assault wiki entry](https://bully.fandom.com/wiki/Stronghold_Assault) matches the script beat-for-beat (starts at the library, given by Algernon; squeeze the nerds for the stronghold location, beat the gate code out of Thad, fight through waves to the observatory, destroy the transformer, turn the nerds' own spud cannon on the observatory doors; respect: nerds −100, jocks +5; unlocked by Fighting Johnny Vincent `3_B`). Pass block matches: respect messages `MRESPECT_NM100`/`MRESPECT_JP5` with `SetFactionRespect(1, 0)` (nerds, absolute 0 — equivalent to −100 from the pre-mission 100, **INFERRED**) and `SetFactionRespect(2, 50)` (jocks, absolute 50 — wiki's "+5" lands on 45 → 50, **INFERRED**; the message keys, not the absolute values, carry the delta semantics). Clique decode: faction 1 = nerds, 2 = jocks, 13 = player.

Setting: library front, the nerd stronghold path (barricaded back route, `_NERDPATH_BRDOOR`), and the observatory/football-field side — area 0 plus interiors (`F_EnteringGauntlet` treats `AreaGetVisible() ~= 0` as "player already inside", auto-marking the code as given). Downstream (**VERIFIED** in the clone):

- `SMissPass.lua:177`: on 4_02 pass, if the player holds no weapon 305 (spud gun), it is granted — the mission's equipment unlock (weapon-id decode per the "unlocking spud gun" comment in the source).
- `AreaScripts/MainMap.lua`: once 4_02 is complete, `world_brocket.DAT` and `world_spudgun.DAT` are loaded with the main map (world pickup spawns); while it is **not** complete, the `_SCHOOL_TURRETTRIPOD`/`_SCHOOL_TURRET` props are deleted — the field cannon exists in the open world only after this mission.
- `SFaction.lua`: nerd model 4 is suppressed while 4_02 is available and restored on completion; jocks' default attitude to the player becomes 2 after 4_02 (until 4_01's later pass sets 1).
- No `ChapterSet` — chapter stays 4 (the 4→5 advance happens at `5_01`).

## 2. Mission flow

Driver: `MissionSetup()` (baked intro "4-02", DAT, place player) → `main()` runs a strict sequence of blocking stage functions, each a `Wait(0)` poll on approach flags: `F_GetInfo` → `F_GetKeyCode` → `F_Wave01..04` → `F_GetOnSpudCannon` → `F_DestroyDoors` → `F_GetInObservatory`. Threads: `T_PlayerAsks` (Fatty confrontation), `T_FattyScared` (Fatty submission watch), later `T_Spawn_Window` + `T_Door_Health` + `T_Breaker_Box` + `T_Delete_Wave3_2`. Every stage loop exits early if `MissionActive()` goes false (player KO is the only generic fail — **no scripted fail keys exist in this mission**).

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup** | POI disabled; nerd models 4–11 loaded (Fatty 5, Melvin 6, Thad 7, Cornelius 9, Earnest 10, plus 4/8/11), weapons 301/303/307/309 loaded, `F_NERDS` + `IDLE_NERD_C` anim groups, `Act/Conv/4_02.act`. Unique models 5/6/7/9 suppressed (counts snapshotted for restore). Three barricade cover props (`_4_02_BAR_01/02/03`) pre-created via `PAnimRequest`/`PAnimCreate`. Nerd↔player attitudes set hostile-leaning (`PedSetPedToTypeAttitude(...,13,2)` on the greeter pair). |
| S1 | **Shake down the nerds** (`F_GetInfo`) | Fatty + Cornelius spawn at the library with social overrides (`4_02_Taunt.act`/`4_02_Greeting.act`, stimulus→response overrides 55/10/9, gift/blacklist wiring, greet+taunt callbacks registered). Objective `4_02_NERDS` + Fatty blip. The player greets (line 1 or 6) or hits either nerd: `T_PlayerAsks` flips Fatty hostile (stat overrides 0→362, 1→100, attack level 3) while Cornelius flees with flee-stat overrides. **Submission**: `T_FattyScared` watches Fatty — health ≤ 25, grappled (`Hold_Idle/RCV`), or **stuffed in a garbage can** (`Garbagecan/.../StuffGrap/RCV` then `InCan/die` anim states) → control off, letterbox, cower node, scripted line 12, objective `4_02_SECDOOR` (where the door is); Fatty goes ambient and flees (unless still canned). Hitting Fatty with weapon 301/308 short-circuits the ask loop (break without the talk). |
| S2 | **Gatekeeper Thad / the code** (`F_GetKeyCode`) | Thad (7) was spawned at `_4_02_GATEKEEPER` at setup with his health frozen as a min-health floor and the same social override set. Two paths converge: deal with Fatty first (S1) then approach Thad, or skip straight to the gate (`_4_02_GATEKEEPER` trigger or hit Thad — the `bShortCut` branch). Either way Thad refuses (line 13), his min health drops to 0, and the player beats him down: at health ≤ 45 (`F_ThadSubmitts`) he submits (`F_ThadCode`): cower or break node depending on grapple state, line 17 = the door code, objective `4_02_KEYCODE` completes, `4_02_OPENDOOR` + door blip (`_4_02_SECDOORBLIP`), Thad flees scared toward the start point. Taunting Thad instead makes him attack (he never submits verbally — **INFERRED**: the taunt path is a dead end by design). |
| S3 | **Open the stronghold door** | **OPEN / external**: the in-file `F_OpenDoors` (unlock `_NERDPATH_BRDOOR`, objective `4_02_GO_OBS` + observatory blip) is never called from 4_02.lua — the keypad interaction presumably fires from the door trigger's own wiring (cf. `MainMap.lua`'s same-named helper for the ambient side). `F_EnteringGauntlet` also force-sets `bCodeGiven` if the player is already in an interior, so the gauntlet can't soft-lock on a skipped door beat. |
| S4 | **Point of no return + wave 1** (`F_Wave01`) | Entering `_4_02_SPOTTED_SCOUT`: the back door closes and locks behind the player, `bPointOfNoReturn` kills the Fatty watch thread, all pickups swept (`PickupRemoveAll`), action music locked on (`MS_ActionHigh.rsm`), 2D line 19, barricades 01/02 made invulnerable/untargetable. Wave 1 = 3 nerds (models 6/11/7, weapons 301/307/307) spawned via `F_SetUpWave` at cover points with cover profiles (`4_02_p1_*_cover`), stat overrides (3→80, 0→362, 1→100), flag 134 set, all attack the player. |
| S5 | **Waves 2–3** | Approach triggers `_4_02_P_WAVE2` / `_4_02_P_WAVE3` spawn 4- and 6-nerd waves (same recipe; wave 3's two scouts use `bNoCover` and plain spawns; wave 2 also drops weapon-pickup 301 on `_4_02_ISLANDPICKUP` and locks barricade 03). 2D line 21 replays at each wave. Earlier-wave survivors are force-deleted when the player crosses `_4_02_DELETE_WAVE1` / wave-4 setup (`T_Delete_Wave3_2` re-creates the cannon nerd fresh when the player crosses `_4_02_DELETE_WAVE3`). |
| S6 | **Earnest on the cannon (NIS)** (`F_Wave4`) | At `_4_02_P_WAVE4`: control off, letterbox; the observatory gate locks; Earnest (10) spawns at `_4_02_SPUD_NERD01`, gravity off, stripped of weapon 299, and mans the spud cannon (`GetOnCannon` node spin-waited, then the `UseSpudCannon/.../Fire` AI task). Scripted camera: look at Earnest, then a camera path with look-path (`_4_02_NIS_CAM`/`_4_02_NIS_CAM_LOOK`) while 2D lines 21/31 play and the camera swings to the transformer point (`_4_02_FUSE_POINT`). Earnest then swaps to a ranged AI tree and keeps firing. Objective `4_02_TRANS` (destroy the transformer) + transformer blip; wave 4 spawns (4 nerds); `T_Breaker_Box` starts. |
| S7 | **Breaker box → Earnest bails** | `T_Breaker_Box` polls the pooled prop `SC_ObservTrans` at (33.18, −130.38, 10.68); on `PAnimIsDestroyed` → `F_RunEarnestNIS`: Earnest teleported to `_4_02_NIS_EARNEST`, plays a surrender NIS node under an 80-FOV path camera, 2D line 36, then runs `_4_02_NIS_EARNESTRUN` and despawns. Gate `_SCGATE_OBSERVATORY` unlocks + opens; objective `4_02_GCANNON` + cannon blip. |
| S8 | **Mount the cannon** (`F_GetOnSpudCannon`) | Polls the player's cannon anim state (`/Global/WPCANNON/UseSpudCannon/...`); on mount → `F_PlayerOnCannonAction`: the observatory door (`_DT_OBSERVATORY`) becomes targetable with damage override 1200 and a **prop health bar** (`4_02_OBSDOOR` label); objective `4_02_KILLDOORS` + door blip; two cannon-side nerds spawn; `T_Spawn_Window` + `T_Door_Health` start. While the player is **off** the cannon, the door reverts to invulnerable/untargetable and the cannon is re-blipped (`F_DestroyDoors` toggle) — only cannon fire can kill the door. |
| S9 | **Door siege + window snipers** | `T_Spawn_Window` stages counterattack spawns by door-health band: ≤0.75 → snipers 4+3; ≤0.5 → snipers 2+1; ≤0.2 → respawn any dead snipers. Six window/roof spawn points (`_4_02_O_WN1..4_2`, `_4_02_O_R1/2_1`), each sniper armed with 307 (30 rounds), locked on the player, then walks to an attack point (`_4_02_O_R1_2`/`_O_R2_2` via `PedMoveToPoint` callback) and goes stationary-attack (`PedAttackPlayer`). `T_Door_Health`: on door destroyed → health bar hides, the door plays its open node (`scObsDr.act`), objective complete. |
| S10 | **Enter + pass** (`F_GetInObservatory`, pass block) | Fade, scripted dismount (`WPCANNON/Disengage`), player teleported to `_4_02_PLAYERENDCUT`, window nerds swept, one fixed camera beat. Then: control off, player walks to `_4_02_OBSERVATORY` under letterbox, peds cleared, `MinigameSetCompletion("M_PASS", true, 0)` (no cash), respect messages `MRESPECT_JP5`/`MRESPECT_NM100`, `SetFactionRespect(1, 0)` + `SetFactionRespect(2, 50)`, `MissionSucceed(true, false, false)` (first-arg true = save prompt, **INFERRED**). SMissPass adds the spud-gun grant (§1). |

### Fail conditions and retry

No keyed fail strings exist in 4_02.lua. The only failure is the player being knocked out (engine-side `MissionActive()` false unwinds every stage loop → `MissionFail()` with default presentation). Thad/Fatty cannot be killed in a way that blocks progress (submission thresholds fire first; min-health floors protect them until their beats). No checkpoints: retry replays from S0. Cleanup restores the four suppressed unique models, closes the observatory door prop, unlocks the gate, re-enables POI.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked (`4-02`, extra args `true, true, true`) | `fade` + `letterbox` + `say` |
| CS-2 | S1 Fatty submits | in-engine (letterbox, face-player, cower node, one line) | `letterbox` + `ped_action_node` + `say` |
| CS-3 | S6 Earnest fires the cannon | in-engine (path camera + look path, ~14 s, 2D lines) | `camera_path_cycle` (3_01C) + `say` |
| CS-4 | S7 Earnest bails | in-engine (path camera, surrender node, run-off) | `camera_path_cycle` + `npc_follow_path` |
| CS-5 | S10 observatory entry | in-engine (fade, dismount, teleport, fixed camera) | `fade` + `camera` + `npc_move_to` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 5 | Fatty | library shakedown target; submit at ≤25 hp / grappled / garbage-canned |
| 9 | Cornelius | library flavor; flees when Fatty turns |
| 7 | Thad | gatekeeper; submit at ≤45 hp → door code |
| 10 | Earnest | cannon nerd S6/S7; stat 3→80, gravity toggled |
| 6, 11, 7 | wave 1 | cover fighters, weapons 301/307/307 |
| 11, 8, 5, 9 | wave 2 | + island pickup drop |
| 7, 8, 5, 9, 11, 6 | wave 3 | two scouts `bNoCover` |
| 9, 8, 4, 6 | wave 4 | transformer-defense wave |
| 5, 6, 7, 11, 9, 8 | window snipers ×6 | spawn at door-health bands; weapon 307 ×30 |

### Animation groups / action trees / AI

`F_NERDS`, `IDLE_NERD_C`; `Act/Conv/4_02.act` (`AlgieCower/Cower_Start`, `Break/BreakNode`, `NIS/Earnest_01`, `Blank`), `Act/WProps.act`, `Act/Anim/Ambient.act` (`MissionSpec/GetOnCannon`), `Act/AI/AI.act` (`UseSpudCannon` task), `Act/Anim/N_Ranged_A.act`, `Act/Props/scObsDr.act` (door open/close nodes), `Act/Props/WPCANNON.act` (`Disengage`). Cover **profile files** referenced by name: `4_02_p1_1_cover`, `4_02_p1_2_cover`, `4_02_p1_5_cover`, `4_02_p2_scout_cover`, `4_02_p1_scout_cover`, `4_02_p3_scout1/2_cover`, `4_02_p3_1_cover`, `4_02_p3_5_cover`, `4_02_p3_7_cover`, `4_02_p4_2_cover` — a separate cover-data asset class fed to `PedCoverSetFromProfile`.

### Props / pickups / weapons

- Destructibles: `_DT_OBSERVATORY` (observatory door; damage override 1200 while cannon-manned, health bar, open node on death), pooled prop `SC_ObservTrans` (transformer/breaker at 33.18, −130.38, 10.68), barricades `_4_02_BAR_01/02/03` (script-invulnerable at set stages), `_4_02_OBSDOOR` PAnim instance.
- Doors: `_NERDPATH_BRDOOR` (code door; also ped-locked), `_SCGATE_OBSERVATORY` (gate, player+ped lock).
- Pickup 301 at `_4_02_ISLANDPICKUP` (`MissionPermanent`); `PickupRemoveAll` at the gauntlet door.
- Weapon models loaded: 301, 303, 307, 309; 305 granted only via SMissPass; 299 is stripped from spawned nerds (default carry, **INFERRED**).
- The spud cannon itself = `_SCHOOL_TURRET`-family mounted prop (`WPCANNON` anim set; world props `_SCHOOL_TURRET`/`_SCHOOL_TURRETTRIPOD` exist post-mission per `MainMap.lua`).

### Speech

`M_4_02` scripted ids: 1, 2, 3, 6, 12, 13, 15, 17, 26 — **9 scripted lines**; `M_4_02_2D` ids 19, 21, 31, 36 (radio-style 2D barks); ambient `SCARED`, plus generic grapple/can reactions. Music: `MS_ActionHigh.rsm` locked interactive stream from the gauntlet on; pass/fail stingers (track 10).

### Named world data to extract from `4_02.dat`

- POINTLISTs: `_4_02_PLAYERSTART`, `_4_02_LIBCORNY`, `_4_02_LIBFATTY`, `_4_02_GATEKEEPER`, `_4_02_SECDOORBLIP`, `_4_02_OBSERVATORY`, `_4_02_SPUD_NERD01`, `_4_02_FUSE_POINT`, `_4_02_OBSDOORBLIP`, `_4_02_PLAYERENDCUT`, `_4_02_NIS_EARNEST`, `_4_02_ISLANDPICKUP`, `_4_02_O_WN1_2`…`_4_02_O_WN4_2`, `_4_02_O_R1_1`, `_4_02_O_R2_1`, `_4_02_O_R1_2`, `_4_02_O_R2_2`, wave tables `_4_02_P_NERD01/02/05`, `_4_02_NERD_SCOUT2_01`, `_4_02_P2_NERD01/02/03`, `_4_02_P3_NERD_SCOUT1_01/_02`, `_4_02_P3_NERD_SCOUT2_01/_02`, `_4_02_P3_NERD01/02/05/07`, `_4_02_P4_NERD02/03/05/06`.
- TRIGGERs: `_4_02_GATEKEEPER`, `_4_02_BAR_01/02/03`, `_4_02_SPOTTED_SCOUT`, `_4_02_P_WAVE2/3/4`, `_4_02_DELETE_WAVE1`, `_4_02_DELETE_WAVE3`, `_4_02_OBSDOOR`, `_DT_OBSERVATORY`, `_SCGATE_OBSERVATORY`, `_NERDPATH_BRDOOR`.
- PATHs: `_4_02_NIS_CAM`, `_4_02_NIS_CAM_LOOK`, `_4_02_NIS_EARNESTRUN`, `_4_02_CANNON_PATH`.
- Hardcoded: transformer prop query point (33.1794, −130.376, 10.681).
- Area codes seen: **0** + an interior check (`AreaGetVisible() ~= 0`).
- **Trigger data (DAT): YES** — `DATLoad("4_02.DAT", 2)`; original `4_02.dat` lives in `Trigger.img` per `trigger-format.md`, but the research clone is Lua-only. Extraction pending; not extracted here (by design). TCRF notes 4_02/4_03 DATs also carry unused extra spawns — worth a look when the DAT is pulled.

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles (`min_health`, `mission_critical` not needed — submission thresholds instead), `social_override_set` + `social_action_blacklist` + `gated_social_branch` (greet/taunt wiring), `decision_by_health_percent` (Fatty ≤25, Thad ≤45 submissions — the 2_B pattern), `humiliation_ko_detect` (garbage-can stuffing is exactly the 3_02 can/grapple-state watch), `npc_stat_override`, `group_flee`/`flee_loop`, `wave_group` + `count_threshold` (schema `spawn_wave`), `npc_cover_behavior` + `cover_point_register` (2_03), `mission_spawner` (3_02), `npc_spawn_walk_then_attack` (2_05 — the window snipers), `prop_health_bar` (3_XM), `prop_destroy_threshold_event` (breaker box watch), `door_prop_control` + `door_lock_set` (code door, gate, point-of-no-return lock), `camera_path_cycle` (3_01C — both Earnest NIS beats), `npc_ai_tree_override` (cannon → ranged swap), `faction_attitude_matrix_override`, `interactive_music_stream`, `unique_ped_suppress`, `pickup_spawner`, `player_action_state` (cannon mount detection), `world_state_swap` (post-mission turret props + world pickup DATs), `give_weapon` (SMissPass 305), `set_faction_respect` (absolute variant), `completion_banner`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| S8 cannon-manned door siege | `player_mounted` (ch2), `prop_health_bar` | **mounted_turret_mode**: player mans a fixed weapon emplacement (enter/exit via anim state); while dismounted the assigned target prop is invulnerable+untargetable and the emplacement is re-blipped; fires mount/dismount events |
| S9 health-band counterattack | `spawn_wave`, `prop_destroy_threshold_event` | **prop_health_stage_spawns**: spawn groups armed by a destructible prop's health bands (≤0.75 / ≤0.5 / ≤0.2), final band optionally respawns dead members |
| S2 beat-out-the-code | `decision_by_health_percent`, `social_override_set` | none — submission → info/objective/door-unlock is composition; taunt-into-fight branch is `gated_social_branch` |
| S3 keypad door | `door_prop_control` | none (but the interact source is external — see OPEN) |
| S4 point-of-no-return | `door_lock_set`, `set_flag` | none |
| S6/S7 cannon NIS | `camera_path_cycle`, `npc_ai_tree_override` | none |

Genuinely NEW names introduced here: **mounted_turret_mode**, **prop_health_stage_spawns**. (The `PedCoverSetFromProfile` cover-file asset class is a data-ingest note for the engine port, not a mission primitive; the on/off-cannon door targetability toggle folds into `mounted_turret_mode`.)

## 6. Open questions

- Who actually opens `_NERDPATH_BRDOOR` once the code is known — in-file `F_OpenDoors` is never called; the interact presumably hangs off the door trigger (DAT or area script). **OPEN**; must be resolved before authoring stage S3 in the remake.
- The respect semantics: script sets absolutes (nerds 0, jocks 50) while the message keys and wiki describe deltas (−100/+5) — engine `SetFactionRespect` absolute-vs-delta convention needs one confirmation read. **OPEN**.
- Weapon-id decode for 301/303/307/309 (nerd loadouts) and 299 (stripped default) — **OPEN**, pending `default.ide`/weapon table cross-check (305 = spud gun is the only confirmed id, via the SMissPass comment).
- Why `F_GetInObservatory` sits in a `while MissionActive()` that sets success on first tick — reads like a cut "get inside alive" beat flattened to a cutscene. **OPEN**.
- `F_Social_Greet_Fatty`/`F_Social_Greet_Thad`/`F_Social_Taunt_Thad` and the `cbCannonNerd` attack callback are registered-nowhere/dead in this file — cut social branches. **OPEN**.
- Whether the `bShortCut` skip-Fatty path was reachable in the shipped game (wiki walkthroughs all show the Fatty beat) — **OPEN**.
