# 4_05 — Nice Outfit: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_05.lua` (1591 lines, complete). No `SMissPass.lua` branch exists for `4_05` (**VERIFIED**, SMissPass read) — the wardrobe grant happens in `MissionCleanup`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_05 = "Nice Outfit"** (canonical Chapter 4 list, mission #50, 8AM–7PM) — **VERIFIED** by content: the football-field mascot (model 88, `NPC_Mascot` anim group, own AI tree `Act/AI/AI_MASCOT_4_05.act`) is harassed until enraged, chased to the school pool, beaten in a boss fight, and the player steals the mascot costume (`ClothingSetPlayerOutfit("Mascot")` + cleanup-time `ClothingGivePlayerOutfit("Mascot")`). Cross-checked against the [Bully fandom wiki Chapter IV page](https://bully.fandom.com/wiki/Chapter_IV) ("Nice Outfit 4-05") and fan 100% guides (mascot outfit unlocked by this mission). Catalog cross-check: `docs/story-mode-catalog.md` #50.

Two DAT files: `4_05.DAT` (live mission) + `4_05cut.DAT` (the two cutscene peds/areas) — both `DATLoad`ed, **flagged, not extracted**.

Dead/loose ends in the file: `F_CleanupNerdAmbush` (references `pedCornelius`/`pedAlgie`/`pedBucky`/`pedDonald` which are never created on any live path — a cut mid-field nerd-ambush beat, **INFERRED**; the surviving "nerd ambush" is the *cutscene* `T_CutsceneNerdAmbush` with `pedCutThad`/`pedCutBucky`/`pedCutCornelius`), `FightingPattern` consumers live in the AI tree (external), `bHitMascotInPOI4`/`bHitMascotInPOI5` are initialized `true` so the POI-4/POI-5 hit reactions in `F_CheckMascotStatus` can never fire (**INFERRED** bug or deliberately disabled reactions), `cbPlayerKneel` (defined, never registered), debug entries `F_StartAtStage2/3`.

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → `F_Stage1()` → `F_Stage2()` → `F_Stage3()`, each stage = setup + poll loop over `StageN_Objectives()`; stage 1 runs three service threads (`T_MonitorMascotField`, `T_MonitorMascotForHits`, `T_MonitorFieldTriggers`) plus `T_MissionTextQueue`. No checkpoints: fail replays from the top.

