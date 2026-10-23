# Chapt4Trans — Chapter 4 → 5 interstitial: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/Chapt4Trans.lua` (191 lines, complete) + the ch3 doc's transition pattern (`../ch3/chapt3trans-chapter-transition.md`) + `SMissPass.lua` (4_B2 case, shared restart-point block). Structure described in our own words; no narration text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**Chapt4Trans = the Chapter 4 → Chapter 5 interstitial.** **VERIFIED** by: file name; `ChapterSet(4)` (0-indexed — entering the fifth chapter); chapter-completion banner keys `Chapt4Trans_Title`/`Chapt4Trans_Name`; three `NARRATION` lines (115-117); the closing baked cutscene `"5-0"` (Chapter 5 intro — matches the Chapt2Trans `"3-0"` / Chapt3Trans `"4-0"` pattern); and the final Pete farewell reusing `Act/Conv/Chapt3Trans.act`'s `JimmyBye` node.

Launch path (presumably the mission table right after 4_B2) is outside the Lua — **INFERRED**, strongly supported by `SMissPass.lua`'s 4_B2 case installing the same industrial restart-point block this script installs in `MissionSetup` (same `shared.indusrtialRestartPointsAdded` guard, same point lists) — i.e. the boss pass pre-seeds what the transition re-verifies.

Differences from Chapt3Trans: the montage is a football-field mowing vignette + a truck drive-by (post-"big game" morning-after feel, **INFERRED** from staging); it adds the industrial-area restart points and **opens the industrial barricade** (`AreaSetNodesSwitchedOffInTrigger(TRIGGER._INDUSTRIALBARRICADE, false)` — Chapter 5 unlocks the townie/industrial district); and its cleanup faction table flips jocks to 100 and townies to 0.

## 2. Mission flow

Single linear `main()` after `MissionSetup()`; no objectives, no fail path, no retry logic.

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Setup** | `Chapt4Trans.dat` (layer 2); no fade-in; ped models {134 (Pete), 56 (janitor), 236 (truck driver)} + vehicles {284 (riding mower), 291 (truck)}; current speech stopped; **front-end/select screens disabled**; industrial restart-point block installed once: KO + arrest restart points (`_RESTART_I_HS` / `_RESTART_I_PS`, area 0) + interior KO/arrest/asleep points for interiors {20, 16, 54} + restart camera positions. |
| T1 | **Field vignette + narration 1-2** | Player HP 200, punishment zeroed; `ChapterSet(4)`; clock 07:00; `WeatherSet(4)`; camera transition locks; player transitioned to area 0 `_CHAPT4TRANS_FOOTBALL`; control off, widescreen, POI off. Hardcoded camera over the field; narration **115** (preloaded-while-waiting pattern). Janitor (56) warped into mower (284), engine on, cruise 3, follows `PATH._CHAPT4TRANS_MOWER`; camera cuts to the mower shot; narration **116**; narration 117 preloaded in flight. |
| T2 | **Truck vignette + title card + narration 3** | Fade; POI re-enabled; player moved to `_CHAPT4TRANS_DRIVER` element 1; driver (236) + truck (291) spawned, engine on, cruise 7, follows `PATH._CHAPT4TRANS_TRUCK`; hardcoded roadside camera. Chapter-completion banner raised and **held** (`MinigameSetChapterCompletion` + `MinigameHoldCompletion`); camera flies `PATH._CHAPT4TRANS_CAMERA` looking at `PATH._CHAPT4TRANS_CAMERALOOKAT`, speed 15, 6 s; narration **117**; banner released and awaited. |
| T3 | **Chapter 5 intro cutscene** | Fade; driver/truck and janitor/mower exited and deleted; models unloaded; clock 08:30; player transitioned to `_CHAPT4TRANS`; baked `"5-0"` with load. |
| T4 | **Pete farewell handoff** | Pete (134) re-spawned at `_CHAPT4TRANS` element 2, faces the player; player faces 270°; Pete plays `JimmyBye` (from `Chapt3Trans.act`) + `BYE` scripted speech, goes ambient, walks to hardcoded (187.708, -159.923, 8.944); camera returned; weather released; transition locks released; `MissionSucceed(false, false, false)`. |

### Cleanup / world-state side-effects

