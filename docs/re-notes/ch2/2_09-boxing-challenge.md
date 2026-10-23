# 2_09 — Boxing Challenge: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_09.lua` (75 lines, a thin wrapper) + `chap2/Boxing_util.lua` (716 lines, the shared boxing minigame that runs the whole fight) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_09 = "Boxing Challenge"** — **VERIFIED** by content: the player walks to the boxing club in Old Bullworth Vale, enters the ring, and fights the preppy champion Bif (opponent table entry 1: model 133 with damaged variants `PR2nd_Bif_OBOX_D1`/`_D2`, name key `2_09_01`, boss music `MS_BoxingBossFight.rsm`). Matches the canonical Chapter 2 list entry "Boxing Challenge (8-25, chapter boss)".

Two identification notes worth flagging:

1. **The script is a wrapper; the mission is the minigame.** `2_09.lua` itself is only an approach beat plus door interaction; every fight rule lives in `Boxing_util.lua`, which also serves the repeatable side boxing (`BoxingSetRandom/Chad/Justin/Parker/Bryce`, opponents 2-5) and the `PrepVendetta`/`2_R11` rematches (`shared.PrepVendettaRunning`, `shared.BoxingSuccess`). Reconning 2_09 means reconning the boxing system.
2. **The canonical "chapter boss" label is loose.** Beating Bif plays the baked cutscene `"2-B"` inline and succeeds 2_09; the actual boss *fight* is the separate mission `2_B.lua` (Darby, in the same gym, area 27 — its `SMissPass` case sets the chapter transition). 2_09 is the boss *gateway*, not the boss arena fight itself.

Setting: approach outdoors (area 0; blip at (397.421, 144.495, 5.277) = boxing club door in the Vale, **INFERRED**), fight indoors — **area code 27 = `iboxing`** (VERIFIED by `AreaTransitionPoint(27, ...)` and the `iboxing_ESCDoor*` door ids). New area code beyond the known list: **27 = iboxing (VERIFIED)**.

## 2. Mission flow

