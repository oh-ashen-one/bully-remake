# 3_R09 — Clique challenges (Nerd/Greaser/Prep/Jock/Dropout): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_R09.lua` (2026 lines). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_R09 = the clique challenge series** — the five "save-house" challenges ([Nerd/Prep/Greaser/Jock challenges confirmed by name](https://www.ign.com/articles/2006/11/04/bully-walkthrough-741867); IGN/GameFAQs guides tie each to unlocking a save house). One parametric engine, 13 parameter sets chosen by `F_SetupVendetta(param)` (the param arrives from the launcher; `gMissionWait` blocks `main()` until setup writes it):

| Param | Challenge | DAT | Area | Shape | Reward |
|---|---|---|---|---|---|
| 0 | **Nerd** (comic shop) | `3_R09_Nerds.DAT` | 30 | shop NIS → ConSumo arcade sub-mission → bottle-rocket handover | $5.00 + nerd save (`shared.nerdsSave`) + weapon 307 + 10 ammo |
| 1 | Greaser part 1 | `3_R09_Greasers.DAT` | 61 | 4 enemies + 1 prep ally street brawl | $10.00 |
| 2 | Greaser part 2 | same | 61 | 8 enemies w/ POI dressing (wall-smokers, weapons 376/323) | $15.00 |
| 3 | **Greaser part 3 (hideout)** | same | 61 | 3 named greasers NIS → kill them → spawner siege → takeover | $16.00 + `greasersSave` |
| 4 | Prep part 1 | `3_R09_Preppies.DAT` | 60 | 4 enemies + 1 greaser ally | $20.00 |
| 5 | Prep part 2 | same | 60 | 9 enemies (catch-playing pairs, dog at index 8!, weapons 318/320/335) | $22.00 |
| 6 | **Prep part 3 (boxing club)** | same | 60→27 | poster NIS → 3 boxing wins (`2_R11_Random` ×3) → club celebration | $25.00 + `prepsSave` + outfit `Boxing NG` |
| 7 | Jock part 1 | `3_R09_Jocks.DAT` | 59 | 5 enemies + 1 nerd ally | $25.00 |
| 8 | Jock part 2 | same | 59 | 10 enemies (jock sport models, weapons 300/331, two on a train path) | $26.00 |
| 9 | **Jock part 3 (gym)** | same | 13 | Casey NIS → win `C_Dodgeball_5` → unlock tour | $27.00 + `jocksSave` |
| 10 | Dropout part 1 | `3_R09_Dropouts.DAT` | 57 | 7 enemies + 2 jock allies, 2 parked cars (293) | $30.00 |
| 11 | Dropout part 2 | same | 57 | 12 enemies (POI dressing, weapons 311/`_W_GARBBIN`) | $31.00 |
| 12 | **Dropout part 3 (hideout)** | same | 57 | 3 named dropouts NIS → spawner siege → takeover | $35.00 + `dropoutsSave` |

The engine (in-file) has three combat templates plus two sub-mission templates:

