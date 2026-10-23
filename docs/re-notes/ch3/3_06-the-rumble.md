# 3_06 — The Rumble: recon notes

> **Correction (2026-10-23, reconciliation pass):** this file was first written with the title "Bait". That mapping was wrong — **3_02 = Bait** (VERIFIED: anim path namespace `3_02_Bait`, `main.lua` BMX-garage unlock gated on `IsMissionCompleated("3_02")`; see `3_02-bait.md`). This script's content (Peanut bike chase through New Coventry, greaser/prep street brawls, final alley fight vs Peanut/Vance) is **"The Rumble"**, the pre-boss Chapter 3 mission leading into 3_B (Fighting Johnny Vincent). The catalog's #43 "Rumble" entry conflates this mission with the 3_B boss fight. Body below otherwise unchanged; read "Bait" in reward/wiki cross-references as stale.

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_06.lua` (690 lines, complete) + `SMissPass.lua` (no `3_06` branch — verified by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_06 = "The Rumble"** (canonical Chapter 3 pre-boss mission, 8AM–11PM; originally mislabeled here as "Bait" — see correction note above) — **VERIFIED** by content: greasers-vs-preppies street brawls in New Coventry, Peanut (model 21, `nPeanutMaxUnique`) on his bike (272) drawing the player to the alley, a timed chase leg, and a final alley fight against Peanut, Vance (27) and a third greaser (24), with a scripted cop-bust vignette (`Cop_Frisk` anim group, cop car 295, cops 97/83) on the way. The mission DAT is `3_06New.DAT` — the name implies a revised trigger file superseding an older `3_06.DAT` (**INFERRED**).

Dead/broken ends in the file: `shared.gMissionEventFunction = F_DecideIfPOIShouldBeCreated` — **the function is never defined in the file**, so the hook is nil (**VERIFIED**; same for `RegisterGlobalEventHandler(7, F_PedWasAttacked, 1)` — nil handler) and `bLeftSide` is never assigned (nil → the outro always places the player on the right-side point). `TRIGGER._AMBIENTEVENT2` is armed and its handlers are (un)registered, but only event 1 has create/cleanup functions — a second ambient vignette was cut or lives elsewhere (**OPEN**). `F_SetupFinalFightPed` (tether-to-trigger + attack helper) is defined but never called. Weapon model 311 (brick, per 1_03 notes) is requested but nothing in-file arms it; `LibTrigger.lua` is imported but unused on the live path.

## 2. Mission flow

Driver: a function-pointer state machine — `missionStage` holds the current tick function (`F_Stage1Setup` → `F_Stage1` → `F_Stage2Setup` → `F_Stage2` → `F_Stage3Setup` → `F_Stage3`), called every frame by `main()` alongside `F_ManageAmbientEvents()` (cop vignette) and `F_CheckPlayerLocation()` (fail timers), until `missionCondition` leaves `"running"`. `LibTrigger.lua` imported.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Baked cutscene "3-06"** | `PlayCutsceneWithLoad("3-06", true, true)` in `MissionSetup`; `MissionDontFadeIn`; locked interactive stream `MS_EpicConfrontationHigh.rsm` armed before the cutscene; `DATLoad("3_06New.DAT", 2)`. |
| B1 | **Setup** | Fast culling on, POI groups off; unique models 25 (Lola), 38 (Pinky), 21 (Peanut), 23 (Johnny) suppressed (`unique_ped_suppress`); weapon models 303/311 requested; population and ambient vehicles zeroed. Faction attitudes saved then overridden for the mission: greasers(4)→preppies(5) hostile, greasers→jocks(2) hostile, jocks→greasers hostile, preppies→greasers hostile, and jocks/preppies→player-type(13) set to 3 (calm/friendly, **INFERRED**) — the player can walk through the brawls unmolested until scripted otherwise. Anim groups `Cop_Frisk`, `NPC_AggroTaunt`, `IDLE_GREAS_C`, `NIS_3_06`; conv tree `Act/Conv/3_06.act`. `PedSetGlobalAttitude_Rumble(true)` (worldwide rumble-mode attitude switch, semantics **OPEN**), `shared.gDisableBusStops = true`, four persistent barricade/prop entities created at hardcoded coords (`3_06BinBarr02`, `3_06CarBarr03`, `TrainCarA`, `hydroSup`) — the alley fight dressing. Player transitioned to `_STARTINGPOINT` (area 0). |
| B2 | **Stage 1: walk into the rumble** | Ambient-event triggers armed; a staged brawl: two preppies (31/32) paired against two greasers (26/24) (`PedAttack` cross-pairs, then all made ambient so AI brawls on), plus an invulnerable greaser attacker (28) who converts to attack the player if the player comes within 15 m or hits him. A **300 s mission timer** starts with the timer HUD component shown. Peanut (21) spawned at `_3_06_GREASERS` node 4: untargetable, invulnerable, ignore-stimuli, put on bike 272 (`_3_06_PEANUTBIKE`); point blip on him. Objective `3_06_Obj01`. Population re-overridden to a light ambient mix (`(8, 0,0,0,0, 4,4, 0…)` — slot semantics **OPEN**). |
| B3 | **Stage 1: approach Peanut / second brawl** | Leaving `_3_06_INNERAREA` prints a get-back warning (once per exit); leaving `_3_06_OUTERAREA` = fail `LeftFight`. Entering `_STAG2TRIG` spawns the second paired brawl (preps 31/32 vs greasers 29/24/26; one greaser attacks the player; all ambient). When the player gets within 20 m of Peanut or hits him: timer stops, HUD timer hidden, `F_PeanutRun` — Peanut stops, ignores stimuli, and rides to `_FINALFIGHT` node 5 with a char blip; `gPeanutTimer` starts a fresh 300 s chase budget (**VERIFIED** as `GetTimer()` delta, not the mission timer). |
| B4 | **Stage 2: follow Peanut** | Stage-2 setup spawns Vance (27) and a greaser (24) at `_FINALFIGHT` nodes 3/4 — stationary and **asleep** (pre-positioned for the finale); objective swaps to `3_06_PENTFLLW` (follow Peanut, **paraphrase**). During the leg: entering `_THIRDFIGHT` spawns the third paired brawl (same recipe; one greaser on the player). If the player strays out of `_306POORAREA`, a **15 s visible re-entry countdown** starts (warning text + timer HUD); returning cancels it; expiry is what feeds `MissionTimerHasFinished` → fail `PeanutRan`. A redundant in-stage check also fails the mission if `gPeanutTimer` exceeds 300 s (generic fail, no key). Reaching `_3_06_FIGHTTRIG`, or the player hitting any of the final-fight trio, puts Peanut + the two greasers asleep/stationary and hands off to stage 3 setup. |
| B5 | **Stage 3 setup: alley NIS** | Peanut dismounted, control off, fade. `F_DoPlayerAlleyNIS`: vehicles cleared, `MS_EpicConfrantation_NIS.rsm` (sic — original's spelling), NIS-safe, speech tree muted; the trio deleted and respawned clean at `_FINALFIGHT` nodes 2/3/5 (Vance 27, greaser 24, Peanut 21); weapons 323 cleared on all three; player at `_FINALFIGHTNIS` node 4; mutual facing/lock, FOV-70 then FOV-30 fixed cameras, greaser idle anims, two speech beats (Jimmy 39, Vance 40). Fade, stream swaps to locked `MS_EpicConfrontationHighPart2.rsm`, player repositioned to a left/right side point (always right — `bLeftSide` is never set, **VERIFIED** dead branch), blips on all three, control back, sleep flags cleared. |
| B6 | **Stage 3: the alley fight** | The trio fights the player (attack intensity 1; Peanut restored targetable, health 300, invulnerability off; greaser 24 gets the `CreateAuthFight` action node). Objective `3_06_FightPeanut`. Leave-the-fight logic: exiting `_FINALFIGHTTRIG` swaps the objective to a get-back prompt (`3_06_Obj05`, shown with an effectively-permanent 1,000,000 s print); staying out past `_3_06_FailTrig` (or a lingering finished timer) = fail `LeftFight` with the fight peds made ambient; re-entering restores the fight objective. Pass check per tick: both rooftop/backup greasers dead **and** Peanut dead → stop the interactive stream, play the ending stream once (`MS_EpicConfrontationEnding.rsm`), wait 3.5 s, fade, complete objectives, `missionCondition = "passed"`. |
| B7 | **Ambient cop vignette (runs all mission)** | Player entering `_AMBIENTEVENT1` (once until they leave): cop car 295 spawns with engine + siren on; cop 97 tethered to its point (10 m radius, cheap, scripted collision); cop 83 warped into the car (cheap, ignore-stimuli); a vandal (29) and a second cop (83) spawned asleep/cheap at their points; the pair get mutual target locks and the cop plays the `3_06/Frisk/GrappleAttempt` conv node — a background "bust in progress" scene. A watch thread makes either cop go ambient (normal AI) if he acquires the player as target. Leaving the trigger deletes the whole vignette. Re-entering re-creates it. |
| B8 | **Pass** | No banner, no respect changes, no in-file reward screen — unusually bare: stop the stream; **if `"3_08_PostDummy"` is not yet completed: load the `"Christmas"` special entity set, `MissionForceCompleted("3_08_PostDummy")` + success-count increment** (a marker-mission force-complete; what `3_08_PostDummy` gates is **OPEN** — likely the post-3_08 world state that Bait also unlocks, and the `"Christmas"` entity name is suspicious reuse, **OPEN**); then `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player outside `_3_06_OUTERAREA` (stage 1) | trigger-exit poll | `3_06_LEFTTRIG` |
| Player stays out of the alley (stage 3: past `_3_06_FailTrig`) | objective-swap two-step + poll | `3_06_LEFTTRIG` |
| Peanut chase budget blown (300 s) / 15 s re-entry countdown expiry | `GetTimer` delta + `MissionTimerHasFinished` | `3_06_PEANUTRAN` (timer path) / generic `MissionFail(false,false,false)` (stage-2 delta path) |
| Anything else | fallthrough `"failed"` branch | generic, retryable (`MissionFail(true, …)`) |

