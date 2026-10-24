# 5_05 — Revenge on Mr. Burton: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_05.lua` (1100 lines, complete). No `SMissPass.lua` branch exists for `5_05` — all pass effects are in-file (**VERIFIED**, SMissPass read). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_05 = "Revenge on Mr. Burton"** (canonical Chapter 5 list, mission #58, 8AM–7PM) — **VERIFIED** by content: the player fetches bolt cutters, recruits Zoe (model 48, unique-model suppression in setup), hands her the cutters in an NIS, sabotages the park's portable toilets while Burton (model 55) jogs his route, and finally rams the rigged toilet with the ride-on mower (vehicle 284) while Burton is inside. Catalog cross-check: `docs/story-mode-catalog.md` #58; story/beat match per the [IGN guide mirror](https://www.bazicenter.com/features/ign_guides/bully_ignpdf.pdf) and [Gamer's Temple walkthrough](https://www.gamerstemple.com/vg/games16/001865/001865g166.asp). Note script id ≠ catalog row (#58).

Retry structure (**VERIFIED**, notable): `shared.g5_05` is set to 1 inside the give-cutters NIS. If the mission is re-entered with that flag set (i.e., a fail-and-retry after the midpoint), `main()` skips the baked intro *and* stages 1–2 entirely, warps straight to the park (`_5_05_BURTONWARP`), and spawns Burton + Zoe directly. `MissionCleanup` never clears the flag — a session-scoped mid-mission checkpoint.

Dead/loose ends in the file: a whole **stealth-guard layer that is never instantiated** — `F_CreateGuard` (stealth-flagged peds with a spot-callback `F_KickPlayerOut`), `gGuardList`, the six guard paths `_5_05_MEAT01-06`, `CbStealthBehaviour`/`bMeatSpotted`, `CbCheckDoor`/`gCheckingDoorDone`, and `StageTwoSetup`'s reset of `tblGuardPaths[4].id` (whose `id` fields are never assigned) are all dead in-file; `nPlayerSpottedByPed` is written but never read (**INFERRED** cut meat-plant stealth segment, or DAT/area-script-driven — confirm when area scripts are surveyed). Also dead: `F_Debug`, `StageThreeRemake` (empty), `F_AlphaBurton`/`F_AlphaBurtonOff`, `T_PottyOccupiedCheck` and `F_PottyCheckOccupied` (an older index-based occupancy variant), the `TextQueue` machinery, `socWantGift`/`socFollow` stubs, and `F_PedIsHurt`.

Externally-bound hooks (called by DAT events or area scripts, **INFERRED** — the `drv*`/`cb*` naming cluster): `drvBurtonLeavePotty` (→ fail `5_05_FAILPOT`), `drvLiftText` (push-prompt texts), `DrvToiletSplode` (empty), `cbPottyPushed`, `drvDogAttack` (the park dog attacks), `CbStealthBurton` (Burton spots the player → instant fail). Shared-state reads: `shared.gParkSprinklers` (park sprinkler system blocks one toilet — producer outside, **OPEN**), `shared.idDog` (the dog ped is exported to shared state).

## 2. Mission flow

Driver: linear stage calls in `main()` (`F_Intro` → `StageOneSetup/StageOne` → `StageTwoSetup/StageTwo` → `F_ParkScenario`), each a blocking `while` poll; parallel threads `T_PottyBlipManager` and `T_ParkOut` run during the park half. Mid-mission checkpoint via `shared.g5_05` (see §1). No in-file stage retry; fail replays from the last *shared-flag* checkpoint, not the true top.