- **`F_Part01S1`** (params 1, 4, 7, 10): blip → enter `_3_R09_AREAT01` → population override + N enemies (random models from the faction pool) + M allies (recruited, auto-engage, double HP) → all required enemies dead = pass. Leaving the area fails.
- **`F_Part02S1`** (params 2, 5, 8, 11): same shape at `_3_R09_AREAT02`, but enemies get **POI set dressing** (`gPOIList`, `gFunctions`: `PlayCatch` pairs, wall-smoking action nodes, path walkers) and optional weapons; a **stealth-see branch** (`PedSetStealthBehavior(1, CbSeenPlayer)`: spotted peds either snap out of the smoking idle or attack). One "dog point" index spawns model 141 (dog) as a non-required enemy (prep part 2, VERIFIED).
- **`F_Part03S1` + `F_Part03S2`** (params 3, 12; jocks/prep variants differ, below): enter the faction's hideout area → KO point overridden to `_3_R09_BLIPP3` (`MissionOverrideKOPoint`) → 3 named enemies at `_3_R09_FIRST` play a sit-down cutscene (`F_P03Cut<Greasers|DOs>`) → they attack; all 3 dead → **`AreaAddMissionSpawner`** siege: quota `gMaxKills` 3, simultaneous 3, spawn cadence 1500 ms, two spawn locations (`_3_R09_SPAWN01/02` + triggers), per-spawn callback adds blips + trash-talk throttle + every 3rd spawn gets stat overrides (0→362, 1→100); spawner exhausts → takeover cinematic → save unlocked.
- **Nerd (0)**: comic-shop clerk NIS (`shared.vendettaClerk`, waits on `shared.finishedLoadingShop` from the shop area script) → basement lock-in → 3 nerd NIS (186 Fatty, 4 Algie, 8 Bucky — names **INFERRED** from model id knowledge of ch1 docs) → `shared.NerdVendettaRunning` + vendetta nerd globals handed to the world → arcade-machine objective: beat **ConSumo** (`shared.ConSumoFinished` > 0, waited while `TrainASumo` is not active) → bottle-rocket-launcher handover (weapon **307** + 10 ammo, `LoadBrocket` anim) → `nerdsSave` + `shared.unlockedClothing`.
- **Prep P3 (6)**: poster-viewing NIS (persistent `BX_PosterHI` + `GeometryInstance` swap of `BX_PosterLO`) → 3 consecutive `2_R11_Random` boxing wins (`shared.BoxingSuccess` poll; a loss fails with `3_R09_Tournament`) → boxing outfit swap, club celebration NIS with 5 cheap boxers (133/118/117/119/36) → unlock banner chain → `prepsSave` + `Boxing NG` outfit granted, previous clothing restored.
- **Jock P3 (9)**: gym NIS with ball-playing jocks (`PedPlayCatch`, weapon 331 dodgeball) → **`ForceStartMission("C_Dodgeball_5")`** and wait on `shared.DodgeballSuccess` (0 = fail `3_R09_Dodgeball`) → unlock NIS with 204/17/207 → two-stage completion banner + camera-path tour of the unlocked room (`_3_R09_UNLOCKPATH` @2.2) → `jocksSave`.

### Shared beats, fail states, pass side-effects

- **Fail**: player death → `F_FailIfDead` plays a loser taunt scene (an end-ped, model from `gEndPed` 29/43 or 0, shoves the player at `_3_R09_ENDSEQUENCE` under a fail camera path) then `M_FAIL_DEAD`; leaving the mission area → `3_R09_Left`; nerd-specific: hitting the comic clerk (`PedIsHit` watch) or aggressing in-store (`shared.playerAggressedInStore` / `gAggressed` via mission-critical nerds) → `3_R09_Nfail`/`3_R09_Sfail`; sub-mission losses as above.
- **Pass**: `MissionSucceed` (+ completion banner variants), per-param money (500-3500 cents), save unlocks (`shared.<clique>Save` + `shared.unlockedClothing`), cleanup re-locks doors, restores attitudes/unique-model statuses, clears all `*VendettaRunning` shared flags.
- Punishment disabled + game clock paused for the whole mission (VERIFIED in `MissionSetup`/`MissionCleanup`).

## 2. Mission flow (engine summary)

`MissionSetup()` (punish off, clock paused, `shared.vendettaRunning = true`, wait flag) → launcher calls `F_SetupVendetta(param)` → `main()` loads the per-faction DAT, transitions to `gInitialArea` at `_3_R09_PLAYER`, runs `gSetupFunction()` then pumps `gMainFunction()` per frame until `gMissionRunning` clears. `MissionCleanup()` is the big shared reset (doors, POI, attitudes, unique models, shared flags, KO-point/aggro state).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | nerd shop / basement | in-engine, skippable (`F_PlaySpeechAndWait` skip flag) | `camera` shots + `speech_sequence` |
| CS-2 | P3 sit-down scenes (greaser/prep/jock/dropout) | in-engine, 4-7 lines each, hardcoded cams | `camera` ×2-3 + `speech_sequence` |
| CS-3 | sub-mission gates | engine mission boundary | `minigame_launch` (ConSumo/dodgeball stubs) / boxing = own mission |
| CS-4 | takeover/unlock tours | in-engine banner + camera path (`_3_R09_UNLOCKPATH`/`_3_R09_BookPath`) | `chapter_title_card`-style held banner + `camera` path |
| CS-5 | death taunt | in-engine fail scene | `fade` + shove `pair_anim` + fail banner |

