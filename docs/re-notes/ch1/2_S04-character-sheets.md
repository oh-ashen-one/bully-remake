# 2_S04 — Character Sheets: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources (read-only, nothing copied): decompiled Lua `Bully-SE-scripts/scripts/secnd/2_S04.lua` (1699 lines, complete), cross-refs `SMissPass.lua` (`F_LaunchingMission` 2_S04 block; **no** 2_S04 branch in `F_SetupMissionRequirements`), `SFaction.lua`, `AreaScripts/Bdorm.lua`, `chap1/Chapt1Trans.lua`, and our own `chapt1trans-chapter-transition.md` / `1_03-the-setup.md`. Structure described in our own words; no dialogue, cutscene script or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_S04 = "Character Sheets"** — Melvin's optional side mission: four of his character sheets are held by bullies at four spots around the school grounds; collect all four (each stash is a distinct mini-encounter) and return them to Melvin in the library. Confidence: **VERIFIED**.

Evidence:
- Loaded anim group `"2_S04CharSheets"` and cutscene id `"2-S04"` name it directly.
- The HUD counter icons are `charasheet`/`charasheet_x`, the pickup model id is 497 (also the inventory item the player accumulates, cleared on cleanup), and `AreaScripts/Bdorm.lua` awards a boys'-dorm trophy on completion whose comment ties it to character sheets pinned on the wall.
- Quest giver is ped model 6, held in script global `gMelvin` and removed from the ambient world while the mission is available (`SFaction.lua`: `PedSetUniqueModelStatus(6, -1)` when available, restored on completion). Model-name join (`ND2nd_Melvin`) **INFERRED** from `bully-mact-tool/DB/HASHES_TITLES.txt` naming, consistent with the role.

### Chapter placement — the one surprise

The prompt context calls this a Chapter 1-era mission, but the engine hooks it to the chapter boundary: `SMissPass.lua F_LaunchingMission` fires `DebugMissionPassDependant(MissionGetIndex("Chapt1Trans"))` **when 2_S04 is launched** (same hook fires when 2_S06 passes). Per `chapt1trans-chapter-transition.md`, that mechanism is how the chapter-1→2 transition gets applied at the very start of chapter-2 content. `DebugMissionPassDependant` is engine-side (**OPEN** internals), but the effect reads as: launching this side mission guarantees the Chapt1Trans interstitial has run/passed. Combined with the `2_` prefix and the `secnd/` folder, treat 2_S04 as **chapter-2-onset content** for scheduling purposes (available once the chapter transition is done), not mid-chapter-1 — even though fan canon often lists it as a chapter-1 side mission. Flagged as a contradiction between canon placement and engine evidence; **INFERRED** the engine evidence is authoritative for our mission table.

### Rewards and side effects (pass)

- Cash via the pass banner: `MinigameSetCompletion("M_PASS", true, 1000)` → **$10.00** (money unit is cents: 1_G1 pays 2000 = canonical $20; **INFERRED** unit, VERIFIED value).
- Dorm trophy: `Bdorm.lua` adds +1 trophy (sheets pinned on the bedroom wall) when completed — **VERIFIED**.
- Melvin restored to the ambient world (`SFaction.lua`) — **VERIFIED**.
- **No** weapon/item/outfit/unlock/faction-respect branch: 2_S04 appears nowhere in `F_SetupMissionRequirements` or `F_MissionFactionChanges` — **VERIFIED absence**. This mission is a pure cash + collectible beat; it gates nothing downstream except the chapter-transition guarantee above.

## 2. Mission flow