Fail stinger id 10. No checkpoints; retry replays from B0. Cleanup: rumble-mode attitude + fast culling + POI groups + bus stops + `gMissionEventFunction` all restored, both ambient-event trigger handlers unregistered, blips removed, health bar hidden, population reverted, player invulnerability cleared, Peanut's bike made ambient, the four persistent barricade entities deleted, unique models restored.

### Pass-side effects

- In-file: the `3_08_PostDummy` force-complete + `"Christmas"` entity load (see B8). **No completion banner, no respect deltas, no money in the Lua.**
- `SMissPass.lua`: **no `3_06` branch — VERIFIED by absence.** (An earlier draft of this doc cited the wiki's $20 + Bike Park reward for "Bait"; that reward belongs to 3_02 and lives in `main.lua`'s `F_BMXGarage` gating. Any reward for The Rumble is **OPEN** — candidate: engine-side mission reward table or the `3_08_PostDummy` marker.)

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0 intro | baked (`3-06`) | `fade` at `_STARTINGPOINT` + `letterbox` + one `say` |
| CS-2 | B5 alley confrontation | in-engine, ~10 s | `letterbox` + fixed `camera` ×2 (FOV moves) + `speech_sequence` (2 lines) + `ped_action_node` idles + respawn-and-place of the trio |
| CS-3 | B6 victory beat | in-engine micro | stop stream + one-shot ending stream + 3.5 s hold + `fade` (no banner — **do not add one**) |

