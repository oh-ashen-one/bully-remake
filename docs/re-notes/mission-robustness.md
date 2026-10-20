# Mission robustness: thin triggers, teleports and door links

Status: 2026-10-20 (branch `phase2-missions`). Written without an engine: the data fixes are verified offline with
`tools/ue/audit_mission_skips.py`; the C++ and the `--wrong-way` smoke are NOT compiled or run yet (see "Not verified").

## The failure

`AMissionTrigger` is a data volume polled once per frame against the pawn's CENTRE (`ContainsPoint`). The front door is a
120 x 320 cm box (`main_door`) sitting on exactly the same doorway as the `main_door_ext` door link. The link starts its
fade when the pawn's capsule first touches its box and teleports after the fade-out, with input ignored meanwhile, so the pawn's centre usually never gets inside
the mission box. `use_door` (`trigger_enter main_door`) therefore never completed after a door use, and the stage waypoint
(`StageTarget`, nearest positional child of the stage condition) pointed at the lobby pocket ~875 m away. The playtest hit the
same race one level down: `leave_office` / `exit_building` on `door_int` (fixed 2026-10-19 with an `any` fallback).

Rule that fixes it: a stage that waits on a doorway must also complete on ARRIVING anywhere on the far side (a generous radius
around a point there). `any` lists the original trigger first, so the waypoint still points at it while the player is on the
near side.

## Audit (every stage whose completion is a position)

Offline rule (also what `--wrong-way` does in the engine): the pawn is teleported onto the primary goal of the NEXT stage
(first reach point / trigger centre of its condition, +100 cm Z) and must then satisfy the CURRENT stage; and any stage with a
goal on a door-link doorway must be satisfied on the link's exit. `python3 tools/ue/audit_mission_skips.py`.

| Mission | Stage | Risk | Fix |
| --- | --- | --- | --- |
| 1_01, day_one | `walk_in` (`trigger_enter 1_01_BullyTrig`, 580 x 3340 cm) | Medium. Teleport / vehicle past the box; next stage's goal `gauntlet_clear` is 16 m beyond it and not accepted | `any` + `reach gauntlet_clear` r 800 z 500 |
| 1_01, day_one | `get_past_bullies` (`event gauntlet_complete` or `reach gauntlet_clear` r 500) | Medium. Landing at the next goal (`courtyard_approach`, 53 m on) leaves it open | + `reach courtyard_approach` r 1200 z 600 |
| 1_01, day_one | `cross_courtyard` (`reach courtyard_approach` r 450 z 450) | Low. The door box is 400 cm from the point and inside r 450 | none (audit ok) |
| 1_01, day_one, smoke_day_one | `use_door` (`trigger_enter main_door`, 120 x 320 cm, SAME doorway as the `main_door_ext` link) | HIGH, the known race. Link arrival is ~875 m away at the lobby pocket; the stage stays open and the blip points back at the door | + `reach lobby` r 3000 z 1000 (whole school interior pocket; the exterior is 875 m away, only `ischool` lies within 80 m of it, so no false hit). The original `on_complete` events (`npc_gauntlet_clear`, `entered_school`) still fire |
| 1_01, day_one, smoke_day_one | `reach_lobby` (`reach lobby` r 450 z 300) | Medium. A stair/cheat landing at the office (next goal) skips it | + `trigger_enter 1_01_PrincOffice` + `reach office_approach` r 900 z 400 (smoke_day_one: the reach only; it has no office trigger) |
| 1_01, day_one | `go_upstairs` (`any` trigger or `reach` r 600) | Low. Already has the reach fallback; last stage of 1_01 | none |
| 1_02, day_one | `leave_office` (`any` lobby / `door_int` / `door_ext_out` r 900) | Fixed 2026-10-19 | none (audit ok; door-link lint ok) |
| 1_02, day_one | `exit_building` (`any` `door_int` / `door_ext_out` r 900) | Low-medium. Door arrival was already covered; a cheat/teleport landing in the dorm court (next goal `gary_trigger`) left it open | + `trigger_enter gary_trigger` |
| 1_02, day_one | `cross_campus` (`trigger_enter gary_trigger`, 2900 x 1128 cm) | Low. Next stage is a timer whose `on_start` teleports the player itself, so there is nothing to skip to | none |
| 1_02, day_one | `to_dorm` (`any` `dorm_door` trigger 360 x 400 / `reach` r 350) | Low. No door link here (a `fade` action teleports); box is wider than a sprint step | none |
| 1_02, day_one | `briefing`, `fight_intro`, tutorial / fight stages, `crowd_leaves`, `staff_coming`, `end_card` | n/a. Timers and combat events, nothing positional | none |
| side_errand | all five stages (`interact` on 400 x 400 cm boxes, z +-250) | n/a. Interacts need the player to press E inside the box; a teleport cannot satisfy or step past them | none |
| arrival | all six stages (timers) | n/a | none |

Parser floor (all missions): `reach_location` radius is never narrower than 200 cm and `z_tolerance_cm` never under 150 cm
(`MinReachRadiusCm` / `MinReachZCm` in `MissionSubsystem.cpp`, logs a warning when it widens). No shipped mission is under it.

Audit run, before -> after: `1_01` 4 flagged -> 0, `1_02` 1 -> 0, `smoke_day_one` 2 -> 0, `day_one` 0 -> 0 (it is generated from
1_01 + 1_02 by `Config/Missions/verify/build_day_one.py`, rerun after editing either; it was rebuilt here). `arrival` and
`side_errand` have nothing to flag.

## Wrong-way smoke

    tools/ue/run_mission_smoke.sh 1_01 --wrong-way            # also: smoke_day_one
    python3 tools/ue/audit_mission_skips.py [mission ...]     # same rule, offline, no engine

`-BRMissionSmokeWrongWay` is an option of the existing `-BRMissionSmoke=<id>` harness (`UMissionSubsystem::TickSmoke`). For every
stage whose completion is positional (`SkippablePositional`: a reach / trigger_enter, alone or in any/all), 1.5 s after the stage
begins it (1) teleports 8 m past the stage's primary goal (overshoot; result is informational: the player would walk back), then
(2) teleports onto the primary goal of the next stage if that is positional too (the far side of a door link) and gives the stage
2 s to complete. A stage still open is logged `BR_MISSION_SMOKE wrongway STUCK ...`, added to the stuck list, and the normal driver
takes over so the remainder of the mission still runs; any stuck stage makes the final `BR_MISSION_SMOKE` line FAIL. Output goes
to `.../missions/smoke_<id>_wrongway/`. The normal-mode driver now aims at the first positional condition in declaration order
(`PrimaryGoal`) instead of the nearest, so adding fallbacks to an `any` does not change which spot it visits.

## Not verified (no engine here)

- `MissionSubsystem.h/.cpp` changes are uncompiled: see the review list in the task report (parser clamp, `FSmoke` fields,
  `SkippablePositional`, `PrimaryGoal`, wrong-way block in `TickSmoke`, `NoteWrongWayDone`, completion verdict).
- The wrong-way smoke has not run. Normal `run_mission_smoke.sh 1_02` cannot finish on its own (the driver has no handler for
  `any(event, event)` stages such as `free_fight`), so use `1_01` or `smoke_day_one` for `--wrong-way`.
- Door-link behaviour is read from `BullyDoorLink.cpp` and `tools/ue/door_links.json`, not observed this session: the lobby
  fallback radius assumes the link exit (-62860, 32200, 0) and the lobby point (-62860, 31070, 0) as authored there.
