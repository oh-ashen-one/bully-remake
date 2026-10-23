# 2_06 — Movie Tickets: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_06.lua` (1162 lines, complete) + `chap2/2_06supplemental.lua` (see its own note) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_06 = "Movie Tickets"** (canonical Chapter 2 list, available 8-20) — **VERIFIED** by content and audio, not by an anim-group title string: the loaded groups are `IDLE_SEXY_C` and `NIS_2_06_1` (no mission title embedded), but the romance interactive stream is literally `MS_MovieTixRomance.rsm`, and the flow is the Pinky movie-date errand chain: Pinky (model 38) waits by the theater (`POINTLIST._2_06_THEATER`), the player steals Gord's bike (vehicle model 283) to lure Gord (30) and Lola (25) away, buys chocolates (item 478) at the Yum Yum market (`_2_06_YUMYUM`) for Eunice (74) to clear the line, kisses her in the alley (`_2_06_KISSKISS`), scares off Petey (13) and his buddy (85), then returns to Pinky. Matches the canonical "Movie Tickets" entry exactly. No surprise.

Setting: rich area outdoors (area 0) plus the store interior — `AreaGetVisible() == 26` is the in-store check, so **area code 26 = store interior (Yum Yum market), INFERRED** (new code; not in the ch1 table). Prep dorm doors `_RA_PREPDOOR14/15` are locked to peds for the mission duration.

## 2. Mission flow

Driver: linear `main()` chain — `F_MissionInit()` spawns the whole cast up front, then `F_StealTheBike() → F_GoingBackToTheater(1) → F_Eunice() → F_GoingBackToTheater(2) → F_Petey() →` final Pinky NIS. Each stage is skipped if `bFailedDueToViolence`. No threads; everything is polled in stage loops with `F_CheckMoney()` folded in.

Setup (`MissionSetup`/`F_MissionInit`): cutscene `2-06` (baked), `2_06.DAT` loaded, POI disabled, ambient peds cleared, a 20 m-radius ambient spawner at (346.87, 218.43, 4.95) deactivated, player money floored at 100 ($1.00, **INFERRED** from cents convention in 1_E01/1_G1), player placed at `_2_06_PLAYER_START` facing Pinky. Five **social override trees** are loaded for the mission (`Mission/2_06WantGift.act`, `2_06Greeting.act`, `2_06Flee.act`, `2_06Follow.act`, `2_06Praise.act` — custom greet/gift/flee/follow/praise behavior, ids 24/18/19/4/15). Unique-model statuses of all six named characters are saved and suppressed (`PedSetUniqueModelStatus(-1)`) so no ambient duplicates appear. All six are **mission-critical with distinct fail callbacks**.