`MissionCleanup()`: clock pinned to 08:30; Pete left wandering ambient; janitor/mower safety-deleted if still live; queued player speech flushed; **Chapter 5 faction standings**: 11 → 100, 1 → 100, 5 → 100, 4 → 100, **2 → 100** (jocks — beaten this chapter, vs. the 45 they got after Chapt3Trans), **3 → 0** (townies — the new antagonists). Faction ids **INFERRED** from prior docs (11 = bullies, 1 = nerds, 5 = preppies, 4 = greasers, 2 = jocks, 3 = townies). Saved-data slot 3 = player bust count; slot 14 = 0 (same per-chapter bookkeeping as Chapt2/3Trans). Front-end re-enabled; **industrial barricade nav nodes switched on** (the `_INDUSTRIALBARRICADE` trigger zone un-blocked); DAT unloaded.

No rewards/unlocks beyond the chapter set + faction table + restart points; no `SMissPass` entry for this id (**VERIFIED** absence — 4_B2's case carries the force-completes).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | T1 field/mower | in-engine (2 fixed cameras + vehicle path) | `camera` pair + `npc_ride_vehicle_path` vignette + `say` (narration_sequencer) |
| CS-2 | T2 truck + card | in-engine (camera paths + held banner) | **chapter_title_card** (pool) held over a `camera` path/look-at pair + `ambient_vehicle_override` |
| CS-3 | T3 `"5-0"` | baked | `fade` + original chapter-5 title card |
| CS-4 | T4 Pete farewell | in-engine (anim + bark) | `say` + `pair_anim`-ish wave + `npc_move_to` exit |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Id | Role | Notes |
|---|---|---|
| 134 | Pete | farewell wave + BYE bark, then ambient wander |
| 56 | janitor | mowing vignette (warped into mower 284, cruise 3, path `_CHAPT4TRANS_MOWER`) |
| 236 | truck driver | truck 291, cruise 7, path `_CHAPT4TRANS_TRUCK` |
| 284 / 291 | riding mower / truck | vignette vehicles, deleted after the montage |

Action tree: `Act/Conv/Chapt3Trans.act` (reused for the Pete wave — note the Chapt4Trans file borrows the previous transition's tree). No animation groups loaded. Speech: `NARRATION` 115-117 (sequential preload-while-playing) + Pete `BYE`.

### Named world data to extract from `Chapt4Trans.dat` (trigger data **VERIFIED present** — `DATLoad("Chapt4Trans.dat", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_RESTART_I_HS`, `_RESTART_I_PS` (+ `_CAM` variants), `_CHAPT4TRANS_FOOTBALL`, `_CHAPT4TRANS_MOW` (2), `_CHAPT4TRANS_DRIVER`, `_CHAPT4TRANS_TRUCK`, `_CHAPT4TRANS` (2).
- PATHs: `_CHAPT4TRANS_MOWER`, `_CHAPT4TRANS_TRUCK`, `_CHAPT4TRANS_CAMERA`, `_CHAPT4TRANS_CAMERALOOKAT`.
- TRIGGERs: `_INDUSTRIALBARRICADE`.
- Hardcoded cameras: field (13.13, -144.02, 13.47 → …), mower (-4.78, -61.58, 1.89 → …), truck (24.48, -352.84, 2.81 → …).
- Interior ids seen: **20, 16, 54** (industrial-area interiors wired to the new restart points).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| T0 setup | `frontend_ui_lock`, `set_restart_points` (pool — identical block to `SMissPass` 4_B2) | reuse |
| T1/T2 vignettes | `npc_ride_vehicle_path`, `ambient_vehicle_override`, `camera_path_cycle`, `narration_sequencer`, `chapter_title_card`, `weather_force` (pool) | none new |
| chapter roll | `chapter_set`, `set_faction_respect` matrix, `saved_data_slot_write` (pool) | none new |
| district unlock | `world_state_swap` (pool, partial) | **nav_barricade_toggle** (`AreaSetNodesSwitchedOffInTrigger` — enable/disable a named trigger zone's nav blockers; opens the industrial district for ch5) — could fold into `door_lock_set`; flag the zone-vs-door difference |
| T4 handoff | `say`, `npc_move_to`, ambient release (pool) | none new |

## 6. Open questions

- Which content areas/interiors 20, 16, 54 are (industrial park buildings — **INFERRED** from the restart-point naming) and what `_INDUSTRIALBARRICADE` physically blocks before this point.
- `WeatherSet(4)` type meaning (morning-after clear? type table needed).
- Why the arrest restart point is added twice (`AddKORestartPoint` is genuinely duplicated back-to-back — likely a copy-paste bug, **VERIFIED** duplicated line) and whether the extra `SMissPass` 4_B2 variant with hour windows (8/18) vs. this script's no-window call differ in effect.
- Mission-table wiring: exact 4_B2 → Chapt4Trans handoff and whether the `"5-0"` baked cutscene doubles as `5_01`'s intro.
