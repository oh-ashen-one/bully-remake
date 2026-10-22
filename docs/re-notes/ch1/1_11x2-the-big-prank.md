# 1_11x2 — "The Big Prank"

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources (read-only, nothing copied): decompiled Lua `Bully-SE-scripts/scripts/chap1/1_11x2.lua` (935 lines), cross-refs `SMissPass.lua`, `main.lua:322`, `SFaction.lua:258-341`. This document describes structure and logic in our own words; no dialogue, cutscene script or narrative text is reproduced. Script names are citations only.

## 1. Identification

**This is the catalog mission "The Big Prank"** (optional, 20:00-01:00, Halloween night only). Confidence: **VERIFIED**.

Evidence, all in-script: a dog-poop-bag weapon (399) and `W_PooBag` anim group; the persistent prop is literally named `SC_PooBag`; the victim is ped model 55 — the gym teacher (context: staff-lounge door `DT_ISCHOOL_STAFF`, fire-alarm flag `shared.gSchoolFAlarmOn`, he stomps out a flaming bag); the pass reward is outfit `Ninja_RED` with unlock card text id `1_11_x2_UNLKCSTM`; and the classic prank structure (collect → plant → light → pull alarm → victim stomps it) is unmistakable. Placement in the night chain is also verified by `main.lua:322` (Gary+Petey allies persist between 1_11xp and this mission) and `SFaction.lua`, where completing 1_11x2 flips the Bully faction (type 11) attitude toward the player from friendly to hostile — the prank's narrative fallout. Optional-mission behavior is confirmed by `SMissPass.lua`: passing 1_B (Help Gary) **force-completes 1_11x2** so the chapter can advance without it.

Requires: 1_11xp completed (Gary+Petey waiting as allies is the in-world lead-in; the actual availability gate lives in the mission table, **OPEN**).

## 2. Mission flow

