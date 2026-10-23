# 3_02 — Bait: recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_02.lua` (1104 lines, complete) + cross-references in `main.lua` (BMX garage gating), `SFaction.lua`. `SMissPass.lua` has **no** 3_02 case (**VERIFIED** by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_02 = "Bait"** (canonical Chapter 3 list, #36, 8AM–11PM) — **VERIFIED** by content: fresh off 3_01, the player finds Gord (30) in the business district, provokes him into a chase, and lures him on bikes across town into the BMX park/warehouse (area 62) where Johnny Vincent (23) and two greasers (24, 27) wait; a bike brawl between three greasers and three preps follows. The warehouse in-engine NIS plays the bait-and-switch reveal (anim calls literally namespaced `3_02_Bait` in `Act/Vehicles.act` — **VERIFIED** title string in an anim path). No surprise.

Areas: 0 (town) and **62** (BMX warehouse interior). The [StrategyWiki walkthrough](https://strategywiki.org/wiki/Bully/Chapter_3:_Love_Makes_The_World_Go_Round) corroborates the whole shape: find Gord in Bullworth Vale, provoke him (egg/stinkbomb/punch), lead him to the New Coventry bike park — "if you knock him off his bike and get too far ahead he'll lose you and give up" (the B4 60 m watchdog) — then fight alongside Johnny's crew; rewards $20, preps −5, greasers +5, and the bike park unlocked permanently. Downstream world effects are real this time: `main.lua`'s `F_BMXGarage` only registers the garage-door open/transition handlers once `IsMissionCompleated("3_02")` — **passing Bait unlocks the BMX park as a free-roam location** (**VERIFIED** both in-tree and by the walkthrough). `SFaction.lua`: after 3_02, preps' default attitude to the player (5 vs 13) moves to 3 (from 4 post-`2_B`; decode **INFERRED** as worsening, matching the story).

## 2. Mission flow

Driver: `MissionSetup()` (baked intro + `3_02.DAT`) → `main()` → a **`F_StageFunction` pointer chain** (`F_StageGetGordSetup → F_StageGetGord → F_StageAttackGordSetup → F_StageAttackGord → F_StageLureGordSetup → F_StageLureGord → F_StageEnterParkNIS → F_StageBikeBrawlSetup → F_StageBikeBrawl → F_EndCut`), plus thread `T_DidPlayerHitGord`.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Baked cutscene "3-02"** | `PlayCutsceneWithLoad("3-02", true)`. `DATLoad("3_02.DAT", 2)`. No attempt-count check — retry skip **OPEN**. |
| B1 | **Setup** (`main`) | Player transitioned to `_3_02_JOHNNY` (area 0), POI off, BMX warehouse door `TRIGGER._DT_TPOOR_BMX` locked, player placed at `_3_02_PLAYER` with a bike (282) created at `_3_02_PLAYERBIKE`. Prep-vs-player attitude set hostile (5 vs 13 → 0). Blip on the `_3_02_BUSINESSAREA` anchor + objective `3_02_FINDOBJ`. |
| B2 | **Find Gord** (`F_StageGetGord`) | Advance when the player enters `TRIGGER._3_02_BUSINESSAREA`. Gord spawned on his bike (283) at `_3_02_GORD`, wandering, **tethered to the business-area trigger** (`PedSetTetherToTrigger`), eggs (312 ×99), mission-critical-on-death (`cbCriticalGord` → fail `3_02_GORDKO`, and if the garage was open it closes), with **social insult callbacks** (28 = player-vs-ped social, 7 and 8 = ped social events — **INFERRED** greet/taunt family) that set `bGordInsulted`. One prep escort (34) spawns on a bike and follow-focuses Gord; both initially non-hostile (attitude 2). |
| B3 | **Provoke** (`F_StageAttackGord`) | First sight within 15 m → objective swaps to `3_02_MADOBJ`. Once the player **insults Gord/his escort (social callbacks) or hits either** (who-hit-me-last): both go hostile and attack on their bikes, stat overrides maxed (all but stat 30 → 100), tethers cleared, escort made ambient, type attitude 5v13 hostile, **120 s timer starts**, music `MS_BikeChaseMid.rsm`. Leaving area 0 here → fail `3_02_OUTOFRANGE`. |
| B4 | **Lure across town** (`F_StageLureGordSetup/Loop`) | Objective `3_02_LUREOBJ`, blip on the `_3_02_PARKENTRANCE` anchor. A **mission spawner** is installed (`AreaAddMissionSpawner(1, 1, TRIGGER._POORAREA, 5)` + one location `_3_02_SPAWNER01` restricted to prep model 40, population overridden to mostly preps, ambient vehicles overridden): spawned preps queue up (`F_PrepAddToSpawnQueue`) and each is converted to a **bike harasser** — a bike (283) is created under them, random prep weapon (312/301/309), attack the player. Cap `numAmbientPrep = 3`: the oldest harasser+bike are recycled to ambient when a fourth arrives. **Distance watchdog**: if the player is > 60 m from Gord, the lure blip swaps to a Gord blip + `3_02_TOOFAR` nag and the 120 s timer keeps running; coming back within 60 m stops/pauses the timer (`MissionTimerStop` — note: the timer is *stopped*, so the 120 s only burns while engaged or away, **VERIFIED** the pause logic is asymmetric and worth re-checking at port time). Timer expiry → `3_02_OUTOFTIME`; leaving area 0 → `3_02_OUTOFRANGE`; Gord dying (incl. the garbage-can stuff KO anim, checked explicitly) → `3_02_GORDKO`. |
| B5 | **Garage + transition** | Player entering `TRIGGER._3_02_PARKENTRANCE` opens the BMX garage prop (`_BA_BMXGARAGE`, `BIKEGAR` open node). Player entering `TRIGGER._DT_BMXGARAGE` with the lure live: timer stopped, spawner removed, **everything from the chase is deleted except the player's current bike** (vehicle kept if it's the one being ridden), control cut, music swaps to `MS_FightingPreps.rsm`, preps made hostile to greasers (4) too. |
| B6 | **Warehouse NIS** (`F_StageEnterParkNIS` + `F_SetupBMXIntro`) | `DATLoad("3_02_BMX.DAT", 2)` — a **second DAT for the interior** (**VERIFIED**). Transition to area 62 `_3_02_WHPLAYER`; player mounted on their bike (recovered via `PlayerGetLastBikeId` or a fresh 282 if none). Six riders created on bikes: greasers Johnny/24/27 (282s, bats 301) and preps Gord/31/34 (283s, 301). A fully in-engine skippable scene: several hardcoded camera cuts (FOV 90→30), Johnny's `3_02_Bait` anim node, scripted lines 10/11/12 with speech-waited cuts, all skippable via `WaitSkippable` (`bSkipNIS`). Then everyone deleted, control restored. |
| B7 | **Bike brawl** (`F_StageBikeBrawlSetup/Loop`) | Player re-mounted (bike to `_3_02_WHPLAYERBIKE`); the six riders re-created with **doubled health**; all six put in `PedBikeBrawl` mode; greasers made untargetable; Johnny mission-critical (`3_02_JOHNNYKO`); objective `3_02_BRAWLOBJ`. Per frame: any rider on a bike but not in the `BBrawl` task is re-set to brawl; any rider **off** their bike switches to on-foot attack on a random opposing prep once (`bAttackOnFoot`); weapon 301 ammo restocked to 10 when dry (infinite brawl). Stage ends when all three preps are dead. `T_DidPlayerHitGord` records whether the player ever hit Gord (who-hit-me-last on the WH Gord) — an outro/reward flag. |
| B8 | **Outro** (`F_EndCut`) | Control off, dismount forced; preps deleted; Johnny + surviving greasers respawned at cut points facing the player; fixed cameras + **two variant beats**: if the player never hit Gord, Johnny's line 16 + player line 17 and **`PlayerAddMoney(1000)`** ($10); if the player did hit Gord, Johnny's line 14 + player line 15 instead. Greasers walk off. Banner `M_PASS` 2000 with tag **`3_02_UNLOCK`**, respect messages `MRESPECT_GP5` (+5 greasers) and `MRESPECT_PM5` (−5 preps), `SetFactionRespect(4, +5)`, `SetFactionRespect(5, −5)`, pass music, `MissionSucceed(false, false, false)`. Cleanup sets `shared.bBMXWarehouseInit = false` so `main.lua` re-arms the now-unlocked garage. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Gord dies (any source, incl. garbage-can stuff KO) | `cbCriticalGord` + anim check | `3_02_GORDKO` |
| 120 s chase timer expires | B4 timer | `3_02_OUTOFTIME` |
| Player leaves area 0 during chase | B3/B4 area check | `3_02_OUTOFRANGE` |
| Johnny dies in the brawl | `cbCriticalJohnny` | `3_02_JOHNNYKO` |

