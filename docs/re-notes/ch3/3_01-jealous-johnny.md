# 3_01 — Jealous Johnny: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_01.lua` (823 lines, complete) + cross-references in `main.lua`, `SFaction.lua`, `AreaScripts/MainMap.lua`. `SMissPass.lua` has **no** 3_01 case (**VERIFIED** by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

Variant files note: `3_01A.lua` / `3_01C.lua` / `3_01D.lua` share the id prefix but are **unrelated missions** — the Scholarship Edition Christmas content hung off the 3_01x numbering (3_01A's debug prints even call it "3_10"). They get their own docs: `3_01A-balls-of-snow.md`, `3_01C-nutcrackin.md`, `3_01D-rudy-the-red-nosed-santa.md`.

## 1. Identification

**3_01 = "Jealous Johnny"** (canonical Chapter 3 list, #35, 8AM–11PM) — **VERIFIED** multiple ways: content (Johnny Vincent, model 23, hires the player to photograph his girlfriend Lola, 25, with Gord, 30 — the player must capture three poses: hand-holding, a flower gift, and a kiss); the [Bully wiki](https://bully.fandom.com/wiki/Jealous_Johnny) entry "Take photos of Lola kissing, holding hands, and getting gifts from Gord" matches the script's three photo objectives (`3_01_PHOTKISS`/`3_01_PHOTHAND`/`3_01_PHOTFLOW`) exactly; and `main.lua:189` wires a bulletin-board notice (`BulletinSetupMessage("3_01", "BB_18")`). No anim group embeds the title (`NIS_3_01` only) — the id rests on content + wiring. No surprise.

Setting: Bullworth town streets and five alley kiss spots, all area 0 (the [StrategyWiki walkthrough](https://strategywiki.org/wiki/Bully/Chapter_3:_Love_Makes_The_World_Go_Round) starts the mission at the entrance to New Coventry and confirms the $20 / greaser-respect-+5 reward). Note one place where the walkthrough and the script disagree: it claims being spotted by the couple fails the mission — the script only breaks the moment and jeers (J3); there is no spotted-fail path (**VERIFIED** — script wins). Downstream: `MainMap.lua` gates `3_R08_Poor1` (Discreet Deliveries job board entry) on `prereq = "3_01"`; `SFaction.lua` raises the greasers' default attitude toward the player (types 4 vs 13) to 3 once 3_01 is complete (baseline 2 — value decode **INFERRED** as friendlier, matching the story turn toward Johnny).

## 2. Mission flow

Driver: `MissionSetup()` (baked intro + DAT) → `main()` runs one big polling loop plus threads `T_Photography`, `T_Dialogue`, `T_MissionStatus`. No `MissionStageRun` pointer chain — the loop is flag-driven (`bHandHolding`, `bInitiateKiss`, `bKissOver`, `shared.gKiss`).

| # | Beat | Logic (functional) |
|---|---|---|
| J0 | **Baked cutscene "3-01"** | `PlayCutsceneWithLoad("3-01", true)` in `MissionSetup`. Also: `shared.inPhotoMission = true` (suppresses other photo systems, **INFERRED**), stealth music `MS_FootStealth{Low,Mid,High}.rsm` armed, `DATLoad("3_01.DAT", 2)`, POI disabled, ambient Lola suppressed (`PedSetUniqueModelStatus(25, -1)`). No attempt-count check — retry skip **OPEN** (engine pattern). |
| J1 | **Setup + teaser NIS** (`F_SetupMission`) | Player transitioned to `_3_01_GOTOBLIP` then placed at `_3_01_PLAYERSTART`. Johnny spawned at `_3_01_JOHNNYSPAWN`, mission-critical (`cbFail` on player hit), with a **social gift callback** registered against him (`PlayerRegisterSocialCallbackVsPed(johnny, 32, cbJohnnyTalk, true)`). Gord + Lola spawned at the first alley point, min-health floored, mission-critical on player hit (`3_01_HITGORD`/`3_01_HITLOLA`), non-attacking, stimuli-ignoring, infinite sprint. Then an in-engine teaser: widescreen, camera planted at the couple's alley point, Gord plays the flower-give node once, ~6.5 s, control restored. HUD photo counter (icon `HUDIcon_photos`) set 0/3; three photo objectives added. |
| J2 | **Couple date loop** | Gord and Lola walk as a recruited ally-pair between five alley destinations (`_3_01_KISSBIKEALLEY`, `_KISSCOMICALLEY`, `_KISSYUMALLEY`, `_KISSBARBERALLEY`, `_KISSDUMPSTERALLEY`; `F_GetNextDestination` picks randomly, never repeating the current one). Cycle: hand-hold walk to the point (`PedMoveToPoint` with arrival callback `F_KissTime`) → flower-give paired anim → walk into the alley (`F_KissTimeAlley`) → kiss paired anim → next spot. After **7** kiss cycles (`bNumKisses`) they head to `_3_01_KISSFINAL` and run `F_DoLastKiss` (a forced flower-give then the `KissFinal` node, both spin-waited until playing). While hand-holding and unspotted, the couple trade generic question/reply barks on a 6.5 s cadence. |
| J3 | **Spotted break** | Each frame: `bPlayerIsSpotted = PedCanSeeObject(gord/lola, gPlayer, 3)`. If spotted outside a kiss: both stop, play a `Break` node, face the player, and jeer (random of Lola line 59 / Gord `JEER` / Lola `BUMP_RUDE`, ≥2.2 s apart) with `3_01_SPOTTED` text until line of sight is broken; then they resume the cycle at a fresh random destination. Non-hostile — purely a stealth reset. |
| J4 | **Photography** (`T_Photography`) | Three latched shot types, each valid only while its condition holds: hand-hold flag set (J2 walking), `FlowerGive` playing (excluding its `SeeBreak` sub-node), `Kiss`/`KissFinal` playing. Per frame: a target type is valid if `PhotoTargetInFrame(gord, 2)` + condition; `PhotoSetValid(validTarget)` drives the camera HUD reticle. `PhotoHasBeenTaken()` returning (true, wasValid) latches that type: counter +1, objective completed, per-type remaining-letters text (`3_01_PHOT`+H/K/G), and **the player is given item 526** (the incriminating photo, **INFERRED** from `giftModel`/giftType 16 and the Johnny gift handoff). First valid shot also blips Johnny. `T_Dialogue` plays a queued couple conversation until the player holds 526. |
| J5 | **Proximity (spook) meter** | While the counter runs, player–couple 2D distance is mapped onto a 6–40 m band, inverted and clamped to 0–100% (`nCurrentDistancePercent`) — a stealth proximity meter value (consumer is engine HUD/music side, **INFERRED**; the Lua only computes it). |
| J6 | **Containment watchdog** | If the player leaves `TRIGGER._3_01_MISSIONBLOCK` before 3 photos: re-blip `_3_01_GOTOBLIP` with a nag (once); if the player also leaves `TRIGGER._3_01_MISSIONAREA` → fail `3_01_FAILLEFT`. |
| J7 | **Couple wrap-up** | When the 7-kiss cycle exhausts (`drvKissOver` → `bKissFail`): with zero photos → fail `3_01_ONOPIC`; otherwise the date loop ends, and if the counter is still < 3 → fail `3_01_ONOPIC` (finite photo opportunities, so missed shot types can soft-lock into a fail — **VERIFIED** by the two counter checks). |
| J8 | **Return to Johnny** (`T_MissionStatus`) | At 3/3: Johnny flagged stationary/ignore-stimuli, the three photo objectives complete, objective `3_01_ORETCASH` + Johnny blip. When the player comes within 4 m of Johnny (`PlayerIsInAreaObject(johnny, 2, 4, 0)`): control off, bike dismount forced, camera FOV 40, player locks onto Johnny and Johnny walks to 0.8 m, then the player line 55 + `GivePhotos` hand-off anim plays. The social gift callback (`cbJohnnyTalk`, Johnny line 56) marks success. Johnny walks off to a hardcoded point (501.12, -196.33, 2.87). |
| J9 | **Pass** | `MinigameSetCompletion("M_PASS", true, 2000)` ($20, **INFERRED** from the cents convention), respect message `MRESPECT_GP5`, `SetFactionRespect(4, +5)` (faction 4 = greasers), pass music, Johnny deleted, `MissionSucceed(false, false, false)`. No clothing/weapon/chapter effects; no SMissPass case. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player hits Johnny | mission-critical cb | `3_01_HITJOHN` |
| Player hits Gord / Lola | mission-critical cbs | `3_01_HITGORD` / `3_01_HITLOLA` |
| Leaves mission area before 3 photos | J6 watchdog | `3_01_FAILLEFT` |
| Couple's cycle ends with < 3 photos | J7 | `3_01_ONOPIC` |

Retry: `MissionFail(false, true, msg)` — retry, no fade; on fail the player plays a `FailAnim` node under a shoulder camera before the banner. No checkpoints; retry replays from J0. Cleanup removes item 526, restores POI/population, sets `shared.gHitchHikeLoc/gHitchHikeMis` (hitch-a-ride return bookkeeping, consumers outside this file — **OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | J0 intro | baked (`3-01`) | `fade` in at `_3_01_PLAYERSTART` + `letterbox` + one `say` beat |
| CS-2 | J1 alley teaser | in-engine (~8 s: fixed camera, one paired anim, text) | `camera` + `pair_anim` + `banner`; fully authorable |
| CS-3 | J8 photo hand-off | in-engine (control off, FOV 40, approach + `GivePhotos` pair anim + two lines) | `pair_anim` + `say` + `camera`; authorable |
| CS-4 | J9 pass | engine UI | `completion_banner` + respect line |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 23 | Johnny Vincent | stationary quest giver/end point; social gift callback 32; hit = fail |
| 30 | Gord | date-loop leader; min health 0.05; photo subject |
| 25 | Lola | date-loop follower (recruited ally of Gord); ambient spawn suppressed during mission |

### Animation groups / action trees

`LoadAnimationGroup`: `NPC_Love`, `NIS_3_01`, `MINI_React`. Action tree `Act/Conv/3_01.act` with nodes `Anims/Break`, `Anims/FlowerGive(/FlowerGive)` (+ `Give_Attempt/Receive/SeeBreak` exclusion), `Anims/Kiss`, `Anims/KissFinal`, `Anims/GivePhotos/GiveJohnny3_01`, `Anims/FailAnim`.

### Props / pickups / weapons

- Item/pickup **526** = the photo evidence (one granted per valid shot; removed on cleanup). `giftType = 16`.
- Weapon models loaded: 328, 359, 361 — never handed out in the script (**OPEN**: likely camera-adjacent or leftover loads).
- No props, no vehicles.

### Speech

`M_3_01` scripted ids: 55 (player hand-off), 56 (Johnny reply), 59 (Lola spotted bark) — **3 distinct scripted lines**, plus generic events (`CONVERSATION_QUESTION/REPLY`, `JEER`, `BUMP_RUDE`, and an oddly reused `SEE_BOY_IN_DORM`). Music: `MS_FootStealth` low/mid/high interactive set; pass/fail stingers.

### Named world data to extract from `3_01.dat`

- POINTLISTs: `_3_01_GOTOBLIP`, `_3_01_PLAYERSTART`, `_3_01_JOHNNYSPAWN`, `_3_01_KISSBIKEALLEY`, `_3_01_KISSCOMICALLEY`, `_3_01_KISSYUMALLEY`, `_3_01_KISSBARBERALLEY`, `_3_01_KISSDUMPSTERALLEY` (each ≥3 elements: couple points + camera), `_3_01_KISSFINAL`.
- TRIGGERs: `_3_01_MISSIONBLOCK`, `_3_01_MISSIONAREA`.
- Hardcoded: Johnny's exit walk target (501.12103, -196.32683, 2.87158).
- Area codes seen: **0 only**. DAT referenced as `3_01.DAT` — trigger data exists in the original assets; the research clone is Lua-only, extraction pending.

## 5. Framework gaps (beat → primitives)

Reuses ch1/ch2 vocabulary: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `mission_critical_npc`, `ped_hit_event` (who-hit-me-last), `recruit_ally`, `pair_anim`, `collect_counter`, `dynamic_blip`, `completion_banner`, `set_faction_respect`, `stay_in_zone_fail`/`zone_exit_countdown` family (J6), `unique_ped_suppress`, `social_interact`/`gated_social_branch` (J8 gift callback), `give_item_scene`, `cutscene_skip_on_retry`. Schema-side, `MissionSubsystem.h` already has `HasItem`, `Timer`, `LeaveArea` conditions and `GiveItem`, `Reward`, `SetRespect`, `MinigameLaunch` actions.

| Beat | Covered by | New primitives needed |
|---|---|---|
| J4 photo shots | `collect_counter`, `HasItem` | **photo_capture_condition**: valid-shot event = target-in-frame AND scripted pose predicate (anim-playing / behavior flag); per-type latch; drives camera reticle validity. The photo camera itself (aim mode, shutter, valid indicator) is a new player mode: **photo_mode** |
| J2 couple date loop | `recruit_ally`, `pair_anim`, `npc_follow_path`-adjacent | **npc_pair_date_loop**: two NPCs walk hand-in-hand between random non-repeating POIs, running paired anim beats at each, N cycles then a scripted finale point |
| J3 spotted break | `stealth_spotter` (hostile) | **spotted_pause_resume**: non-hostile variant — on LOS: stop + face + jeer loop; on LOS broken: resume cycle. No combat escalation |
| J5 spook meter | `annoyance_meter`-adjacent | **distance_band_meter**: continuous 0–100% value from 2D distance across a min/max band (HUD/music consumer) |
| J7 finite-chance fail | — | nothing new: stage-complete condition = counter==3 OR (cycle_done AND counter<3 → fail) — composition of `Timer`-style flags |
| J8 hand-off | `give_item_scene`, `pair_anim` | nothing new (approach-to-0.8 m + pair anim + gift callback = composition) |
| pass | `completion_banner`, `set_faction_respect`, `Reward` | nothing new |

Genuinely NEW names introduced here: **photo_mode**, **photo_capture_condition**, **npc_pair_date_loop**, **spotted_pause_resume**, **distance_band_meter**. (`PedAlwaysUpdateAnimation`, `DoublePedShadowDistance`, audio-focus switches are render/audio flags — note for the engine port, not mission primitives.)

## 6. Open questions

- Retry skip of `3-01` (no attempt check in file) — **OPEN**, engine attempt-count assumed.
- What consumes `nCurrentDistancePercent` (HUD meter vs music intensity) — engine-side, **INFERRED** HUD.
- Weapon models 328/359/361 loaded but unused in-file — **OPEN**.
- `shared.gHitchHikeLoc`/`gHitchHikeMis` consumers (post-mission ride offer?) — **OPEN**.
- Whether the gift social action (callback 32) is the required success trigger or a redundant belt-and-braces alongside the 4 m proximity scene — both set success; exact engine order **OPEN**.
- `SFaction` attitude value semantics (2→3 for greasers after pass) — cited, decode **INFERRED** only.
- Photo camera ownership: the script never grants the camera — assumes `C_Photography_1` already done (mission table ordering, outside the Lua) — **OPEN** here.
