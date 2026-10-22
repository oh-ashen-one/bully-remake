# 1_10 — Halloween (recon)

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED**, **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_10.lua` (1112 lines), `SMissPass.lua`, `main.lua`, `STimeCycle.lua`. Functional description in our own words only; script/file/native names are citations.

New area codes seen here: **8 = the school janitor basement** (`shared.gAreaDATFileLoaded[8]`, door/prop names `DT_JANITOR_*`, `isc_janitor`-family triggers `_JAN*`; **INFERRED** from naming and the basement content, bounds not checked).

## 1. Identification

**1_10 = Halloween. VERIFIED.** Evidence:

- `MissionInit` loads the Halloween set dressing (`AreaLoadSpecialEntities("TombstonePost"/"PumpkinPost", true)`) and `main.lua` (~line 974) makes those permanent once `IsMissionCompleated("1_10")`.
- The mission is a night-time escort of Gary (ped model 130, variable `gGary`) into the school basement, ending when the player enters "the hole" — the canonical lead-in to the chapter-1 boss; on pass the script spawns a 4-ped audience at `_1_10_AUDIENCE`/`_1_10_AUDVIEW` points, i.e. the crowd for the Russell fight in 1_B.
- Animation group `1_10Betrayal` is loaded (Gary's betrayal beat). It is loaded/unloaded but never referenced by a `PedSetActionNode` in this Lua — presumably consumed by the baked cutscene or DAT-side prop/tree assets (**INFERRED**).
- The clock stays paused from 1_09's cleanup (nothing in 1_10 unpauses it) — the canonical "time-stopped" Halloween night.

Not to be confused with the Halloween-night prank side-missions 1_11x1/1_11xp/1_11x2 (those are separate scripts; `SMissPass.lua` and `main.lua` show costumed Gary/Petey allies, models 160/165, for that chain).

## 2. Mission flow

`MissionSetup`: DAT `1_10.DAT` loaded (slot 2), POI disabled, ped-type 11 population suppressed, no fade-in. `MissionInit` (called inside the intro): models {23,10,19,37} audience, {102,99,85} bullies, {130 Gary, 362}, weapons {326, 303}, anim groups `1_10Betrayal`, `Px_RedButton`, `MINI_Lock`, trees `Act/Anim/1_10.act`, `Act/Props/Switch.act`, `Act/Props/AsySwtch.act`, `Act/Anim/Funhouse.act`, sound bank `MISSION\1_10.bnk`; player transitioned to area 0 `_1_10_PSTART_MM`; school side doors `DT_TSCHOOL_SCHOOLSIDEDOORL/R` locked.

Note: `bMissionFailed` initializes **true** and is only cleared on pass — the default state is fail.

| # | Beat | Stage function(s) | Logic (functional) |
|---|---|---|---|
| H0 | Baked intro cutscene `1-10` | `CS_Intro` | `PlayCutsceneWithLoad("1-10", true)`, then control returns; Gary (400 HP, mission-critical with attacked/KO callback, infinite sprint, social actions 28/29 disabled vs player) walks `_1_10_TO_PARKINGLOT` at run speed. Retry cutscene-skip is engine-side (**OPEN**). |
| H1 | Follow Gary | `Stage00_FollowGaryInit/Loop` | Objective id `1_10_OBJ00` + blip on Gary; Gary's named health bar shown; the three bullies spawn at `_1_10BULLY01/02A/02B` with hit-event handlers; bully 2 carries drop item 362 (**OPEN** what 362 is). Stage advances when Gary reaches the path end near the bullies AND (player enters `_1_10_BULLYTRIGGER` OR anyone hit a bully). |
| H2 | Parking-lot fight | `Stage01_BullyFightInit/Loop` | Gary faces the bullies and plays a taunt line (id 13, skippable by aggro); Gary is recruited as a player ally; objective id `1_10_OBJ01` + red blips on the three bullies; bully 1 attacks Gary, bullies 2-3 attack the player. **Fail:** Gary KO'd (message id `1_10_Fail1`). Stage ends when all three bullies are KO'd. |
| H3 | To the basement door | `Stage02_EnterBasementInit/Loop` | Gary plays a key-pickup anim (`/Global/1_10_anims/Gary/KeyPickup`), is dismissed as ally, and walks `_1_10_TO_BASEMENT`; blip on `_1_10JANDOORBLIP`, objective id `1_10_OBJ02`. When the player nears `_1_10_NEAR_BASEMENTDOOR`: side door unlocks, objective id `1_10_OBJ03`, ambient area transitions disallowed. When the player opens `DT_TSCHOOL_SCHOOLSIDEDOORL`, Gary walks `_1_10_ENTERBASEMENT`; stage ends when the visible area leaves 0. |
| H4 | Basement: Room 00 — plain switch | `Room00_SimpleSwitchInit/Loop` + `Room00_HintCheck` | Radar zooms to 30 m; basement exit `DT_JANITOR_MAINEXIT` locked; interactive music `MS_CarnivalFunhouseMiner.rsm`; wait for area 8 DAT; blip on prop switch `_JANSWTCH00`, objective id `1_10_OBJ04`; Gary teleports to `_1_10_GARYBS` then walks `_1_10_ROOM00` and waits facing the player. Player activates the switch (PAnim prop, `AsySwtch.act` Active node) → switch goes NotUseable, door NIS (below), Gary walks `_1_10_ROOM01`. Hint system: while the player is in `_1_10_ROOM0`, every 10 s (+2-5 s jitter) Gary speaks/points (id 23, `PointAtSwitch` anim); after 3 hints the switch is re-blipped. |
| H5 | Room 01 — the cage | `Room01_TheCageInit/Loop` + `Room01_HintCheck` | Blip on switch `_JANSWTCH01`; the switch sits behind/inside a cage (trigger `_1_10_INCAGE`); reaching it presumably involves the loaded `MINI_Lock` group (lockpicking?) — the Lua never says how the cage opens (**OPEN**). Activate switch → door `_JANDOORS01` → Gary walks `_1_10_ROOM02`. Hints: opening line id 24, then random from {25,26,28}, same 10 s cadence, re-blip after 3. |
| H6 | Room 02 — the broom | `Room02_BroomSwitchInit/Loop` + `Room02_HintCheck` | Blip on prop `_ANIBROOM`. This room's "switch" is the flag `bBroomHit`, set by `cbBroomHit` — whose event registration is not in this Lua (prop/DAT side, **OPEN**); functionally the player knocks the broom (throw/hit) to trigger the door. Success → door `_JANDOORS02` → Gary walks `_1_10_ROOM03`. Hints: id 28 opening, then random from {30,31}. |
| H7 | Room 03 — electric floor | `Room03_ElectricInit/Loop`, `T_GaryTurnOffElectric`, `Room03_HintCheck` | Electric arc effects (`ElectrocuteLRG`) spawn at every `_JANELECPTS` point, `PlasmaBall` sound emitter on, a No-Go blocker (`NOGO_ElecRoomOP` at (-762.0, -74.6, 10.8)) + `_JANELECTRICPROXY` panim; crawl tutorial `CRAWLING1`; blip at `_1_10_ELECBLIP`. Two solution routes: (a) the player crawls (`_1_10_CRAWL_ELEC`, crouch state checked) across and reaches `_1_10_GARY_TURN_OFF_ELEC` — Gary then walks to `_1_10_GARYSWITCHELEC`, plays the prop-interact node, arcs go off, door `_JANDOORS03B` unlocks; (b) the player pulls switch `_JANSWTCH03A`, which kills the arcs for only ~4 s (`T_ElectricityBackOn` re-arms them and resets the switch) — a timed dash. When `_JANDOORS03B` is open → door NIS (line 47) → Gary walks `_1_10_ROOM04`. Hints: id 33 opening, then 39; crouch reminder text id `1_10_CRAWL` when in `_1_10_CROUCHHINT` standing. |
| H8 | Room 04 — furnace | `Room04_FurnaceRoomInit/Loop` + `Room04_HintCheck` | A damageable fire entity (`boilerfire2`, 1000 HP, 40 dmg) on `_JANEXTRAFIRE` plus two ambient fires `_JANFIRE01/02`; steam jets (`steam_pipe` effects at `_JANSTEAMPTS`) + `NOGO_SteamOP` blocker + `_JANSTEAMPROXY`; blip on the fire. If the player stands at `_1_10_ATFURNACE` holding weapon 326 (**INFERRED** = fire extinguisher) a one-shot tutorial text id `1_10_FURNEXT` shows. Kill the fire (health ≤ 0) → `FlameOut` sound + `BoilerSteam` effect, steam off, door NIS (line 44), blip to `_1_10_HOLEBILIP`; after the player crosses `_AFTERSTEAM` the steam re-arms. Hints: id 41 opening, then random {42,43}. |
| H9 | Room 05 — the hole | `Room05_EnterHoleInit/Loop` | Gary's min-health set to 50. Player near `_1_10_NEARHOLE` → Gary line 49 + walks `_1_10_ENTERHOLE`. **Pass** when the player enters `_1_10_THEHOLE` (or Gary is attacked — odd branch, `bGaryAttacked` also passes the mission here). |
| H10 | Pass | `main` pass branch | If the player still has weapon 326 it is unequipped; the 4-ped audience (models 23/10/19/37) spawns and walks to `_1_10_AUDVIEW` facing the player (staging for the 1_B boss reveal); `MissionSucceed(true, false, false)` — first-arg meaning **OPEN** (likely "play success NIS/cutscene", which would be the betrayal scene using `1_10Betrayal`). |

Door NIS shared by H4-H8 (`NIS_OpenDoor` / `NIS_Room03DoorOpen` / `NIS_Room04DoorOpen`): ~1-2 s each — widescreen, control off, fixed camera at hard-coded coords aimed at the door, door opens and stays open (`PAnimOpenDoor` + pathable), Gary plays one random run-line from {40, 50, 9, 36} (or a fixed line for rooms 3/4), camera and control return.

Fail conditions:

| Fail | Cause | Handling |
|---|---|---|
| Gary KO'd | mission-critical callback `cbGaryAttacked` with Gary dead | message id `1_10_Fail1`; if the player is in the basement, extra fade and the player is placed at `_1_10_FAILURE`, side door re-locked |
| Player KO'd | `F_PedIsDead(gPlayer)` in the main loop | generic fail, same basement-respawn handling |

Retry/restart: not described beyond engine cutscene skip (**OPEN**). Cleanup always: radar restored, side doors re-locked on fail, `DT_JANITOR_MAINEXIT`/`DT_JANITOR_SCHOOLEXIT` unlocked, all hazards/effects killed, population/POI restored, `shared.on_mission_110 = false`.

Chains to: **1_B Help Gary (Russell boss)** — inferred from the audience staging at the hole and the `1_10Betrayal` group; the launch wiring is outside the Lua (**OPEN**). `STimeCycle.lua` keeps the day counter pinned until 1_B is done; `MainMap.lua` blocks school exits until 1_B.

## 3. Cutscene points

| Marker | Where | Type | Content (functional) | Suggested replacement |
|---|---|---|---|---|
| 1_10-CS1 | Before H1 | baked (`1-10`) | Night-time setup with Gary | `fade` + `letterbox` + short original `say` exchange |
| 1_10-CS2 | H4-H8, ×5 | in-engine micro-NIS | Door-open beat: fixed cam on the door, one Gary line, ~1-2 s | `letterbox` + `camera` + `say` + door `event`; cheap, keep as one reusable stage |
| 1_10-CS3 | H10 | baked/engine-side success scene (the betrayal, via `1_10Betrayal` + `MissionSucceed(true,…)`) | Gary turns on the player at the hole; audience revealed | `letterbox` + `camera` + `say` + spawn of 4 spectators; or fold into the 1_B intro |

## 4. NPCs and assets needed

**Ped models:** 130 (Gary, mission-critical), 102/99/85 (three bullies), 23/10/19/37 (hole audience).

**Animation groups:** `1_10Betrayal`, `Px_RedButton`, `MINI_Lock`. **Action trees:** `Act/Anim/1_10.act` (Gary key pickup, point-at-switch), `Act/Props/Switch.act`, `Act/Props/AsySwtch.act` (switch Active/Inactive/NotUseable states), `Act/Anim/Funhouse.act`, `Act/WProps.act` (Gary's scripted prop interact).

**Weapons/pickups:** 326 (player-usable in furnace room, **INFERRED** fire extinguisher), 303 (slingshot, model loaded), 362 (drop item on bully 2, **OPEN**), 502 health pickups at `_1_10_HEALTHPICKUPS` (pickup script `HealthBute`).

**Props/hazards:** PAnim switches `_JANSWTCH00/01/02/03A`, doors `_JANDOORS00/01/02/03B`, `_ANIBROOM` broom prop, `_JANELECTRICPROXY`/`_JANSTEAMPROXY` panim proxies, `NOGO_ElecRoomOP`/`NOGO_SteamOP` persistent blockers, fire entities on `_JANEXTRAFIRE`/`_JANFIRE01/02` (`boilerfire2`, HP 1000, dmg 40), effects `ElectrocuteLRG`, `steam_pipe`, `BoilerSteam`, sound emitter `PlasmaBall`, sound `FlameOut`. Special entities `TombstonePost`, `PumpkinPost`.

**Speech usage (counts only):** 23 distinct scripted `M_1_10` line ids (9, 13, 19, 21, 23-26, 28, 30, 31, 33, 36, 37, 39-44, 47, 49, 50). Music: `MS_CarnivalFunhouseMiner.rsm` (basement), fail end music.

## 5. Framework gaps

| Beat | Covered by | NEW primitive(s) |
|---|---|---|
| H1/H3 follow-Gary escort | none | `system: escort_follow` — NPC path-following with waypoint callbacks (`condition: npc_reached_path_node`), wait-for-player-proximity, ally recruit/dismiss (`action: npc_recruit_ally`) |
| H1 aggro-on-hit | event (partial) | `condition: ped_hit_by` with attacker/victim filters |
| H2 ally-assisted brawl | spawn_enemy (partial) | `condition: all_ko(group)`, KO tracking of a named ped group; `action: npc_attack(target)` |
| Gary fail | none | `condition: npc_ko(target)` + mission-critical flag on spawn (`action: spawn_npc` needs `mission_critical`, `on_attacked` hooks) |
| H4-H6 switch rooms | interact (partial) | `condition: prop_state` (PAnim node Active/Inactive/NotUseable) + `action: prop_set_state`, `action: door_open(stay_open, pathable)` vs simple `door_set_locked` |
| H4-H8 hint cadence | hint (one-shot) | `system: hint_escalation` — repeat hint lines on a 10 s jittered timer while player in room trigger, escalate to a blip after N hints |
| H7 crawl | none | `condition: player_stance(crouch)` + crawl tutorial text |
| H7 arcs / H8 steam & fire | none | `action: hazard_create` (effect list at named points + damage volume + No-Go blocker entity) / `hazard_destroy`; `condition: hazard_hp_below` (fire health); timed re-arm (`event` after delay) |
| H8 extinguisher check | none | `condition: has_weapon(326)` |
| Door micro-NIS ×5 | letterbox + camera + say | reusable composite stage; hard-coded cam coords become per-room JSON |
| H9/H10 pass & audience | start_mission/reward | `action: spawn_npc_group` at points with face-player; pass-with-custom-success-scene flag |
| Basement respawn on fail | none | `action: mission_fail` with respawn point + re-lock list |
| Radar zoom lock | none | `action: radar_zoom(minmax)` / restore (or skip — cosmetic) |
| Health pickups | none | `action: spawn_pickups` (type 502 at point list) |
| Music per segment | none | `action: play_music` / `stop_music` |

## 6. Open questions

- How the Room 01 cage is opened (`MINI_Lock` suggests lockpicking minigame; no Lua reference) and what registers `cbBroomHit` in Room 02 (prop/DAT-side).
- What drop item 362 is, and weapon 359's identity (only seen in 1_E01).
- `MissionSucceed(true, …)` semantics — whether the betrayal scene is an engine cutscene keyed off the mission id (`1-10`-family cutscene files) or driven elsewhere; `1_10Betrayal`'s actual consumer.
- The `bGaryAttacked → pass` branch in H9 (attacking Gary at the hole also passes the mission) — deliberate alternate trigger or script sloppiness; mirror or drop it.
- Timings: hint cadence (10 s + 2-5 s jitter), arc re-arm (4 s), NIS durations are in-script; cutscene `1-10` length unmeasured.
- Whether the three bullies respawn/reset on retry after H2 (stage machine restarts from Stage00; bully spawn is one-shot in Stage00 init — state carryover on retry is engine-side).

## 7. World-data names to extract from `1_10.DAT`

**POINTLIST:** `_1_10_PSTART_MM`, `_1_10_NEMESIS_MM`, `_1_10BULLY01`, `_1_10BULLY02A`, `_1_10BULLY02B`, `_1_10_HEALTHPICKUPS`, `_1_10JANDOORBLIP`, `_1_10_GARYBS`, `_1_10_ELECBLIP`, `_1_10_GARYSWITCHELEC`, `_1_10_HOLEBILIP`, `_1_10_FAILURE`, `_1_10_AUDIENCE` (4), `_1_10_AUDVIEW` (4), `_JANELECPTS` (n), `_JANSTEAMPTS` (n).

**PATH:** `_1_10_TO_PARKINGLOT`, `_1_10_TO_BASEMENT`, `_1_10_ENTERBASEMENT`, `_1_10_ROOM00`, `_1_10_ROOM01`, `_1_10_ROOM02`, `_1_10_ROOM03`, `_1_10_ROOM04`, `_1_10_ROOM05`, `_1_10_ENTERHOLE`.

**TRIGGER:** `_1_10_BULLYTRIGGER`, `_1_10_NEAR_BASEMENTDOOR`, `_1_10_ROOM0`, `_1_10_ROOM1`, `_1_10_ROOM2`, `_1_10_ROOM3`, `_1_10_ROOM4`, `_1_10_INCAGE`, `_1_10_CRAWL_ELEC`, `_1_10_GARY_TURN_OFF_ELEC`, `_1_10_CROUCHHINT`, `_1_10_ATFURNACE`, `_1_10_NEARHOLE`, `_1_10_THEHOLE`, `_AFTERSTEAM`, `_JANELECTRICPROXY`, `_JANSTEAMPROXY`, `_JANSWTCH00`, `_JANSWTCH01`, `_JANSWTCH02`, `_JANSWTCH03A`, `_JANDOORS00`, `_JANDOORS01`, `_JANDOORS02`, `_JANDOORS03B`, `_JANEXTRAFIRE`, `_JANFIRE01`, `_JANFIRE02`, `_ANIBROOM`, `_DT_TSCHOOL_SCHOOLSIDEDOORL`.

**Doors by name:** `DT_TSCHOOL_SCHOOLSIDEDOORL`, `TSCHOOL_SCHOOLSIDEDOORR`, `DT_JANITOR_MAINEXIT`, `DT_JANITOR_SCHOOLEXIT`. **Persistent blockers:** `NOGO_ElecRoomOP` (-762.0, -74.6, 10.8), `NOGO_SteamOP` (-761.2, -62.6, 10.8). **Special entity sets:** `TombstonePost`, `PumpkinPost`.

**Area codes used:** 0 (tschool parking lot), 8 (janitor basement). Hard-coded door-NIS cameras are all in area 8 around (-741..-757, -52..-80, 9-10.5).