Retry: `MissionFail(false, true, msg)` everywhere — retry, no fade, no checkpoints; replay from B0. On cleanup: if the mission ended with the player inside area 62 without completing, they're transitioned back to `_3_02_PARKSPAWN` in area 0; if the player is in the garage trigger at cleanup they're nudged to the park spawn — portal safety.

### Pass effects

In-script: $10 (only if Gord untouched by the player), banner 2000 + `3_02_UNLOCK` tag, greasers +5 / preps −5. External (VERIFIED in `main.lua`/`SFaction.lua`): **BMX garage/warehouse unlocked for free roam**; preps' default attitude step. No SMissPass case, no clothing/weapon/chapter effects.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0 intro | baked (`3-02`) | `fade` + `letterbox` + `say` beat at `_3_02_PLAYER` |
| CS-2 | B6 warehouse NIS | in-engine, skippable (~20 s, 4 camera cuts, 3 lines) | the ch1 skippable-in-engine-cutscene pattern: `camera` + `say` + `WaitSkippable` semantics |
| CS-3 | B8 outro (2 variants) | in-engine, fixed cameras | `camera` + `say`; variant picked by a `Flag` condition (`player_hit_gord`) — pure composition |
| CS-4 | B8 pass banner | engine UI | `completion_banner` + respect lines |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 30 | Gord | chase target: tethered wanderer → provoked biker → warehouse brawler; death = fail |
| 34 | prep escort (town) / prep brawler (WH) | follow-focus on Gord; made ambient after provocation |
| 23 | Johnny Vincent | WH greaser leader; doubled health; untargetable in brawl; death = fail |
| 24, 27 | greasers ×2 | WH brawlers, doubled health |
| 31, 34 | preps ×2 (WH) | Gord's brawl wingmen (30/31/34 on 283s) |
| 40 | spawner preps | `_POORAREA` mission-spawner harassers, auto-biked, cap 3 recycled to ambient |
| 282 | greaser/player bike | player bike preserved across the transition (or recreated) |
| 283 | prep bike ("AquaBike" per a print — Aquaberry livery, **INFERRED**) | |

