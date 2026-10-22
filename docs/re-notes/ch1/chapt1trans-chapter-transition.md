# Chapt1Trans — chapter 1 → 2 transition interstitial

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources (read-only, nothing copied): decompiled Lua `Bully-SE-scripts/scripts/chap1/Chapt1Trans.lua` (176 lines, complete), cross-refs `SMissPass.lua` (`F_SetupMissionRequirements` 2_S06 block, `F_LaunchingMission` 2_S04 block, 1_B chapter-advance block). This document describes structure and logic in our own words; no dialogue, narration text, or cutscene script is reproduced. Script names are citations only.

## 1. Identification

**This is the transition that closes chapter 1 and opens chapter 2** — the flyover + chapter title card + baked chapter-2 intro cutscene. Note this corrects the "chapter-1 intro" reading: it is played at the *end* of chapter 1, not the start of the game. Confidence: **VERIFIED**.

Evidence:
- `ChapterSet(1)`: chapter indices are **0-based** in this engine. Cross-checked across `SMissPass.lua`: 1_B's block calls `ChapterSet(1)` (Help Gary ends chapter 1 → chapter 2); 2_B calls `ChapterSet(2)` with a "setting chapter to 3" print; 3_B → `ChapterSet(3)` "chapter 4"; 5_01 → `ChapterSet(4)` "chapter 5". So `ChapterSet(1)` here advances into chapter 2.
- It ends by playing baked cutscene `"2-0"` — the chapter 2 intro (cutscene naming matches `1-1-1`/`1-1-2` convention).
- `MissionCleanup` seeds faction respect values including cliques that only matter from chapter 2 on (Greasers, Preppies): type 11 (Bullies) 100, 1 (Nerds) 75, 5 50, 4 50, 2 45, 3 0. Faction-id→clique mapping **INFERRED** from `SFaction.lua` usage and story order (Bullies adore the player post-Russell).
- Launch points: `SMissPass.lua` fires `DebugMissionPassDependant("Chapt1Trans")` when 2_S06 passes and when 2_S04 launches — i.e. it runs at the very start of chapter-2 content.

## 2. Mission flow

Single linear sequence, no stages, no fail path.

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **MissionSetup** | Speech/ambience/PA silenced, all peds' POIs cleared, all peds cleared, population override zeroed, no fade-in. `Chapt1Trans.DAT` loaded. Widescreen on. If outdoors (area 0): closed-gate geometry instances placed at both front gates — (301.44, -72.51, 8.05) and (225.93, 5.80, 8.39) — and the nav radius around them sealed (`AreaSetPathableInRadius`); the school is being locked behind the player (**INFERRED** narrative purpose). Models loaded: 69, 145, 3, 70, 73, 30, 35. Clock fixed 08:30. Front-end/select screens disabled. |
| T1 | **Player + camera set** | Player made NIS-safe, health forced to 200, punishment points zeroed, `ChapterSet(1)`, control off, weather id 4 (**OPEN** which look). Front-gate door anim (`TRIGGER._TSCHOOL_FRONTGATE`) deleted and re-created fresh. Player moved to `POINTLIST._CHAPT1TRANSPLYR` (area 0). Static camera at (215.46, -73.08, 9.91) looking at the gate; narrator line 105 preloaded (blocking until loaded). |
| T2 | **Flyover + chapter card** | Camera attaches to `PATH._CHAPTTRANS_FOLLOW` looking at (289.52, -73.00, 7.99), speed 2. Chapter title card raised and **held** (`MinigameSetChapterCompletion("Chapt1Message", "Chapt1Name", true, 0)` + `MinigameHoldCompletion`). Ambient vignette spawned: five students from `POINTLIST._CHAPTTRANSPLACEMENT` elements 1-5 walking paths `_CHAPTTRANS_FOLLOW1B/1A/2A/2B/3A`, plus a chase pair — small student (69) sprinting `_CHAPT1CHASE` 1→3, bully (145) attacking him. Fade in, camera speed up (8), narration 105 plays. |
| T3 | **Narration chain** | Three narrator lines (105 → 106 → 107), each preloaded during the previous one and played back-to-back; gates on speech-finished. During 107's start the front gate anim opens. |
| T4 | **Card down, wrap up** | Chapter card released, wait for it to clear, fade out. Vignette peds deleted, models unloaded (quirk: unload list has **4** where the load list had 69 — script typo or intentional, **OPEN**). Player un-safed, sound/ambience resumed, widescreen off, weather released. |
| T5 | **Chapter 2 intro + handoff** | Baked cutscene `"2-0"` plays (with load). `shared.g2_03_shirt = true` (seeds a chapter-2 clothing beat, **OPEN** detail). Camera control for area transition disabled, player transitioned to area 2 (`ischool`) at `POINTLIST._CHAPT1TRANS`, faced heading 90, control on, camera back to player. `shared.updateDefaultKOPoint` and `shared.addBusPoints` set (KO respawn moves to the school; bus-stop blips enabled — **INFERRED** from names). `MissionSucceed(true, ...)` — first arg true (chapter-complete style pass, **INFERRED**). |
| T6 | **MissionCleanup** | Front-gate anim deleted, population reverted, queued speech purged, transition camera lock released, DAT unloaded, PA/ambience/speech restarted. Faction respect defaults written (see Identification). Saved-data slots: slot 3 ← times-busted count, slot 14 ← 0. Front-end screens re-enabled. |

