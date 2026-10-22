# 1_09 — The Candidate (recon)

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the data or script), **INFERRED**, **OPEN**.

Sources (read-only, nothing copied): decompiled Lua `Bully-SE-scripts/scripts/chap1/1_09.lua` (1698 lines), pass-rewards `SMissPass.lua`, global chain `main.lua`. This document describes structure and logic in our own words; no dialogue, cutscene script, objective text or narrative text is reproduced. Script/file/native names are citations only.

New area codes seen here (beyond the opening-slice list 0/2/5/14): **19 = the auditorium interior (`isc_audt`)** — speaker/amp prop names are `isc_audt_Aud_*` (**VERIFIED** by prop naming; bounds not checked).

## 1. Identification

**1_09 = The Candidate. VERIFIED.** Evidence:

- Animation group `1_09_Candidate` loaded/unloaded; music stream `MS_Candidate.rsm`.
- Mission body is a defense of the nerd leader (ped model 10, variable `Earnest`) giving a speech in the auditorium (area 19) while jock models attack him; canonical availability "8-18:30" matches the script forcing the clock to 18:45 if earlier.
- `SMissPass.lua` 1_09 branch grants weapon 306 and loads the `Halloween2` special entities with `shared.g1_09JustFinished = true`, i.e. this is the mission directly before Halloween night — matches the canonical order (The Candidate → Halloween).

## 2. Mission flow

`MissionSetup`: baked intro cutscene `PlayCutsceneWithLoad("1-09", true)`, DAT `1_09.DAT` loaded (slot 2), player placed in `ischool` (area 2) at (-669.87, -308.5, 0.01), all peds cleared, no fade-in. Interactive stream `MS_RunningLow02.rsm` starts here.

