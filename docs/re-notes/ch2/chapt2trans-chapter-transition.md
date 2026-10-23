# Chapt2Trans — Chapter 2 → 3 interstitial: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/Chapt2Trans.lua` (162 lines, complete) + `SMissPass.lua` (2_B case) + the ch1 doc `chapt1trans-chapter-transition.md` for the pattern. Structure described in our own words; no dialogue, cutscene or narrative text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**Chapt2Trans = the Chapter 2 → Chapter 3 interstitial.** **VERIFIED** by: file name; `ChapterSet(2)` (chapters are 0-indexed in the scripts — 1_B's `SMissPass` case calls `ChapterSet(1)` entering Chapter 2, 2_B's calls `ChapterSet(2)` entering Chapter 3); a chapter-completion banner keyed `Chapt2Trans_Title`/`Chapt2Trans_Name`; four `NARRATION` lines (108-111); the closing baked cutscene `"3-0"` (the Chapter 3 intro, matching Chapt1Trans's `"2-0"` pattern — **INFERRED**); and the final handoff placing the player in the boys' dorm (area 14) with Pete wandering by.

Notable: it **`DATLoad`s `3_07.DAT`** alongside `Chapt2Trans.DAT` and starts the flyover from `POINTLIST._3_07_PLAYERPOOR` — the opening beats borrow the Chapter 3 poor-area mission's data; the flyover tours the newly unlocked part of town (**INFERRED** from the point name and the construction vignette below). How the transition is launched (presumably by the mission table after 2_B passes; `SMissPass` only does the chapter set) is **OPEN** — table lives outside the Lua.

## 2. Mission flow

Single linear `main()` after `MissionSetup()`; no objectives, no fail path, no retry logic.

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Setup** | `Chapt2Trans.DAT` + `3_07.DAT` loaded; all POI cleared, all peds cleared, population zeroed; `ChapterSet(2)`; ped models {131, 97, 236, 237, 195, 123} + vehicle model 288 loaded; no fade-in; **Christmas special entities loaded** (mirrors the 2_B `SMissPass` case — snow season begins with Chapter 3, VERIFIED as a pair); `Hang_Talking` anim group; clock forced to 08:30; current speech stopped; **front-end/select screens disabled** (`SystemEnableFrontEndAndSelectScreens(false)`). |
| T1 | **Town flyover** | Player health set to 200, punishment zeroed, ambient vehicles cleared; player transitioned to area 0 at `_3_07_PLAYERPOOR`; control off, widescreen; camera rides `PATH._C3T_TOWN` (speed 13) while looking along `PATH._C3T_LOOKAT` — a slow aerial tour. Set dressing spawns at `_C3T_CONSTRUCTION`: four peds (236, 237, 195, 123) + vehicle 288; three of the peds walk paths `_C3T_FARLEFT`/`_C3T_CENTER`/`_C3T_FARRIGHT`, ped 123 warps into the vehicle which cruises `PATH._C3T_TRACTOR` at speed 2 (a roadworks vignette, **INFERRED**); two pedestrians (131, 97) walk `_C3T_PEDESTRIAN1/2`. |
| T2 | **Title card + narration** | Chapter-completion banner raised and **held** (`MinigameSetChapterCompletion(...)` + `MinigameHoldCompletion`); fade in; four narration lines (`NARRATION` 108, 109, 110, 111) played strictly sequentially, each preloaded while the previous plays. Then banner released and waited out. |
| T3 | **Chapter 3 intro cutscene** | Fade out; flyover peds/vehicle deleted and models unloaded; baked cutscene `"3-0"` with load. |
| T4 | **Dorm handoff** | `SetFactionRespect(1, 85)` and `(5, 100)`; forced area transition to **area 14** (`isc_dorm`) at `POINTLIST._CHAPT2TRANS` element 1; Pete (model 134) spawned at element 2; control on, widescreen off, Pete made ambient and wandering; fade in; `MissionSucceed(false, false, false)`. |

### Cleanup / world-state side-effects