**Fail conditions:** none. No retry behavior needed or present.

## 3. Cutscene points

| Marker | Where | Type | Replace with |
|---|---|---|---|
| CS-T1 | T2-T3 flyover + narration + chapter card | in-engine (camera path + VO + card) | `camera` on a spline (our camera primitive needs path playback, see gaps) + `say` ×3 with preload + `banner`/chapter card held; vignette peds optional (our ambient spawner can fake it or skip) |
| CS-T2 | T5 cutscene `"2-0"` | baked | Out of scope for the Halloween chain; stub with fade + chapter card if not building chapter 2 yet |

## 4. NPCs and assets needed

| Role | Model id | Notes |
|---|---|---|
| Small fleeing student | 69 | `GN_Hispanicboy` per opening-slice doc |
| Chasing bully | 145 | `GN_Bully04` per opening-slice doc |
| Walking students ×5 | 3, 70, 73, 30, 35 | ids only, names **OPEN** (73 also appears in 1_02A-era dorm scenes; 70 in the opening slice's ambient set) |

- Animation: none loaded directly (vignette peds just path-follow).
- Speech: **3 narrator lines** (`NARRATION` events 105, 106, 107, `supersize` pool), preloaded-and-queued.
- Props/geometry: `ScGate01Closed` / `ScGate02Closed` closed-gate geometry instances; front-gate door anim `TRIGGER._TSCHOOL_FRONTGATE` (delete/create/open).
- UI: chapter title card (`Chapt1Message`/`Chapt1Name` keys) with hold/release.

Named world data to extract later (from `Chapt1Trans.DAT`):
- Points: `POINTLIST._CHAPT1TRANSPLYR`, `_CHAPTTRANSPLACEMENT` (elements 1-5), `_CHAPT1CHASE` (elements 1-3), `_CHAPT1TRANS` (area 2 destination).
- Paths: `PATH._CHAPTTRANS_FOLLOW` (camera), `_CHAPTTRANS_FOLLOW1B`, `_1A`, `_2A`, `_2B`, `_3A`.
- Triggers: `TRIGGER._TSCHOOL_FRONTGATE` (door anim id).
- Hardcoded coords (in script): camera (215.46, -73.08, 9.91) → (216.46, -73.08, 9.89); camera look-at (289.52, -73.00, 7.99); gate instances (301.44, -72.51, 8.05) and (225.93, 5.80, 8.39) + nav-seal radii.
- Areas: 0 (outdoors) → 2 (`ischool`).

## 5. Framework gaps

Covered by existing primitives: fade, letterbox, say (×3, needs preload variant), teleport, set_flag (shared flags), camera (static setups), reward (none needed), spawn_enemy → spawn_npc for the vignette.

New primitives/conditions needed:
1. **camera path playback** — attach camera to a named path with speed control and look-at target (distinct from our fixed `camera` shots).
2. **chapter card** — held/released title card distinct from a timed `banner` (hold until explicitly released, with completion-card styling).
3. **geometry instance swap** — place/remove named closed-gate geometry at coordinates + seal nav radius (shared with the 1_01 closed-gate swap in the opening slice).
4. **chapter advance action** — `chapter_set` + faction-respect defaults + KO-respawn-point update as pass side-effects (an `on_pass` block again).
5. **baked-cutscene launch with load** — `play_cutscene "2-0"` slot in a mission flow (framework needs the concept even if content is stubbed).
6. **narration preload chain** — `say` with preload-ahead so long VO lines chain without gaps.
7. **population suppression** — clear all peds + zero population override + restore (shared with 1_01/1_02 needs).

## 6. Open questions

- The unload-list quirk (loads 69, unloads 4): typo or model-alias behavior in the engine.
- Weather id 4's actual look.
- What `shared.g2_03_shirt` drives (a 2_03 mission clothing beat — relevant only when we reach chapter 2).
- Whether the transition can be skipped/replayed (nothing in Lua suggests a skip; the frontend-disable implies it's unskippable).
- Exact trigger for firing Chapt1Trans relative to 1_B passing and the 2_S04/2_S06 class missions (the `DebugMissionPassDependant` mechanism is engine-side, **OPEN**).