| # | Beat | Stage function(s) | Logic (functional) |
|---|---|---|---|
| A0 | Baked intro cutscene `1-09` | `MissionSetup` | Plays before control. Skip-on-retry behavior is engine-side, not in this Lua (**OPEN**). |
| A1 | Get to the assembly hall | `main` | Objective + blip on `POINTLIST._1_09_HALLTRIG`; a 30 s mission timer starts. Player must enter a 1×7 m area box at that point. **Fail:** timer expires (message id `1_G1_FAILTIME`) — note the `1_G1_` prefix, fail-string ids were copied from The Diary's script. |
| A2 | Travel to the auditorium | `F_MissionLoop` | On arrival: fade out, interactive music off, special entity set `vote` loaded, area transition to area 19 at `_1_09_AUDTRIG`; wait for streaming, player snapped to `_1_09_CUTSTART` facing heading 0. Clock is paused; if earlier than 18:45 it is set to 18:45. Ped models {16,13,18,20,15,88} and weapon model 306 load. |
| A3 | In-engine intro sweep (~12 s) | `F_MissionLoop` | Widescreen, FOV 90, camera follows `_1_09_NISINTROCAM` looking along `_1_09_NISINTROCAM_LOOK` (speed 0.8/0.2/0.2); an objective-banner text id `1_09_08` shows ~3.5 s; then a second pass on `_1_09_NISINTROCAM2`(+`_LOOK`) for 7.5 s, fade, camera back to player at `_1_09_PSTART`. Player is given weapon 306 with 1 ammo-unit count and made NIS-safe. |
| A4 | Defense setup | `F_MissionLoop` | Player stationary at `_1_09_PSTART`, camera locked to camera 2 (`CameraSetActive(2)`, `CameraAllowChange(false)`), weapon HUD locked, input locked to a whitelist (`PlayerLockButtonInputsExcept(true, 18, 22, 19, 23, 2, 3, 12)`), punishment system disabled, points zeroed, slingshot tutorial `SSLING1` starts. Threads spawned: `F_EarnestSpeech`, `F_JockTaunt`, `F_EnemyTimer`, `F_ProcessAttackers`, `F_CheckFailure`. Objective id `1_09_08` added with a very long reminder time. Speech-by-action-tree disabled (`SoundDisableSpeech_ActionTree`). |
| A5 | The speech (success track) | `F_EarnestSpeech`, `F_SetupDialog` | Earnest stands at `_1_09_EARNEST` (stationary, asleep, untargetable, ignores stimuli/attacks, health 100 with a 5 HP min-health floor, named health bar `N_Earnest` shown even in first-person) and plays 30 scripted speech lines (ids 3–32) in order, each paired with a numbered conversation anim from `Act/Conv/1_09.act`. When all 30 lines finish → success. If Earnest is hit: his line stops, a random hit-response line (id 37) plays, he walks back to his mark if knocked off it, then resumes where he left off. |
| A6 | Attacker waves (the gameplay) | `F_EnemyTimer`, `F_SetupEnemyTable`, `F_CreateAttacker`, `F_ProcessAttackers`, `F_AttackPoint1` | A schedule of 26 jock attackers (models 13/15/16/17/18/20, weapon 312 ×99) spawns at fixed times from 5 s to 158 s; at most 6 alive at once (`JockNum` gate pauses the schedule clock). Each entry has a spawn point, an optional cover point (`PedCoverSet` aimed at Earnest; ranged stats `STAT_109_RANGED`), a follow path, an exit trigger and a red blip. Catwalk entries (no cover point) walk `_1_09_CATWALK1` and go stationary-attacking at path node 2. **Player counterplay:** when the player hits an attacker, that attacker goes invulnerable, has a 25% chance to cower / 50% to look around (`Act/Anim/Ambient.act` mission nodes), then walks its exit path and is deleted on entering its exit trigger; each disposal increments `JocksKilled`. Some disposals (indices 3,6,9,11,12) trigger the crowd reaction (jocks stand + applause). |
| A7 | Mascot event (~1:45 in) | `F_Mascot`, `F_CheckMascot`, `F_MascotWait/Dance/Dance2` | At 105 s the mascot (model 88, health 150) spawns at `_1_09_STAGERIGHT`, walks `_1_09_MASCOTENTER` and dances; while alive and not routed it bleeds Earnest for 0.2 HP per tick. If the player hits it below 80 HP it plays a break node, becomes invulnerable and flees `_1_09_MASCOTEXIT`, despawning at the `_1_09_STAGELEFT` trigger (counts as a `JocksKilled`). Earnest reacts with scripted lines (ids 51, 48, 45) and a cry anim; while paused for this, he re-seats himself at his mark. |
| A8 | Ambient crowd | `F_CreateSeated`, `F_JockTaunt`, `F_StimulateCrowd` | 13 seated audience peds (models 5,9,8,15,13,17,6,21,30,3,14,31,22) at `_1_09_SEATEDPOINTS`, untargetable/invulnerable, running a custom ped tree (`Act/Conv/1_09.act`) with shared-flag stand-up hooks (`g109Nerd`/`g109Jock`, read through `cbNerdStandCheck`/`cbJockStandCheck`). Every 3–5 s a random seated ped plays a jeer/taunt bark from a weighted table (events `JEER`, `TAUNT`, `TAUNT_RESPONSE_DONT_CARE`, plus scripted comment ids on `M_1_09`, `M_4_06`, `M_2_G2`, `M_2_05`). `SoundPlay2D("Applause")` on stimulation. |
| A9 | Speaker breakables (fail track) | `F_SetupSpeakerTable`, `F_SpeakerBroken` | 10 auditorium props (`isc_audt_Aud_Speaker`, `…Speaker01..07`, `…Amp`, `…Amp01`) are registered by name-hash for break events; each break increments `SpeakerCount`. |
| A10 | Win conditions | `F_EarnestSpeech` / `F_EnemyTimer` → `F_Success` | Success when **either** the 30-line speech completes **or** elapsed time passes 165 s (`CompleteTime`) with `JocksKilled >= 26` (i.e. the whole schedule disposed). On success: remaining attackers are disarmed (weapon 312 destroyed) and flagged to flee; then A11/A12. |
| A11 | In-engine closing NIS (~8 s) | `F_EarnestFinishNIS`, `F_EndCut` | Fade, control off, player disarmed, widescreen; two fixed cameras (hard-coded coords around (-770, 308, 78)) on Earnest playing closing anims + lines 34/35/36, then a wide shot on camera path `_1_09_CUT3` looking along `_1_09_NISWIDE` while the crowd stands and applause plays six times. HUD counter/meter explicitly hidden. |
| A12 | Reward NIS + pass | `F_PlayerGotThingyNIS` | `vote` entities unloaded, area transition back to area 2 at `_1_09_ENDTRANS`, wait for area DAT 32; fixed camera in the hall (-619.96, -297.1), player re-equipped with 306 and plays a "pick up slingshot" anim (`/Global/1_09/PlayerPickupSlingshot`); completion screen: `M_PASS`, respect messages `MRESPECT_JM5`, `MRESPECT_NP5`, unlock message id `1_09_UNLOCK`; faction respect set (1→70, 2→45); the old slingshot (303) is destroyed; mission end music; `MissionSucceed`. `MissionCleanup` then loads `Halloween2` special entities and **pauses the clock** (time-stopped Halloween night begins). |