| # | Beat | Logic (functional) |
|---|---|---|
| R0 | **Setup** | Baked cutscene `5-05` (skipped when `shared.g5_05` set — built-in `cutscene_skip_on_retry`); Zoe's unique model suppressed (`PedSetUniqueModelStatus(48, -1)`); POI disabled; `DATLoad("5_05.DAT", 2)`; anim groups `Dodgeball`, `Dodgeball2`, `SAUTH_U`, `1_03The Setup`, `QPed`, `NIS_5_05`; three persistent entities spawned as park dressing: the hill toilet (`PortaPoo` pool entity at 483.0, 267.6, 19.9), `pxCrappy`, and the fence rail chunk `RI1d_railChunk1` Zoe later cuts. |
| R1 | **Get the bolt cutters** | Pickup 487 (permanent respawn) at `_5_05_BOLTCUTTERS` + blip; entering `_5_05_ALLEY` swaps the objective to the fetch variant; on pickup, item 487 is force-granted if somehow missing; objective → meet Zoe. |
| R2 | **Meet Zoe / hand over the cutters** | Zoe spawns at `_5_05_ZOESTART`: flag 113, stationary, stimuli/attacks ignored, attitude-to-player 3, **mission-critical** (`cbMissionCritFailure` → fail `5_05_FAILHITZOE`). Once the player enters `_AMB_RICH_AREA`, Burton spawns at `_5_05_BURTONSTART`, tethered to `_5_05_BURTONTETHER`, infinite-sprint jogging `_5_05_BURTONMAINPATH` (looping patrol, mode 2). Player approaching Zoe *while carrying the cutters* (`PlayerHasItem(487)` poll) → `F_GiveZoeCuttersNIS`: widescreen multi-beat NIS (five speech beats ids 3–7, two camera setups with FOV 70/40, give/receive paired anim); **`shared.g5_05 = 1`**; Burton keeps jogging; Zoe walks `_5_05_ZOEHIDE`. |
| R3 | **Sabotage the toilets (timed)** | Park dog (141) spawns at `_5_05_PPDOG`. Objective + **90 s countdown**. The three target toilets (`tblPotties`: `sprinklers`, `doggy`, `hide` — hardcoded coords, per-toilet exit path and dialogue id) run a state machine in `T_PottyBlipManager`: **open** → blip shown; **occupied** (`StealthDoor/Occupied` playing) → blip removed, recovers to open on `Unoccupied`; **destroyed** (prop health < 1 or `NotUseable/Damaged` playing) → permanent; **blocked** — the `sprinklers` toilet only, while `shared.gParkSprinklers` is set (external sprinkler system, **OPEN** producer). Sabotage verbs in-file: smashing (damage detection); the sprinkler block and the dog's role (`drvDogAttack` → dog attacks player) are external hooks (**INFERRED**: each toilet neutralized a different way — damage, sprinklers, dog). Stage completes when no toilet is open. Timer expiry → fail `5_05_FAILRUN`. Leaving the park (`T_ParkOut`): inside the outer `_5_05_PARKOUT` ring → rapid re-printed return warning; fully outside → fail `5_05_LEFTPARK`. |
| R4 | **Regroup with Zoe → the rigged toilet** | Player reaches Zoe → `F_CheckNIS`: vehicles purged around the hill, mower (284) spawned at `_5_05_MOWER`, pickups cleared from `_5_05_CLEARMOWER`, dog deleted; Burton re-staged at `_5_05_CHECKPOT1` (desperate pee-dance beat), then runs `_5_05_BURTONRUNLAST` to the hill toilet; Zoe walks `_5_05_ZOECUT` and cuts the fence — the `RI1d_railChunk1` persistent entity is deleted; Zoe is handed the cutters as a weapon prop (412). **30 s countdown** starts; objective: push the toilet; blip ping-pongs between the mower (when on foot) and the final potty point (when mounted). |
| R5 | **Mower ram** | Per-frame `F_FinalPottyPushCheck`: player inside `_5_05_LASTPOTTY` **while driving vehicle model 284** → success latch (`PortaCrash` 2D sound). Zoe flees (`_5_05_ZOERUNAWAY`) when the mounted player crosses `_5_05_RUNAWAYZOE`. Fail paths: `drvBurtonLeavePotty` (external: Burton gives up and leaves) → fail `5_05_FAILPOT`; 30 s timer expiry → fail (same scene, reason only if Burton left). Both fails play a camera beat on Burton (`_5_05_BURTONCINPOTTY`). |
| R6 | **Success NIS + pass** | On the ram: control off, player invulnerable, preloaded `5-05_NIS_PottyFall.rsm`, camera path pair `_5_05_CINPOTTY` / `_5_05_CINPOTTYTARGET` (speed-set), the `PortaPoo` pool entity plays `StealthDoor/Falldown`. Then the outro: **second baked cutscene `5-05B`**, special-entity override set 4, the crashed toilet persistent entity `rc2d_PortaPoo_A` spawned at the hill bottom, Burton walks `_5_05_BURTONPOOPED`, player `Success` anim, `M_PASS` banner (3000 ms), pass stinger 4, `MissionSucceed(true, false, false)`. `bMissionPassedCreatePotty` keeps override set 4 in cleanup (failed runs restore set 1). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Zoe attacked (mission-critical) | `cbMissionCritFailure` (also makes her ambient and deletes Burton) | `5_05_FAILHITZOE` |
| Burton spots the player during the park half | `CbStealthBurton` (external stealth callback) | generic (no key set) |
| 90 s sabotage timer expires | `MissionTimerHasFinished` in the R3 poll | `5_05_FAILRUN` |
| Leave the park entirely | `T_ParkOut` thread (dual-zone: warn ring `_5_05_PARKOUT`, hard fail outside `_5_05_PARK`) | `5_05_LEFTPARK` |
| Burton leaves the toilet before the ram | `drvBurtonLeavePotty` (external event) | `5_05_FAILPOT` |
| 30 s ram timer expires | `MissionTimerHasFinished` in `F_FinalPottyPushCheck` | generic (camera beat on Burton) |

