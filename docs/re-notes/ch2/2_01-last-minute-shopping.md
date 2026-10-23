# 2_01 — Last Minute Shopping: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_01.lua` (837 lines) + `SMissPass.lua` (`F_SetupMissionRequirements` 2_01 block) + cross-refs `AreaScripts/Cafeteria.lua` (`gEdnaOverride`), `SStores.lua`/`AreaScripts/StyleStores.lua` (`vendettaClerk`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_01 = "Last Minute Shopping"** — **VERIFIED** by the loaded animation group `"2_01LastMinuteShop"` and by content: Edna (ped model 58, the cafeteria lady — confirmed by `Cafeteria.lua`'s `shared.gEdnaOverride` suppressing her ambient spawn, same pattern as 1_S01) sends the player to fetch her bike and collect three shopping items (grocery / barber / clothing stores) in the Vale business district, then take the bus back. Matches the canonical Chapter 2 list entry "Last Minute Shopping (optional, 8-19)". No surprise. The optionality and time window live in the world mission table (outside the Lua) — **OPEN**, as for all missions.

Setting spans the main map: starts in area 2 (`ischool`, cafeteria end — **INFERRED** from the end-scene coords (-638.7, -269.5) and `_2_01_CAFENDPOS`), shopping interiors are areas 26 / 39 / 34 (see §4), everything else outdoors (area 0).

## 2. Mission flow

Driver: `main()` is a **blocking sequence** — `F_tBikeManager()` → counter setup + `F_tIngredientsGet()` → wait for area 0 → `F_CheckBackToEdna()` — with three concurrent threads (`T_FailCond`, `T_Timer`, `T_BikeMountTut`). No stage-function chain.

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **Baked cutscene "2-01"** | `PlayCutsceneWithLoad("2-01", true, true, true)` in `MissionSetup`. Loads `2_01.DAT` (slot 2). `gEdnaOverride = true` (suppresses ambient cafeteria Edna), interactive music tiers `MS_SearchingLow/Mid/High`, no fade-in, objective `2_01_O1` added, Edna's unique model (58) suppressed for the mission (`PedSetUniqueModelStatus(58, -1)`), groups `2_01LastMinuteShop` + `NIS_2_01` loaded, school-gate override released, bike-mount tutorial thread armed (`TUT_BIKE3` if the player crosses `_2_01_TUTPLAY1` off a vehicle). Retry-skip is not explicit in the file — **OPEN** (engine attempt count assumed, pattern from 1_01/1_03). |
| A1 | **Cafeteria setup** | Doors `DT_ischool_Janitor` and `DT_ischool_Attic` unlocked (reason **OPEN**), player transitioned to area 2 at `_2_01_PLAYERSTART`, fade in. Edna (58) spawned at `_2_01_EDNA`: mission-critical with a hit-fail callback, ignore-stimuli, flag 133 cleared. Fail and timer threads start. Objective text: find the bike (paraphrase). |
| A2 | **Get the bike** (`F_tBikeManager`, blocking) | Bike (vehicle 281, variable named `idGirlBike`) created at `_2_01_BIKESPAWN`, owned by the player, blipped. Beat completes when the player mounts **any** vehicle **or** reaches the business district (`TRIGGER._AMB_BUSINESS_AREA`). Objective `2_01_O1` completes; collect objective text follows. |
| A3 | **Three shopping stops** (`F_tIngredientsGet`, blocking) | HUD counter 0/3 with `caflady`/`caflady_x` icons; three store blips + objectives `2_01_O2/O3/O4` (grocery `_2_01_PLAYERSTORE`, barber `_2_01_RAZORSPAWN`, clothing `_2_01_PANTYSPAWN`, order free). Each store's pickup is **lazily created when the player enters its interior** (area 26 → pickup 528 `ednaMeat`; 39 → 499 razor; 34 → 507 lingerie; all `PermanentButes`). On pickup: counter +1, objective complete, blip removed; the grocery hand-off additionally plays one clerk bark (`M_2_01` id 6) on the store-system clerk ped (`shared.vendettaClerk`, spawned by `SStores`/`StyleStores`, not by the mission). After each stop, once back outdoors, a "N remaining / 1 remaining" message pair (`2_01_OGETREMAIN`/`_1`). At 3/3 the counter HUD hides and the loop exits. |
| A4 | **Return to school** | Wait for visible area 0. Objective `2_01_GOTOBUS` + blip on `_2_01_busstoppoint` + bus tutorial (`BUS01`). `F_CheckBackToEdna` waits until the player is in `TRIGGER._ZONESCHOOL` **and not mid-bus-transition** (`shared.gBusTransition`, set by the bus system — **OPEN** which script). Then: objective complete, Edna teleported to `_2_01_EDNAFIN` with a character blip, new objective `2_01_OBACKEDNA`; Edna turns to face the player when he enters `_2_01_EDNAFACEPLAYER`. |
| A5 | **Delivery + pass** (`F_CinEnding`) | Player within ~2 m of Edna: Edna invulnerable, mission timer stopped. Control off, widescreen, NIS-safe, mutual lock-on; player plays the hand-over node `/Global/2_01/Anim/Give/GiveEdna_2_01` (`Act/Conv/2_01.act`) with player line 19, then a second fixed camera while Edna's line 20 plays on node `/Global/2_01/Anim/EndingNIS/20`. Edna made ambient (persisted as `shared.gEdnaID`), mission-critical cleared, walks off to (-629.4, -265.0, -0.7). Fade; player moved to `_2_01_CAFENDPOS`; two more camera shots; pass banner (`M_PASS`, 1.5 s) + pass music; `MissionSucceed(false, false, false)`; control back. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Mission timer expires | `MissionTimerStart(325)` in `T_Timer` (325 s; note the declared `iMissionTime = 300` is never used — **VERIFIED** vestigial) | `2_01_FAILTIME` |
| Player hits Edna | `PedGetWhoHitMeLast(Edna) == gPlayer` poll in `T_FailCond`, or the mission-critical callback `cbFail` | `2_01_HITEDNA` |