Anim groups per part: `NIS_3_R09_N/G/P/J/D`, `Hang_Talking`, `Cheer_Nerd3`, `Cheer_Cool1/2`, `IDLE_DOUT_B/D`, `WeaponUnlock`, `POI_Smoking`. Action tree `Act/Conv/3_R09.act` (all cutscene nodes + the `Empty` reset node), `Act/Anim/GenericSequences.act` (wall smoking), `Act/AI/AI_POI.act`, `Act/Player.act` (shove).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Models | Role |
|---|---|
| 5, 4, 7, 9, 8, 11 | nerd enemy pool |
| 24, 29, 28 | greaser enemy pool |
| 32, 34, 40 | prep enemy pool |
| 13, 15, 231 | jock enemy pool |
| 109, 111, 16, 17, 18 | jock sport pool (P2) |
| 43, 45, 46 | dropout enemy pool |
| 186, 4, 8 | nerd basement trio (named, VERIFIED ids) |
| 26, 22, 27 / 30, 31, 35 / 18, 17, 16 / 44, 41, 42 | P3 cutscene trios per faction |
| 204, 17, 207 | jock unlock-scene trio |
| 133, 118, 117, 119, 36 | boxing-club celebration boxers (cheap) |
| 141 | dog (prep P2 index 8, non-required) |
| 70 / 33 / 21 / 6 / 12 | challenge givers per faction (`gGiverModel`, at `_3_R09_N/G/P/J/D` points) |

### Props / vehicles / weapons

- Weapons sprinkled per part: 376, 323 (greaser P2); 318, 320, 335 (prep P2); 300, 331 (jock P2 — 331 also the catch ball); 311, `MODELENUM._W_GARBBIN` (dropout P2); reward 307 (bottle-rocket launcher, nerd) + 10 ammo.
- Vehicles: 2 parked 293s (dropout P1).
- `BX_PosterHI/LO` poster geometry swap (prep P3); `pxDormTV` PAnim set to play (greaser P3 dressing); spawner doors `gSpawnerDoors` (FMDOOR01/FMDOOR, DRPDOOR/01) locked-to-player-open-to-peds during sieges; `gDoorToLock` hideout door re-locked on cleanup.

### Named world data (5 DATs, VERIFIED by name)