Fail presentation: fail stinger 4; `5_05_FAILPOT` gets `MissionFail(true, true, key)` + a 3 s hold, others `MissionFail(false, true, key)`. Retry after the midpoint resumes at the park via `shared.g5_05` (§1) — the cutters/Zoe half is not replayed.

### Pass-side effects

- In-file only: `M_PASS` banner, stinger 4, second baked cutscene `5-05B`, crashed-toilet world dressing (`rc2d_PortaPoo_A` + special-entity set 4), item 487 (cutters) removed from the player on cleanup, Zoe's unique model restored.
- **No `SMissPass.lua` branch for `5_05`** (**VERIFIED**) — no respect/money/item grants. (Fan wikis list Zoe-related unlocks around this point of the story; nothing is granted here — **OPEN**, likely handled by mission-availability logic elsewhere.)

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | R0 intro | baked (`5-05`, skipped on retry via `shared.g5_05`) | `fade` in + `letterbox` + one `say` beat |
| CS-2 | R2 hand over the cutters | in-engine, ~15 s | `give_item_scene` (1_S01/1_G1: give/receive pair anim) + 5 `say` beats + 2 `camera` setups with FOV cuts |
| CS-3 | R4 Burton's dash / Zoe cuts the fence | in-engine, ~12 s | `inline_cutscene_chain`: `camera` on Burton + pee-dance `ped_action_node` + flee path + persistent-entity delete (fence rail) |
| CS-4 | R6 toilet falls | in-engine, ~4 s | `camera_rail_pair` (`CameraSetPath` + `CameraLookAtPath` with speed) + `prop_set_state` (`Falldown`) + preloaded one-shot stream |
| CS-5 | R6 outro | baked (`5-05B`) + in-engine tail | baked cutscene + fixed `camera` + solo player anim + `completion_banner` |

Music: `MS_StealthHigh.rsm` (setup), `MS_RunningLow/High.rsm` interactive (stages 1–2), `MS_MisbehavingHigh.rsm` (park), `MS_FunLow/Mid/High.rsm` (finale), stinger id 4 both ways.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 48 | Zoe | unique-model suppressed for the mission; mission-critical (hurt = fail); carries cutters (412) after the fence cut |
| 55 | Burton | jogging patrol on a tethered loop with infinite sprint; invulnerable + stimuli-ignored during the finale; cleaned up to ambient wander or deleted |
| 141 | park dog | spawned at `_5_05_PPDOG`; attack hook `drvDogAttack` (external); exported as `shared.idDog` |
| 123 | worker | in the loaded model table; used only by 5_06's dead path (sibling file) — here likely for guard/ambient dressing, unreferenced in live code (**OPEN**) |

### Props / world systems

- PAnim `PortaPoo` (`Act/Props/StealthDoor.act`): states `Unoccupied` / `Occupied` / `NotUseable/Damaged` / `Mission505/Idle` / `Falldown`; prop health drives the destroyed state. One pool entity + three coordinate-addressed instances in `tblPotties` + the finale `rc2d_PortaPoo_A`.
- Persistent entities: `PortaPoo` (hill toilet), `pxCrappy`, `RI1d_railChunk1` (fence), `rc2d_PortaPoo_A` (post-pass dressing). Area special-entity overrides: set 2 at mission start, set 4 after pass, set 1 on cleanup if not passed (`AreaEnsureSpecialEntitiesAreCreatedWithOverride("5_05", n)`).
- Vehicles: mower 284 (ram vehicle, `while not VehicleRequestModel(284)` gate), bikes 273/284 in the load table; broad vehicle purge helpers (`F_ClearVehiclesXYZ`, `F_DeleteUnusedVehicles`).
- Pickups/items: 487 bolt cutters (pickup → inventory item), 412 as Zoe's prop weapon.
- Speech: `M_5_05` ids 3–7 (NIS), 9, 11, 13, 20; 2D `PortaCrash`; preloaded streams for the two NIS payoffs.

