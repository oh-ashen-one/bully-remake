# 4_01 — Paparazzi: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_01.lua` (1230 lines, complete) + cross-references in `SMissPass.lua`, `SFaction.lua`, `AreaScripts/Gdorm.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_01 = "Paparazzi"** (canonical Chapter 4 list, #48 in `docs/story-mode-catalog.md`, 8AM–7PM) — **VERIFIED** multiple ways: the [Bully wiki Chapter IV page](https://bully.fandom.com/wiki/Chapter_IV) lists "Paparazzi 4-01" by script id; content matches the [Paparazzi wiki entry](https://bully.fandom.com/wiki/Paparazzi) (Earnest hires Jimmy to photograph Mandy: once at cheerleading practice in the gym, once in the girls' dorm shower, once in her bedroom; rewards $25, nerds +5, jocks −10); and the script's pass block pays `2500` (cents convention ⇒ $25) with `SetFactionRespect(1, +5)` / `SetFactionRespect(2, −10)` and messages `MRESPECT_NP5`/`MRESPECT_JM10` — clique decode: faction 1 = nerds, 2 = jocks, 13 = player (consistent with `SFaction.lua`'s greaser=4 checks and the wiki reward line). Note the **script id order ≠ catalog story order** (catalog #48 of ch4, but the lowest 4_0x id): SE renumbered story order around the SE-exclusive missions; the wiki's script-id mapping is the authority here.

Setting: school gym (area 0) then the girls' dorm interior (area 35, `AreaScripts/Gdorm.lua` owns the ambient dorm systems). Upstream: the wiki says Paparazzi is unlocked by Nerd Boss Fight (`4_B1`). Downstream (**VERIFIED** in the clone):

- `SMissPass.lua`: **no** 4_01 case (no pass-side class/chapter effects).
- `SFaction.lua`: once 4_01 is complete, jocks' default attitude toward the player (type 2 vs 13) becomes 1 in `F_MissionFactionChanges` (later entries 4_B2→4, 5_09→1 override chronologically — value decode **INFERRED** as the story turn against the jocks).
- `AreaScripts/Gdorm.lua:38`: while 4_01 is active, teacher models are disabled and the `_GDORM_GIRLSONLY` population/bust trigger is deactivated — the mission replaces the ambient "boy in the girls' dorm" rule with its own bust logic. The dorm head (model 54, `shared.gdormHeadID`) is created by the area script with a stealth behavior; 4_01 drives it through shared flags (`gdormHeadStop/Start/CanMove/SpottedPlayer`).

## 2. Mission flow

Driver: `MissionSetup()` (baked intro + DAT) → `main()` runs three sequential stages (`F_Stage1` gym photo → `F_Stage2` dorm shower+bedroom photos → `F_Stage3` hand-off to Earnest), each a flag-driven polling loop (`StageN_Objectives`). Threads: `T_MonitorCheerleaders`, `T_SpeechChristyDorm`, `T_ResetGirlsDorm`, plus the skippable NIS thread `T_Cutscene01`. Debug stage-jump entries exist (`F_StartAtStage2/3`, gated on `bDebugFlag`).

