# 1_08 — That Bitch: recon notes

Status: 2026-10-22, read-only recon. Confidence tags: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_08.lua` (actual file is 1857 lines; the task brief's line counts for 1_07/1_08 are swapped), `SMissPass.lua` (`F_SetupMissionRequirements`), `main.lua` (~300-340). Functional logic in our own words; no dialogue or objective text reproduced. Script/function/native names are citations only.

Area codes seen: 0 = outdoors (`tschool`), 2 = `ischool` (cafeteria), 13 = gym girls' locker-room interior (**INFERRED** from `gAreaDATFileLoaded[13]` + the locker/toilet props), 35 = the area where the Beatrice scene peds spawn (**OPEN** — not in the opening-slice area list; context places it near the girls' dorm).

## 1. Identification

**1_08 = That Bitch — VERIFIED.** Evidence:

- `LoadAnimationGroup("1_08_MandPuke")`; Mandy (ped model 14) is led to her rigged locker, plays a stink-trap hit reaction (`/Global/HitTree/Standing/Ranged/Bomb/Stink_Trap`), a scripted collapse (`1_08_MandyCollpase`), then runs to a toilet stall and loops an `InteractWithToilet` node — the canonical payoff.
- Beatrice (model 3) receives her stolen notes back (give animation `GiveLabNotes1_08`, item 488) and the mission ends in a make-out pair animation — the canonical reward scene.
- Gary (model 130) gives the mission in an in-engine intro outside the girls' dorm (`PATH._1_08_OUTSIDEGIRLS`, conversation node `/Global/1_08/NIS_1_08_1/ConvInitiate`).
- Fatty (script variable `gFatty`, spawned with model 155) plays dice (`AniDice` prop `_BITCHDICE`, `MG_Craps` group) and supplies the stink bombs; the cafeteria lady (model 58, `TE_Female` group) guards the cake.
- `SMissPass.lua` on pass: `GiveAmmoToPlayer(309, 5)` if the player has none — item 309 = stink bombs (**VERIFIED**: the same item rigs the locker and is handed out by Fatty mid-mission).

## 2. Mission flow

A single `main` loop calls a stage function pointer (`missionStage`) each frame; stages are also advanced by conversation/action-tree callbacks (see Open questions).

| # | Beat (stage function) | Logic (functional) |
|---|---|---|
| B0 | **Setup + intro** (`MissionSetup`, `F_PlayIntroNIS`) | Stealth music streams (`MS_StealthLow/Mid/High`); baked cutscene `1-08` via `PlayCutsceneWithLoad`; `1_08.DAT` loaded. Then, **only if the player carries no stink bombs (309)**, an in-engine walk-and-talk: control off, fixed camera, player auto-walks `PATH._1_08_OUTSIDEGIRLS` with Gary (model 130) waiting; conversation at path node 2; skippable (`WaitSkippable` cancels the conversation). If the player already has stink bombs (retry/shortcut), the intro is skipped and the mission jumps straight to the gym stage (`F_PlayerHasStinkbombs`). |
| B1 | **Meet the dice-playing nerd** (`MissionStageMeetWithFatty`, objective key `1_08_OBJ09`) | Blip on `_BUCKYCUTSCENE`. Fatty exists only while the player is inside `TRIGGER._BUCKYTRIGGER` (leaving deletes him, re-entering respawns him at his dice game). Social override trees are loaded for gift-request/follow/greet (`Mission/1_08WantGift.act`, `1_08Follow.act`, `1_08Greeting.act`); via those trees he demands a gift (required-gift id 22 — the chocolate). The actual call sites that advance the stage live in the undecoded conversation data (**OPEN**). |
| B2 | **Cake run** (conv-driven; `F_UpdateCakeObjective`, `UpdateEventsGetStinkbomb`) | Entering the cafeteria trigger `_1_08_CAFTRIG` (area 2) spawns the chocolate-bar pickup (item 478) at `_CHOCOLATEBAR` and the cafeteria lady (model 58), who patrols `PATH._KITCHENLADY` tethered to `_CAFBACKTRIGGER` with ambient chatter lines. A sneak tutorial (key `1_08_OBJ08`) shows once. If the player enters the kitchen trigger `_CAFBACKTRIGGER`, she confronts him: punishment points are forced to the bust threshold (120), she attacks — a bust (**INFERRED**: mission ends through the game-level bust system, not script). Picking up the cake sets objective `1_08_OBJ07` (return to the nerd) → stage `MissionStageGetTheStinkBomb`. |
| B3 | **Trade for stink bombs** | Two routes, both end at `F_GivePlayerStinkBombs` / `F_PlayerHasStinkbombs` (player receives 7× item 309; objective `1_08_OBJ05` = go to the gym; blip on `_GYMCUTSCENELOCATIONS`): (a) give Fatty the chocolate through the gift social override; (b) **bully him** — a humiliation stimulus (type 49), >40 hp of damage, or a grapple makes him flee and drop a stink-bomb pickup (309) at `_1_08_STINKBOMBS` (`F_MonitorFatty`). Safety net: if the player spends all stink bombs before rigging, objectives reset and the pickup respawns (`F_MonitorStinkBombs`, key `1_08_FATTYAGAIN`). |
| B4 | **Get into the gym locker room** (`MissionStageGymFight`, area 13) | On arrival: objective `1_08_OBJ03`, blip on `_GYMCUTSCENELOCATIONS` slot 1. A girl (model 38, group id "pinky") stands at the mirror; when the player enters `TRIGGER._CHANGEROOM` (or she is attacked) she flees along `PATH._BATHROOMLACKEYS` and summons a prefect (model 50), who patrols `PATH._PREFECTPATH` with look-around pauses at the path ends (5 s on the way back, 50 s outbound) — `T_MonitorPrefect`. If the prefect spots the player inside `_1_08_GYMGIRLWASH`, he attacks (bust attempt). The gym door `_DT_GYM_DOORL` is closed behind the player. |
| B5 | **Break into the locker** (`MissionStageBreakIntoTheLocker`) | Mandy's locker = prop `_LCKRGYMM` at `_LOCKERPOS` (NLockA prop act). The shared lockpick minigame is hooked: starting a lockpick within 1 m of the locker registers `F_PlayerPickedBeatriceLocker` / `F_PlayerFailedToPickBeatriceLocker` as the success/failure callbacks. Success → notes obtained (item 488 given), then the gym finale cutscene fires (`F_SetupFinalGymCutscene`: gym door closes, Mandy spawns at `_GYMCUTSCENELOCATIONS` slot 10 with a speech line, walks `PATH._MANDYFINALPATH`). Objective `1_08_OBJ01` + blip on the girls'-dorm side door `_TSCHOOL_GIRLSDORMSIDEDOOR` → stage `MissionStageDeliverBook`. Lockpick failure just returns (retry the minigame). |
| B5' | **Rig variant** (`MissionStageRigTheLocker`) | Same outcome via planting a stink bomb in the locker (player anim `PlantStinkbomb/...`, `LOCKERBOMB` tutorial). **No Lua line ever assigns this stage** — the rig-vs-pick branch is selected from undecoded conversation/action-tree data or is dead in this build (**OPEN**). The Mandy payoff below only makes sense with the locker rigged, so treat "plant the bomb" as the intended beat. |
| B6 | **Payoff + deliver the notes** (`MissionStageDeliverBook`) | Mandy reaches her locker (path node 1 of `_MANDYFINALPATH`, while player indoors): scripted prop interact, stink-trap reaction, collapse animation, made invulnerable/non-targetable, then runs `PATH._MANDYTOTOILET` (or `_SECONDSTALL` if the player is in `_BathroomRightTrig`) and loops the toilet-interact node. If Mandy instead spots the player inside `_1_08_GYMGIRLWASH` first, she flees and the gym door closes (alternate branch). Once the player is back outdoors (area 0), Beatrice (model 3) spawns at `_BEATRICECUTSCENE` — mission-critical (`F_CriticalPedDied`), blipped, hostile-faction-immune, required-gift set (id 12), player social actions 28/29 disabled against her. |
| B7 | **Success scene** | Player within 3 m of Beatrice → control off, widescreen, mutual face/lock, give-notes animation (`GiveLabNotes1_08`); `missionCondition = "Success"`; then the make-out pair animation on player + Beatrice, completion screen `M_PASS` with key `1_08_UNLOCK`, pass stinger; Beatrice becomes ambient and wanders off with a new required gift (id 2). `MissionSucceed`. |

**Fail conditions**

| Cause | Handling |
|---|---|
| Beatrice is hit (once mission-critical) | `F_CriticalPedDied` → `MissionFail(false, true, "1_08_BEAHIT")` |
| Beatrice dies | `MissionFail(false, true, "1_08_BEAKO")` |
| Anything else setting `missionCondition = "Failure"` | generic `MissionFail(false, true)` |
| Busted by the cafeteria lady or the gym prefect | punishment points forced to 120 → game-level bust (**INFERRED** to end the mission; not handled in the script) |

**Retry/restart:** full restart; carrying stink bombs (309) at start skips the Gary intro *and* the entire Fatty leg, jumping to the gym stage (**VERIFIED**). No mid-mission checkpoints. `F_SetupNISOne`, `F_SetupCafeteriaNIS`, `F_DoFatty*Dialogue` are debug helpers (`gDebugging = true`), not live; `F_SetupCafeteriaIntroCutscene` is an empty function (dead).

**Chains to:** no next mission. On cleanup after success, if `1_07` is complete, `AreaLoadSpecialEntities("Halloween1", true)` — the second of the 1_07/1_08 pair turns on the Halloween dressing (**VERIFIED**; see the 1_07 doc for the full chain). Also sets `shared.g1_08_bGymPop = true` (re-enables gym population) and clears the player's note items (488 → 0).

**Unlocks on pass** (`SMissPass.lua`, mission id `1_08`): 5× item 309 (stink bombs) if the player has none (**VERIFIED**; identity from in-mission use). No weapon, no outfit, no chapter set, no force-completed missions, no collectible toggle.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-A | Before B1 (`1-08`) | baked | fade + letterbox + `say`; or fold into the intro below |
| CS-B | B0 Gary walk-and-talk | in-engine, skippable | npc_walk path + `say` exchange + letterbox + skip support |
| CS-C | Fountain patrol scene (`F_ExecuteStealthCutscene`, TRIGGER `_FOUNTAINCUTSCENE`) | in-engine conversation between two patrolling peds (models 13/15); player invulnerable, punishment points stashed | two NPCs + `say`; **note: its only trigger check lives in `MissionStageGetToGym`, which no Lua line assigns — possibly dead in this build (OPEN)** |
| CS-D | Cafeteria "intro" | function is empty | — (dead) |
| CS-E | Gym finale (`F_SetupFinalGymCutscene` + Mandy collapse/toilet, B5/B6) | in-engine | camera + `say` + npc path + collapse/toilet anims; can compress to one camera cut |
| CS-F | Beatrice hand-off + kiss (B7) | in-engine pair anims | camera + letterbox + give anim + fade; kiss stub-able |

## 4. NPCs and assets needed

Ped model ids (as scripted; no `default.ide` at hand — roles from script variables):

| Role | Model id | Notes |
|---|---|---|
| Beatrice | 3 | final stage; mission-critical; unique-model status forced |
| Mandy | 14 | gym finale; invulnerable once sick; unique status forced |
| Cafeteria lady | 58 | patrol/chase; `TE_Female` group |
| Dice-playing nerd ("Fatty") | 155 | gift-giver; flees/drops bombs if bullied |
| (unique status also forced on) | 5 | never spawned in the live path — likely Fatty's ambient model (**OPEN**) |
| Gary | 130 | intro only |
| Locker-room girl | 38 | flees, summons prefect; group id "pinky" |
| Prefect (gym) | 50 | patrol + bust attempt |
| Patrol pair | 13, 15 | fountain/patrol scenes (`UpdateRussellsLackeys`, `F_SetupFountainPatrol`) — faction/role **OPEN** |

- **Animation groups:** `NPC_AggroTaunt`, `Area_School`, `Px_Sink`, `TE_Female`, `N_Striker_B`, `MG_Craps`, `F_Girls`, `NIS_1_08_1`, `1_08_MandPuke`, `Px_Tlet`. **Action trees:** `Act/Conv/1_08.act`, `Act/Props/AniDice.act`, plus referenced `Act/Anim/GenericSequences.act`, `Act/AI/AI_Reactions.act`, `Act/WProps.act`, `Global/Props/NLockA.act`, `Act/Player.act` (make-out). **Social override trees:** `Mission/1_08WantGift.act`, `Mission/1_08Follow.act`, `Mission/1_08Greeting.act`.
- **Weapons/items:** 309 (stink bombs), 434 (loaded, never clearly used — **OPEN**), 488 (the notes; also `WeaponRequestModel`), 478 (chocolate bar pickup). Gift ids: 22 (Fatty), 12 then 2 (Beatrice) — mapping **OPEN**.
- **Props:** dice PAnim `_BITCHDICE`; locker `_LCKRGYMM` (prop flags 11/19/22 toggled; NLockA states `Locked1_08`, `Unlocked/Default`); gym door `_DT_GYM_DOORL`; toilet/sink interact props (groups `Px_Tlet`, `Px_Sink`).
- **Speech:** 6 `SoundPlayScriptedSpeechEvent` + 5 `PedStartConversation` + 5 ambient/queued call sites; the Fatty/Gary/patrol conversations carry most of the mission's talking. Music: `MS_Stealth Low/Mid/High`; pass/fail stingers.
- **Special entities:** conditional `AreaLoadSpecialEntities("Halloween1")` on cleanup.
- **DAT:** `1_08.DAT`.

**Named world data to extract from `1_08.dat`:**
POINTLIST: `_1_08_STARTPOINT`, `_BEATRICECUTSCENE`, `_BUCKYCUTSCENE`, `_PLAYERWITHFATTY`, `_CHOCOLATEBAR`, `_1_08_STINKBOMBS`, `_LOCKERPOS`, `_GYMCUTSCENELOCATIONS`, `_EDNAPOINT`, `_CAFRESTARTPOINT`, `_TSCHOOL_GIRLSDORMSIDEDOOR`, `_LACKEYSETA`, `_LACKEYSETB`, `_LACKEYSETC`, `_LACKEYSETD`, `_LACKEYSETE`, `_LACKEYPATROL`, `_LACKEYFINALA`, `_LACKEYFINALB`, `_LACKEYFINALC`, `_LACKEYFINALD`.
PATH: `_1_08_OUTSIDEGIRLS`, `_KITCHENLADY`, `_PREFECTPATH`, `_BATHROOMLACKEYS`, `_MANDYFINALPATH`, `_MANDYTOTOILET`, `_SECONDSTALL`, `_LACKEYFINALA`–`_LACKEYFINALD`, `_LACKEYPATROL`.
TRIGGER: `_BUCKYTRIGGER`, `_1_08_CAFTRIG`, `_CAFBACKTRIGGER`, `_CHANGEROOM`, `_BathroomExit`, `_BathroomRightTrig`, `_1_08_GYMGIRLWASH`, `_FOUNTAINCUTSCENE`, `_DT_GYM_DOORL`, `_LCKRGYMM`, `_BITCHDICE`, `_EDNAPOINT`, `_LACKEYSETA`–`_LACKEYSETE`.

## 5. Framework gaps

Covered by existing primitives: `trigger_enter`/`reach_location` (all the stage gates), `timer` (patrol pauses), `flag`/`set_flag`/`clear_flag` (stage state), `fade`/`letterbox`/`say`/`camera` (NIS work), `teleport`, `reward` (needs ammo support), `hint`/`banner`, `spawn_enemy` (with per-spawn options).

NEW (1_08):

| Primitive | Used for |
|---|---|
| `minigame` with success/failure callbacks | the shared lockpick on `_LCKRGYMM` (hooked via `shared.gLockpickSuccessFunction`/`FailureFunction`) |
| `conversation` (start conversation, wait-while-in-conversation, skippable, tree-driven stage callbacks) | Gary intro, Fatty gift negotiation, patrol dialogue — **this mission's stage machine is partly conversation-driven** |
| `give_gift` / required-gift social interaction | chocolate → Fatty; Beatrice's gift ids |
| `item_count` condition + `give_item` / `give_ammo` actions | stink bombs gate the whole start; notes (488); cake (478) |
| `pickup_create` / `pickup_delete` / `pickup_picked_up` event (+ respawn rule) | cake, dropped stink bombs |
| `stealth_spot` (ped-can-see-ped → alert/chase) | cafeteria lady, prefect, Mandy |
| `busted` integration (punishment points, set-to-threshold) | kitchen catch, prefect catch |
| `npc_scoped_spawn` (exists only while player inside a trigger) | Fatty's dice game; locker-room girl; Beatrice per-area |
| `npc_patrol` (path + per-node callbacks + timed pauses) | cafeteria lady, prefect, patrol pair |
| `npc_flee` / `npc_summon` (fleeing ped spawns a hunter) | locker-room girl → prefect |
| `pair_anim` | give-notes, make-out, (humiliation stimulus detection for the bully route) |
| `attacked_by` event (who-hit-me-last + damage threshold) | Fatty's flee conditions; Beatrice fail |
| `prop_flag_set` + prop anim states | locker trap/lock flags, dice table |
| `area_visibility` condition (gate logic on `AreaGetVisible`) | spawn/despawn rules for areas 0/2/13/35 |
| `stage_function` state machine (per-stage tick fn) | the whole mission shape — our JSON framework needs per-stage `on_tick` |
| `cutscene_skip` (skippable conversations/NIS + skip-intro-on-retry item check) | B0, retry shortcut |

## 6. Open questions

1. **Conversation-tree wiring.** `F_SetStageGetCake`, `F_SetMissionStageGivePlayerStinkers`, `F_BeatriceReceivedGift` (which references a `MissionBeatriceReceivedNotes` stage that does not exist in this file), and the condition callbacks `F_FattyWantsGift` / `F_FattyWantsCake` / `F_PlayerRiggedLocker` have no Lua callers — they are invoked from `Act/Conv/1_08.act` / the social-override acts, which are undecoded. The exact Fatty-negotiation branch order is therefore **OPEN**.
2. **Dead or dormant stages:** `MissionStageGetCake`, `MissionStageGetToGym` (holder of the only `_FOUNTAINCUTSCENE` check) and `MissionStageRigTheLocker` are never assigned in Lua; `F_UpdateCakeObjective` (holder of the cake pickup creation) is never called in Lua. Either conversation trees set them, or this build ships the pick-the-lock path only. Decide our JSON flow from the intended beat (rig the locker), not from this ambiguity.
3. Area codes 13 (gym locker room — **INFERRED**) and 35 (**OPEN**) need confirmation against the area table when we extract `1_08.dat`.
4. Gift ids 22/12/2, item/weapon 434, ped models 5/13/15, and which prefect model 50 is — all **OPEN** pending `default.ide`/text lookup.
5. Bust behaviour (120 punishment points) presumably routes through the global bust system to a mission fail; not handled in the script — confirm against the punishment system notes.
6. The `gLackey` table indices 1-12/20/21/30-33/35/36/40 reference many lackey sets; several (`_LACKEYSET*`, `_LACKEYFINAL*`) are only ever created/deleted, suggesting trimmed crowd content — extract the points anyway, but expect some to be unused.
