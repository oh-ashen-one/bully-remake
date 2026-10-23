# 3_S03 — Cheating Time: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/3_S03.lua` (1041 lines). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_S03 = Cheating Time.** **VERIFIED** by content (photograph a prep selling test answers to greasers, then jocks, then to teacher Hattrick himself; deliver the evidence to Crabblesnitch, who confronts Hattrick in the outro) and externally: [The Cutting Room Floor notes Cheating Time's script id is 3_S03](https://tcrf.net/Bully_(PlayStation_2)/Oddities), filed as a Chapter III *side* mission although it only becomes available in Chapter V (catalog #54 lists it under Chapter 5). Same S-numbering caveat as 1_S01 in the ch1 docs: the id's chapter digit is an authoring-era artifact, not the unlock chapter.

Structure: four hand-rolled stages (`F_Stage1..4` setup/loop pairs with flag-based advancement) plus a debug mode (`bDebugMode` + `F_StartAtStage2/3/4` — dev-only). Camera (item 328/426) is the core mechanic via engine photo natives `PhotoTargetInFrame`, `PhotoSetValid`, `PhotoHasBeenTaken`.

## 2. Mission flow

Driver: `MissionSetup()` (baked intro `"3-S03"`) → `main()` → `F_SetupMission()` → stages 1→4 → pass branch.

| # | Beat | Logic (functional) |
|---|---|---|
| C0 | **Setup** | `3_S03.DAT`; anim group `NIS_3_S03`; action trees `Act/Conv/3_S03.act` + three gift trees (`NIS_3_S03_GiveCas/GiveHat/GiveVan` — the hand-off animations per stage, VERIFIED by name); bottle prop model 327 preloaded; photo counter HUD (max 3, `HUDIcon_photos`). |
| C1 | **Stage 1: greaser deal** | Blip at `_3_S03_OBJECTIVEGREASER`. Entering its trigger plays `F_NIS_Greaser`: prep (34) + greasers (27 Vance-role, 26 Lucky-role — roles **INFERRED** from node names) spawn; greaser/prep wait nodes; a two-bark exchange; player repositioned to `_3_S03_FACETARGETGREASER` with camera item auto-equipped (`F_ResetCamera`: prefers 426, else 328). Then a **15 s photo window**: prep locks onto the greaser and loops a `Give_Attempt` anim; stealth music tier (`MS_StealthLow/Mid/High`). Snap the prep in-frame → counter 1, timer stopped, everyone disperses ambient. |
| C2 | **Stage 2: pool deal** | Blip at `_3_S03_OBJECTIVEJOCK`; `_3_S03_POOLAREA` trigger swaps objectives mid-approach; `_3_S03_OBJECTIVEPOOL` plays `F_NIS_Jock` (pool area — jock 17 + Mandy 14 + a second pool prep 34; jock/Mandy chat barks, prep walks in, same give-attempt loop). Same 15 s photo window → counter 2. The pool prep then walks out (`_3_S03_PREPLEAVEPOOL`, deleted at path node 2) and a blip marks the pool exit; leaving to area 0 launches the alley-prep thread (`T_LaunchAlleyPrep`: re-blip the original prep, he walks `_3_S03_PREPALLEY01` after 3 s). |
| C3 | **Stage 3: the teacher's deal** | Entering `_3_S03_OBJECTIVEALLEY` plays `F_NIS_Hattrick`: Hattrick (61) spawns; the prep is made invulnerable and re-positioned; two-bark NIS; prep idles in the give pose; background conversation thread (`T_HattrickConversation`, two more prep barks). Same 15 s window, photo target = Hattrick → counter 3 → `F_NIS_DealDone`: prep walks off (`_3_S03_DEALDONEPREPLEAVE`), Hattrick barks and leaves; both released. |
| C4 | **Stage 4: deliver the photos** | Counter hidden; blip back at the parking lot (`_3_S03_SPAWNGALLOWAY`). If the abandoned prep ever sees the player during this stage he taunts and attacks (one-shot branch). Entering `_3_S03_PARKINGLOT` plays `F_NIS_Outro`: Crabblesnitch (65), Galloway (57) and Hattrick (61) spawned at `_3_S03_OUTRO*` points; bottle model 361 loaded; ~10-line four-way scene (Hattrick ids 48/53/55, Crabblesnitch 49/51/54, player 50/52/56/57) across six hardcoded camera shots — the confrontation/firing scene (**INFERRED** from casting + arc knowledge) — then all three deleted. Pass: `M_PASS` banner with 6000 ($60), pass music, control back. |
| — | **Galloway wanderer (all stages)** | `T_HandleGalloway` thread: while the player is in the parking-lot trigger, Galloway (57, faction 9) exists at `_3_S03_SPAWNGALLOWAY` playing a give-bottle idle and walking `_3_S03_GALLOWAY`; leaving deletes him. He's mission-critical — hurting him fails with key 3 (see fails). This is the drunk-Galloway continuity gag, **INFERRED** from the GiveBottle node + faction 9 + the 1_S01 storyline. |

