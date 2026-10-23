# 4_03 — Defender of the Castle: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_03.lua` (616 lines, complete) + cross-references in `SMissPass.lua`, `SFaction.lua`, `AreaScripts/MainMap.lua`, `main.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_03 = "Defender of the Castle"** (canonical Chapter 4 list, #49 in `docs/story-mode-catalog.md`, 8AM–11PM) — **VERIFIED**: the [Bully wiki Chapter IV page](https://bully.fandom.com/wiki/Chapter_IV) lists "Defender of the Castle 4-03" by script id; the [Defender of the Castle wiki entry](https://bully.fandom.com/wiki/Defender_of_the_Castle) matches the script (jocks assault the observatory; the nerds man barricades while Jimmy takes the spud cannon; hold two defensive lines and keep the jocks out of the observatory grounds) and the [StrategyWiki ch4 walkthrough](https://strategywiki.org/wiki/Bully/Chapter_4:_A_Healthy_Mind_in_a_Healthy_Body,_and_Other_Lies) confirms the rewards ($25, nerd +20, jock −10) and the unlock requirement (both Paparazzi 4_01 and Funhouse Fun 4_04 complete). Pass block matches: `MinigameSetCompletion("M_PASS", true, 2500)` ($25), messages `MRESPECT_NP20`/`MRESPECT_JM10`, `SetFactionRespect(1, +20)` (nerds) / `SetFactionRespect(2, −10)` (jocks) — same faction decode as 4_01 (1 = nerds, 2 = jocks, 13 = player).

