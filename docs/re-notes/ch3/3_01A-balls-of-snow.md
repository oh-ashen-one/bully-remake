# 3_01A — Balls of Snow: recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_01A.lua` (654 lines, complete). `SMissPass.lua` has **no** 3_01A case (**VERIFIED** by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_01A = "Balls of Snow"** (canonical Chapter 3 list, #28, 8AM–7PM; Scholarship-Edition-exclusive Christmas mission, available from chapter start) — **VERIFIED** by content against the [Bully wiki page](https://bully.fandom.com/wiki/Balls_of_Snow): Rudy the drunken hobo-Santa (model 252) has the player climb onto a building and throw snowballs at **townies** walking and biking past in New Coventry; Rudy cheers each hit; 10 hits complete it; "if Jimmy hits any adults with snowballs, the mission fails — the police will apprehend Rudy when they show up" — exactly this script's heat-mirror/cop-bust mechanic (R3c). The wiki's victim cast (townies Clint/Edgar/Otto/Jerry/Leon) matches the victim model pool 41/42/43/45/91; a completion guide credits the **Reindeer Antlers** reward to Balls of Snow, matching `ClothingGivePlayer("SP_Antlers", 0)`. First draft of this doc mis-filed it as "Rudy the Red Nosed Santa" — that mission is **3_01D** (ornaments + Santa's-lap photos), corrected in the same pass.

Setting: New Coventry (wiki intro), town area 0. File-name note: the script's debug prints and all its DAT point names call it **3_10** (`_3_10_*`) — internal id vs filename mismatch, worth knowing when grepping. Shares only the id prefix with 3_01 (Jealous Johnny); unrelated content.

No references to "3_01A" exist anywhere else in the Lua tree (**VERIFIED** by grep) — the offering mechanism (presumably the data-driven mission table, post-`3_XM`-family Christmas unlock) lives outside the scripts — **OPEN**, same as all offering windows.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → a `tblMissions` function chain (`F_InitMission → F_StartMission → F_MissionEnd`) indexed by `F_SetNextMission` — a hard-coded three-stage sequencer.

