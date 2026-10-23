# 2_B — Boxing Challenge boss fight (Darby): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_B.lua` (1299 lines) + `SMissPass.lua` (`F_SetupMissionRequirements` 2_B case) + `SZone.lua` (weapon/item id table). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_B = the Darby Harrington boss fight — the actual Chapter 2 boss.** **VERIFIED** by content: boss ped model 37 with dedicated AI tree `Act/AI/AI_DARBY_2_B.act` and boss action tree `Act/Anim/BOSS_Darby.act`, boxing outfit swap (`ClothingSetPlayerOutfit("Boxing")` / `"Boxing NG"`), boss music `MS_DishonorableFight.rsm`, preppy minions, anim groups `"N2B Dishonerable"` (sic) / `"NIS_2_B"` / `"Boxing"`, and the `SMissPass.lua` 2_B case performing the chapter transition (see Rewards). Matches the canonical Chapter 2 list entry "Boxing Challenge (8-25, chapter boss)" as its boss half.

Relationship to the sibling doc: `2_09-boxing-challenge.md` covers the gateway mission (walk to the club, beat Bif in the boxing minigame, baked cutscene `"2-B"`); **2_B is the arena fight that gateway leads into** (already flagged in that doc). 2_B has **no baked intro cutscene in this file** — it opens in-engine with Darby fleeing through the lounge. Its own completion cutscene is `"2-BB"` (baked). Whether a baked intro is played by the outer launcher before `MissionSetup` is **OPEN**.

Setting: entirely indoors in the preppy boxing gym, **area code 27 = `iboxing`** (VERIFIED by `AreaTransitionPoint(27, POINTLIST._2_B_PSTART)` and the `_IBOXING_ESCDOOR*` ids; consistent with the 2_09 doc).

## 2. Mission flow