### Named world data to extract from `5_05.DAT` (present — `DATLoad("5_05.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_5_05_PLAYERSPAWN`, `_5_05_BURTONWARP`, `_5_05_BOLTCUTTERS`, `_5_05_ZOESTART`, `_5_05_ZOEPOTTY`, `_5_05_BURTONSTART`, `_5_05_PPDOG`, `_5_05_MOWER`, `_5_05_CHECKPOT1/2`, `_5_05_PPFINAL`, `_5_05_BURTONPOOPED`, `_5_05_PLAYERAFTERPUSH`.
- PATHs: `_5_05_MEAT01-06` (dead guard paths), `_5_05_BURTONMAINPATH`, `_5_05_ZOEHIDE`, `_5_05_ZOECUT`, `_5_05_ZOERUNAWAY`, `_5_05_BURTONRUNLAST`, `_5_05_CHECKPOT1`, `_5_05_CINPOTTY`, `_5_05_CINPOTTYTARGET`, `_5_05_BURTONCINPOTTY`, `_5_05_BURTONCINEMATIC1A/2A/3A`, `_5_05_BURTONPOOPED`.
- TRIGGERs: `_5_05_ALLEY`, `_AMB_RICH_AREA`, `_5_05_FACTORYAREA`, `_5_05_BURTONTETHER`, `_5_05_PARK`, `_5_05_PARKOUT`, `_5_05_SPRINKLERTRIG01-03`, `_5_05_ZOETRIGGERPARK`, `_5_05_RUNAWAYZOE`, `_5_05_LASTPOTTY`, `_5_05_CLEARMOWER`.
- Hardcoded coords in-file: the three `tblPotties` toilets, all persistent-entity spawns, NIS cameras (FOV 70/40/80 rigs).
- Area codes seen: **0** only (all exterior: industrial area → rich area → park).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1–ch4 set | New primitives needed |
|---|---|---|
| R0 setup dressing | `special_entity_set_load` / `special_entities_load` (ch3), `persistent_fade_entity` | none new |
| R1 pickup fetch | `pickup_spawner`, `give_item` | none new |
| R1/R3 mid-mission retry | `script_checkpoint` (2_04) — same "skip intro + early stages on retry" role, but keyed on a `shared.*` session flag instead of saved data | none new (variant note on `script_checkpoint`: session-scoped shared flag, survives fail/retry, never cleared on cleanup) |
| R2 hand-off NIS | `give_item_scene` (1_S01), `mission_critical_npc`, `inline_cutscene_chain` | none new |
| R2 tethered jogger | `npc_patrol_cycle` (2_08) + `npc_tether_trigger` + `npc_stat_override` (infinite sprint) | none new |
| R3 toilet sabotage | `destructible_prop_objective` (damage detection), `countdown_fail_timer` (3_01A), `blip_on_prop` | **prop_state_blip_table** (N coordinate-addressed world props each running open/occupied/destroyed/blocked states: blip shown only while open, occupancy/destroyed read from prop anims + health, one state's block flag driven externally — sprinklers; stage completes when no prop remains open) |
| R3 park confinement | `leave_area_fail` (1_02B) — but two-zone | none new (warn ring + fail ring = two nested zones; `zone_exit_countdown`-adjacent, data) |
| R4 staging NIS | `vehicle_area_purge` (2_07), `inline_cutscene_chain`, `world_state_swap` (fence rail delete) | none new |
| R5 mower ram | `player_mounted` (2_04), `blip_on_vehicle` | **mounted_model_trigger** (condition variant of `trigger_enter`: true only while the player is inside the trigger *driving a specific vehicle model* — the ram check; also drives the blip ping-pong between vehicle and target) |
| R5/R6 fails | `countdown_fail_timer`, `mission_critical_npc`, `spotted_fail_trigger` (Burton spot, external) | none new |
| R6 payoff | `camera_rail_pair` (ch2), `prop_set_state`, `completion_banner` | none new |
| stealth guards (dead) | `stealth_spotter` (1_04) / `vision_cone_eject_loop` (2_S06) | none new — document only; do not build |

## 6. Open questions

- Producer of `shared.gParkSprinklers` (park sprinkler switch — area script or DAT event?) and how the `doggy` toilet is meant to be neutralized (dog-related, per the name + `drvDogAttack`; **INFERRED**).
- Whether the dead guard layer (`_5_05_MEAT01-06`, `F_CreateGuard`) was a cut industrial-area stealth segment or is DAT-driven; confirm during area-script survey. `CbStealthBurton` is live logic with no in-file registration — find its binder.
- `PedSetUniqueModelStatus(48, -1)` semantics vs the `nMaxZoeUnique` save/restore (unique-model spawn caps).
- `PedOverrideSocialResponseToStimulus(idZoe, 28, 4)` stimulus-slot meaning.
- HUD component 20 toggles (vehicle HUD? force-shown on mower dismount).
- `PickupCreatePoint(..., "PermanentButes")` respawn-list semantics.
- Whether `shared.g5_05` persisting past the *session* (save/load) is intended — the shipped behavior of refailing after the midpoint.
- The `T_ParkOut` warning re-printed at 0.1 s — original spam or decompile timing artifact.
- All trigger/point geometry (pending DAT extraction); the `drv*` event bindings presumably live in `5_05.DAT` or the park area script.
