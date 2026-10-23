# 3_R08 — Bike race series (Bullworth race circuit): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_R08.lua` (2046 lines) + `Library/BikeRace_util.lua` (race engine + clothing milestone rewards). Structure described in our own words; no dialogue or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_R08 = the repeatable bike-race content** — catalog §3 "Races — Bike: 14 (7 Vale, 4 Town, 2 New Coventry, 1 Academy in Ch5); $475 + gear". **VERIFIED** by content: 14 race configurations (indices 0-13) selected via `shared.g3_R08_CurrentRace` (set by `3_R08_SignUp` for 0-4, elsewhere for 5-13), all driving the `BikeRace_util` engine with per-race param tables, per-race DAT files, cash rewards 2000-5000, and per-race unlock message keys.

The 14 configurations (VERIFIED from `F_TableInit`/`F_InitDATLoad`):

| Idx | DAT | Circuit | Laps | Reward (cents) | Unlock key | raceLevel |
|---|---|---|---|---|---|---|
| 0 | `3_R08_rich.DAT` | Rich Area 1 (`_3_R08_RACE`, 3 shortcuts) | 1 | 2000 | `3_R08_UNLOCK1` | 1 |
| 1 | `2_04.DAT` | Rich Area 2 — **reuses the 2_04 "Race the Vale" course** (`_2_04_RACE`, 5 shortcuts) | 1 | 2500 | `3_R08_UNLOCK2` | 2 |
| 2 | `3_R08_business.DAT` | Business (jump node 127) | 1 | 3500 | `3_R08_BWTC` | 3 |
| 3 | `3_R08_poor.DAT` | Poor (6 jump nodes, 7 shortcuts) | **2** | 3000 | `3_R08_UNLOCK1` | 4 |
| 4 | `3_R08_school.DAT` | School (no jumps/shortcuts) | 1 | 5000 | `3_R08_BWAC` | 5 |
| 5-13 | `3_R08_richShortA/B/C/AM`, `BusShortA/B/C`, `PoorShortA`, `RichShortD` | short variants | 1-2 | 2000-4500 | `3_R08_UNLOCK1..6`, `3_R08_NCC`, `3_R08_BWVLC` | 1 |

Racers: always 3 AI riders on bike 273 with the same rubber-band profile shape as 3_G3 (sprint 0.81-0.86 / normal 0.73-0.81, catch-up 20 m @1.6×, slow-down 25 m @0.35×, shortcut odds 30/40/100, aggressiveness 0.2-1.0); models vary per race (preppy 30/17/40 on the Vale course; mixed 32/11/34, 9/16/27, 24/11/28, 8/13/28 elsewhere). One racer in race 2 carries a polo-mallet-class weapon (`MODELENUM._POLO`, 1 ammo).

## 2. Mission flow

Driver: `MissionSetup()` → `main()` waits on `shared.g3_R08_CurrentRace` (set by the sign-up) → `F_InitDATLoad()` (per-index DAT + camera path pair + `raceLevel`) → `F_TableInit()` → library `RaceSetup()` → pre-race NIS → `RaceControl()` (library) → win/lose.

| # | Beat | Logic (functional) |
|---|---|---|
| R0 | **Setup** | Garage deactivated; interactive music off; weapon model 300 preloaded; ambient vehicles forced to a trickle (2,2,0,0); player made mission-critical (`cbCritPlayer` → `shared.gPlayerIncapacitated`). Population override (4,0,0,0,0,0,0,1,1,0,2,0,0); all peds/vehicles cleared. |
| R1 | **Pre-race NIS** | `F_NISRace`: racer weapons cleared, unused vehicles near player deleted, widescreen, camera path/look path pair at speed 5 (per-race paths: `_3_R08_RICHPATHCAM(LOOK)`, `_2_04_INTROPATH/LOOK`, `_3_R08_BUSPATHCAM(LOOK)`, `_3_R08_POORPATHCAM(LOOK)`, `_3_R08_SCHOOLPATHCAM(LOOK)`, default `_3_R08_PATHCAM(LOOK)`) with three `TRASH_TALK_TEAM` barks; fade through black. |
| R2 | **Race** | Library `RaceControl()`: win objective, racers added/statted, countdown start, soundtrack `MS_BikeRace02.rsm`. Fail-side library rules: off-bike `get_on_delay` countdown → `RACE_GETONOBJ` timeout; distance to pack > warn distance → `RACE_FAR` spam, > fail distance → too-far fail; `F_AboutToPassOut` clock guard (1:59-3:00 AM) suspends both. |
| R3 | **Win** | `StatAddToInt(179, reward)` (race-winnings stat, **INFERRED**) + `MissionSucceed(false, false, false, gReward)` — reward carried as the succeed param. `MissionCleanup` records `shared.g3_R08_LevelAttained = raceLevel` and clears the selection var; `shared.gRefreshRacePosters = true` so the poster/sign-up layer re-paints. Library layer (in `BikeRace_util`, VERIFIED there): `F_AnyRaceFinished` grants `SP_BikeHelmet` + `SP_BikeJersey` + outfit `BMX Champion` on the first win; `F_AllRacesFinished` grants `SP_BMXHelmet` when every entry in its race table is completed. |
| R4 | **Lose** | Fail music; keyed reason if any else generic `GKART_YOULOSE` banner. |

