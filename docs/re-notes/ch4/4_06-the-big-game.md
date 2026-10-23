# 4_06 — The Big Game: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_06.lua` (2715 lines, complete) + the shared `CowDance.lua` reference (dance mechanic lives there; this mission hooks it via `shared.forceCowDanceEnd` and task queries). No `SMissPass.lua` branch exists for `4_06` (**VERIFIED**, SMissPass read) — pass effects are in-file. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_06 = "The Big Game"** (canonical Chapter 4 list, mission #52, 8AM–9PM, chapter-boss lead-in vs. the Jocks) — **VERIFIED** by the loaded animation group `"4_06BIGGAME"`, the dedicated speech bank `MISSION\4_06.bnk`, and by content: wearing the mascot costume won in 4_05, the player runs four order-free prank operations for the nerds (lace the team cooler, plant a rigged ball, glue the bleacher benches, marble the practice field) plus a finale (hack the scoreboard) while using the mascot dance to distract patrolling jocks. Cross-checked against the [Bully fandom wiki Chapter IV page](https://bully.fandom.com/wiki/Chapter_IV) ("The Big Game 4-06"). Catalog cross-check: `docs/story-mode-catalog.md` #52.

Dead/loose ends in the file: `bCoachDistracted` is initialized `true` and never set `false` — the scoreboard-hack wait loop (`while not bCoachDistracted`) is inert, so the intended "distract Coach Burton away from the press box first" gate never engages (**INFERRED** cut or broken); the `gLostJocks >= 2` completion guard in Op E reads a variable that is only ever set to 0 (dead check); `Frisbee` (pickup 335 at `_4_06_OBJECTIVE_C` element 4) is created but never referenced again (**INFERRED** vestige of the ball-drop mechanic); `cbPlayerKneel`-style helpers from 4_05 don't appear; `gOutsidePoint`/`gInsidePoint`/`gFlagrantChecks` are fixed 20-slot tables fed by `F_InsertIntoTable`.

Retry/resume semantics worth noting: the per-operation completion flags are `shared.g4_06Op{A,C,D,E,F}complete` — they survive **scene transitions** (the dossier hub is rebuilt whenever the player leaves and re-enters area 0, `F_StripDossiers` → `F_Init_Dossiers`), but `MissionCleanup` resets them all to `false`, so a failed mission replays from zero operations (**VERIFIED** in-file). `shared.g4_06GateTutComplete` is reset in cleanup and never set `true` in this file — the gate tutorial plays on every attempt; the true-setter is external (presumably `CowDance.lua` or an area script, **OPEN**).

## 2. Mission flow