`MissionCleanup()`: snow force-off and weather released (fresh state for the new chapter), population reverted, `PlayerSetScriptSavedData(3, bust-count)` and slot 14 = 0, front-end re-enabled, queued speech flushed, and the **Chapter 3 faction standings** set: 11 → 100, 1 → 85, 5 → 100, 4 → 50, 2 → 45, 3 → 0. Faction ids **INFERRED** from prior docs: 11 = bullies (1_03), 5 = preppies (2_B), 1 = nerds, 2 = jocks, 3 = townies, 4 = greasers (all but 11/5 **OPEN**).

No rewards/unlocks beyond the chapter set and the faction table; no `SMissPass` entry for this id (**VERIFIED** absence — chapter set happens in the 2_B case).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | T1 town flyover | in-engine (camera path pair, ~the duration of 4 narration lines) | our scripted `camera` path + look-at target + `letterbox`; dressing = a handful of `spawn_npc` + `npc_follow_path` and one scripted vehicle |
| CS-2 | T2 title card | in-engine banner | **chapter_title_card** (see gaps) held over CS-1 |
| CS-3 | T3 `"3-0"` | baked | `fade` + original chapter-3 title card or short establishing shot |
| CS-4 | T4 dorm arrival | engine fade-in | trivial: `fade` + control handoff |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 236, 237, 195 | construction crew (3 walkers) | **INFERRED** from `_C3T_CONSTRUCTION` point name |
| 123 | construction vehicle driver | warped into the vehicle |
| 131, 97 | pedestrians | simple path walks |
| 134 | Pete | dorm arrival dressing, ambient |
| 288 | work vehicle | follows `_C3T_TRACTOR` — a tractor/roadworks vehicle, **INFERRED** |

### Animation / audio / text

- `LoadAnimationGroup`: `Hang_Talking` only.
- Speech: `NARRATION` ids 108, 109, 110, 111 — **4 narration lines**, sequentially preloaded (`SoundPreloadSpeech` → `SoundPlayPreloadedSpeech`).
- Text keys: `Chapt2Trans_Title`, `Chapt2Trans_Name` (banner only).
- No action trees, no weapons, no props.

### Named world data to extract from `Chapt2Trans.DAT` (+ `3_07.DAT`)

- POINTLISTs: `_3_07_PLAYERPOOR`, `_C3T_CONSTRUCTION` (elements 1-5), `_C3T_PEDESTRIANS` (1-2), `_CHAPT2TRANS` (1-2).
- PATHs: `_C3T_TOWN` (camera), `_C3T_LOOKAT` (camera target), `_C3T_FARLEFT`, `_C3T_CENTER`, `_C3T_FARRIGHT`, `_C3T_TRACTOR` (vehicle), `_C3T_PEDESTRIAN1`, `_C3T_PEDESTRIAN2`.
- Hardcoded camera start: (514.27, -190.18, 7.10 → 513.67, -190.95, 6.88).
- Area codes seen: **0** (flyover) and **14** (`isc_dorm` handoff).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| T1 flyover | `camera` path (1_02 precedent), `letterbox`, `spawn_npc` + `npc_follow_path` | **camera_rail_pair** (camera path + separate look-at path at fixed speed — a variant of the 1_02 camera path; list as new if the 1_02 primitive is position-only) |
| T2 title + narration | `say` | **chapter_title_card** (`MinigameSetChapterCompletion` hold/release: banner raised, held across narration, released and awaited); **narration_sequencer** (preload-next-while-playing chain of N narration lines, all under the held card) |
| T0/T4 lifecycle | `chapter_set`, `set_faction_respect` (×6), `world_state_swap` (Christmas), clock set | **frontend_ui_lock** (`SystemEnableFrontEndAndSelectScreens` off for the interstitial, restored in cleanup) |
| T4 handoff | `fade`, area transition, ambient-ify | — |

## 6. Open questions

- Launch path: which table entry chains 2_B → Chapt2Trans → first Chapter 3 mission (3_07? — the borrowed DAT suggests it); mission table is outside the Lua.
- The exact content/duration of `"3-0"` and the four narration lines (baked; not read).
- Whether the Christmas entity load here and in `SMissPass` 2_B double-applies (both guarded differently — the `SMissPass` one checks `IsMissionCompleated("3_08")`; this one does not — **OPEN** which one actually wins at runtime, harmless either way).
- Faction ids 1/2/3/4 mapping (only 11 = bullies and 5 = preppies are anchored by other docs).
