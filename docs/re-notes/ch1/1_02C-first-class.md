# 1_02C — first class connector ("get to Chemistry"): recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_02C.lua` (119 lines) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_02C = the "go to your first class" connector mission** (not a named story mission in the canonical list). **VERIFIED** by content: clock set to 9:00, the school bell is rung (`F_RingSchoolBell`), a ~57 s countdown starts, and the player must reach the Chemistry class point (`POINTLIST._CLASS_CHEM`) and press the interact button before it expires. On the first attempt only, three class-system tutorial cards (`TUT_CLASS01/02/03`) play while the game clock is paused. This is the hand-off from the scripted morning (1_02B) into the class/schedule system.

**Role in the chain:** 1_02B passes → `shared.b102finished` set → 1_02C starts immediately (1_02C's `MissionSetup` sees the flag and skips its own fade/ped-clear — seamless continuation, **VERIFIED**). On pass, the Chemistry 1 class (`C_Chem_1`, `secnd/ClassChem.lua`) becomes the next beat; the exact launch mechanism (mission table / class system) is outside the Lua — **OPEN**.

## 2. Mission flow

| # | Beat | Logic (functional) |
|---|---|---|
| C0 | **Setup** | No fade-in; if `shared.b102finished` was set (normal flow from 1_02B) it is consumed and no fade/ped-clear happens; otherwise fade + clear all peds. Camera returned to player, clock set to 09:00, fire alarm stopped, punishment points zeroed. `1_02C.DAT` loaded. |
| C1 | **Placement** | Class point read from `_CLASS_CHEM`. If the player is already in area 2 (`ischool`), teleport to `_1_02C_SPAWNPLAYER` facing heading 180; otherwise area-transition there. Brief camera nudge at (-635.5, -299.65) looking at the hall, then back to player, widescreen off. Player health topped to 200, game clock paused, fade in. |
| C2 | **First-attempt tutorials** | If `GetMissionAttemptCount("1_02C") == 1`: three 4.5 s tutorial cards in sequence (`TUT_CLASS01/02/03`), clock unpaused during the last one. Retries skip all three (clock simply unpauses). |
| C3 | **Bell + countdown** | `F_RingSchoolBell()`, objective text + objective added (`1_02C_OBJ01` — get to class, **paraphrase**), control on. A 57 000 ms timer starts. The player must stand within 0.75 m of the `_CLASS_CHEM` point and press button 9 (interact); a contextual prompt (`BUT_CLASS_CHE`, **paraphrase**: enter class) shows while in range. |
| C4 | **Pass** | Interact in time: fade, objective complete, `F_SetRestartPoints()` (`shared.extraKOPoints = true` — extra KO restart spots), `MissionSucceed(false, false, false)`. Cleanup: mailbox/crate smashables enabled (see rewards), `shared.passed1_02C = true`, `Pickups.DAT` loaded, no fade-in after completion. |
| C5 | **Fail** | Timer expires: fail music, `MissionFail(false, true, "1_02C_FAIL_01")`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Countdown expires (57 s) | `gTimer` watchdog in the main loop | `1_02C_FAIL_01` |

Only fail path. Retry replays from C0 but skips the tutorial cards (attempt count ≥ 2). No checkpoints (the mission is one room away from its start).

### Rewards / pass side-effects (`SMissPass.lua` + in-script cleanup)

- `ObjectTypeSetPickupListOverride("DPI_CardBox", "PickupListMailBox")` and `DPI_CrateBrk`/`DPE_CrateBrk` → `PickupListCrate`: **smashable crates/cardboxes start dropping pickup loot** from this point in the game (VERIFIED; also duplicated in 1_02C's own `MissionCleanup` guarded by `bSuccess`).
- `DATLoad("Pickups.DAT", 1)` — the world pickup set becomes active.
- `F_SetRestartPoints()` → `shared.extraKOPoints = true` (extra KO restart points unlocked, exact spots in `main.lua`'s `F_SetupExtraKOPoints` — **OPEN** list).
- Game clock unpaused (the schedule system starts ticking for real).

## 3. Cutscene points

None — no baked cutscene, no NIS. The tutorial cards are HUD popups (our `hint`/`banner` primitives). The only camera work is the C1 placement nudge, covered by `camera` + `fade`.

## 4. NPCs and assets needed

- **Peds:** none created. (Existing ambient/authority peds carry the "halls clearing for class" feel; the mission itself spawns nobody.)
- **Animation groups / action trees:** none loaded.
- **Props/weapons:** none. (`classDoor = TRIGGER._DT_TSCHOOL_GYML` is declared at file scope but never used — vestigial, **OPEN** why the gym door name.)
- **Speech:** none (0 scripted speech events). Bell audio via `F_RingSchoolBell()`.
- **Special entities:** none.

### Named world data to extract from `1_02C.dat`

- POINTLISTs: `_CLASS_CHEM` (the class door/interact point), `_1_02C_SPAWNPLAYER`.
- PATHs / TRIGGERs: none referenced in the Lua.
- Hardcoded coords: camera (-635.5, -299.65, 0.8 → -635.95, -298.8, 0.9); player safety set (-637.4, -296.2, 0) — all `ischool` (area 2).
- Area codes seen: **2 only**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| C2 first-attempt tutorials | `hint` | condition **attempt_count_eq(n)** (retry-aware gating) |
| C3 bell + countdown + interact | `timer`, `reach_location`, `interact` | **countdown_fail_timer** (visible remaining-time HUD + fail path on expiry); **ring_bell**/schedule-system event hook; **clock_pause/unpause** action |
| C4 pass side-effects | `reward`, `set_flag` | **pickup_table_override** (world loot tables on), **set_restart_points** (KO/arrest restart config) |
| C1 placement | `teleport` | area-aware teleport (already have per 1_03 doc) |
| chaining | `start_mission` | mission-chain edge 1_02B → 1_02C (shared-flag consumed) and 1_02C → class `C_Chem_1` (mechanism **OPEN**) |

## 6. Open questions

- How the Chemistry class actually starts after the pass: standing at `_CLASS_CHEM` during class hours vs. explicit `C_Chem_1` launch — the class system (`ClassChem.lua`, schedule) owns this, not the mission Lua.
- Whether the 57 s timer is lenient-by-design (walk from the cafeteria area to the chem room is short) and whether being busted during the countdown does anything special (no arrest override here — default behavior, **OPEN**).
- `F_RingSchoolBell` side effects beyond audio (PA? NPC schedule shifts) — defined outside this file.
- The unused `_DT_TSCHOOL_GYML` declaration (vestigial or cut content).
- Exact list of "extra KO points" enabled by `shared.extraKOPoints` (`main.lua` `F_SetupExtraKOPoints`).