Driver: `main()` → `F_MissionSetUp()` → `F_GateNerd()` (one-time dance gate) → `F_ChooseYourOwnAdventure()` (free-order hub for ops A/C/D/E) → `F_OperationF()` (finale) → pass block. Long-lived threads: `T_FieldOps` (suspicion/distraction engine + field population gate), `T_DeletePeds` (docking despawn), `T_CheckToAttack` (flagrant-witness attacks). No checkpoints.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup + baked cutscene "4-06"** | Clock paused for the whole mission; wardrobe session: `ClothingBackup`, if the player is already wearing Mascot it's swapped to `MascotNoHead` for the intro, then forced into `Mascot` after; `shared.lockClothingManager = true`; `shared.bFootBallFieldEnabled = false` (field population trigger `_4_06_LOAD_FIELD` deactivated at cleanup); the "good" scoreboard LED persistent entity `SC_JocksLEDgood` is created. Attitude matrix: types 1/5/2/4/9 vs player-type 13 set to 4/1/2/1/0 (slot semantics **OPEN**, same standing question as 3_04/4_04). `PlayCutsceneWithLoad("4-06", true)`; `MS_MisbehavingLow.rsm` loops; weapon models 335/331/400/349 requested. Area peds cleared, POI disabled, player disarmed, transitioned to `_4_06_PLAYER_START`. |
| B1 | **Gate tutorial: dance for Thad** | The observatory gate `_SCGATE_OBSERVATORY` is closed + locked; GateNerd (Thad, model 7) waits at `_4_06_GATENERD`, mission-critical with the aggression-fail callback. Tutorial card `TUT_BULL1`. The player must perform the mascot dance inside `_4_06_OUTSIDEGATE`: the script watches the CowDance anim set (idle variants + the four button dances). A **failed** dance → reject bark, brief cooldown, retry allowed. A **successful** dance → short in-engine scene: Thad walks `PATH._4_06_GATEPATH`, plays a picklock anim, the gate unlocks and opens, congrats bark, he goes ambient. Skipped on `shared.g4_06GateTutComplete` (gate just opens; a `WRESTCHANGE` tutorial starts instead — odd pairing, **OPEN**). |
| B2 | **Dossier hub (free order)** | Four nerd givers spawn at dossier points: Algie (4, `_4_06_DOSSIER_A`), Bucky (8, `_DOSSIER_C`), Fatty (5, `_DOSSIER_D`), Melvin (6, `_DOSSIER_E`) — each mission-critical (attacking any → fail `4_06_AGRSV`), blip icon 17, social override set 18 (`Mission/4_06Greeting.act`), and most social actions against them disabled (`F_Socialize` blacklists ids 23–36). Each giver: player inside a 6 m ring → greeting anim + wait; player inside the 2–3 m activation ring → that operation starts; player leaving the 7 m ring first → bail. Completed givers don't respawn. After each op, a progress hint prints (three variants by ops remaining). |
| B3 | **Op A — lace the cooler (Algie; gym/pool, area 13)** | Intro NIS (`F_OPA_NIS(INTRO)`): control off, widescreen, bike auto-parked + nearby bikes purged, mutual face/target-lock, Algie's three beats + one player line (ids 1–4), fades; cooler prop `_4_06_COOLER` created; player repositioned. Gameplay: enter area 13 (gym) → non-mission ped generation off, op-1 guard set spawns (gym patrol 13/12 + cheerleaders 14/180/181 in `Cheerleading` ambient anims) → reach the pool (`_4_06_GATOBJECTIVE`) → blip on the cooler → use it (prop anim flips to `GatClr/NotUseable`) → `LacedDrink`, a `PCafGat_piss` persistent entity appears (the visual gag, **INFERRED**) and `gPlayerFlagrant` is raised during the use anim. Objective to get out: pool/gym doors lock behind until the player reaches either exit point; outro NIS at the field: three jocks, one drinks and pukes (`4_06_Wizz/Puke`), the other two laugh him off (ids 90/92/94 + cruel-laugh bank); doors unlock, player transitioned outside, population re-enabled. Op A flag set. |
| B4 | **Op C — plant the rigged ball (Bucky)** | Intro NIS: Bucky hands over via a `GiveWftbomb` gift anim (ids 32–33); the player receives **weapon 400 (the ball)**; duffle-bag prop created (`DuffBag` act). A decoy runner (232) spawns when the player nears the bag zone and paths along `_4_06_DUFFELGUY` playing a "planting" anim (script variables call this the dog beat — `gPlayerNearDog`, `GuardDog/Planting`; whatever the prop is, its id comes from the DAT, **OPEN**). **Ball-loss watchdog**: if the player stops having weapon 400 while away from the bag → objective swap + **6 s recovery timer** → fail `4_06_OPC_03`; reacquiring the ball cancels. At the bag with the ball: a scripted kneel/rummage sequence (`RummageInDuffel`), ball destroyed, rigged ball planted. Outro NIS: two jocks play catch with the rigged ball (`PedPassBall` after a stat override), it goes off (ids 96/80/23), fade out. Op C flag set. |
| B5 | **Op D — glue the benches (Fatty)** | Intro NIS: `GiveSuperGlue` gift anim (ids 20–21). Eight bench PAnims created (4 seats + 4 paired halves); bleacher guard set (232/109/20) plays workout loops. On the field: radar zooms in (`RadarSetMinMax(30, 60, 30)`), four bench blips. Per bench: stand in a 0.95 m ring → the `GlueBench` use anim plays (flagrant window while it does) → the bench pair flips to used/unusable + non-targetable, blip clears, progress text. All four → outro NIS: a jock sits and sticks (`4_06_Glue/Sit_Start`), two teammates crack up (ids 96/91/98/97). Op D flag set. |
| B6 | **Op E — marble the field (Melvin)** | Intro NIS: `GiveBagmrbls` gift anim (ids 38/39/41); player given **weapon 349 (marbles), 5 ammo**. Three field zones `_4_06_FIELD01/02/03` blipped; first entry shows tutorial `TUT_MARBX1`. A zone is done when `ObjectTypeIsInTrigger(349, zone) > 0` — one marble spill per zone (the variable is misnamed `gTackedJocks`; it counts zones). **Ammo watchdog**: at 0 marbles for 2 s → refill objective + a permanent 349 pickup + blip back at dossier D; picking it up restores 5 and the field blips return. Three zones → outro NIS: three jocks (232) sprint drills and slip in sequence (`HIT_LEG_FRONT_L/R_FALL` variants, id 99; Marbles sound bank loaded/unloaded around it). Op E flag set. |
| B7 | **Op F — hack the scoreboard (Cornelius; finale)** | Unlocks only when A/C/D/E are done (`F_CheckAllMissions` → `AllDone`); Cornelius (9) waits at `_DOSSIER_F`; weather forced (`WeatherSet(1)` — id semantics **OPEN**). Intro NIS (ids 22–26), then the hack-switch prop `_4_06_HACK_SWITCH` (`CtrlBx` act) is created; field guard set 5 + Coach Burton spawn. Reaching the press-box point and using the switch → hacked: success stream, a looping panel SFX, widescreen, camera runs `PATH._4_06_SCOREBOARD_PATH` at fixed speed, the "good" LED entity is deleted and `SC_JocksLEDbad` appears. (The Burton-distraction gate is dead — see §1.) |
| B8 | **Pass** | After op F: **jocks −10 respect**, banner `M_PASS` (2500 ms) + `MRESPECT_JM10`, pass stinger 10, `MissionSucceed(true, false, false)`. Cleanup: shared op flags reset, clock unpaused, wardrobe restored (`ClothingRestore` + rebuild, manager unlocked), field population trigger reactivated, LED entities removed, weather released, gym/pool doors unlocked if the player is in area 13, player moved to `_4_06_MISSIONFAIL` only if the mission ended via `MissionOver`. |

