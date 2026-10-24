# 5_G5 — Smash It Up: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/5_G5.lua` (2605 lines, complete — ~1700 of them are the destruction table) + cross-references in `AreaScripts/Bdorm.lua`, `main.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_G5 = "Smash It Up"** (canonical Chapter 5 list, #59 in `docs/story-mode-catalog.md`, optional, 7PM–11PM) — **VERIFIED**: the [Bully wiki Chapter V page](https://bully.fandom.com/wiki/Chapter_V) maps script id `5-G5` to "Smash It Up" under non-storyline missions; the [Smash It Up wiki entry](https://bully.fandom.com/wiki/Smash_It_Up) (Jimmy meets Zoe; a competition to smash the Spencer Shipping warehouse) and the [StrategyWiki ch5 walkthrough](https://strategywiki.org/wiki/Bully/Chapter_5:_The_Fall_and_Rise_of_Jimmy_Hopkins,_Aged_15) match the script (starts at Zoe's house in Blue Skies, requires "Revenge on Mr. Burton", reward **$60** = the in-file `MinigameSetCompletion("M_PASS", true, 6000)`; whoever causes the most damage in the time limit wins; lose and you repeat the mission — the in-file `5_G5_LOST` fail). `gMinutesToSmash = 4` — the time limit is 4 minutes.

Downstream (**VERIFIED** in the clone):