| # | Beat | Logic (functional) |
|---|---|---|
| M0 | **Baked cutscene "2-06"** | `PlayCutsceneWithLoad("2-06", true)`. No explicit attempt-count skip in this file — retry behavior **OPEN** (engine attempt count, pattern from ch1). |
| M1 | **Objective: the date setup** | Objective `2_06_MAIN_OBJ` (meet/handle the date, **paraphrase**) + 4 s text. Then straight into the bike beat. |
| M2 | **Steal Gord's bike** (`F_StealTheBike`) | Objective `2_06_STEAL_BIKE` + blip on the bike (vehicle 283, `VehicleSetOwner` = Gord). Gord + Lola stand socializing at `_2_06_WAITERS`; Bryce (35) and Justin (34) at `_2_06_OTHERWAITERS`. **Trigger:** player mounts the bike **or** Gord strays >12 m from it. Then: Gord + Bryce + Justin all attack the player, Gord gets a blip, Lola ally-link dismissed, ambient bark `BIKE_STOLEN`. **Reclaim loop:** if the player is on foot inside `TRIGGER._2_06_BIKEZONE` and not fleeing, Gord walks back to the bike, mounts it, rides it to its parking point (`_2_06_GORDSBIKE`, callback `cbStopGordsBike`), dismounts, and returns to Lola — resetting the beat for another steal attempt. **Stage ends** when the player is on the bike outside `_2_06_BIKEZONE` **or** Gord himself leaves the zone (lured past it): objective completes, the Lola NIS (M3) plays, and the return-to-theater objective opens. Player scooping the bike mid-reclaim (`bPlayerScoopedBike`) short-circuits Gord's remount. |
| M3 | **Lola NIS (in-engine, ~10 s)** (`F_LolaNIS`) | Control off, widescreen, camera hardcoded at (344.96, 217.98, 6.15); Lola placed at `_2_06_NISLOLA`, faces heading 210, plays conv node `NISLOLA/Lola/Lola01` + speech id 99, walks toward Gord; second camera (329.16, 228.58, 6.05) for Gord's reply (same speech id 99, generic category); Gord recruited as Lola's ally; then **Gord, Lola, Bryce and Justin are all deleted** — the whole prep group leaves the mission here. |
| M4 | **Return leg 1 + Eunice setup** (`F_GoingBackToTheater(1)`) | Objective `2_06_GETBACK` + theater blip; stage gates on the player within 7.5 m of `_2_06_THEATER`. Eunice (74) is spawned at `_2_06_WAITERS` with social overrides (greet/gift/want-gift), required-gift id 1 registered but requirement disabled, chocolate-give override `Mission/206GiveChocolates.act` loaded for the player, flee-on-hit detection armed (global event handler 7). Petey + Constantine (85) are pre-spawned at `_2_06_OTHERWAITERS` (both mission-critical). |
| M5 | **Eunice: the chocolate detour** (`F_Eunice`) | Objective `2_06_EUNICE`. On greeting Eunice with no chocolates (item 478 == 0): objective `2_06_CHOCOLATE` added, blip swaps to `_2_06_YUMYUM` (store), bark `GIFT_REQUEST_GIRL`. The store blip re-points between outside (area 0) and inside (area 26) versions. If the player already carries chocolates at greet, the buy step is skipped (line 96 reused from `M_1_02` — cross-mission speech reuse, **VERIFIED**). Once 478 is held: objective `2_06_CHOCOGIVE`, blip back on Eunice, wait until the player is outside (area 0) then within 1.5 m of her on foot → `F_GiftEunice`. |
| M6 | **Gift scene (in-engine micro-NIS)** (`F_GiftEunice`) | Widescreen, control off, mutual face/lock, Eunice walks to 0.8 m, player plays `/Global/Player/Gifts/GiveChocolates`, romance stream `MS_MovieTixRomance.rsm` starts, Eunice thanks (`THANKS_JIMMY`), **one 478 is consumed** (`ItemSetCurrentNum(count-1)`), then objective `2_06_ALLEY`: Eunice becomes the player's recruited ally with a kiss-zone blip at `_2_06_KISSKISS`. |
| M7 | **Escort to the alley + kiss** | Ally-follow with a 5 m separation rule: if Eunice falls >5 m behind she returns to her wait point, objective reverts to `2_06_GET_EUNICE`, and the player must re-recruit her by walking within 1 m. At the kiss point (1 m radius): ally dismissed, `F_KissEunice` runs — fade, widescreen, both moved to `_2_06_KISSKISS`, mutual approach to 0.6 m, then the make-out action node chain (`MakeOut/Makeout/GrappleAttempt` → `.../Kisses`) plays on the player until the anim ends; camera fades back mid-anim. Eunice is then released to ambient wander. One **seen-event** beat: the first time Pinky has line-of-sight to both the player and Eunice, Pinky barks `DISGUST`. |
| M8 | **Return leg 2 + Petey** (`F_GoingBackToTheater(2)`, `F_Petey`) | Theater objective again; then objective `2_06_PETEY` + blip on Petey. Petey and Constantine stand at `_2_06_WAITERS`. When the player comes within 8 m of Petey (and Petey isn't alerted): `F_PeteyNIS` — mutual facing, Petey barks `TAUNT_RESPONSE_CRY`, Constantine barks `INDIGNANT`, both run off along `_2_06_PETEYRUNSOFF`; `bEndIt` set. (Dead code: `cbGreetPinky`/`F_GreetPinky` reference a `bPeteyRanInFear` that is never set — an alternate "greet Pinky to finish" path, **INFERRED** cut.) |
| M9 | **Final Pinky NIS (in-engine)** | Control off, fade, widescreen; player and Pinky placed at `_2_06_NISPINKYEND` 1-2, mutual lock; three fixed cameras (FOV 30) alternate across six supersize speech lines (ids 66-71) with conv nodes `NISPINKY/Pinky/Pinky01-04` and `Jimmy/Jimmy01-02`. Then `_RA_PREPDOOR14` unlocked, Pinky walks off along `_2_06_PINKYWALKOFF`, control returns. |
| M10 | **Pass** | Fade, pass camera at (341.74, 222.55, 6.22), completion banner `M_PASS` with value 1500 (**INFERRED** $15.00), respect message `MRESPECT_PM5`, pass music (stinger id 7), `SetFactionRespect(5, GetFactionRespect(5) - 5)` — faction 5 **= preppies, INFERRED** (the mission humiliates Gord); `MissionSucceed(false,false,false)`. No chained mission start; no `SMissPass.lua` branch exists for 2_06 — **no item/weapon/class unlock** for this mission, **VERIFIED** by absence from `F_SetupMissionRequirements`. |

### Fail conditions and retry

All fails route through `bFailedDueToViolence` + a specific key; fail music stinger 7, `MissionFail(false, true, key)`, `shared.b2_06Failed = true` (cleanup then makes peds ambient instead of deleting them).

| Cause | Mechanism | Fail key |
|---|---|---|
| Pinky hit/KO'd | `cbPinkyCritical` (mission-critical callback) | `2_06_PINKYHURT` |
| Gord hit by the player | `cbMissionCriticalGord` counts player hits (bike grapple-throw hits explicitly excluded — the bike-steal yank is legal); first counted hit fails | `2_06_GORDKO` |
| Broke | `F_CheckMoney()` polled in every loop: money < 100 **and** no chocolates **and** not currently shopping | `CMN_STR_06` |
| Lola / Bryce / Justin / Petey / Constantine hit | their mission-critical callbacks (any hit) | `2_06_VIOLENCE` (generic) |
| Eunice | first player hit only shows an apology tutorial (`TUT_APOLOGY1`, 4 s); a **second** hit (or her death, or being hit by a vehicle the player drives) fails — `2_06_VIOLENCE` | |
| Eunice flees | polled: Eunice running the generic flee task = fail | `2_06_VIOLENCE` |

No checkpoints: a retry replays from M0/M1. Cleanup restores POI, the radius spawner, prep door locks, unique-model statuses, radar range, and unloads `IDLE_SEXY_C`.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | M0, before control | baked (`2-06`) | fade in at `_2_06_PLAYER_START` facing Pinky + letterbox + one `say` beat of original VO |
| CS-2 | M3 Lola scene | in-engine, 2 cameras | letterbox + fixed `camera` ×2 + `say` ×2 + `timer` (3 s walk beat); fold the four-ped delete into the outro fade |
| CS-3 | M6 gift | in-engine micro-scene | control off + mutual face + `pair_anim` (give) + romance music; already our `give_item_scene` shape |
| CS-4 | M7 kiss | in-engine, partial fade | `fade` out/in around one `pair_anim` (make-out) at a marked point; per existing 1_G1 D6 pattern |
| CS-5 | M8 Petey scare | in-engine, no camera work | two barks + two flee paths on `npc_near` event; no cut needed |
| CS-6 | M9 final Pinky scene | in-engine, 3 fixed cameras, 6 lines | letterbox + `camera` cycle + `say` chain sequenced by speech-end; the longest authored scene of the mission |
| CS-7 | M10 pass camera | engine camera set | trivial, fold into `completion_banner` outro |

Cutscene-only anim groups: `NIS_2_06_1`, `IDLE_SEXY_C`. Action trees: `Act/Conv/2_06.act` (all conv nodes), plus the five social override trees and `Mission/206GiveChocolates.act`.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 38 | Pinky (date) | mission-critical, own fail key; invulnerable during final NIS; several player social actions disabled against her (ids 23, 28, 29, 32) |
| 30 | Gord (bike owner) | stationary + ally-paired with Lola until provoked; hit-counting critical callback with bike-throw exemption |
| 25 | Lola | stationary, socializing; deleted after M3 |
| 35 | Bryce | attacks on steal; deleted after M3 |
| 34 | Justin | attacks on steal; deleted after M3 |
| 74 | Eunice | gift requirement (id 1 registered, disabled), required-gift chocolate flow, warn-first hit tolerance, ally-follow segment, kiss scene |
| 13 | Petey | scare-off beat; mission-critical |
| 85 | `idConst` (Petey's buddy) | mission-critical; **note:** named like Constantine but model 85 is a bully id in ch1 docs (1_03 wave bully) — do not assume identity, **OPEN** |

### Vehicles / weapons / items

- Vehicle **283** = Gord's bike (**INFERRED** bicycle; `VehicleSetOwner`, blip-for-car, ride-back-to-point behavior). Weapon model **431** loaded, never used in-file — **OPEN** (also an unused load in 1_02B; possibly a theater/prop weapon).
- Item **478** = box of chocolates — **VERIFIED** by context (1_02B's store item and this mission's `GiveChocolates` anim); consumed one per gift.
- No other pickups/props created.

### Speech

Scripted `M_2_06` ids: 3 (Pinky greet), 66-71 (final NIS, supersize), 99 (Lola/Gord, played twice) — **9 distinct scripted lines**, plus one reused `M_1_02` id 96 (Eunice). Ambient events: `BIKE_STOLEN`, `DISGUST`, `THANKS_JIMMY`, `GIFT_REQUEST_GIRL`, `TAUNT_RESPONSE_CRY`, `INDIGNANT`. Music: interactive stream `MS_MovieTixRomance.rsm` from the gift onward; pass/fail stingers id 7.

### Named world data to extract from `2_06.DAT`

- POINTLISTs: `_2_06_PLAYER_START`, `_2_06_CORONA`, `_2_06_PINKYWAITS`, `_2_06_WAITERS`, `_2_06_OTHERWAITERS`, `_2_06_GORDSBIKE`, `_2_06_THEATER`, `_2_06_NISLOLA`, `_2_06_NISPINKYEND`, `_2_06_PINKYWALKOFF`, `_2_06_YUMYUM`, `_2_06_KISSKISS`, `_2_06_PETEYRUNSOFF`.
- TRIGGERs: `_2_06_BIKEZONE`, `_RA_PREPDOOR14`, `_RA_PREPDOOR15` (ped-locked doors).
- Hardcoded coords (all cameras/spawner): (346.87, 218.43, 4.95 r20); (344.96, 217.98, 6.15)→(345.28, 218.92, 6.17); (329.16, 228.58, 6.05)→(329.87, 227.87, 5.99); (339.18, 219.69, 6.47)→(339.53, 220.63, 6.40); (337.78, 224.96, 6.41)→(338.52, 224.29, 6.36); (341.74, 222.55, 6.22)→(340.78, 222.27, 6.27).
- Area codes seen: **0** (outdoors/rich area), **26** (store interior, **INFERRED** new code).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1 set | New primitives needed |
|---|---|---|
| M2 bike steal trigger | `event`, `blip` | **vehicle_guard_reclaim** (owner NPC state machine: chase-on-theft → walk to vehicle → mount → ride to parking point → dismount → return to post; resets while player stays in zone on foot; broken by player exiting zone mounted or owner exiting zone); **player_mounts_vehicle(v)** / **npc_distance_to_object(n, obj)** conditions; **blip_on_vehicle** |
| M2 aggro bundle | `spawn_npc` flag bundles, `ped_hit_event` | **mission_critical_hit_exemption** (per-anim-state hit whitelist — the bike grapple-throw is legal); nothing else new |
| M3/M9 NIS scenes | `camera`, `letterbox`, `say`, `fade`, `timer` | none new (fixed-camera `say` chains, existing shape) |
| M4/M5 social setup | `social_interact`, `mission_critical_npc` | **social_override_set** (per-mission custom greet/gift/flee/follow/praise trees registered by id); **npc_flee_task_watch** (fail if NPC enters generic flee state) |
| M5 store detour | `dynamic_blip` (1_G1), `player_has_item` (1_02B) | **money_floor_fail** (polled money < X ∧ ¬has-item ∧ ¬in-shop-state → fail with key); **money_floor_ensure** at setup; **area_blip_variant** (blip re-point on area 0↔26 swap — or fold into `dynamic_blip`) |
| M6 gift | `give_item_scene` (1_S01/1_G1), `pair_anim` | **consume_item(id, n)** (decrement inventory on gift) |
| M7 escort + re-recruit | `escort_follow` (1_02B) | **escort_separation_reset** (ally >R behind → ally returns to wait point, objective reverts, re-recruit by proximity ≤r) |
| M7 kiss | `pair_anim` (1_G1 make-out), `reach_location` | none new |
| M7 Pinky witness | — | **npc_witness_event** (NPC has LOS to two targets simultaneously → one-shot bark; `PedCanSeeObject` pair) |
| fails | `mission_critical_npc` | **warn_first_hit** (first player hit shows tutorial, second fails — Eunice); **vehicle_hit_fail** (being struck by a player-driven vehicle counts as player violence) |
| world control | `population_override`, `poi_enable`, `door_lock_set` | **spawner_radius_toggle** (disable/restore one ambient spawner by center+radius) |
| pass | `completion_banner`, `set_faction_respect` | none new (respect here is **relative delta**, not absolute set — note variant) |

## 6. Open questions

- Whether the baked `2-06` cutscene is skipped on retry (no attempt check in-file).
- The five social override trees (`Mission/2_06*.act`) and `206GiveChocolates.act` are undecoded; we know their registration ids and when they fire, not their internals.
- Weapon model 431's purpose; what gift id 1 would have been (`PedSetRequiredGift(idEunice, 1)` then disabled — **INFERRED** a cut requirement).
- `bPeteyRanInFear` / greet-Pinky-to-finish: dead alternate ending path, wiring outside this file or cut.
- Exact `_2_06_BIKEZONE` shape vs. the theater/store geometry (coords pending DAT extraction).
- How the money fail interacts with the store purchase UI (`shared.playerShopping` is set by the shop system, outside mission scripts).
- Who the model-85 ped `idConst` is meant to be (name suggests Constantine, id suggests a generic bully).
