# 2_G2 — Carnival Date: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_G2.lua` (1082 lines) + `SMissPass.lua` (no 2_G2 case — **VERIFIED** absence) + `SZone.lua` (item/weapon ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_G2 = "Carnival Date"** (canonical Chapter 2 list, 16:30-23) — **VERIFIED** by the loaded animation groups `"2_G2CarnivalDate"` and `"2_G2_GiftExchange"`, Pinky (model 38) as the date, carnival minigames (dunk tank, baseball toss, high striker, shooting gallery), tickets (item 495), and the teddy-bear prize (363 = "Teddy Bear", VERIFIED via `SZone.lua`). No surprise.

Setting: the carnival grounds outdoors (area 0; player spawned at `_2_G2_SPAWNPLAYER`, intro cameras around (205-210, 422-436)) plus the **prize-tent interior, area code 50** (`AreaGetVisible() == 50` gates the tent blips — **INFERRED**: 50 = carnival gift/freak-show tent interior; new code beyond the known list).

**Start gate (unusual):** the mission requires the player to carry **flowers (item 475 = "FlowerGift", VERIFIED name)**. Without them it prints a hint + tutorial and ends with `MissionFail(false, false)` — no fade, no retry, i.e. a soft refusal, not a real fail. On a real start the flowers are **consumed** (`ItemSetCurrentNum(475, ...-1)`). Also on start: money topped up to **$10.00** (1000 cents) if below.

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → `F_Stage1()` → `F_Stage2()` → `F_Stage3()` sequential stage functions (debug entry points `F_StartAtStage2/3` exist). Threads: `T_MonitorMiniGames`, `T_MonitorPinky`, `T_TextCarnyTutorial`, plus per-event speech threads. `MissionAllowConcurrentMissions(true)` — errands can run alongside (**INFERRED** from the call).

| # | Beat | Logic (functional) |
|---|---|---|
| G0 | **Baked cutscene "2-G2"** (flowers branch only) | `PlayCutsceneWithLoad("2-G2", true)` in `MissionSetup`, then `2_G2.DAT` loaded. No explicit retry-skip in this file — **INFERRED** global attempt handling; **OPEN**. |
| G1 | **Meet Pinky (timer 240 s)** | Player transitioned to `_2_G2_SPAWNPLAYER`, blip on `_2_G2_PINKYARRIVED`, objective 1 (`2_G2_MOBJ_01`), `MissionTimerStart(240)`. Social overrides loaded (`Mission/2_G2Follow.act`, `Mission/2_G2PlayerGift.act`). Reaching the trigger → in-engine arrival cutscene (below) → stage 2. |
| G2 | **Pinky arrival NIS (skippable, button 7)** | Fade, control off, widescreen; if the player rode in on a bike it's parked at `_2_G2_MOVEBIKE`; population suppressed; Pinky (38) spawned at `_2_G2_INTROPINKY`, two jocks (13, 15) posted at `_2_G2_INTROJOCK1/2`; scripted exchange: player waits (`JimmyWaits` node, speech 1), jocks walk off on paths, Pinky walks in (`_2_G2_INTROPINKY` path, faces player at node 4), lines 3/4/6 over three camera setups, then both walk to `_2_G2_CUTBEARSPAWN*` points. Population restored. |
| G3 | **Win 10 tickets** | Pinky recruited as ally + **mission-critical** (her own fail callback), objective 2, ticket HUD counter (icon `CarnTicket`, max 10, capped display), carnival tutorial (4 s delayed). Tickets come from the four booths + store; `T_MonitorMiniGames` watches `shared.minigameRunning` (`DunkTank`/`BaseballToss`/`CarnieStriker`/`ShootingGallery`/`CarnieStore`): on start it repositions Pinky to the booth (`_2_G2_MGDUNKPINKY`/`MGSTRIKEPINKY`/`MGSHOOTPINKY`, faces her at the player, counter hidden); on finish (`shared.minigameRunning == nil`) it plays a result-dependent Pinky bark (win lines 31/44/37, lose line 15), restores the counter, and re-recruits her via per-booth exit points (`_2_G2_HACKEXIT*`). During play, `shared.MGaction` 1/2 = cheer/boo barks (27/35, 28/36) on a 20 s cooldown. Rides: `shared.g2_G2_HidePinky`/`g2_G2_ReturnPinky`/`g2_G2_ReturnWhere` hide her (teleport to `_2_G2_HIDEPINKY`, invulnerable) and return her at per-ride exits (`_2_G2_EXITCOASTER/EXITSQUID/EXITWHEEL/EXITKARTS`). Booth proximity flavor: lines at striker (54/56), dunk (30), toss (26), shooting (34) when tickets short; freak-show trigger line 43. |
| G4 | **Tickets ≥ 10 → buy the bear** | Stage 3: objective 3, blip on the tent entrance (`_2_G2_BLIPTENTENTER`), counter removed 3 s later. The blip swaps between tent entrance and exit as the player crosses area 50. When the player has the bear (**weapon 363 = Teddy Bear**, VERIFIED; bought in the tent — purchase itself is the store/minigame side, **OPEN** mechanics) and enters `_2_G2_TICKETSHOP`: Pinky dismissed/re-recruited, walks `_2_G2_PINKYWANTSBEAR`, gift requirement set (gift id 21 = bear, **INFERRED**), kiss/humiliate social actions disabled on her, speech 49, blip on her. |
| G5 | **Give the bear → pass** | Outside the tent with the bear: invulnerable pair, control off, widescreen, fixed camera (179.95, 434.85, 7.64), player teleported to `_2_G2_MOVEPLAYERWITHBEAR` with the bear equipped, Pinky at `_2_G2_OUTSIDEVENDOR`, mutual lock-on; the give anim (`Act/Gifts/Give2G2.act` `Give_Attempt`) flows into the make-out pair state; **pass banner with tag `2_G2_PINKYUNLCK`**, pass music; after the kiss: fade, Pinky's gift requirement reset (2), emotion set (8 = romantic, **INFERRED**), she wanders off ambient and is deleted, player teleported to a second camera spot (184.13, 436.15, 6.72), control back. `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| No flowers at start | `F_PlayerHasFlowers()` false | none — `2_G2_NOFLOWER` text + `2_G2_FLOWERTUT` tutorial, `MissionFail(false, false)` (no retry) |
| Pinky hit/KO'd (mission-critical) | `F_MissionCritical` | `2_G2_FAIL_01` |
| Clock reaches **01:10** | `F_CheckClockForFail` per-frame | `2_G2_FAIL_02` |
| Meet-Pinky timer (240 s) expires | `MissionTimerHasFinished` in stage 1 | `2_G2_FAIL_03` |
| Abandoning Pinky | warned once at `_2_G2_PINKYARRIVED` (line 14), then entering `_2_G2_PINKYANGRYFAIL` | `2_G2_FAIL_04` (she dismisses, goes ambient) |
| Pinky dies | `T_MonitorPinky` | `2_G2_FAIL_05` |
| Money < $1.00 while not shopping and (no item 479 or tickets < 10) | `F_CheckMoney` per-frame in stage 1 | `CMN_STR_06` (generic broke message) |