### Animation groups / action trees

`LoadAnimationGroup("NIS_3_02")`; `LoadActionTree("Act/Conv/3_02.act")` (NIS lines + outro nodes `/Global/3_02/NIS/Outro/Johnny/Johnny_01/02`). `Act/Vehicles.act` bike NIS node `3_02_Bait`. `Act/Props/Door.act` `BIKEGAR` open/close nodes on the garage prop. Brawl behavior is the engine `BBrawl` task (`/Global/AI/OnBike/BikeGeneralObjectives/BBrawl`).

### Props / pickups / weapons

- BMX garage prop `_BA_BMXGARAGE` (+ door trigger `_DT_BMXGARAGE`, interior twin `_BMX_GARAGEDOOR`/`_DT_BMXGARAGEDOOR` per `main.lua`).
- Weapons: **312 = eggs** (Gord's town loadout), **301** (brawl melee — bat?, restocked to 10, name **OPEN**), **309** (prep spawner pool, **OPEN**).
- No pickups.

### Speech

`M_3_02` scripted ids: 10, 11, 12 (warehouse reveal), 14/15 (outro — player hit Gord), 16/17 (outro — clean) — **7 distinct lines**. Music: `MS_BikeChaseMid` (chase), `MS_Confrontation_NIS` (reveal), `MS_FightingPreps` (brawl); pass/fail stingers.

### Named world data to extract from `3_02.dat` + `3_02_BMX.dat`

- POINTLISTs: `_3_02_PLAYER`, `_3_02_PLAYERBIKE`, `_3_02_JOHNNY`, `_3_02_GORD`, `_3_02_GORDBIKE`, `_3_02_PREP1`, `_3_02_PREP1BIKE`, `_3_02_SPAWNER01`, `_3_02_PARKSPAWN`; interior (`3_02_BMX.DAT`): `_3_02_WHPLAYER`, `_3_02_WHJOHN(+BIKE)`, `_3_02_WHGR1/2(+BIKE)`, `_3_02_WHGORD(+BIKE)`, `_3_02_WHPREP1/2(+BIKE)`, `_3_02_PLAYERPARK`, `_3_02_PREPPIESINTRO` (3), `_3_02_GREASERSINTRO` (2), `_3_02_WHPLAYERBIKE`, `_3_02_PLAYERENDCUT`, `_3_02_JOHNNYENDCUT`, `_3_02_GREASERSCUT` (2).
- TRIGGERs: `_3_02_BUSINESSAREA`, `_3_02_PARKENTRANCE`, `_DT_BMXGARAGE`, `_BA_BMXGARAGE` (prop), `_POORAREA` (shared), `_3_02_SPAWNER01`, `_DT_TPOOR_BMX` (door lock).
- Hardcoded NIS/outro cameras: (-756.35, 636.40, 30.01)→(-757.20, 636.91, 30.15); (-775.92, 627.81, 32.22)→(-774.98, 628.14, 32.18); (-762.77, 634.81, 30.48)→(-761.81, 635.09, 30.52); (-753.39, 636.15, 30.09)→(-754.33, 636.48, 30.12); (-754.84, 635.22, 30.78)→(-753.92, 635.56, 31.00); outro (-761.07, 636.00, 30.52)→(-760.09, 635.83, 30.47) and (-755.17, 632.43, 30.82)→(-755.80, 633.20, 30.71). All area 62.
- Area codes: **0** and **62**. DATs referenced: `3_02.DAT`, `3_02_BMX.DAT` — trigger data exists in the original assets; not in the Lua-only clone.
- Dead data: `F_JumpNodes` jump-node id tables (3/8/18/23/29/37 etc.) are defined but never used (**VERIFIED** vestigial — likely a cut BMX-park jump challenge).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `mission_critical_npc`, `ped_hit_event`, `npc_stat_override`, `dynamic_blip`/`blip_on_npc`, `population_override`, `poi_enable`, `door_prop_control`/`door_lock_set` (garage + `_DT_TPOOR_BMX`), `countdown_fail_timer` (with a **pause/resume** variant — Timer condition + stop-on-regain), `npc_ride_vehicle_path` n/a (wander, not path), `npc_weapon_restock_loop` (ch2 2_03 — the brawl's 301 restock is exactly it), `completion_banner`, `set_faction_respect`, `area_visible_eq` (leave-area-0 fail), `cutscene_skip_on_retry` (in-engine, via `WaitSkippable`), `social_interact` (insult callbacks), `player_mounted`/vehicle preservation composition, `world_state_swap` (garage unlock via shared flag + main.lua consumer), `vehicle_leash_respawn` n/a. Schema: `Flag`/`Timer`/`LeaveArea`/`TriggerEnter` conditions and `StartMission`-style actions all already exist; `Reward` covers the conditional $10 with a `Flag` gate.

| Beat | Covered by | New primitives needed |
|---|---|---|
| B2 Gord wander | `spawn_npc` | **npc_tether_to_zone** (`PedSetTetherToTrigger` — NPC wanders but cannot leave a trigger) |
| B3 provoke | `ped_hit_event`, `social_interact` | **npc_insult_event**: insult social action (vs. NPC) as a stage-advance event, OR-composed with hit — small, but "taunt to enrage" is a distinct authored gate |
| B4 lure | `escort_follow`-adjacent (reversed) | **lure_to_zone**: hostile NPC chases the player; pass when player reaches zone with NPC alive and within range; >range nag + blip swap; timer burns only appropriately (needs the pause semantics) |
| B4 harassers | `population_override` | **mission_spawner**: `AreaAddMissionSpawner` + spawn locations + model filter + spawn callback → queue → per-spawn converter (mount a created bike, arm, attack) + cap-and-recycle-to-ambient. This is a real subsystem, not a flag bundle |
| B7 brawl | `npc_weapon_restock_loop`, `wave_group`-adjacent | **bike_brawl_mode**: `PedBikeBrawl` task wrapper — keep-on-bike re-engage loop, dismount → one-time on-foot attack switch, team-vs-team, ends on all-opponents-dead |
| B7 Gord KO by can-stuff | `mission_critical_npc` | **humiliation_ko_detect**: death OR playing a specific humiliation-death anim counts as the fail (small variant; could fold into mission_critical_npc config) |
| B6 scene | `camera`, `say` | nothing new (ch1 skippable in-engine cutscene) |
| B8 variant outro + $10 | `Flag` condition, `Reward` | nothing new (composition) |
| B8 garage unlock | `world_state_swap` | nothing new (shared flag + area-script consumer) — but note it as the first mission whose pass changes a **traversal unlock**, not just respect |

Genuinely NEW names introduced here: **npc_tether_to_zone**, **npc_insult_event**, **lure_to_zone**, **mission_spawner**, **bike_brawl_mode**, **humiliation_ko_detect**.

## 6. Open questions

- The 120 s timer's stop/restart asymmetry in B4 (`MissionTimerStop` when the player returns within 60 m — does a restart reset to 120 s or resume? `MissionTimerStart(gordTimer)` is only called once in B3, so post-stop behavior is engine semantics — **OPEN**, and it changes difficulty materially).
- Whether Gord actually follows the player cross-town by AI chase or by tether trickery after B3 (tethers cleared, attack mode 3 — presumed chase-the-player engine behavior, **INFERRED**).
- Spawner params `AreaAddMissionSpawner(1, 1, trigger, 5)` (counts? cap? faction 5) — **OPEN**.
- Weapon 301/309 identities — **OPEN** (301 = bat, **INFERRED** from greaser loadouts in other chapters, but not verified here).
- `PedSetPedToTypeAttitude` value semantics throughout (0/2/4) — consistent with ch2 usage, decode still **INFERRED**.
- What the `3_02_UNLOCK` banner tag displays (BMX park unlock toast, **INFERRED**).
- `shared.test3_02active` (set true at B1, never read in-file) — a debug/test hook, consumer **OPEN**.
- Retry cutscene skip for `3-02` (no attempt check) — **OPEN**.
- `F_PedDeleteCallback` and the `tblJump` data are dead in-file (**VERIFIED**) — likely leftovers of a cut BMX-jump beat; whether `3_02_BMX.DAT` still carries those paths is worth noting at extraction time.
