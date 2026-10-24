# Chapt5Trans — Chapter 5 → 6 interstitial (end-of-story credits into Endless Summer): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/Chapt5Trans.lua` (360 lines, complete) + `SMissPass.lua` (no Chapt5Trans case — **VERIFIED** absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**Chapt5Trans = the Chapter 5 → Chapter 6 interstitial — the end-of-story sequence.** **VERIFIED** by: file name; `ChapterSet(5)` (0-indexed per the established convention — entering the **sixth** chapter, "Endless Summer"); chapter-completion banner keys `Chapt5Message`/`Chapt5Name`; the credits music `MS_6B_EndlessSummerCreditsNIS.rsm`; a full **scrolling-credits roll** (`CreditLoadDB` + `RollCredits`); and the closing placement — the player is put to bed at `_BOYSDORM_BEDWAKEUP` in area 14 (boys' dorm) with the clock set to 08:00.

Unlike Chapt1–4Trans this is not a "next morning" vignette with a baked chapter-intro card: there is **no baked cutscene at all** — the whole file is one scripted scene (the cheering-crowd celebration in front of the school + the Zoe kiss) plus the credits and the dorm wake-up. It is the playable-credits endpoint of the main story: per the [fandom Chapter V page](https://bully.fandom.com/wiki/Chapter_V), the story ends with Jimmy walking out to a cheering crowd and Zoe kissing him as the credits roll — exactly what this script stages.

**Chain:** launched right after `6_B` (Final Showdown, the Gary roof fight) — **INFERRED**; no `SMissPass.lua` case for either `6_B` or `Chapt5Trans` (**VERIFIED** absence), so the link is mission-table work outside the Lua, same posture as the earlier transitions. `main.lua` corroborates the end-state: once `6_B` is completed it forces sunny weather (`WeatherSet(4)`) — the same weather 4 this script sets (**VERIFIED**, main.lua line ~1001).

Structure: linear single-scene script in `main()` + one thread (`RollCredits`). No stages, no fail path — a pure presentation mission.

## 2. Mission flow

Driver: `MissionSetup()` (loads) → `main()` (scene + credits + banner + dorm) → `MissionCleanup()` (credit DB unload, saved-data writes).

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Setup** | `Chapt5Trans.DAT` (layer 2); no fade-in; action tree `Act/Conv/Chap5Trans.act` (note: `.act` drops the second "t", the DAT keeps it — VERIFIED); cheer anim groups `CHEER_COOL2/NERD1/NERD3/POSH1/POSH3/GEN3/GIRL2/GIRL3` (+ `2_06_MOVIETICKETS` reused) and a **class-gated kiss group**: `KISS2`/`KISS3`/`KISS4` depending on which of `C_Engllish_2/3/4` is completed (misspelling "Engllish" VERIFIED in script); interactive stream stopped; credits DB loaded; 14 ped models; clock 08:30; weather 4. |
| T1 | **Crowd tableau** | Player HP 200, punishment points zeroed; **`ChapterSet(5)`**; player transitioned to area 0 `_C5T_PLYR`; population trigger `_C5T_POP` activated then population fully overridden to zero; all peds cleared. The 14 named characters spawn at their `_C5T_*` points — Phillips, Galloway, Edna (the `gCafeteria` var), Ted, Derby, Johnny, Edgar, Russell, Earnest, Mandy, Petey, Algie, a generic fat girl, Zoe — and each is put on a looping cheer action node from the `Chap5Trans` tree. Control off, widescreen, player made NIS-safe; narration 120 preloaded. |
| T2 | **Celebration shots + narration** | FOV 70 wide shot of the crowd; player walks `_C5T_PLYR` el 2; credits music starts (`MS_6B_EndlessSummerCreditsNIS`, 2 s fade); narration **120** plays; while it plays the camera tightens (FOV 20) on a second angle; narration **121** preloaded + played; Zoe is taken off her cheer node and **runs up to the player** (`_C5T_ZOE` el 2); more angle cuts. |
| T3 | **The kiss + credits roll** | Zoe set to love the player (emotion 8, attitude 4, flag 84); mutual target lock; the player plays the make-out/grapple node (`Act/Player.act`) into the kiss loop from the class-gated `KISS*` group; the camera moves to a close FOV 30 shot; **`RollCredits` thread starts** once the kiss loop begins; fade-with-camera decoupled so the music keeps playing; when the kiss anim ends the screen is held at black (`CameraHoldFadeout`). |
| T4 | **Credits scroller** | `RollCredits`: builds a table from the credits text DB — `TITLE<n>` headers + `TITLE<n>_NAME<m>` rows (count from `TITLE<n>_NUMNAMES`), title scale 0.75 / name scale 0.6, white text, scrolling upward 2 px per 20 ms tick; `TITLE64` is the two-column **actor/cast section** (`ACTOR_INDEX = 64`, alternating left/right justification at x-offsets 600/50). Ends when every row has scrolled off, or instantly when cleanup sets `bEndCreditsNow`. |
| T5 | **Chapter banner + dorm wake-up** | Crowd deleted; credits awaited; control back; forced area transition to `_CST_PLAYEREND` (note the inconsistent `_CST_` prefix — VERIFIED) with a fixed camera; **chapter-completion banner `Chapt5Message`/`Chapt5Name` raised and held** ~4 s (`MinigameSetChapterCompletion` + `MinigameHoldCompletion`/`Release`); fade; camera reset; player transitioned to **area 14 `_BOYSDORM_BEDWAKEUP`** (the dorm bed wake-up point); **clock 08:00**; `MissionSucceed(false, false, false)`. Endless Summer begins. |

### Fail conditions and retry

None — no fail path, no objectives, no threads that can fail. Presentation only.

### Rewards / pass side-effects

- `ChapterSet(5)` — the campaign enters Chapter 6 (Endless Summer): the free-roam end state.
- `MissionCleanup`: `PlayerSetScriptSavedData(3, PlayerGetNumTimesBusted())` and `PlayerSetScriptSavedData(14, 0)` — saved-data slots 3/14 written (bust count carried, slot 14 cleared; semantics of slot 14 **OPEN**).
- No respect changes, no money, no force-completes, no `SMissPass` case.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | T1/T2 crowd tableau + narration | in-engine (4-5 hardcoded FOV/camera cuts) | `camera` sequence + `say`/narration + `spawn_npc` tableau + `ped_action_node` cheer loops |
| CS-2 | T3 the kiss | in-engine (paired grapple anim) | `pair_anim` (existing stub covers facing + clip) + `camera` close-up |
| CS-3 | T4 credits | engine credit roll | **new `credits_roll` primitive** — placeholder: scrolling text overlay over black |
| CS-4 | T5 banner + dorm | in-engine | `chapter_title_card` (held variant) + `fade` + `teleport` + `clock` |

## 4. NPCs and assets needed

### Peds (numbers as scripted; role labels are the script's own variable names)

| Model id | Role | Notes |
|---|---|---|
| 2 | Zoe | the kiss; emotion/attitude set to love, flag 84 |
| 134 | Petey | cheer loop |
| 91 | Edgar | cheer loop (same id as 5_B — consistent) |
| 75 | Russell | cheer loop |
| 23 | Johnny / 37 Derby / 19 Ted / 10 Earnest | clique leaders, cheer loops — note Ted is **19** here vs **110** in 4_B2 (outfit variant, **OPEN**/INFERRED) |
| 14 | Mandy / 74 generic fat girl (`_C5T_GN_FATGIRL`) / 4 Algie | students |
| 63 Ms. Phillips / 57 Galloway / 58 Edna (`gCafeteria`) | teachers/staff | |

### Animation groups / action trees

`Act/Conv/Chap5Trans.act` (cheer nodes + blank); groups `CHEER_COOL2`, `CHEER_NERD1/3`, `CHEER_POSH1/3`, `CHEER_GEN3`, `CHEER_GIRL2/3`, `2_06_MOVIETICKETS` (reused cheer source), `KISS2/3/4` (exactly one, gated on English class completion), player side from `Act/Player.act`.

### Speech / music / text DB

`NARRATION` ids 120, 121 (preloaded, supersize). Music `MS_6B_EndlessSummerCreditsNIS.rsm`. Credits text database via `CreditLoadDB`/`CreditFindText` — `TITLE<n>` / `TITLE<n>_NAME<m>` / `TITLE<n>_NUMNAMES` keys; the credit roll content is a data file, not Lua. Banner keys `Chapt5Message`/`Chapt5Name`.

### Named world data to extract from `Chapt5Trans.DAT` (trigger data **VERIFIED present** — `DATLoad("Chapt5Trans.DAT", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_C5T_PLYR` (2), `_C5T_GALLOWAY`, `_C5T_PHILLIPS`, `_C5T_CAFETERIA`, `_C5T_TED`, `_C5T_DARBY`, `_C5T_JOHNNY`, `_C5T_EDGAR`, `_C5T_RUSSELL`, `_C5T_EARNEST`, `_C5T_MANDY`, `_C5T_PETER`, `_C5T_ALGERNON`, `_C5T_GN_FATGIRL`, `_C5T_ZOE` (2), `_CST_PLAYEREND` (prefix inconsistency VERIFIED), `_BOYSDORM_BEDWAKEUP`.
- TRIGGERs: `_C5T_POP` (population pocket for the crowd scene).
- Hardcoded cameras: crowd wide (224.12, -73.19, 8.79 → …), two FOV-20 tights (216.30, -73.21, 10.90 → …; 226.75, -74.58, 7.52 → …), kiss close-up (218.37, -74.22, 9.85 → …), banner shot (95.19, -68.43, 9.77 → …).
- Area codes seen: 0 (scene), 14 (boys' dorm wake-up).

## 5. Framework gaps (beat → primitives)

Deduped against the ch1–ch4 gap pools and the `MissionSubsystem.h` schema.

| Beat | Covered by | New primitives needed |
|---|---|---|
| T0 setup | `weather_force`, `clock` (set), `population_override`, `population_trigger_toggle` | none new |
| T1 tableau | `spawn_npc` + `ped_action_node` (cheer loops) + `clear_ambient_peds` | reuse — the 14-ped cheer tableau is a pure data cast table |
| T2 narration | `narration_sequencer` (pool) | reuse; the preload-while-playing chain is an implementation detail |
| T2/T5 banner | `chapter_title_card` (pool) | note the held variant (`MinigameHoldCompletion`/`MinigameReleaseCompletion` + explicit 4 s) — add a `hold_seconds` param to the existing primitive rather than a new one |
| T3 kiss | `pair_anim` (existing stub: face + clip) | **class_gated_anim_variant** (pick the animation group by class-completion state — `KISS2/3/4` via `IsMissionCompleated("C_Engllish_*")`; generalizes to any class-progress-flavored cutscene variant) |
| T4 credits | — | **credits_roll** (load a credit text DB; build title/name rows; scroll upward at fixed px/tick; two-column cast section; ends on scroll-off or force-end; skippable-safe) — the file's one big new system |
| T5 entry | `chapter_set`, `teleport` (area 14), `clock` | **bed_wakeup_placement** — foldable into teleport+clock; listed only so the campaign spine remembers the dorm-wake convention |
| cleanup | `saved_data_slot_write` (pool) | reuse (slots 3 = bust count, 14 = cleared) |
| fade hold | `fade` (out/hold/in) | reuse — `CameraHoldFadeout(true)` == an open-ended hold phase |

## 6. Open questions

- Launch wiring: the exact mission-table edge `6_B` → `Chapt5Trans`, and whether anything may run between them (the wiki describes a Crabblesnitch office resolution scene — that belongs to `6_B`'s outro, **OPEN** until 6_B is recon'd).
- `ACTOR_INDEX = 64` implies the credits DB has ≥ 65 title groups; where the DB lives (text DAT?) and its total size — data-side question.
- Zoe's flag 84 and emotion 8 semantics (the same love/emotion ids as the dating system?).
- Saved-data slot 14 cleared on entry to Endless Summer — what does slot 14 gate (Halloween dressing? mission-state scratch)?
- Ted model 19 vs 110 (4_B2): outfit-variant model ids need a roster pass against `default.ide`.
- Whether the kiss group fallback is safe when no English class is passed (no `KISS*` loaded — the make-out node may fail silently; **OPEN** — likely masked because English 2 is force-completed by earlier chapter boss passes).
