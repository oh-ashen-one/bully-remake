# 6_01 — Go See the Principal: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap6/6_01.lua` (51 lines, complete) + launch logic in `scripts/main.lua` and `scripts/SGlFunc.lua`. No `SMissPass.lua` branch exists for `6_01` (**VERIFIED**, SMissPass read in full — it has no `6_*` cases at all). Structure described in our own words; no dialogue/cutscene/objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**6_01 = "Go See the Principal"** (canonical Chapter 5 list, mission #62 — expelled-from-school interstitial) — **VERIFIED** by content and by the launch logic: after `5_02` (Preppies Vandalized, **VERIFIED** by its bonfire/photo/Darby content) is completed, the wake-up hook force-starts a launcher marker (`6_01_Launch`) and spawns a PA-summon thread that nags the player to report to the office until `6_01` runs (`SGlFunc.lua:807-811` → `T_ExpelledLogic`, `SGlFunc.lua:1048-1070`; re-armed on session load by `main.lua:983-986`). The mission body is a single baked cutscene (`"6-0"`), an "expelled" text beat (`6_01_EXPELLED`), and a teleport to the boys' dorm. StrategyWiki's walkthrough describes exactly this shape: "Less of a mission, more of a cut-scene… Jimmy is expelled," and notes that while the mission is pending, no other tasks can be started. Catalog cross-check: `docs/story-mode-catalog.md` #62 ("chains from #61").

Dead/loose ends in the file (**VERIFIED** in-file): `bSuccess` is never set `true`, so the `MissionCleanup` block that writes saved-data slot 3 (bust count) and slot 14 (0) never executes — the same slot pair the chapter transitions write (`saved_data_slot_write`); `F_PlayerGetOutOfBed` is a dead action-tree query hook (`bGetOutOfBed` never set `true` — **INFERRED** leftover of a sleep/get-up beat); locals `destBlip`, `gPAAnnouncementTimer`, `gCurrentArea`, `bPlayerGoToPrincipal` are unused, suggesting the in-engine walk-to-the-office phase was cut in favor of the launcher thread + cutscene (**INFERRED**).

## 2. Mission flow

No stages, no conditions, no spawns. `MissionSetup()` loads `6_01.DAT` **and** `3_08.DAT` (the Christmas mission's data — presumably for the dorm bed/wake points it shares, **INFERRED**), suppresses the mission-name splash and the fade-in. `main()`:

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | Baked cutscene | `PlayCutsceneWithLoad("6-0", true, true)` — the office scene. |
| B1 | Uniform strip | If the player's torso is the school sweater (`S_Sweater5`) it is swapped to a casual jacket (`B_Jacket6`); school trousers (`S_Pants1`) → casual trousers (`B_Pants2`); then `ClothingBuildPlayer()`. Expulsion = you can't wear the uniform any more (**VERIFIED** conditional swap). |
| B2 | Wake up in dorm | `AreaTransitionPoint(14, POINTLIST._6_01_BDORM, 1, false)` (area 14 = boys' dorm interior), camera reset/return, 500 ms fade-in. |
| B3 | Expelled beat + auto-pass | 4 s text beat (`6_01_EXPELLED`), `shared.gPlayerInitialArea = nil`, `MissionSucceed(false, false, false)` (no completion banner, no minigame card). |

`MissionCleanup()`: unpause clock, widescreen off, `DATUnload(2)`. Retry semantics: none — it cannot fail (no fail path exists in-file).

### Launch environment (from main.lua / SGlFunc.lua, **VERIFIED**)

- `T_ExpelledLogic`: every **240 s**, while the clock reads 08:00–19:00, no mission is active, and the player is in a school area (main map school trigger, library, school interior, boys'/girls' dorm, gym), a 2D PA bark (`PA_JIMMY_OFFICE_STRONG`) plays. The thread dies when `6_01` becomes active. Parallels `T_ChristmasdLogic` (the 3_08 summon, same shape, 120 s).
- While `6_01` is available, other missions are suppressed (walkthrough-corroborated; the suppression itself is engine mission-table behavior, not in the Lua — **OPEN** mechanism).

### Pass-side effects

- In-file: none (auto-succeed, no rewards).
- `SMissPass.lua`: no branch (**VERIFIED**). The knock-on effects of expulsion (uniform lock, class lockout) are world/availability consequences, not pass hooks; the exact flag plumbing is engine-side (**OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-1 | B0 | baked (`6-0`) | `fade` + `letterbox` + one `say` beat |
| CS-2 | B1–B3 | script | `outfit_predicate` + `give_clothing` swap, `teleport`, `hint`, auto-complete |

## 4. NPCs and assets needed

None spawned. Named world data: `POINTLIST._6_01_BDORM` (dorm wake point). DAT: **`6_01.DAT` present** (`DATLoad("6_01.DAT", 2)`) plus `3_08.DAT` — **flagged, not extracted**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by existing set | New primitives needed |
|---|---|---|
| Launch nag | `mission_active_query` (area scripts), `timer_restart` | **pa_summon_mission_gate** (a mission-pending state that (a) plays a repeating PA/radio summon bark on a multi-minute cadence, only in a whitelisted area set and hour window, while no mission is active, and (b) suppresses all other mission starts until the gated mission is begun — the engine-side lock is the load-bearing half) |
| B1 uniform strip | `outfit_predicate` (3_05-era), `give_clothing`, `temp_outfit_swap` (3_01A) | none new — conditional per-slot swap ("if wearing X replace with Y") is data on `outfit_predicate` |
| B2 dorm wake | `teleport` (schema), `area_visible_eq` | none |
| B3 auto-pass | `mission_presentation_flags` (no banner), `force_complete_marker_mission` (marker pattern) | none |
| cleanup saved-data | `saved_data_slot_write` (ch2trans/ch3trans) | none — note the slots here are **dead code** (never written), not a live write |

## 6. Open questions

- How "while 6_01 is available, nothing else can start" is enforced (mission-table metadata, presumed; no Lua implements it).
- Whether `6_01_Launch` is a real (invisible) mission entity or a table flag; only `ForceStartMission("6_01_Launch")` and completion checks reference it.
- Why `3_08.DAT` is co-loaded (dorm wake points shared with the Christmas wake, presumed).
- The cut "walk to the office under your own control" phase (unused `F_PlayerGetOutOfBed` / `bPlayerGoToPrincipal`) — PS2-era design?
