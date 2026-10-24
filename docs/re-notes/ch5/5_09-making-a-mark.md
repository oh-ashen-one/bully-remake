# 5_09 — Making a Mark: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_09.lua` (806 lines, complete) + cross-references in `SFaction.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_09 = "Making a Mark"** (canonical Chapter 5 list, #54 in `docs/story-mode-catalog.md`, 8AM–11PM) — **VERIFIED**: the [Bully wiki Chapter V page](https://bully.fandom.com/wiki/Chapter_V) maps script id `5-09` to "Making a Mark" (the chapter opener: Jimmy tags the face of Town Hall with a giant slogan, photographs it for proof, returns to the dorm to find the whole school has turned on him); the [StrategyWiki ch5 walkthrough](https://strategywiki.org/wiki/Bully/Chapter_5:_The_Fall_and_Rise_of_Jimmy_Hopkins,_Aged_15) matches the script exactly (climb the back ladders, three sequential tag spots, police respond and the trouble meter pins full, photograph the finished tag from the street, return to Jimmy's room; rewards: Townie respect +25 (=25), Nerd/Preppy/Greaser/Jock respect −75 (=25)).

Pass block (**VERIFIED** in-file): `MinigameSetCompletion("M_PASS", true, 0)` ($0), completion messages `MRESPECT_DP25`, `MRESPECT_NM75`, `MRESPECT_PM75`, `MRESPECT_GM75`, `MRESPECT_JM75`, then `SetFactionRespect(1, 25)` / `(5, 25)` / `(4, 25)` / `(2, 25)` / `(3, 25)`. The StrategyWiki "(=25)" notation reads these as **absolute** sets (cliques fall 100 → 25 = "−75"; townies rise 0 → 25 = "+25"), which is consistent with the message keys — but the 4_03 recon read the same native as a delta (`+20`/−10). The two readings conflict; §6. Downstream: `SFaction.lua` — after 5_09 (until 6_B) the *default attitudes toward the player* for factions 1, 5, 4, 2 and 6 drop to 1 (hostile-ish): the whole-school-hates-Jimmy state is persisted there, not in the mission.