Cutscene-adjacent anim groups: `NIS_3_06`, `Cop_Frisk` (vignette), `NPC_AggroTaunt`, `IDLE_GREAS_C`.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 21 | Peanut (bait → final boss) | untargetable/invulnerable bike guide in B2-B4, rides to the alley on proximity/hit; restored for the finale with 300 health |
| 27 | Vance (final fight) | asleep pre-staging at `_FINALFIGHT`; NIS speech 40 |
| 24 | greaser backup ×(2 staged + brawls) | asleep pre-staging; `CreateAuthFight` node at fight start |
| 28 | invulnerable brawl greaser (converts on approach/hit) | stage-1 pressure ped |
| 26, 24, 29 | brawl greasers | paired `PedAttack` setups, then ambient |
| 31, 32 | brawl preppies | paired, then ambient |
| 97, 83 | cops (vignette) | 97 tethered point-cop; 83 warped into the car; a second 83 plays the frisk pair |
| 29 | vandal (vignette) | asleep + cheap until the frisk loop |
| 25, 38, 23 | Lola, Pinky, Johnny | unique models suppressed only — none spawned in-file (intro cutscene cast, **INFERRED**) |

Weapon models 303 (slingshot — **OPEN** who uses it in-file; nobody is armed on the live path) and 311 (brick, per 1_03 notes — requested, never armed). Weapon 323 is cleared from the trio before the NIS (what they carry in is **OPEN** — a carried weapon from the baked cutscene, **INFERRED**).

