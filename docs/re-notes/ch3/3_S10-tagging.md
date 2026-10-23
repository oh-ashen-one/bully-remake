# 3_S10 — Tagging: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_S10.lua` (1060 lines) + `SMissPass.lua` (3_S10 case). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_S10 = Tagging** (catalog #38 "Tagging — unlocks free tagging"). **VERIFIED** by content: spray-can weapon 321, tag prop system (`Act/Props/3_S10_Tags.act`, `TaggingStartPersistentTag`, `TaggingOnlyShowMissionTags`), 29 greaser-territory tag targets with per-tag cash values, the buy-spray stage at the market, the greasers-get-angry outro NIS, respect deltas (preppies +5, greasers −25), and the `3_S10_UNLOCK` banner key + `SMissPass` spray-can crate overrides.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → baked intro `"3-S10"` → stage-loop function pointer (`gCurrentStageLoop`, reassigned per stage — a function-pointer stage machine, same family as the hand-rolled loops in ch2 docs).

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Setup** | `3_S10.DAT`; action trees `Act/Props/3_S10_Tags.act` + `Act/Conv/3_S10.act`; anim groups `W_SprayCan`, `IDLE_BULLY_C`; **money floor**: if the player has < $1.00, set to $1.00 (spray can costs `SPRAY_PAINT_COST` = 100 cents); mission-tags-only mode on. Player starts at `_3_S10_PLAYER_START`. |
| T1 | **Go to the poor area** | Blip at `_3_S10_POORAREABLIP`, objective `3_S10_01`. A **spray refresher** runs every poll: if the player has no spray (item 321) and isn't mid-tag anim, top up 1 ammo. Entering `_3_S10_TUTORIALSTART` starts the 3-tier interactive music (`MS_WildstyleLow/Mid/High`) and advances. |
| T2 | **Tutorial tag** | Objective `3_S10_02`; the tutorial tag prop (`_3_S10_TUT_TAG`) activated; persistent-tag mode on; done-callback registered. `T_TagIt` thread shows the `TAGGINGTUT1` tutorial whenever the player is inside `_3_S10_TUTTRIGGER` (re-arms if they leave). Tagging it sets `shared.bSprayUnlocked = true` and completes the stage. |
| T3 | **Buy spray** | 2 s beat; spray weapon stripped if held; objective `3_S10_06`, blip at `_3_S10_MARKET`. Shop is **area 26**: entering clears the blip; leaving to area 0 with spray in inventory/weapon slot advances. **Money fail**: standing outside with < $1.00, no spray, and not currently shopping → `3_S10_FAIL01`. |
| T4 | **Tag 5 greaser spots** | Objective `3_S10_03`; the 29-tag `gGreaserTags` table activated; blips per tag (short-ranged; altitude blip icon 29 for high spots); counter HUD (spraycan icon, max **5**). Tags activate in four batches by load triggers `_3_S10_A/B/C/DTAGSLOAD` (per-tag `activateTrigger`), each flipping the prop from its invisible node to `Useable`. Each completed tag: counter +1, per-tag `money` (500-20000 cents; see table) added to the reward accumulator; at 2 tags a hint (`3_S10_12`) shows; at 5 the stage passes. A 4 s spray watchdog re-fails the money condition and prints a buy-more reminder (`3_S10_07`) when out of cans. |
| T5 | **Pass NIS** | Tagging anim finishes; `NIS_GreasersGetAngry`: if indoors, emergency-teleport to `_3_S10_PLAYEREMERGENCYSPOT` first; two greasers (22, 24) spawn at `_3_S10_GREASERS` and play `NISend` nodes with two barks (M_3_S10 ids 4, 3) around the `M_PASS` + `3_S10_UNLOCK` banner (with respect toasts `MRESPECT_PP5` +2-ish preppies / `MRESPECT_GM25` greasers, **INFERRED** signs; the actual deltas apply after: preppies **+5**, greasers **−25**). Then back to free roam, counter cleared, greasers made ambient. Pass also sets the spray-can loot overrides (`DPI_CardBox`, `DPI/DPE_CrateBrk` → spray-can pickup lists) and `MissionSucceed`. |
| T6 | **Fail cleanup** | Fail music; keyed text if any. On cleanup, pass → `F_LoadSprayCans(true)` (world spray-can spawns on, **INFERRED** from name); fail → spray weapon 321 destroyed. `shared.bSprayUnlocked` cleared (nil) either way. |

### Fail conditions