| # | Beat | Logic (functional) |
|---|---|---|
| N0 | **Baked cutscene "4-05"** | `PlayCutsceneWithLoad("4-05", true)` in `MissionSetup`; fight music streams armed (`MS_FightMid02` + high layer); both DATs loaded. Weapon models 341/305/301 requested (341's user never appears in-file — **OPEN**; 301/305 are the ambush-cutscene nerd weapons). |
| N1 | **Stage 1 setup: the field tableau** | Player transitioned to `_4_05_SPAWNPLAYER` (area 0). The mascot spawns at `_4_05_SPAWNMASCOT` with **3× health, mission-critical** (KO here = fail — he must be annoyed, not beaten), ignore-stimuli, infinite sprint, emotion-toward-player 1, social action 35 against him disabled, stat overrides (stat 6 → 0, stat 38 → 100), flag 117 cleared, custom AI tree. Around him: Burton (55) watching, a push-up jock (111, looping anim), a smoking jock (16), a sprinting jock (206, looping `PATH._4_05_LAPCLOCKWISE`), two cheerleaders (182/181, cheer routine). Three of the jocks carry a drop item 362 (health pickup, **INFERRED** from the pool-stage `HealthBute` pickup of the same id) on KO. Four gym/pool doors locked. The **aggro meter** shows: a 0/36 HUD counter-meter with a happy-mascot icon pair. Objective `4_05_MOBJ_01`. A taunt callback is registered against the mascot (social action 28 → `F_PlayerTauntedMascot`). |
| N2 | **Stage 1: annoy the mascot (aggro loop)** | The mascot cycles a **performance POI loop**: idle → pick a random un-dispersed POI point (never the same twice) from `_4_05_MASCOTPOI` (5 elements) → walk there → perform (per POI: dance 2–5 random moves for the cheerleaders / push-ups or chat / chat with the smoker / dance or practice-charge facing a heading / sit-and-dance variant) → idle. Ambient dressing reacts: cheerleaders stop cheering while the mascot is at POI 1, the push-up jock stops while he's at POI 2. **Every hit on the mascot (or a taunt)** does *not* damage him: his health is reset to full, his current performance is cancelled, he retreats to POI point 2, the counter +1, and a reinforcement jock spawns (every ≥20 s, cap 3, models 17/109/204, alternating 362 drop) who attacks the player only if the player is still inside `_4_05_FIELD`. Stink bombs (weapon 309) count too, with a 5 s re-arm window. **POI dispersal**: hitting the mascot while he stands in POI trigger 1/2/3 permanently removes that POI from his rotation and scatters its crowd — cheerleaders go ambient and flee (POI 1); Burton goes ambient and the push-up jock attacks unless the player is inside a stealth prop (POI 2); the smoker attacks with the same stealth check (POI 3). Meter thresholds: 12 → neutral icon + bark (id 52); 24 → angry icon + bark (id 44); **36 → stage 2** (unless the mascot is mid-grapple anim). Warn/fail rings: `_4_05_WARNPATH/WARNMAIN/WARNALLEY` spam a leave-warning line; `_4_05_FAILPATH/FAILMAIN/FAILALLEY` fail the mission (`4_05_FAIL_01`). |
| N3 | **Stage 2: the chase (two-part skippable in-engine cutscene)** | Meter full → `F_cutMascotEnraged`: control off, widescreen, extra jocks dismissed to ambient, fixed camera pair on the mascot; the mascot plays his enraged beat (speech id 57). Fade; the field is rebuilt with cutscene peds — mascot + Juri (20), Bo (18), Casey (17), Dan (111), Kirby (109), all weapons cleared, infinite sprint — player placed at `_4_05CUT_SPAWNPLAYER`. `T_CutsceneMascotAngry`: jock speeches 40/39/13/45 with per-line action nodes; then the player flees along `PATH._4_05CUT_JIMMYFLEE` and the mascot chases along `_4_05CUT_MASCOTCHASE` while the four jocks peel onto chase paths. Second half: a fake pool door PAnim is swapped in, the nerd trio Thad (7), Bucky (8), Cornelius (9) spawn stationary + invulnerable with weapons 301/301/305 and stat bundles, the player + mascot path to the pool (`_4_05CUT_JIMMYTOPOOL` — the player opens the fake door at node 6), and `T_CutsceneNerdAmbush` has the nerds ambush the pursuing jocks (target locks, taunt anims at path node 5, speeches 8/15/17/16). Both halves skippable with button 7 (`bSkipFirstCutscene`/`bSkipSecondCutscene`). Player flag 108 set for the duration. |
| N4 | **Stage 3: pool boss fight** | Area transition to 13 (pool), punishment system disabled, POI off, peds cleared, population override zeroed. The mascot respawns at `_4_05_SPAWNMASCOTPOOL` with **1.5× max health**, damage-taken ×0.1 on channels 0 and 3, a blip, and a health-bar (`4_05_HEALTHBAR`). A health pickup (362, `HealthBute` callback) spawns at `_4_05_SPAWNHEALTHPOOL`. Intro NIS beats: mascot pool-line (id 54) under a fixed camera, player line (id 60), weapon HUD locked, mascot invulnerable for a scripted jump to `_4_05_MASCOTJUMP` (speech 58), then control returns, his AI tree attaches and he attacks. `T_MonitorMascotPool`: at half health he barks (id 58) and `gFightPattern` flips to 1 — read by his AI tree via the exported `FightingPattern()` (**INFERRED** — the tree is external, same pattern as ch3's `tree_condition_function`). |
| N5 | **Costume steal + pass** | Mascot KO → `F_CutStealCostume`: fade, the mascot is deleted and a stand-in (model 88) plays knocked-out at `_4_05CUT_POOLCONST`, fixed camera, player's line (id 47); fade to black, **`ClothingSetPlayerOutfit("Mascot")` + `ClothingBuildPlayer`**; the stand-in is replaced by a stripped kid (model 235, knocked-out anim), the player dances, whine bark. Completion banner `M_PASS` (tag `4_05_MPASS01`) + respect messages `MRESPECT_NP5`/`MRESPECT_JM10`, pass stinger 4, **nerds +5 / jocks −10 respect**, `MissionSucceed(false,false,false)`. Cleanup additionally grants the outfit permanently (`ClothingGivePlayerOutfit("Mascot")`), moves the player to `_4_05CUT_ENDPLAYER`, restores population, and stages a leftover ambient jock-vs-nerd brawl (`F_setupAmbientBrawl`, speeches incl. id 14) whose peds all go ambient. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player crosses any fail ring (leaves the field fight area) | `Stage1_Objectives` trigger poll | `4_05_FAIL_01` |
| Mascot KO'd during stage 1 (before the meter fills) | `PedSetMissionCritical` callback `F_MissionCritical` | `4_05_FAIL_02` |
| Any other failure | generic | `MissionFail(false)` |

No timers, no countdowns. Fail music stinger 4. Retry = full replay; the baked intro has no in-file attempt skip (**OPEN**, engine pattern per ch1).

### Pass-side effects

- In-file: Mascot outfit worn immediately; nerds +5 / jocks −10 respect; banner + respect messages.
- `MissionCleanup` on pass: `ClothingGivePlayerOutfit("Mascot")` (permanent wardrobe unlock) — **VERIFIED**; plus the ambient brawl dressing and the `_4_05CUT_ENDPLAYER` placement.
- No `SMissPass.lua` branch (**VERIFIED**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | N0 intro | baked (`4-05`) | `fade` in at `_4_05_SPAWNPLAYER` + `letterbox` + one `say` beat |
| CS-2 | N3 mascot enraged | in-engine, skippable, ~15 s | `inline_cutscene_chain`: `letterbox` + fixed `camera` pair + `speech_sequence` (4 jock lines) + paired `ped_action_node` |
| CS-3 | N3 field chase + pool ambush | in-engine, skippable, ~25 s, world rebuilt mid-scene | `inline_cutscene_chain` with `player_follow_path` + `npc_follow_path` chase paths + door-prop swap + `npc_pair_date_loop`-style paired taunt locks; nerd ambush = `npc_spawn_walk_then_attack` variant during the scene |
| CS-4 | N4 pool intro | in-engine micro-NIS | fixed `camera` + `say` ×2 + `npc_jump_entry` (1_07) for the mascot jump + **weapon_hud_lock** |
| CS-5 | N5 costume steal | in-engine, ~10 s | `inline_cutscene_chain`: KO stand-in (`ped_action_node`) + `say` + `give_clothing` at black + model-swap stand-in + player dance + `completion_banner` |

Anim groups: `NPC_Mascot`, `NIS_4_05`, `Hang_Workout`, `IDLE_JOCK_A`, `F_Girls`, `Cheer_Cool1`, `Russell` (loaded in setup/debug, no Russell spawn in-file — **INFERRED** shared fight-move set), `Px_Ladr`. Music: `MS_FightMid02/High02`, `MS_ActionBeatBreak.rsm`, `MS_ActionHigh_NISReturn.rsm`; stinger id 4 both ways.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 88 | the Mascot | stage 1: 3× health, mission-critical, invulnerable-in-practice (health reset per hit), performance-POI loop; stage 3 boss: 1.5× max health, damage-taken ×0.1 (channels 0/3), half-health phase switch via `FightingPattern()` |
| 55 | Mr. Burton | field dressing stage 1; goes ambient if POI 2 disperses |
| 111 | push-up jock (Dan in cutscene) | POI 2 reaction: attacks unless player in stealth prop |
| 16 | smoker jock | POI 3 reaction: same stealth-conditional attack |
| 206 | sprint jock | laps the field on `PATH._4_05_LAPCLOCKWISE`; POI 4 reaction dead (see §6) |
| 182, 181 | cheerleaders | cheer loop; flee ambient on POI 1 dispersal or when hit |
| 17, 109, 204 | reinforcement jocks | trickle spawns, 20 s cadence, cap 3, attack only while the player is on the field |
| 20, 18, 17 | cutscene Juri / Bo / Casey | chase-pack NIS actors |
| 111, 109 | cutscene Dan / Kirby | chase-pack NIS actors (Dan/Kirby deleted before the ambush half) |
| 7, 8, 9 | Thad / Bucky / Cornelius (ambush nerds) | stationary, invulnerable, weapons 301/305/301, stat bundle (10 → 100, 11 → 85, 3 → 25); also the ambient post-pass brawl |
| 235 | stripped kid | post-steal stand-in under the mascot costume |

Weapons: 301/305 (nerd cutscene loadout), 309 (stink bomb special case), 362 (health pickup + drop item), 341 (requested, user not found in-file — **OPEN**), 400 not used here.

### Named world data to extract from `4_05.DAT` + `4_05cut.DAT` (present — **flagged, not extracted**)

- POINTLISTs: `_4_05_SPAWNPLAYER`, `_4_05_DEBUGSTAGE2PLAYER`, `_4_05_SPAWNPLAYERPOOL`, `_4_05_SPAWNMASCOT`, `_4_05_SPAWNMASCOTPOOL`, `_4_05_SPAWNBURTON`, `_4_05_SPAWNJOCKPUSHUPS`, `_4_05_SPAWNJOCKSMOKE`, `_4_05_SPAWNJOCKSPRINTS`, `_4_05_SPAWNCHEERLEADERS`, `_4_05_MASCOTPOI` (5), `_4_05_MASCOTJUMP`, `_4_05_SPAWNJOCKS`, `_4_05_SPAWNHEALTHPOOL`, `_4_05CUT_SPAWNMASCOT`, `_4_05CUT_SPAWNJURI/BO/CASEY/DAN/KIRBY/THAD/BUCKY/CORNELIUS`, `_4_05CUT_SPAWNPLAYER`, `_4_05CUT_KIRBYMOVE`, `_4_05CUT_DANMOVE`, `_4_05CUT_POOLJIMMY(/02)`, `_4_05CUT_POOLCONST`, `_4_05CUT_ENDPLAYER`, `_4_05CUT_ENDBO/CASEY/JURI/THAD/CORNELIUS/BUCKY`.
- PATHs: `_4_05_LAPCLOCKWISE`, `_4_05CUT_JIMMYFLEE`, `_4_05CUT_MASCOTCHASE`, `_4_05CUT_JOCKSCHASE(/02/03)`, `_4_05CUT_JIMMYTOPOOL`, `_4_05CUT_JURI`, `_4_05CUT_BO`, `_4_05CUT_CASEY`.
- TRIGGERs: `_4_05_WARNPATH/WARNMAIN/WARNALLEY`, `_4_05_FAILPATH/FAILMAIN/FAILALLEY`, `_4_05_FIELD`, `_4_05_MASCOTPOI1-5`, `_4_05CUT_FAKEDOOR` (prop swap for `_DT_TSCHOOL_POOLL`).
- Doors by name (not DAT): `DT_TSCHOOL_GYML`, `TSCHOOL_GYMR`, `TSCHOOL_POOLR`, `DT_TSCHOOL_POOLL` (locked for the mission, unlocked on cleanup).
- Hardcoded coords in-file: N3/N4/N5 camera pairs (field, pool intro, steal).
- Area codes seen: **0** (field) and **13** (pool interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1/ch2/ch3 set | New primitives needed |
|---|---|---|
| N0/N3/N4 cutscenes | `fade`, `letterbox`, `inline_cutscene_chain` (3_04), `player_follow_path`, `npc_jump_entry` | none new |
| N1 tableau | `spawn_npc` flag bundles, `ped_action_node`, `npc_stat_override`, `unique_ped_suppress` | none new |
| N2 aggro meter | `annoyance_meter` (2_S05 — health-bar-as-meter), `meter_threshold_escalation` (3_XM) are adjacent but neither is this widget | **counter_threshold_meter** (the `Counter*` HUD meter: max N, scripted +1 increments, icon-pair swap at declared thresholds, final threshold = stage advance; distinct from `annoyance_meter` which fills from proximity rates and fails at max — this one *passes* at max) |
| N2 hit = meter, not damage | `ped_hit_event` (2_05) | **hit_feeds_meter_no_damage** (target NPC's health is restored on every registered hit; the hit instead increments a counter and cancels the NPC's current behavior into a flee-to-point; could fold into `counter_threshold_meter` as its hit source) |
| N2 taunt counts | `npc_insult_event` (2_06), `social_interact` (1_02B) | **social_action_callback** (register a mission callback for one social action id performed on one specific ped — `PlayerRegisterSocialCallbackVsPed`; small, data over the social system) |
| N2 performance POI loop | `npc_sit_at_poi` (2_G2), `npc_patrol_cycle` (2_08), `crowd_ring` (1_B) | **npc_performance_poi_loop** (performer NPC cycles a random non-repeating POI list with per-POI performance sets; ambient reactors (cheerleaders/push-up jock) toggle with his presence; hitting him inside a POI permanently disperses that POI — crowd flees or guards attack, with a `PlayerIsInStealthProp` conditional that turns the reaction ambient) |
| N2 reinforcement trickle | `continuous_wave_spawner` (2_07) | none new (time-gated single spawn, cap 3, alternating drop item = data on the spawner) |
| N2 warn/fail rings | `leave_area_fail` (2_07), schema `trigger_enter` fail | none new (warn ring = `hint` on `trigger_enter`, re-shown while inside) |
| N3 chase scene | `inline_cutscene_chain` | **door_prop_swap** (delete a real door PAnim, create a same-named fake for the scene, restore on cleanup — variant of `door_prop_control` 2_05) |
| N4 boss fight | `boss_profile` (2_B), `damage_taken_multiplier` (3_04), `model_swap_on_health_threshold`-adjacent phase switch, `tree_condition_function` / `lua_condition_query_from_tree` (3_B) for `FightingPattern()` | none new |
| N4 HUD bits | — | **weapon_hud_lock** (`PlayerWeaponHudLock` — hide/lock the weapon wheel during a scripted beat; small action) |
| N5 costume steal | `give_clothing` (2_03), `temp_outfit_swap` (3_01A) | none new (KO stand-in + build-player = `ped_action_node` + `give_clothing` sequencing) |
| fails | `mission_critical_npc`, trigger-ring fail | none new |

## 6. Open questions

- Weapon id 341 (requested in setup, never used in-file) and the exact identity of 362 (health pickup — strong contextual read from `HealthBute`, **INFERRED**). Weapon 305 here is the nerd cutscene weapon; SMissPass calls 305 the spud-gun-class unlock for 4_02, so 305 = spud gun (**INFERRED**, consistent).
- `PedOverrideStat` slots 6/38 (mascot field setup) and 10/11/3 (ambush nerds) — engine stat numbers, same standing OPEN as other chapters.
- `PedSetFlag` 117 (mascot) and 108 (player during the chase scene) semantics.
- POI 4/5 hit reactions are unreachable as shipped (`bHitMascotInPOI4/5` start `true`); whether the sprint-jock revenge beat was deliberately disabled or is a bug (**OPEN**).
- `Russell` anim group loaded but no Russell spawn — shared wrestling-move set for the mascot boss, **INFERRED**.
- The aggro counter's exact increment on taunts vs. hits (both +1 as read); whether grapple moves bypass the meter's stage-advance guard intentionally.
- Whether retry skips baked `4-05` (no attempt check in-file); debug stage entries suggest the designers jumped straight to 2/3.
- All trigger geometry (warn/fail rings, POI rings, field bounds) and both DATs' point/path layouts (pending extraction).
