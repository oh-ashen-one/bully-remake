# 6_B — Final Showdown (Gary, school roof): recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap6/6_B.lua` (968 lines, complete). No `SMissPass.lua` branch (**VERIFIED**) — all pass effects are in-file, and the chapter transition lives in the chained `Chapt5Trans` (see §7). Structure in our own words; script/native/string-key names are citations only.

## 1. Identification

**6_B = "Final Showdown"** — the last story mission of the game: the rooftop pursuit of Gary across the school roof (planks, ladders, wheelbarrow brick dumps, dropping bells), the push onto the scaffolding, the boss fight on the collapsing scaffold, and the crash through the glass skylight into the principal's office. **VERIFIED** by content and by walkthrough cross-check ([StrategyWiki Ch.5](https://strategywiki.org/wiki/Bully/Chapter_5:_The_Fall_and_Rise_of_Jimmy_Hopkins,_Aged_15): bricks thrown on the plank run, wheelbarrow dumps at both ladders, three bell groups dropping in left/left+middle/two-right patterns, push onto the scaffold, then the office finale). Catalog: folded into #65's note ("final boss: Gary").

Retry semantics (**VERIFIED**): `shared.b602reachedScaffolds` is set when stage 2 begins; on a failed attempt the next run reads it in `MissionSetup`/`main` and jumps straight to stage 2, playing the alternate baked cutscene `"6-BB"` instead of `"6-B"`. This is a **mid-mission checkpoint that survives mission restart** — the flag is never cleared in-file (cleared externally, presumably on pass/cleanup of the chain — **OPEN** where).

