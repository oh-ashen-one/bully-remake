# 2_08 — Weed Killer: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_08.lua` (1388 lines) + its area-script partner `AreaScripts/PrepHouse.lua` (329 lines, the persistent Harrington House script that owns Bif, the plant prop and the breakable-prop noise events) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_08 = "Weed Killer"** — **VERIFIED** by the loaded animation group `"2_08WeedKiller"` and by content: the player disguises himself as a preppy (haircut + Aquaberry sweater, no hat), talks or fights his way past Bif into Harrington House, steals a pesticide sprayer on the third floor, destroys a giant weed plant (`_PANIM_FLYTRAP`, `Act/Props/VFlyTrap.act`) in the solarium, and fights back out through the house. Matches the canonical Chapter 2 list entry "Weed Killer (class-gap windows)". The class-gap scheduling is not in the Lua (mission table lives outside) — **OPEN** here.

No surprise in identity, but one structural surprise: **the mission is split across two scripts.** `2_08.lua` never spawns Bif, never watches the plant's health, and never registers the breakable-prop events — all of that lives in the always-on area script for area 32 (`PrepHouse.lua`), which checks `MissionActiveSpecific("2_08")` and talks back through `shared.*` globals (`shared.gBif`, `shared.bBifDefeated`, `shared.bAllowPlayerInside`, `shared.bWeedPlantIsDestroyed`, `shared.nFloorHeard`, `shared.bBifAttacked`, `shared.gAreaDATFileLoaded[32]`). Our framework needs a mission ↔ area-script shared-state channel, not just self-contained mission scripts.

Setting: starts in the school biology room (area 2 = `ischool`, `_2_08_BIOLOGYROOM`), moves outdoors (area 0) for shopping, then into Harrington House — **area code 32, INFERRED = `isc_prep`** (interior; PrepHouse geometry coords around (-524, 118, 50) and (-516, 133, 55) are interior-scale, and `shared.gAreaDATFileLoaded[32]` is set by PrepHouse). New area code beyond the known list: **32 = isc_prep (INFERRED)**.

## 2. Mission flow