On fail: Edna made ambient (persisted via `shared.gEdnaID`), fail music, `MissionFail(false, true, key)` — retry allowed. No partial checkpoints: retry replays from A0/A1. Cleanup: blips removed, interactive music stopped, `gEdnaOverride` released, the three ingredient inventory counts zeroed (528/507/499), Edna unique-model status restored, punishment points zeroed, counter HUD hidden, DAT slot 2 unloaded. **VERIFIED bug:** cleanup calls `F_CleanBlip(blipClothing)` but `main` assigns `blipCloth` (undeclared global) — the declared `blipClothing` is always nil, so the clothing-store blip can leak on cleanup.

### Dead / vestigial code (do not port)

- **Four complete in-engine cutscenes** `F_NIS_CinGrocery` / `F_NIS_CinBarber` / `F_NIS_CinCloth` / `F_NIS_CinEnding` (plus `NISTable`, `F_NISSelect`, `F_Clamp`, `T_NIS_Loop`, `F_FindClosestPed`): full store-interior scenes — area transition (26/39/34/2), population zeroed, player auto-walked along `_2_01_CINGROCPATH`/`_CINBARBPATH`/`_CINCLOTHPATH`, four skippable text beats each (`2_01_GROC1-4`, `BARB1-4`, `CLOTH1-4`), a give/receive pair anim (`GiveItem/Give` + `Receive`), plus an alternate ending with camera path `_2_01_CAMENDING` and lines 18/19/20/21. **Never called from the live flow** (only from the debug NIS selector, itself never started) — the shipped mission has **no** store cutscenes. Their camera coords still document the store interiors (§4).
- `F_tEdnaWalk` (Edna wandering random cafeteria paths `_2_01_EDNACAF1/2/3` with a shrug anim between legs) — never started; live Edna stands at her point. The random-wander primitive is **not required** by the shipped mission.
- `F_Debug` (bDebugNIS path), `F_CameraDebug`, `F_KillCars` (a `Wait(0)`-less infinite loop, never started), `F_CreatePedTable`, `F_BlipCreateXYZ/F_BlipCleanup`, `blipX/Y/Z` (209.0, -73.3, 8.6), `tblIngredientLocations` x/y/z fields (hardcoded duplicates of the store points; the live flow uses POINTLISTs), `tblPickupModels` entry 521 (loaded, never spawned — 521 is the errand parcel in 1_E01, **INFERRED** a generic package prop), `tCamera`, `factModels`, `Cop1/2`, `idCopCar1/2`, `bShopKeepGetsAngry`, `idGenStoreBlip`, `currentBlip` (mostly).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | A0, before control | baked (`2-01`) | `fade` in on the cafeteria + `letterbox` + one `say` exchange (Edna briefs the errand), or straight to A1 |
| CS-2 | A5 delivery | in-engine, ~10 s, 2 fixed cameras + mutual lock + give anim | `letterbox` + fixed `camera` + give/receive pair beat (`give_item_scene`) + `say` ×2 + `completion_banner`; compressible to fade + banner |
| (dead) | A3 store scenes ×3 + alt ending | in-engine, complete but unwired | reference only; if ever wanted: `player_follow_path` + `camera` + `say`-as-text + `pair_anim` |