Dead/loose ends (**VERIFIED** in-file): `F_Intro`, `T_MissionComplete`, and `bStartedCompletionThread` are unused; `CB_GARYPATH03` faces Gary at path node 9; the `Two`/`Three` globals in `F_ResetPAnims` assign undeclared names (Lua globals, harmless); `T_GaryBlahBlah`'s `TalkFive` fires on the second bell trigger. Who calls `F_ScaffoldBreak` (the `Break` counter increment that drives the scaffold collapses) is **not** in this file — it's invoked from Gary's AI/action tree (the file exports tree-facing hooks: `F_GaryAngry`, `F_DodgeCondition`, `F_WhatScaffoldAreWeOn`, `F_SecondStage`, `F_NISDoorOpening`-style 0/1 query functions; **VERIFIED** the pattern, **OPEN** the exact tree nodes).

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → stage 1 (`F_Stage1`, roof pursuit) or straight to stage 2 (`F_Stage2`, scaffold boss) on the checkpoint flag → pass block. Long-lived threads: `T_CheckPlayerHealth`, `T_GaryBlahBlah`, `T_RoofCheck`, `T_GaryUpdater` (re-spawned per stage), `T_GaryThrowActions`, `T_CollisionApply`, `T_ScaffoldBreak`, `T_AngryHit`, ladder monitors. No in-stage checkpoints besides the stage-2 flag.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup + baked cutscene** | Clock forced to 19:00, weather forced (`WeatherSet(2, true)` — storm, per the mood; id semantics **OPEN**); two DATs loaded (`6_Ba.DAT` + `6_B.DAT`); punishment off, non-mission peds off; player disarmed + weapon inventory snapshotted; player flag 58 set (knockback/edge behavior, **OPEN** semantics); Gary spawned on the roof (130) with Nemesis anim tree, invulnerable-adjacent (`PedSetMinHealth` at full + mission-critical with a fail callback), ignore-stimuli, blip, taunt schedule armed; 5 steam effects placed; bell/wheelbarrow PAnims reset; cutscene `"6-B"` (or `"6-BB"` on retry). Music `MS_FinalShowdown03Low`. |
| B1 | **Roof pursuit, trigger-gated stages 0→5** | Sequential triggers gate Gary's retreat: `_6_B_GARY_TRIGGER_1` (Gary goes invulnerable + dodge-capable, climbs ladder 1 — `PedClimbLadder`, drops a projectile-immunity flag mid-climb); at `_6_B_LADDER1_BEGIN` with the player in the climb-on anim, a **wheelbarrow dump** plays: camera looks at the dump point along a path, Gary plays `WheelBarrowInteract`, the wheelbarrow geometry gets collision applied, the struggle-button HUD widget (component 17) shows briefly, ladder monitor resets the camera if the player bails early; `_GARY_TRIGGER_3/5` repeat the invulnerable-retreat pattern up paths 01/02 with warp-out at trigger 4; `_6_B_LADDER2_BEGIN` = second wheelbarrow dump (same machinery). Between gates Gary **throws bricks**: `T_GaryThrowActions` locks the player, runs the throw tree to its release node, then crouches 5 s; 50% chance of a taunt when not already speaking. |
| B2 | **Bell gauntlet** | `_6_B_GARY_TRIGGER_6` starts three bell sound emitters; passing under each bell group (`_6_B_BELLS_ONE/TWO/THREE`) fires that bell's `DoDamage` PAnim node + applies collision geometry in sequence (1 bell, then 2, then 2 — matching the documented drop pattern) and silences its emitter. Gary taunt beats run off the same triggers via `T_GaryBlahBlah`. |
| B3 | **The push** | Player reaches `_6_B_SCHOOLROOFTOP` → control off, Gary blip removed, baked cutscene `"6-BB"` (the verbal sparring + push over the edge), stage 2 begins: checkpoint flag set, stage-1 Gary deleted, **boss Gary** spawned at `_6_B_GARYPOSITION` with 400 hp, Nemesis anim tree + `GaryAI` AI tree, both actors play a knocked-down bounce-getup, boss health bar on (`6_B_GARY`), music `MS_FinalShowdown03High`, Gary attacks. |
| B4 | **Scaffold boss fight** | Health thresholds drive the collapse: <75% → `GARY_ANGRY` (enables `T_AngryHit`: within 1 m while Gary is in locomotion/post-hit and the player isn't downed → Gary's special grapple); the tree-driven `Break` counter triggers `T_ScaffoldBreak` — per break, `PAnimApplyDamage` hammers one scaffold level (three stacked at descending heights) until destroyed, with a crash SFX and a camera preset change (path cameras 2→3→4); after the third break the player is invulnerable and fades prepare. Gary <1% → both play `AdjustGravity` nodes (the scripted mutual fall) → passed. |
| B5 | **Skylight crash + finale** | Pass block: if the fourth break didn't come from the tree, all three scaffold levels are force-destroyed with the crash SFX + camera 4; `F_CrashThroughRoofCutscene` waits for the player to hit `_6_B_GLASS_TRIGGER` (falling), then plays the glass-break SFX stack (BIG/MED/SML window breaks) + 4 shard effects, fades with camera-linked audio; baked cutscene `"6-BC"` (office: Crabblesnitch freed, Burton fired, Gary expelled, Pete made head boy, Jimmy & Zoe re-enrolled); player force-loaded back to the school front (`_PLAYER_START`). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Gary KO'd during stage 1 (he's min-health-protected; belt-and-braces) | `PedSetMissionCritical(gGary, true, F_MissionCriticalGary)` | `6_B_FAIL_01` (+ a fail camera framing the player and a `Failure` anim) |
| Player KO | `T_CheckPlayerHealth` poll + Gary taunt (id 20) | `6_B_FAIL_02` |
| Player leaves the roof | `T_RoofCheck` on `_6_B_OFFROOF` | `6_B_FAIL_03` |

Fail stinger 10. Retry starts at stage 2 if the checkpoint flag is set (see §1). Cleanup: bell emitters stopped, weather released, weapon snapshot restored, player health topped to ≥200, `GravityReset` node, anim groups unloaded, HUD widget 17 hidden, roof-top geometry un-instanced if shown.

### Pass-side effects

- In-file (**VERIFIED**): **all six factions set to 100 respect** (`SetFactionRespect(11,1,5,4,2,3, 100)` — townies, nerds(? slot 1), preppies, greasers(?), jocks, bullies — slot-to-clique mapping is the usual attitude-table question, all six = max regardless), `UnlockYearbookPicture(130)` (Gary's yearbook photo), `F_UnlockYearbookReward()`. Player health floor 200 on cleanup.
- `SMissPass.lua`: no branch (**VERIFIED**). Notably **no forced class completions** — unlike 1_B/2_B/3_B/4_B2, the final boss grants nothing curricular.
- Chain: into `Chapt5Trans` (credits + Endless Summer) — engine mission-table chaining, **OPEN** in Lua; see the 6_Dummy doc §3 for the full Endless Summer state machine.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-1 | B0 | baked (`6-B`, or `6-BB` on checkpoint retry) | `fade` + `letterbox` + `say`; `cutscene_skip_on_retry` variant |
| CS-2 | B1 wheelbarrow dumps ×2 | in-engine, ~6 s each | `camera` + `camera_path_cycle` look-pan + `ped_action_node` + `geometry_instance_toggle` (collision apply) + HUD prompt |
| CS-3 | B2 bell drops ×3 | scripted hazards | `hazard_drop` (exists) + emitter start/stop + PAnim `DoDamage` node |
| CS-4 | B3 push | baked (`6-BB`) | placeholder trio |
| CS-5 | B4 scaffold collapses ×3 | in-engine, ~2 s each | `destructible_prop_objective`/`prop_destroy_threshold_event` + `camera` presets + 2D SFX |
| CS-6 | B5 skylight crash | scripted fall | glass SFX stack + `effect_spawn_at_points` + `fade` |
| CS-7 | B5 office finale | baked (`6-BC`) | placeholder trio |

Anim groups: `Px_Rail`, `6B_PARA`, `Boxing`, `Russell`, `Nemesis`, `MINI_React`; trees `Act/AI/AI_Gary.act`, `Act/Conv/6_B.act`, `Act/Anim/Nemesis.act`, `Rails`. Music: `MS_FinalShowdown03{Low,Mid,High}.rsm`.

## 4. NPCs and assets needed

| Model id | Role | Notes |
|---|---|---|
| 130 | Gary | two incarnations: stage-1 fleeing/mission-critical/min-health version, stage-2 400 hp boss with custom AI tree; blip in stage 1; boss bar in stage 2 |

Pickups: 2× 362 health pickups (`HealthBute`) on the roof; pickup 311 (bricks?) purged at two points (`PickupRemoveAll(311)`). Persistent geometry: 9 bell PAnims (`SCBell`/`SCBell2`), 2 wheelbarrows (`WheelBrl`), 3 stacked `Scaffold` levels, `SC1d_mainTop(+Proxy)` bell-tower camera shell. Effects: 5 `SteamSlow`, `GlassLRGshards` ×4. Sound emitters: `BELLSONE/TWO/THREE` (`BellTower` bank); 2D `ScaffoldCrash`, window breaks.

### Named world data to extract from `6_Ba.DAT` + `6_B.DAT` (both present — **flagged, not extracted**)

- POINTLISTs: `_6_B_PLAYERSTART`, `_6_B_GARYSTART`, `_6_B_GARYPOSITION`, `_6_B_PLAYERPOSITION`, `_6_B_LADDERS`, `_6_B_LADDERSETUP1/2`, `_6_B_WARP_POINT`, `_6_B_POPPICKUP`, `_6_B_GLASSBREAK` (4 elements), `_PLAYER_START`.
- PATHs: `_6_B_MINIPATH`, `_6_B_MINIPATH2`, `_6_B_GARYPATH01-03`, `_6_B_LADDERSETUP1/2`, `_6_B_FINAL_CAM1-3`.
- TRIGGERs: `_6_B_GARY_TRIGGER_1/2/3/4/5/6`, `_6_B_LADDER1_BEGIN`, `_6_B_LADDER2_BEGIN`, `_6_B_BELLS_ONE/TWO/THREE`, `_6_B_SCHOOLROOFTOP`, `_6_B_OFFROOF`, `_6_B_GARY_THROW_1/2`, `_6_B_GLASS_TRIGGER`.
- Hardcoded coords in-file: all bell/wheelbarrow/scaffold PAnim positions, steam effects, emitter positions, bell-tower geometry, camera pairs, fail camera offsets.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by existing set | New primitives needed |
|---|---|---|
| B0 forced storm/clock | `weather_force`, `clock` (schema) | none |
| B0 disarm + snapshot | `weapon_strip`, `weapon_inventory_snapshot` | none |
| checkpoint retry | `cutscene_skip_on_retry`, `script_checkpoint`, `area_script_shared_state` | none — note the checkpoint flag lives in `shared.*` and survives the mission restart (session-scoped, not save-scoped… presumably; **OPEN**) |
| B1 trigger-gated retreat | `trigger_chain`, `npc_ai_tree_override`, `ped_set_invulnerable` | **boss_retreat_climb** (a trigger-gated vertical pursuit: at each gate the boss goes invulnerable + dodge-enabled, climbs/paths to the next vantage (incl. ladder-climb with a mid-climb projectile-immunity window and a warp-out recovery), then re-engages his throw loop; the player's progress triggers, not damage, drive the boss) |
| B1 brick throws | `npc_bomb_lob_cycle`, `boss_bark_loop` | none — composition |
| B1 wheelbarrow dumps | `hazard_drop` (pool), `ped_action_node`, `player_action_state` (ladder-anim check) | **ladder_hazard_dump** variant note: the dump only arms if the player is in the climb-on anim inside the trigger, shows the struggle-button HUD widget during the dump, and a monitor resets the camera if the player aborts the ladder mid-sequence |
| B2 bells | `hazard_drop`, `ambient_emitter_toggle` | none |
| B4 health-threshold anger | `decision_by_health_percent`, `boss_bar` (schema) | none |
| B4 tree-driven collapses | `lua_condition_query_from_tree`, `prop_destroy_threshold_event` | **tree_to_script_event** (the boss's action tree *calls into* the mission script to mutate state — `F_ScaffoldBreak` increments the collapse counter; reverse direction of the existing tree-query hooks; needed for any boss whose tree owns phase timing) |
| B4 grapple special | `force_grapple_intercept`, `npc_stat_override` | none — proximity + anim-state gated special = data |
| B5 fall-to-pass | `trigger_enter` (schema) | **collapse_finish_trigger** variant note: mission completes when the player enters a trigger *below* the arena during a scripted fall — a completion condition mid-ragdoll/fall, not a normal walk-in |
| B5 respect sweep | `set_faction_respect` (schema) ×6 | none |
| B5 yearbook | `yearbook_unlock` | none |

## 6. Open questions

- Which `AI_Gary.act` nodes call `F_ScaffoldBreak` / read `F_GaryAngry` / `F_DodgeCondition` / `F_WhatScaffoldAreWeOn` / `F_SecondStage` — the tree owns the collapse cadence; needs the action-tree dump.
- Where `shared.b602reachedScaffolds` is cleared (nothing in 6_B clears it; if never cleared, every later 6_B replay would start at stage 2 — presumably cleared on pass by the chain or a later script, **OPEN**).
- `WeatherSet(2, true)` id semantics (storm presumed) and player flag 58 / Gary flag 107 / entity flag 56 semantics.
- `PedOverrideStat(gGary, 34, 0)` and `(10, 50)` stat ids.
- What `6_Ba.DAT` holds vs `6_B.DAT` (likely stage-1 vs stage-2 split, **INFERRED**).
- The pass cutscene's respect: the in-file `SetFactionRespect` sweep is the mechanical half of "100% respect from all factions"; the SFaction defaults (see 6_Dummy doc) make it permanent.
