# 6_Dummy — Chapter 6 marker / Endless Summer state machine: recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** / **INFERRED** / **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap6/6_Dummy.lua` (10 lines, complete) **plus the distributed Endless Summer plumbing**: `scripts/chap5/Chapt5Trans.lua` (credits + `ChapterSet`), `scripts/SMissPass.lua`, `scripts/SFaction.lua`, `scripts/main.lua`, `scripts/AreaScripts/{Bdorm,SchoolHallways}.lua`, `scripts/secnd/ClassPhoto.lua`, `scripts/Test/RobertoTest.lua`. Structure in our own words; script/native/string-key names are citations only.

## 1. Identification

**6_Dummy = the Chapter 6 ("Endless Summer") marker mission** — a mission-as-flag with literally no content:

```lua
function MissionSetup()  MissionDontFadeIn()  end
function MissionCleanup() end
function main()  MissionSucceed(false, false)  end
```

(**VERIFIED** — full file.) Same pattern as `1_11_Dummy` (the "Halloween is over" marker, see ch1 notes): it exists so the campaign system has a completable entity other content can gate on with `IsMissionCompleated`. No DAT, no cutscene, no conditions, no rewards.

Evidence for what it marks (**INFERRED**): the only references in the entire dump are in test scripts. `Test/Missions/Test6_Dummy.lua` just runs it; `Test/RobertoTest.lua` (the debug "complete the whole game" script) force-completes it **after `5_02` (Preppies Vandalized) and immediately before `6_01_Launch`/`6_01`** — i.e., at the exact seam where the endgame/"expelled" arc (Chapter 6 content) begins. So `6_Dummy` most plausibly marks "the Chapter 6 mission set is now the active set" for the tasks/availability bookkeeping, *not* the post-credits free-roam (that state is established elsewhere — §3). What force-completes it in the retail flow: **OPEN** (no Lua found; presumed engine mission-table behavior on the chapter switch, same mechanism as other markers).

No `SMissPass.lua` branch for `6_Dummy` — or for **any** `6_*` mission (**VERIFIED**, full read of SMissPass: its last chapter-advance cases are `4_B2`→classes and `5_01`→`ChapterSet(4)`).

## 2. Mission flow

None. Setup suppresses the fade-in; main auto-succeeds without banner or card. That is the entire script.

### Pass-side effects

- In-file: none.
- SMissPass: none (**VERIFIED**).

## 3. The Endless Summer state machine (how post-game free-roam is actually established)

This is the section that matters for the remake. The original does **not** establish Endless Summer in 6_Dummy or in SMissPass; it is assembled from the final boss's in-file pass block, the chained credits mission, and a set of `IsMissionCompleated("6_B")` readers across the global scripts:

