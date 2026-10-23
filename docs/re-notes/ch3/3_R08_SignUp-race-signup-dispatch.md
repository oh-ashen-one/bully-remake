# 3_R08_SignUp — bike-race sign-up dispatcher: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_R08_SignUp.lua` (29 lines, complete). Structure described in our own words; no objective text reproduced beyond key names. Script/native/string-key names are citations only.

## 1. Identification

**3_R08_SignUp = the bike-race sign-up sheet** — a 29-line pseudo-mission that converts the walk-up race marker into a race selection. Not a real mission: no objectives, no DAT load, no world interaction. **VERIFIED** in full (file is complete and trivial).

## 2. Mission flow

`main()` reads `MissionGetCurrentName()` (the registered *launcher* name, not the script id) and string-matches it:

| Launcher name contains | Sets `shared.g3_R08_CurrentRace` | Prints text key |
|---|---|---|
| `Rich Area 1` | 0 | `GOTO_RACERICH1` |
| `Rich Area 2` | 1 | `GOTO_RACERICH2` |
| `Business` | 2 | `GOTO_RACEBUSINESS` |
| `Poor` | 3 | `GOTO_RACEPOOR` |
| `School` | 4 | `GOTO_RACESCHOOL` |

Then pass music (`SoundPlayMissionEndMusic(true, 10)`) and `MissionSucceed()`. The matching race mission (3_R08) polls `shared.g3_R08_CurrentRace` on start (see `3_R08-bike-races.md`); this launcher only covers indices **0-4**, the five full district circuits. Indices 5-13 (the short variants) are presumably selected elsewhere (race posters/`shared.gRefreshRacePosters` flow — **OPEN**).

## 3. Cutscene points

None.

## 4. NPCs and assets needed

None. No DAT.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| name→index dispatch | `mission framework` | **sign_up_dispatch** (launcher param → shared selection var + "go to start" text + instant succeed). Compare `job_escalation_shared_state` / `area_script_shared_state` in the pool — this is the minimal form: one shared int consumed by the real mission |

## 6. Open questions

- Where indices 5-13 get set (poster interactables after the base race is cleared? `shared.gRefreshRacePosters` suggests a poster refresh script — not read).
- Whether the sign-up marker itself enforces prerequisites (e.g. Rich Area 2 needing Rich Area 1, or Chapter gating) — the launcher table, outside this Lua.
- Why a string match on the mission name instead of a numeric param (the `2_06supplemental` doc's subquest-switch pattern has the same smell — engine convention, **INFERRED**).