`3_R09_Nerds.DAT`, `3_R09_Greasers.DAT`, `3_R09_Preppies.DAT`, `3_R09_Jocks.DAT`, `3_R09_Dropouts.DAT`. Shared keys (same ids in each): POINTLISTs `_3_R09_PLAYER`, `_3_R09_BLIPP1/2/3`, `_3_R09_ENEMIESP1/2`, `_3_R09_ALLIESP1`, `_3_R09_FIRST`, `_3_R09_GOTO`, `_3_R09_SPAWN01/02`, `_3_R09_ENDSEQUENCE`, `_3_R09_FAILLOOKAT`, `_3_R09_UNLOCKPEDS`, `_3_R09_NISPLAYER`, `_3_R09_POSTERVIEWING`, `_3_R09_BOXERS`, `_3_R09_BOXINGOUTSIDE`, `_3_R09_ENDAFTERGAME`, `_3_R09_ARCADEMACH`, `_3_R09_COMICSHOP`; PATHs `_3_R09_CAMERAPATH`, `_3_R09_PLAYERPATH`, `_3_R09_TOMACHINE`, `_3_R09_UNLOCKPATH`, `_3_R09_BookPath`, `_3_R09_FAILCAM`, `_3_R09_TRAINPATH`; TRIGGERs `_3_R09_AREAT01/02`, `_3_R09_SPAWNT01/02`, `_3_R09_MAINAREA`, `_3_R09_BASEMENTTRIGGER`, `_3_R09_UPSTAIRS`, `_FMDOORN/_FMDOORN01/_FMDOOR/_FMDOOR01`, `_ICOMSHP_BASEMENT`, `_DT_ISAFEGRSR_DOORL`, `_DT_ISAFEPREP_DOORL`, `_DT_ISAFEDROP_DOORL`, POIs `_3_R09_POI01..04`. Areas: 30 (nerd shop), 61 (greaser), 60 (prep), 59 + 13 (jock / gym), 57 (dropout), 27 (boxing club, prep P3).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| 13-in-1 params | mission framework | **challenge_giver_table** (param → faction model pool, ally pool+type, area/DAT, money, template function, giver model/point, unlock text). Sibling of `race_series_selector` (3_R08) — same "launcher sets shared var, one script fans out" shape |
| part 1/2 brawls | `spawn_npc` bundles, `wave_group`/`all_dead`, `recruit_ally` (pool), `population_override` | **poi_set_dressing** (per-index POI assignment + behavior function table: PlayCatch pairs, wall-smoke node, path walk) + **stealth_seen_branch** (`PedSetStealthBehavior(1, cb)`: on seen → attack / snap out of idle); **dog_spawn_exemption** (non-required enemy index — trivial flag on wave entries) |
| P3 siege | `continuous_wave_spawner` (pool) | delta vs pool: **area_mission_spawner** (`AreaAddMissionSpawner(quota, simult, cadence)` + per-spawn callback with blip/stat/bark throttle + `AreaMissionSpawnerExhausted` event + per-location model rotation + spawn doors locked-to-player-open-to-peds) — the pool's spawner needs quota-exhaustion and door semantics |
| sub-missions | `minigame_event_bridge` (pool) | **nested_mission_gate** (`ForceStartMission(<id>)` + poll `shared.<X>Success` until the sub-mission is inactive; success advances a local stage counter, failure = keyed mission fail; used for ConSumo, `C_Dodgeball_5`, `2_R11_Random` ×3) |
| KO/taunt | `mission_critical_npc`-style health poll | **mission_ko_point_override** (`MissionOverrideKOPoint` — respawn spot inside the hideout) + **fail_taunt_scene** (spawn end-ped, shove anim, fail camera path, then `M_FAIL_DEAD`) |
| unlock | `completion_banner` | **savehouse_unlock** (`shared.<clique>Save` + held banner + camera-path room tour + optional outfit grant/`shared.unlockedClothing`) |
| nerd shop | `area_script_shared_state` (pool: `shared.vendettaClerk`/`finishedLoadingShop` come from the shop's area script) | **shopkeeper_violence_watch** (`PedIsHit(clerk)` + `shared.playerAggressedInStore` → keyed fail + door re-lock + eject point) |

## 6. Open questions

- Where `F_SetupVendetta(param)` gets called from (giver NPC / poster launcher outside this Lua; `gMissionWait` proves the handshake).
- The exact ConSumo flow: `shared.ConSumoFinished` is set by the arcade machine (train mission `TrainASumo` mentioned — actually the arcade machine tutorial); win threshold data-side.
- `2_R11_Random` boxing internals (the random-opponent boxing mission — separate script, not read) and how its 3-win ladder selects opponents.
- Ped flag semantics used here: 58 (cheap-KO immunity on the player during P3), 108/117 toggles in other files' pattern.
- What `shared.unlockedClothing` gates beyond `Boxing NG` (nerd path sets it without an explicit clothing grant — the `3_R09_Unlock` banner key presumably covers it).
- `gMissionNoRunning`/`F_NerdStage02` (spawner-based second nerd stage) appears unreachable from the param table — dead/legacy path, flagged.