| Step | Where | Effect (**VERIFIED** unless noted) |
|---|---|---|
| 1. Boss pass block | `6_B.lua` main() | All six factions `SetFactionRespect(..., 100)`; Gary's yearbook photo (`UnlockYearbookPicture(130)`) + yearbook reward check; player force-loaded to the school front. |
| 2. Chain to credits | engine mission table (**OPEN** in Lua) | `Chapt5Trans` (stored in the **chap5** folder despite being the Chapter 5→6 transition) auto-starts. |
| 3. **ChapterSet(5)** | `Chapt5Trans.lua:196` | The chapter index is **0-based** (1_B→`ChapterSet(1)` = Chapter 2, 5_01→`ChapterSet(4)` = Chapter 5), so `ChapterSet(5)` = **Chapter 6 = "Endless Summer"**. This single call is the canonical entry into post-game. It happens *before* the credits NIS, unconditionally. |
| 4. Credits + cheer NIS | `Chapt5Trans.lua` main() | Player healed to 200, punishment zeroed, clock 08:30, weather 4; the named cast spawns at the school front cheering; player narration (preloaded speech 120/121) plays over it; Zoe walks up and the kiss beat plays (kiss tier anim group chosen by highest completed English class, `C_Engllish_2/3/4` — note the typo'd id is verbatim); the **credit roll thread** (`CreditLoadDB` + `RollTitleName` scroll loop) runs over the kiss; then a chapter-completion card (`MinigameSetChapterCompletion("Chapt5Message", "Chapt5Name")`) shows. |
| 5. Wake into free-roam | `Chapt5Trans.lua` tail | Player transitioned to the boys'-dorm bed, clock set 08:00, `MissionSucceed(false, false, false)`. Cleanup: saved-data slot 3 ← bust count, slot 14 ← 0 (the `saved_data_slot_write` pair, same as every chapter transition). |
| 6. Permanent sunshine | `main.lua:1001-1004` | On every session load with 6_B completed: `WeatherSet(4)` — Endless Summer is *literally* endless summer weather, re-forced each load. |
| 7. Adoring world | `SFaction.lua` `F_MissionFactionChanges()` | With 6_B completed, every clique's **default** attitude toward the player is set to 4 (max friendly) — townies (11), nerds (1), preppies (5), greasers (4), jocks (2), bullies (3), plus row 6; row 8 (girls?) to 3. This is the persistent half of the boss's in-file respect sweep. |
| 8. Cast changes | `SFaction.lua` `F_MissionCompleteModelChanges()` | Gary's unique model (48) suppressed forever (`-1`); Zoe (2) enabled (`1`). |
| 9. Dorm dressing | `Bdorm.lua` | Room trophy #22 (Bullworth crest + crossed cricket bats) appears; the dorm bulletin boards switch to the `BulletinChapter6_*` message set (also `SchoolHallways.lua` board 2). |
| 10. Yearbook | `ClassPhoto.lua:1018` + 6_B in-file | Gary (130) auto-added to the yearbook. |

**What Endless Summer does NOT do** (**VERIFIED** by absence): no forced class completions (unlike every previous chapter boss), no new restart points (those were 4_B2's SMissPass branch), no special-entity dressing swap (no `AreaLoadSpecialEntities` for a summer set), no clock stop — the game simply continues as free-roam with all leftover stars/errands/classes available (per catalog §1 Chapter 6 note). `RobertoTest.lua` also lists a `6_PassAll` marker completed right after `6_B` — **OPEN** what completes it in retail (presumed the 100%-adjacent "all chapter-6 missions done" bookkeeping flag).

## 4. NPCs and assets needed

None for 6_Dummy itself. For the Endless Summer transition (Chapt5Trans, for reference): 14 cheering cast models (57/58/63/19/37/23/91/75/10/14/134/4/74/2), credit text DB, cheer anim groups, kiss groups, `MS_6B_EndlessSummerCreditsNIS.rsm`, `Chapt5Trans.DAT` (present — flagged, not extracted; `_C5T_*` points, `_CST_PLAYEREND`, `_C5T_POP` trigger).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by existing set | New primitives needed |
|---|---|---|
| 6_Dummy itself | `force_complete_marker_mission` (1_11_Dummy) | none — same marker pattern |
| ChapterSet(5) | `chapter_set` (schema) | none — note 0-based indexing must be documented in the campaign table |
| Credits NIS | `narration_sequencer` (trans pool), `chapter_title_card`, `spawn_npc` ring, `social_reward_kiss` | **credit_roll** (a scrolling-text DB renderer layered over live scene/camera, with a start/stop thread contract — `CreditLoadDB`/`CreditPrintText`/`CreditReset` cycle; the only place in the game that needs it) |
| Kiss-tier selection | `mission_complete_on_door_action`-style gating → here: read "highest completed class" | none new — a `mission_completed(X)` query in campaign data (the marker-mission note in 1_11_Dummy already calls for completion-queryable flags) |
| Permanent weather on load | `weather_force` | **load_time_world_rules** (a session-load hook table: `if completed(6_B) → weather 4; if completed(1_B) → bus points; …` — main.lua's load block is one big rules table; the remake wants this as data, not code) |
| Default-attitude cascade | `conditional_faction_attitude` / `set_faction_respect` | none — SFaction's if/elif chains are exactly `conditional_faction_attitude` rows evaluated latest-completion-wins |
| Dorm/board dressing | `poi_set_dressing`, `world_state_swap` | none |
| Cast suppression | `unique_ped_suppress` | none |

## 6. Open questions

- What force-completes `6_Dummy` in retail (engine mission table presumed; zero Lua references outside tests).
- What `6_PassAll` is and what completes it (test script only).
- How 6_B → Chapt5Trans chaining is declared (same standing engine-table question as the 6_02→6_03→6_B seams).
- Whether `ChapterSet(5)` alone drives the mission-availability switch to the post-game set, or whether the table also keys on `IsMissionCompleated("6_B")`/`6_Dummy`.
- English-class kiss tier: the checked ids are spelled `C_Engllish_*` in the source — presumably a real typo'd id that exists in the data; confirm before authoring (don't "fix" the name).