Cutscene-only anim group: `NIS_2_01`. Action nodes live in `Act/Conv/2_01.act` (`Give/GiveEdna_2_01`, `EndingNIS/20`, `EdnaShrug`, `GiveItem/Give`, `GiveItem/Receive`).

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 58 | Edna (cafeteria lady) | mission-critical (hit → fail), ignore-stimuli; unique-model suppressed for the mission (`unique_ped_suppress` pattern, status saved/restored) |
| store clerks | grocery hand-off bark | **not spawned by the mission** — `shared.vendettaClerk` comes from the ambient store system (`SStores.lua` / `StyleStores.lua`); the grocery beat no-ops if the clerk isn't valid |

### Pickups / weapons / vehicles

- Pickups: 528 (grocery item, variable `ednaMeat` — **VERIFIED** meat by name), 499 (razor, `_2_01_RAZORSPAWN` — **VERIFIED** by name), 507 (lingerie, `_2_01_PANTYSPAWN` — **VERIFIED** by name); 521 loaded but never spawned (dead load).
- Weapon model 370 loaded (`LoadWeaponModels`) and requested in `F_CinEnding` — hand prop for the delivery anim, **INFERRED** a grocery bag.
- Vehicle 281 (`idGirlBike` — **VERIFIED** name; girl's bike **INFERRED**), spawned owned-by-player with a car blip.
- `LoadModels(tblPedModels)` = { 58 }.

### Animation groups / action trees

`LoadAnimationGroup`: `2_01LastMinuteShop`, `NIS_2_01`. `LoadActionTree`: `Act/Conv/2_01.act` (give/ending/shrug nodes above).

### Speech / music

`M_2_01` scripted ids in the live path: **6** (grocery clerk), **19** (player, delivery), **20** (Edna, delivery) — 3 lines; dead NIS code adds 18 and 21. Music: `MS_SearchingLow/Mid/High` interactive tiers; pass/fail stingers via `SoundPlayMissionEndMusic`. Tutorials: `TUT_BIKE3` (bike mount), `BUS01` (bus travel).

### Named world data to extract from `2_01.DAT`

- POINTLISTs: `_2_01_PLAYERSTART` (area 2), `_2_01_EDNA`, `_2_01_EDNAFIN`, `_2_01_BIKESPAWN`, `_2_01_PLAYERSTORE` (area 26), `_2_01_RAZORSPAWN` (area 39), `_2_01_PANTYSPAWN` (area 34), `_2_01_busstoppoint` (area 0), `_2_01_CAFENDPOS` (area 2), `_2_01_CINBARBERSTART`, `_2_01_CINCLOTHSTART`, `_2_01_CINENDINGSTART` (dead-NIS only).
- PATHs: `_2_01_EDNACAF1/2/3` (dead wander), `_2_01_CINGROCPATH`, `_2_01_CINBARBPATH`, `_2_01_CINCLOTHPATH`, `_2_01_CAMENDING` (dead NIS).
- TRIGGERs: `_2_01_TUTPLAY1`, `_2_01_MOUNTBIKE`, `_2_01_TUTOFF2` (tutorial-off, live), `_2_01_EDNAFACEPLAYER`, `_AMB_SCHOOL_AREA`, `_AMB_BUSINESS_AREA`, `_ZONESCHOOL` (the last three shared ambient triggers).
- Doors: `DT_ischool_Janitor`, `DT_ischool_Attic` (unlocked at setup), `DT_tbusines_BikeShopDoor` (debug toggle only).
- Hardcoded coords: end-scene cameras (-638.6, -271.4, -0.1 → look) and (-638.7, -270.4, -0.36), pass cameras (-639.1, -275.3, -0.46) etc.; player delivery spot (-638.7, -269.5, -1.7); Edna walk-off (-629.4, -265.0, -0.7). Dead-NIS interior cameras document the three stores: grocery ~(-571.7, 392.7, 2.1) (area 26), barber ~(-656.3, 131.4, 5.0) (area 39), clothing ~(-645.4, 259.9, 2.9) (area 34).
- Area codes seen: **0** (outdoors), **2** (ischool/cafeteria), **26** (grocery/general store, **INFERRED**), **34** (clothing store, **INFERRED**), **39** (barber, **INFERRED**). New codes 26/34/39 not in the known list — all three are Vale business-district interiors (**INFERRED** from `_2_01_*`/`tbusines` context).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by (ch1 set) | New primitives needed |
|---|---|---|
| A0 cutscene + retry | `fade`, `letterbox`, `cutscene_skip_on_retry` | — |
| A1 setup | `spawn_npc` flag bundles (mission_critical, ignore-stimuli), `door_lock_set`, `unique_ped_suppress` | — |
| A2 bike fetch | `trigger_enter` | **vehicle bundle**: `spawn_vehicle(model, point, owner, blip)` + condition **player_in_vehicle(any\|id)** (see 2_02 doc for the full bundle) |
| A3 shopping | `collect_counter` (icon + n/max), `pickup_spawner` composed with `area_visible_eq` (1_G1) for the lazy per-store spawn, `dynamic_blip` | — (composition covers it) |
| A3 clerk bark | `say` | dependency note: the clerk ped belongs to the ambient store system — mission reads `shared.vendettaClerk`; our port needs a store-system handle or a mission-spawned clerk stub |
| A4 bus return | `trigger_enter`, `hint` | **fast_travel_leg**: go-to-transit-point objective + wait on destination-zone trigger AND not-mid-transition flag (`gBusTransition` handshake with the bus system) |
| A5 delivery | `give_item_scene` (pair anim + lines), `completion_banner`, `fade`, `camera`, `letterbox`, condition `player_near_ped` | — |
| A5 Edna persist | `event` | **persist_npc_across_mission** (`shared.gEdnaID` handoff: mission ped survives into ambient world; trivial shared-slot + make-ambient) |
| fails | `countdown_fail_timer` (325 s), `mission_critical_npc` (hit variant via who-hit-me-last) | — |
| music | — | **interactive_music tiers** (low/mid/high streams armed at setup; intensity switching engine-side — also needed by 2_02 in a distance-driven variant) |

## 6. Rewards and unlocks (`SMissPass.lua`, 2_01 block — VERIFIED)

- If the player holds zero of item **312**, give 5 ammo of 312. **312 = eggs** (VERIFIED by cross-script evidence: `2_05.lua` "EggsBute" pickup and `GiveWeaponToPlayer(312)`, `Ambient/EggPoorH.lua`/`EggGreaser.lua` egg-throwing with 312). Note: this grants egg ammo well before the egg-centric missions (2_03/2_05) — reward order does not gate eggs behind "The Eggs".
- No force-completes, no chapter set, no class unlocks, no outfit/weapon grants in this branch. No money in the Lua (would come from the mission table — **OPEN**).

## 7. Open questions

- Retry behavior for the baked cutscene (no attempt check in the file).
- How/where the mission is offered (mission table outside Lua), and what enforces the 8-19 optional window.
- Why `DT_ischool_Janitor`/`DT_ischool_Attic` are unlocked at setup (no janitor/attic content in the flow).
- The bus system's `shared.gBusTransition` writer and the bus-stop trigger geometry (bus stop point is named; the transition logic lives elsewhere).
- Whether the timer is visible on HUD (engine-side; same OPEN as 1_11x2's countdown).
- Area codes 26/34/39 vs the install's IPB bounds (pending DAT extraction).
