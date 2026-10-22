# 1_02B — This Is Your School: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_02B.lua` (1662 lines) + `SMissPass.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_02B = "This Is Your School"** — **VERIFIED** by the loaded animation group `1_02BYourSchool` and by content: Gary walks the player through the main school building — apologize to Russell (social system tutorial), pick a locker (lockpick minigame tutorial), hide from a prefect in a trash bin (punishment/trouble tutorial), cheer up Eunice (greet + gift tutorial, via Constantine's chocolates), then follow Gary to the cafeteria. Matches the real game's mission that follows the 1_02/1_02A opening.

**Role in the opening chain:** 1_02 (briefing + tutorial fight) → 1_02A (dorm, uniform) → **1_02B** (school systems tour, all inside `ischool`, area 2) → **1_02C** (first class connector, see `1_02C-first-class.md`). The handoff is seamless: on pass, `MissionCleanup` sets `shared.b102finished = true`, and 1_02C's `MissionSetup` detects that flag to skip its own fade/ped-clear (**VERIFIED** in both files). This mission is absent from the canonical list we were given (the list starts at 1_03) — expected, it is part of the scripted first morning.

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → `F_Stage1 → F_Stage2 → F_Stage3` (flag-gated stage chain via `bGoToStage2/3`), with threads `T_CreateGary`, `T_MonitorPlayerLocation`, `T_MonitorGarysHealth`, `T_LockerLoop`, `T_ConSocLoop`, `T_RemindForObj`. Debug stage-jumps exist (`F_StartAtStage2/3`, gated behind `bDebugFlag`).

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup (no baked intro)** | No `PlayCutsceneWithLoad` — the mission opens in-engine, continuing from 1_02A. `1_02B.DAT` loaded; arrest point overridden to the school interior point `_1_02B_OBJSCHOOL` (busted ⇒ wake inside school); bathroom POI disabled; punishment points zeroed; interactive music `MS_Misbehaving*` armed; six ped models made unique (70, 74, 99, 4, 69, 52) so ambient dupes don't appear. |
| S1 | **Gary + Russell + Algie intro** | Player transitioned to area 2 (`_1_02B_OBJSCHOOL`). Russell (75) spawned stationary at `_1_02B_SPAWNBULLY01`, mission-critical; Gary (130) spawned at `_1_02B_SPAWNGARYNEW`, mission-critical, infinite sprint, gets a blip; Algie (4) spawned at `_1_02B_SPAWNGARYVICTIM` and Gary plays a shove on him (scene dressing — Gary being Gary). The locker prop `_NLOCK01A` is put in `Locked` state; 26 locker props registered. Yellow-arrow objective tutorial (`YELAROW1`). Thread `T_CreateGary`: after ~3 s or when the player enters `_1_02B_RECRUITGARY`, Algie flees, Gary speaks (`M_1_02B` 117), objective **MOBJ_01** added (follow Gary, **paraphrase**), Gary walks to `_1_02B_GARYLOCKER`. |
| S2 | **Russell confrontation NIS** (`F_NewRussell`) | Entering `TRIGGER._1_02B_LOCKERMESSAGE` or `_LOCKERMESSAGE02`: control off, widescreen, slingshot-ish weapon 381 (**OPEN** id) destroyed if held, money topped up to 200¢ if below, player/Gary teleported to variant points (`MOVEPLAYERRUSSELL1/2`, `MOVEGARYRUSSELL1/2`), Gary line (118), Russell walks up to the player, mutual lock, fade. Ambient peds cleared, population suppressed. |
| S3 | **Apology social tutorial** | Button input locked except the social key; tutorial cards `TUT_RUSS01A/02C`; player must perform the social action on Russell (registered callback), then a second round (`TUT_RUSS03A/03C`) where Russell demands a gift (gift id 22 = money, **INFERRED**) — the game takes 200¢ from the player; Russell plays a humiliation-then-accept-apology animation chain (`SocialHumiliateAttack` → `SocialAcceptApology/Bully/GiveUp`); `TUT_RUSS04A`; input unlocked. Russell is released to ambient and flees (`F_RussellLeave`), Gary line (119), objective **MOBJ_03** added with a blip on `_1_02B_OBJLOCKER` (the locker). |
| S4 | **Lockpick the locker** | At `_1_02B_OBJLOCKER` the lockpick minigame is offered; `shared.gLockpickSuccessFunction = F_UnlockBalloons` is the success hook; `T_LockerLoop` shows the `RUSLOCKX` tutorial while the minigame is active. Gary comments (37) when the picking loop starts. On success: locker prop switches to `Unlocked`, **player receives clothing `S_Bhat1`** (hat), `PlayerClearRewardStore`, objective **MOBJ_03** completes. |
| S5 | **Prefect bust NIS + hide-in-bin tutorial** (`F_SetupLockerPrefect`) | Prefect Karl (52) spawned at `_1_02B_SPAWNKARL`; Gary is moved off; scripted camera beats: player victory anim, Gary line (`M_1_11` 21 — cross-mission id, as scripted), **punishment points set to 200 with min 199** (forced trouble), punishment tutorials `TUT_PUN01/04/03`; Karl follows `PATH._1_02B_KARLPATH`, barks a warning (83), Gary flees to `_1_02B_GARYFLEELOCKER`, Karl focuses Gary... then the player is auto-walked along `PATH._1_02B_PLAYERTOTRASH`, camera FOV beats, fade; player placed at `_1_02B_MOVEPLAYERLOCKER` facing `_1_02B_BLIPGARBAGE`; objective **GARB_01** (hide) + `InTheBin` tutorial; Karl then attacks (bust attempt) the player. |
| S6 | **Cooldown** | While the player is in a stealth prop (trash bin): Karl is released/flee-made-ambient (`F_ReleasePrefect`), patrol paths re-enabled, and punishment allowed to decay (`PlayerSetPunishmentPoints(200)` once, then it drains). When points hit 0: objective **GARB_01** completes, Gary re-recruited as ally (mission-critical again, health bar on), objective **MOBJ_01** re-added (follow Gary), Gary line (81) when within 5 m → stage 2. |
| S7 | **Eunice NIS** (`F_cutEunice`, stage 2) | Entering `TRIGGER._1_02B_SOCIALMESSAGE`: peds cleared, population off; Eunice (74) spawned at `_1_02B_SPAWNEUNICE` playing a cry loop along `PATH._1_02B_PATHEUNICE`, mission-critical; widescreen; player moved to `_1_02B_MOVEPLAYEREUNICE`; Gary teleported to `_1_02B_GARYEUNICE`, speaks (120), walks `PATH._1_02B_GARYTOEUNICE`, player auto-follows `PATH._1_02B_PLAYERTOEUNICE`; fade, camera resets; objective **MOBJ_04** (talk to Eunice, **paraphrase**); input locked to the social keys; player lock-targets Eunice. |
| S8 | **Greet tutorial → gift objective** | Tutorials `TUT_EUNSOC01/03`; greeting Eunice (callback `F_EuniceGreeting`, line 95) stops her crying; camera FOV 30 beat; Eunice's required gift set (gift id 1, **OPEN**) and she waits; objective **MOBJ_04A** added: get the chocolates. Gary re-recruited; `1_02B_TUT09` card; `T_ConSocLoop` shows the `CONSTANTX` social tutorial inside `TRIGGER._1_02B_BATHROOM`. |
| S9 | **Constantine and the chocolates** | Constantine (70, the Greekboy — "Constantinos") spawned at `_1_02B_SPAWNCONSTANTINE` with blip, tethered to `_1_02B_CONSTTETHER`, eating/idle anim, hostile-neutral attitude. Three scripted routes to the chocolates (item **478**, the gift box — shop item at the grocery store per `SStores.lua`, **VERIFIED** as the chocolates by context): (a) **pay him** — he demands gift 22 (money), callback `F_ConstantineReceivedMoney` (line 99) → `F_GiveCardToPlayer` gives 478; (b) **humiliate him** — player stimulus type 49 on him → `F_HumiliatedConstantine` (line 98), he drops 478 and flees along `PATH._1_02B_BULLYFLEE`; (c) **beat him** — any health loss makes him attack the player; on his KO, 478 spawns as a pickup on the body (`F_SpawnCard`). Objectives: 04A → (04B pickup if dropped) → **04C** (return to Eunice) when the player has 478. Gary waits at `_1_02B_GARYWAITBATHROOM` while the player is inside the bathroom trigger, follows otherwise; Gary lines 124/125/126 bracket the exchange. |
| S10 | **Return the chocolates NIS** (`F_NIS_ReturnChocolates`) | Player near Eunice with 478: widescreen, control off, player give-chocolates anim (`GiveChocolateBox`), Eunice responds with the kiss/make-out dismiss chain, Gary's closing quip (125), item 478 consumed, Eunice released to ambient wander → stage 3. |
| S11 | **Follow Gary to the cafeteria** | Gary line (35), objective **MOBJ_02** (follow Gary, blip), Gary walks to `_1_02B_OBJCAFE`. |
| S12 | **Baked cutscene "1-02D" + pass** | Player entering `TRIGGER._1_02B_OBJCAFE`: control off, fade, interactive music off, player moved to `_1_02B_EXITEDCAFE`, `PlayCutsceneWithLoad("1-02D")` (the cafeteria cliques scene, **INFERRED** content), fade back, `bMissionPassed`. Then `F_OutroCuts`: school doors 10029/10030 closed, prefect Edward (51) spawned at `_1_02B_OUTROPREFECT`, widescreen warning beat (line 169 + `PrefectEdwardWarn` anim) — the "get to class" warning; Gary runs off to `_1_02B_OUTROGARYFLEE` (marked for deletion in cleanup); `MissionSucceed(true, false, false)`. Cleanup sets `shared.b102finished = true` → **chains straight into 1_02C**. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Eunice KO'd (`F_MissionCriticalEunice`) | mission-critical callback | `1_02B_FAIL_01` |
| Gary KO'd (`F_MissionCriticalGary`) | mission-critical callback | `1_02B_FAIL_02` |
| Russell KO'd (`F_MissionCriticalRussell`) | mission-critical callback | `1_02B_FAIL_03` |
| (Russell's goon — callback defined, no ped ever registered it) | dead code | `1_02B_FAIL_04` |
| Player leaves the school (`AreaGetVisible() ~= 2`) | `T_MonitorPlayerLocation` thread | `1_02B_FAIL_05` |

No attempt-count cutscene skipping needed (no baked intro). No checkpoints: retry replays from S0. On fail, cleanup removes the hat if it was granted (`ClothingRemovePlayer("S_Bhat1")`), re-locks all lockers to random states, deletes the chocolates, restores POI/population/patrol paths, restarts the PA. Also of note: arrest inside the mission sends the player to the overridden in-school arrest point rather than failing.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | S2 Russell confrontation | in-engine | `letterbox` + fixed `camera` + teleports + Russell approach; keep — it is our primitives plus `say` |
| CS-2 | S5 prefect bust | in-engine, ~15 s of camera/FOV beats | trim to: Karl appears (`spawn_enemy`-adjacent), 2 `say` beats, auto-`player_follow_path` to the bin, `fade` |
| CS-3 | S7/S8 Eunice intro + greet | in-engine | `letterbox` + `camera` on Eunice + follow-paths; greet tutorial as our `hint` + input gate |
| CS-4 | S10 chocolates handoff | in-engine pair anims | one `interact` → give anim + `say` beat; kiss optional |
| CS-5 | S12 cafeteria | **baked** (`1-02D`) | original 10-20 s clique-overview sequence (letterbox + VO + subtitles) or a title card; out of slice scope |
| CS-6 | S12 prefect outro | in-engine | `letterbox` + one warning `say` beat, Gary runs off, `fade` into 1_02C |

Cutscene/NIS anim groups: `NIS_1_02B`, `1_02BYourSchool`, plus social/kiss sets `KISSF`, `SGEN_I`, `SNERD_I`, `SNERD_S`, `SBULL_X`, `G_Johnny`, `3_04WrongPtTown` (the last two are odd loads — **INFERRED** shared social anims).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`; names cross-checked with the opening-slice doc where noted)

| Model id | Role | Notes |
|---|---|---|
| 130 | Gary (guide/ally) | `Nemesis_Gary` per opening doc; mission-critical, ally recruit/dismiss cycles, health bar shown in stages 2-3 |
| 75 | Russell | `DOlead_Russell`; stationary blocker for the apology tutorial |
| 74 | Eunice | `GN_Fatgirl`; crying → greeted → gift recipient |
| 70 | Constantine ("Constantinos") | `GN_Greekboy`; chocolate holder, 3 resolution routes |
| 4 | Algie | opening-scene victim, flees, ambient |
| 52 | Prefect Karl | bust NIS + bin-hiding chase |
| 51 | Prefect Edward | outro warning only (`PFH2_Edward` per opening doc) |
| 99 | bully | unique-ified and model loaded but never spawned in the live path (**INFERRED** dead load) |
| 69 | Pedro | unique-ified, never spawned (**INFERRED** dead load) |
| 139 | loaded in `LoadPedModels`, never spawned (**OPEN**) |

### Animation groups / action trees

`LoadAnimationGroup`: `1_02BYourSchool`, `SGEN_I`, `3_04WrongPtTown`, `G_Johnny`, `SNERD_I`, `SNERD_S`, `KISSF`, `NIS_1_02B`, `SBULL_X`.
`LoadActionTree`: `Act/Conv/1_02B.act` (all mission conv nodes), `Act/Props/NLockA.act` (locker), social overrides `Mission/1_02B{WantGift,Follow,Flee,Greeting}.act`, `Mission/1_02BGiveChocolates.act`; shared `Act/Anim/Ambient.act` (apology chain), `Act/Player.act` (gift give).

### Props / pickups / weapons

- Locker props: `_NLOCK01A` (the tutorial locker) + 26 registered lockers (`_NLOCK01A01.._NLOCK02B10`) with lock-state nodes and prop flags — the world locker system.
- Item **478 = box of chocolates** (VERIFIED by context + `SStores.lua` grocery listing); pickup variant `PermanentMission`.
- Gift ids: 22 (money, **INFERRED**), 1 (**OPEN**).
- `WeaponRequestModel(341)` (**OPEN**), `LoadWeaponModels({431, 398})` (**OPEN**), weapon 381 destroyed before NIS scenes (**OPEN** id; likely a carried prop/weapon the NIS doesn't want visible).
- `LoadVehicleModels({282})` — never used in the live path (**OPEN**, likely dead load).
- Clothing: `S_Bhat1` hat granted at S4.
- School door props 10029/10030 at (-628, -327) closed for the outro.

### Speech

`M_1_02B` scripted ids: 35, 37, 76, 81, 83, 95, 97, 98, 99, 101, 104, 117, 118, 119, 120, 121, 124, 125, 126, 169 — **20 distinct lines**; plus `M_1_11` id 21 (cross-mission reuse, as scripted) and one ambient prefect bark (`WARNING_COMING_TO_CATCH`). Music: `MS_Misbehaving*` interactive streams.

### Special entities

None via `AreaLoadSpecialEntities`. Mission-scoped systems touched: POI groups off, bathroom POI flag, patrol path `_HALLSPATROL_1A` disabled until the locker beat, population override (clear/suppress vs revert), arrest-point override.

### Named world data to extract from `1_02B.dat`

- POINTLISTs: `_1_02B_OBJSCHOOL`, `_1_02B_SPAWNGARYNEW`, `_1_02B_SPAWNBULLY01`, `_1_02B_SPAWNGARYVICTIM`, `_1_02B_GARYLOCKER`, `_1_02B_MOVEPLAYERRUSSELL1/2`, `_1_02B_MOVEGARYRUSSELL1/2`, `_1_02B_OBJLOCKER`, `_1_02B_SPAWNKARL`, `_1_02B_GARYFLEELOCKER`, `_1_02B_MOVEPLAYERLOCKER`, `_1_02B_BLIPGARBAGE`, `_1_02B_SPAWNEUNICE`, `_1_02B_GARYEUNICE`, `_1_02B_MOVEPLAYEREUNICE`, `_1_02B_SPAWNCONSTANTINE`, `_1_02B_SPAWNCHOCOLATES`, `_1_02B_GARYWAITBATHROOM`, `_1_02B_OBJCAFE`, `_1_02B_EXITEDCAFE`, `_1_02B_OUTROPREFECT`, `_1_02B_OUTROGARY`, `_1_02B_OUTROGARYFLEE`.
- PATHs: `_1_02B_KARLPATH`, `_1_02B_PLAYERTOTRASH`, `_1_02B_PATHEUNICE`, `_1_02B_GARYTOEUNICE`, `_1_02B_PLAYERTOEUNICE`, `_1_02B_BULLYFLEE`, `_HALLSPATROL_1A` (shared patrol).
- TRIGGERs: `_1_02B_LOCKERMESSAGE`, `_1_02B_LOCKERMESSAGE02`, `_1_02B_RECRUITGARY`, `_1_02B_OBJLOCKER`, `_1_02B_SOCIALMESSAGE`, `_1_02B_BATHROOM`, `_1_02B_CONSTTETHER`, `_1_02B_OBJCAFE`, `_1_02B_WARN01..04`, locker props `_NLOCK01A` + `_NLOCK01A01`, `_NLOCK01A02`, `_NLOCK01B`, `_NLOCK01B01..08`, `_NLOCK02A`, `_NLOCK02A01..03`, `_NLOCK02B`, `_NLOCK02B01..10`.
- Hardcoded camera coords (all area 2): Russell NIS (-604.7, -302.2) / (-605.8, -311.3); Karl NIS (-609.1, -321.8), (-608.2, -295.1), (-601.7, -297.4); Eunice NIS (-668.8, -318.8), (-670.9, -317.1), (-671.6, -319.3); outro (-640.1, -295.2).
- Area codes seen: **2 only** (leaving area 2 fails the mission).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| S1/S6/S11 follow Gary | `spawn_enemy` (no) | **escort_follow**: ally recruit/dismiss, follow-player vs wait-at-point, ally health bar, distance-catchup bark |
| S3/S8 social tutorials | `hint` | **social_interact**: per-NPC required social action/gift, button-gated input lock, social callback event; **money_ensure/money_take** actions |
| S4 lockpick | — | **minigame_launch(id)** + condition **minigame_completed(id)** (shared success hook); **prop_set_state** (locker locked/unlocked); **give_clothing** + remove-on-fail |
| S5/S6 punishment + hiding | — | **heat system**: set/min punishment points, condition **heat_zero**, **stealth_prop_entered** condition, prefect bust-chase AI, arrest-point override |
| S9 three-route objective | `event` | **npc_humiliated_event** (stimulus on target), **npc_ko** drops (**npc_drop_item_on_ko**), gift-received callback, **pickup_spawner** (at point or corpse), condition **player_has_item** |
| S7/S10 NIS handoffs | `camera`, `letterbox`, `say`, `fade` | **player_follow_path** (control-off auto-walk), pair-anim play (give/kiss) — same gap as 1_G1/1_S01 |
| fails | `event`, `flag` | **mission_critical_npc** (KO/hit → fail with key), **leave_area_fail** (area-code watch thread) |
| world control | — | **population_override** (suppress/revert), **poi_enable**, **patrol_path_enable**, **door_lock_set** |

## 6. Open questions

- Lockpick minigame internals (`MinigameIsReady`, `Act/Props/NLockA.act` node semantics, prop flags 7/11/19/22) — we treat it as a black-box minigame with a success callback.
- Social-system id semantics (28/29/32/35/23 = which actions, gift ids 1/22) — only the money id is inferred; mapping lives outside the Lua.
- Punishment-point decay rate and bust behavior (prefect catch → arrest override flow) — engine-side.
- Whether the `bAllowPunishmentToDrop` / min-199 logic has edge cases (player leaves the bin early) — behaviorally it reads as "points can't drain until you hide", exact drain curve **OPEN**.
- Item/weapon ids 341, 398, 431, 381 and vehicle 282 (all unused-or-undeclared in the live path).
- `T_MonitorGarysHealth` shows a `CLIMBING` tutorial if Gary drops below 75 % health — placeholder-looking reuse; real intent **OPEN**.
- How 1_02B is launched from 1_02A's pass (mission table outside the Lua) — the reverse link (1_02B → 1_02C via `shared.b102finished`) is verified in-script.