Setting: the observatory and its field — area 0 only, with the ambient population fully cleared (`AreaOverridePopulation(0,…)` + `AreaClearAllPeds()` twice) and four nav radii around the observatory toggled unpathable for the fight (`AreaSetPathableInRadius` ×4, restored in cleanup; the same radius at (1.1, −113, 2.1) is managed by `MainMap.lua`'s walkable-mesh callback so the state survives mission end correctly). Weather forced to clear (`WeatherSet(0)`). Downstream (**VERIFIED** in the clone):

- `SMissPass.lua:224`: on 4_03 pass (first time only, `shared.bCrateStateSetup`), breakable-crate loot tables are overridden — `DPI_CardBox` → `PickupListMailBoxSprayCan`, `DPI_CrateBrk`/`DPE_CrateBrk` → `PickupListCrateSpudGuns` (crates now drop spud guns); `main.lua:942` re-applies the same override on save load. Supersedes the 4_B1 bottle-rocket tables.
- No `ChapterSet` — the 4→5 advance happens at `5_01`.
- `SFaction.lua`: no 4_03-specific attitude entry (4_B2/5_09 flank it in the jock chain).

## 2. Mission flow

Driver: `MissionSetup()` (baked intro "4-03" + DAT + population purge) → `main()` arms `T_FailureConditions` and runs two blocking defense stages: `F_DefendFirstBarricade` → `F_DefendSecondBarricade`. All win/lose adjudication funnels through the `mission_success` global watched by the thread; the stage functions never call `MissionFail`/`MissionSucceed` directly (except the mount-countdown fail at the stage-2 transition). Epic-confrontation interactive music (`MS_EpicConfrontationLow/High.rsm`) rides the stage boundaries.

| # | Beat | Logic (functional) |
|---|---|---|
| D0 | **Setup** | Player placed at `_4_03_PLAYER_START` with weapon 305 (spud gun, per the SMissPass comment) + 25 of ammo-item 316. Melvin (6) spawned untargetable at `_4_03_MELVIN`. Attitude matrix set for the battle: nerds↔player allied (1→13 = 4), jocks hostile to both (2→13 = 0, 2→1 = 0), nerds hostile to jocks (1→2 = 0). Observatory gate opened to the player but locked to peds; the obs-door prop plays its open node. Three destructibles pre-created with damage overrides: barricades `_4_03_BAR_05_01/02` (500), `_DT_OBSERVATORY` (300). POI disabled. |
| D1 | **Man the first barricade** | Objective `4_03_21` + blip on barricade `_4_03_BAR_05_02`, prop health bar shown. Four nerd defenders (7, 5, 9, 8) spawned at cover points (`_4_03_B2_J_*` → cover `_4_03_NERD_D_WAVE1_*`), untargetable, nerd-loyal. The stage arms when the player enters `_4_03_TRIGGER_WAVE1` **or** the barricade is already breached. |
| D2 | **First defense** | Objective `4_03_07`; music goes high-intensity; defenders get weapon 307 with 99 ammo and hold cover forever (`F_TableTakeCover(..., true)`). Jock wave 1: two **cover shooters** (18, 17) with weapon 303 (99 rounds) at cover points, two **chargers** (109, 111) that ignore stimuli *and attacks* and beat directly on the current barricade prop (`PedAttackProp`). Reserve pool = 6. The assault manager (`F_JockAssaultManager`, ~500 ms cadence) respawns each dead jock at its original spawn/pointlist while reserves last (role preserved: cover vs charger), then removes the slot when dry. Stage ends: all jock slots empty → `bDefend1 = "success"`; barricade health hits 0 → defenders+jocks swept, `bDefend1 = "failure"` (**not** a mission fail — the line falls back). Any jock inside `_4_03_OB_GROUNDS` at any manager tick ⇒ mission fail `4_03_JOCKTHRU`. |
| D3 | **Race to the cannon** | Objective `4_03_01` + blip at `_4_03_BLIPCANNONFIRST`; two fresh nerd defenders upstairs; a ring of pickup-362 cans created along `_4_03_FRAFFYCANS` (spud-cannon ammo top-ups, **INFERRED**). **70-second countdown** (`MissionTimerStart(70)`): reach `_4_03_OPEN_DOOR` in time or fail `4_03_FAILREACH` (fade + retry). Then a second wait (same timer still running) for the player to mount the cannon (`/Global/WPCANNON/UseSpudCannon` anim state); timer stops on mount. Objective `4_03_23`. |
| D4 | **Second defense (observatory doors)** | Defenders re-arm with 307s; jock wave 2: three cover shooters (18, 17, 16) + two chargers (109, 111), reserves = 10. Charger target flips to `_DT_OBSERVATORY` on the first manager tick with live jocks (`F_AttackNewTarget`). **Auxiliary wave**: if line 1 fell (`bDefend1 == "failure"`), 18 s into D4 the script smashes the field gate itself (four `PAnimApplyDamage("scFieldObsGate", …)` hits at two hardcoded points, 9000 each) and spawns 4 extra jocks (2 cover, 2 charger) from the aux points — the flank the broken gate implies. Win: jock table empty → `bDefend3 = "success"`. Lose: `_DT_OBSERVATORY` health 0 → `mission_success = FAILURE` (generic), or any jock in `_4_03_OB_GROUNDS` → `4_03_JOCKTHRU`. |
| D5 | **Player containment** (`F_RunAwayCheck`, every thread tick) | Player outside the stage's escape trigger (`_4_03_JOCK_PAST_WATER` in D1–D2, `_4_03_OPEN_DOOR` in D4) → a 0.3 s nag line; also outside the final leave trigger (`_4_03_LEAVE1` / `_4_03_LEAVE2`) → fail `4_03_LEFTOBS`. Two-tier: inner zone warns, outer zone fails. |
| D6 | **Pass** (`T_FailureConditions` → `F_NISSucceed`) | Player dismounted from the cannon (anim node), letterbox, invulnerable, jock/nerd tables swept, `MINI_React` loaded; player teleported to `_4_03_NIS_PLAYER` and plays a celebration node under a fixed camera; `M_PASS` + $25 + respect messages; music; then the pass-side effects (crate loot-table override, nerds +20 / jocks −10) and `MissionSucceed(false, false, false)`. |
| D7 | **Fail NIS (jock break-through only)** | `4_03_JOCKTHRU` gets a bespoke fail vignette (`F_NISFail`): fade, cannon dismount, four jocks (110, 111, 109, 111) + five beaten nerds (Thad 7, Bucky 8, Donald 11, Cornelius 9, Earnest 10) spawned at 0.2 health; the jocks chase/attack them across two camera setups with `FIGHT_WATCH` barks while the `M_FAIL` banner (`4_03_JOCKTHRU`) shows; then `MissionFail(false, false)`. Other fails skip the vignette (`MissionFail(true, true, reason)` — fade + retry). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Jock enters the observatory grounds | `F_JockAssaultManager` trigger check | `4_03_JOCKTHRU` (+ fail NIS, D7) |
| Observatory door prop destroyed | D4 health poll | generic (`mission_success = FAILURE`, keyed text falls through to the `szFailReason` path — set only by the other two fails; door-death shows no key, **VERIFIED** quirk) |
| 70 s countdown expires before reaching the cannon | `MissionTimerHasFinished` in D3 | `4_03_FAILREACH` |
| Player abandons the field | D5 two-tier check | `4_03_LEFTOBS` |

First-barricade loss is deliberately **not** a fail (D2 → fallback, plus it arms the D4 auxiliary flank). Retry: `MissionFail(true, true, …)` (fade + retry) except the break-through NIS path (`MissionFail(false, false)`). No checkpoints: replay from D0. On failure cleanup additionally calls `AreaEnsureSpecialEntitiesAreCreatedWithOverride("1_03", 1)` — reusing chapter-1's special-entity set id (**OPEN**: likely a copy-paste that happens to restore shared map dressing; flagged for the port).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked (`4-03`, extra args `true, true, true`) | `fade` + `letterbox` + `say` |
| CS-2 | D6 celebration | in-engine (fixed camera, one player anim, banner) | `camera` + `ped_action_node` + `completion_banner` |
| CS-3 | D7 break-through fail | in-engine (spawned brawl vignette, 2 cameras, barks) | `fail_taunt_scene` (existing) + `spawn_wave`-style stand-ins |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 6 | Melvin | pre-placed untargetable set dressing |
| 7, 5, 9, 8 | nerd defenders (line 1 ×4, line 2 ×2) | weapon 307 ×99, permanent cover |
| 18, 17, 16 | jock cover shooters | weapon 303 ×99, `PedCoverSet` raw params |
| 109, 111 | jock chargers | `PedAttackProp` on barricade/door; ignore stimuli + attacks |
| 110, 111, 109 | fail-NIS jocks | spawned only for D7 |
| 7, 8, 11, 9, 10 | fail-NIS nerds (Thad, Bucky, Donald, Cornelius, Earnest) | 0.2 health, flee |

### Animation groups / action trees

`F_JOCKS`, `DodgeBall` (loaded… never referenced in-file beyond load/cleanup — **OPEN**, likely charger anim deps), `MINI_React` (D6). Trees: `Act/Conv/4_03.act` (`Anims/Celebrate`), `Act/Props/OBSDoor.act` (`PlayOpenAnim`), `Act/Props/WPCANNON.act` (`Disengage/null`, mount state query).

### Props / pickups / weapons

- Destructibles: `_4_03_BAR_05_01`, `_4_03_BAR_05_02` (barricades, damage override 500; only _02 is actually attacked in-file — _01 is created but unused, **OPEN**), `_DT_OBSERVATORY` (override 300), `scFieldObsGate` (script-killed aux flank, 4×9000 at (0.88, −111.46, 2.00) / (0.82, −113.95, 2.06)).
- Doors/gates: `_SCGATE_OBSERVATORY` (open to player, locked to peds), `_4_03_OBSDOOR` open node.
- Player kit: weapon 305 + 25 of ammo 316 at start; pickup 362 ring at `_4_03_FRAFFYCANS`.
- Defender weapon 307; jock weapon 303. Pickup models loaded: 316, 362; weapon models: 305, 307, 303.
- The cannon: `_SCHOOL_TURRET` blipped for the mount beat (the world prop whose existence is gated on 4_02 — see `4_02-stronghold-assault.md` §1).

### Speech

`M_4_03` scripted ids: 2, 3, 11, 13 (played by-model via `F_PlaySpeechOnModel`); `M_4_03_2D` ids 8, 16, 17; ambient `FIGHT_WATCH` in the fail NIS — **7 scripted lines** total. Music: `MS_EpicConfrontationLow/High.rsm` locked interactive; pass/fail stingers.

### Named world data to extract from `4_03.dat`

- POINTLISTs: `_4_03_PLAYER_START`, `_4_03_MELVIN`, `_4_03_NERD_D2`, `_4_03_B2_J_P1/L1/L2/L3`, `_4_03_NERD_D_WAVE1_02/03/04/06`, `_4_03_JOCK_SPAWN_GOTO_P1/L1/L2/L3`, `_4_03_JOCK_P1`, `_4_03_JOCK_P2`, `_4_03_N_UD_1`, `_4_03_N_UD_2`, `_4_03_FRAFFYCANS` (N), `_4_03_BLIPCANNONFIRST`, `_4_03_SPAWNTWO1..5`, `_4_03_JOCK_B2_2W_P1_2`, `_4_03_B2_J_P1_2`, `_4_03_B2_J_P2_2`, `_4_03_JOCK_B2_2W_L2`, `_4_03_JOCK_B2_2W_P1`, `_4_03_AUXCOVER2`, `_4_03_AUXCOVER3`, `_4_03_AUXSPAWN1`, `_4_03_AUXSPAWN2`, `_4_03_CAMFAILJOCKS` (4), `_4_03_NIS_NERDS` (4), `_4_03_NIS_PLAYER`.
- TRIGGERs: `_4_03_BAR_05_01`, `_4_03_BAR_05_02`, `_4_03_OB_BAR2`, `_DT_OBSERVATORY`, `_4_03_OBSDOOR`, `_SCGATE_OBSERVATORY`, `_SCHOOL_TURRET`, `_4_03_TRIGGER_WAVE1`, `_4_03_JOCK_PAST_WATER`, `_4_03_OPEN_DOOR`, `_4_03_LEAVE1`, `_4_03_LEAVE2`, `_4_03_OB_GROUNDS`.
- Hardcoded: nav-radius toggles (19.8, −127.1, 2.8) / (18.2, −116.7, 3) / (19, −141.1, 2.8) / (8.7, −139.7, 2.8) + (1.1, −113, 2.1); gate-damage points ×2; fail cameras (23.86, −137.63, 3.82) / (50.31, −148.11, 5.38); success camera (29.67, −135.60, 8.64).
- Area codes seen: **0 only**.
- **Trigger data (DAT): YES** — `DATLoad("4_03.DAT", 2)`; original `4_03.dat` lives in `Trigger.img` per `trigger-format.md` (TCRF notes it also carries unused extra nerd/jock/Earnest/Melvin spawns), but the research clone is Lua-only. Extraction pending; not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `wave_group` + `count_threshold` (schema `spawn_wave`), `npc_cover_behavior` + `cover_point_register` (2_03 — though here cover comes from raw `PedCoverSet` params, not profile files), `prop_health_bar` (3_XM — barricade + door bars), `door_barricade_prop` / `door_prop_control`, `countdown_fail_timer` (the 70 s cannon dash), `stay_in_zone_fail` / `zone_exit_countdown` family (D5's warn/fail tiers), `give_weapon` + `give_ammo` (schema), `weather_force` (3_XM), `nav_block_radius` (existing — the four radius toggles), `population_override` + `clear_ambient_peds`, `faction_attitude_matrix_override`, `interactive_music_stream`, `dynamic_blip`, `fail_taunt_scene` (D7), `pickup_table_override` (3_S10 — crate loot swap on pass), `set_faction_respect` / `respect_delta`, `completion_banner`, `player_action_state` (cannon mount), `mounted_turret_mode` (4_02 — same WPCANNON state query + scripted dismount). `npc_respawn_fresh` (ch2 2_05 area) is the respawn half of the reserve system.

| Beat | Covered by | New primitives needed |
|---|---|---|
| D2/D4 chargers | — | **npc_attack_prop**: melee NPC whose target is a destructible prop (`PedAttackProp`), ignoring player aggro (`PedIgnoreAttacks`); retargetable mid-fight (`F_AttackNewTarget`) |
| D2/D4 reserves | `continuous_wave_spawner` (3_R09), `npc_respawn_fresh` | **wave_reserves**: bounded reinforcement budget per wave (dead member respawns in-role at its original spawn while budget > 0; slot removed when dry). A budget parameter on the continuous spawner would cover it |
| D2/D4 jock-in-grounds fail | `spotted_in_zone_fail` (shape only) | **npc_zone_breach_fail**: fail when any member of a hostile group enters a trigger (the inverse of the player-side `stay_in_zone_fail`) |
| D2→D4 fallback | — | **defense_line_fallback**: destruction of defense objective 1 is a *branch*, not a fail — sweep survivors, retarget attackers to line 2, optionally arm a timed auxiliary flank (D4's 18 s gate-burst + aux spawns) |
| D3 mount dash | `countdown_fail_timer`, `player_mounted` | none — `timer` + `player_action_state` composition (the same timer spans the reach-trigger *and* the mount wait; just note the reuse) |
| D5 warn/fail tiers | `stay_in_zone_fail` | none — two concentric triggers (inner = nag, outer = fail); parameterize the existing zone family |

Genuinely NEW names introduced here: **npc_attack_prop**, **wave_reserves**, **npc_zone_breach_fail**, **defense_line_fallback**. (`AreaEnsureSpecialEntitiesAreCreatedWithOverride("1_03", 1)` on fail cleanup is an engine-call note, not a mission primitive.)

## 6. Open questions

- `_4_03_BAR_05_01` is created and damage-overridden but never attacked or health-checked in-file — second physical barricade that lost its script beat, or the visual half of a pair? **OPEN** (DAT will show placement).
- Door-death fail (`_DT_OBSERVATORY` → 0) sets `mission_success = FAILURE` without a fail key, so the banner shows no reason line, unlike the other two fails — intended or bug? **OPEN**.
- `F_AnyJocksAlive` (an infinite-loop helper that can never return) and `F_DestroyCurrentTarget` / `F_GetToSecondBarricade` are dead code — cut alternate fail/timer logic. **OPEN**.
- The `"1_03"` special-entity reload on failure cleanup — cross-chapter id reuse; whether it's a deliberate shared-dressing restore or a copy-paste bug. **OPEN**.
- Whether the 0.2-health fail-NIS nerds can actually die and break the vignette — the fail path deletes them only after the banner; engine invulnerability assumptions **OPEN**.
- `DodgeBall` anim group loaded but never referenced — charger-anim dependency or leftover. **OPEN**.
- Nerd defender weapons are re-issued at D2/D4 with 99 ammo — infinite in practice; whether defenders can be killed (they're untargetable but not invulnerable) and what that does to the battle — **OPEN**, engine-behavior question.