Fail conditions (all routed through `F_FailStuff`, which has Earnest play line 45, flee `_1_09_FLEEEARNEST` invulnerable, seats the crowd as ambient, plays fail music):

| Fail | Cause | Message id |
|---|---|---|
| Timer | A1's 30 s reach-the-hall timer expires | `1_G1_FAILTIME` |
| Earnest hurt | `F_CheckFailure`: Earnest health ≤ 5% (his min-health floor is 5) | `1_G1_FAILHUM` |
| Speakers | `F_CheckFailure`: all 10 registered speaker/amp props broken | `1_G1_FAILSPKR` |

On fail while in area 19 there is extra teardown: wait 5.5 s, weapon HUD unlock, unequip, destroy weapon 306, transition the player back to area 2 `_1_09_ENDTRANS`, unload `vote`. Retry/restart: not described in this Lua beyond the engine's cutscene handling (**OPEN** whether retry skips `1-09`; the `PlayCutsceneWithLoad(..., true)` flag suggests skippable).

Dead/ancillary code: `F_Debug` (jump straight into the defense), `F_SpeechCut` (a short reaction cut on the first attacker — defined but never called; the call site in `F_CreateAttacker` was replaced by empty `F_Sleep`), `gCutscene2Enabled` (external/debug skip to `_1_09_ENDC1`), `F_SetupStudents`/`F_CreateStudents` (empty).