### Fail conditions (per stage, keyed)

| Cause | Mechanism | Fail key |
|---|---|---|
| Photo timer (15 s) expires | `MissionTimerHasFinished` per stage | `3_S03_FAIL_01` |
| Spotted: player enters the stage's `_3_S03_FAILGREASER/_FAILPOOL/_FAILALLEY` trigger | target aggros the player, prep flees | `3_S03_FAIL_02` |
| Any mission-critical ped (prep / greasers / jock+Mandy / Hattrick) hurt or KO'd | `PedSetMissionCritical(..., F_MissionCriticalX)` per stage | `3_S03_FAIL_02` |
| Galloway hurt | `F_MissionCriticalGalloway` (he turns hostile faction 8 and attacks) | `3_S03_FAIL_03` |

No checkpoints; fail replays from the baked intro.

### Rewards / pass side-effects

- $60 banner money (6000). No respect deltas, no unlocks; no `SMissPass.lua` case (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked `"3-S03"` | `fade` + original card |
| CS-2 | C1/C2/C3 deal NIS ×3 | in-engine, 2-4 barks, 1-3 hardcoded cams each | `camera` ×2 + `speech_sequence` + spawn/reposition |
| CS-3 | deal-done / prep-walk-off micro-beats | in-engine | `npc_follow_path` + `say` |
| CS-4 | C4 outro (4 characters, ~10 lines, 6 shots) | in-engine | `speech_sequence` + `camera` set — the file's biggest scene |

Anim groups: `NIS_3_S03`, `NIS_3_S03_B` (outro). Action trees: `Act/Conv/3_S03.act` + `Act/Gifts/NIS_3_S03_GiveCas/GiveHat/GiveVan.act`. Debug-only leftovers: `bDebugMode`/`bDebugStage`/`F_StartAtStage2/3/4` (stage 3's debug path uses `4_01_MOBJ_*` keys — copy-paste fossil, VERIFIED dead unless debug flag set).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 34 | the prep (dealer) | reused across stages 1/3; separate pool prep (34) in stage 2; mission-critical, flag 108 set / 117 cleared in stage 3 |
| 27, 26 | greaser buyers | stage 1 |
| 17, 14 | jock buyer + Mandy | stage 2 (pool) |
| 61 | Hattrick | stage 3 + outro |
| 57 | Galloway | parking-lot wanderer (faction 9; → 8 + hostile if hurt) + outro |
| 65 | Crabblesnitch | outro only |
| 29 | model requested in `F_PedLoadModels` but never spawned (**OPEN** fossil) | |

### Props / items

- Camera items **426** (digital) / **328** — auto-equipped after each NIS.
- Prop models 327 (bottle, setup) and 361 (outro bottle) preloaded.

### Speech / music

`M_3_S03` ids seen: 8, 14, 19, 20, 21, 35, 37, 39, 46, 48-57, 59-62 — ~20 distinct lines, heaviest in the outro. Stage photo windows run the `MS_StealthLow/Mid/High` interactive tier (stage 1 setup; VERIFIED).

### Named world data to extract from `3_S03.DAT` (DAT present, VERIFIED)

- POINTLISTs: `_3_S03_SPAWNGALLOWAY`, `_3_S03_OBJECTIVEGREASER/_JOCK`, `_3_S03_SPAWNPREPAUTO`, `_3_S03_SPAWNGREASERAUTO01/02`, `_3_S03_SPAWNPREPPOOL`, `_3_S03_SPAWNJOCKPOOL`, `_3_S03_SPAWNMANDYPOOL`, `_3_S03_SPAWNPREPALLEY(+CUT)`, `_3_S03_SPAWNHATTRICKALLEY`, `_3_S03_PREPMOVETO`, `_3_S03_PREPHATTRICKMOVETO`, `_3_S03_FACETARGETGREASER/POOL/ALLEY`, `_3_S03_BLIPPOOLEXIT`, `_3_S03_OUTROCRABBLESNITCH/GALLOWAY/HATTRICK/JIMMY`, `_3_S03_SPAWNPLAYER`, `_3_S03_DEBUGSPAWNPLAYERSTAGE2/3/4`.
- PATHs: `_3_S03_GALLOWAY`, `_3_S03_PREPPOOL`, `_3_S03_PREPLEAVEPOOL`, `_3_S03_PREPALLEY01`, `_3_S03_DEALDONEPREPLEAVE`.
- TRIGGERs: `_3_S03_OBJECTIVEGREASER/POOL/ALLEY`, `_3_S03_POOLAREA`, `_3_S03_FAILGREASER/FAILPOOL/FAILALLEY`, `_3_S03_PARKINGLOT`.
- Hardcoded cameras: stage NIS shots (114.80, -3.19, 7.54 → …; 119.42, 3.57, 7.87 → …; 115.74, 1.33, 7.97 → …; pool -673.45, -77.60, 60.82 → …; alley 67.58, -107.81, 8.25 → …; outro six-shot set around (171.7, -9.5, 7.2) / (169.1, -3.3, 7.1) / (165.7, -8.5, 7.4)). Areas: **0** and **13** (pool/gym block, per the debug transition `AreaTransitionPoint(13, …)`, VERIFIED).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| photo mechanic | — | **photo_evidence_stage** (equip camera item → target-in-frame predicate + valid-frame HUD flag + taken latch; success = photo of ped X within the window). The camera natives are engine-side; this is the mission-level wrapper |
| 15 s windows | `countdown_fail_timer` (pool) | reuse + a **spotted_fail_trigger** per stage (enter → target aggros, mark flees, keyed fail) — a fail-trigger variant of `stealth_spotter` |
| give-attempt loop | `pair_anim` | **loop_until_snapped** (mutual lock + repeated give anim while the photo window runs; cosmetic) |
| Galloway wanderer | `spawn_npc`/`npc_despawn` | **presence_gated_npc** (exists only while player inside trigger X; mission-critical with a hostile-on-hurt fail variant) |
| C4 prep revenge | `npc_*` | **abandoned_mark_vengeance** (one-shot: if the duped ped sees the player in stage 4, taunt + attack — fold into a flag branch) |
| camera equip | `give_item`/weapon set | **equip_item_if_owned** (prefer 426 else 328 — conditional equip helper) |
| outro | `speech_sequence`, `camera` | covered (multi-shot variant) |

## 6. Open questions

- How the mission is gated to Chapter 5 availability despite the 3_S03 id (launcher table; TCRF confirms the oddity).
- Photo evaluation internals: does `PhotoTargetInFrame(ped, 2)` need the target centered/zoomed; what `PhotoSetValid` drives in the HUD; whether shots are saved to the photo album (this predates/performs alongside the `HUDPhotographySetSaveLevel` unlocks from 2_B/3_B).
- The "joshLazyHack" (a photo counts if the target is valid now OR was last frame) — lag-compensation read VERIFIED in-file; keep the same leniency in ours.
- Model 29 requested-but-unused; `4_01_MOBJ_*` keys in the stage-3 debug path (fossils from 4_01 "Here's to You, Ms. Phillips"-era code sharing, **INFERRED**).
- Whether failing Hattrick's stage has unique aggro behavior vs the other two (his mission-critical callback makes him attack; the others make the buyers flee).