### The dance-distract stealth engine (`T_FieldOps`, runs all mission)

- Entering `_4_06_LOAD_FIELD` zeroes the population and spawns the context guard sets; leaving reverts population and ambient-izes them (`F_FieldDelete`). Guard sets 1–5 are per-operation; set 6 (initial guards 20/16/17 + lap runners 231/112 on `PATH._4_06_JOCKLAP`) accompanies any non-finale op.
- Guards are stealth peds (`PedSetStealthBehavior(0, CbSeePlayer)`). On seeing the player: stop, break anim, their **group** turns to face the player, a see-you bark (per-guard speech ids from `gSpeechTable`), the dance tutorial re-shows, and a **12 s suspicion timer** starts per guard. While the player is dancing (`CowDance` task check on the guard or player), the timer resets. Expiry → the guard attacks (bad-dance bark).
- A **successful** dance near a watcher (`CowDanceSuccess` task or player stimulus type 24) distracts him permanently: good-dance bark, he walks to a docking point (inside `_4_06_INSIDEDOCKINGPOINT` if the player is in the pool area, else `_4_06_DOCKINGPOINT`) and `T_DeletePeds` deletes him when he enters the dock trigger. During op F, distracting group 2/3 guards also starts a 4 s timer that chases Burton off (`gPedsFled` → Burton breaks, walks to the inside dock, despawned).
- **Flagrant acts**: `gPlayerFlagrant` is raised while a prank use-anim plays (cooler, benches) or in the cooler room; any currently-watching guard with line of sight during the window attacks immediately (`T_CheckToAttack`), with a disgust bark variant indoors.
- Out-of-bounds: `_4_06_OOB01-03` exit handlers print a warning and start a **10 s** countdown; `_4_06_BIB01-03` cancel it. Expiry → fail `4_06_LEFT`.

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Out of bounds 10 s | OOB/BIB trigger pair + `MissionTimer` polled in `T_FieldOps` | `4_06_LEFT` |
| Ball lost > 6 s (Op C) | weapon-loss watchdog in `F_PlantRiggedBall` | `4_06_OPC_03` |
| Player attacks a mission-critical nerd (givers, gate nerd) | `PedSetMissionCritical` aggression callback `CbPlayerAggressed` | `4_06_AGRSV` |
| Any other failure | `MissionOver` generic | `MissionFail()` |