### Fail conditions

| Cause | Mechanism | Fail key |
|---|---|---|
| A rival finishes first | library `RaceHasAnyRacerFinished` | generic lose banner |
| Off the bike past `get_on_delay` | library countdown | (objective-timeout lose) |
| Too far behind | library distance gates | `RACE_FAR` → lose |
| Player incapacitated | `cbCritPlayer` shared flag | engine-driven |

No checkpoints; fail replays from the sign-up. Reward/economy side is pure cash + clothing milestones.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | R1 pre-race | in-engine camera path pair + 3 barks | `camera_rail_pair` + `say` ×3 |
| CS-2 | R3 win | library `F_RaceEndNIS` (`M_RACEWIN` banner + `RaceVictory` anim) | `completion_banner` + player anim beat |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

Racer pools by race: 0: {32, 11, 34}; 1: {30, 17, 40} (preppies, Vale course); 2: {9, 16, 27}; 3/5-13: {24, 11, 28} (greasers, VERIFIED for 3/5-13) and 4: {8, 13, 28}. No crowd peds in this script (unlike 3_G3).

### Vehicles / items

Bike 273 for everyone; weapon model 300 preloaded; race 2 racer 2 carries `MODELENUM._POLO`. Clothing milestone rewards in the library: `SP_BikeHelmet`, `SP_BikeJersey`, `BMX Champion` outfit (first win), `SP_BMXHelmet` (all races).

### Named world data (DAT per race — 14 DATs, VERIFIED by name)

`3_R08_rich`, `2_04`, `3_R08_business`, `3_R08_poor`, `3_R08_school`, `3_R08_richShortA/B/C`, `3_R08_richShortAM`, `3_R08_BusShortA/B/C`, `3_R08_PoorShortA`, `3_R08_RichShortD`. Each contributes its `_3_R08_PLAYER/_PLAYERBIKE/_PED1..3(+BIKE)` start points (or `_2_04_PLAYERRICH` etc. for index 1), the course `PATH._3_R08_RACE`/`_2_04_RACE`, shortcut paths, highlighted-node lists, and camera path pairs.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| 14-in-1 config | mission framework | **race_series_selector** (shared index var set by sign-up → per-index {DAT, course path, laps, reward, unlock key, racer table, camera pair, raceLevel}; on pass write `LevelAttained` + `gRefreshRacePosters`). Extends `repeatable_job_tiers` (2_R03 pool) from tiers-of-one-job to sibling-circuits-of-one-series |
| race core | `race_controller` (2_04 pool) | reuse; add per-racer **weapon/ammo** field (race 2) and `path_smoothing`/`auto_lineup` flags to its param surface |
| milestones | `give_clothing` (pool) | **series_milestone_rewards** (first-win bundle; all-14-done helmet — predicate over the campaign table, i.e. library `F_AnyRaceFinished`/`F_AllRacesFinished`) |
| pass economy | `reward` action | `StatAddToInt(179)` — a **stat_counter_add** (campaign stats ledger; also on the catalog §3 tracker list) |
| win banner | `completion_banner` | reward as `MissionSucceed` param = banner money; covered |

## 6. Open questions

- What the unlock keys actually unlock (`3_R08_UNLOCK1..6`, `3_R08_BWTC`, `3_R08_BWAC`, `3_R08_NCC`, `3_R08_BWVLC` — presumably the next poster/tier; naming suggests "Beat the Wealthy/Townie Circuit" style; data-side).
- How indices 5-13 are started (no sign-up case covers them; poster interactables likely, `shared.gRefreshRacePosters` is the hook — launcher scripts not read).
- `raceLevel` semantics: written to `shared.g3_R08_LevelAttained` only for indices 0-4 (all 5-13 write 1); consumed where? (Probably poster availability.)
- Time-of-day/weather gating of the markers (launcher table).
- The Ch5 Academy race from the catalog ("1 Academy in Ch5") — none of the 14 indices obviously is it; possibly one of the short variants or a separate script (**OPEN**).
