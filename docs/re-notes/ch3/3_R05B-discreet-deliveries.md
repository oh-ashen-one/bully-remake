# 3_R05B — Discreet Deliveries: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_R05B.lua` (302 lines). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_R05B = Discreet Deliveries** (catalog #42, optional). **VERIFIED** by structure: collect a package from an assistant on a timer, hand-deliver to five stationary recipients around the rich district on a second timer, then report back to the chemistry class — the "deliver packages, don't hit the clients" errand. The `R05` numbering is the delivery job family; `B` marks the second half of a two-part script (cutscene `"3-R05A"` opens the mission, `"3-R05B"` closes it). Quirk: the script loads **`3_09.DAT`** and prints "3_09" in its debug strings (VERIFIED) — an internal alias; the canonical star is 3_R05B.

Notable: `PlayCutsceneWithLoad("3-R05A")` in `MissionSetup` and `PlayCutsceneWithLoad("3-R05B")` in the pass sequence — both baked.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → `F_ArrangePickup` → poll loop → `F_Mission()` → `BackToChemClass()` → `F_MissionEnd()`.

| # | Beat | Logic (functional) |
|---|---|---|
| D0 | **Setup** | Fade from black; `3_09.DAT`; anim groups `1_G1_TheDiary`, `1_06ALittleHelp`; action tree `Act/Conv/3_09.act`; ped models {106, 183, 55, 151, 131, 132} + prop model 521; baked `"3-R05A"` intro; player placed at `_3_09_PLAYSTART` in **area 2** (VERIFIED). |
| D1 | **Fetch the package (120 s)** | Assistant ped (model **11** — note: not in the `LoadPedModels` list, **OPEN**/possible under-load) spawned at `_3_09_ASSISTANT`, invulnerable, blipped; objective `3_R05B_ASSISTANT`; **120 s mission timer**. Reaching within 3.5 m (on foot, weapon ≠ 437 skateboard) triggers the hand-off: control off, timer stopped, player bark (M_3_R05A id 1), mutual grapple-target + face, the assistant's `Give/GivePackage_3_09` anim, assistant made ambient. |
| D2 | **Five deliveries (300 s)** | Five recipients spawn at `_3_09_PAK01..05` (models 183, 55, 151, 131, 132), each stationary + ignore-stimuli + **mission-critical with a player-hit callback** (`F_HitReceiver`), blipped (icon 17). Objective counter `3_R05B_MOBJ_02` counts down from 5 with a `package` HUD icon. Each delivery: walk within 2 m on foot (no skateboard) → control off, mutual grapple/face, player's `GivePackage_3_09` anim, recipient released to ambient, counter +1 and objective param refreshed. New **300 s** timer. |
| D3 | **Return leg** | All five delivered → objective swaps to `3_R05B_MOBJ_03`, counter HUD cleared, blip on `POINTLIST.CLASS_CHEM`, `"delivery"` special entities loaded; timer **stopped** (untimed return, VERIFIED). Reaching the chem-class point ends it. |
| D4 | **Pass** | Recipients deleted; baked `"3-R05B"` outro; back to `_3_09_PLAYSTART` area 2; pass music; `MissionSucceed`; completion banner `M_PASS` with **5000** money param ($50). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Fetch timer expires (120 s) | `MissionTimerHasFinished` in the pickup poll | (unkeyed) |
| Delivery timer expires (300 s) | same in `F_Mission` loop | (unkeyed) |
| Player hits a recipient | `RegisterPedEventHandler(dude, 0, F_HitReceiver)` (attacker == player) | `3_R05B_FAIL_02` |

No checkpoints; fail replays from the intro cutscene. `F_KillControls`/`F_ResetControls` are the standard punish/clock/lock-target reset pair; the mission itself pauses nothing except during hand-off beats (control-off windows only).

### Rewards / pass side-effects

- $50 banner money (5000 cents param, VERIFIED by `MinigameSetCompletion("M_PASS", true, 5000)`).
- No `SMissPass.lua` case for 3_R05B (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | D0 intro | baked `"3-R05A"` | `fade` + original card |
| CS-2 | D1 fetch hand-off | in-engine give anim | `give_item_scene` (ch1 pool) |
| CS-3 | D2 each delivery | in-engine give anim ×5 | `give_item_scene` |
| CS-4 | D4 outro | baked `"3-R05B"` | `fade` + `completion_banner` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 11 | assistant (fetch target) | invulnerable; ambient after hand-off |
| 183, 55, 151, 131, 132 | the five recipients | stationary, ignore-stimuli, hit = fail |
| 106 | loaded but never spawned in-file (**OPEN** — likely used by the baked cutscenes) | |

### Props / items

- Model 521 loaded (the package prop, **INFERRED**).
- `GivePackage_3_09` anim nodes in `Act/Conv/3_09.act` (both directions: assistant→player and player→recipient).
- Weapon 437 (skateboard) explicitly disallowed during hand-offs (must be on foot).

### Speech / text

`M_3_R05A` id 1 (one bark); text keys `3_R05B_ASSISTANT`, `3_R05B_MOBJ_01/02/03`, `3_R05B_FAIL_02`. No music stream (end music only).

### Named world data to extract from `3_09.DAT` (note: DAT name differs from script id, VERIFIED)

- POINTLISTs: `_3_09_PLAYSTART`, `_3_09_ASSISTANT`, `_3_09_PAK01..05`, `CLASS_CHEM`.
- No paths/triggers used. Area codes: **2** (rich district, per the `_3_09_PAK`/rich-client models, **INFERRED**).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| D1 timed fetch | `timer` condition + `blip_on_npc` | **handoff_anim_beat** (proximity-on-foot gate → mutual grapple-target + give anim + counter/objective advance; `give_item_scene` in the pool is close — this is its repeatable form) |
| D2 deliveries | `collect_counter` (icon+max, here `package`), `mission_critical_npc` (hit = fail) | **no_vehicle_interact** variant on the proximity gate (weapon ≠ 437 and not in vehicle — a condition flag on `interact`) |
| D2/D3 timers | `timer` | **timer_restart** (stop the 120 s timer, start a fresh 300 s one mid-mission — our `timer` is per-stage; a stage boundary covers it) |
| D3 return | `reach_location` | — |
| fails | keyed fail text | — |

## 6. Open questions

- Why the assistant is model 11 while `LoadPedModels` lists 106/183/55/151/131/132 (11 missing — possibly loaded by the cutscene or a default set).
- What model 106 is for (baked cutscene cast, presumably).
- The mission-name string this registers as (`3_R05B` vs the `3_09` internal alias) and how the job board chains `3_R05A` → this — the sign-up/launcher lives outside the Lua.
- Whether hitting the assistant also fails (only recipients get `F_HitReceiver`; the assistant is invulnerable).
- `AreaLoadSpecialEntities("delivery", true/false)` — what the delivery dressing actually is (props at the chem lab door, **INFERRED**).