Setting: main map, area 0 (Bullworth Town / Town Hall) plus two interior waits: area 14 (boys' dorm — the intro waits for it to be left) and area 26 (shop interior — waited out after a spray-can purchase). All VO lives in the two baked cutscenes (`PlayCutsceneWithLoad("5-09")` intro, `"5-09B"` outro); the only scripted speech in-file is the gawker crowd bark loop. Music: locked interactive `MS_WIldstyleLow/Mid/High.rsm`.

## 2. Mission flow

Driver: `MissionSetup()` (baked intro "5-09", DAT, anim groups, **money floor: `PlayerSetMoney(100)` if below**) → `main()` runs four blocking phases in order: `F_GetToCityHall` → `F_TagCityHall` → `F_PhotoCityHall` → `F_GetBackToDorm`, then the outro + pass block. One daemon thread each for the tower-top camera (`T_TowerCam`) and, during the tag phase, the spray-can watchdog (`T_GetSpray`).

| # | Beat | Logic (functional) |
|---|---|---|
| M0 | **Setup** | Models: peds 81/131/80, vehicle 295, weapon 321 (spray can). `T_TowerCam` armed: while the player is inside `_5_09_TOWERTOP` the camera rides the `_5_09_TowerCam`/`_5_09_TowerCamLook` path pair and user camera control is locked; leaving restores it. Player placed at `_5_09_PLAYER_START`. |
| M1 | **Get to Town Hall** | Objective `5_09_OGETHALL` + blip on the grounds. Spray watchdog running (below). Wait for `_5_09_CITYHALL_GROUNDS`; then objective `5_09_OT_07` and the blip moves to `_5_09_LADDER_CITYHALL_CHEAT`. Progress requires the player to be inside `_5_09_CITYHALL_BUILDING` *while playing the ladder-climb action state* (`PedIsPlaying(gPlayer, "/Global/Ladder/Ladder_Actions")`) — i.e. actually climbing, not standing inside. Then blip to the tag point, wait for `_5_09_CITYHALL_TOWER`, then the tag PAnim chain is created (`F_CityHallTagsInit`: `TaggingStartPersistentTag` + `PAnimCreate(TRIGGER._5_09_CITYHALL_TAG)`), and `_5_09_CITYHALLTAG2`'s trigger arms the final leg. Objective `5_09_OTAGCITY`; camera range multiplier ×4 applied (digicam 426 if owned, else camera 328) — the photo beat is being pre-armed. |
| M1a | **Spray-can watchdog** (`T_GetSpray`) | If the player lacks item 321: money < 100 and not currently shopping → mission fail `5_09_FAILSPRAY` (no fade, retry allowed). Otherwise the current objective/blip are **suspended** and replaced by a buy-spray objective `5_09_OSPRAY` + blip on `_5_09_GETSPRAY`; once the player owns spray again, the suspended objective and blip are restored verbatim (waiting out the shop-interior area 26 first). Runs until tagging is done. |
| M2 | **Tag the wall (3 sequential tags)** | The big slogan is three PAnim tag props chained: completing `_5_09_CITYHALL_TAG` sets its anim node to the first "tagged" state (`.../Tagged/Bull`) and creates `_5_09_CITYHALLTAG2`; that one flips to `Tagged/Worth` and creates `_5_09_CITYHALLTAG3`; the third flips to `Tagged/Less`. (`cbTagDone` per prop; together the three compose the single slogan — paraphrased; the node names are citations.) Readiness helpers `F_Tag1/2/3Ready` exist for the tagging system to poll. All three → `bTagsDone`. |
| M2b | **Busted-by-design NIS** | On completion: control off, **punishment points pinned to 200**, and `F_CreateGawkers` runs the in-engine scene — full population + vehicle purge, a cop car (295 + cop 83) arrives along `_5_09_COPCARNISPATH` with siren forced on, five gawker peds (models 81/135/131/78/80) spawn at `_5_09_GAWKERS` playing the crowd anim, crowd ambience `MS_5-09_CrowdNIS.rsm`, camera rides `_5_09_AFTERTAGCAM`/`_AFTERTAGLOOK` (~8 s), then hands back via the tower-cam paths. Population/vehicles restored. Thread `T_HACKPUNISHMENTLEVEL` then **re-applies 200 points every 250 ms** while the player stays on the grounds and isn't hidden in a stealth prop — the heat cannot decay until the player leaves (matches the walkthrough's "meter doesn't empty until you're back on the ground"). Objective `5_09_OT_09`: photograph the tag. |
| M3 | **Photograph the proof** | Blip on `_5_09_PHOTOPOINT`; wait for the `_5_09_CITYHALL_WALL_CLIMBED` trigger (off the wall, back at street level). The finished-tag prop `DL_BU1d_bWless` is created as a persistent entity at hardcoded (650.224, −90.3781, 34.5168) and the three tag PAnims are deleted — the authored slogan replaces the interactive decals. `T_Photography`: the tag's **four corner points** (`_5_09_CITYHALL_TAG` elements 1–4) must *all* be inside the camera frame for the shot to be valid (`PhotoTargetInFrame` ×4 → `PhotoSetValid`); a valid `PhotoHasBeenTaken` while not inside the building trigger sets `CityHallPhoto`. (Dead alternate path `F_PictureTaken`/`IsTagValid` — see §6.) Gawker bark daemon `T_GawkerSpeech` — defined with a random `SEE_SOMETHING_CRAP` picker every 5 s, but never started in-file; §6. |
| M4 | **Return to the dorm** | Objective `5_09_ORETURN` + blip on `_5_09_PLAYERS_ROOM`. Petey (134) is spawned in the room, stimuli- and attack-immune, min health 0.5, kept in his `PeteSit` loop and snapped back to his spot if displaced. The phase ends when the player enters the room trigger — **or if Petey is hit** (`PedIsHit(idPeter, 2, 1000)`; he's attack-immune so this is an edge-case guard, but it's a real alternate exit — **VERIFIED** quirk). Control off, objective complete. |
| M5 | **Outro + pass** | Models unloaded, fade, Petey deleted, baked cutscene `5-09B`, player teleported to `_5_09_ENDPOINT` under a fixed camera (−496.31, 312.60, 32.44 → …), `M_PASS` banner ($0 + the five respect messages), pass music, the five absolute respect sets, `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| No spray and can't afford it (< $100) while not shopping | `T_GetSpray` watchdog | `5_09_FAILSPRAY` (`MissionFail(false, true, …)` — no fade, retry) |
| Player KO / busted (the pinned-200 heat makes a bust likely) | engine default | — |

No checkpoints; retry replays from M0. Cleanup: camera range multipliers restored to 1, camera control re-allowed, the persistent slogan entity deleted, interactive music stopped, anim groups unloaded. Note the punishment-pin thread and tower-cam thread terminate on `MissionActive()` going false.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked (`5-09`, extra arg `true`) | `fade` + `letterbox` + `say` |
| CS-2 | M2b crowd/police arrival | in-engine (population purge, cop-car path, gawker anim loop, two camera path pairs) | `crowd_gawk_disturbers` + `camera_rail_pair` + `scripted_vehicle_arrival` |
| CS-3 | outro | baked (`5-09B`) then in-engine fixed camera for the banner | `inline_cutscene_chain` + `completion_banner` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 134 | Petey | dorm-room sit loop; min health 0.5; stimuli/attack immune |
| 83 | cop | NIS driver, warped into the patrol car |
| 81, 135, 131, 78, 80 | gawkers ×5 | spawned at `_5_09_GAWKERS`, crowd anim, (unused bark daemon) |
| 10 | Earnest | table entry only — **never spawned in-file** (§6) |
| 141, 24, 28, 26, 29, 22, 45, 43, 46 | dog / greaser thief / greaser thugs / dropout contact & thugs | **cut side-errand layer** — creators exist, never called (§6) |

### Animation groups / action trees / props

`5_09MakingAMark`, `W_SprayCan`, `NPC_Adult`, `Act/Conv/5_09.act` (incl. `PeteSit`, `5_09_Cityhall_Cut`). Tag props: `_5_09_CITYHALL_TAG` / `TAG2` / `TAG3` (PAnim, `BigTag/NotUseable/Tagged/{Bull,Worth,Less}` nodes) + persistent replacement `DL_BU1d_bWless`. Vehicles: 295 patrol cars (NIS + unused `F_CreateCops`), 281/279 transport pair (unused), 280 dropout bike (unused).

### Speech

No `M_5_09` scripted speech events in-file — all dialogue is inside the baked cutscenes. Ambient: gawker `SEE_SOMETHING_CRAP` picker (daemon defined, never started). Music: `MS_WIldstyle*.rsm` interactive, `MS_5-09_CrowdNIS.rsm` ambience, pass/fail stingers.

### Named world data to extract from `5_09.dat`

- POINTLISTs: `_5_09_PLAYER_START`, `_5_09_CITYHALL_GROUNDS`, `_5_09_LADDER_CITYHALL_CHEAT`, `_5_09_CITYHALL_TAG` (4 — the photo corner points), `_5_09_CITYHALL_SQUARE` (150 m arming radius), `_5_09_GETSPRAY`, `_5_09_PHOTOPOINT`, `_5_09_CITYHALL_PHOTOOP`, `_5_09_PLAYERS_ROOM`, `_5_09_PETEY`, `_5_09_GAWKERS` (5), `_5_09_COPCAR`, `_5_09_ENDPOINT`, `_CITYHALLTAGBLIP`, plus the cut-content lists `_5_09_EARNEST`, `_5_09_TRANSPORT`, `_5_09_DROPOUT_CONTACT`, `_5_09_DROPOUT_BIKE`, `_5_09_GREASER_THIEF`, `_5_06_FIRST_GUARD_DOG` (note the `5_06` prefix — shared/copied data), `_5_09_PAINT`, `_5_09_DROPOUT_THUG_01/02`.
- PATHs: `_5_09_GREASER_THIEF` (unused), `_5_09_FIRST_GUARD_DOG` (unused), `_5_09_COPCARNISPATH`, `_5_09_AFTERTAGCAM`, `_5_09_AFTERTAGLOOK`, `_5_09_TowerCam`, `_5_09_TowerCamLook`.
- TRIGGERs: `_5_09_CITYHALL_GROUNDS`, `_5_09_CITYHALL_BUILDING`, `_5_09_CITYHALL_TOWER`, `_5_09_CITYHALL_TAG` / `_5_09_CITYHALLTAG2` / `_5_09_CITYHALLTAG3` (prop/trigger name pairs), `_5_09_CITYHALL_WALL_CLIMBED`, `_5_09_TOWERTOP`, `_5_09_PLAYERS_ROOM`.
- Hardcoded: persistent slogan entity position (650.224, −90.3781, 34.5168); outro camera (−496.31, 312.60, 32.44).
- Area codes seen: **0**, plus waits on visible-area 14 (dorm) and 26 (shop).
- **Trigger data (DAT): YES** — `DATLoad("5_09.DAT", 2)`. Original lives in `Trigger.img` per `trigger-format.md`; the research clone is Lua-only. Extraction pending; not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `inline_cutscene_chain` (two baked cutscenes), `money_floor_ensure` (setup floor of $100) + `money_floor_fail` (`5_09_FAILSPRAY` — the same affordance pattern as 3_S10), `tag_prop_system` + `trigger_chain` (three chained tag props with per-stage prop states), `player_action_state` (ladder-climb gate), `punishment_points_add` (the pin is a zone-polled re-application of it — composition, no new name), `camera_photo_upgrade` (range multiplier ×4), `photo_capture_condition` (M3 extends it to *all four corner points in frame* — parameterize as multi-target framing), `crowd_gawk_disturbers` + `bark_loop` (gawkers), `camera_rail_pair` (NIS + tower-top override), `scripted_vehicle_arrival` + `vehicle_siren_engine_flags` (cop car), `persistent_fade_entity` + `world_entity_swap` (`DL_BU1d_bWless` replacing the tag PAnims), `area_visible_eq` (dorm/shop waits), `completion_banner`, `set_faction_respect`, `respect_delta`, `ped_action_node`, `npc_ignore_stimuli`, `stealth_prop_entered` (the heat-pin escape hatch), `dynamic_blip`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| M1a spray errand | `money_floor_fail` (fail half) | **objective_suspend_resume**: a mid-stage errand that swaps the live objective + blip out for a side objective and restores them verbatim on completion (the original tracks `gCurrentObjective`/`gCurrentBlipPoint` manually). Schema stages currently have no notion of a temporary sub-objective |
| M3 four-corner framing | `photo_capture_condition` | none — add a "points[]: all in frame" parameter to the photo condition |
| M2b heat pin | `punishment_points_add` + zone poll | none — composition (re-apply while in zone and not `stealth_prop_entered`) |
| M4 Petey-hit alternate exit | `ped_hit_event` + schema `any` | none — `any(trigger_enter, ped_hit)` covers it |

Genuinely NEW names introduced here: **objective_suspend_resume**.

## 6. Open questions

- **Cut side-errand layer (large)**: `F_LeaveDorm` (+ transport bikes 281/279, dropout contact 45), `F_CreatePaint`/`F_CreateCutPaint` (spray-can pickup + blip at `_5_09_PAINT`), `F_CreateMeat` (two BBQ-meat pickups — its `BBQMeat` table is never even initialized), `F_CreateGuardDog` (sleeping stealth dog 141 on the `5_06`-prefixed point/path), `F_CreateGreaserThief` (greaser 24 walks a path and mounts dropout bike 280 at node 4), `F_CreateGreaserThugs` (four sleeping greasers woken hostile), `F_CreateCops`, the `GotBike`/`GotPaint`/`GotMeat`/`DogBribed`/`BikeReturned` flags — all defined, none reachable from `main()`. TCRF's unused-dialogue page for Chapter V supports a bigger original mission (fetch/bribe/retrieve errands before the tagging). **OPEN** — document as cut content; do not design the port around it.
- `SetFactionRespect(x, 25)` semantics: absolute here (per the walkthrough "(=25)"), delta in the 4_03 reading. One native, two recon conclusions — resolve against engine behavior or other call sites before the campaign table is authored. **OPEN**.
- `T_GawkerSpeech` (random gawker bark every 5 s until the photo is taken) is written but never `CreateThread`ed — the gawkers are silent in the shipped mission. **OPEN** (bug or deliberate mute).
- `F_PictureTaken` / `IsTagValid` / `F_GivePlayerCamera` (empty) / `F_CameraText` (empty) are dead alternates of the photo validation — the live path is `T_Photography`. **OPEN** (cut earlier photo design).
- The M1 ladder beat keys on `PedIsPlaying(.../Ladder/Ladder_Actions)` inside the building trigger — if the player reaches the roof by any non-ladder route (future physics), the stage still completes at `_5_09_CITYHALL_TOWER`; the `_5_09_LADDER_CITYHALL_CHEAT` point name suggests the designers knew. Cosmetic. **OPEN**.
- `T_HACKPUNISHMENTLEVEL` is named as a hack in-file; confirm the ported heat system reproduces "pinned at 200 while on the grounds, decays once off them" without the 250 ms re-application loop. **OPEN**.