Driver: `main()` in `2_09.lua` → `MissionSetup()` (cutscene + door locks) → approach → `BoxingSetBif()` → `BoxingMissionSetup(true)` → `BoxingMissionControl()` (the entire match loop, blocking) → pass/fail handled *inside* the util (`MissionSucceed` in `IsEnemeyKO`, `MissionFail` on a lost decision / `MissionFailBoxing`).

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Baked cutscene "2-09"** | `PlayCutsceneWithLoad("2-09", true)`. Four boxing-gym doors locked for the mission (`DT_trich_BoxingDoor01/02`, `iboxing_ESCDoorL01/R01`). |
| B1 | **Walk to the gym in the snow** | Snow is forced (`WeatherForceSnow(true)`, `WeatherSet(2)`). `2_09.DAT` loaded; player placed at `_2_09_PLAYERSTART` (area 0); objective + static blip at (397.4, 144.5, 5.3). The beat ends on an **entity interact prompt** at the `_2_09_BOXINGCLUBDOOR` point: `EntityInteract(pos, 1, "BUT_ENTER", 5, 7, 9)` — stand at the door and press the context button (**INFERRED** arg meanings: radius/icon/flags). |
| B2 | **Enter, cutscene 2-09B** | Fade, weapons stripped (`PedSetWeaponNow(gPlayer, -1, 0)`), transition to area 27 at `_2_09_CUTSCENE1`, baked cutscene `"2-09B"`, second objective added, control off, `2_09.DAT` unloaded (mission DAT cleanup flag prevents double-unload), ambient ped generation disabled. |
| B3 | **Match setup** (`BoxingMissionSetup`) | Loads `Boxing.DAT`, anim groups `Boxing`/`MINI_React`, player tree `Act/Anim/BoxingPlayer.act`, opponent AI `Act/AI/AI_BOXER.act`, sound bank `Mission\2_09.bnk`. **Clock paused.** Player: clothing backed up and swapped to the `"Boxing"` outfit, health floored at 200 if lower, placed at `_BOXING_PLAYERSTART`, fighting system 1 (boxing ruleset). Bif: model 133 at `_Boxing_EnemyStart`, tree `/Global/P_Bif`, always-strafe, combat-zone mask (true,false,false), stat 62 zeroed, name-bar health bar. Both lock target on each other; player targeting of others disabled; HUD components toggled (4 on; 0, 11, 20 off). Crowd: 7 preppy models {38, 32, 34, 40, 35, 30, 31} spawned cheap/invulnerable/stationary at `_BOXING_CROWD` points running a cheering AI node — model 38 gets the `isPinky` special case (no cheer node; **INFERRED** 38 = Pinky). A persistent black-fade entity `BX_BlkFade` is created at (-712.0, 377.7, 299.1, area 27). Intro: camera path `_BOXING_INTROCAM`, widescreen, boss music at 0.7. Stat 180 incremented (matches played, **INFERRED**). |
| B4 | **Round loop** (`BoxingMissionControl`, 40 s rounds, up to 12) | Each round: announcement banner `BOXING_Rxx` + a second banner, bell (`Fite_Bell` 2D), crowd-cheer thread starts, both fighters made vulnerable, timer shown with the round label, boxer AI tree activated and attacks. Mid-round watch: (a) **damage-stage model swaps** — at <50% and <25% of starting health the opponent's model is swapped to the damaged variants (Bif: 172 then 243, names `PR2nd_Bif_OBOX_D1`/`D2`); (b) **stun event** — while the boxer plays the `BoxingStun/StunControl` node the round timer pauses, a prompt shows (`BOXING_09`), and the player regenerates +1 health per tick until the stun ends (first stun gets a 1.5 s grace); (c) **round end** on timer — both invulnerable, control off, corner camera (two fixed cameras chosen by which ring half the player is in, 50/50 look-at player/boxer coin flip), bell, both walk to corners (`_BOXING_PLAYERCORNER`/`_BOXING_ENEMYCORNER`). Between rounds (`RoundAdvance`): player healed +24% of max if hurt, Bif healed +23% (non-Bif opponents only +10%), repositioned, re-faced, control back. |
| B5 | **Player knockdowns / get-up struggle** | On a player KO the util pauses the timer, plays crowd reaction `M_2_09` id 2, opponent celebrates, knockdown counter increments. Getting back up re-seats the boxer and resumes. The get-up *difficulty* is exposed to the action tree as Lua condition functions `F_CheckMediumGetUp` / `F_CheckHardGetUp` / `F_CheckImpossibleGetUp` (2 / 3 / 5+ knockdowns) and `F_IsOpponentStunned`, `F_HoldUpHomeBoy` — **INFERRED** these are called from `BoxingPlayer.act`/`HitTree.act` nodes (the trees are not decoded), same pattern as the 1_02 tutorial gates. Losing all-get-up / death routes to `MissionFailBoxing` (fail key `M_FAIL_DEAD`). |
| B6 | **Decision at round 12** | If round 12 ends without a KO: compare health as % of match-start values — player ahead → win path; else `gOpponentWins`, fail music, fail with key `PUN_02`. |
| B7 | **Win: KO or decision** (`IsEnemeyKO`) | Opponent dead or `gPlayerWins`: timer paused, struggle HUD off, player plays a cheer node, `mission_completed`. Because the opponent is Bif (`gBoxingAgainstBif`), the generic end-NIS is skipped: scripted-camera locks released, black-fade entity deleted, **baked cutscene `"2-B"` plays inline**, then widescreen off, player repositioned to `_BOXING_PLAYERSTART`, stat 181 incremented (boxing wins, **INFERRED**), `MissionSucceed(true, false, false)`. **Chain target: mission `2_B`** (Darby boss fight in the same gym) is picked up from the world mission table after the 2-B cutscene — the table itself is **OPEN** (outside the Lua), but 2_B's own script starts in the gym with the same door props, so the handoff is effectively immediate. |
| B8 | **Cleanup** (`BoxingMissionCleanup` via `MissionCleanup`) | Gym doors unlocked, ped generation re-enabled, clock unpaused, fighting system off, health topped to max, HUD restored (0/11/20 on, 21 off, 17 off), alpha restored, AI butes back to `Default`, clothing restored from backup, player tree reset to `/Global/Player`, `LockFPS30(true)`, crowd/DAT/anim groups unloaded. If the Bif fight ended without a win (fail), the player is transitioned back outside to `_BOXING_END_2_09` (area 0). Snow is released only if the gym was never reached or the mission failed (**INFERRED**: snow persists into the 2_B aftermath on a win). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player KO'd / can't get up | knockdown counter → tree-called get-up conditions; death path calls `MissionFailBoxing` | `M_FAIL_DEAD` |
| Lose the 12-round decision | health-% comparison at the final bell | `PUN_02` |

