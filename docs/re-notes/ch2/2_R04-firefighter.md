# 2_R04 — Firefighter (repeatable extinguisher job): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/2_R04.lua` (516 lines, complete) + `SMissPass.lua` (no 2_R04 case — **VERIFIED** absence) + `SZone.lua` (weapon ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_R04 = the "Firefighter" volunteer job** — **VERIFIED** by content: a fireman (model 82) briefs the player via conversation tree `"R04Conv"` (`Act\Conv\2_R04.act`), the player is handed weapon **326 = "fire extinguisher"** (VERIFIED name via `SZone.lua`), and the loop is: reach a randomly chosen block, where street props are set on fire (`FireCreate` with per-model burn tuning), extinguish every fire, repeat for N areas. Radar blips use `RADARICON_FIRE`.

**IDENTIFICATION SURPRISE:** like 2_R03, this is a **repeatable town job, not a race** (the `2_R*` guess was wrong; Race the Vale is `2_04`). Progression persists across runs in `shared.g2SS07MissionState` — the name suggests it is shared with a `2_S07` script (a second firefighter beat or the job's story wrapper; that file was not read — **OPEN**).

Setting: outdoors (area 0), the business district. Only one region is actually registered (**"Downtown"**, 4 fire areas); `F_DialogueGetRegion` also knows a `"PoorHouse"` name but no region data exists for it — dead/incomplete content (**INFERRED** cut second region).

## 2. Mission flow

Driver: `MissionSetup()` → `main()`; one `LibObjective` (`objPutOutFires`: success `F_FiresOut`, failure `F_MonitorObjects`) driven by `T_ObjectiveMonitor` + `L_MonitorTriggers`. No stage machine — one objective loops over N areas.

| # | Beat | Logic (functional) |
|---|---|---|
| F0 | **Setup** | `2_R04.DAT`; player transitioned to `_2_R04_PSTART`; weapon 326 requested. Objective framework armed. |
| F1 | **Fireman briefing (first attempt only)** | Fireman (82) spawned at `_2_R04_CORONA`; `PedStartConversation("R04Conv", ...)` — the conv tree runs the whole briefing; script waits until both sides leave the conversation, then deletes the fireman. **Skipped on retry** (`IsMissionFromRestart`). |
| F2 | **Job start** | Player given 326 with 0 ammo (bottomless extinguisher, **INFERRED**); text `2_R04_10`; on the very first run (`g2SS07MissionState < 1`) an extra 4 s control-off tutorial beat (text `2_R04_11`). |
| F3 | **Per-area fire call** | Region picked at random (only "Downtown" exists; difficulty filter is vestigial — all regions are difficulty 0), then an **unused area** within it (no repeats within a run). A `RADARICON_FIRE` blip marks the area point; a one-shot enter trigger is armed. On entry: `FireCreate` on each of the area's registered props with per-model settings (burn interval, fire damage, min/max fire health); first run gets the hint line again. |
| F4 | **Extinguish** | Player sprays fires down (fire health → 0 = out; prop health bars hidden as they go out — bars were suppressed at creation, so the bar logic is belt-and-braces). **Fail:** any registered prop is fully destroyed by its fire (`PAnimIsDestroyed`) → `F_MonitorObjects` trips → `F_FailMission` (`M_FAIL` text, fail music, `MissionFail()` — no key, no explicit retry flag = defaults). |
| F5 | **Next area / pass** | All fires in the area out: blip cleared; if more areas remain (`intNumAreasChosen`, see ladder) a "next fire" line (`2_R04_12`) prints and F3 repeats. After the last area: `shared.g2SS07MissionState += 1`, `M_PASS` text, pass music, `MissionSucceed()` (defaults). |

### Difficulty ladder (persistent across runs)

`shared.g2SS07MissionState` ≤ 2 → 1 area; ≤ 5 → 2 areas; ≤ 10 → 3 areas; else 4 areas. (`intChosenDifficulty` is always 0 — the region-difficulty filter never excludes anything; vestigial.)

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| A burning prop is destroyed | `F_MonitorObjects` (`PAnimIsDestroyed` on any registered prop) | none (`MissionFail()` plain) |

No timer, no player-death handling (engine default), no other fail. Retry re-arms from F0 and skips F1.

### Rewards / pass side-effects

None visible: no money, no items, no unlocks, no `SMissPass` entry (**VERIFIED** absence). The only progression is `shared.g2SS07MissionState` (area-count ladder) — presumably the *wrapper* (`2_S07`?) pays out; **OPEN**.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | F1 briefing | in-engine **conversation tree** (`Act\Conv\2_R04.act`), not a camera scene | our conversation system + `say` beats; no letterbox |
| — | everything else | pure gameplay | — |

No baked cutscenes, no loaded anim groups, no speech events (`M_2_R04` does not exist in this file — the conv tree owns all voice).

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 82 | fireman | briefing conversation only, then deleted |

### Props / fires (the content table)

Three-region-per-model tuning (`tblFireSettings`): burn interval ms / fire damage per tick / fire min-max health — news 15000/15, dumpster 15000/12, sign 30000/1, lgcrate 5000/1, garbagecan 15000/5, benchb 15000/15, crate 45000/1, fruitstand 15000/10 (min 20 / max 100 for all).

Registered fire props by area (TRIGGER names, 24 total):
- `_2_R04_BUSINESS_FIRE_REGION_01`: `_2_R04_NEWS_03` (news), `_ANIMPROPS_MAINMAP_BENCHA/01/02/03` (benches)
- `_2_R04_BUSINESS_FIRE_REGION_02`: `_TBUSINESS_FTSTAND01` (fruit stand), `_2_R04_NEWS_01` (news), `_2_R04_SIGN_01` (sign), `_TBUSINESS_GARBCANA08` (garbage can)
- `_2_R04_BUSINESS_FIRE_REGION_03`: `_ANIMPROPS_MAINMAP_DUMPSTER04/05`, `_TBUSINESS_DUMPSTER09` (dumpsters), `_2_R04_CRATES_01/02` (large crates), `_ANIMPROPS_MAINMAP_CRATE21` (crate)
- `_2_R04_BUSINESS_FIRE_REGION_04`: `_TBUSINESS_DUMPSTER06/07/08` (dumpsters), `_ANIMPROPS_MAINMAP_GARBAGECAN06/07`, `_TBUSINESS_GARBCANA04/03` (garbage cans), `_ANIMPROPS_MAINMAP_CRATE18` (crate)

### Weapons

326 = fire extinguisher (VERIFIED name), granted with 0 ammo (infinite use, **INFERRED**).

### Text

Keys `2_R04_10` (go to the fire — **paraphrase**), `2_R04_11` (extinguish tutorial — **paraphrase**), `2_R04_12` (another fire — **paraphrase**), `M_PASS`/`M_FAIL`. No scripted speech ids.

### Named world data to extract from `2_R04.DAT`

- POINTLISTs: `_2_R04_CORONA`, `_2_R04_PSTART`, `_2_R04_BUSINESS_FIRE_REGION_01`..`_04` (blip points).
- TRIGGERs: `_2_R04_BUSINESS_FIRE_REGION_01`..`_04` (enter-to-ignite), plus the 24 prop triggers listed above.
- PATHs: none. Area codes seen: **0** only.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| F1 briefing | conversation system, `cutscene_skip_on_retry` (`IsMissionFromRestart`) | — |
| F3 area pick | mission framework | **random_objective_area** (region → random unused area within the run; per-run no-repeat; fire blip) |
| F3/F4 fires | `prop_set_state` (partial) | **fire_prop_system** (`FireCreate`/`FireGetHealth`/`FireDestroy` bound to PAnim props with per-model burn tuning; fire-out event; **prop burned down = mission fail** variant of `object_destroyed`) |
| F5 ladder | `timer`-less repetition | **job_escalation_shared_state** (cross-run counter in `shared.*` gating objective count — same family as 2_R03's success-count tiers; could be one primitive with two backends) |
| extinguisher | `give_weapon` | — (0-ammo = infinite; note as a weapon-grant flag) |

## 6. Open questions

- What `2_S07` is and how it shares `g2SS07MissionState` (likely the job's story wrapper; file not read).
- Whether any payout exists (this script pays nothing; wrapper presumably does).
- Fire mechanics internals: how fire damage eats prop health vs. extinguisher effect on fire health (engine-side, undecoded); whether fires spread beyond the registered props.
- The vestigial `"PoorHouse"` region and `intChosenDifficulty` — cut content or a second job variant.
- Why prop health bars are suppressed at creation but hidden again on extinguish (dead code path, harmless).
