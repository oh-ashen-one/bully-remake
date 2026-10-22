# 1_04 — Slingshot: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_04.lua` (1226 lines) + `SMissPass.lua`, `SFaction.lua`, `main.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_04 = "Slingshot"** — **VERIFIED** by the loaded animation group `"1_04TheSlingshot"` and by content: Gary (ped model 130) leads the player to the school parking lot, has him shoot out the windows of a bus with the slingshot (weapon 303, projectile 304), then to the football field where the player snipes six working-out jocks from a tree while their coach (Burton, model 55) patrols — getting spotted, climbing down too long, or KO'ing the coach fails the mission. Matches the canonical Chapter 1 list entry "Slingshot (8-19)". No surprise.

Setting is entirely outdoors (area 0 = `tschool`): player start `AreaTransitionPoint(0, ...)`, no interior transitions anywhere in the script.

## 2. Mission flow

Driver: `main()` runs a `fCurrentFunc` state function each tick: `F_IntroObjective → F_WaitForMainMap → F_StageOneGetToBus → F_IntroObjectiveMainMap → F_StageTwoShootTheBottles → F_StageThreeGetToField → F_TreeAssault`. Threads: `T_MiscHandler` (text queue), `T_WatchForGaryDeath`, `T_NemBleach` (Gary walking to the bleachers), `T_Treed` (tree-presence watch), `T_JumpingJacks` (jock workout cycle). `F_NISSelect`/`F_RunNIS` are a debug NIS browser, unreachable in the live flow (dead code).

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Baked cutscene "1-04"** | `PlayCutsceneWithLoad("1-04", true)`; `DATLoad("1_04.DAT", 2)`; no fade-in. Retry-skip not explicit in this file — **INFERRED** engine attempt count; **OPEN**. |
| B1 | **Setup** (`F_MissionSetup`, `main`) | Player is **given the slingshot (303) with 30 ammo** up front. Gary (130) spawns at the player's start (`_1_04_PLAYERSTART`) with a health bar, infinite sprint, stimuli ignored, player on his ignore list, marked **mission-critical** (death → fail). Gary gets a blip. Five bus-window breakable objects (`tschool_BusWindow` + `01`–`04`) are registered with a broken-object hash event handler. Bus windows start invulnerable; when the player first enters the parking population trigger, a 1 s timer makes them breakable. |
| B2 | **Follow Gary to the lot** (`F_IntroObjective`) | Fade in, Gary barks a "this way" ambient line, objective 1 added (go with Gary — key `1_04_OBJ01`), blip on `_1_04_NEMESISPARKING`, Gary runs there. Parking/field population suppression triggers activate. |
| B3 | **Parking-lot scene (in-engine, ~10 s)** (`F_IntroObjectiveMainMap` → `F_NIS_Parking1`) | On entering `TRIGGER._1_04_PARKING`: objective 1 completes; peds cleared, control off, widescreen, fixed camera at a hardcoded spot near (181.5, 6.0, 9.6); player teleported to `_1_04_NEMESISPARKING`, Gary placed 1–2 m off the player and gives his pitch (speech id 48); control back, one more Gary line (id 6), camera reset. Objective 2 added (break the bus windows — key `1_04_BOT`), `TutorialStart("Shooting")` arms the shooting tutorial. |
| B4 | **Shoot the bus windows** (`F_StageTwoShootTheBottles`) | Player must break the 5 bus windows. Attribution logic per window: if destroyed by the player, it's credited to the slingshot only if the zone's slingshot-projectile (304) impact count went up since the last break — otherwise marked "wrong weapon". Stage completes when ≥3 windows are broken, or all 5 with ≥3 by the wrong weapon (progress is granted either way). If the player wanders out of `_1_04_BOTTLESHOOTAREA`, the lot blip is re-added and a "get back" text nags (key `1_04_LOT`). A `bFailForWrongWeapon` → `MissionFail(..., "1_04_WRONG_GUN")` path exists but **nothing ever sets the flag** — dead fail path, **VERIFIED**. |
| B5 | **To the field** (`F_StageThreeGetToField`) | Gary line (id 47), Gary runs to `_1_04_PLAYERFIELDPT`, blip on `_1_04_FIELD`, objective 3 (go to the field — key `1_04_OBJ02`), POI system disabled. On `TRIGGER._1_04_FIELDTRIG`: if Gary is >15 m away and out of sight he is teleported to `_1_04_FIELDSPAWN`. Objective completes. |
| B6 | **Get in the tree** (`F_GetInTheTree` / `F_LaunchFieldBullies`) | Gary points at the tree (anim `GaryPointAnim`, speech 51), objective 4 (climb the tree — key `1_04_TREE`), `TutorialStart("Remember")`. Gary walks `PATH._1_04_NEMPATHTOBLEACHERS` and sits at `POI._1_04_NEMSITSPOT` (bleachers) for the rest of the mission. Radar zoom is clamped (40–50). A blip marks `_1_04_TREE1`. "In tree" = player inside `TRIGGER._1_04_TREE1_TRIG` while playing the tree-climb perch node (`/Global/TreeClimb/Actions/ON_BOT`). When the player first perches in `TRIGGER._1_04_TREE2_TRIG`, the field squad spawns. |
| B7 | **The field squad** (`F_CreateBulliesAndBurton`) | Six jocks (models 109, 111, 112, 231, 232, 110) spawn at `_1_04_FIELDSPAWN`, each with 22/22 health, no weapons, infinite sprint, a blip, and flag 58 set; each runs to its own field point then cycles: run `PATH._1_04_RUNPATH0n` ↔ stop at node 1 for a jumping-jacks workout (`/Global/Ambient/Scripted/Workout`, ~3 s). Coach Burton (55) spawns, walks his patrol to `_1_04_BURTONFIELD`, gets a **stealth spotter behavior** (spot → fail callback), and shouts encouragement/warnings at most every 10 s (speech ids 34/38) or immediately when he sees a hit land (id 34). |
| B8 | **Tree assault** (`F_TreeAssault` loop) | High-intensity music while the player is perched with the slingshot equipped. Hits register through a global hit-event handler: a jock counts as hit if the last hit weapon was 304 (slingshot) or the hitter was the player **while the player is in the tree**; hit jocks break out of the workout (coach yells them back after ~1.5 s + 15 s cooldown). Non-fatal hits trigger a "what was that" ambient reaction. When all 6 jocks are down (`totalhit >= 6`): Burton reacts (speech), widescreen, field peds made ambient, `SetFactionRespect(1, 55)` (faction 1 = jocks, **INFERRED** from context), pass banner (`M_PASS` + respect message `MRESPECT_NP5`), pass music, `MissionSucceed(false, false, false)`. |
| B9 | **Tree discipline** (`T_Treed`) | After the squad has spawned, leaving the perch starts a **15 s mission countdown** with a warning text (key `1_04_TREE_WARN`); getting back in the tree cancels it; expiry fails the mission (`1_04_TREE_FAIL`). The tree blip toggles with perch state. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Out of the tree too long | 15 s `MissionTimer` after leaving the perch (post-spawn) | `1_04_TREE_FAIL` |
| Spotted | Burton's stealth behavior fires → in-engine "busted" scene (`F_PlayerSpotted`: control off, camera over the spotter's shoulder at the player, accusation line id 39, ~2 s) → fail | `1_04_SPOTTED` |
| Burton KO'd | death check each tick | `1_04_BURTON_KO` |
| Gary KO'd | mission-critical ped callback | `1_04_KO_GARY` |
| Wrong weapon | declared but unreachable (dead flag) | `1_04_WRONG_GUN` |
| Generic | any other failure | `MissionFail(false, true)` |