Retry replays from B0 (no attempt-count skip is visible for the two baked cutscenes — **OPEN**, presumably the engine's standard retry handling). No mid-match checkpointing.

### Pass effects (`SMissPass.lua`)

**No `2_09` case exists** in `F_SetupMissionRequirements` — **VERIFIED**. In-script effects: stats 180/181, clothing restore, health top-up. The money reward in `Boxing_util` (`rewardMoney = 2000`, banner `BOXING_04` + `StatAddToInt(182)`) applies only to the *repeatable non-Bif* opponents, not to 2_09. The chapter-level unlocks (ChapterSet, force-completed classes `C_Chem_2`/`C_Art_2`/`C_Wrestling_2`/`C_English_2`/`C_Photography_1`/`C_Photography_2`, photography save levels) hang off mission **`2_B`**, not 2_09 — so 2_09's only real reward is unlocking the boss fight.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0, before control | baked (`2-09`) | fade in at `_2_09_PLAYERSTART` in forced snow + letterbox + one `say` beat, or straight to B1 |
| CS-2 | B2 gym entry | baked (`2-09B`) | `fade` + `teleport` to area 27 + short original ring-approach card |
| CS-3 | B3 intro | in-engine (camera path `_BOXING_INTROCAM` in `Boxing.dat`) | keep: scripted `camera` path + `letterbox`, no content needed |
| CS-4 | B4 round breaks | in-engine micro-scenes | two fixed corner `camera`s + coin-flip look-at + `timer`; corner walk is `npc_move_to` on both fighters |
| CS-5 | B7 win | baked (`2-B`) played inline from the KO path | fade + original VO card that hands off to the 2_B boss fight |
| CS-6 | B5 knockdown/get-up | in-engine (tree-driven) | `letterbox`-free: `camera` shot `BoxingCam` + `button-sequence prompt` struggle widget + timed `say` bark |

Cutscene/anim groups: `Boxing`, `MINI_React`; trees `Act/Anim/BoxingPlayer.act` (player), `Act/Anim/P_Bif.act` (Bif idle/celebration), `Act/AI/AI_BOXER.act` (opponent AI), `Act/Anim/Ambient.act` (cheer nodes), `Act/HitTree.act` (KO/get-up).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`; names from script tables — **INFERRED**)

| Model id | Role | Notes |
|---|---|---|
| 133 | Bif, boxing base model | opponent index 1 (internally re-tagged 69 after round 1 starts — watch for this when reading conditions); health bar with name key `2_09_01` |
| 172 / 243 | Bif damaged variants (`PR2nd_Bif_OBOX_D1`/`_D2`) | swapped in at <50% / <25% of match-start health |
| 117/241/242, 118/244/245, 119/246/247, 36/239/240 | Chad, Justin, Parker, Bryce base + 2 damage tiers each | repeatable side opponents (indices 2-5), not used by 2_09 but loaded by the same util |
| 38, 32, 34, 40, 35, 30, 31 | ringside crowd (7 points) | cheap/invulnerable/stationary, cheering AI node; index matching the current opponent is skipped; 38 special-cased (`isPinky`, no cheer node) |

### Animation groups / action trees / systems

`LoadAnimationGroup`: `Boxing`, `MINI_React`. `LoadActionTree`: `Act/Anim/BoxingPlayer.act`, `Act/AI/AI_BOXER.act`; also referenced: `Act/Anim/P_Bif.act`, `Act/Anim/Ambient.act`, `Act/HitTree.act`. Boxing-specific engine features the scripts lean on: fighting system 1 (ruleset switch), `PedSetAlwaysStrafe`, `PedSetCombatZoneMask`, mutual target lock, `HUDSetFightStyle`, `PlayerWeaponHudLock`, `PedSetAIButes("Boxing")`, mutual `PedFaceObject` during intros, player alpha 180 during rounds (corner-camera transparency, **INFERRED**), `LockFPS30`.

### Props / pickups / weapons

- Doors: `DT_trich_BoxingDoor01/02` (Vale exterior, **INFERRED** from `trich` = the Vale's rich district), `iboxing_ESCDoorL01/R01` (gym escape doors) — all four locked during the mission, unlocked in cleanup.
- Persistent entity `BX_BlkFade` at (-712.038, 377.699, 299.063, area 27) — a black fade card used around the 2-B handoff.
- No weapons/pickups: player is stripped on entry (`PedSetWeaponNow(-1)`), weapon HUD locked; `PedRestoreWeaponInventorySnapshot` appears in 2_B's fail path, implying an inventory snapshot/restore pair across the gym sequence (**INFERRED**).

### Speech

`M_2_09` ids used: 1, 2, 3, 4, 10, 30 — **6 distinct 2D commentary lines**, chosen contextually by the cheer thread every ~7.5 s: player just KO'd (2), player recently hit (Bif-specific 30 at 45% chance, else 3), boxer recently hit (4), idle (Bif-specific 10 at 45%, else 1). Music: `MS_BoxingBossFight.rsm` (Bif) vs `MS_BoxingReg.rsm` (others); bell `Fite_Bell`; pass/fail stingers. Sound bank `Mission\2_09.bnk`.

### Stats

`StatAddToInt(180)` on setup, `181` on a win, `182` on side-match money (**INFERRED**: boxing played/won/earned counters).

### Named world data to extract from `2_09.dat` + `Boxing.dat`

- POINTLISTs: `_2_09_PLAYERSTART`, `_2_09_BOXINGCLUBDOOR`, `_2_09_CUTSCENE1`, `_Boxing_EnemyStart`, `_BOXING_PLAYERSTART`, `_BOXING_PLAYERCORNER`, `_BOXING_ENEMYCORNER`, `_BOXING_CROWD`, `_BOXING_END_2_09`, `_BOXING_FINISHEDBOXING`.
- PATHs: `_BOXING_INTROCAM`.
- TRIGGERs: `_BOXHALFTHERING`, `_CORNER_CHECK1`, `_CORNER_CHECK3`; doors `DT_trich_BoxingDoor01/02`, `iboxing_ESCDoorL01/R01`.
- Hardcoded coords: door blip (397.421, 144.495, 5.277, area 0); `BX_BlkFade` (-712.038, 377.699, 299.063, area 27); ring-half cameras (-708.14, 373.82, 295.70) / (-715.79, 381.58, 295.49); corner face target (-715.931, 373.532, 295.225); player heading -53.64 on the generic end NIS.
- Area codes seen: **0** (approach), **27** (`iboxing` gym, VERIFIED).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by (ch1 set) | New primitives needed |
|---|---|---|
| B0/B2 cutscenes | `fade`, `letterbox`, `say`, `cutscene_skip_on_retry` | — |
| B1 approach + door | `dynamic_blip`, `prop_interact` | **weather_force** (force/release snow state, with release deferred past the interior transition), **interact_button_prompt** if `prop_interact` stays door-only (`EntityInteract` with a named button + prompt at an arbitrary point) |
| B3 setup | `clear_ambient_peds`, `crowd_ring`, `clock_pause/unpause`, `blip_on_npc`-style health bar (`health_bar(name)`) | **temp_outfit_swap** (backup current clothes → forced outfit → restore on cleanup), **weapon_strip** + inventory snapshot/restore, **fighting_system_switch** (ruleset id + strafe/combat-zone/AI-butes bundle per fighter), **persistent_fade_entity** (named black-card entity created/deleted across scene boundaries) |
| B4 rounds | `timer`, `completion_banner` (round announcements), `boss_hit_count_health`-adjacent health logic | **round_loop** (timed rounds → corner reset, between-round heal %, re-face/re-lock, round-count cap), **model_swap_on_health_threshold** (50%/25% damaged variants), **stun_state_event** (opponent stun node → timer pause + player regen + prompt, first-stun grace), **corner_camera_select** (ring-half trigger → fixed camera + coin-flip look-at) |
| B5 knockdowns | `player_ko`, `button-sequence prompt` | **ko_count_getup_difficulty** (knockdown counter → tree-queryable condition functions gating the get-up struggle), **lua_condition_query_from_tree** (general mechanism: action trees calling `F_*` Lua predicates — shared with 1_02's gates but named here because boxing has 4+ of them) |
| crowd commentary | `bark_loop` | **contextual_commentary** (event-driven 2D speech picker: player-hit / opponent-hit / idle, per-opponent line sets with % weights, 7.5 s cadence) |
| B6 decision | — | **decision_by_health_percent** (at round cap, compare health % of match-start values → win/fail branch) |
| B7/B8 win/fail | `completion_banner`, `fade`, `teleport` | **inline_cutscene_chain** (a KO path that plays another baked cutscene mid-success and hands off to a follow-up mission without returning to free roam) |

## 6. Open questions

- `BoxingPlayer.act` / `AI_BOXER.act` / `HitTree.act` internals: punch set, block/dodge rules, stun entry condition, and exactly which tree nodes call `F_Check*GetUp`, `F_IsOpponentStunned`, `F_HoldUpHomeBoy`, `SetPlayerIsKnockout`, `MissionFailBoxing` (the trees are not decoded; the Lua gives only the round scaffolding).
- `EntityInteract` argument meanings (radius/icon/flags `1, "BUT_ENTER", 5, 7, 9`).
- Why the opponent index is re-tagged 1 → 69 after round 1 (`shared.gBoxingOpponent = 69`) — it gates the Bif-only branches in cleanup; probably a "mission Bif vs. challenge Bif" marker read outside these files. **OPEN**.
- `PedOverrideStat(boxer, 62, 0)` — stat semantics undecoded.
- Whether the `"2-B"` cutscene or the mission table is what actually starts `2_B` (the cutscene plays inline, then `MissionSucceed`; the boss mission pickup mechanism is outside the Lua).
- `LockFPS30(true)` in cleanup with no matching unlock in these files — where the cap is released is **OPEN**.