Cleanup restores the player's pre-mission ticket count and zeroes the bear if not passed. Retry behavior for G0 not explicit — **OPEN** (pattern from other missions suggests attempt-skip).

### Rewards / pass side-effects

- Completion banner tag `2_G2_PINKYUNLCK` (**Pinky unlocked** as a girlfriend/kissable NPC — **INFERRED** from the tag and the post-pass social/emotion changes).
- Flowers consumed at start; tickets/bear consumed during. No `SMissPass.lua` entry: no force-completes, no chapter change (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | G0 `"2-G2"` | baked | `fade` + `letterbox` + 1-2 original `say` beats (the invitation), or title card |
| CS-2 | G2 Pinky arrival | in-engine, skippable (button 7), ~15 s | `letterbox` + 3 `camera` setups + `npc_follow_path` entrances + 4 `say` beats; keep the skip |
| CS-3 | G5 bear gift + kiss | in-engine, ~10 s | `letterbox` + fixed `camera` + `pair_anim` (give → make-out) + `completion_banner` |

Cutscene-only anim groups: `2_G2CarnivalDate`, `2_G2_GiftExchange`, plus `F_Girls`, `NPC_Love` for Pinky/social beats.

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 38 | **Pinky** (date) | unique-ified for the mission (`PedSetUniqueModelStatus(38, -1)`), mission-critical, ally-follow via social override |
| 13, 15 | two jocks | intro gag only, walk off on paths |

### Animation groups / action trees

`LoadAnimationGroup`: `F_Girls`, `NPC_Love`, `2_G2CarnivalDate`, `2_G2_GiftExchange`.
`LoadActionTree`: `Act/Conv/2_G2.act` (intro/Idle beats), `Act/Gifts/Give2G2.act` (gift give → make-out), social overrides `Mission/2_G2Follow.act`, `Mission/2_G2PlayerGift.act`.

### Props / pickups / weapons

- Items: **475 = FlowerGift** (start gate, consumed), **495 = ticket** (counter currency), **363 = Teddy Bear** (weapon-slot prize prop, equipped for the give scene), 479 (checked in the money fail — unidentified, **OPEN**; possibly a free-ride/pass item).
- Money: topped up to $10 at start; $1 floor fail.
- Vehicle 283 (Pinky's bike) declared in `F_TableInit` but never created — dead table entry (**INFERRED** cut content).
- No weapons requested besides 363.

### Speech

`M_2_G2` ids: 1, 3, 4, 6, 7, 14, 15, 26, 27, 28, 30, 31, 34, 35, 36, 37, 43, 44, 45, 49, 54, 56, 57, 100 — **24 distinct scripted lines**. Music: `MS_RomanceHigh.rsm` on arrival; pass/fail stingers.

### Shared-state contract with the carnival systems

`shared.g2_G2` (mission active), `shared.minigameRunning`, `shared.MGaction`, `shared.quit_minigame`, `shared.g2_G2_HidePinky`, `shared.g2_G2_ReturnPinky`, `shared.g2_G2_ReturnWhere`, `shared.playerShopping` — the carnival minigame/ride scripts drive these; our side must expose the same event surface.

### Named world data to extract from `2_G2.DAT`

- POINTLISTs: `_2_G2_SPAWNPLAYER`, `_2_G2_PINKYARRIVED`, `_2_G2_INTROPINKY`, `_2_G2_INTROJOCK1`, `_2_G2_INTROJOCK2`, `_2_G2_INTROJIMMY`, `_2_G2_CUTBEARSPAWNPINKY`, `_2_G2_CUTBEARSPAWNJIMMY`, `_2_G2_BLIPTENTENTER`, `_2_G2_BLIPTENTEXIT`, `_2_G2_MOVEPLAYERWITHBEAR`, `_2_G2_OUTSIDEVENDOR`, `_2_G2_MOVEBIKE`, `_2_G2_MGDUNKPINKY`, `_2_G2_MGSTRIKEPINKY`, `_2_G2_MGSHOOTPINKY`, `_2_G2_HIDEPINKY`, `_2_G2_EXITCOASTER`, `_2_G2_EXITSQUID`, `_2_G2_EXITWHEEL`, `_2_G2_EXITKARTS`, `_2_G2_HACKEXITDUNK`, `_2_G2_HACKEXITTOSS`, `_2_G2_HACKEXITSHOOT`, `_2_G2_HACKEXITSTRIKE`, `_2_G2_STAGE3DEBUG`.
- PATHs: `_2_G2_INTROJOCK1`, `_2_G2_INTROJOCK2`, `_2_G2_INTROPINKY`, `_2_G2_PINKYWANTSBEAR`.
- TRIGGERs: `_2_G2_PINKYARRIVED`, `_2_G2_HIGHSTRIKER`, `_2_G2_DUNKTANK`, `_2_G2_BALLTOSS`, `_2_G2_SHOOTINGGALLERY`, `_2_G2_TICKETSHOP`, `_2_G2_PINKYANGRYFAIL`, `_2_G2_FREAKSHOW`.
- Hardcoded cameras: intro (210.56, 422.20, 4.56 → 210.02, 423.00, 4.79), (205.90, 424.04, 5.61 → 206.87, 424.26, 5.66), (209.60, 423.38, 4.85 → 208.98, 424.15, 5.02); gift scene (179.95, 434.85, 7.64 → 180.70, 435.50, 7.53), outro (184.13, 436.15, 6.72 → 183.17, 436.16, 6.99).
- Area codes seen: **0** and **50** (tent interior, INFERRED).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| Start gate | — | **mission_start_item_gate** (require item N to start: soft-refuse with hint + tutorial, no retry; consume 1 on start), **player_money_check** (top-up floor at start; min-money fail variant) |
| G1 meet timer | `timer` | countdown variant already exists (`countdown_fail_timer`); **clock_fail_at(hh:mm)** (mission fails when the world clock hits 01:10 — extend the ch1 clock actions with a fail edge) |
| G2 arrival | `letterbox`, `camera`, `say`, `npc_follow_path` + node cb, `dismount_vehicle` (bike parking) | — |
| G3 date loop | `escort_follow` / `recruit_ally`, `mission_critical_npc`, `collect_counter` (capped display), `minigame_launch` | **minigame_event_bridge** (mission consumes `shared.minigameRunning` start/finish + `MGaction` cheer/boo events from minigame scripts; repositions companion per booth), **ride_exit_teleport** (companion hidden for a ride, returned at a per-ride exit point via shared request flags), area-blip swap on `area_visible_eq` (exists) |
| G4/G5 gift | `give_item_scene`, `pair_anim`, `completion_banner` (tagged) | **social_unlock_npc** (permanent relationship change: required-gift id set/reset, emotion state, unlock tag in the pass banner) |
| fails | `mission_critical_npc`, `timer` | abandonment two-step (warn trigger → fail trigger) = `trigger_enter` pair, no new primitive |

## 6. Open questions

- The bear purchase mechanics inside the tent (store script side — not this file) and what item 479 is.
- How minigame scores convert to tickets (minigame scripts own that; this file only watches `shared.*`).
- Gift-type ids (21 bear, 2 reset) mapping to the gift table — **OPEN**.
- `PedSetEmotionTowardsPed(..., 8)` value semantics (**INFERRED** romantic).
- Whether retry skips the baked `"2-G2"` (no in-file attempt check).
- `F_PlayerSnarky` (line 57, control off) is defined but never called in this file — dead code, likely fired from the social/gift tree (**OPEN**).
