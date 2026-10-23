# 4_S12 — Here's to You, Ms. Phillips (timed gift fetch): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_S12.lua` (714 lines, complete) + `SFaction.lua` / `SInitGl.lua` cross-references. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_S12 = Here's to You, Ms. Phillips** — catalog entry #44 (Chapter 4, 3:30PM–7PM window). **VERIFIED** by content: recipient ped `tblMsPhillips` model 63 at `_4_S12_MSPHILLIPS`, the three gift pickups (perfume 490, necklace 491, dress 492) consolidated into gift item 521, rival `tblPeanut` model 21 (Peanut, greaser — same id as the 3_B crate-jumper greaser), violent-fail key `4_S12_PHILIPS_HURT`, completion payout `M_PASS` + 5000 (money — cents; **INFERRED** $50), cutscenes `"4-S12"`/`"4-S12B"`.

Structure: single timed free-roam fetch — a 450-second `MissionTimerStart`, three interior-gated pickups in any order, a decoy-bike ambush choice at the start, chaser pairs on each grab, then a delivery back to Ms. Phillips at the school. No stage checkpointing; the main loop is nested `while` polls on area visibility, triggers and timers. `shared.g4S12Perfume = false` is set at script load (cross-mission flag consumed elsewhere — `SFaction.lua` references 4_S12 availability/completion; exact consumer **OPEN**).

Setting: town + three interiors — dress gated to **area 34**, necklace to **area 33**, perfume to **area 46**; delivery at the school (**area 2**, art-room door `TRIGGER._DT_ISCHOOL_ART`). Area ids **VERIFIED** from `AreaGetVisible()` gates; which shop each interior is needs the DAT/world map (**OPEN**).

## 2. Mission flow

Driver: `MissionSetup()` (DAT, baked `"4-S12"`, transition to `_4_S12_PLAYERSTART`) → `main()`: table init → bikes → Peanut decoys → objectives/timer/counter → outer loop { ambush watch → `F_PeanutGoes` → `F_GetItems` → `F_GetBack` → `F_Outro` } → pass/fail tails.

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup** | `4_S12.DAT` layer 2; baked cutscene; no fade-in; player at start point. Models: peds {26, 28, 21, 63}, bike 282 ×3, weapons {312, 301}, pickups {490, 491, 492, 521} spin-loaded. |
| S1 | **Briefing** | Control on; objective banner `4_S12_MAINOBJ`; **timer 450 s**; three objectives (dress/necklace/perfume) + three overworld blips; HUD counter with Ms. Phillips icon (0/3); chaser-threat thread armed; interactive stream `MS_SearchingHigh.rsm` locked. |
| S2 | **Peanut decoy ambush** | Two identical Peanut peds sit asleep on bikes at `_4_S12_PEANUTSTART` elements 1/2 (each stat-tuned: 36→70, 25→70, 33→40). The player springs the trap by entering either bike trigger (`_4_S12_PEANUTBIKE` / `_4_S12_PEANUTBIKE2`) **or** hitting either decoy (`PedGetWhoHitMeLast`). `F_PeanutGoes`: the sprung Peanut wakes, focuses and attacks the player; the other decoy and its bike are deleted. (Prepping for a chase that never fully materializes in-script — see OPEN.) |
| S3 | **Item collection ×3** | Each item is **interior-gated**: the pickup spawns only while the player is inside its area (34/33/46); leaving to area 0 deletes it and restores the overworld blip. Picking one up: objective completes, counter increments, that item's chasers arm (`bDressChasers`/`bNecklaceChasers`/`bPerfumeChasers`) — on next return to area 0, `F_CreateChasers(n)` spawns a pair of greasers (models 26/28, eggs 312 ×12, stat-tuned 10→50 / 11→60) from the item's spawn list who focus/attack the player; repeat logic teleports lagging chasers (≥30 m) to the spawn or despawns/rebuilds ones that didn't follow within 10 m. A thread fires a `FIGHT_INITIATE` bark the first time each chaser closes to 3 m. |
| S4 | **Consolidation** | All three collected → return text + objective 4; Ms. Phillips blip on; the three item counts zeroed and the player is handed the single gift item **521**; counter hidden. The mission then waits for the player to be back in area 0 before arming the return leg. |
| S5 | **Delivery** | Return to the school (area 2): Ms. Phillips spawns stationary at her point with a blip, the art-room door is locked to peds, and she is made **mission-critical** (`cbHitHer` — player hit → she goes ambient, violent fail). Player entering `_4_S12_MSPHILLIPS` trigger ends the leg. |
| S6 | **Outro** | Fade; art door prop closed; chasers/Peanut/Ms. Phillips deleted; models unloaded; baked `"4-S12B"`; pass tail. |
| S7 | **Pass** | Player placed at the Ms. Phillips point, fixed camera, NIS-safe; `MinigameSetCompletion("M_PASS", true, 5000)`; win music track 10; `MissionSucceed(false, false, false)` (first arg false — same shape as 4_G4; **OPEN** what arg 1 suppresses here). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Timer expires | `MissionTimerHasFinished` polled in every leg | `AS_NO_TIME` |
| Ms. Phillips hurt by player | mission-critical callback `cbHitHer` | `4_S12_PHILIPS_HURT` |
| (Player death) | no in-script watch — engine default | — |

Cleanup zeroes whichever item counts are outstanding (or gift 521 on success), unlocks the art door to peds if the player is inside area 2, resets type attitudes, stops timer/counter/music. No checkpoint; retry = full restart.

