# Campaign spine design — clock, mission stars, chapters, unlocks

Date: 2026-10-22 · Branch: `kimi-backend` · Status: design for implementation.

The piece that turns "a mission runner" into "a story mode": missions become things that exist
in the world (star markers), are available at times, unlock each other, pay out the original's
rewards, and advance chapters. Scope here is everything Chapter 1 needs; later chapters add
content, not new spine.

## Data: `Config/Campaign.json`

One mission table, authored from `docs/re-notes/ch1/*.md` + the catalog's time windows:

```json
{ "_format": "bully-remake.campaign/1",
  "clock": { "start_hour": 8, "start_minute": 30, "minutes_per_second": 1 },
  "factions": ["bullies", "nerds", "preppies", "greasers", "jocks", "townies"],
  "missions": {
    "1_03": { "title": "...", "chapter": 1, "star": "gold",
              "star_location": { "area": 0, "game_m": [x, y, z] },
              "window": [8, 19],
              "prereq": ["1_02C"],
              "on_pass": { "give_weapon": 303, "respect": { "bullies": 20 } } },
    "1_B": { "title": "...", "chapter": 1, "star": "gold",
             "prereq": ["1_09"],
             "on_pass": { "chapter_set": 2,
                          "force_complete": ["1_11x2", "C_Chem_1", "C_Art_1", "C_Wrestling_1", "C_English_1"] } } } }
```

- `star_location` is optional (missions that chain or auto-start have no star). `area` is the
  original area code; the star only spawns while that area is the visible/loaded one (area 0 =
  outdoors; interior stars deferred — Ch1's stars are all outdoors or handled by chaining).
- `window` = [start_hour, end_hour) on the game clock; omitted = always. Halloween's
  time-stopped missions pair with the clock being paused, so a paused clock never expires a
  window (availability is checked, not aged).
- `on_pass` is the SMissPass.lua equivalent, per mission, in data. Supported v1 keys:
  `give_weapon` (int id → BullyWeaponComponent unlock), `give_ammo` {id, n},
  `respect` {faction: delta-or-set — v1: set, matching SFaction semantics},
  `force_complete` [ids], `chapter_set` (int), `unlock_collectible` (name → flag),
  `money` (cents → existing wallet). Unknown keys log a warning, never fail the mission.

## Systems

### `UClockSubsystem` (GameInstance)
Hours/minutes/day; ticks at `minutes_per_second`; `PauseClock`/`UnpauseClock`/`SetClock(h, m)`;
`IsInWindow(start, end)`; delegate on hour change (HUD clock later). Missions drive it through
new mission actions; Chapt1Trans/1_09/1_11xp clock behavior becomes data, not code.

### `UBullyCampaignSubsystem` (GameInstance)
- Completion map (id → success count), current chapter, per-faction respect values.
- `IsAvailable(id)`: chapter matches (or mission is chapterless), all `prereq` completed, clock
  within `window`, and id not already completed (v1: story missions are one-shot; repeat
  variants like `C_*_repeat` are a later classes concern).
- `NotifyPassed(id)`: applies `on_pass` (incl. force-completes, which recurse through the same
  path), bumps the count, broadcasts so stars refresh.
- Attempt counting per mission id (retry-skips-intro reads this instead of the per-session
  counter once missions start from the world).

### `AMissionStar` + `UMissionStarSubsystem` (World)
- Star actor: trigger volume + placeholder beacon visual (gold = story, red = side, blue X =
  errand later). Player overlap while no mission is active → `MissionSubsystem.StartMission(id)`.
- The world subsystem re-evaluates the table on: world begin play, clock hour change,
  `NotifyPassed`, mission end. Spawns available stars whose `star_location` matches the current
  area; despawns the rest. Stars never spawn while a mission runs or while `-BRMission=` /
  smoke/shot harness flags are on (test runs stay deterministic).

### `MissionSubsystem` glue (small, additive)
- New actions: `give_weapon`/`give_ammo`, `set_respect` {faction, value}, `clock`
  ({set: "8:30" | pause | unpause}), `chapter_set`, `force_complete` — each just forwards to the
  campaign/clock subsystems, so mission JSON can express in-script beats (1_09 pausing the clock)
  while campaign `on_pass` covers the SMissPass pass-side.
- `StartMission` refuses a mission the campaign says is unavailable **unless** started via
  `-BRMission=`/`Mission.Start` console (dev override — harness runs must keep working).
- `CompleteMission` → `Campaign.NotifyPassed(id)` (campaign absent = no-op, sandbox unaffected).
- Boot: unchanged (`BootMission`/`arrival` still the front door). After the boot chain completes,
  the world is free-roam with stars; the 1_02B→1_02C chain slots in as the first star-gated
  content exactly where the original puts it.

## Verification (headless, behavioral)

`-BRCampaignSmoke` (new flag in the smoke harness style):
1. Boot → day_one chain completes → assert the next expected star set appears (1_02B available,
   1_03 not yet).
2. Teleport into the 1_02B star → mission starts; complete it (driver) → 1_02C unlocks.
3. Clock: set 23:00 → day-windowed stars despawn; pause clock → availability stable; set 8:00 →
   stars return.
4. Pass-side: force-complete 1_09 → clock paused; force-complete 1_B → chapter = 2, 1_11x2 and
   level-1 classes completed, respect values set.
5. `run_mission_smoke.sh day_one` and `side_errand` still PASS (no regression to non-campaign
   runs).

## Explicitly out of scope for v1 (later waves)

Save/load persistence of campaign state, HUD clock widget, class schedule/attendance, prefect/
police authority loop, errand (blue X) giver framework, interior stars, minimap integration
(current waypoint system reused as-is). Each is called out in the ch1 recon docs where a mission
needs it; v1 missions stub around them exactly as the opening slice did.