All fails play fail music and allow retry; cleanup restores POI, radar range, population triggers, removes blips, releases Gary to ambient. No mid-mission checkpoints — retry restarts at B0/B1. After a non-completion exit, surviving field peds are made ambient and (except Burton) turned hostile to the player.

### Rewards / unlock side-effects

- In-mission: slingshot (303) + 30 ammo granted at start; on pass `SetFactionRespect(1, 55)` and completion banner with respect line.
- `SMissPass.lua`: **no `1_04` entry** — no weapon/outfit/collectible/chapter grant on pass. (Neighbor entries for orientation: `1_03` tops up slingshot 303; `1_07` grants 437 and force-completes `1_E01`; `1_08` tops up ammo 309; `1_09` grants 306 and arms the Halloween chain.)
- `SFaction.lua`: once `1_04` is complete, several factions' **default attitude toward the player (type 13)** shifts (factions 11, 5, 4, 2 → 2; 1 → 3; 6 → 3) — **VERIFIED** as code, exact faction names **INFERRED**.
- `main.lua`: `BulletinSetupMessage("1_04", "BB_02")` — a dorm bulletin-board notice keyed to this mission (setup side, not a reward).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0, before control | baked (`1-04`) | fade in at `_1_04_PLAYERSTART` + letterbox + 1–2 `say` beats of original VO (Gary recruits Jimmy), or straight to B2 |
| CS-2 | B3 parking-lot pitch | in-engine, fixed camera | `letterbox` + `camera` (fixed, hardcoded coords above) + `say` + control off/on — trivially covered |
| CS-3 | B6 Gary points at the tree | in-engine micro-beat | `say` + NPC point anim; no camera cut needed |
| CS-4 | B8 spotted-busted scene | in-engine | `camera` over spotter's shoulder + `letterbox` + `say` + `fade` to fail screen |
| CS-5 | B8 pass beat | in-engine | `letterbox` + completion `banner` + `reward` (respect) |

Cutscene-only anim group: `NIS_1_04`.

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 130 | Gary (guide) | mission-critical; walks to lot, then bleachers POI |
| 55 | Coach Burton (spotter) | stealth behavior; unique-model status forced to -1 during the mission (`PedSetUniqueModelStatus(55, -1)`) |
| 109, 111, 112, 231, 232, 110 | six jock "goons" | 22 HP each, run/workout cycle, per-ped blip |
| 75 | requested in `F_MissionSetup` but never spawned in live Lua | dead request or baked-cutscene-only — **OPEN** |

### Animation groups / action trees