| Cause | Mechanism | Fail key |
|---|---|---|
| Broke and no spray (can't continue) | money < 100 ∧ no item 321 ∧ not shopping (checked in T3 and on a 4 s watchdog in T4) | `3_S10_FAIL01` |
| Left the poor area after starting | once `_3_S10_UNDERBRIDGE` (T2) has been entered, leaving `_3_S10_POORAREA` + `_3_S_10_FAILTRIGGER` grace volume → warn text then fail | `3_S10_FAIL02` |

Note the area fail only arms after passing the under-bridge trigger (`bEnteredNewCoventry`) — walking out before the tutorial tag is harmless (VERIFIED).

### Rewards / pass side-effects

- Accumulated tag money (per-tag 500-20000, min run 5 tags ⇒ at least ~$25-30 typical; exact sum depends on which tags) shown as the `M_PASS` money param.
- Respect: preppies +5, greasers −25 (in-script).
- Free tagging unlocked: `3_S10_UNLOCK` banner + `SMissPass.lua` 3_S10 case re-applies the three spray-can pickup-list overrides (guarded by `shared.bCrateStateSetup`) and tops spray ammo to 10 if empty.

### The tag table (VERIFIED — 29 greaser tags + 1 tutorial)

Fields per tag: prop/trigger id, start node (invisible), active node (`Useable`), tagged variant node (`Tagged/GreaserMed`), `money`, `time` (tag trace difficulty, seconds ×10?, undecoded), `bAltitude` (high spot → blip icon 29), `activateTrigger` (batch A/B/C/D).

| Batch | Count | Money values (cents) | Notes |
|---|---|---|---|
| A (`_3_S10_ATAGSLOAD`) | 8 | 500×5, 500 (tut-adjacent 02/03), 20000 (`_POORAREA_MEDIUM_024`, altitude, "Wonder Meats"-adjacent), 1000×1 | ids `_3_S10_TUT_TAG02/03` + `_POORAREA_MEDIUM_001/004/005/023/024/028` |
| B | 8 | 500×5, 1000×2, 5000×1 (altitude garage) | `_POORAREA_MEDIUM_002/006/007/008/020/021/030` + `_010` |
| C | 8 | 500×5, 1000×2 | `_POORAREA_MEDIUM_013..019, 025` |
| D | 5 | 1000×2, 2000 (altitude), 5000 (altitude drive-through) | `_POORAREA_MEDIUM_022/026/027/029` + `_010` |

(Two high-value outliers: the 20000 "Wonder Meats" tag with time 350, and 10000-listed… — the `_POORAREA_MEDIUM_024` entry is money 20000/time 40/**altitude**, `_028` is 10000/350/altitude. Exact figures VERIFIED in-file.)

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked `"3-S10"` | `fade` + original card |
| CS-2 | T2 tutorial | engine tutorial (`TutorialStart("TAGGINGTUT1")`) | `hint`/tutorial toast while in trigger |
| CS-3 | T5 outro | in-engine (2 greasers, 2 barks, banner) | `spawn_npc` ×2 + `camera` + `say` ×2 + `completion_banner` |

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 22, 24 | two greasers (outro only) | ambient after |

### Props / items

- Weapon/item **321 spray can** (anim group `W_SprayCan`): refresher keeps 1 in stock during T1-T2; shop restock in T3; destroyed on fail.
- 30 tag props (`Act/Props/3_S10_Tags.act`): state graph invisible → Useable → Tagged/<variant>; persistent-tag mode for the open-world leftovers (`TaggingStartPersistentTag` / `TaggingOnlyShowMissionTags`).
- Pickup-list overrides on mailboxes/crates → spray-can loot (world-state change on pass).

### Named world data to extract from `3_S10.DAT` (DAT present, VERIFIED)

- POINTLISTs: `_3_S10_PLAYER_START`, `_3_S10_POORAREABLIP`, `_3_S10_TUTBLIP`, `_3_S10_MARKET`, `_3_S10_GREASERS`, `_3_S10_PLAYEREMERGENCYSPOT`.
- TRIGGERs: `_3_S10_TUTORIALSTART`, `_3_S10_TUT_TAG`, `_3_S10_TUT_TAG02/03`, `_3_S10_TUTTRIGGER`, `_3_S10_UNDERBRIDGE`, `_3_S10_POORAREA`, `_3_S_10_FAILTRIGGER`, `_3_S10_A/B/C/DTAGSLOAD`, `_POORAREA_MEDIUM_001..030` (subset, 27 used).
- Hardcoded outro camera: (506.61, -211.98, 3.23 → 507.35, -211.31, 3.28). Areas: **0** (map) and **26** (market interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| T2/T4 tags | `prop_set_state`, `prop_interact`, `collect_counter` (spraycan icon) | **tag_prop_system** (tag prop state graph invisible→Useable→Tagged-variant; batch activation by area-load triggers; done/fail callbacks into the mission; per-tag money + trace-time + altitude-blip; persistent mode for post-mission free tagging). The underlying trace minigame (`DrawMedTag` anim / tagging input) is engine-side — flagged as a dependency, not spec'd here |
| spray economy | `give_ammo`, `money_ensure` | **consumable_watchdog** (if consumable missing and not mid-use → auto-top-up; plus the 4 s broke-and-dry → fail/reminder poll). Money-side is the pool's `money_floor_fail` |
| area fail | `leave_area` condition | armed-by-progress variant (**leave_area_fail after gate trigger** — the under-bridge latch) |
| shop stage | `interact`/area triggers | **shop_inventory_check** (enter shop area → blip off; exit with item X → advance; exit without → re-blip). Could fold into stages |
| T5 respect | `set_faction_respect` (delta form — pool has set; need **delta variant**: `GetFactionRespect + 5 / − 25`) | **respect_delta** |
| pass world-state | `pickup_table_override` (pool) | reuse ×3 object types |

## 6. Open questions

- The tag trace interaction itself (timing window per tag `time` field, input scheme) lives in the engine/prop tree, not this Lua — the remake needs its own tagging minigame regardless.
- `TaggingOnlyShowMissionTags` / persistent-tag mode semantics for the post-mission free-tagging world state (which non-mission tags exist globally).
- Exact sign on the banner respect toasts vs the applied deltas (banner says `MRESPECT_PP5`/`MRESPECT_GM25`; applied = preppies +5, greasers −25 — consistent).
- Whether tags persist after the mission (props stay tagged via the DAT/world state; `TagsRelease` only removes blips — **INFERRED** persist via `TaggingStopPersistentTag` semantics).
- `gTargetTags`/`cbTargetTagDone`/`MAX_TARGET_TAGS` — dead code (a second tag tier never populated).