### Vehicles

- 272 = Peanut's bike (`_3_06_PEANUTBIKE`; made ambient on cleanup). 295 = cop car (vignette; engine + siren on while parked, deleted on trigger exit).

### Speech

Scripted `M_3_06` ids used: 39 (Jimmy), 40 (Vance) — **2 distinct scripted lines** (tiny); everything else is text objectives. Music: `MS_EpicConfrontationHigh.rsm` (locked), `MS_EpicConfrantation_NIS.rsm` (sic), `MS_EpicConfrontationHighPart2.rsm` (locked), `MS_EpicConfrontationEnding.rsm` (one-shot); fail stinger id 10.

### Named world data to extract from `3_06New.DAT` (present — `DATLoad("3_06New.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_STARTINGPOINT`, `_INITIALPREPS` (2), `_INTRONIS` (≥10 elements — brawl + attacker spawns), `_3_06_GREASERS` (≥4), `_3_06_PREPS` (2), `_3_06_PEANUTBIKE`, `_FINALFIGHT` (≥5), `_FINALFIGHTNIS` (≥4), `_3_06_RIGHTSIDE`, `_AMBIENTEVENT1` (5 elements).
- TRIGGERs: `_3_06_INNERAREA`, `_3_06_OUTERAREA`, `_STAG2TRIG`, `_THIRDFIGHT`, `_3_06_FIGHTTRIG`, `_FINALFIGHTTRIG`, `_3_06_FailTrig`, `_306POORAREA` (note: single underscore + no digit-separator in the original name), `_AMBIENTEVENT1`, `_AMBIENTEVENT2`.
- Hardcoded coords in-file: the four persistent entities (525.4, -355.9 / 525.5, -321.4 / 466.9, -247.6 / 516.2, -301.7) and the two alley-NIS cameras (547.7, -464.3 … / 543.6, -470.8 …).
- Area codes seen: **0 only**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1/ch2 set | New primitives needed |
|---|---|---|
| B0/B1 world setup | `population_override`, `unique_ped_suppress`, `poi_enable`, `persistent_fade_entity` (barricades ×4), `area_script_shared_state` (`gDisableBusStops`) | **faction_attitude_matrix_override** (save current faction→faction attitudes, apply a mission attitude set, restore on cleanup — named set-level version of the inline `PedSetTypeToTypeAttitude` calls also seen in 2_07/2_08); **global_rumble_attitude_toggle** (`PedSetGlobalAttitude_Rumble` — one global "everyone brawls" switch, semantics **OPEN**) |
| B2/B4 paired brawls | `spawn_npc` bundles | **brawl_pair_spawn** (spawn N peds as cross-attacking pairs on a point list, then make ambient so the crowd AI sustains the brawl; one designated member converts to attack the player on proximity/hit — the conversion itself is `ped_hit_event` + proximity) |
| B2 asleep staging | `spawn_npc` flags partially | **npc_asleep_staging** (`PedSetAsleep` + stationary spawn state, woken by script at fight start — also used in 3_05's vignette peds; small spawn_npc flag) |
| B2-B4 Peanut guide | `npc_ride_vehicle_path` (2_05) partially | **npc_within_radius** condition (player within R of a named NPC — gates Peanut's run and the stage-1 converter; also needed by 3_05's Lola approach); Peanut's ride-off is `PedMoveToPoint` mounted, not a path — note as a mounted move-to variant of the same primitive |
| B2/B4 timers | `countdown_fail_timer` (1_02C, 2_S02) | none new — the 300 s budget + 15 s re-entry countdown + HUD timer component are that primitive, twice |
| B4/B6 leave-zone | `stay_in_zone_fail` (2_07), warn→fail trigger pairs (2_G2) | none new — objective-swap-on-exit is two `trigger_exit/enter` + `objective` actions |
| B7 cop vignette | `scripted_vehicle_arrival` (2_S02) partially, `pair_anim` (frisk grapple) partially | **tethered_npc_spawn** (`PedSetTetherToPoint` with radius + move-to-center + cheap/scripted-collision flags; the trigger variant `npc_tether_trigger` exists from 1_S01 — this is the point variant); **vehicle_siren_engine_flags** (engine/siren on a parked vignette car); **ambient_vignette_zone** (composite: trigger-enter creates the vignette cast, trigger-exit deletes it, re-enter recreates; a watch thread de-scripts any member that targets the player). The `_AMBIENTEVENT2` half is data for the same composite |
| B5 alley NIS | `letterbox`, `camera`, `speech_sequence`, `ped_action_node`, `weapon_strip` (clear 323 ×3) | none new |
| B6 pass | `event`, `fade` | none new; the no-banner pass is just absence |
| B8 cross-mission effect | `force_complete` (schema action) + `MissionSuccessCountInc` | **force_complete_marker_mission** variant (force-complete a *different* mission id + bump its success count, guarded by not-yet-completed — the schema's `force_complete` covers the call; the conditional guard is an `attempt`-style condition); **special_entities_load** (`AreaLoadSpecialEntities("Christmas", …)` — world-dressing set swap; closest existing: `world_state_swap`, verify coverage) |
| fails | above + generic `MissionFail` retry variants | none new |

## 6. Open questions

- `PedSetGlobalAttitude_Rumble(true)` semantics (global brawl-mode attitude override) and the attitude value table (0 hostile / 3 calm / 4 fear, per usage across chapters — **INFERRED**, undecoded).
- What `3_08_PostDummy` gates and why passing The Rumble also loads the `"Christmas"` special entities (likely a shared post-3_08 world state; the Christmas name may be deliberate for ch3's winter segment or stale reuse — needs the area-scripts side).
- Resolved: the wiki-listed $20 + Bike Park reward belongs to 3_02 (Bait), not this mission (see correction note at top).
- `RegisterGlobalEventHandler(7, nil)` and `gMissionEventFunction = nil` — dead hooks in the shipped script (event 7 = ped-attacked, **INFERRED** from the handler name).
- `bLeftSide` never assigned — was a left/right approach split planned from `_FINALFIGHTNIS`? Cut branch.
- `_AMBIENTEVENT2` — armed and unregistered but never created: second cop vignette cut, or DAT-side wiring.
- Weapon 323 on the trio (cleared pre-NIS) and 303/311 requesters — id-level confirmation pending a weapon-table note.
- Exact trigger geometry for the two-stage leave-zone fail and the three brawl trigger placements (pending `3_06New.DAT` extraction; also whether a legacy `3_06.DAT` still ships).