Driver: `MissionSetup()` → `MissionInit()` → `main()`, with a `LibObjective` param table of three objectives: `objStayAlive` (failure → `F_FailMission`), `objDefeatDarby` (success → `F_CompleteMission`), `objOpenDoor` (success → `F_BeginDarbyFight`). Threads: `F_MonitorDarby`, `T_DarbyShouldAvoid`, `T_DarbyThrowBottle`, `F_MonitorSpawnedPreppies`, `F_PickupSpawn`, `F_MonitorFirstPreps`, `T_DoorChecker`, `T_LockDoors`, `TextQueueThread` (local text-queue system), `T_ObjectiveMonitor`, `L_MonitorTriggers`.

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **Setup** | Player clothing backed up and swapped to the boxing outfit; punishment system off; player health refilled to max; **weapon inventory snapshot taken** (`PedSaveWeaponInventorySnapshot` — restored on fail, deleted on pass: a bare-knuckle fight); transition to area 27 `_2_B_PSTART`; population zeroed; POI disabled; all escape doors closed. |
| A1 | **In-engine opening (first attempt only)** | Player placed at `_2_B_PSTART`, heading -172.3, fixed camera, widescreen. Darby (37; 300 HP; stat overrides 38/39 → 50; infinite sprint; damage-taken ×0.2 for damage types 0 and 3) spawns at `_2_B_DARBY_START`, plays his opening line (`M_2_B` id 1) and flees along `PATH._2_B_DARBY_FLEE` with a map blip; at path node 13 the four lounge doors close behind him (`CbDarbyRun`); he then vaults to his rest point (first `F_DarbyRest`). **On retry** (`IsMissionFromRestart`) the chase is skipped: Darby is moved straight to the lounge (`F_MoveDarbyToLounge`, fade out/in). Meanwhile the two posted preppy pairs are already live: `first_wave` (30 + 35 at `_2_B_PREP_02/03`) attacks the player immediately; `boxing_ring` (34 + 32 at `_2_B_PREP_06/10`) joins when the player is close or hits them. Biff (133) lies KO'd ringside (cheap, untargetable, invulnerable, `BiffKO` node). Objective `2_B_11` added (beat the preppies — **paraphrase**). |
| A2 | **First-wave fight** | `F_MonitorFirstPreps` tracks both groups (`L_PedAllDead`). KO'd preppies may drop health pickup **502 = "PChealth"** (VERIFIED name via `SZone.lua`): 50% chance, forced 100% when player HP < 40%. |
| A3 | **Ring intro NIS** | Entering `TRIGGER._2_B_BOSS_FIGHT_INTRO` **or re-opening any closed escape door** (`F_OpenDoor` objective — the doors were closed at A1's node 13) fires `F_BeginDarbyFight`: fade, control off, widescreen, surviving posted preps deleted, mutual lock-on player↔Darby, then a scripted exchange (speech ids 3, 8, 13, then 6 or 11 depending on whether preps are still up) over four hardcoded camera/FOV shots. Doors lock for the duration. Biff re-plays his KO node. |
| A4 | **Summon phase** | Darby retreats to `_2_B_DARBY_REST` (vault anim over the ropes — **INFERRED** from the `DrbyVault` node), invulnerable, cheering every 30 s (id 11 cycling a dead table — see dead code), and summons preppies (speech id 5): **2 at a time** from two door spawn points (`_2_B_DOOR01/02_PREP_SPAWN` → `_MOVE`), models drawn from {30, 32, 34, 35, 40} with a no-immediate-repeat rule; the first two summoned are flagged "immortal" (flag 58 — when their HP hits 0 they just go untargetable). Summoned preps walk to `TRIGGER._2_B_BOSS_FIGHT` then attack; summon doors unlock for 5 s to let them in (`T_LockDoors`). While preps live, Darby lobs projectiles when the player comes near the bar (`T_DarbyThrowBottle`, `TRIGGER._2_B_PLAYER_NEAR_BAR`; weapon models **310 = apple** and **327 = bottle** preloaded, VERIFIED names; which one he throws **INFERRED** bottle from the `BOSS_Darby/Special/Throw` tree name and the loop's function name). |
| A5 | **Door bracing (player counter-play)** | Two `DRBrace` PAnim props (`_DRBRACE`, `_DRBRACE01`) are set to their `Useable` node with a pulsating light and made targetable. Bracing a door (or the brace being destroyed) calls `F_BlockDoors`/`F_RemoveDoor` (invocation path not in this Lua — **OPEN**, likely the prop action tree): that door's spawn location is invalidated, the wave cap drops by 1, Darby barks (id 12). When **both** doors are shut off: spawning stops, surviving preps outside the ring are deleted, Darby's final return begins (speech id 14). `T_DoorChecker` additionally disables a door's spawn loc while the player stands in its trigger (anti-camp). |
| A6 | **Boss cycle → solo fight** | Cadence (**INFERRED** from the flag flow): 5 s after each wave spawns, Darby returns toward the ring (`F_DarbyReturn`: vault anim, invulnerability off, attitude hostile, attacks; his current HP becomes the new phase max). His AI tree gates real engagement on `F_FightCondition` (no preps alive + spawning still on + not at rest), so he postures while preps fight and commits once a wave is wiped. If the **player is knocked down** (belly-up anim state) with no preps alive, Darby backs off to rest again and — 4 s later — calls the next wave. On a projectile hit, Darby gains a 5 s avoid window (`T_DarbyShouldAvoid`, entity flag 56). A player-landed hit that drops him into the boxing-stun subtree forces the knockout anim (`F_MonitorDarby` + `F_PedIsHitByPlayer`). **Win:** Darby HP ≤ 0 with no preps alive → `F_CompleteMission`. |
| A7 | **Pass** | Brace props set `NotUseable`, 5 s hold, immortal-prep flags cleared, anim groups unloaded, baked cutscene `"2-BB"`, doors re-opened, `SetFactionRespect(5, 100)` (faction 5 = preppies, **INFERRED** from context), `UnlockYearbookPicture(37)`, `F_UnlockYearbookReward()` (defined in `main.lua`), saved-data slot 14 = 0, `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player dead | `objStayAlive` failure condition `F_MonitorHealth` | `M_FAIL_DEAD` (fail music, doors re-opened, weapon snapshot restored) |

No mission-critical NPCs, no timers, no leave-area fail — a pure arena fight. Retry replays from A0/A1 but skips the A1 chase (`IsMissionFromRestart` branch, VERIFIED in-file).

### Rewards / pass side-effects (`SMissPass.lua` 2_B case + in-script)

- `ChapterSet(2)` if chapter ≤ 2 (0-indexed: enters the third chapter — **the chapter boss transition**, VERIFIED) + loads the `Christmas` special entities if 3_08 not yet completed.
- Force-completes (with success-count increments): `C_Chem_2`, `C_Art_2`, `C_Wrestling_2`, `C_English_2`, `C_Photography_2`, **and** `C_Photography_1`; then `HUDPhotographySetSaveLevel1/2(true)` (camera photo-save unlocks).
- In-script: preppy respect 100, yearbook picture 37 + yearbook reward, saved-data 14 = 0.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | A1 opening chase | in-engine (path + fixed camera) | keep as-is: `letterbox` + Darby `npc_follow_path` + `fade`; one `say` beat |
| CS-2 | A3 ring intro exchange | in-engine, ~10 s, 4 hardcoded camera/FOV shots | `letterbox` + 2 fixed `camera` angles + 3-4 `say` beats, then doors-lock `flag` |
| CS-3 | A7 victory | baked (`2-BB`) | `fade` + `completion_banner` + short original outro card |
| CS-4 | A4 summon bark / A5 brace beats | micro | none: `say` bark over gameplay |

Cutscene-only anim groups: `N2B Dishonerable`, `NIS_2_B` (both loaded for the whole mission). Dead code: `T_DebugController`, `F_SecondaryAttack` (empty, hooked to `_2_B_SECOND_ATTACK`), the `gDarbyReturningComments`/`gDarbyCheer` tables (16-21) — the index cycles but playback uses hardcoded ids 9/11; constant `SUMMON_HEALTH_PERCENT` never used.

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 37 | **Darby** (boss) | AI tree `Act/AI/AI_DARBY_2_B.act`, action tree `Act/Anim/BOSS_Darby.act`, 300 HP, stat overrides 38/39→50 (meaning undecoded), infinite sprint, damage-taken ×0.2 (types 0, 3), projectile-dodge flag 56, "immortal" flag 58 |
| 133 | Biff | KO'd ringside dressing only (consistent with 2_09's opponent id 133) |
| 30, 35 | `first_wave` pair | at `_2_B_PREP_02/03` |
| 34, 32 | `boxing_ring` pair | at `_2_B_PREP_06/10` |
| 30/32/34/35/40 | summoned waves | half normal max health; first two flagged 58 |

### Animation groups / action trees

`LoadAnimationGroup`: `N2B Dishonerable`, `NIS_2_B`, `Boxing`.
`LoadActionTree`: `Act/Conv/2_B.act` (mission conv/vault/BiffKO/player-belly-up nodes), `Act/Anim/BOSS_Darby.act` (boss move set incl. `Special/Throw`), `Act/Props/DRBrace.act`, `Act/AI/AI_DARBY_2_B.act`, `Act/HitTree.act` (knockout node). Player KO pose read from `/Global/HitTree/.../BellyUp`.

### Props / pickups / weapons

- Doors: `_IBOXING_ESCDOORL/R`, `_IBOXING_ESCDOORL01/R01` (PAnim + area lock/pathable control), `_DT_IBOXING_DOORL` (locked for the mission).
- `_DRBRACE`, `_DRBRACE01` barricade props: `Useable`/`Falling`/`destroyed` states, pulsate light, targetable.
- Weapons: 310 apple, 327 bottle (Darby's throws, **INFERRED**), player inventory stripped via snapshot. Pickups: 502 = PChealth (VERIFIED name), dropped by KO'd preps and spawned by `F_PickupSpawn` when 1 prep remains and player is in the ring.

### Speech

`M_2_B` live ids: 1, 3, 5, 6, 8, 9, 11, 12, 13, 14 — **10 distinct scripted lines** (dead tables reference 16-21). Music: `MS_DishonorableFight.rsm` stream; pass/fail stingers via `SoundPlayMissionEndMusic`.

### Named world data to extract from `2_B.DAT`

- POINTLISTs: `_2_B_PSTART`, `_2_B_DARBY_START`, `_2_B_DARBY_REST`, `_2_B_DARBY_RETURN_FROM_REST`, `_2_B_DARBY_BOTTLE_THROW`, `_2_B_PLAYERNIS01`, `_2_B_PREP_02`, `_2_B_PREP_03`, `_2_B_PREP_06`, `_2_B_PREP_10`, `_2_B_BIFF`, `_2_B_DOOR01_PREP_SPAWN`, `_2_B_DOOR01_PREP_MOVE`, `_2_B_DOOR02_PREP_SPAWN`, `_2_B_DOOR02_PREP_MOVE`.
- PATHs: `_2_B_DARBY_FLEE` (node 13 = door-close beat).
- TRIGGERs: `_2_B_BOSS_FIGHT`, `_2_B_BOSS_FIGHT_INTRO`, `_2_B_SECOND_ATTACK`, `_2_B_PLAYER_NEAR_BAR`, `_2_B_DRBRACE_DOOR`, `_2_B_DRBRACE_DOOR01`, `_IBOXING_ESCDOORL`, `_IBOXING_ESCDOORR`, `_IBOXING_ESCDOORL01`, `_IBOXING_ESCDOORR01`, `_DT_IBOXING_DOORL`, `_DRBRACE`, `_DRBRACE01` (props).
- Hardcoded camera coords (ring intro): (-729.03, 385.36, 299.53 → -729.71, 386.08, 299.42), (-728.88, 386.52, 299.41 → -729.74, 387.04, 299.41), (-734.72, 388.75, 299.48 → -733.73, 388.59, 299.47); bottle-throw reference (-735.36, 389.93, 298.04); opening camera (-715.24, 369.98, 297.38 → -714.30, 370.23, 297.15).
- Area codes seen: **27** only (`iboxing`, VERIFIED).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| A0 setup | `mission framework`, outfit swap | **weapon_inventory_snapshot** (strip on start; restore on fail; discard on pass) |
| A1 opening | `letterbox`, `camera`, `npc_follow_path` + node cb, `cutscene_skip_on_retry` (`IsMissionFromRestart` variant) | — |
| A2/A4 waves | `wave_group` / `all_dead(group)`, `npc_drop_item_on_ko` (needs player-HP-conditional chance variant), `npc_stat_override` | **summon_wave_from_doors** (N door spawn points, random model without immediate repeat, walk-to-arena-then-attack, wave cap shrinks as doors are braced, 5 s door unlock window) |
| A5 bracing | `door_prop_control`, `prop_set_state` | **door_barricade_prop** (targetable/interactable prop with Useable→Falling→destroyed states + pulsate light; placing/destroying it disables a spawn door; anti-camp: spawn disabled while player inside the door trigger) |
| A4/A6 boss | `boss_profile`, `health_bar(name)`, `bark_loop`, `player_action_state` (player belly-up), lock-on pair | **boss_rest_cycle** (retreat to rest point w/ move-to + teleport fallback, invulnerable while resting, return on wave-clear/player-KO; HP at return becomes phase max); **projectile_dodge_window** (on hit by projectile-type weapon: dodge/invuln flag for N ms); **damage_taken_multiplier** per damage type (×0.2) — can fold into `boss_profile` |
| A7 pass | `completion_banner`, `set_faction_respect`, `give`-style unlocks | **yearbook_unlock** (picture id + reward hook), `world_state_swap` (Christmas — already in ch1 set), `chapter_set` (already in ch1 set), `force_complete_mission` (classes — already in ch1 set) |
| fails | `mission_critical`-style fail via objective conditions | none new |

## 6. Open questions

- `AI_DARBY_2_B.act` / `BOSS_Darby.act` internals: attack cadence, throw conditions, what stat overrides 38/39 and damage-type indices 0/3 actually are, and how `F_FightCondition`/`F_DodgeCondition` are wired into the trees (same undecoded-tree situation as 1_02's tutorial trees).
- How `F_BlockDoors`/`F_RemoveDoor` are invoked (no registration in this Lua; presumably the DRBrace prop tree or `LibPropNew` — behavior is clear).
- Ped flags 56 (dodge) and 58 (immortal/cheap-KO) semantics.
- Whether a baked intro precedes `MissionSetup` (2_09 boundary) and how the world mission table chains 2_09 → 2_B → `Chapt2Trans`.
- Why model 102-style extra loads are absent here but `LoadModels` includes exactly the six preppy ids — clean file, no dead loads.