- `AreaScripts/Bdorm.lua:190`: completing 5_G5 adds a dorm-room trophy to the 100% counter (a framed picture of Zoe on the wall).
- `main.lua:537`: `PedModelCreateGiftRequirement(2, 2, "5_G5", 0, true, 100)` — a social gift/kiss gate keyed on this mission (Zoe's romance gate; parameter decode **INFERRED**).
- No `ChapterSet`, no faction respect changes, no SMissPass entry.

Setting: starts in area 0 (Blue Skies, walk to the warehouse), then `AreaTransitionPoint(54, …)` into the warehouse interior (area 54). The **punishment system is disabled** for the smashing phase (`DisablePunishmentSystem(true)` at stage 2, re-enabled in cleanup) — vandalism is free here. Radar min/max is restored in cleanup but never set in-file (presumably pinned by the DAT or the area transition — **OPEN**).

## 2. Mission flow

Driver: `MissionSetup()` (baked intro "5-G5", DAT) → `F_SetupMission()` (weapon models 418/300/`W_CRUTCH`/323, Zoe model 48 with unique-model suppression, warehouse doors locked, `MINI_React`, the big destruction table) → `F_Stage1` (walk with Zoe) → `F_Stage2` (4-minute destruction contest) → `F_Stage3` (scoring scene → pass or fail). `main()` then runs the win/lose tail: pass = `MissionSucceed`; fail = one of two keyed fails with different presentation.

| # | Beat | Logic (functional) |
|---|---|---|
| G0 | **Setup** | Zoe (48) spawned at `_5_G5_ZOE`: mission-critical (`F_MissionCritical` fail callback), stimuli-immune, attitude-to-player 4, always-update-animation, infinite sprint, one ped flag (98) cleared. Front/roof warehouse door props get flag 11 set; doors `DT_INDOOR_WHOUSEFRONT`/`ROOF` locked. |
| G1 | **Walk to the warehouse** | Zoe follows `_5_G5_ZOE2WAREHOUSE` at gait 3 **tethered to the player (12 m)**; blips on both warehouse doors (front counts as the objective blip, back as plain); objective `5_G5_MOBJ01`; intro bark (id 1). If Zoe reaches `_5_G5_WAREDOORZOE` she snaps inside (`_5_G5_ZOE_WH`) and drops the tether. Player entering either door trigger → stage 2. (Debug starts teleport straight to `_5_G5_START`.) |
| G2 | **Interior NIS** (skippable) | Fade; area transition to 54; both characters teleported in; peds cleared; `T_Cutscene01` plays five speech beats (ids 8, 9, 16, 17, 10) with alternating hardcoded cameras and Zoe/Jimmy anim nodes from `Act/Conv/5_G5.act`; any-beat skip via button 7. Then: punishment off, `MS_DestructionVandalismMid.rsm` streams, exterior warehouse doors (`DT_whouse_front`/`roof`) locked, weapon pickups spawned (300 at `_5_G5_SPAWNBAT`, 323 at `_5_G5_SPAWN2X4`, 300 again at `_5_G5_SPAWNWRENCH` — the "wrench" point spawns another 300, **VERIFIED** as scripted), Zoe gets a blip, her stats are overridden (slot 15 → 100, 3 → 100, 2 → 360 — the free-roam vandalism behavior package, **INFERRED** decode) and she wanders off smashing. |
| G2b | **The contest (240 s)** | `MissionTimerStart(240)` + dual rolling HUD counters (`CounterEnableRoll("ROLL_JIMMY_COUNTER", "ROLL_ZOE_COUNTER", 25)`; Jimmy's = current, Zoe's = max). Every registered prop break attributes its value to its destroyer (§4). Half-time (≤120 s left): music steps up to `MS_DestructionVandalismHigh.rsm` and Zoe is redirected to the plant room (move → `_5_G5_ZOETOPLANTS`, then path `_5_G5_ZOETOEXIT`, then wander) with a hint line. Timer end → counters hidden, Zoe's destruction stat zeroed and her weapon 418 cleared, she stops → stage 3. |
| G3 | **Scoring scene** | Letterbox, time-up text; Zoe's "you lost"/"you won" bark is chosen by the score (id 34 if player ≤ Zoe, else 35). Fade; both teleported to `_5_G5_ENDPLAYER`/`_5_G5_ENDZOE`; Zoe stationary, emotion-locked toward the player, facing him; two fixed cameras show the two totals (`TextAddParamNum`). **Branch**: `playerTotal <= zoeTotal` → Zoe taunt (id 103) + `ZoeFailure` anim, she walks off to `_5_G5_CENTER`, fail `5_G5_LOST`; else player `Success` anim + camera over Jimmy's shoulder, `M_PASS` + $60, pass music, Zoe's concession (id 21), she walks off, pass. Note the pass/fail decision is `<=` — a **tie loses**. |

### The destruction table (core data)

~190 destroyable props, each `{ name, pedTarget = true, destroyed, destroyer, value }` in dollars: bird baths / columns / terrariums 75; glass carts, large crates, rich chairs, statue busts/horses, warehouse crates 100; vases, plants, lamps, stools, TVs, masks, cardboxes, ship-in-boxes, whisky 25–50; seed bags, asy-tables, small cardboxes 25; `iware_CrateLikr4Galway` 200; and `iware_DPI_Fraffy` — the planted-exotic — alone at **1000**. A second table `tPropsTemp` holds 4 armor-suit anim props @200 registered on hash event 4 (anim-prop break) instead of 3; only these four feed `totalPossibleValue` (=800) and `DamageValueClimbLadderThreshold` (=400) — **both computed but never read** (**VERIFIED** dead code, §6).

Mechanics of the table: names → `ObjectNameToHashID`; `RegisterHashEventHandler` on **create** (event 0) and **break** (event 3/4). On create: if the prop was already broken earlier in the mission it is re-broken on stream-in (`ObjectBreak` — destruction persists across cell streaming), and `pedTarget == false` props are made untargetable to peds. On break: `PAnimDestroyedByPed(ModelPoolIndex, 0)` attributes the kill — player → `playerTotal` (Jimmy's counter), Zoe → `zoeTotal` (Zoe's counter); any other breaker prints a report-bug string. Breaks after the timer are ignored (`bGoToStage3` guard).

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Zoe dies | `PedSetMissionCritical` → `F_MissionCritical` | `5_G5_FAIL_01` — fade+retry if already in stage 2 (`bGoToStage2`), else no-fade retry; cleanup then boots the player to `_5_G5_PLAYERBOOTED` (a point outside the warehouse) |
| Lose the contest (timer ends with `playerTotal <= zoeTotal`) | G3 score branch | `5_G5_LOST` — no fade, retry; the player plays his failure anim under a shoulder camera first |
| Timer expires | — | **not a fail** — it triggers the G3 scoring scene |

No checkpoints; retry replays from G0 (the walk is short). Cleanup: punishment system re-enabled, radar restored, all four warehouse doors unlocked, prop flags 11 cleared, unique-Zoe suppression lifted, hash handlers deregistered, `MINI_React` unloaded.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked (`5-G5`) | `fade` + `letterbox` + `say` |
| CS-2 | G2 interior plan | in-engine skippable chain (5 speech beats, alternating fixed cameras) | `skippable_scene_chain` |
| CS-3 | G3 scoring | in-engine (two fixed cameras, param-number score texts, branch anims) | `camera` + `completion_banner` + branch on score |
| CS-4 | G3 lose tail | in-engine (shoulder cam + player failure anim) | `fail_taunt_scene` (existing) |

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 48 | Zoe | mission-critical in G1 only for the walk; autonomous vandal companion in G2 (wander + stat overrides); unique model suppressed for the mission |

No other peds — the warehouse is the whole cast. (`AreaClearAllPeds()` on entry.)

### Animation groups / action trees / weapons

`NIS_5_G5` (G2 scene), `MINI_React`, `Act/Conv/5_G5.act` (`NIS_Anims/Zoe_01..03`, `Jimmy_01/02`, `Success`, `ZoeFailure`, `Empty`, `Failure`). Weapon models requested: 418 (Zoe's smasher, cleared at time-up), 300 (bat, spawned ×2 under two names), 323 (2x4), `_W_CRUTCH`, 324 (sledge — see dead code). Music: `MS_DestructionVandalismMid.rsm` → `…High.rsm` at half-time; pass/fail stingers.

### Speech

`M_5_G5` scripted ids: 1 (G1 bark), 8, 9, 16, 17, 10 (G2 NIS), 34/35 (time-up verdict), 15 (player), 103 (Zoe wins), 21 (Zoe loses) — **11 scripted lines** total.

### Named world data to extract from `5_G5.dat`

- POINTLISTs: `_5_G5_ZOE`, `_5_G5_WHFRONTDOOR`, `_5_G5_WHBACKDOOR`, `_5_G5_ZOE_WH`, `_5_G5_PLAYER_WH`, `_5_G5_START`, `_5_G5_SPAWNBAT`, `_5_G5_SPAWN2X4`, `_5_G5_SPAWNWRENCH`, `_5_G5_SLEDGE01..04`, `_5_G5_ZOETOPLANTS`, `_5_G5_CENTER`, `_5_G5_ENDPLAYER`, `_5_G5_ENDZOE`, `_5_G5_PLAYERBOOTED`.
- PATHs: `_5_G5_ZOE2WAREHOUSE`, `_5_G5_ZOETOEXIT`, `_5_G5_ZOE_TO_STAIRS2/3/4`, `_5_G5_ZOEGODOWNSTAIRS` (the stairs paths feed dead helper `F_SendZoeToStairsBase`, §6).
- TRIGGERs: `_5_G5_WAREDOORZOE`, `_5_G5_WHFRONTDOOR`, `_5_G5_WHBACKDOOR`, `_5_G5_ZONE01..04`, `_DT_INDOOR_WHOUSEFRONT`, `_DT_INDOOR_WHOUSEROOF`.
- Doors: `DT_INDOOR_WHOUSEFRONT`, `DT_INDOOR_WHOUSEROOF`, `DT_whouse_front`, `DT_whouse_roof`.
- Hardcoded: G2 NIS cameras (around (−666..−671, −162, 1.2)); G3 cameras (−609.7, −160.1, 1.4) / (−610.1, −160.1, 1.6) / (−608.2, −162.0, 1.3); the ~190-prop destruction table itself is in-file Lua (names + values), not in the DAT.
- Area codes seen: **0** (walk) → **54** (warehouse interior).
- **Trigger data (DAT): YES** — `DATLoad("5_G5.DAT", 2)`. Original lives in `Trigger.img` per `trigger-format.md`; the research clone is Lua-only. Extraction pending; not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `skippable_scene_chain` (G2 NIS), `mission_critical_npc` (G1 Zoe) + `mission_ko_point_override` (the `_5_G5_PLAYERBOOTED` eject on her death), `door_lock_set` + `prop_set_state` (four doors + flag 11), `unique_ped_suppress` (model 48), `pickup_spawner` (weapon pickups, `PermanentMission`), `npc_stat_override` (Zoe's vandalism stats), `countdown_fail_timer` schema `timer` (the 240 s clock — here expiring is a *scene trigger*, not a fail), `interactive_music_stream` (mid → high at half-time), `weighted_destruction_meter` (2_S02 / 3_XM — the per-prop dollar table), `npc_follow_path` + `npc_wander` equivalents, `fail_taunt_scene` (G3 lose tail), `completion_banner`, `reward` (schema, $60), `clear_ambient_peds`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| G2b/G3 two-sided scoring | `weighted_destruction_meter` (one side only) | **rival_score_race**: per-prop destruction values attributed to player vs companion (`PAnimDestroyedByPed`), dual rolling HUD counters (`CounterEnableRoll`), end-of-timer comparison driving a win/lose scene branch (tie loses) |
| G2 Zoe free-roam smashing | `npc_attack_prop` (4_03 — single targeted prop) | **npc_autonomous_vandalism**: companion NPC wanders the arena and destroys weighted props on her own agenda (wander + destruction-stat package), so the rival total grows without scripting individual targets |
| G2b persistence | — | **destroyed_prop_streaming_reapply**: a prop broken before its cell streamed out is re-broken when it streams back in (hash create-event → `ObjectBreak`), keeping the arena state coherent for the whole timer |
| G2 punishment off | `punishment_points_add` (add only) | **punishment_system_toggle**: master on/off for the punishment system (`DisablePunishmentSystem`) around a law-free arena |
| G1 walk | `escort_follow` | **npc_tether_to_ped** (small): the follow is a hard 12 m tether to the player rather than a stop-radius follow; a `tether` parameter on `escort_follow` would cover it |

Genuinely NEW names introduced here: **rival_score_race**, **npc_autonomous_vandalism**, **destroyed_prop_streaming_reapply**, **punishment_system_toggle** (plus the `npc_tether_to_ped` parameter note).

## 6. Open questions

- `DamageValueClimbLadderThreshold` (half the armor-suit total, 400) is computed and never used — a cut "climb to the upper shelves once X damage is done" gate, or leftover tuning? The walkthrough mentions the best loot is upstairs; no ladder gate exists in-file. **OPEN**.
- `F_SpawnSledge` (four sledgehammer pickups 324) and `F_CleanupSledge` are written but never called — sledges were cut from the shipped mission (only bat/2x4 spawn). **OPEN** — consider restoring as port flavor or document as cut.
- `F_SendZoeToStairsBase` + the four zone-conditional stairs paths (`_5_G5_ZONE01..04`) are dead — a cut "Zoe heads upstairs at half-time" variant; the live half-time beat sends her to the plant room instead. **OPEN**.
- `bFadeOnFail = true` is set on the lose branch but never read (the fail is `MissionFail(false, …)` — no fade). Cosmetic dead flag. **OPEN**.
- The radar is restored (`RadarRestoreMinMax`) in cleanup but never pinned in-file — the pin presumably comes from the DAT or the area-54 transition. Confirm when the DAT is extracted. **OPEN**.
- Zoe's mission-critical flag is dropped at stage 3 (`PedSetMissionCritical(..., false)`) but the G2 smashing phase keeps it armed — can she actually die during the contest (she has 360 of stat-slot-2 health), and is `5_G5_FAIL_01` reachable in practice? Engine-behavior question. **OPEN**.
- `PAnimDestroyedByPed` attributes a break to a ped — how it treats environmental/indirect breaks (explosions, thrown props, Zoe's targets knocked over by physics) decides whether the "unknown destroyer" report path is reachable in production. **OPEN**.
- Whether `pedTarget = false` (never produced by the in-file tables — every entry is `true`) was meant for the armor suits (they live in the separate `tPropsTemp` and skip the flag entirely). **OPEN**.