Driver: `main()` → `MissionSetup()` (baked cutscene) → `F_MissionSetup()` → a `fMissionStage` function pointer walking `F_StageOne_DisguiseYourself → F_StageTwo_SneakIntoHarrington → F_StageThree_DestroyThePlant → F_StageFour_ClearOutThirdFloor → F_StageFive_FightBif`, with a global `F_MonitorObjectives()` called from most stages and threads `T_PrepsLikePlayer` (whole mission) plus `T_MonitorFirst/Second/ThirdFloor` (inside the house). The plant-health watcher and Bif live in `PrepHouse.lua`.

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **Baked cutscene "2-08"** | `PlayCutsceneWithLoad("2-08", true, true)`; player is **topped up to $12.00** if he has less (haircut/clothes money). Loads `2_08.DAT` + `SP_BoysDorm.DAT`, anim groups, ped models {30, 31, 32, 34, 35, 40, 362}, weapon models {300, 302, 353, 357}, conv tree `Act/Conv/2_08.act`. Preppy door `DT_TSCHOOL_PREPPYL` locked. Player placed in the biology room (area 2). PA silenced. |
| A1 | **Stage 1: disguise shopping** | Checklist of three wearable predicates, each with its own objective key + blip: fancy haircut (blip `_2_08_SALONCORONA`; accepted hair model families `R_GoodBoy_*`, `R_Hwood_*`, `R_HThrob_*`, `R_SShag_*`, `R_ILeague_*`, `R_SSmart_*` — 24 named variants), Aquaberry torso (`R_Sweater1`/`R_Sweater5`; blip `_2_08_CLOTHINGMANAGER`), and **no hat** (any head-slot item fails the gate). Already-satisfied conditions auto-complete at mission start (including the case where the player arrives fully disguised → skips straight to A2). Objectives/blips re-shuffle live as items are bought or removed. **Fail:** if a haircut is still needed and money < $12.00 while the hat state is pending, mission fails with generic key `CMN_STR_06` ("OutOfMoney" path). |
| A2 | **Stage 2: go to Harrington House** | Disguise complete → `DT_TSCHOOL_PREPPYL` unlocks, objective `2_08_HARRINGOBJ` + blip `_2_08_BIFLOC`. Thread `T_PrepsLikePlayer`: while outdoors (area ≠ 32), preppy faction (type 5) attitude toward the player (type 13) is forced to friendly (4) while the disguise holds, reset to default if the disguise breaks. **Changing out of the disguise reverts the mission to stage 1 and relocks the door** (VERIFIED in `F_StageTwo`). |
| A3 | **Bif gate: greet or fight** (in `PrepHouse.lua`) | Bif (model 33) is spawned at `_PREPHS_BIF` with health **and max health cut to 40%** whenever 2_08 is active. Friendly path: Bif greets (queued generic greet lines), the player's social greeting back (`PlayerRegisterSocialCallbackVsPed`, action ids 35/23) sends Bif along `PATH._PREPHS_OPENDOORPATH` playing a door-opening conv node (`/Global/PrepHouse/OpenFoyeurDoor/DoAnim`); the foyer door ends unlocked and held open. Hostile path: hitting Bif flips `shared.bBifAttacked`, objective swaps `2_08_GETBYBIF` → `2_08_BEATBIF` with a Bif char blip, and Bif attacks. **Bif KO'd** → foyer door forced open/unlocked and the "destroy the plant" objective is granted directly. Exact handshake that sets `shared.bAllowPlayerInside` on the friendly first-run path is **not visible in these two files** (`F_OpenHarringtonHouseDoors` has no live first-run caller) — **OPEN**, behavior is still clear: greeted Bif lets you in, beaten Bif's door is opened for you. |
| A4 | **Three floors, three guarded rooms** | Entering area 32 starts threads `T_MonitorFirst/Second/ThirdFloor`. Each floor has a spawn trigger (`_2_08_FIRSTFLOORTRIG`/`SECNDFLOORTRIG`/`THRDFLOORTRIG`) that creates its pair of preppies at the floor's spawn point list: floor 1 Gord (30) + Parker (40), floor 2 Justin (34) + Tad (31), floor 3 Chad (32) + Bryce (35) (names from the script's table keys, **INFERRED** id↔name). Each pair runs a queued two-ped conversation (`M_2_08` lines 38-45 / 29-31 / 32-35, talk/listen anim nodes), then is released to a floor POI hangout (`POI._2_08_*FLOORHANG`). Three provocations turn a floor hostile: **hit either ped** (`PedGetWhoHitMeLast == gPlayer`), **break a prop registered to that floor** (PrepHouse hash events set `shared.nFloorHeard = 1/2/3` — see §4), or the plant-death beat (A6) which makes everyone hate the player. Parker/Tad/Chad get `PedOverrideStat(id, 0, 362)` + stat 1 = 100 (stat ids undecoded — **OPEN**). |
| A5 | **Third floor: grab the pesticide** | Entering `TRIGGER._HOUSETHIRDFLOOR` swaps the objective to `2_08_PESTICIDE` with a blip on `_2_08_POISON`. When the player holds **weapon 395** (the sprayer, **INFERRED** from the mission theme; name not in script), objective completes, action-music streams start (`MS_ActionMid/High`), objective `2_08_DESTROY` appears with a blip on `_2_08_PLANT`, and the plant prop gets a health bar (`PAnimShowHealthBar(_PANIM_FLYTRAP, ..., "2_08_PLANT", ...)`). |
| A6 | **Kill the plant** | In `PrepHouse.lua`: `T_MonitorWeedDestroy` sets `shared.bWeedPlantIsDestroyed` when the flytrap PAnim's health drops to ≤ 0.15 (it also bails early if the player somehow grabs 395 first, then still requires the plant to die). Plant death fires `F_RunPlantNIS()` in 2_08 — see CS-2. Afterward: plant target deleted, **all surviving house preppies set to hate the player**, music locked to `MS_ActionHigh`, objective `2_08_OUTHAR` (get out) with blip `_2_08_MIDDLEFLOOREXIT`, side door `DT_PREPTOMAIN` unlocked, Bif repositioned inside (`_PREPHS_BIFINSIDE`). |
| A7 | **Stage 4: fight down and out** | Fights are tracked per floor (`bFirst/Second/ThirdFloorFight`); dead preps are made ambient. Once the 2nd- and 3rd-floor pairs are down, the stairs door `_DOOR_PREPHOUSE_STAIRS` is closed and locked and a fresh **ambush prep (model 30)** is spawned at `_2_08_SECONDFLOORPED`. When all fights are done and the player steps into `_2_08_DOORSTAIRS`, this prep plays an open-door conv node, the stairs door opens and stays open, and he attacks — the scripted "one more guy" beat. |
| A8 | **Stage 5: Bif rematch at the exit** | If Bif is alive and the player enters `_2_08_BIFATTACKTRIG`: Bif plays a `FIGHT_INITIATE` scripted speech + `ComeOn` conv node, then attacks (attitude hate, `PedAttack`). No arena, no health scripting — a plain fight. |
| A9 | **Leave = win** | With the plant destroyed, the mission ends when the player plays a door-use action (`PedIsPlaying(gPlayer, "/Global/Door/PedPropsActions", true)`): camera control for the transition is disabled, fade, if weapon 395 is equipped the player plays a holster/remove conv node, player is moved to `_2_08_OUTHAR` and a fixed exterior camera (~(103.6, -125.0, 7.5), area 0) fades in. `sMissionState = "Success"`. |
| A10 | **Pass** | Completion banner `M_PASS` with param 3000 (**INFERRED**: $30.00 reward in cents, matching the `F_EndNIS` money-param convention in `Boxing_util.lua`), pass music, player moved to `_2_08_FINALPOS`, `MissionSucceed(false, false, false)`. No chain target in the Lua (**OPEN** — mission table). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Too poor to finish the disguise | stage-1 guard: haircut outstanding + money < $12.00 (hat pending) | `CMN_STR_06` ("OutOfMoney" state) |
| Generic | any other failure path | `MissionFail(false)` no key |