Fail music stinger 10; all fails go through `MissionOver` (which also sets `shared.forceCowDanceEnd = true` to kill any running dance). Full replay; op flags reset on cleanup (**VERIFIED**), gate tutorial replays (its skip flag is never set in-file, **OPEN**).

### Pass-side effects

- In-file only: jocks −10 respect, banner + respect message, stinger 10. No outfit/weapon/class grants (the mascot outfit came from 4_05).
- No `SMissPass.lua` branch (**VERIFIED**). Chapter advance to 5 belongs to `5_01`'s SMissPass branch, not this mission.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0 intro | baked (`4-06`) | `fade` + `letterbox` + one `say` beat; outfit force = `give_clothing` |
| CS-2 | B1 gate dance | in-engine micro-scene | `camera` + `npc_follow_path` + `ped_action_node` (picklock) + `door_prop_control`; the dance itself = **dance_distract_stealth** tutorial mode |
| CS-3 | B2 giver greets | in-engine, per giver | `social_override_set` greeting + **quest_giver_hub** approach/activate/bail rings |
| CS-4 | B3–B7 op intro NIS ×5 | in-engine, ~10–20 s each | `inline_cutscene_chain`: mutual face/target-lock + `speech_sequence` + `give_item_scene` (2_08) gift anims + bike park/purge (`vehicle_park_dismount` 2_R03 + `vehicle_area_purge`) |
| CS-5 | B3–B6 op outro NIS ×4 | in-engine, ~10–15 s each | `inline_cutscene_chain` with spawned outro actors + `ped_action_node` gag beats (puke / ball pass / glue sit / marble slips) |
| CS-6 | B7 scoreboard reveal | in-engine, ~8 s | `camera` path variant: one-shot `CameraSetPath` + `CameraSetSpeed` pan (cite `camera_path_cycle` 3_G3) + **world_entity_swap** for the LED board |

Anim groups: `4_06BIGGAME`, `GEN_SOCIAL`, `NPC_CHEERING`, `PX_TLET`, `NPC_MASCOT`, `POI_ChLead`, `NIS_4_06`, `MINI_Lock` (gate), `W_PRANK`, `Player_VTired`, `MINI_React`, `Hang_Workout`. Gift trees: `GiveWftbomb`, `GiveSuperGlue`, `GiveBagmrbls`. Music: `MS_Misbehaving{Low,High,High_NIS01-03,_NISPrankInfo,_NISPrankSucess}.rsm`; stinger id 10 both ways; `PanelLoop` 2D SFX + `weapons\Marbles.bnk` for the marble gag.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 4 / 8 / 5 / 6 | Algie / Bucky / Fatty / Melvin (dossier givers) | mission-critical (aggression fail), social override 18, social-action blacklist; wander off ambient after their op |
| 9 | Cornelius (finale giver) | same bundle; waits at `_DOSSIER_F` |
| 7 | Thad (gate nerd) | mission-critical; picklock unlock beat |
| 55 | Coach Burton | op-F stealth ped; 8 s-throttled bark (id 42); chase-off + dock-despawn beat (gate dead, see §1/§6) |
| 13, 12, 14, 180, 181 | op-A gym guards + cheerleaders | stealth guards; cheerleaders play ambient cheer anims |
| 15, 180 | op-C ball guards | stealth guards |
| 232, 109, 20 | op-D bleacher guards | workout anims |
| 182, 14, 180 / 232, 111, 109 / 231, 112 | op-E field cheer group / runner group / lap group | stealth guards; groups 1–3 |
| 20, 16, 17 + 231, 112 | shared set 6 (initial guards + lap runners) | spawn on any field visit outside op F |
| 232 | decoy runner (op C) | paths to a point playing a planting anim, then deleted |
| 13, 12, 14 / 15, 13 / 20, 109, 232 / 232 ×3 | outro NIS actors (ops A/C/D/E) | spawned per-outro, deleted after |

