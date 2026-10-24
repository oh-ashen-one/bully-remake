# 5_B — Showdown at the Plant (Edgar Munsen, chemical plant; Chapter 5 boss): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap6/5_B.lua` (891 lines, complete) + `SMissPass.lua` (no 5_B case — **VERIFIED** absence) + `AreaScripts/MainMap.lua` + `SZone.lua` (weapon ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_B = Showdown at the Plant**, the Chapter 5 boss: Jimmy vs. **Edgar Munsen** (Townie leader) inside the abandoned chemical plant in Blue Skies Industrial Park. **VERIFIED** by content (boss ped model 91 with `DO_Edgar`/`AI_EDGAR_5_B` trees, chem-plant doors/elevator PANims, steam hazards, `MS_ShowdownAtThePlant*.rsm` streams, pass-side `SetFactionRespect(3, +100)` with the `MRESPECT_DP100` banner key — faction 3 + "DP" = dropouts/townies, **INFERRED** by key name — and `UnlockYearbookPicture(91)`) and externally: the [fandom Chapter V page](https://bully.fandom.com/wiki/Chapter_V) lists "Showdown at the Plant `5-B`" directly. Lives in the `chap6/` folder despite being a Chapter 5 mission — same folder/id mismatch family as 3_S03 (ch3 id, Ch5 mission, in `secnd/`). The folder digit is an authoring-era artifact.

**Chain:** fandom's id map gives `6-01` Go See the Principal → `5-06` Busting In, Part I → `5-07a` Busting In, Part II → **`5-B`** → `6-02`/`6-03` Complete Mayhem → `6-B` Final Showdown. `5_07a.lua` ends with a plain `MissionSucceed(true, false, false)` (no in-script launch of 5_B) and 5_B has no `SMissPass` case, so both links are mission-table work outside the Lua (**INFERRED**, same posture as 4_B2 → Chapt4Trans). Note the repo catalog folds the pair: catalog #64 "Busting In, Part II" == script `5_07a`, and **5_B "Showdown at the Plant" is a separate catalog row** the catalog currently misses (catalog #65 "Complete Mayhem" == `6_02`/`6_03`; `6_B` Final Showdown is also unlisted — worth a catalog correction pass).

Structure: hand-rolled stage machine — `main()` polls `STAGE_NUM` 1→4 (`StageOne`…`StageFour`) plus a nested `FinalStage` 1→3 inside stage 4. Persistent threads: `T_ElevatorControl` (player-operated lift), `T_CameraShots` (zone camera presets), `T_SLUDGECHECK` (fall-into-sludge fail), `T_ShieldBlips` / `T_PipeBlips` (pickup guidance). Baked cutscene `"5-BC"` on pass only; everything else is in-engine.

Setting: chemical plant interior (**INFERRED** area 20 — the debug path `AreaTransitionPoint(20, _5_B_START)`; the pass NIS returns to area 0), no clock/weather forcing. Edgar gets damage-taken multipliers 0.3 (type 0) / 0.2 (type 3), stat 34 → 0, stat 8 → 85, flag 107; the player gets HP floored to 200, flag 58, and a weapon-inventory snapshot (restored only on fail; deleted on pass — the bat/pipe rule, see B3).

## 2. Mission flow

Driver: `MissionSetup()` (`5_Ba.DAT` — note the `a` suffix — layer 2) → `main()` → `MissionInit()` (loads, doors, player placement) → stage loop → pass/fail branch.