| # | Beat | Logic (functional) |
|---|---|---|
| P0 | **Baked cutscene "4-01"** | `PlayCutsceneWithLoad("4-01", true)` in `MissionSetup`, `MissionDontFadeIn`, stealth music set armed (`MS_FootStealth{Low,Mid,High}.rsm`, high at 0.7 volume), `DATLoad("4_01.DAT", 2)`. Retry skip: no attempt check — **OPEN** (engine pattern). |
| P1 | **Gym photo** | Player placed at `_4_01_SPAWNPLAYER`; Mandy (14) mission-critical (`F_MissionCriticalMandyGym`) plus Christy (181), Angie (180), Pinky (182) spawned at gym points, all ignore-stimuli; ambient Mandy suppressed (`PedSetUniqueModelStatus(14, -1)`). Objective `4_01_MOBJ_01` + photo HUD counter 0/3 (`HUDIcon_photos`). Entering `_4_01_GYMINSIDEDOOR`/`_4_01_GYMTUNNEL` starts the four-girl cheer routine (`/Global/Ambient/Scripted/Cheerleading`). Photo validity = `PhotoTargetInFrame(mandyGym, 2)` with a one-frame sticky latch (`bValidNowOrBefore`); a valid shutter press → counter 1 → `F_CutPhotoGym`. |
| P2 | **Gym NIS** (`F_CutPhotoGym` + `T_Cutscene01`) | In-engine, skippable (button 7 polls in the main loop; `bSkipFirstCutscene` short-circuits every beat of the thread): control off, player teleported to `_4_01_GYMMOVEJIMMY`, letterbox, FOV 80, two camera setups; Mandy runs a multi-beat routine (jump/fall, idle variants, a self-conscious beat) with three scripted lines (ids 91, 5, 92), then exits along `_4_01_MANDYEXITGYM`. Afterward gym-Mandy is deleted and the other three girls path to the locker room and go ambient. `NIS_4_01` anim group loaded/unloaded around the scene. |
| P3 | **Into the girls' dorm** (`F_Stage2_Setup`) | Shower-Mandy (230, towel model) and bedroom-Mandy (93, pajamas) spawned with social actions 23/28/29/35 disabled against them; Eunice (74), Karen (137), dorm-Christy (67, on laundry path) spawned; `GDORM_UPPERDOOR` locked; the bust rule armed (`bBustPlayer`); bed-cream pickup 347 created (`PermanentMission`); objective `4_01_MOBJ_01B` + attic blip. A trellis tutorial latch: first time the player is in `_4_01_TUTLATTICE` playing the trellis climb-on node, `bTutLattice` sets (one-shot tutorial marker — consumer **OPEN**, likely an engine help popup). |
| P4 | **Dorm arrival swap** | When `AreaGetVisible() == 35` (girls' dorm interior): objective swaps back to `4_01_MOBJ_01`, attic blip removed, shower-Mandy blipped. Leaving area 35 later re-arms Eunice's launch (`T_ResetGirlsDorm`). Christy gets a two-line scripted exchange (ids 18/19) on `_4_01_CHRISTYSPEECH`, with Karen playing a prop-interact node. Eunice walks downstairs on `_4_01_LAUNCHEUNICE` (bark pair 15/17 at path node 5) and back upstairs after the shower photo (prop-interact at node 21). |
| P5 | **Shower photo** | While the player is in `_4_01_GIRLSDORMBATHROOM`, shower-Mandy plays a washing loop and three steam effects spawn (2× `ShowerSteam2` at the shower/steam points + `SteamRoom`); leaving stops the anim and slow-kills the effects. Warn trigger `_4_01_SHOWERWARN` prints a one-shot caution line. Photo validity = `PhotoTargetInFrame(mandyShower, 2)` + sticky latch; valid shot → counter 2 → `F_CutPhotoShower` (in-engine: Mandy finishes, covers up, exits along `_4_01_MANDYEXITSHOWER`; `GDORM_UPPERDOOR` unlocked and held open; bedroom-Mandy blipped and posed in a leg-massage loop) → Eunice sent upstairs → objective `4_01_MOBJ_01C`. |
| P6 | **Bedroom photo** | Same warn/bust/photo pattern in Mandy's room (`_4_01_MANDYROOMWARN`, `_4_01_MANDYROOMBUSTED`, `_4_01_MANDYROOM` for her idle line 90). Valid shot → counter 3, bedroom-Mandy goes ambient and wanders, upper-door storage held open, → stage 3. |
| P7 | **Dorm bust (soft fail)** | While `bBustPlayer` and area 35: if the dorm head has LOS to the player (`PedCanSeeObject(gdormHeadID, gPlayer, 3)`) or is hit → `F_PlayerSpotted` (bust camera framed on her, scripted line 45) → `F_CutBootedOut` (player walked out along `_4_01_ROUTECUTBOOT` under letterbox, line 40) → `F_EjectPlayerFromDorm`: player teleported to `_4_01_PLAYERBOOTED`, **punishment points reset to 0**, Eunice repositioned (downstairs if counter ≤ 2 else upstairs), mission continues. Non-fatal — a soft-fail eject. |
| P8 | **Caught by Mandy (hard fail)** | Entering `_4_01_SHOWERROOM`, or hitting shower/bedroom Mandy, or entering `_4_01_MANDYROOMBUSTED`: a scripted freak-out vignette (control off, letterbox, fixed camera at the room, cover-up/freak-out nodes, lines 10/11), she flees, goes ambient, **+200 punishment points**, dorm head restarted → fail, message slot 2 (`4_01_FAIL_02`). `F_MandyBustedPlayerInGym` is a third variant of the same scene, defined but never called in this file (**dead code**). |
| P9 | **Gym violence fail** | Hitting any of the four cheerleaders (direct hits via `PedGetWhoHitMeLast` or stimulus type 55 against the three non-critical girls) → cheer routine stopped, Mandy line 7, all four go ambient → fail slot 2. |
| P10 | **Hand-off to Earnest** (`F_Stage3`) | Counter hidden; player given item 526 (the photo set); Earnest (10) spawned stationary/ignore-stimuli with required gift 16 and the follow/gift social overrides loaded (`Mission/4_01Follow.act`, `Mission/4_01PlayerGift.act`), mission-critical (`F_MissionCriticalEarnest` → he storms off, fail slot 3 `4_01_FAIL_03`). Player within 3 m → control off, letterbox, bike dismount, mutual face + lock, the `GiveEarnest_4_01` give node spin-waited, 2 s beat, gift requirement cleared, Earnest walks to `_4_01_EARNESTLEAVE` and goes ambient → `bMissionPassed`. The named callbacks `F_EarnestReceivedPhotos`/`F_PlayerGavePhotos` (and the closing-dialogue pair 25/26 in `F_ClosingDialogue`) are **never registered/called in this file** — dead code or engine-side gift hookup, **OPEN**. |
| P11 | **Pass** | `SetFactionRespect(2, −10)` (jocks), `SetFactionRespect(1, +5)` (nerds), `MinigameSetCompletion("M_PASS", true, 2500)` ($25) + respect messages, pass music, fade out/in with camera reset, `MissionSucceed(false, false, false)`. No clothing/weapon/chapter effects; no SMissPass case. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player hits Mandy (gym) or any cheerleader | mission-critical cb + `T_MonitorCheerleaders` | `4_01_FAIL_02` (slot 2) |
| Mandy sees/hit by player in shower or bedroom | P8 vignette | `4_01_FAIL_02` (slot 2) |
| Player hits Earnest | mission-critical cb | `4_01_FAIL_03` (slot 3) |
| Dorm head busts player | P7 | **not a fail** — eject + continue |

`gMissionFailMessage == 1` (`4_01_FAIL_01`) is never assigned anywhere in the file — **dead fail slot**. Retry: `MissionFail(false, true, msg)` — retry, no fade; no checkpoints, retry replays from P0. On fail slot 2 with stage ≥ 2 the script explicitly strips player control first (`PlayerSetControl(0)` guard in `main()`).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | P0 intro | baked (`4-01`) | `fade` in at `_4_01_SPAWNPLAYER` + `letterbox` + one `say` beat |
| CS-2 | P2 gym NIS | in-engine, skippable (~15 s, 2 cameras, 3 lines) | `camera` + `say` + `ped_action_node` beats inside `skippable_scene_chain` |
| CS-3 | P5 shower wrap | in-engine (~10 s, 2 cameras, 1 line) | `camera` + `say` + `npc_follow_path` exit |
| CS-4 | P7 boot-out | in-engine (bust cam + walk-out path) | `scripted_bust_vignette` + `player_follow_path` |
| CS-5 | P8 freak-out | in-engine (fixed room camera, 2 lines, flee) | `camera` + `say` + `ped_action_node` |
| CS-6 | P10 hand-off | in-engine (proximity trigger, give anim, no lines registered) | `give_item_scene` + `pair_anim` |
| CS-7 | P11 pass | engine UI | `completion_banner` + respect lines |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 14 | Mandy (gym) | mission-critical; ambient suppressed (unique model −1) |
| 230 | Mandy (shower/towel) | photo target 2; social actions 23/28/29/35 disabled |
| 93 | Mandy (bedroom/PJs) | photo target 3; same social blacklist |
| 181 / 180 / 182 | Christy / Angie / Pinky (gym) | cheer set dressing; hit ⇒ fail via monitor thread |
| 67 | Christy (dorm) | laundry path + speech trigger |
| 74 | Eunice | dorm flavor; unique model suppressed during stage 2+; path barks |
| 137 | Karen | dorm flavor, prop-interact beat |
| 10 | Earnest | stage-3 gift receiver; hit ⇒ fail slot 3 |
| 54 | dorm head (area-owned) | created by `Gdorm.lua`; mission drives via `shared.gdormHead*` |

### Animation groups / action trees

`LoadAnimationGroup`: `F_Girls`, `ChLead_Idle`, `SHWR`, `UBO`, `POI_ChLead`, `NIS_4_01` (P2 only). Action tree `Act/Conv/4_01.act` with nodes `Anims/Empty`, `Anims/Showering/Wash01`, `Anims/Showering/FinishedShower`, `Anims/FreakOut/FreakOut(/CoverUp)`, `Anims/GIVE/GiveEarnest_4_01`, plus the `GymCut/*` NIS set (`Mandy/Mandy01`, `FallDownGetUp/Jump`, `ChLeadIdle04/05`, `SmellSelf/Smell_Pits`). Also `Act/WProps.act` (`PropInteract`), `Act/Anim/Ambient.act` (`Scripted/Cheerleading`), trellis node `/Global/Trellis/Trellis_Actions/Climb_ON_BOT`.

### Props / pickups / weapons

- Pickup **347** (bed cream) — `PermanentMission` dorm dressing.
- Item **526** (the photo evidence) — granted once at stage 3, zeroed on cleanup; `PedSetRequiredGift(..., 16)` matches the 3_01 gift-type convention.
- Weapon model **341** requested at setup; **361** loaded in `F_CutPhotoShower` — neither is ever handed out in-file (**OPEN**: leftover loads; 341 plausibly a camera-adjacent model).
- Camera re-equip helper `F_ResetCamera` re-arms item **426** else **328** after each photo NIS (camera ownership assumed, as in 3_01).
- Effects: `ShowerSteam2` ×2, `SteamRoom` ×1, spawned/slow-killed with the washing loop.
- Doors: `GDORM_UPPERDOOR` (lock + hold-open), `_GDORM_UPPERDOORSTORAGE` (hold-open).

### Speech

`M_4_01` scripted ids seen: 5, 7, 9, 10, 11, 15, 17, 18, 19, 25 (dead), 26 (dead), 40, 45, 90, 91, 92 — **14 live + 2 dead scripted lines**, plus ambient event `CHEERLEADING`. Music: `MS_FootStealth` interactive set (stopped at stage 3); pass/fail stingers.

### Named world data to extract from `4_01.dat`

- POINTLISTs: `_4_01_SPAWNPLAYER`, `_4_01_DEBUGSPAWNPLAYERSTAGE1/2/3`, `_4_01_SPAWNMANDYGYM`, `_4_01_SPAWNCHRISTYGYM`, `_4_01_SPAWNANGIEGYM`, `_4_01_SPAWNPINKYGYM`, `_4_01_SPAWNMANDYSHOWER`, `_4_01_SPAWNMANDYBEDROOM`, `_4_01_SPAWNCHRISTYDORM`, `_4_01_SPAWNEUNICE`, `_4_01_SPAWNKAREN`, `_4_01_SPAWNEARNEST`, `_4_01_BLIPLATTICE`, `_4_01_BLIPGDORMATTIC`, `_4_01_BEDCREAM`, `_4_01_EFFECTSTEAMSHOWER`, `_4_01_EFFECTSTEAMROOM`, `_4_01_BUSTEDSHOWER`, `_4_01_MANDYOUTOFSHOWER`, `_4_01_GYMMOVEJIMMY`, `_4_01_CUTSHOWERMANDY`, `_4_01_CUTSHOWERPLAYER`, `_4_01_EARNESTLEAVE`, `_4_01_PLAYERBOOTED`, `_4_01_CUTBOOTJIMMY`, `_4_01_CUTBOOTHEAD`, `_4_01_SPAWNEUNICERESETDOWN`, `_4_01_SPAWNEUNICERESETUP`.
- TRIGGERs: `_4_01_GYMINSIDEDOOR`, `_4_01_GYMTUNNEL`, `_4_01_TUTLATTICE`, `_4_01_GIRLSDORMBATHROOM`, `_4_01_SHOWERWARN`, `_4_01_SHOWERROOM`, `_4_01_MANDYROOMWARN`, `_4_01_MANDYROOMBUSTED`, `_4_01_MANDYROOM`, `_4_01_CHRISTYSPEECH`, `_4_01_LAUNCHEUNICE`, `_GDORM_UPPERDOOR`, `_GDORM_UPPERDOORSTORAGE`.
- PATHs: `_4_01_CHRISTYLAUNDRY`, `_4_01_EUNICEDOWNSTAIRS`, `_4_01_EUNICEUPSTAIRS`, `_4_01_MANDYEXITGYM`, `_4_01_GIRLSTOGYMLOCKERROOM`, `_4_01_MANDYEXITSHOWER`, `_4_01_ROUTECUTBOOT`.
- Hardcoded cameras: gym NIS (−619.71, −60.45, 60.86) / (−622.16, −56.71, 62.06) / (−621.42, −56.91, 60.16); shower bust (−423.56, 303.98, −0.16); bedroom bust (−415.20, 304.50, −1.27); shower wrap (−428.81, 301.21, −1.15) / (−428.16, 307.29, −0.87); boot-out (−437.95, 313.58, −5.99). Bust cam helper derives a two-point frame from the busting ped's offsets.
- Area codes seen: **0** (school), **35** (girls' dorm), 13 (debug-only spawn).
- **Trigger data (DAT): YES** — `DATLoad("4_01.DAT", 2)`; per `trigger-format.md` the original assets carry `4_01.dat` in `Trigger.img`, but the research clone is Lua-only, so extraction is pending. Not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses the ch1/ch2/ch3 vocabulary heavily: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `mission_critical_npc`, `ped_hit_event`, `unique_ped_suppress`, `collect_counter`, `dynamic_blip`, `completion_banner`, `set_faction_respect` / `respect_delta`, `interactive_music_stream` (stealth set), `door_lock_set`, `area_visible_eq` (35-checks), `player_action_state` (trellis climb node), `effect_spawn_at_points` (steam), `ped_action_node`, `npc_follow_path`, `social_override_set` + `social_action_blacklist` (23/28/29/35 disables, gift overrides), `social_gift_callback` + `give_item_scene` (Earnest hand-off), `punishment_points_add` (+200 on hard fail), `equip_item_if_owned` (camera re-arm), `warn_first_hit`-family one-shot warn text, `skippable_scene_chain` (P2 with its button-7 poll). Schema-side, `MissionSubsystem.h` already covers `Timer`, `HasItem`, `LeaveArea`, `GiveItem`, `Reward`, `SetRespect`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| P1/P5/P6 photos | `photo_mode`, `photo_capture_condition` (3_01) | none — note one refinement: the validity predicate here is pure target-in-frame (no pose predicate), and 4_01 adds a **one-frame sticky latch** (`bValidNowOrBefore`) so a shutter press landing a frame after the target leaves the reticle still counts. Fold into `photo_capture_condition` as a `sticky_frames: 1` parameter |
| P7 dorm bust → eject | `soft_fail_eject_reset` (2_S06 panty raid — same dorm, same shape) | none — 4_01's variant additionally zeroes punishment points and repositions a flavor NPC; both are parameters of the eject, not a new primitive |
| P8 caught vignette → fail | `scripted_bust_vignette` + fail path | none (composition: control off + fixed camera + freak-out node + flee + `punishment_points_add` + fail) |
| P3 trellis tutorial latch | `player_action_state` + `hint` | none (one-shot latch on anim-state-in-trigger) |
| P5 steam on/off | `effect_spawn_at_points` | none (trigger-gated create/slow-kill pair) |
| P10 gift hand-off | `give_item_scene`, `social_gift_callback` | none — though note the registered-callback side is dead code here; the proximity give-anim is the live path |

Genuinely NEW names introduced here: **none**. 4_01 is entirely a composition of the ch1–ch3 primitive set — the closest calls (sticky photo latch, eject-with-punishment-reset) are parameter variants of existing names, listed above.

## 6. Open questions

- Retry skip of `4-01` (no attempt check in file) — **OPEN**, engine attempt-count assumed.
- Fail slot 1 (`4_01_FAIL_01`) and `F_MandyBustedPlayerInGym` are unreachable in this file — cut content or external caller? **OPEN**.
- `F_EarnestReceivedPhotos` / `F_PlayerGavePhotos` / `F_ClosingDialogue` (lines 25/26) never registered or called — was the gift social action supposed to drive the closing beat (the 3_01 pattern)? **OPEN**.
- Weapon models 341/361 loaded, never used in-file — **OPEN** (leftover or engine-side camera swap).
- What consumes the `bTutLattice` latch — likely an engine tutorial popup for trellis climbing; **OPEN**.
- Whether the dorm head's bust detection cooperates with `Gdorm.lua`'s own stealth callback during the mission (the mission sets `gdormHeadStop/Start` but the area script's `F_stealthCallbackHeadmistress` may also fire) — **OPEN**, needs the area script's stealth path traced.
- `SFaction` attitude value semantics (jocks → 1 after pass) — cited, decode **INFERRED** only.