Weapons/pickups: 400 (rigged ball, given + destroyed by script), 349 (marbles, 5-ammo grants + refill pickup), 335 (frisbee pickup, vestigial), 341 (cooler-op model load, **OPEN**), 331 (requested, use not found in-file, **OPEN**). Persistent entities: `SC_JocksLEDgood`/`SC_JocksLEDbad` (scoreboard), `PCaf_GatCooler` (area 13 cooler), `PCafGat_piss` (gag), all via `CreatePersistentEntity`/`DeletePersistentEntity`.

### Named world data to extract from `4_06.DAT` (present — `DATLoad("4_06.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_4_06_PLAYER_START`, `_4_06_GATENERD`, `_4_06_DOSSIER_A/C/D/E/F`, `_4_06_OBJECTIVE_A/C/D/E/F` (multi-element: D = 4 benches, E/C variants), `_4_06_BIKES`, `_4_06_MISSIONFAIL`, `_4_06_OPAOUTSIDE`, `_4_06_OPAEND_PED/_PLAYER`, `_4_06_OPCEND_PLAYER`, `_4_06_OPDENDPED`, `_4_06_OPEND_PLAYER`, `_4_06_OPEENDPED`, `_4_06_JOCKSPAWN`, `_4_06_MOVETOJOCK`, `_4_06_BALLGUARDS`, `_4_06_BALL_GUARD`, `_4_06_COACH_BURTON`, `_4_06_GYM_PATROL`, `_4_06_GYM_CHEERLEADER`, `_4_06_BLEACHER_GUARDS`, `_4_06_FIELD_WORKER`, `_4_06_FIELD_RUNNERS`, `_4_06_INITIAL_GUARDS`, `_4_06_DOCKINGPOINT`, `_4_06_INSIDEDOCKINGPOINT`, `_4_06_FIELDBLIPS` (3).
- PATHs: `_4_06_GATEPATH`, `_4_06_LONGRUNNER01/02/03`, `_4_06_JOCKLAP`, `_4_06_DUFFELGUY`, `_4_06_OPAEND_PATHA`, `_4_06_SCOREBOARD_PATH`.
- TRIGGERs: `_4_06_OOB01-03`, `_4_06_BIB01-03`, `_4_06_OUTSIDEGATE`, `_4_06_LOAD_FIELD`, `_4_06_GATOBJECTIVE`, `_4_06_FIELD01/02/03`, `_4_06_BENCH_01-04` + `_01A-04A`, `_4_06_DOCKINGTRIGGER`, `_4_06_INSIDEDOCKINGTRIGGER`, `_4_06_COOLER`, `_4_06_DUFFLE_BAG`, `_4_06_HACK_SWITCH`, `_SCGATE_OBSERVATORY`.
- Hardcoded coords in-file: gate camera, op A/C/D/E NIS camera pairs, outro player placements (incl. a raw `(0.2, −40.7, 1.1)` + heading 102.7 reset), scoreboard camera path anchor, `AreaSetPathableInRadius` nav lock at the field gate, cooler/pee/scoreboard entity placements.
- Area codes seen: **0** (field/school) and **13** (gym/pool interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1/ch2/ch3 set | New primitives needed |
|---|---|---|
| B0 wardrobe session | `temp_outfit_swap` (3_01A), `give_clothing`, `player_wearing` (2_10?) | **outfit_session_lock** (`ClothingBackup` → forced outfit → `shared.lockClothingManager` (wardrobe UI locked) → `ClothingRestore`+rebuild on cleanup; includes the "already wearing it → use the headless variant for the intro" conditional) |
| B0 clock/field gates | `clock pause` (schema), `population_trigger_toggle` (3_01C?), `poi_enable` | none new |
| B1/B2 dance gate + giver hub | `social_interact` (1_02B), `social_override_set` (2_06), `mission_critical_npc` | **quest_giver_hub** (N free-order giver NPCs: approach ring plays a greet + waits, inner ring activates that giver's operation, outer-ring exit bails; completed givers never respawn; progress-count hint variants; giver KO/aggression = keyed fail) |
| B1 + engine | `stealth_spotter` (1_04), `spotted_pause_resume` (3_01), `npc_sees_player` | **dance_distract_stealth** (full CowDance loop: guards with a see-player callback; per-guard suspicion timer (12 s) that resets while the player dances and escalates to attack on expiry; successful dance permanently distracts the watcher — bark, walk to a dock point, despawn on dock-trigger enter; failed dance = bark only; plus the one-time tutorial gate where a dance unlocks a door) |
| B3–B6 prank use-anims | `prop_interact` (1_G1), `prop_set_state`, `traverse_interact` | **prop_used_poll** (mission detects "player used the prop" by polling the prop's anim state flip to its `NotUseable` node, rather than a callback; paired with `PAnimMakeTargetable(false)` lockout — a polling variant of `prop_interact`) |
| B3/B5 flagrant window | `npc_witness_event` (2_05?) | **crime_window_witness** (a mission flag is raised for the duration of a prank use-anim; any currently-alerted guard with LOS during the window instantly attacks; window-only variant of the witness event) |
| B4 ball watchdog | `give_weapon`, `weapon_equipped` (2_B) | **carried_item_loss_timer** (if a required carried weapon/item leaves the player's inventory away from its target → short recovery countdown + objective swap → keyed fail on expiry; reacquire cancels) |
| B6 marble zones | `projectile_impact_in_trigger` (1_04/3_01A) | **object_in_zone_counter** (count persistent thrown/pickup objects of a type inside a trigger as objective progress — `ObjectTypeIsInTrigger`; impact-counting variant covers projectiles, this covers lingering spill objects; per-zone blip clear) |
| B6 marble refill | `ammo_out_refill` (2_07), `pickup_spawner` | none new (refill pickup + blip + objective swap = data) |
| B7 scoreboard | `geometry_swap` (2_08?), `world_state_swap` (3_07), `camera_path_cycle` (3_G3) | none new for the swap; the one-shot `CameraSetPath` + `CameraSetSpeed` pan is a `camera_path_cycle` variant (note) |
| engine | `npc_despawn_on_path_end` (2_06) | **dock_despawn** variant note: peds walk to a dock *point* and are deleted on entering a dock *trigger* by a service thread (works for ambient-ized peds, not just path followers) |
| OOB | schema `zone_timer`, `leave_area_fail` | none new (OOB/BIB = two-trigger zone_timer with warn text) |
| NIS input gating | — | **input_lock_subset** (`PlayerLockButtonInputsExcept(true, 9)` — lock all buttons except the skip/advance button during NIS beats; small action) |
| fails | `mission_critical_npc` (aggression variant), `countdown_fail_timer` | none new |
| op flags | `area_script_shared_state` (3_05) | none new — but note the scope: `shared.*` op flags bridge in-mission scene transitions and are reset on cleanup (session-scoped mission progress, not cross-mission) |

## 6. Open questions

- The dead Burton-distraction gate (`bCoachDistracted` never false): intended design (dance to lure Burton from the press box before the hack) is fully wired — Burton spawn, chase-off beat, dock despawn — but the gate itself is bypassed. Cut for difficulty or broken by the initializer; **OPEN**.
- `shared.g4_06GateTutComplete` is never set `true` in this file — external setter presumed (`CowDance.lua` or the observatory area script); confirm in the area-script survey.
- `WeatherSet(1)` id semantics (rain? overcast?) and why the finale wants it.
- Weapon ids 331/341 (requested/loaded but no in-file user found) and 335 (vestigial frisbee pickup at the ball bag); 400 = football and 349 = marbles are content-obvious but id-table confirmation pending.
- `PedSetTypeToTypeAttitude` rows (1/5/2/4/9 vs 13) — the usual attitude-slot question.
- Social action ids 23–36 (blacklist) and 18 (greeting override set) — social-system table decode pending.
- `PedHasGeneratedStimulusOfType(gPlayer, 24)` — stimulus type 24 = successful-dance aura, **INFERRED**.
- `gSpeechTable` seePlayer/goodDance/badDance/leave id rows (ids 44–75) — which guard personality owns which row is DAT/order dependent; the `leave` ids are never used in-file (**INFERRED** cut flee barks).
- Whether the decoy runner in op C is literally a dog handler (variable names say "Dog", the model id 232 is shared with generic jocks) — needs the DAT point context.
- All trigger geometry, bench pairing, dock-point layout, and guard spawn elements (pending DAT extraction).