Three stages plus two in-engine skippable cutscenes. Debug entry `F_StartAtStage2` exists (spawns into stage 2 with the bag already in hand).

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **MissionSetup / F_SetupMission** | `1_11X2.DAT` loaded, no fade-in. Halloween music layers. Any persisted Gary/Petey allies deleted. Player transitioned to `POINTLIST._1_11X2_CUTPLAYER` (area 0). Weapon models 399 (poop bag) and 411 (Gary's item) requested. Anim groups: `POI_Smoking`, `POI_WarmHands`, `Halloween`, `W_PooBag`, `Px_Sink`, `MINI_React`, `NIS_1_11`. Sound bank `MISSIONFlameBag.bnk`. Unique-spawn capture-and-disable for Burton (55), Chad (32), the dog (141) so duplicates can't wander in. Patrol paths disabled; punishment system disabled. Eight shared prank blips (`shared.gHallowPrankBlip01-08`, created elsewhere, **OPEN**) removed. Debug launch path sets the clock to 22:00 and force-wears the Halloween outfit. |
| B1 | **In-engine cutscene 1: the plan** (`F_cutDogPlan` + thread `T_Cutscene01`) | Widescreen, control off, player/Gary/Pete moved to `_1_11X2_CUTPLAYER/CUTGARY/CUTPETE`; dog follows `PATH._1_11X2_DOG01`. Five speech beats (Gary 92, Gary 93, Pete 94, Pete 97 + flees on `PATH._1_11X2_PETEFLEE`, Gary 97) with two camera setups (FOV 90 then 40, hardcoded coords near (107,-119,7)). **Skippable with button 7** — every beat checks `bSkipFirstCutscene`. On end/skip: fade, Pete deleted, Gary's health bar shown, dog/Gary/player reset to `_1_11_X2_STARTDOG/STARTGARY/STARTPLAYER`, control back. |
| B2 | **Stage 1: follow the dog** | Objective 01 ("go with the plan", paraphrased). Gary line 98. The dog walks `PATH._1_11X2_DOGGARDEN` with infinite sprint; Gary focus-follows the dog (ignores attacks/stimuli). Gary is mission-critical and armed with 411. |
| B3 | **Stage 1 ambush: Chad** | When Gary enters `TRIGGER._1_11X2_CHADATTACK`: Chad (32) yells (speech 99) and attacks Gary; objective 01B (protect Gary) with a blip on Chad. When Chad's health hits ≤0 Gary turns invulnerable (scripted safeguard). When Chad is KO'd: dog stops and is dismissed, → cutscene 2, then stage 2. |
| B4 | **In-engine cutscene 2: collect the bag** (`F_CutPoop` + thread `T_Cutscene02`) | Fade, control off; Gary and the dog respawned stationary at `_1_11X2_CUTPOOPGARY/CUTPOOPDOG`, player at `_1_11X2_CUTPOOPPLAYER`, camera FOV 80→40 with two hardcoded setups around (132-138, -132, 7-10). Gary kneel-feeds the dog (Gary 100/101), the dog plays chew/defecate anims (player line 102), Gary bags it (weapon 399 + pickup sound, Gary 103), walks to `_1_11X2_GARYHANDBAG` and hands it over — player plays a take/receive anim and is granted weapon 399 ×1. **Skippable with button 7** (`bSkipSecondCutscene`). After: dog and Gary restored to normal, Chad deleted if still around. |
| B5 | **Stage 2: deliver the bag to the staff lounge** | Gary re-recruited, made vulnerable again, stays mission-critical (line 104). Objective 02 + blip on `POINTLIST._1_11X2_BLIPLOUNGE`. Staff-lounge door `DT_ISCHOOL_STAFF` locked. **Bag-keeper thread** (`T_CheckIfPlayerHasPoop`): if the player ever stops holding weapon 399, the blip is pulled and a **15 s countdown** starts to get it back; expiry → fail (message 2) with a disappointment vignette (`F_CutGaryDissapointed`: bag pickups cleared in 50 m, Gary stops being an ally, wanders off, one line `M_1_11X1` event 42, camera on the pair). If the school fire alarm is somehow already ringing, the target blip hides until it stops. When the player stands in `TRIGGER._1_11X2_SHITTARGET` holding the bag: objective 03 ("drop it") and the staff door's prop flag 22 is set (door readied, **INFERRED**); the flag clears if the player steps back out. When a weapon-399 entity is inside the trigger (`ObjectTypeIsInTrigger`): bag placed, drop sound, → stage 3. |
| B6 | **Stage 3: light it and pull the alarm** | Pickups cleared in the target trigger; the bag becomes a persistent placed prop `SC_PooBag` at (-589.35, -295.33, 0.12) in area 2 (`ischool`). Gary line 106; Gary made untargetable/invulnerable, walks `PATH._1_11X2_GARYLIGHTPOOP` and at node 1 plays a kneel-and-light anim. Objective 03B + blip on the fire alarm (`POINTLIST._1_11X2_BLIPALARM`). **15 s mission timer starts: pull the fire alarm before it runs out.** Timer expiry → fail (message 2, same disappointment vignette). |
| B7 | **Payoff + pass** | On `shared.gSchoolFAlarmOn` (set by the alarm-pull elsewhere, **OPEN** which script): timer stopped, control off, widescreen, camera on the bag, Gary line 57, Gary runs `PATH._1_11X2_GARYHIDE`. Burton (55) spawns at `_1_11X2_SPAWNBURTON`, line 113, door-open sound, walks `PATH._1_11X2_BURTONTOSHIT` and at node 1 stomps the bag; Gary watches kneeling. Fire effects (`ShowerSteam2` + `boilerfire2`) slow-killed, a `MudImpact` effect plays at the bag. Pass card `M_PASS` + unlock text id `1_11_x2_UNLKCSTM`, mission-end music. Burton line 114, Gary line 45. Waits for speech and card, fades, deletes the bag prop, everyone made ambient, `MissionSucceed`, fade in, control back. |

**Fail conditions (all verified):**
1. Gary KO'd at any point (mission-critical callback) → fail message 1 (`1_11X2_FAIL_01`).
2. Bag-recovery 15 s timer expires (dropped/lost the bag in stage 2) → fail message 2 (`1_11X2_FAIL_02`) + disappointment vignette.
3. Alarm-pull 15 s timer expires in stage 3 → fail message 2 + vignette.

**Retry/restart:** both cutscenes are replayable in-engine scenes with a skip button; no attempt-count skip logic in this file (unlike 1_01/1_02's baked intros). On a fail between 02:00-08:00 game time, cleanup runs wake-up logic (`F_ProcessWakeUpMissionBasedLogic`) — Halloween night is ending anyway.

**Pass side-effects:** outfit `Ninja_RED` granted, all clothing-slot locks and the clothing manager unlocked, `shared.g1_11X2JustFinished = true`, music stopped. **No `SMissPass.lua` entry exists for 1_11x2** — all rewards are in this script. Knock-on effects elsewhere: `SFaction.lua` (bullies turn hostile), `SMissPass.lua` 1_B block (force-completes 1_11x2 when Help Gary passes in chapter 1), `1_11_Dummy` ignores it (Dummy only checks/offloads dressing).

## 3. Cutscene points

| Marker | Where | Type | Replace with |
|---|---|---|---|
| CS-P1 | B1 the plan (~20 s, 5 lines) | in-engine, skippable | letterbox + two `camera` setups + `say` sequence + skip support; Pete's flee = `npc follow-path` |
| CS-P2 | B4 collect the bag (~25 s, 4 lines + prop anims) | in-engine, skippable | letterbox + `camera` + `say`; if the dog/bag anims aren't built, collapse to: fade → bag in player inventory (`reward` weapon 399) → one Gary `say` |
| CS-P3 | B6 Gary lights the bag | gameplay-adjacent scripted path + anim | `npc follow-path` with node anim callback; not a cutscene |
| CS-P4 | B7 Burton stomps the bag (~15 s, 3 lines) | in-engine, not skippable | letterbox + fixed `camera` + `say` ×3 + prop/fire effects; or fade-to-card minimum |
| CS-P5 | fail vignette (Gary disappointed) | in-engine, short | one `camera` + one `say`, then fail banner |

## 4. NPCs and assets needed

| Role | Model id | Notes |
|---|---|---|
| Gary (costume) | 160 | ally, mission-critical, weapon 411, health bar (`1_11X2_GHEALTH` label) |
| Petey (costume) | 165 | cutscene 1 only, flees and is deleted |
| Chad | 32 | ambushes Gary at the garden; name from script var `pedChad`, **VERIFIED** as far as the script goes |
| Chad's dog | 141 | invulnerable/untargetable runner; stationary actor in cutscene 2 |
| Burton (gym teacher) | 55 | spawn-captured so no ambient duplicates; **INFERRED** identity from context (staff lounge + gym-teacher model id) |

- Weapons/pickups: 399 (poop bag — weapon, pickup, persistent placed prop `SC_PooBag`), 411 (Gary).
- Animation groups: `Halloween`, `W_PooBag`, `Px_Sink`, `POI_Smoking`, `POI_WarmHands`, `MINI_React`, `NIS_1_11`, `QPED`, `GEN_Social`, `NPC_Cheering`; action tree `Act/Conv/1_11X2.act` (nodes: `CutPlan/Gary92`, `Gary93`, `Pete94`, `Pete96`, `Gary97`, `Animations/KneelFeed/KneelFeed`, `DogChew/ChewIn`, `DogShit/ShitIn`, `Stop/Stop`, `Sounds/StuffPooBag`, `Receive/Take`, `GaryLightBag/KneelIn`, `ShitStomp/StompIn`, `KneelIdle/Kneel`, `Failure`, `empty`).
- Audio: sound bank `MISSIONFlameBag.bnk`; 2D sounds `PaperbagDrop`, `PaperbagPicup`, `ESCDOORL_Open`; **~18 scripted speech plays** across `M_1_11X2` ids (92-94, 97 ×2 speakers, 98-104, 106, 113, 114, 45, 57) plus one borrowed `M_1_11X1` line (42) in the fail vignette.
- Effects: `ShowerSteam2`, `boilerfire2`, `MudImpact`.
- Systems flags: punishment system disabled for the mission; door lock `DT_ISCHOOL_STAFF`; door prop flag 22 on `TRIGGER._DT_ISCHOOL_STAFF`; world flag `shared.gSchoolFAlarmOn` (fire alarm, written elsewhere).

Named world data to extract later (from `1_11X2.DAT`; stage-1 garden coords are in area 0 around (107-138, -119 to -132) per hardcoded cameras; stage 2-3 are area 2 around (-589, -295)):
- Points: `_1_11X2_SPAWNGARY`, `_1_11X2_SPAWNPETE`, `_1_11X2_SPAWNDOG`, `_1_11X2_SPAWNCHAD`, `_1_11X2_SPAWNBURTON`, `_1_11X2_CUTPLAYER`, `_1_11X2_CUTGARY`, `_1_11X2_CUTPETE`, `_1_11_X2_STARTDOG`, `_1_11_X2_STARTGARY`, `_1_11_X2_STARTPLAYER`, `_1_11X2_CUTPOOPCAMERA`, `_1_11X2_CUTPOOPDOG`, `_1_11X2_CUTPOOPGARY`, `_1_11X2_CUTPOOPPLAYER`, `_1_11X2_GARYHANDBAG`, `_1_11X2_BLIPLOUNGE`, `_1_11X2_BLIPALARM`, `_1_11X2_DEBUGSPAWNGARY`, `_1_11X2_DEBUGSPAWNPLAYER`.
- Paths: `PATH._1_11X2_DOG01`, `_1_11X2_DOGGARDEN`, `_1_11X2_PETEFLEE`, `_1_11X2_GARYLIGHTPOOP`, `_1_11X2_GARYHIDE`, `_1_11X2_BURTONTOSHIT`.
- Triggers: `TRIGGER._1_11X2_CHADATTACK`, `TRIGGER._1_11X2_SHITTARGET`, door `TRIGGER._DT_ISCHOOL_STAFF`.
- Areas: 0 (outdoors, garden), 2 (`ischool`, staff lounge).

## 5. Framework gaps

Covered by existing primitives: reach_location (blips), trigger_enter (Chad attack, bag target), timer (both 15 s countdowns), flag, fade, letterbox, say, camera, reward (outfit), spawn_enemy (Chad), teleport.

New primitives/conditions needed:
1. **skippable in-engine cutscene primitive** — sequence of say/anim/camera beats, each interruptible by a skip button, with a defined post-skip state (positions, inventory, control). Our biggest single gap across this mission.
2. **ko_specific_target condition** — Chad KO'd advances the stage (distinct from generic combat end).
3. **npc_health_threshold event** — Chad ≤0 → set Gary invulnerable (event + invulnerability action).
4. **defend-ally beat** — ally attacked → objective + blip on attacker; composes 2+3+mission-critical.
5. **follow-target NPC movement** — Gary focus-follows the dog (not a fixed path).
6. **path node callbacks** — run an anim/action when an NPC reaches path node N (Gary lights, Burton stomps).
7. **must-hold-item condition with recovery timer** — drop/lose item → countdown → fail; restore on re-acquire. Plus **item-placed-in-trigger condition** and **persistent placed prop** (bag becomes world geometry), plus **clear pickups in trigger**.
8. **world-flag condition** — mission waits on a global flag written by another system (fire alarm). Our `flag` condition must be able to read world/shared flags, not just mission-local ones.
9. **door lock + door prop flag actions** — lock by door id; toggle a door anim prop flag while a trigger is occupied.
10. **unique-model spawn capture** — suppress ambient spawns of mission characters for the duration (Burton/Chad/dog), restore on cleanup.
11. **punishment disable** + punishers-player-only (shared with 1_11xp doc).
12. **shared-ally health bar with label**, and health-bar show/hide.
13. **cutscene skip button binding** (button 7) as framework-level input.
14. **fail vignette variant per fail cause** — distinct fail sequences keyed by fail message id (we have banner; need per-cause branching in the fail path).

## 6. Open questions

- Which system sets `shared.gSchoolFAlarmOn` and what the alarm interactable is (pull station? any alarm in `ischool`?) — not in this file.
- What creates `shared.gHallowPrankBlip01-08` (likely a Halloween free-roam ambient script; 8 prank blips removed on mission start).
- Dog AI during stage 1 (path with infinite sprint — a scripted run, not real AI) and how Gary's focus-follow resolves if the player lags far behind.
- Whether cutscene-2's bag handover can fail/be interrupted (control is off; assume safe).
- The 15 s alarm timer's UX: is the countdown shown on HUD? `MissionTimerStart` implies yes, but display style is engine-side (**OPEN**).
- Burton's behavior after the pass (made ambient in the school at ~2am) — cosmetic.
