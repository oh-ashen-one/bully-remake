# 2_S02 — Small Offenses (Hattrick's house vandalism): recon notes

> **Correction (2026-10-23, ch5 recon pass):** per the `2_S07` spec (`ch5/2_S07-small-offences.md`), the canonical name "Small Offences" belongs to **2_S07** (the SE-exclusive gnome-hunt unlock at the carnival). This script, 2_S02, is the Hattrick house-vandalism mission — a scrapped-PS2-mission remnant whose name SE appears to have reused. The identification below ("Small Offenses") should be read with that caveat; the mission content description stands.

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/2_S02.lua` (1697 lines) + `SMissPass.lua` (no 2_S02 case — **VERIFIED** absence) + `SZone.lua` (weapon ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_S02 = "Small Offenses"** (canonical Chapter 2 list, 8-24) — **VERIFIED** by content: the baked cutscene is `"2-S02"` (function `CS_GallowayMeeting` — Galloway gives the job), the player follows **Mr. Hattrick** (ped 61, `TE_MathTeacher` per the opening doc) driving home in his car (vehicle 297), slips into his yard as the gate closes, and smashes his property against a **destruction meter** while Hattrick's escalation barks play and **police waves** arrive, then escapes. This is the vandalism mission at Hattrick's house; the canonical-name match is **INFERRED** by elimination (no `LoadAnimationGroup` embeds a title — this file loads none).

**IDENTIFICATION SURPRISE / cross-reference:** the canonical list's "Hattrick vs. Galloway (8-18:30)" entry was recon'd as `1_S01` (see `ch1/1_S01-hattrick-vs-galloway.md` — the SE-added bottle-collecting stealth mission, Chapter 1). **2_S02 is a different mission** — the Chapter 2 follow-up where the feud escalates to wrecking Hattrick's house. Note the script does `ClockSet(18, 30)` at the meeting — the same 18:30 wall time the canonical list attaches to the 1_S01 entry; treat the canonical window labels as swapped/copy-pasted between the two Hattrick missions (**INFERRED**; worth a wiki correction).

Setting: entirely outdoors (area 0) — streets from the school to the rich neighborhood and Hattrick's yard. No interior area transitions.

## 2. Mission flow

Driver: `main()` polls a `gCurrentStage` function pointer (`Stage00 → Stage01 → Stage01A → Stage01 → Stage02 → Stage03 → Stage04`) under a `MISSION_RUNNING/PASS/FAIL` state. Threads: `T1_HattrickCarGovernor` (speed control), `T_HattrickEntersHouse`, cop-wave spawners/monitors, `T_DestructoTutorial`.

| # | Beat | Logic (functional) |
|---|---|---|
| V0 | **Setup + baked cutscene "2-S02"** | `2_S02.DAT`; nav nodes inside `_2_S02_PATHVOLUME` switched off; slingshot (303) requested; `Engine.bnk` audio. `CS_GallowayMeeting`: cutscene with load, player placed at `_2_S02_AFTER_NIS`, models loaded (297 Hattrick's car, 295 cop car, 275 cop bike, peds 61/97/83), **clock forced to 18:30**. |
| V1 | **Find the car (Stage00)** | Blip on `_2_S02_HATTRICK_CAR` + objective `2_S02_OBJ00`. When the player enters `_2_S02_CARINIT` the car + Hattrick spawn (Hattrick asleep at the wheel, untargetable, player-ignoring, flag 108) and the car creeps onto `PATH._2_S02_FROMSCHOOL` then stops; blip moves to the car. Damaging the car (damage number ≥ 2) → fail + Hattrick hops out hostile. Entering `_2_S02_CARTRIGGER` → next stage. |
| V2 | **Tail the car home (Stage01/A)** | Objective `2_S02_OBJ01`, ambient vehicles reduced, follow tutorial (`TUT_SKTC01` — skateboard, **INFERRED**). After 3.5 s the car drives off along `_2_S02_FROMSCHOOL`; a governor thread ramps cruise speed 0.5 → 7, bumps to 12 at `_2_S02CARSPEEDUP`, and **opens the estate gate** (`_HATTRICK_GATE`) at `_2_S02_INTRO_FAILURE`. Fails: stand on the car or damage it (Hattrick leaves the car hostile), or **fall more than 70 m behind** (3D distance check each frame). Car reaching `_2_S02_HATTRICKNIS` → arrival NIS. |
| V3 | **Arrival NIS (in-engine)** | Widescreen, control off; player (bike dismounted) walks `_2_S02_PLAYERENTERYARD` while the car pulls into the driveway (`_2_S02_CARENTERDRIVEWAY`, speed ramp 1.5→4.5), engine off, door sounds, Hattrick walks `_2_S02_HATTRICK_TO_HOUSE`. **Breakable registry armed here** (see V5); if Hattrick is hit or anything breaks before he's indoors → fail + he attacks. Once inside: house door locks, outdoor Hattrick deleted, an **invulnerable, untargetable, immobile indoor Hattrick** spawns (`_2_S02_HATTRICKINHOUSE`, min health 50). |
| V4 | **Get in the yard (10 s)** | Objective `2_S02_OBJ04`, radar zoomed (30/75/45), blip on the yard, `MissionTimerStart(10)`. Timeout → the gate locks (`HattrickGateOpen(false)`) and the mission fails with a punishment-flavored key (`PUN_02`). Entering `_2_S02_HATTRICKYARD` stops the timer → destruction stage. |
| V5 | **Smash everything (meter 0/80)** | Counter HUD in **meter mode** (icon `Window`), objective `2_S02_OBJ02`, tutorial after 3 s. **~140 breakables registered by name hash** (see assets): windows/doors cost 3, props (vases, planters, flowers, lamps) cost 2; each break event adds its cost to the meter (car damage ticks add +2 per damage state). Escalation ladder on the meter: line at 5; **cop wave 1 at 10** (two patrol cars 295 with sirens drive in and park at their STOP triggers; cops 97 dismount and start stealth patrols); lines at 15, 20; **cop wave 2 at 25** (cop bikes 275 + cops 3/4/5 = models 97/83/83 on foot patrol; music to `MS_DestructionVandalismMid`); lines at 30/35; **cop 6 at 40**; music to `...High` at 45; lines at 50/60. Meter ≥ 80 → final bark (id 8), cop 3 runs for the house, `bAllBroken`. Leaving the yard early → fail `2_S02_FAIL04`. |
| V6 | **Escape (Stage04)** | Objective `2_S02_OBJ03`; indoor Hattrick converted to a **chaser**: moved to the door, targetable, min-health floor removed, stats boosted (3→40, 14→100, 33→150 — chase/speed class, meanings undecoded), stealth behavior with attack-on-spot callback (bark 10), walks `_2_S02_HATTRICKLEAVEHOUSE`; the house door locks behind him (path-node cb). **Player exits the yard trigger → mission passes.** |
| V7 | **Pass** | Widescreen, pass music, `MissionSucceed(false, true, true, 4000)`. No reward in-script or in `SMissPass` (**VERIFIED** absence — a pure story beat). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Hit Hattrick / break anything before he's indoors | `T_HattrickEntersHouse` watch (`bHattrickHit`/`bObjectDestroyed`) | `2_S02_FAIL01` (he attacks) |
| Car damaged (≥ 2 damage states) or player stands on it | per-frame check in V1/V2 | `2_S02_FAIL02` / `2_S02_FAIL01` (he leaves the car hostile) |
| Fall behind the car | distance ≥ 70 m | `2_S02_FAIL03` |
| Not in the yard within 10 s | mission timer | `PUN_02` |
| Leave the yard during the smash stage | trigger exit | `2_S02_FAIL04` |

All fails use `MissionFail(false, true, key)` — retry allowed; no in-file cutscene-skip logic (**INFERRED** global attempt handling). Notably **cop busts are not a Lua fail** — cops use stealth behavior; the punishment system owns what a bust does (**OPEN**, same posture as 1_S01).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | V0 `"2-S02"` | baked | `fade` + `letterbox` + 2-3 original `say` beats (Galloway's request) or title card |
| CS-2 | V3 arrival | in-engine, ~8 s | `letterbox` + scripted `vehicle` path (driveway) + `player_follow_path` + door sounds + `npc_follow_path` (Hattrick indoors) |
| CS-3 | V6 Hattrick storms out | micro in-engine | `npc_follow_path` + `say` bark; no cut |

Cutscene anim groups: none loaded (all beats use stock sets).

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 61 | **Mr. Hattrick** | two instances (driver → indoor → chaser); `TE_MathTeacher` per opening doc |
| 97 | cop (cars wave; cop 6) | stealth behavior 1, perception override 3→15 |
| 83 | cop (bike wave: cops 4/5) | same |
| 297 | Hattrick's car | color 96/96; damage-number watched |
| 295 | cop car ×2 | sirens on, path drive, park at STOP triggers |
| 275 | cop bike ×2 | engine off, sirens on (parked dressing) |

### Breakable registry (the destruction meter)

~140 props registered via `ObjectNameToHashID` + create/break event handlers, all named `trich_*` (Hattrick's yard/house-trim instances): window families `trich_DPE_G_300x*` and `trich_DPE_G_150x*` (~60 panes), interior/door glass `trich_HT_MrHatricksXINT01-09`, `trich_HT_DormGxref85/86`, door props `trich_PrepDoor01`, `trich_DPI_pDoorBrk01`, `trich_DPI_pCabDoor02-05`; prop families `trich_HT_DPE_FlowersA*`, `trich_HT_DPE_FlowersB*`, `trich_HT_DPE_HatSVase21-33`, `trich_HT_DPI_pVase05-14`, `trich_HT_DPI_Planters19-23`, `trich_HT_DPI_SCfern04/05`, `trich_wall_lampSM67-111` (11). Costs: window/door class 3, prop class 2; meter max 80. Broken state persists: a create-callback re-breaks props already recorded broken (streaming re-creation).

### Weapons / audio / text

- Weapon 303 slingshot requested and given the smash context (**INFERRED** intended tool) — not force-equipped.
- Speech `M_2_S02` ids: 3, 4, 5, 6, 7, 8, 10 — **7 distinct scripted lines** (escalation ladder + attack bark).
- Music: `MS_DestructionVandalismMid/High` locked streams; `Engine.bnk`; pass/fail stingers.
- Tutorials/text: `TUT_SKTC01`, `2_S02_24` (destructo), objective keys `2_S02_OBJ00..04`, yard-leaving nag `2_S02_25`.

### Named world data to extract from `2_S02.DAT`

- POINTLISTs: `_2_S02_AFTER_NIS`, `_2_S02_PLAYER_INTRO_NIS2`, `_2_S02_BIKE_DELETE`, `_2_S02_HATTRICKYARDBLIP`, `_2_S02_EMERGENCYPOINT`, `_2_S02_HATTRICK_CAR` (1-2), `_2_S02_HATTRICK_DRIVER` (1-2), `_2_S02_HATTRICKINHOUSE` (1-2), `_2_S02_COP_CAR1`, `_2_S02_COP_CAR2`, `_2_S02_COP1`..`_COP6`, `_2_S02_COP_BIKE1`, `_2_S02_COP_BIKE2`.
- PATHs: `_2_S02_FROMSCHOOL`, `_2_S02_CARENTERDRIVEWAY`, `_2_S02_PLAYERENTERYARD`, `_2_S02_HATTRICK_TO_HOUSE`, `_2_S02_HATTRICKLEAVEHOUSE`, `_2_S02_COP_CAR1_PATH`, `_2_S02_COP_CAR2_PATH`, `_2_S02_COP1STARTER`, `_2_S02_COP1_PATH`..`_COP6_PATH`.
- TRIGGERs: `_2_S02_CARTRIGGER`, `_2_S02_CARINIT`, `_2_S02_CARFAILTRIGGER`, `_2_S02_HATTRICKNIS`, `_2_S02CARSPEEDUP`, `_2_S02_INTRO_FAILURE`, `_2_S02_HATTRICKYARD`, `_2_S02_END_TRIGGER1`, `_2_S02_PATHVOLUME`, `_2_S02_COP_CAR1_STOP`, `_2_S02_COP_CAR2_STOP`, `_2_S02_COP1`, `_HATTRICK_GATE`, `_HATTRICKDOOR` (+ gate id string `HATTRICK_GATE`).
- Hardcoded camera: arrival NIS (404.65, 500.79, 22.78 → 404.26, 501.70, 22.87).
- Area codes seen: **0** only.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| V1-V2 tail | `blip`, `timer` | **vehicle_tail** (follow an NPC-driven car: too-far fail variant of `npc_distance_leash`, car-damage fail (`CarGetDamageNumber` ≥ N), stand-on-vehicle fail, speed-governor thread: ramp cruise speed by stage, react to route triggers) |
| V3 arrival | `letterbox`, `npc_follow_path` + node cb, `door_prop_control` (gate/house door), `vehicle path` | **vehicle_park_dismount** (drive path → stop in trigger → engine off → door sounds → ped exits onto a walk path) |
| V5 meter | `object_destroyed` events, `collect_counter` (meter mode), `count_threshold` ladder | **weighted_destruction_meter** (name-hash-registered breakable set with per-family point costs; car-damage ticks feed the same meter; threshold ladder fires barks/spawns/music; broken-state re-apply on prop streaming re-create) |
| V5 cop waves | `wave_spawner`, `stealth_spotter` (behavior 1 + perception overrides), `spawn_npc` bundles | **scripted_vehicle_arrival** (sirened car drives a path, parks at a stop trigger, officer dismounts into a patrol) — composite of vehicle path + spawn; list once |
| V4/V6 yard timer/exit | `countdown_fail_timer` (10 s), `radar_set_range` | **exit_area_pass** (leaving the trigger completes — the mirror of ch1's `leave_area_fail`) |
| V6 chaser conversion | `spawn_npc` flag bundles | **npc_stat_override** (exists in ch1 set) + stealth-attack callback — no new |

## 6. Open questions

- What happens when a cop's stealth behavior spots the player mid-mission (bust vs. mission fail) — punishment system owns it; the mission itself never checks.
- `PAnimNumDestroyed(_2_S02_HATTRICKYARD)` (count of *any* destroyed PAnim in the yard trigger) is folded into the meter alongside the weighted registry — the two counters can double-count or drift; the script just takes the max (**INFERRED** intent: catch unregistered breakables).
- Gate id handling mixes `HATTRICK_GATE` (hash var) and `TRIGGER._HATTRICK_GATE` — same door, two lookup paths (**OPEN** which the DAT owns).
- Where the mission is offered (Galloway at the school? window 8-24) — mission table outside the Lua.
- The 18:30 clock set vs. the canonical window discrepancy noted in §1.