Chains to: **1_10 Halloween** — pass effects set `g1_09JustFinished`, load `Halloween2` and freeze the clock (**VERIFIED** in `MissionCleanup` + `SMissPass.lua`; the actual mission launch wiring lives outside the Lua, **OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Content (functional) | Suggested replacement |
|---|---|---|---|---|
| 1_09-CS1 | Before A1 | baked (`1-09`) | Pre-assembly scene | `fade` + `letterbox` + 1-2 `say` lines or a title `banner` |
| 1_09-CS2 | A3 | in-engine | Two camera paths over the auditorium + objective banner (~12 s) | `camera` (fixed shots or a simple dolly along 2-3 keys) + `letterbox` + `banner`; camera path data exists in `1_09.DAT` if we want to reuse it |
| 1_09-CS3 | A11 | in-engine | Earnest's closing thanks, two fixed cams + wide path, crowd applause (~8 s) | `letterbox` + `camera` ×2 + `say` ×3 + applause SFX event |
| 1_09-CS4 | A12 | in-engine | Slingshot pick-up anim + completion screen | `camera` + paired `say`/anim + `reward` + `banner` |

Crowd stand/applause beats inside CS3 are driven by shared flags (`g109Nerd`, applause 2D sound), not the cutscene.

## 4. NPCs and assets needed

**Ped models (numbers as scripted, unchecked):** 10 (Earnest), jocks 13/15/16/17/18/20, mascot 88, seated audience 5/9/8/15/13/17/6/21/30/3/14/31/22 (13 seats), plus the load-list duplicates 16,13,18,20,15,88.

**Animation groups:** `1_09_Candidate`, `NPC_Mascot`, `NIS_1_09`, `1_03The Setup`, `IDLE_NERD_A`, `Cheer_Nerd1`. **Action trees:** `Act/Conv/1_09.act` (speech anims, seated custom tree, player pickup), `Act/Anim/NPC1_09.act` (mascot dance/break, Earnest speech/cry, catwalk attacker tree), `Act/Anim/Ambient.act` (cower/lookaround), `Act/Anim/J_Striker_A.act` (attacker locomotion).

**Weapons/pickups:** 306 (player defense weapon, mission-granted, **INFERRED** = super slingshot: replaces destroyed 303 and pairs with tutorial `SSLING1`), 312 (attacker thrown weapon ×99, **INFERRED** = eggs), 303 destroyed on pass (the 1_03-era slingshot).

**Props:** 10 breakable speakers/amps `isc_audt_Aud_Speaker[01-07]`, `isc_audt_Aud_Amp[01]`; special entity set `vote` (auditorium dressing) loaded/unloaded around the defense; `Halloween2` loaded after pass.

**Speech usage (counts only):** ~36 distinct scripted `M_1_09` line ids (3–38 range plus 45/48/51), plus crowd bark events (`JEER`, `TAUNT`, `TAUNT_RESPONSE_DONT_CARE`, `M_4_06`, `M_2_G2`, `M_2_05`) fired every 3–5 s. Music: `MS_RunningLow02.rsm` (setup), `MS_Candidate.rsm` (defense), pass/fail end music; `Applause` 2D SFX.

**Special entities:** `AreaLoadSpecialEntities("vote", …)`, `AreaLoadSpecialEntities("Halloween2", …)`.

## 5. Framework gaps (existing primitives → what's needed)

Existing: conditions reach_location, trigger_enter/exit, interact, event, timer, flag, all/any; actions hint, banner, event, set_flag/clear_flag, log, teleport, start_mission, spawn_enemy, fade, reward, letterbox, say, camera, shake.

| Beat | Covered by | NEW primitive(s) |
|---|---|---|
| A1 reach + countdown fail | reach_location + timer (partially) | `condition: timer_expired` wired to mission fail with message |
| A2 area hop with streaming wait | teleport (partially) | `action: area_transition` (area code + point + wait-for-stream) |
| A3/CS camera paths | camera (fixed only?) | `action: camera_path` (path ref, look-target path, speed, FOV) + `action: camera_lock` |
| A4 input whitelist | none | `action: lock_inputs` (except-list) |
| A4 punishment/population off | none | `action: world_toggle` (punishment, POI, population override) |
| A4 clock to 18:45 + pause | none | `action: clock_set_min` / `clock_pause` |
| A5 speech sequencer | say (single lines) | `system: speech_sequencer` — ordered line list with per-line anim, advance-on-finish, interrupt-on-hit with response lines, resume-at-line; drives `say` + anim |
| A5 VIP health floor + named bar | none | `action: npc_set_protection` (min-health, invulnerable, untargetable, ignore-stimuli/attacks), `action: show_health_bar`, `condition: npc_health_below(pct)` |
| A6 wave schedule | spawn_enemy (one-shot) | `system: wave_spawner` — timed spawn table (time, model, weapon, spawn/cover/exit refs, blip), concurrency cap, disposal counter; `condition: counter_gte` (JocksKilled ≥ 26); `condition: ped_hit_by(player)` per-spawn flee behavior; `action: npc_follow_path` with per-node callbacks (`ped_reached_path_node`) |
| A7 mascot | spawn_enemy + events | same as A6 plus `action: npc_damage_over_time(target)` or scripted periodic damage; flee-on-hit below HP threshold |
| A8 crowd barks/stand | none | `system: ambient_bark_loop` (weighted table, interval range, seated group) + `action: crowd_react` (shared flag + applause SFX) |
| A9 breakables | none | `condition: prop_destroyed_count` over a named-prop registry (`action: register_breakables`) |
| A10 dual win (speech done OR time+kills) | all/any + flag/event | `condition: speech_sequence_done`, combined with timer + counter via any() |
| A11/A12 reward NIS | letterbox, camera, say, reward | `action: give_weapon`/`equip_weapon`/`destroy_weapon`, `action: faction_set_respect`, `action: mission_complete_screen` (pass screen with respect/unlock message slots) |
| Retry/skip | none | `condition: attempt_count` (skip intro on retry) |
| Fail variants | none | `action: mission_fail` with message key + optional respawn point |
| Music | none | `action: play_music` / `stop_music` (stream name, volume) |

## 6. Open questions

- Retry behavior: does a restart replay baked cutscene `1-09`? Not in the Lua.
- The 30 s A1 timer: real-time seconds via `MissionTimerStart(30)`; units assumed seconds (**INFERRED**).
- How the `g109Nerd`/`g109Jock` shared flags map to seated-ped stand-up anims: the custom ped tree in `Act/Conv/1_09.act` is undecoded — same class of unknown as the 1_02 tutorial trees.
- Exact cover-shooting cadence of attackers (`PedCoverSet` params are in the script but the fire rate/accuracy lives in `STAT_109_RANGED` and the weapon def for 312, both unread).
- `MinigameSetCompletion("M_PASS", true, 1000)` third argument — money cents? Two later calls pass 0; exact semantics **OPEN**.
- Whether attackers' eggs can break the speakers (fail interplay) or only the player can — break handler counts any break event on the 10 props, source-agnostic.
- Which engine system consumes `g1_09JustFinished` to actually launch 1_10 — mission table is outside the Lua.

## 7. World-data names to extract from `1_09.DAT` (Trigger.img)

**POINTLIST:** `_1_09_EARNEST`, `_1_09_HALLTRIG`, `_1_09_AUDTRIG`, `_1_09_CUTSTART`, `_1_09_PSTART`, `_1_09_ENDC1`, `_1_09_ENDTRANS`, `_1_09_SEATEDPOINTS` (13), `_1_09_AISLELEFT`, `_1_09_AISLERIGHT`, `_1_09_STAGELEFT`, `_1_09_STAGERIGHT`, `_1_09_BOXLEFT1`, `_1_09_BOXLEFT2`, `_1_09_BOXRIGHT1`, `_1_09_BOXRIGHT2`, `_1_09_CATWALK1`, `_1_09_COVERFLOORL`, `_1_09_COVERFLOORL1`, `_1_09_COVERFLOORR`, `_1_09_COVERSTAGEL`, `_1_09_COVERSTAGER`, `_1_09_COVERL1`, `_1_09_COVERL2`, `_1_09_COVERR1`, `_1_09_COVERR2`.

**PATH:** `_1_09_AISLELEFT`, `_1_09_AISLERIGHT`, `_1_09_STAGELEFT`, `_1_09_STAGERIGHT`, `_1_09_BOXLEFT1`, `_1_09_BOXLEFT2`, `_1_09_BOXRIGHT1`, `_1_09_BOXRIGHT2`, `_1_09_CATWALK1`, `_1_09_MASCOTENTER`, `_1_09_MASCOTENTER2`, `_1_09_MASCOTEXIT`, `_1_09_FLEEEARNEST`, `_1_09_NISINTROCAM`, `_1_09_NISINTROCAM_LOOK`, `_1_09_NISINTROCAM2`, `_1_09_NISINTROCAM2_LOOK`, `_1_09_CUT2`, `_1_09_CUT3`, `_1_09_NISWIDE`.

**TRIGGER:** `_1_09_AISLELEFT`, `_1_09_AISLERIGHT`, `_1_09_STAGELEFT`, `_1_09_STAGERIGHT`, `_1_09_BOXLEFT1`, `_1_09_BOXLEFT2`, `_1_09_BOXRIGHT1`, `_1_09_BOXRIGHT2`, `_1_09_CATWALK1` (all attacker despawn gates; `_1_09_STAGELEFT` doubles as mascot despawn).

**Props by name:** `isc_audt_Aud_Speaker`, `isc_audt_Aud_Speaker01`–`07`, `isc_audt_Aud_Amp`, `isc_audt_Aud_Amp01`. **Special entity sets:** `vote`, `Halloween2`.

**Area codes used:** 2 (ischool hall), 19 (auditorium). Hard-coded camera coords in area 19 (~(-770, 308, 78)) and area 2 (~(-620, -297, 6.6)).