### Rewards / pass side-effects

- In-script only: 5000 money via the completion banner. **No `SMissPass` entry** for 4_S12 (VERIFIED absence — chapter-level `F_MissionFactionChanges` still runs generically). No respect change, no unlock key; `shared.g4S12Perfume` stays false (purpose unknown — **OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | S0 opening | baked `"4-S12"` | `fade` + original establishing card |
| CS-2 | S2 ambush spring | in-engine (wake + attack) | `spawn_npc` + `say` + attack order |
| CS-3 | S6 handoff | baked `"4-S12B"` | `fade` + `completion_banner` + original outro card |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 63 | Ms. Phillips | stationary delivery target; mission-critical (player-hit fail); blip icon 17 |
| 21 | Peanut ×2 (decoy pair) | asleep on bikes; stat-tuned (36/25→70, 33→40); sprung one attacks, other deleted |
| 26, 28 | chaser pair | eggs 312 ×12 each; stat-tuned (10→50, 11→60); per-item spawn lists; bark on close approach |

### Animation groups / action trees

None loaded in-file (no `LoadAnimationGroup`/`LoadActionTree` calls — all beats use stock AI: sleep, focus, attack, stationary). **VERIFIED.**

### Props / pickups / weapons

- Pickups: 490 (perfume, area 46), 491 (necklace, area 33), 492 (dress, area 34) — `PermanentMission` butes; consolidated into **521** (gift bundle) for delivery.
- Bikes 282 ×3 (two Peanut decoys + a player bike at `_4_S12_PLAYERBIKE`).
- Weapons: 312 (chaser eggs), 301 (requested; unused in-file — **OPEN**).

### Speech

Peanut taunt/line tables `4_S12_PEANUT01..06` (defined; the taunt table is wired to nothing visible in-file — **dead-ish code**, **OPEN**), chaser `FIGHT_INITIATE` barks. Roughly **0-6 scripted lines** actually reachable. Music: `MS_SearchingHigh.rsm` locked for the whole mission.

### Named world data to extract from `4_S12.DAT` (trigger data **VERIFIED present** — `DATLoad("4_S12.DAT", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_4_S12_PLAYERSTART`, `_4_S12_PLAYERBIKE`, `_4_S12_PEANUTSTART` (2), `_4_S12_PEANUTBIKE` (2 elements — bike spawns), `_4_S12_SEXYDRESS`, `_4_S12_NECKLACE`, `_4_S12_PERFUME` (+ `_BLIP` variants), `_4_S12_DRESS_CHASER`, `_4_S12_NECKLACE_CHASER`, `_4_S12_PERFUME_CHASER`, `_4_S12_MSPHILLIPS`.
- TRIGGERs: `_4_S12_PEANUTBIKE`, `_4_S12_PEANUTBIKE2`, `_4_S12_MSPHILLIPS`, `_DT_ISCHOOL_ART` (door).
- Hardcoded cameras: pass cam (-668.71, -295.55, 6.61 → …).
- Area codes seen: **0** (overworld), **2** (school), **33** (necklace interior), **34** (dress interior), **46** (perfume interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| S1 briefing | `mission framework`, `collect_counter`, timer (`countdown_fail_timer` pool) | none new |
| S2 decoy ambush | `spawn_npc` | **npc_decoy_pair** (N identical sleeping NPCs at N spots; the one the player approaches/hits becomes a live attacker, the rest despawn with their vehicles) |
| S3 interior-gated pickups | `pickup_spawner` (pool, partial) | **area_gated_pickup** (pickup spawns only while player is inside a given area id; despawn + overworld-blip restore on exit; per-item spawn ↔ door blip swap) |
| S3 chasers | `chaser` / `mounted_chaser_spawner` (pool) | reuse, plus variant note: **re-aggro teleport** (chaser ≥30 m from player is teleported to its spawn point and re-aimed; one that never followed within 10 m is rebuilt) — fold into `chaser` |
| S4 consolidation | `give_item`, `inventory_item_consume_on_fail` (pool) | **item_bundle_consolidation** (N quest items zeroed, single delivery item granted) — minor, could fold into `give_item` |
| S5 delivery target | `mission_critical_npc`, `door_lock_set` (pool) | reuse |
| fails | `countdown_fail_timer`, `mission_critical_npc` | reuse |

## 6. Open questions

- `shared.g4S12Perfume`: written false here, consumed where? (`SFaction.lua` gates on 4_S12 availability/completion but the flag itself is unaccounted for.)
- Whether Peanut's post-ambush behavior is really just "attack on foot" (bike left behind; `PedPutOnBike` only used for staging) and why two near-identical decoys exist rather than one escort beat.
- `F_GiftInit`, `cbPeanutAway`, `tblRace`, `tblPeanutSays`/`tblPeanutTaunt` wiring: defined but (mostly) uncalled — **dead code**, possibly from a cut race-against-Peanut design (**INFERRED** from names; the `tblPeanut` table even carries a `deliveryRadius` field).
- Which interiors areas 33/34/46 map to (shops in Bullworth town — likely the clothing store / jewellery / drugstore).
- `MissionSucceed(false, …)` first-arg semantics vs. the `true` used in 4_B1/4_B2.
- The `F_CreateChasers` re-aggro condition at line ~621 is convoluted (`not (i ~= 1 or bChaser01FollowedPlayer) or i == 2 and not bChaser02FollowedPlayer`) — verify intended per-slot logic against gameplay before reimplementing.
