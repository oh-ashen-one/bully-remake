# 2_07 — Beach Rumble: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_07.lua` (827 lines, complete) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_07 = "Beach Rumble"** (canonical Chapter 2 list, available 8-23) — **VERIFIED** by the loaded animation group `"2_07BeachRumble"` (mission title embedded in the group name) and by content: the player chases Gord (model 30) to the beach/pier with the greaser Ricky (28) as a recruited ally, fights waves of preppies (model pool 31/32/34/40), and takes Gord's trophy (weapon/pickup 385). Setting is the rich-area beach and pier/lighthouse (`PATH._2_07_LIGHTHOUSE_PATH`, triggers `_2_07_THEDOCK`, `_2_07_REAR_PIER`), all in area 0 (`AreaTransitionPoint(0, ...)`; no other area code referenced). Boxing interactive music streams (`MS_StreetFightLarge{Low,Mid,High}_Boxing.rsm`). No surprise.

Dead code in the file (never called from the live path): `F_SetUpPeds` (alternate spawn setup), `F_Activate_Group` (a three-prep alley ambush at `_2_07P_ALLEY01/02/03` via `L_PedLoadPoint`/`L_PedExec` — a cut alley wave, **INFERRED**), `F_PushPlayerAway` (the blocker's shove anim sequence — likely wired to a trigger outside this file or cut, **OPEN**; its guard `bCanPush` is never read), `F_BryceAttack` duplicate logic, unused locals (`gLeaveArea`, `gJustinAllies`, `gPrepBikeAttack`).

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → `F_Stage1()` (setup + loop) → `F_Stage2()` → `F_Stage3()`, with three concurrent threads (`T_PlayerLeavesRichArea` fail watch, `T_PlayerIsFast` anti-skip, `T_PrepSpawner`/`T_MonitorSpawnedPreppies` wave engine started in stage 2). Stage loops poll objective functions every frame.

| # | Beat | Logic (functional) |
|---|---|---|
| R0 | **Baked cutscene "2-07"** | `PlayCutsceneWithLoad("2-07", true, true, true)` in `MissionSetup`; `shared.gCutsceneRunning` bracketed around it. Loads `2_07_BEACHA.DAT`, disables POI groups, arms the boxing music streams. No attempt-count skip in-file — **INFERRED** engine attempt count (ch1 pattern); **OPEN**. |
| R1 | **Setup** | Models loaded (28, 35, 32, 34, 40, 31), anim groups `2_07BeachRumble`/`Boxing`/`F_NERDS`/`Cheer_Posh1`, weapon model 385 (trophy) requested. Gord created at `_2_07_GORDFOOTRUN` holding the trophy (weapon 385, 1 ammo): invulnerable, untargetable, ignore-stimuli/attacks, infinite sprint, damage-taken multipliers 0.1 (classes 0 and 3), own action tree `Act/Anim/P_2_07_Gord.act` whose trophy-shaking branch reads the Lua condition `F_GordTheTrophyShaker` (**INFERRED** — same tree-condition pattern as 1_02's gates). Player transitioned to `_2_07_PLAYER_START`; all vehicles within 100 m of the player deleted (anti-bike-catchup measure, **INFERRED**). |
| R2 | **Stage 1: chase Gord to the beach** | Ricky (28) spawned at `_2_07_GREASER_START` and recruited as the player's ally (greasers set hostile to preppies: type 5 vs 13 attitude 0); Ricky's slingshot (303) destroyed. Gord runs `PATH._2_07_GORD_RUN_TO_BEACH` (arrival callback at node 8). Ricky moves to `_2_07_GREASER_RUN`, faces the player, speaks (speech id 6). Objective `2_07_MOBJ_02` (follow/chase Gord, **paraphrase**) with a blip on Gord. Stage advances when the player enters `TRIGGER._2_07_PREPPY_NINJA_TRIG` (beach arrival). |
| R3 | **Stage 2 setup: the pier siege** | Prep safehouse door `DT_trich_SafePrep` locked. Trophy-shake condition cleared. Gord taunts (speech id 54), turns hostile-to-player attitude. Parker (40) spawned at `_2_07_LAST_THUGS` and walks to `_2_07_MOVEPARKER`, where he becomes a **blocker**: gravity off, cheering AI node, facing 150, locked onto the player, stationary (`CB_BlockerInPosition`). The wave engine starts (`T_PrepSpawner`). Bryce (35) spawned with weapon 312 and drop item 362, walks to `_2_07_BRYCEMOVE` then cheers facing 330. If the player is not on the dock yet, Gord locks on stationary/targetable and Bryce attacks. |
| R4 | **Waves 1-3** (`T_PrepSpawner` + `T_MonitorSpawnedPreppies`) | Continuous spawner: up to **2 concurrent** preppies (random model from {31,32,34,40}, never repeating the last three picks), spawn point chosen by player position (`_2_07_ALTENEMYSPAWN` when the player is outside `_2_07_OUTSIDEDOOR`, else `_2_07_LAST_THUGS`), then each spawn **jumps in** (`PedJump` to `_2_07_LAST_THUGS_JUMP` with a style variant 1-6 — three per side depending on whether the player is on `_2_07_REAR_PIER`, no fall damage), then attacks. Most spawns carry drop item 362 (2 of 3 variants). When ≤1 preppy remains alive the wave ends: `Wave_Counter` increments, Gord barks per wave (ids 51/53/52, only if the player is in the beach trigger). **End of wave 1:** `F_GordComesOut` — Gord drops invulnerability and walks `PATH._2_07_LIGHTHOUSE_PATH`; at node 1 he stations facing 320, cheering (a spectator boss watching his minions). **End of wave 3:** `F_WavesCompleted` — pier unlocked (`PlayerNotAllowedOnPier = false`), Parker converts from blocker to attacker (`RestoreBlocker`), Bryce attacks, Gord is fully restored (damage multipliers to 1, invulnerable/ignore flags off) and attacks. |
| R5 | **Gord fight + trophy drop** | Gord fights as a normal but pre-buffed enemy. When his health hits ≤0 the trophy weapon is destroyed on him; on his KO the trophy spawns as a **permanent mission pickup** (385) — at `_2_07_trophySpawn` if he fell behind the door trigger (`_2_07_BEHINDDOOR`), else from his body — with a blip. Objective swaps to `2_07_MOBJ_End` (stage 3). Any earlier acquisition also works: `PlayerHasWeapon(385)` is polled every frame in stage 2 and passes immediately. |
| R6 | **Stage 3: take the trophy** | Loop waits on `PickupIsPickedUp(Trophy)`. On pickup: all spawned preppies + Gord + Bryce + Parker are made ambient and flee the player (`F_MakePrepsFlee`), spawner/monitor threads terminated. |
| R7 | **Pass scene (in-engine)** | Gord's defeat line (speech id 55) if valid; control off, widescreen, player safed; player faces heading 135 with a camera offset (0.2, 2, 0.8 / look 0, -0.2, 1.2); completion banner `M_PASS` with tag `2_07_UNLOCK` (**OPEN** what the tag gates — likely a room trophy display, **INFERRED**), respect message `MRESPECT_PM10`; pass music (stinger id 10); if on foot the player plays the `CelebrateTrophy` conv node; `SetFactionRespect(5, 15)` — faction 5 **= preppies, INFERRED**, set absolute to 15; `MissionSucceed(false,false,false)`; fade back, control on. No `SMissPass.lua` branch exists for 2_07 — **no item/weapon/class unlock from the pass-requirements table, VERIFIED** by absence (the trophy itself is the reward, via the banner tag). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player leaves the rich/beach area | `T_PlayerLeavesRichArea`: not inside `TRIGGER._2_07_OUT_OF_AREA` → immediate fail (allies dismissed/made ambient first); not inside the larger `_2_07_OUT_OF_AREA_WARN` → 1 s warning text re-print (`2_07_MOBJ_02` reused) | `2_07_FAIL` |

That is the only fail path — Ricky, Gord, and the preppies are all expendable, and there is no timer. On fail: fail music (stinger 10), `MissionFail(false, true, key)`. No checkpoints: retry replays from R0/R1. Cleanup restores default population/vehicles/punishment, unlocks `DT_trich_SafePrep`, removes the counter HUD, stops the music stream, unloads the two anim groups, and sets `shared.gRefreshRacePosters = true` (world poster refresh hook, consumed outside — **OPEN**).

Anti-skip: `T_PlayerIsFast` — if the player reaches `_2_07_THEDOCK` before the pier is unlocked, or Gord himself leaves the dock trigger, Gord is restored early and `Wave_Counter` jumps to 4 (waves over). Also while locked, the dock is policed by the Parker blocker.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | R0, before control | baked (`2-07`) | fade in at `_2_07_PLAYER_START` + letterbox + one `say` beat of original VO |
| CS-2 | R2 chase | in-engine gameplay | no cut: Gord's path run + blip is already our `npc_follow_path` + `blip_on_npc` shape |
| CS-3 | R3 siege setup | in-engine | letterbox optional: one `say` (id 54) over a `camera` on Gord, then control |
| CS-4 | R7 trophy celebration | in-engine, ~5-10 s | letterbox + fixed-offset `camera` + `say` (55) + one celebratory `player_anim` + `completion_banner` with tag; fold outro `fade` into banner |

Cutscene-adjacent anim groups: `2_07BeachRumble`, `Boxing` (fight set), `Cheer_Posh1` (blocker/spectator cheering), `F_NERDS` (loaded, no explicit node in-file — used by tree internals, **OPEN**).

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 30 | Gord (chase target → cheering spectator → boss) | own action tree `P_2_07_Gord.act`; stat override 31→60; invuln/untargetable/mult 0.1 staging, restored for the real fight; carries trophy 385 |
| 28 | Ricky (greaser ally) | recruited follower for the whole mission; slingshot destroyed on spawn |
| 40 | Parker (dock blocker → attacker) | gravity-off cheering blocker, push-away scene (dead/external wiring), `RestoreBlocker` on hit |
| 35 | Bryce (cheerer → attacker) | weapon 312, drop 362, converts on hit (`PedIsHit`) or on waves-complete |
| 31, 32, 34, 40 | wave preppies | random pick with 3-deep no-repeat; jump-in entrance; drop 362 on 2 of 3 variants |

Item **362** = KO drop — **OPEN** (same unnamed drop id as 1_03/1_05; no name in script). Weapon **312** = Bryce's weapon — **OPEN** (ch1 notes flag 312 as attackers' weapon in 1_09 and ammo given by 2_01's pass effect; a thrown/melee weapon, exact item undetermined).

### Weapons / pickups

- **385 = the trophy** — **VERIFIED** by naming (`gGordShakeThatTrophy`, `_2_07_trophySpawn`, `CelebrateTrophy`): carried as a weapon by Gord, destroyed on his KO, respawned as a `PermanentMission` pickup, win condition is holding it.
- No other pickups created.

### Speech

Scripted `M_2_07` ids: 6 (Ricky rally), 51, 52, 53 (per-wave Gord barks), 54 (siege-start taunt), 55 (defeat), 56 (Gord restored/attacks) — **7 distinct lines** (large category). Music: interactive streams `MS_StreetFightLarge{Low,Mid,High}_Boxing.rsm`; pass/fail stingers id 10.

### Named world data to extract from `2_07_BEACHA.DAT`

- POINTLISTs: `_2_07_PLAYER_START`, `_2_07_GORDFOOTRUN`, `_2_07_GREASER_START`, `_2_07_GREASER_RUN`, `_2_07_LAST_THUGS`, `_2_07_LAST_THUGS_JUMP`, `_2_07_ALTENEMYSPAWN`, `_2_07_BRYCEMOVE`, `_2_07_MOVEPARKER`, `_2_07_trophySpawn`; dead-code: `_2_07P_ALLEY01/02/03`.
- PATHs: `_2_07_GORD_RUN_TO_BEACH` (node 8 = beach), `_2_07_LIGHTHOUSE_PATH` (node 1 = Gord's spectator spot).
- TRIGGERs: `_2_07_PREPPY_NINJA_TRIG` (beach arrival / stage gate), `_2_07_OUT_OF_AREA`, `_2_07_OUT_OF_AREA_WARN`, `_2_07_THEDOCK`, `_2_07_REAR_PIER`, `_2_07_OUTSIDEDOOR`, `_2_07_BEHINDDOOR`; door `DT_trich_SafePrep`.
- Hardcoded coords: Parker's push facing (245.93, 330.70, 2.42); pass facing heading 135; Gord spectator heading 320; Bryce heading 330; Parker heading 150.
- Area codes seen: **0 only**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1 set | New primitives needed |
|---|---|---|
| R0 cutscene + retry | `fade`, `letterbox` | none new (`cutscene_skip_on_retry` already named in 1_03/1_04) |
| R1 boss pre-staging | `spawn_npc` flag bundles, sibling `boss_profile` (1_B) | **tree_condition_function** (Lua predicate read by an NPC action tree — the trophy-shake gate; same family as 1_02's tutorial gates, now a named need) |
| R1 delete vehicles | — | **vehicle_area_purge** (delete all vehicles within R of a point) |
| R2 chase + ally | `npc_follow_path`, `blip_on_npc`, `recruit_ally` (2_S04) | none new |
| R3 dock blocker | `crowd_ring` (1_B) partially | **zone_blocker_npc** (stationary gravity-off cheering guard on a forbidden zone; push-away scene on approach; converts to attacker on `ped_hit_event` or zone unlock) |
| R4 waves | sibling `wave_group` (1_03) | **continuous_wave_spawner** (respawn to a concurrency cap until ≤k alive → wave++, per-wave bark hook, stop after N waves → callback; spawn-point switch on player trigger; random model with no-repeat window); **npc_jump_entrance** (`PedJump` to a point with style variant + no-fall-damage, then attack) |
| R4/R5 Gord staging | `npc_invulnerable/untargetable toggles` (1_03), `boss_profile` | none new — damage-taken multipliers and staged restore already belong to `boss_profile`; spectator-cheer state is `crowd_ring` behavior applied to the boss |
| R5 trophy drop | `pickup_spawner`, sibling `npc_drop_item_on_ko` | **boss_drops_carried_weapon** (destroy carried weapon on KO → spawn permanent pickup at body or fallback point, blip it); condition **pickup_picked_up(id)**; shortcut condition `player_has_weapon` (already named in 1_03) |
| R6 mass flee | `flag` | **group_flee** (iterate spawn table + named peds → make ambient + flee player; extends 1_03's A9 flee beat into a named op) |
| fails | `leave_area_fail` (1_03), `zone_exit_countdown` (1_04) | **stay_in_zone_fail** variant (fail when *outside* a trigger, with a larger warn ring re-printing hint text — the inverse nesting of ch1's leave-area patterns; fold-able into `leave_area_fail` with a warn-trigger parameter) |
| anti-skip | — | **sequence_skip_guard** (player reaches late zone early → force-advance wave counter + restore boss; specific, but cheap: `trigger_enter` + set counter) |
| pass | `completion_banner`, `set_faction_respect`, `give_weapon` | none new (banner **tag** `2_07_UNLOCK` is the existing banner's tag field) |

## 6. Open questions

- `Act/Anim/P_2_07_Gord.act` internals: how `F_GordTheTrophyShaker` gates his behavior and what his fight AI does once restored (undecoded tree, same status as ch1 combat trees).
- `F_PushPlayerAway` wiring: no call site in this file — external trigger registration (`LibTriggerNew`-style, from the DAT) or cut content.
- What banner tag `2_07_UNLOCK` unlocks (trophy room display guess, **INFERRED**) and what `shared.gRefreshRacePosters` refreshes (race poster world objects, consumed outside the Lua).
- What pickup 362 and weapon 312 are (ids only).
- Whether the baked `2-07` cutscene is skipped on retry (no attempt check in-file).
- `F_NERDS` group load: no node references it in-file.
- Exact dock/pier/beach trigger geometry and the safehouse door's position (pending DAT extraction).