| # | Beat | Logic (functional) |
|---|---|---|
| R0 | **Baked cutscene "3-01AA"** | `PlayCutsceneWithLoad("3-01AA", true)` in `MissionSetup`; player placed at `_3_10_PLAYERSCENE01` (area transition if not in area 0). `DATLoad("3_01A.DAT", 2)`. No attempt-count check — retry skip **OPEN**. |
| R1 | **Setup** (`main`) | Population overridden to near-empty townies (20 of type index 0 + 15 of index 10 — category decode **OPEN**), ambient peds ignore stimuli, POI off, all vehicles cleared. Player dismounted and disarmed. Rudy (252) created at `_3_10_RUDYSCENE01`, stationary, stimuli-ignoring, mission-critical (fail cb), targetable. Controls killed (widescreen, clock paused, punishment system disabled, player untargetable). |
| R2 | **Stage 1: climb to the stand** (`F_InitMission`) | Christmas music starts. Objective `3_01A_MOBJ_01` + blip on `_3_10_LADDER01`. Advance when the player plays the ladder climb-on node (`/Global/Ladder/Ladder_Actions/Climb_ON_BOT` — i.e. started climbing the ladder, **VERIFIED** anim-name-based detection); Rudy then walks to `_3_10_RUDYMOVE01`. Then blip `_3_10_MISSION01`; advance when the player stands there. Throughout: the Rudy-distance watchdog (below). |
| R3 | **Stage 2: snowball gallery** (`F_StartMission`) | Player teleported to the stand point and put into **snowball cam**: player invulnerable + untargetable, action tree swapped to `/Global/3_01A/SnowBall`, scripted snowball equip/unequip nodes, camera preset 13 locked in, music forced to the high-intensity layer, a hit-event armed on the player (`cbResetPlayer` — on any hit taken, the mode is exited and re-entered after 2 s, control briefly cut). Counter icon `snowface`, max **10**; mission timer **180 s**. Objective text with a live "remaining" param (`MissionObjectiveUpdateParam`). The [StrategyWiki walkthrough](https://strategywiki.org/wiki/Bully/Chapter_3:_Love_Makes_The_World_Go_Round) describes exactly this shape — climb the ladder, "run into the glowing circle by the pile of snow", auto-switch to aiming view, hit ten townies; hitting adults raises the wanted level (which is what gets Rudy apprehended — see R3c); reward $20 + Reindeer Antlers hat. |
| R3a | **Victims** (`F_SetVictim` ×10 slots) | Ten scripted targets from model pool 41/42/43/45/91 (random per slot), three movement types: type 0 = spawn and walk to a point; type 1 = follow `_3_10_PATH01/02`; type 2 = spawn a random bike (272/273/274/281) at `_3_10_BIKE01/02` and ride `_3_10_BIKEPATH01` (second rider starts at node 3). Each blipped; ignore-stimuli/ignore-attacks while unhit. |
| R3b | **Hit confirm** (`F_CheckVicims` thread) | A victim counts only if `PedGetWhoHitMeLast == gPlayer` **and** `PedGetLastHitWeapon == 313` (snowball-only, **VERIFIED** weapon-filtered hit check). On a count: blip cleared, victim unfrozen, made ambient, flees the player; counter +1; Rudy cheers (line 10). Each hit slot is refilled once (`F_SetVictim` re-called on hit while VictimCounter < 10) — so exactly 10 targets exist; missing is not punished, but all 10 must be hit before the timer runs out. |
| R3c | **Heat transfer + bust** | Every frame the player's punishment points are **mirrored onto Rudy** (`PedSetPunishmentPoints(gRudy, PlayerGetPunishmentPoints())`). At ≥ 300 points a cop (97) spawns at `_3_10_BIKER01` and runs at Rudy; if the cop closes to < 4 m → `bRudyBusted` → fail `3_01A_FAIL_01` (Rudy takes the fall for the player's trouble — the mission's real difficulty knob). Rudy also mirrors the *player's* visible heat: hitting people raises it; snowball hits on scripted victims presumably generate little/no heat, wild shots at ambient peds do (**INFERRED** — heat sources are engine-side). |
| R3d | **Ammo nag** | If the player stands at the stand without weapon 313 equipped: re-equip node replayed + Rudy boo line (11, 20 s cooldown). Rudy's generic harping line (9) every 15 s otherwise. |
| R4 | **Stage 3: pass** (`F_MissionEnd`) | Controls killed, punishment cleared and system disabled, vehicles cleared, Rudy deleted. Baked outro **"3-01AB"**. Player warped to `_3_10_PLAYERWARP`. `MinigameSetCompletion("M_PASS", true, 2000)` ($20 **INFERRED**), player success bark, pass music, then **`ClothingGivePlayer("SP_Antlers", 0)`** (antler headgear, **VERIFIED**) and a tutorial/completion tag `TUT_301A` on a blank banner. `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Cop reaches Rudy | R3c heat mirror | `3_01A_FAIL_01` |
| 180 s timer expires before 10 hits | `BOSTimer` wrapper on `MissionTimerStart` | `3_01A_FAIL_02` |
| Player strays > 50 m from Rudy | 30 s grace with tutorial warning first | `3_01A_FAIL_03` |
| Rudy hit by player | mission-critical cb → same bust path | `3_01A_FAIL_01` |

Retry: `MissionFail(false, true, msg)` — retry allowed, no fade, no checkpoints; replay from R0.

### Pass effects

In-script only (no SMissPass case): $20 banner, **SP_Antlers** clothing grant (walkthrough-listed as the Reindeer Antlers hat — **VERIFIED** cross-source), `TUT_301A` completion tag. No respect changes, no ChapterSet, no weapon unlock.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | R0 intro | baked (`3-01AA`) | `fade` + `letterbox` + `say` beat at `_3_10_PLAYERSCENE01` |
| CS-2 | R4 outro | baked (`3-01AB`) | `fade` + short original beat at `_3_10_PLAYERWARP` |
| CS-3 | R4 pass banner | engine UI | `completion_banner` + clothing-grant toast |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 252 | Rudy | mission-critical; walks to his move point after the ladder; heat-mirrored |
| 41, 42, 43, 45, 91 | gallery victims ×10 | three movement types (walk-to-point / path / bike path) |
| 97 | cop | spawns at 300 mirrored heat; reaching Rudy fails |
| 272, 273, 274, 281 | victim bikes | random per bike-type victim |

### Animation groups / action trees

`LoadAnimationGroup("MINI_BallToss")`; action tree `Act/Conv/3_01a.act` with `/Global/3_01A/SnowBall` player tree (`GiveBall/GiveBall`, `RemoveBall` nodes). Ladder detection via the shared `/Global/Ladder/.../Climb_ON_BOT` node.

### Props / pickups / weapons

- Weapon **313 = snowball** (**INFERRED** — Christmas context + `MINI_BallToss`; name not in-file).
- No pickups, no props.

### Speech

`M_3_01A` scripted ids: 9 (Rudy harping loop), 10 (cheer on hit), 11 (boo when player unequipped) — **3 distinct lines**. Music: `MS_XmasBellsRudy` low/mid/high; pass/fail stingers.

### Named world data to extract from `3_01A.dat`

- POINTLISTs: `_3_10_PLAYERSCENE01`, `_3_10_LADDER01`, `_3_10_RUDYMOVE01`, `_3_10_MISSION01`, `_3_10_MISSION01FIRE`, `_3_10_PLAYERWARP`, `_3_10_RUDYSCENE01`, `_3_10_SSSTART01/02` + `_3_10_SSMOVE01/02`, `_3_10_SBSTART01/02`, `_3_10_BIKER01/02`, `_3_10_BIKE01/02`, `_3_10_RudyMove01` (case-inconsistent duplicate name — **VERIFIED**, cited as-is).
- PATHs: `_3_10_PATH01`, `_3_10_PATH02`, `_3_10_BIKEPATH01`.
- TRIGGERs: none named in the Lua (the mission is pure points/paths + ladder anim detection) — trigger usage **unlikely but unproven** until the DAT is extracted.
- Area codes seen: **0 only**. DAT referenced as `3_01A.DAT` — present in original assets, not in the Lua-only clone.

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `completion_banner`, `give_clothing` (ch2 2_03), `collect_counter` (snowface variant), `countdown_fail_timer` (BOSTimer = Timer condition + HUD countdown), `population_override`, `poi_enable`, `spawn_npc` flag bundles, `npc_follow_path`, `npc_ride_vehicle_path` (ch2 2_05 — bike victims), `flee_loop`-adjacent (`PedFlee` on hit victims), `npc_bark_timer` (Rudy loops), `traverse_interact` (ladder stage gate is close: it detects the climb-on anim rather than an interact prompt), `unique` heat concept via `heat_set` (schema) — but see below. Schema conditions `Timer`/`Reach` cover the timers and stand-here checks.

| Beat | Covered by | New primitives needed |
|---|---|---|
| R3 snowball cam | `vehicle_camera_preset`-adjacent, `weapon_strip`-adjacent | **fixed_gallery_mode**: enter/exit wrapper — lock player to a point, swap player action tree, scripted weapon equip/unequip nodes, camera preset locked, invulnerable+untargetable, on-hit reset (exit/re-enter with control cut) |
| R3b snowball-only hits | `ped_hit_event` (who-hit-me-last) | **ped_hit_with_weapon(weapon_id)** condition variant (last-hit-weapon filter) |
| R3c heat mirror + bust | `heat_set`, `spotted_by_authority` | **heat_mirror_to_npc** (NPC's punishment points track the player's each frame) + **authority_reach_npc_fail** (spawn cop at heat threshold; cop within r of NPC → fail) |
| R2/R3d Rudy loops | `npc_bark_timer` | nothing new (cheer-on-event and unequipped-boo are event barks on the same timer) |
| victim table | `spawn_npc`, `npc_follow_path`, `npc_ride_vehicle_path` | **target_gallery_spawner**: fixed-size spawn table with per-slot movement type (walk/path/bike), blip, one-hit-latch, refill-while-slots-remain — could be config on spawn_npc + a latch counter |
| >50 m abandon | `escort_separation_reset`-adjacent | **npc_distance_fail** (warn tutorial + grace timer, then fail) — escort variant without the reset |
| R1 controls kill | `frontend_ui_lock` | nothing new, but note the bundle: clock pause + punishment disable + untargetable + widescreen is a reusable **cinematic_lock** flag set |

Genuinely NEW names introduced here: **fixed_gallery_mode**, **ped_hit_with_weapon**, **heat_mirror_to_npc**, **authority_reach_npc_fail**, **target_gallery_spawner**, **npc_distance_fail**.

## 6. Open questions

- Whether ambient (non-scripted) peds can be snowballed for heat without counting — heat economy is engine-side (**INFERRED** that only non-victims raise trouble meaningfully).
- Punishment-point decay during the 180 s (engine default, not scripted) — **OPEN**.
- `MissionObjectiveUpdateParam` live-count rendering — HUD support needed; trivial.
- `_3_10_MISSION01FIRE` / `tblMissionParams[2]` ("3_01A_TEXT_01") are loaded into the params table but never used — vestigial second beat (**VERIFIED** dead in-file).
- The case-inconsistent `_3_10_RudyMove01` vs `_3_10_RUDYMOVE01` — whether the DAT carries both or the cop walk target silently fails (**OPEN** until extraction; likely the same point, engine name lookup case rules unknown).
- Retry cutscene skip (no attempt check) — **OPEN**.
- Victim model pool includes 91 (a woman/teacher-type?) — exact cast **OPEN** until `default.ide` cross-check.
