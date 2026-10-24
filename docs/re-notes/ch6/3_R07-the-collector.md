# 3_R07 — The Collector (Clint's bikes): recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap6/3_R07.lua` (433 lines, complete). No `SMissPass.lua` branch (**VERIFIED**). Structure in our own words; script/native/string-key names are citations only.

## 1. Identification

**3_R07 = "The Collector"** — the optional Chapter 5 side mission given by Clint ("Clint aka Henry", the Blue Skies trailer-park oddball): steal three marked bikes from the kids riding them around Blue Skies and stash them in Clint's garage. Identification evidence:

- [TCRF, Bully (PS2) Unused Cutscene Content](https://tcrf.net/Bully_(PlayStation_2)/Unused_Cutscene_Content): the lone leftover of the PS2-scrapped mission "The Collector" is `3-R07.cut` — the same cutscene id this script plays (`PlayCutsceneWithLoad("3-R07")`); TCRF's development-leftovers page ties "the scrapped The Collector mission" to Clint's garage behavior, and notes it "would later reappear in Scholarship Edition as The Collector". **VERIFIED** name/id pairing.
- A contemporary walkthrough describes The Collector exactly as: Clint wants three bikes, five marked bikes on the map, bring them back and put them in his garage — this script's loop, verbatim shape (**VERIFIED** by content).
- Catalog cross-check: `docs/story-mode-catalog.md` #60, "The Collector (O)", **7PM–11PM** window, Chapter 5.

Why it sits in the `chap6/` folder: the folder grouping in this dump doesn't match script-id prefixes elsewhere either (5_B lives there too). The script's own text keys are `5_T1_*` (objectives/fail text) while the speech bank is `M_3_R07` and the cutscene `3-R07` — the `5_T1` internal tag (**INFERRED**: fifth "T"-series side slot) supports the Chapter 5 placement; nothing in the script gates on chapter, so availability comes from the engine mission table (**OPEN** — not in the Lua dump). Offered at Clint's place/garage in Blue Skies, 7–11PM per the catalog.

Dead/loose ends (**VERIFIED** in-file): `main()` calls `F_StartGetBike()` three times in a row up front with two duplicate Clint barks — but the first call blocks until bike 1 is delivered, so the duplicates are just the loop unrolled by hand; `MissionCleanup` references `gurney` and `otto` variables that don't exist in this file (copy-paste residue from another mission; `PedIsValid(nil)` is false so they're harmless); `bFirstTimeSecondBike` declared, never used; the speech bank/event keys mix `M_3_R07` with `5_T1_*` text.

## 2. Mission flow

Driver: linear `main()` → `F_StartMission()` → `F_StartGetBike()` ×3 (each blocks until delivered) → `F_MissionFinished()`. Service threads: `F_ClintFacePlayer` (giver faces the player every 3 s), `F_MissionFail` (watches the fail flag), `F_CheckGarage` (garage door proximity). No checkpoints.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup + baked cutscene "3-R07"** | Unique models 45/46/42 suppressed (Clint + his housemates, **INFERRED**); ambient bikes purged (bike generators off, all vehicles cleared, ambient override `3,2,0,1`, every bike within 200 m of the player deleted) so the only bikes in the world are the five mission bikes; Clint (45) spawned asleep/stationary at his start point, mission-critical with an aggression fail callback, emotion-toward-player maxed friendly, two social actions disabled against him (28/29), social override set 4, one stimulus response overridden; HUD counter shows 0/3. |
| B1 | **Five marked riders** | Five rider+bike pairs (peds 44/43/123/222/128 on bikes 274/279/282/280/283) spawn at `_5_T1_PATHPEDSTART01-05` and loop paths `_5_T1_BIKEPATH01-05`. Any three are enough. Watchdog: a rider who loses his bike (player pulls him off) stops, turns hostile to the player, attacks, and goes ambient (`F_CheckPathBiker`). |
| B2 | **Fetch loop** (`F_StartGetBike`) | Blips on every unretrieved valid bike. Objective: take one. When the player is riding a marked bike, ownership is set to the player and the delivery phase starts; proximity to Clint without a bike re-arms his 6 s nag bark. |
| B3 | **Deliver to the garage** (`F_GetBackToClint`) | Objective + blip on the drop point. The garage PAnim door auto-opens within 15 m of the drop point and closes when the player leaves (`F_CheckGarage`). **If the player dismounts for any reason before entering the garage trigger, the delivery aborts and the fetch loop restarts** (objective/blip reset). Entering `_5_T1_BIKEGARAGE` mounted → delivery. Clint's "got one" bark re-arms on a 6 s cooldown within 10 m. |
| B4 | **Stow vignette + counter** (`F_GotBikeToClint`) | Counter +1; the player's punishment points are zeroed (bike theft is instantly forgiven); control off, player invulnerable, auto-dismount, fade; the delivered bike is deleted and **recreated at the garage drop point** (same model id) for a camera beat of the bike sitting in the garage, then deleted again; player walked back to the start point; garage door closes. At 3/3: Clint goes ambient with a final bark, counter HUD hides, the pass card (`MinigameSetCompletion("M_PASS", true, 5000)`) shows, pass stinger 10, `MissionSucceed(true, false, false)`. Otherwise the garage thread restarts for the next bike. |
| B5 | **Bike respawn** | After each delivery, every unretrieved rider/bike pair that was destroyed is **respawned fresh at its path start** on a stock bike (274) and put back on its loop; retrieved riders are deleted. The five-marker pool is effectively inexhaustible until you bank three. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Player attacks Clint | `PedSetMissionCritical(clint, true, F_ClintHit, true)` (aggression flavor — Clint turns ambient/vulnerable first) | `5_T1_FAIL_01` (fail stinger 6) |
| Any other failure | generic | `MissionFail(false)` |