| # | Beat | Logic (functional) |
|---|---|---|
| E0 | **Setup** | `MissionInit`: fade from black; interactive music + action-tree speech off; anim groups `DO_STRIKECOMBO`/`BOXING`/`C_Wrestling`; trees `DO_Edgar`/`AI_EDGAR_5_B`/`Conv/5_B`; prop models {387, 388, 389, 342, 402, 401, 384}; punishment system off; plant doors `5_B_CHEM_DOOR/_2` + `DT_ChemPlant_DoorL` locked; security doors `_5_B_SECDOOR1/2` created and locked; weapon snapshot; player HP ≥ 200 + flag 58; player to `_5_B_START`; all damageable PANims reset. |
| E1 | **Stage 1: find Edgar** | Objective 1 (search the plant). Edgar (91) stands at `_5_B_EDGAR`, infinite sprint, gravity off, stimuli-ignored, attitude 4 vs ped-type 13. Player entering `_5_B_ENTRANCE` (or hitting Edgar) → Edgar invulnerable + bark (id 21, supersize), blip on Edgar, elevator-shaft geometry instance on, the **elevator descends** (`CH_Elevator` PAnim along `PATH._5_B_ELEVATOR_DOWN`, node-1 callback sets `ELEVATOR_DOWN`), objective 1 → 2 (reach Edgar). While Edgar rides, bark id 23 re-fires every 3 s. Player lift controls: `_5_B_LIFTUP`/`_5_B_LIFTDOWN` triggers + button 9 drive the elevator PAnim down/up (speed 1). |
| E2 | **Stage 2: descent** | Elevator done → doors C/D open (0.5 speed), elevator loop sound off, Edgar walks to hardcoded (-768.04, 64.84, 6.20) then snaps to `_5_B_PLAYERENDNIS`. Player entering `_5_B_STAGE2_START` (alive) → 2 s hold, music swaps Low → **High**, `CS_EDGAR_PIPE()` cutscene → stage 3. |
| E3 | **CS_EDGAR_PIPE (in-engine)** | `shared.edgarFightStarted = true` (the retry skip — see Fail); control off; Edgar to `_5_B_EDGARINNIS`; two hardcoded camera shots; player repositioned; a **shield pickup (387)** is created at `_5_B_FALLEN_SHIELD` as `"PermanentMission"`; Edgar bark 22; Edgar walks to `_5_B_GRAB_GOOD` (5 s anti-stuck reposition loop) and does a scripted prop-interact until he holds **weapon 342 (pipe)**; the player is walked onto the shield point, plays the weapon-pickup node, tutorial `5_B_Tut1` (shield use, 5 s), Edgar bark 2; elevator doors close; `T_ShieldBlips` starts. |
| E4 | **Stage 3: shield duel** | Edgar made vulnerable, AI tree `AI_EDGAR_5_B`, attitude vs type 13 → hostile, stat 13 → 90, health bar on (`5_B_01`), `PedSetAIButes("5_B")`; control back, audio focus to player; objective 2 → 3 (beat Edgar). Loop until `EdgarShieldDestroyed()`: true when Edgar holds **none** of {342, 402, 401, 384} (his pipe + shield-piece set, **INFERRED**) **or** his HP ≤ 2/3 of max (he is then force-disarmed). While looping: one-time tutorial `5_B_Tut2` if the player has no shield (387/388); Edgar bark 1 if he connects while the player is shieldless. `T_ShieldBlips`: 5 `_5_B_SHIELDBLIPS` points blipped only while the player lacks shield 387; standing at one with the shield clears it (**INFERRED**: spare-shield pickup spots). Exit → Edgar invulnerable/stopped, music High → Low. |
| E5 | **Stage 4: the chase downstairs** | `FinalStage` 1: Edgar disarmed, flag 13, stimuli-ignored, follows `PATH._5_B_EDGARTOSTAIRS` (with retry loop baked inline); chem doors locked to the player but **unlocked to peds**; path-node-1 callback → doors opened/stay-open + shoulder-run reaction + steam sound + `F_SteamOn` (directional `steam_pipe` FX at every `_5_B_STEAM` point for 5 s; player inside `_5_B_STEAMAREA` plays a steam-burn reaction). |
| E6 | **Stage 4 NIS: final room** | Player in `_5_B_LOWSTAIRCASE` → fade, control off, `MS_RussellInTheHole.rsm` (recycled Ch1 boss track, VERIFIED name); player snapped to `_5_B_PLAYERENDNIS`; both chem doors closed + locked; face-off camera; Edgar prop-interact node + bark 24; Edgar moves to `_5_B_EDGARMOVETO`; `F_SetUpEdgarFinal` (flag 13 off, vulnerable, stat 13 → 70, attack); tutorial `5_B_Tut3`; `T_PipeBlips` (2 `_5_B_PIPEBLIPS` points, same show-while-unarmed logic for weapon 342). |
| E7 | **Stage 4: the duel** | Edgar stationary until: the player holds any of {342, 402, 401, 384}, **or** Edgar is hit, **or** 8 s elapse → he attacks. One-time Edgar bark 4 when the player grabs the pipe. Edgar dead → objective 3 complete, cameras released, 1.5 s, mission passes. (Where pipe pickups come from is not in the Lua — likely DAT-placed, **OPEN**.) |
| E8 | **Pass** | Models/anim groups unloaded; baked `"5-BC"`; interactive stream stopped; Edgar deleted; control off; transition to area 0 `_5_B_END`; final in-engine beat: Edgar re-created at `_5_B_END` el 2, mutual facing, widescreen, hardcoded camera, Edgar speech 20 (jumbo), Edgar walks `_5_B_END` el 3, player speech 42, Edgar deleted. Completion banner `M_PASS` + `MRESPECT_DP100`; end music 10; **townie respect +100** (faction 3); **yearbook picture 91**; `F_UnlockYearbookReward()`; `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player dead | `F_PlayerIsDead()` poll in `main()` | `M_FAIL_DEAD`, fail music 10, weapon snapshot restored |
| Player falls into the sludge | `T_SLUDGECHECK` (`_5_B_FALL_INTO_SLUDGE` trigger) drops `mission_running` | same fail branch (always `M_FAIL_DEAD`) |

Retry: `MissionFail(true, true, …)` — restart allowed. **`shared.edgarFightStarted` is the cross-attempt checkpoint**: it persists across the restart, and `StageOne` then runs a fast-forward — Edgar invulnerable, elevator descended at 5.55× speed, and the player teleported straight to `_5_B_DEBUG_STAGE3` el 2 (the fight arena) (**VERIFIED** code path; the `DEBUG_STAGE3/4` lists double as retry spawn points).

### Rewards / pass side-effects

- In-script only (no `SMissPass` case — **VERIFIED** absence): faction 3 (townies) +100 via `MRESPECT_DP100`; yearbook picture 91 (Edgar); `F_UnlockYearbookReward()`. No money, no forced class completions (contrast 4_B2).
- Story consequence per wiki: beating Edgar earns the Townies' allegiance and opens Blue Skies — the respect bump is the mechanical half of that.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | E3 Edgar arms himself / player takes the shield | in-engine, 2 fixed cameras + 2 speeches + pickup node | `camera` ×2 + `say` ×2 + `npc_move_to` + `ped_action_node` + tutorial hint |
| CS-2 | E6 final-room face-off | in-engine, fixed camera + face pair + bark | `fade` + `letterbox` + `camera` + `say` |
| CS-3 | E8 post-fight talk (Edgar 20 / player 42) | in-engine, hardcoded camera | `camera` + `speech_sequence` (2 lines) |
| CS-4 | E8 victory | baked `"5-BC"` | `fade` + `completion_banner` + original outro card |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 91 | **Edgar Munsen** (boss) | `DO_Edgar` action tree + `AI_EDGAR_5_B` AI tree; stat 34→0, stat 8→85, stat 13→90 (stage 3) / 70 (finale); damage multipliers 0.3/0.2; flags 107 + 13; invulnerability toggled per phase; shield set + pipe weapons |

### Animation groups / action trees

`DO_STRIKECOMBO`, `BOXING`, `C_Wrestling` (player combat set); `Act/Anim/DO_Edgar.act`, `Act/AI/AI_EDGAR_5_B.act`, `Act/Conv/5_B.act` (steam reaction, shoulder-run); stock `Act/WProps.act` prop-interact, `Act/Weapons.act` pickup, ladder actions (camera gating).

### Props / PANims / weapons

- PANims: `CH_Elevator` (player-drivable lift), `CH_ElevDoorC/D` (elevator doors), `_5_B_SECDOOR1/2` (security doors, open/close = path speed ±0.5 with clunk), chem doors `_5_B_CHEM_DOOR/_2` (PAnim + door-lock pair), geometry instance `CH_ElevShaft`.
- Weapon/prop models: 342 (pipe — Edgar's and the player's final weapon), 402/401/384 (Edgar's shield set, **INFERRED**), 387 (player shield pickup; 388/389 loaded too — degraded shield states, **INFERRED**).
- FX: `steam_pipe` directional effects at `_5_B_STEAM`; audio loops `ChemElevator`, `ChemDoorOpen/Close/Clunk`, `PipeSteam`.

### Speech

`M_5_B` ids 1, 2, 4, 20, 21, 22, 23, 24, 42 (Edgar + player). Tutorials `5_B_Tut1/2/3`; objective keys `5_B_obj1/2/3`; crawl hint `5_B_Crawl`; health-bar label `5_B_01`. Music: `MS_ShowdownAtThePlantLow/High.rsm`, `MS_RussellInTheHole.rsm` (recycled), end track 10.

### Named world data to extract from `5_Ba.DAT` (trigger data **VERIFIED present** — `DATLoad("5_Ba.DAT", 2)`; not yet extracted to `imported/triggers/`; note the `a` suffix on the DAT name)

- POINTLISTs: `_5_B_START`, `_5_B_EDGAR`, `_5_B_END` (3), `_5_B_PLAYERENDNIS`, `_5_B_EDGARINNIS`, `_5_B_EDGARMOVETO`, `_5_B_FALLEN_SHIELD`, `_5_B_SHIELDBLIPS` (5), `_5_B_PIPEBLIPS` (2), `_5_B_STEAM` (n), `_5_B_DEBUG_STAGE3` (2), `_5_B_DEBUG_STAGE4` (2).
- PATHs: `_5_B_ELEVATOR_DOWN/_UP`, `_5_B_ELEVATOR_C/_D`, `_5_B_EC_CLOSE/_ED_CLOSE`, `_5_B_DOORA_OPEN/_DOORB_OPEN`, `_5_B_EDGARTOSTAIRS`.
- TRIGGERs: `_5_B_SECDOOR1/2`, `_5_B_CHEM_DOOR/_2`, `_5_B_ENTRANCE`, `_5_B_STAGE2_START`, `_5_B_GRAB_GOOD`, `_5_B_LIFTUP/_LIFTDOWN`, `_5_B_LOWSTAIRCASE`, `_5_B_STEAMAREA`, `_5_B_FALL_INTO_SLUDGE`, `_5_B_LADDERCAMSHOT`, `_5_B_CHEMCAMSHOT`, `_5_B_LADDERCAMSHOT02/03`, `_5_B_CHEMCAMSHOT02`, `_5_B_CRAWL`.
- Door ids also locked by string name: `5_B_CHEM_DOOR`, `5_B_CHEM_DOOR_2`, `DT_ChemPlant_DoorL`.
- Hardcoded cameras: E3 (-763.02, 72.36, 8.18 → -763.22, 73.31, 7.93; then -772.48, 82.24, 8.01 → -769.62, 81.16, 7.47), E6 (-764.69, 78.43, 1.91 → -760.47, 78.22, 1.40), E8 (111.42, -513.41, 4.78 → 113.10, -513.06, 4.42), chem-plant tour cams in `T_CameraShots`.
- Area codes seen: 20 (debug path — plant interior, **INFERRED**), 0 (pass NIS).

## 5. Framework gaps (beat → primitives)

Deduped against the ch1–ch4 gap pools and the `MissionSubsystem.h` schema.

| Beat | Covered by | New primitives needed |
|---|---|---|
| E0 setup | `weapon_inventory_snapshot`, `player_buff` (HP floor), `ped_flag_set`, door locking | **panim_path_mover** (PAnim pool prop driven along a DAT path at a set speed — sign = open/close direction — with path-node completion callbacks; covers the elevator, elevator doors, security doors, chem doors; existing `door_prop_control`/`prop_set_state` don't model speed-signed movers) |
| E1 lift | `interact` (partial) | **lift_call_buttons** (player-in-trigger + button-press drives a named mover up/down; the game's first player-operated platform) |
| E1/E2 gates | `trigger_enter`, `npc_move_to` | reuse; elevator-done = mover callback event feeding a stage gate |
| retry skip | — | **cross_retry_stage_skip** (`shared.*` flag set late in the mission survives the restart; `StageOne` then fast-forwards — accelerated set-piece + teleport to the arena. Distinct from `script_checkpoint`, which is in-mission) |
| E4 shield duel | `npc_health_bar`, `npc_ai_tree_override`, `damage_taken_multiplier`, `npc_stat_override` | **boss_shield_break_phase** (boss carries a destructible shield weapon-set; phase exits on boss-disarmed OR HP ≤ 2/3 max with force-disarm; shieldless-hit bark; one-time unarmed tutorials) |
| E4/E7 guidance | `hint_when_unarmed_in_zone` (partial — hint text only) | **unarmed_pickup_blip_set** (blip set over pickup points shown only while the player lacks weapon X; per-point clear on pickup) |
| E5 steam | `effect_spawn_at_points` (partial) | **timed_hazard_fx_zone** (directional FX at a point list with a lifetime; player inside the paired trigger plays a reaction node; auto-off) |
| E7 standoff | — | **boss_standoff_arm_trigger** (boss stationary/passive until player equips one of a weapon list, lands a hit, or an 8 s timeout; one-time remark on equip; then attacks) |
| chem doors | `door_lock_set` (partial) | **door_lock_peds_only** modifier (`AreaSetDoorLockedToPeds` — locked to the player, passable for the fleeing boss) |
| AI tuning | `npc_ai_tree_override` | **ai_butes_profile** (`PedSetAIButes("5_B")` ↔ `"Default"` — named global AI tuning profile swap) |
| camera zones | — | **triggered_camera_preset_zones** (`T_CameraShots`: trigger-enter switches to a hardcoded camera (ladder-anim gated), exit restores; one-shot each with progress memory) |
| E8 pass | `completion_banner`, `set_faction_respect`, `yearbook_unlock`, `fade`, `teleport` | none new |
| sludge fail | `trigger_enter` → fail | none new |

## 6. Open questions

- Stat semantics: Edgar's stats 8/13/34 and flag 107/13 meanings; player flag 58 (also seen on Damon in 4_B2 — cheap-KO immunity?).
- Weapon models 388/389 and 402/401/384: shield degradation states vs. Edgar's shield pieces — needs `default.ide`/weapon data cross-check.
- Where the stage-4 pipe pickups (342) are spawned — no `PickupCreatePoint` for them in the Lua; likely DAT pickup entries in `5_Ba.DAT`.
- `F_EdgarThirdStage`/`F_EdgarSecondStage` return stage checks and are never called from Lua — likely `lua_condition_query_from_tree` hooks consulted by `AI_EDGAR_5_B.act` (**INFERRED**); `T_RUNEDGAR`, `F_InvulnerableFlag`, `F_NextStageSet`, `Stage2SetUp` are dead code (**VERIFIED** — no callers).
- `shared.edgarFightStarted` lifetime: does anything ever clear it (a later mission, `SInitGl`)? If not, a failed-then-abandoned 5_B could leak the skip into a fresh campaign session.
- The `PedSetEffectedByGravity(edgar, false)` opener and elevator ride — does Edgar visibly stand in the lift, and does the player riding it up/down desync his descent (speed 1 vs 5.55 paths)?
- Mission-table side: exact 5_07a → 5_B → 6_02 chain, star-marker window for 5_B, and whether 5_B's pass seeds anything for Complete Mayhem (no `SMissPass` case, so probably nothing).