Driver: `main()` → `OpeningCutscene()` → `MissionInit()` → patrol-path disable spin → `CreateCharacters()` → free-roam collection loop guarded by four concurrent monitor threads (`T_SheetMonitor`, `T_Sheet1..4AttackMonitor`). Sheets can be collected **in any order**; each has its own trigger-gated encounter.

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Baked cutscene "2-S04"** + setup | `PlayCutsceneWithLoad("2-S04", true)` inside `OpeningCutscene`; `MissionSetup` loads `2_S04.DAT`, action trees `2_S04Greeting.act`, `Act/Conv/2_S04.act`, `Act/Gifts/Give2S04.act`, 13 anim groups, and weapon models 349/309/320/303/392. Interactive music `MS_EpicConfrontation` Low/Mid/High armed. Nerd↔bully attitudes set hostile both ways. POI disabled, rain cleared, Melvin suppressed from ambient population. No attempt-count skip in this file — retry replays the cutscene unless the engine skips it (**OPEN**, same pattern gap as 1_03). |
| S1 | **Melvin walks off** | After the cutscene, Melvin is created at `POINTLIST._2_S04_MELVIN` (mission-critical, non-targetable, ignores stimuli; social actions 28/29/32/35 against him disabled) and walks `PATH._2_S04_TOLIBRARY`; a cleanup thread despawns him at the last node. The player is placed at `POINTLIST._2_S04_START`. He is gone for the whole collection phase, so he cannot be killed mid-mission. |
| S2 | **Collection phase opens** | Four map blips at the sheet points (`_2_S04_SHEET1..4`), a four-entry objective checklist (`2_S04_C01..C04`) added without printing, one headline objective (`2_S04_OBJ1`), HUD counter `charasheet` 0/4, prefect patrol path `_SGD_PREFECT3` disabled. Sheets 1, 3 and 4 guards are spawned now; sheet 2's peds spawn on approach. |
| S3a | **Sheet 1: the rochambeau duel** | Two bullies posted at the sheet point (models 85 + 102; stationary, min-health 20, non-hostile until provoked, hit-callbacks armed). Player entering `TRIGGER._2_S04_SHEET1`, **or hitting either guard**, **or climbing in over the library wall** (`_2_S04_LIBRARYJUMP` + wall-climb/jump anim check — skips the fade/reposition) all converge on the same sequence: guards are deleted and respawned asleep, control off, fade + player repositioned to `_2_S04_SHEET1START`, widescreen scripted-dialog scene (5 conv-tree dialog nodes exchanged between player, guard and bruiser across 4 fixed camera setups), then the bruiser walks up and the **rochambeau** fires: the bruiser plays the sack-kick action node on the locked player, a 1 s emergency timeout, then the player gets an 8 s window (`gRochambeauTimeLimit`) in which the duel resolves. Win/lose callbacks `cbRochambeauWon/Lost`/`cbSacked` and the state helpers `PlayerIsSacked/PlayerRecovered/PlayerKOd` are **never invoked from Lua** — they are action-tree callbacks/conditions, so the duel's input logic lives in undecoded `.act` trees (**INFERRED**: timed reaction, exactly what input is **OPEN**). On win: guard drops the sheet immediately, both bullies flee the player. On loss (or timeout): the pair linger at `_2_S04_SHEET1EXIT` (tethered to `TRIGGER._2_S04_TETHERBULLY`, wander), and the player must KO the guard to drop the sheet. Attacking either guard after the scene makes both attack. |
| S3b | **Sheet 2: auto-shop ambush** | Entering `TRIGGER._2_S04_STARTSHEET2` spawns the stash guard (99), a goon (102), and three nerd bystanders (Thad 7, Fatty 5, "health nerd" 3), all tethered to `TRIGGER._2_S04_AUTOSHOPAREA`; a trash-can prop (`_2_S04_TRASHCAN`, purpose **OPEN**) is created. Entering `_2_S04_NERD_ESCAPE` sends the nerds fleeing along escape paths. Entering `_2_S04_SHEET2` triggers the fight: guard + goon attack. **5 s later Thad returns as a recruited ally** (stat overrides: invulnerable to player, non-targetable, high fight stats, auto-engage) — a temporary AI companion beat. Meanwhile the health nerd circles back along `_2_S04_NERDRETURN`, drops a health pickup (model 502 soda can) near the player, then escapes for good. Sheet drops when the **guard** (not the goon) is KO'd. |
| S3c | **Sheet 3: rescue the teased nerd** | A bully (102) is mid-tease on Bucky (8) when the player approaches `TRIGGER._2_S04_SHEET3OUTER`: a one-time skippable in-engine scene (fade to a fixed camera, ~4 s `WaitSkippable`, fade back) plus a looping tease exchange (paired tease/teased anims + a 7-line alternating speech loop between the two, repeating every ~10 s). Entering the inner `TRIGGER._2_S04_SHEET3` **or hitting either ped** ends the tease: guard attacks the player, Bucky walks a loop path and cheers at the end; if the guard dies, Bucky plays a freed-nerd reaction and wanders off. Sheet drops on the guard's KO. |
| S3d | **Sheet 4: the fleeing smoker** | A bully (99) leans on a wall smoking (`BullyWall_Smoke` idle). Entering `TRIGGER._2_S04_SHEET4` (or hitting him) starts the chase: he stops smoking, taunts, and flees along `PATH._2_S04_FLEE1→4` in sequence with infinite sprint. At each leg's end he waits (crouch-aim stance if the player is beyond 6 m) until the player closes within 6.7 m (`FLEE_DISTANCE`) or hits him, then runs the next leg, barking arrival taunts on a timer. While fleeing, if the player is within 4 m he drops marbles (weapon 349, capped at 3 drops per leg) that trip the pursuit — a moving hazard beat. After **2 full laps** he stops, tethers to `_2_S04_SCHOOLPOP` and turns to fight. Any hit also breaks the flee (and destroys his marble stock). Sheet drops on his KO. |
| S4 | **Sheet bookkeeping** (`T_SheetMonitor`) | Watches all four guards: on each guard's death (after his encounter's arming flag), spawns pickup 497 + blip at the corpse (2 s delayed drop unless scripted instant). Each pickup collected: blip removed, counter +1, next checklist entry ticked. Sheet 1 additionally drops instantly on a rochambeau win. |
| S5 | **Return to Melvin** | At 4/4: counter hidden, Melvin recreated at `_2_S04_MELVIN` (gift-receiving variant), blip on him, new objective (`2_S04_OBJ2`). Main loop exits; waits for the player to come within 3 m of Melvin (while polling that he isn't dead). |
| S6 | **Handover + pass** | Control off, widescreen, both peds invulnerable and locked face-to-face; player plays the give anim from `Give2S04.act`, pass banner with the $10 reward, pass music, Melvin cheers; fade, camera reset, `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player dead | polled in the main collection loop | none (`MissionFail(false, true)`, fail music) |
| Melvin killed | only reachable during S5/S6 (he is despawned before then); a death line plays (speech id 84) before the fail screen | `2_S04_FAIL01` |

Retry: second `MissionFail` arg true → retry allowed; no checkpointing, a retry replays from S0. On cleanup: item 497 count zeroed, POIs/patrols/weather/population restored, Melvin's ambient status restored, all sheet pickups/blips cleared, threads terminated, interactive music stopped.

### Dead/unused code (do not port)

`WorldSetPopulation` (population-trigger choreography — the two triggers survive only as NPC tethers), `NerdEscape` (the S3b escape is driven inline by `NerdsEscape`), `Sheet3_StopNerdTease`, `PedsDoConversation`, `F_BruiserTaunt`, `F_PlaySpeechWait`, `F_HaveAllSheets`, `F_MelvinReceivedSheets`, `F_SetPlayerLoc`, `EmergencySackPlayer`. `PlayerIsSacked/PlayerRecovered/PlayerKOd` and the `cbRochambeau*` callbacks are tree-facing hooks, not dead — but their callers are in undecoded action trees.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | S0, before control | baked (`2-S04`) | fade in at `_2_S04_START` + letterbox + 1-2 original `say` beats establishing Melvin's request, or straight to S2 |
| CS-2 | S3a rochambeau dialog | in-engine (4 fixed `CameraSetXYZ` shots + conv-tree dialog) | `letterbox` + fixed `camera` shots + `say` per line; the duel itself becomes a `qte_duel` beat (see §6) with paired anims |
| CS-3 | S3c tease intro | in-engine micro-scene (fade, fixed camera, skippable 4 s) | `fade` + `camera` + letterbox; keep short |
| CS-4 | S6 handover | in-engine (give anim + cheer + banner) | `camera` close-up + `pair_anim` (give/cheer) + `completion_banner` with the reward |
| — | S1 Melvin walk-off | scripted ambient, not a cutscene | `npc_follow_path` + despawn on last node |

Cutscene-only anim groups: `NIS_2_S04`; conv tree `Act/Conv/2_S04.act` carries all in-engine scene dialog.

## 4. NPCs and assets needed

### Peds (ids as scripted; names per opening-slice `default.ide` table or **INFERRED**)

| Model id | Role | Script global | Notes |
|---|---|---|---|
| 6 | Melvin (quest giver) | `gMelvin` | mission-critical; walk-off at S1; recreated for handover; unique-model suppression swap |
| 85 | sheet-1 guard | `sheet1Guard` | `GN_Bully03` per opening-slice; min-health 20, stationary |
| 102 | sheet-1 bruiser | `sheet1Bruiser` | `GN_Bully01`; performs the sack kick |
| 99 | sheet-2 guard | `sheet2Guard` | `GN_Bully02`; drops sheet 2 on KO |
| 102 | sheet-2 goon | `sheet2Goon1` | no sheet drop |
| 7 | Thad (ally beat) | `gThad` | `NDH2_Thad` (**INFERRED** name); recruited ally for the sheet-2 fight |
| 5 | Fatty (fleeing bystander) | `gFatty` | `NDH1_Fatty` (**INFERRED**) |
| 3 | health nerd | `gHealthNerd` | drops soda can; name **OPEN** (id-level only) |
| 102 | sheet-3 guard | `sheet3Guard` | tease loop; drops sheet 3 |
| 8 | Bucky (teased nerd) | `gBucky` | `NDH3_Bucky` (**INFERRED**) |
| 99 | sheet-4 guard | `sheet4Guard` | fleeing smoker; drops sheet 4 |

### Animation groups (13 loaded)

`2_S04CharSheets`, `NIS_2_S04`, `F_Girls`, `F_Nerds`, `Cheer_Nerd1`, `Hang_Talking`, `NPC_Cheering`, `POI_Smoking`, `POI_Booktease`, `NPC_AggroTaunt`, `LE_Orderly`, `GEN_Social`, `IDLE_GSF` (unload list also names `Hang_Argue`, which is never loaded — script quirk). Action trees: `Act/Anim/Overrides/Mission/2_S04Greeting.act`, `Act/Conv/2_S04.act`, `Act/Gifts/Give2S04.act`; player hit-tree node `Act/HitTree.act` for the sack/recover/KO states.

### Props / pickups / weapons

- Pickup model **497** = character sheet (×4, `PermanentButes`); also inventory item 497 (count zeroed on cleanup).
- Pickup model **502** = soda can health pickup (`HealthBute`), dropped by the health nerd.
- Weapon **349** = marbles (sheet-4 hazard; `AttachMarbles` anim, destroyed when the guard is hit).
- Weapons 309/320/303/392 and models 339 loaded in `OpeningCutscene` but never used in the live path — **INFERRED** dead loads.
- Trash-can prop `PAnimCreate(TRIGGER._2_S04_TRASHCAN)` at sheet 2 — purpose **OPEN**.
- Music: interactive streams `MS_EpicConfrontation{Low,Mid,High}`; pass/fail stingers via `SoundPlayMissionEndMusic`.

### Speech

`SoundPlayScriptedSpeechEvent("M_2_S04", id)` distinct ids used: 3, 6, 7, 8, 10, 12, 14, 16, 18, 19, 21, 23-28, 30, 33, 34, 61, 63, 64, 76, 77, 84 → **26 scripted speech events** (plus 5 conv-tree dialog nodes in the rochambeau scene and anim-carried barks like the bruiser's taunt/laugh). Taunt pools: flee taunts {18}, arrival taunts {8,10,12,14}, marble taunts {6,7}.

## 5. Named world data to extract from `2_S04.DAT` (only DAT loaded: `DATLoad("2_S04.DAT", 2)`)

- POINTLISTs: `_2_S04_MELVIN`, `_2_S04_START`, `_2_S04_SHEET1START`, `_2_S04_SHEET1`, `_2_S04_SHEET1BRUISER`, `_2_S04_SHEET1EXIT`, `_2_S04_SHEET2`, `_2_S04_SHEET2_G1`, `_2_S04_SHEET2_N1`, `_2_S04_SHEET2_N2`, `_2_S04_SHEET2_N4`, `_2_S04_SHEET3`, `_2_S04_SHEET3_1`, `_2_S04_SHEET4`.
- PATHs: `_2_S04_TOLIBRARY`, `_2_S04_PLAYERENTERPATH`, `_2_S04_SBPATH`, `_2_S04_NERD_ESCAPE1`, `_2_S04_NERDESCAPE3`, `_2_S04_NERDRETURN`, `_2_S04_NERD_LOOP`, `_2_S04_FLEE1`, `_2_S04_FLEE2`, `_2_S04_FLEE3`, `_2_S04_FLEE4`; plus shared patrol path `_SGD_PREFECT3` (disabled for the mission). Dead: `_2_S04_NERD_ESCAPE2`.
- TRIGGERs: `_2_S04_SHEET1`, `_2_S04_LIBRARYJUMP`, `_2_S04_TETHERBULLY`, `_2_S04_STARTSHEET2`, `_2_S04_NERD_ESCAPE`, `_2_S04_SHEET2`, `_2_S04_TRASHCAN` (prop), `_2_S04_SHEET3OUTER`, `_2_S04_SHEET3`, `_2_S04_SHEET3CREATE`, `_2_S04_SHEET4`, `_2_S04_SHEET4INNER`, `_2_S04_AUTOSHOPAREA`, `_2_S04_SCHOOLPOP` (population/tether).
- Hardcoded coords (cameras + nav seal): sheet-1 scene cams (160.98,-165.28,9.86)→(161.49,-164.42,9.83), (166.32,-163.24,9.00)→(166.13,-162.26,9.02), (163.12,-159.45,8.84)→(164.06,-159.77,8.96), (164.00,-163.57,9.20)→(164.43,-162.67,9.08), (161.50,-161.17,8.28)→(162.48,-160.97,8.36); nav seal at (164.4,-176.9,7.8); sheet-3 scene cam (240.50,-131.46,7.29)→(241.24,-130.79,7.29).
- Area codes: nothing in the Lua forces an area; the hardcoded coords sit in `tschool` ranges (x 160-241, y -131..-177) → mostly **area 0, INFERRED** pending DAT extraction; Melvin's destination is the library (`_2_S04_TOLIBRARY`), interior code **OPEN** until extraction.

## 6. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| S0 cutscene + setup | `fade`, `letterbox`; sibling `play_cutscene` | — |
| S1 Melvin walk-off | `npc_move_to`/sibling `npc_follow_path` | **npc_despawn_on_path_end** (despawn event at last node; may fold into npc_follow_path callbacks) |
| S2 collection opens | `hint`/`banner`, sibling `blip` | **objective_checklist** (N objective entries ticked off in any order), **collect_counter** (HUD counter with icon/max, increment on pickup event, complete at N) |
| S3a entry variants | `trigger_enter`, sibling `ped_hit_event` | climb-in interrupt is an **anim-state condition** on the player (**ped_action_node** query, sibling name from 1_03) |
| S3a rochambeau | `letterbox`, `camera`, `say`, sibling `pair_anim` | **qte_duel** (scripted paired setup + timed reaction window with win/lose branches and an emergency timeout; real logic was in undecoded trees, so we author it) |
| S3a post-duel guards | sibling `npc_flee`, `npc_attack` | flee-vs-linger behavior switch + **tethered wander** (tether to trigger + wander — may fold into spawn_npc flags) |
| S3b ambush + ally | `trigger_enter`, `spawn_npc` w/ flags, sibling `spawn_enemy` | **recruit_ally** (temporary companion: recruit + auto-engage + stat overrides + dismiss on cleanup) |
| S3b health drop | — | **drop_pickup_at_ped** (arbitrary-time pickup drop at an NPC's position; generalizes sibling `npc_drop_item_on_ko`) |
| S3c tease scene | `camera`, `fade`, sibling `pair_anim` | **bark_dialog_loop** (alternating two-NPC speech loop on a timer until interrupted; extends sibling `bark_loop`) |
| S3d fleeing smoker | sibling `npc_follow_path`, `ped_hit_event`, `bark_loop` | **flee_loop** (waypoint legs + wait-for-player-proximity per leg + max-laps→stand-and-fight), **hazard_drop** (proximity-triggered marble drops, capped per leg) |
| S4 drops/counter | sibling `npc_drop_item_on_ko` | (covered by collect_counter + drop_pickup_at_ped) |
| S5 return gate | `reach_location` (within 3 m of NPC), sibling `blip` | — |
| S6 handover | sibling `social errand flows` (give interaction), `completion_banner` (1_03), `reward` | — |
| Melvin availability swap | — | **unique_ped_suppress** (`PedSetUniqueModelStatus` save/restore around the mission) |
| misc | — | **dismount_vehicle** (F_PlayerDismountBike utility; trivial), player-side **invulnerable toggle** (small; pairs with spawn_npc flags) |

Existing primitives reused as-is: reach_location, trigger_enter/exit, event, timer, flag, all/any, hint, banner, log, teleport, fade, letterbox, say, camera, shake (unused here), reward, spawn_enemy; sibling names reused: npc_follow_path, npc_move_to/face/attack/flee, ped_hit_event, ped_action_node, pair_anim, blip management, npc_drop_item_on_ko (generalized), bark_loop, social errand flows, completion_banner, play_cutscene.

## 7. Open questions

- Rochambeau mechanics: what the player actually does in the 8 s window (input, timing, win rule) lives in `Act/Conv/2_S04.act` / `HitTree` — undecoded. We must author the duel from scratch; only the setup, win/lose consequences and timeouts are known.
- How the mission is offered in the world (start trigger/blip, and the exact interplay between its availability, `SFaction`'s Melvin suppression and the `DebugMissionPassDependant("Chapt1Trans")` hook) — mission table is outside the Lua.
- What `_2_S04_TRASHCAN` prop does at sheet 2.
- Melvin's library destination (interior area code) and whether the handover happens inside or outside the library — pending DAT extraction.
- Whether the baked cutscene replays on retry (no attempt check in this file).
- Money unit assumption (cents) rests on cross-mission comparison, not a header definition.