No timer, no lose-the-bike fail (dismounts just reset the loop). Full replay on fail; cleanup restores ambient traffic/bike generators, deletes all riders/bikes/Clint, re-enables the unique models, hides the counter.

### Pass-side effects

- In-file: pass card + stinger only. (Walkthroughs list a cash reward, ~$50 — **OPEN**: no `PlayerAddMoney` exists in-file, so the reward is granted by engine mission-table metadata, not this script.)
- `SMissPass.lua`: no branch (**VERIFIED**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-1 | B0 | baked (`3-R07`) | `fade` + `letterbox` + one `say` beat |
| CS-2 | B4 stow vignette ×3 | in-engine, ~4 s | `fade` + `vehicle_grant`-style respawn at point + `camera` + `npc_despawn` |

No extra anim groups loaded; music: pass stinger 10 / fail stinger 6, no interactive stream.

## 4. NPCs and assets needed

### Peds / vehicles (model ids as scripted)

| Model id | Role | Notes |
|---|---|---|
| 45 | Clint (giver) | mission-critical (aggression = keyed fail); asleep-staged; face-tracking thread; social overrides; proximity barks (nag / got-one / final) |
| 46, 42 | suppressed unique models | never spawned in-file (Clint's household dressing, **INFERRED**) |
| 44, 43, 123, 222, 128 | bike riders ×5 | loop fixed paths; dismount → hostile → ambient |
| 274, 279, 282, 280, 283 | the five marked bikes | respawn as stock 274 if lost |

Props: `SBikeGar` garage door PAnim (proximity-driven). No weapons.

### Named world data to extract from `3_R07.DAT` (present — `DATLoad("3_R07.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_5_T1_PLAYERSTART`, `_5_T1_CLINTSTART`, `_5_T1_BIKEDROP`, `_5_T1_PLAYERDROP`, `_5_T1_PATHPEDSTART01-05`.
- PATHs: `_5_T1_BIKEPATH01-05`.
- TRIGGERs: `_5_T1_BIKEGARAGE`.
- Hardcoded coords in-file: garage PAnim position (302.1, −421.1, 2.9), garage camera pair, 15 m door radius.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by existing set | New primitives needed |
|---|---|---|
| B0 ambient bike purge | `vehicle_area_purge` (2_R03-era), `ambient_vehicle_override`, `spawner_radius_toggle` | none |
| B0 giver bundle | `mission_critical_npc` (aggression variant), `social_override_set`, `social_action_blacklist`, `npc_asleep_staging`, `npc_bark_timer` | none |
| B1 marked riders | `npc_ride_vehicle_path`, `blip_on_vehicle`, `npc_dismounted_event` (dismount → attack) | none — composition |
| B2/B3 fetch-deliver loop | `player_mounted`, `collect_counter`, `counter_threshold_meter`, `vehicle_guard_reclaim` | **vehicle_delivery_loop** (steal/pick a marked vehicle → ride it into a delivery trigger → short stow vignette (delivered vehicle deleted, a display copy created at the drop point, camera beat, counter++) → repeat to N with HUD counter; **dismounting mid-delivery aborts back to the fetch stage**; destroyed marked vehicles respawn at their route start so the pool can't soft-lock) |
| B3 garage door | `proximity_prop_mover`, `door_prop_control` | none — proximity radius on a PAnim door |
| B4 punishment wipe | `heat_zero` | none |
| pass card | `completion_banner` / minigame completion card | none |

## 6. Open questions

- The launch gate (chapter/window check) — engine mission table; the catalog's 7PM–11PM Chapter 5 window is the current best record.
- The cash reward: walkthroughs say ~$50, but no grant exists in the Lua — engine-side metadata presumed.
- Whether the PS2 cut version differed beyond the cutscene (TCRF notes the garage was "meant to be manipulated by the game" — the shipped SE script does drive it, so the note likely refers to the PS2 leftover state).
- Why models 46/42 are suppressed but never used (cut pre-mission beat at Clint's house?).
- `PedOverrideSocialResponseToStimulus(clint, 28, 4)` + `PlayerSocialDisableActionAgainstPed` ids 28/29 — social-system table decode pending (same standing question as 4_06's blacklist ids).
