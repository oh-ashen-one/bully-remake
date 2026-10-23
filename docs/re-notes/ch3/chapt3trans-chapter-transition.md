# Chapt3Trans — Chapter 3 → 4 interstitial: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/Chapt3Trans.lua` (150 lines, complete) + the ch1/ch2 docs' transition pattern (`chapt1trans-chapter-transition.md`, `chapt2trans-chapter-transition.md`). Structure described in our own words; no narration text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**Chapt3Trans = the Chapter 3 → Chapter 4 interstitial.** **VERIFIED** by: file name; `ChapterSet(3)` (0-indexed — entering the fourth chapter; 3_B's `SMissPass` case calls the same); chapter-completion banner keys `Chapt3Trans_Title`/`Chapt3Trans_Name`; three `NARRATION` lines (112-114); the closing baked cutscene `"4-0"` (Chapter 4 intro — matches Chapt2Trans's `"3-0"` pattern); and the final handoff with Pete waving bye.

Differences from Chapt2Trans: **no Christmas entity load** (winter ends with Chapter 3 — the removal happens in 3_B's `SMissPass` case, VERIFIED there); the flyover is town-people vignettes rather than the construction reveal; no borrowed DAT from another mission; and it ends on the same map (area 0) rather than the dorm. Launch path (presumably the mission table after 3_B) is outside the Lua — **OPEN**.

## 2. Mission flow

Single linear `main()` after `MissionSetup()`; no objectives, no fail path, no retry logic.

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Setup** | `Chapt3Trans.DAT` (layer 2); speech action trees disabled; POI cleared, all peds cleared, population zeroed; `ChapterSet(3)`; `Hang_Talking` anim group + `Act/Conv/Chapt3Trans.act`; ped models {71, 85, 102, 69, 5, 39} + Pete 134; clock forced to 08:30; **front-end/select screens disabled** (`SystemEnableFrontEndAndSelectScreens(false)`); no fade-in. |
| T1 | **Street flyover + vignettes** | Player health 200, punishment zeroed; player transitioned to area 0 at `_CHAPT3TRANSPED` element 1, Pete (134) spawned at element 2; control off, widescreen. Hardcoded camera start (298.86, -73.14, 9.27 → 297.86, -73.11, 9.28), then `PATH._CHAPT3TRANS_CAM` at speed 2 looking at a fixed point (249.26, -72.78, 9.22). Set dressing: a chase pair — ped 69 flees `_C3T_FLEE`, ped 102 attacks/chases it; a heavyset walker (71) strolls `_C3T_FATBOY` while 85 stands by (`_CHAPT3TRANSFATTY`); a couple (5, 39) walk `_C3T_COUPLE1/2` — a "life goes on in Bullworth" montage, **INFERRED** from staging. |
| T2 | **Title card + narration** | Chapter-completion banner raised and held (`MinigameSetChapterCompletion` + `MinigameHoldCompletion`); fade in; player and Pete start walking `_C3T_PLAYER`/`_C3T_PETEY`; camera speed up (10,10,10); narration 112, 113, 114 played strictly sequentially with preload-while-playing (three lines — Chapt2Trans had four). Banner released and awaited. |
| T3 | **Chapter 4 intro cutscene** | Fade out; vignette peds + Pete deleted; models unloaded; baked cutscene `"4-0"` with load. |
| T4 | **Pete farewell handoff** | Pete re-spawned at `_CHAPT3ENDPETE`, player placed at `_CHAPT3ENDPLYR` facing 180°; camera reset to player; Pete plays his `JimmyBye` node (`/Global/Chapter3Trans/JimmyBye`) with a `BYE` scripted speech event, is made ambient, and walks off to (-159.92… hardcoded XYZ 187.708, -159.923, 8.944); 3 s hold; `MissionSucceed(false, false, false)`. |

### Cleanup / world-state side-effects

`MissionCleanup()`: Pete left wandering ambient; **saved-data slot 3 = player bust count** (`PlayerGetNumTimesBusted`) and slot 14 = 0 (same slots as the ch2 transition — per-chapter bookkeeping, **INFERRED**); speech re-enabled; population reverted; queued speech flushed; front-end re-enabled; and the **Chapter 4 faction standings**: 11 → 100, 1 → 100, 5 → 100, 4 → 100, 2 → 45, 3 → 0. Faction ids **INFERRED** from prior docs: 11 = bullies (1_03), 5 = preppies (2_B), 4 = greasers (3_B pass sets it to 100 — consistent with the 100 here), 1 = nerds, 2 = jocks, 3 = townies.

No rewards/unlocks beyond the chapter set and faction table; no `SMissPass` entry for this id (**VERIFIED** absence — the chapter set is duplicated in 3_B's case).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | T1 street flyover | in-engine (camera path + fixed look-at) | `camera` path + look-at + `letterbox`; vignettes = `spawn_npc` + `npc_follow_path` + one chase pair |
| CS-2 | T2 title card | in-engine banner | **chapter_title_card** (pool) held over the walk |
| CS-3 | T3 `"4-0"` | baked | `fade` + original chapter-4 title card |
| CS-4 | T4 Pete farewell | in-engine (anim + bark) | `say` + `pair_anim`-ish wave + `npc_move_to` exit |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 69, 102 | chase pair (flee / chaser) | vignette |
| 71, 85 | heavyset walker + bystander | vignette (`_FATBOY` point name, VERIFIED) |
| 5, 39 | strolling couple | vignette |
| 134 | Pete | setup companion + farewell beat |

### Animation / audio / text

- `LoadAnimationGroup`: `Hang_Talking` only; action tree `Act/Conv/Chapt3Trans.act` (`JimmyBye` node).
- Speech: `NARRATION` ids 112, 113, 114 (three lines, preload-chained); Pete's `BYE` event.
- Text keys: `Chapt3Trans_Title`, `Chapt3Trans_Name` (banner only).

### Named world data to extract from `Chapt3Trans.DAT` (DAT present, VERIFIED)

- POINTLISTs: `_CHAPT3TRANSPED` (1-2), `_CHAPT3TRANSFLEE` (1-2), `_CHAPT3TRANSFATTY` (1-2), `_CHAPT3TRANSCOUPLE` (1-2), `_CHAPT3ENDPETE`, `_CHAPT3ENDPLYR`.
- PATHs: `_CHAPT3TRANS_CAM` (camera), `_C3T_FLEE`, `_C3T_FATBOY`, `_C3T_COUPLE1/2`, `_C3T_PLAYER`, `_C3T_PETEY`.
- Hardcoded camera start (298.86, -73.14, 9.27 → 297.86, -73.11, 9.28) and look-at (249.26, -72.78, 9.22); Pete's exit point (187.708, -159.923, 8.944). Area codes: **0** only.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| T1 flyover | `camera_rail_pair` (ch2trans pool — here path + fixed look-at instead of look-path: same primitive), `spawn_npc`, `npc_follow_path` | **chase_pair_vignette** (fleer on path + attacker chasing — a two-line `npc_*` combo; fold into vignette dressing) |
| T2 title + narration | `chapter_title_card`, `narration_sequencer` (pool) | reuse — 3 lines instead of 4 |
| T0/T4 lifecycle | `chapter_set`, `frontend_ui_lock`, clock set | reuse; note **no** `world_state_swap` here (Christmas removal lives in 3_B's SMissPass) |
| T4 farewell | `fade`, `say`, ambient-ify | — |
| cleanup | saved-data slots | **saved_data_slot_write** (slot 3 = bust count, slot 14 = 0 — the same pair as ch2trans; fold into a chapter-transition checklist primitive) |

## 6. Open questions

- Launch path: mission-table chain 3_B → Chapt3Trans → first Chapter 4 mission (4_01-era), all outside the Lua.
- Content of `"4-0"` and the three narration lines (baked; not read).
- Both 3_B's `SMissPass` case and this file call `ChapterSet(3)` — redundant but harmless; which fires first depends on launch order (**OPEN**, benign).
- Whether anything consumes saved-data slot 3 (bust count snapshot) downstream — flagged in the ch2trans doc too.