`LoadAnimationGroup`: `1_04TheSlingshot`, `Hang_Workout`, `NIS_1_04`.
Trees referenced: `Act/Conv/1_04.act` (mission conv nodes), `Act/Anim/Ambient.act` (workout node), tree-climb nodes under `/Global/TreeClimb/...`.

### Props / weapons / objects

- Breakables: 5 bus windows (`tschool_BusWindow`, `tschool_BusWindow01`–`04`) with per-object destroyed events and projectile-impact attribution per zone (`_1_04_BOTTLEZONE`).
- Weapons: 303 slingshot (granted, 30 ammo); 304 = slingshot projectile id (**INFERRED** from impact/hit checks).
- Special entities: none loaded (`AreaLoadSpecialEntities` not used here).
- Speech: `M_1_04` scripted ids used: 6, 8, 10, 32, 34, 35, 38, 39, 47, 48, 51 — **11 distinct scripted lines**, plus ambient events (`THIS_WAY`, `FIGHT_WTF`). Music: interactive `MS_FunLow/Mid/High` (high while perched + armed), pass/fail stingers.

### Named world data to extract from `1_04.dat`

- POINTLISTs: `_1_04_PLAYERSTART`, `_1_04_NEMESISPARKING`, `_1_04_PLAYERFIELDPT`, `_1_04_FIELD`, `_1_04_FIELDSPAWN`, `_1_04_GOONFIELDNEW`, `_1_04_BURTONFIELD`, `_1_04_TREE1`.
- PATHs: `_1_04_RUNPATH01`–`_1_04_RUNPATH06`, `_1_04_NEMPATHTOBLEACHERS`.
- TRIGGERs: `_1_04_PARKING`, `_1_04_BOTTLESHOOTAREA`, `_1_04_BOTTLEZONE`, `_1_04_FIELDTRIG`, `_1_04_TREE1_TRIG`, `_1_04_TREE2_TRIG`, `_1_04_NEMSITDOWN`, `_1_04_SUPPRESSFIELDPOP`, `_1_04_SUPPRESSPARKINGPOP` (population).
- POI: `_1_04_NEMSITSPOT`.
- Hardcoded coords: parking-NIS camera (181.48, 6.02, 9.58 → 180.89, 6.74, 9.22); window-reaction distance anchor (175.59, 12.46); Gary perch heading 167.8; Burton patrol heading 146.
- Area codes seen: **0 only**.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| B0 cutscene + retry skip | `fade`, `letterbox` | **cutscene_skip_on_retry** (attempt-count gate) |
| B1 loadout | `reward` | **give_weapon / give_ammo** as mission-start actions (not just pass rewards) |
| B2 follow-NPC lead | `flag`, `event` | **npc_lead_to_point** (NPC runs ahead, waits; re-blip if player strays — the `b_blipped` toggle pattern) |
| B4 window shooting | — | **object_destroyed(object_id, breaker=player, weapon=id)** condition with **count_threshold**; per-zone **projectile_impact_count** for weapon attribution; object vulnerability toggle |
| B6 climb/perch | `trigger_enter` | **player_action_state** condition (player playing named node, e.g. tree perch); **npc_sit_at_poi**; radar zoom clamp (**radar_set_range**) |
| B7 squad behavior | `spawn_enemy` | **npc_patrol_cycle** (run path ↔ timed activity at node, per-NPC state machine); **stealth_spotter** behavior with **spotted_by(npc)** condition; **npc_bark_timer** (min-interval ambient lines, react-on-event override) |
| B8 sniping objective | `event` | **ped_hit_event(victim_tag, weapon, requires player_action_state)**; **all_down(tag, n)** count condition; per-target blips (**blip_on_npc**) |
| B9 tree timer | `timer` | **zone_exit_countdown** (leaving a trigger starts cancelable countdown + warning text; expiry fails) — distinct from plain `timer` |
| fails | `event` | **mission_critical_death(npc)** fail (also needed by 1_02); **spotted fail scene** primitive |
| B8 pass | `reward`, `banner` | **set_faction_respect**, completion banner variant with respect line |

## 6. Open questions

- Burton's stealth parameters (`PedSetStealthBehavior(idBurton, 0, cbSpotted)`): vision cone, range, and whether jocks can also spot (script wires only Burton; the goons' hits set `bBurtonSeesHit` for barks, not spotting) — spotter tuning must be authored.
- Why hits must come "from the tree" only via the `b_InTree` gate in `F_CheckHit` while the death counter in `F_CheckBullyAttacked` counts any death — whether a jock killed on the ground also progresses is **OPEN** (code suggests yes, via `PedGetWhoHitMeLast == gPlayer`).
- The dead `bFailForWrongWeapon`/`1_04_WRONG_GUN` path and dead `bWarnDontKOBullies` — intended design or cut feature; we can safely drop or implement as soft nag.
- Tree-climb mechanics (`/Global/TreeClimb` nodes, `HoistUp_Spawns`) — climbing system itself is an engine feature we don't have; needs its own primitive/traversal work.
- Model 75's role (requested, never spawned).
- How the mission is offered in the world and what unlocks 1_05 next — mission table outside the Lua.