On cleanup: camera/HUD/speech restored, `DT_TSCHOOL_PREPPYL` re-locked via setup next attempt, `shared.*` Bif/key/plant flags reset, and **item 493 is zeroed from the player's inventory unless the mission passed** (`ItemSetCurrentNum(493, 0)`) — **INFERRED**: 493 is the Harrington House key item (pairs with `shared.bGrabbedHarringtonHouseKey` / the blipped key pickup), i.e. the key is confiscated on fail but kept on pass. The key pickup id itself is never assigned in any script we can see (`shared.gHarringtonKeyPickup` is only set to `true`/`nil` yet used with `PickupGetXYZ`) — **OPEN**, likely a DAT-side pickup script. No partial checkpointing; retry replays from A0. Retry behavior on the Bif gate: if `shared.bAllowPlayerInside` is already set, `PrepHouse` spawns Bif at `_PREPHS_BIFMOVED` and re-opens the house (VERIFIED).

### Pass effects (`SMissPass.lua`)

**No `2_08` case exists** in `F_SetupMissionRequirements` — **VERIFIED**. All effects are in-script: $12 money floor at start, `M_PASS` banner param 3000 (probable $30, **INFERRED**), item 493 kept (key, **INFERRED**), and the post-mission world state where PrepHouse leaves the plant dead and the house doors open for non-2_08 visits (VERIFIED in `F_SetupHarringtonHouse`'s else branch).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | A0, before control | baked (`2-08`) | fade in on player in the biology room + letterbox + one `say` beat of original VO, or straight to the shopping checklist |
| CS-2 | A6 plant death (`F_RunPlantNIS`) | in-engine | `letterbox` + fixed `camera` on the plant + `prop_set_state` (VFlyTrap `Death/Dead` hold) + `timer` 1.7 s; then the conditional **solarium ambush micro-scene**: if the player is outside `_2_08_SOLARIUMENTRANCE` and third-floor preps survive, fade, `teleport` survivors to `_2_08_SOLARIUMPEDS`, one plays a taunt node + `say` (`FIGHTING` jumbo), the other a lock-pick loop, both charge (`npc_move_to` player), fade back, control on. If the player is inside the entrance, skip the scene — they just attack. |
| CS-3 | A9 exit | in-engine, camera swap | `fade` + holster `ped_action_node` + `teleport` to `_2_08_OUTHAR` + fixed `camera` outdoors + fade in; fold into the pass banner |

Cutscene-only / conv anim groups: `2_08WeedKiller`, `NPC_Chat_1`, `Hang_Talking`, `NPC_AggroTaunt`, `SGEN_S`, `Px_Urinal` (a POI gag prop, **INFERRED**), `MINI_Lock` (the lock-pick loop used in the ambush scene).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`; names from script variable keys — **INFERRED**)

| Model id | Role | Notes |
|---|---|---|
| 33 | Bif (gatekeeper / rematch) | spawned by `PrepHouse.lua`, not by 2_08; health and max health forced to 40% of normal; social-greet callbacks 35/23; `FIGHT_INITIATE` bark on rematch |
| 30 | Gord (floor 1) + exit ambush prep | ambush copy spawned fresh at `_2_08_SECONDFLOORPED` |
| 40 | Parker (floor 1) | `PedOverrideStat(0, 362)` + stat 1 = 100 |
| 34 | Justin (floor 2) | |
| 31 | Tad (floor 2) | same stat overrides as Parker |
| 32 | Chad (floor 3) | same stat overrides as Parker |
| 35 | Bryce (floor 3) | plays the lock-pick loop in the ambush scene |
| 362 | loaded in the ped-model list, never spawned in the live path | dead load (**INFERRED**) |

### Animation groups / action trees

`LoadAnimationGroup`: `Px_Urinal`, `NPC_AggroTaunt`, `2_08WeedKiller`, `SGEN_S`, `NPC_Chat_1`, `Hang_Talking`, `MINI_Lock`; PrepHouse adds `NPC_ADULT`.
`LoadActionTree`: `Act/Conv/2_08.act` (all mission conv nodes: talk/listen pairs, `ComeOn`, `Idle_Remove`, `OpenSecondFloorDoor`, lock-pick loop, taunt), `Act/Conv/PrepHouse.act` (Bif door open), `Act/Props/VFlyTrap.act` (plant idle/death).

### Props / pickups / weapons

- `_PANIM_FLYTRAP` — the weed plant PAnim: idle node, `Death/Dead` node, health with HUD bar, destroyed at ≤ 15% health.
- Door PAnims / door ids: `DT_TSCHOOL_PREPPYL` (school→Vale gate), `_DOOR_PREPHOUSE_FOYER`, `_DOOR_PREPHOUSE_FOYEROUT` (deleted by PrepHouse), `_DOOR_PREPHOUSE_STAIRS`, `DT_PREPTOMAIN` (side exit, unlocked on plant death), plus geometry swaps `PH_OpenDoor01/02` ↔ `DPI_pDoorBrk` at (-524.291, 118.2, 50.68) and (-516.797, 133.414, 55.6463) with `AreaSetPathableInRadius` nav patches.
- Breakable-prop noise system (PrepHouse, hash-event id 3 = broken): 4 path-enabling breakables (`isc_prep_DPI_prepWinBrk01`, `prepWinBrk`, `pDoorBrk01`, `pDoorBrk02` — also set `nFloorHeard`), floor 1 noise set (2 stable doors, gramophone, fern), floor 2 noise set (2 poker tables, 12 chairs, 2 stable doors), floor 3 noise set (4 plants, 3 vases). ~30 named props; any break alerts that floor's guards.
- Weapons: **395 pesticide sprayer** (mission pickup at `_2_08_POISON`, holstered in the exit beat); models preloaded 300/302/353/357 (**OPEN** which these are; 302/300 look like the standard projectile/melee set). Item 493 = house key (**INFERRED**).

### Speech

`M_2_08` ids used: 29, 30, 31 (floor 2), 32-36 (floor 3), 38-45 (floor 1), 53 (Bif opening the door) — **15 distinct scripted lines**, plus generic event lines (`FIGHT_INITIATE`, `FIGHTING` jumbo, greet/clothes-compliment barks on the friendly path). Music: `MS_ActionMid.rsm`/`MS_ActionHigh.rsm` interactive streams from the pesticide grab onward; pass/fail stingers via `SoundPlayMissionEndMusic`.

### Special entities

`SetNumberOfHandledHashEventObjects(50)` — the breakable-noise system needs a wide prop-event budget. PrepHouse also owns `AreaSetPathableInRadius` nav patches at the two breakable doors and loads `PAn_Prep.DAT` + `SP_Prep_House.DAT` permanently (slot 0).

### Named world data to extract from `2_08.dat` (+ `SP_Prep_House.dat`, `PAn_Prep.dat` in area 32)

- POINTLISTs: `_2_08_BIOLOGYROOM`, `_2_08_SALONCORONA`, `_2_08_CLOTHINGMANAGER`, `_2_08_BIFLOC`, `_2_08_TOSOLARIUM`, `_2_08_PLANT`, `_2_08_POISON`, `_2_08_SOLARIUMPEDS`, `_2_08_MIDDLEFLOOREXIT`, `_2_08_EXITLOC`, `_2_08_OUTHAR`, `_2_08_FINALPOS`, `_2_08_SECONDFLOORPED`, `_2_08_FIRSTFLOORSPAWN`, `_2_08_SECONDFLOORSPAWN`, `_2_08_THIRDFLOORSPAWN`, `_PREPHS_BIF`, `_PREPHS_BIFMOVED`, `_PREPHS_BIFINSIDE`.
- PATHs: `_PREPHS_OPENDOORPATH` (Bif's door walk); `PedFollowPath(NISPed, PATH._, ...)` in the ambush scene is a decompiler artifact — original path name **OPEN**.
- TRIGGERs: `_2_08_PLANTTARGET`, `_2_08_FIRSTFLOORTRIG`, `_2_08_SECNDFLOORTRIG`, `_2_08_THRDFLOORTRIG`, `_2_08_TRIGGERSTAIRS`, `_2_08_DOORSTAIRS`, `_2_08_BIFATTACKTRIG`, `_2_08_SOLARIUMENTRANCE`, `_2_08_TOSOLARIUM`, `_HOUSETHIRDFLOOR`, `_PREPHOUSE_MAIN2ND`, `_PANIM_FLYTRAP`, `_DOOR_PREPHOUSE_FOYER`, `_DOOR_PREPHOUSE_FOYEROUT`, `_DOOR_PREPHOUSE_STAIRS`, doors `DT_TSCHOOL_PREPPYL`, `DT_PREPTOMAIN`.
- POIs: `_2_08_FIRSTFLOORHANG`, `_2_08_SECONDFLOORHANG`, `_2_08_THIRDFLOORHANG`.
- Hardcoded coords: plant-death camera (-543.90, 136.86, 59.51)→(-544.72, 136.30, 59.36); ambush camera (-531.17, 137.61, 57.60); door geometry swaps (-524.291, 118.2, 50.68), (-516.797, 133.414, 55.6463) + 4 nav-patch radii; exit camera (103.568, -125.024, 7.459)→(103.499, -126.012, 7.600) area 0.
- Area codes seen: **0** (outdoors), **2** (`ischool` biology room start), **32** (Harrington House interior, **INFERRED** = `isc_prep`).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by (ch1 set) | New primitives needed |
|---|---|---|
| A0 cutscene + money top-up | `fade`, `cutscene_skip_on_retry`, `money_ensure/money_take` | — |
| A1 disguise checklist | `objective_checklist`, `dynamic_blip`, `player_has_item` | **outfit_predicate** (worn hair-model family / torso model / head-slot-empty queries as mission conditions), **shop_purchase_event** (objective completes when the predicate flips, i.e. via salon/clothing-manager state — needs the `shared.PlayerInClothingManager`/`playerShopping` shopping-busy guard) |
| A2 faction loves the disguise | `set_faction_respect`-adjacent attitude API | **conditional_faction_attitude** (per-frame predicate → faction-to-faction attitude override + auto-reset; must suspend inside area 32) |
| A3 greet-or-fight gate | `social_interact`, `ped_hit_event`, `blip_on_npc`, `npc_follow_path`, `door_prop_control` | **gated_social_branch** (greet-callback opens friendly path, hit opens hostile path, KO opens door — three-way branch on one NPC), **area_script_shared_state** (mission script ↔ persistent area script via `shared.*` + `gAreaDATFileLoaded[]` readiness flags) |
| A4 floor guards | `spawn_npc` bundles, `npc_sit_at_poi`, `conversation_beat_machine`, `npc_heard_event`, `npc_stat_override` | **object_destroyed_noise_alert** (breakable-prop hash events mapped to per-floor alert level; ~30 registered props, event-budget sizing) |
| A5 pesticide pickup gate | `player_has_weapon/ammo`, `objective_checklist` | **prop_health_bar** (`PAnimShowHealthBar` on a destructible prop, named HUD bar) |
| A6 plant death | `prop_set_state`, `geometry_instance_toggle`, `camera`, `say`, `timer` | **prop_destroy_threshold_event** (fires at health ≤ 15%, not just 0) |
| A7/A8 exit fights | `wave_group`, `npc_tether_trigger`, `door_prop_control` (stairs door choreography + opener anim) | — |
| A9 exit-to-win | `player_action_state`, `fade`, `teleport` | **mission_complete_on_door_action** (pass triggered by the player's door-use action node while a condition holds — not a trigger volume) |
| pass/fail | `completion_banner`, `give_item`, `force_complete_mission` | **inventory_item_consume_on_fail** (item 493 zeroed unless passed — keep-on-pass / confiscate-on-fail item rule) |

## 6. Open questions

- What sets `shared.bAllowPlayerInside` on the first friendly run, and what assigns the real pickup id to `shared.gHarringtonKeyPickup` (only `true`/`nil` seen in Lua)? Suspect a DAT-side or conv-tree callback — both handshakes work behaviorally but the writer is invisible.
- Item 493 and weapon 395's canonical names (id-only in scripts); 493 = house key is inferred from context.
- `PedOverrideStat(id, 0, 362)` / stat 1 on three of the six guards — stat semantics undecoded.
- Which named path the ambush scene's `PedFollowPath(NISPed, PATH._, ...)` used (decompiler dropped the name).
- Weapon models 300/302/353/357 preloaded but never handed out in this script — probably for the guards' AI trees; **OPEN**.
- How the mission is offered in the world (start trigger/blip, class-gap windows) — mission table is outside the Lua.
