# 5_02 — Preppies Vandalized: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_02.lua` (2620 lines, complete; ~1100 of them are the destroyable-prop registry) + cross-references in `SFaction.lua`, `AreaScripts/SchoolHallways.lua`, `AreaScripts/Boxing.lua`, `SGlFunc.lua`, `main.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_02 = "Preppies Vandalized"** (canonical Chapter 5 list, #61 in `docs/story-mode-catalog.md`, 8AM–11PM) — **VERIFIED**: the [Bully wiki Chapter V page](https://bully.fandom.com/wiki/Chapter_V) lists "Preppies Vandalized `5-02`" by script id; the [Preppies Vandalized wiki entry](https://bully.fandom.com/wiki/Preppies_Vandalized) matches the script beat-for-beat (interrogate greasers about the stolen boxing trophies → tail to the warehouse → stealth-photograph townies packing rat crates → photograph the trophy bonfire at the docks → report back to Derby at the boxing club; anim group `5_02PrVandalized` seals it). Pass block matches: `MinigameSetCompletion("M_PASS", true, 3000)` ($30), respect messages `MRESPECT_PP15` / `MRESPECT_DM25` with `SetFactionRespect(5, +15)` / `SetFactionRespect(3, −25)` (delta form, `respect_delta`). Clique decode: faction 5 = preppies (**VERIFIED** in-file — `PedSetTypeToTypeAttitude(5, 13, 2)` makes preppies hostile for the mission duration), 3 = dropouts/townies, 13 = player.

Setting: greaser hangout (area 0, New Coventry side), warehouse interior (area 54), the industrial docks, and the boxing club in Old Bullworth Vale. Downstream (**VERIFIED** in the clone):

- `AreaScripts/SchoolHallways.lua:623`: after 5_02, the school bulletin board uses its chapter-5-late sign variant (`BulletinChapter5_3`).
- `AreaScripts/Boxing.lua:64`: while 5_02 is active (or 5_01 done), the trophy case spawns busted/empty.
- `SFaction.lua`: unique models 33/34/212 suppressed while 5_02 is available; 33/34 restored on completion.
- `SGlFunc.lua:807` / `main.lua:983`: post-5_02 + pre-6_01 world-state branches exist (the "Go See the Principal" chain — the expulsion beat is gated on 5_02 being done).

## 2. Mission flow

Driver: an explicit stage state machine — `gMissionStage` holds a function ref; `main()` calls it every tick. Stages: `Stage01_FindGreaser` → `Stage02_GetToWareHouse` → `Stage03_FindRatCrates` → `Stage04_PhotographRatCrates` → `Stage_LeaveWarehouse` → `Stage05_GoToDocks` → `Stage06_FindTrophies` → `Stage07_TakeDockPhoto` → `Stage08_ReturnToBoxingClub` → `Stage09_OutroCS`. Fail states are keyed (`5_02_FAIL01/02/03`, `5_02_20`); there is no player-KO-specific handling beyond the engine default. Interactive music: running-low/running-mid/fighting-dropouts set at setup; detention + stealth sets swapped in per stage.

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup** | DAT + anim groups (`5_02PrVandalized`, `NPC_Love`, `Hang_Moshing`, `LE_Orderly`, cheer/dodgeball/smoking sets, `NIS_5_02`); models: greasers 27/21/29, dropouts 42/41/43/44/45, preps 37/33, rat 136 (+425/346); weapons 311/339; preppies→player attitude forced hostile (snapshot/restore); docks population trigger deactivated, mission depopulation trigger activated. Baked intro `5-02`. |
| S1 | **Shake down the greasers** | Blip to the hangout; three greasers spawned with attacked-event handlers. Reaching the hangout *or* hitting them starts the interrogation NIS — if they were attacked, they are deleted and **respawned fresh** for the scene (`npc_respawn_fresh`). NIS: confrontation music, player teleported to the NIS mark, weapons cleared on the lead greaser, five skippable lines (player 10/12/8, greaser 25/26) alternating between two fixed cameras at FOV 30. Wiki-consistent outcome: the greasers point at the townies/dropouts (**INFERRED** — content of lines not read). |
| S2 | **Get to the warehouse** | Objective + blip swap; stage advances when `AreaGetVisible() == 54` (warehouse interior); music swaps to the detention set. |
| S3 | **Find the rat crates (stealth)** | Persistent prop `RatCrateWH` created at a hardcoded point (−589.7, −166.9, 7.1) + blip. Ambient rats spawned (136, faction 2 — jocks id reused as "vermin" faction, **INFERRED**, tethered). Two patrol dropouts (Otto 42, Jerry 41) follow patrol paths with stealth sight callbacks (bark only). Two "rat packers" (Leon 43, Duncan 44) stand stationary at the crates (Duncan loops the packing anim) with hit callbacks. The giant destroyable-prop registry (~250 named `iware_*` props across warehouse/prep-estate/docks families) is hash-registered: any **2 props broken → patrol goes aggro** (noise budget `MAX_PROP_DAMAGED_ALLOWED = 2`). Hitting a packer → patrol + packers aggro and **fail `5_02_FAIL03`**. |
| S4 | **Photograph the rat packing** | At `_5_02_STARTRATPHOTO`: packers become stealth-aware (sight callback → spotted flag), and an ambient dialog thread loops the packers' work chatter (lines 16–21 once, then a 22/23 couplet every ~8 s, up to 5 repeats, cancelled on any alert). Success = a **valid photo**: rat-packer group member(s) in frame (`PhotoIsGood(nil, gRatPackers)` — prop check nil, ped check live + sticky in-frame latch, `PhotoSetValid` drives the HUD frame indicator). Fail routes: spotted by packers (`5_02_20`), patrol followed the player upstairs (same), props budget blown (same), packers attacked (`5_02_FAIL03`). |
| S5 | **Packers leave for the party (NIS)** | Letterbox; Leon line 24; Duncan's stand-up node plays out; both path out of the warehouse and are **warped to curbside points on path end** (thread `T_MoveTheFuckersOutside`, then re-flagged stealth). Player control returns; objective/blip → exit the warehouse (stage ends when area 54 left). |
| S6 | **Tail to the docks** | Packers path to the docks (or go ambient if their warp never landed). Approaching the docks: vehicles suppressed (`VehicleOverrideAmbient(0,0,0,0)`), ambient population swapped to the mission trigger. At the party-create trigger: radar clamped to 30 m; party dropouts created — a chatting pair (`PedSetWantsToSocializeWithPed` both ways) plus a fire-barrel watcher (stealth: on sight → bark + attack); the **trophy pile** persistent prop + blip created at (353.6, −240.3, 2.9). |
| S7 | **Bonfire + the evidence photo** | At either bonfire trigger: setup NIS — party peds slept, three bonfire peds created (gas-pourer 45, torch-smoker 44, observer 42), 3D gas-pour loop plays ~5 s. `T_BonfireStarter` then auto-lights the fire (torch-stub anim → `BonfireStart`). `BonfireStart`: three `FireCreate`s on flame triggers (`GymFire` type, scale 1, 0.5 damage radius), per-flame smoke effect (40% grey / 60% black roll) + one big smoke stack, `TrophyFire` 3D loop; party + packers walk to watch points and mosh. Objective = the photo: `PhotoIsGood(gTrophyPile, gBonfireDropouts)` — **trophy pile in frame AND ≥1 live bonfire dropout in frame**. Fails: stepping onto the barge (`PlayerOnBarge` → everyone aggros, bonfire ignites, `5_02_FAIL01`); leaving the docks (outside `_5_02_GAMEAREA` → warning text; outside `_5_02_FAILTRIGGER` → `5_02_FAIL02`); hitting a bonfire ped (bonfire starts early, they pull weapon 311 and rush the player, `5_02_FAIL01`). |
| S8 | **Report back** | Objective + blip: Derby (37) + a second prep (33) spawned at the boxing club with hit callbacks (hitting them is a shortcut into the outro, not a fail). Leaving the port restores population/vehicles and swaps to the stealth music set. Reaching the final-NIS trigger (or the prep-hit shortcut) → pass. |
| S9 | **Outro + pass** | Fade, player dismounted from any bike, baked cutscene `5-02B`, player teleported to `_5_02_PLAYERENDPOINT`; both preps deleted and respawned at walk-off marks and walked off under a fixed camera; `PlayerRecordAttackTime(2000)`. Then `M_PASS` + $30 + `MRESPECT_PP15` / `MRESPECT_DM25` + preppies +15 / dropouts −25; `MissionSucceed(true, false, false)` (save prompt, **INFERRED**). |

### Fail conditions and retry

Keyed: `5_02_FAIL01` (barge / bonfire-ped violence at the docks), `5_02_FAIL02` (leaving the docks area), `5_02_FAIL03` (attacking the rat packers), `5_02_20` (spotted while photographing the packing). Generic: player KO. No checkpoints — any fail replays from S0; the stage-machine cleanup (population, vehicles, preppie attitude, persistent props, bonfire fires/effects, hash handlers) is fully unwound in `MissionCleanup()`. Retry-safety note: the greaser attacked-flag is not reset in-file, so a retry after S1 combat still plays the NIS correctly via the respawn-fresh path (**INFERRED**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro | baked (`5-02`) | `fade` + `letterbox` + `say` |
| CS-2 | S1 greaser interrogation | in-engine (2 fixed cameras, 5 skippable lines) | `skippable_scene_chain` + `camera` + `speech_sequence` |
| CS-3 | S5 packers leave | in-engine (1 fixed camera, 1 line, stand-up node, path-out) | `letterbox` + `ped_action_node` + `npc_follow_path` |
| CS-4 | S7 bonfire setup | in-engine (1 fixed camera, gas-pour audio, ~5 s) | `letterbox` + `camera` + `npc_asleep_staging` |
| CS-5 | S9 report + walk-off | baked (`5-02B`) + in-engine walk-off | `fade` + `npc_move_to` + `camera` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 27, 21, 29 | greasers ×3 | S1 interrogation; respawned fresh if pre-fought |
| 42 (Otto), 41 (Jerry) | warehouse patrol | path patrol + sight-bark stealth; aggro on noise budget |
| 43 (Leon), 44 (Duncan) | rat packers | stationary; photo subjects S4; escorted out S5, to docks S6 |
| 136 | rats ×N | warehouse ambience (faction 2, tethered) |
| 45 (gas), 44 (torch), 42 (observer) | bonfire crew | S7 photo subjects; weapon 311 when provoked |
| random dropout (42–45 pool) | party ×3 | chat pair + fire-barrel stealth watcher |
| 37 (Derby), 33 | boxing club | S8 objective targets; walk-off in outro |

### Animation groups / action trees / AI

`5_02PrVandalized`, `NPC_Love`, `NPC_ADULT`, `Hang_Moshing`, `LE_Orderly`, `Cheer_Cool1/2`, `Cheer_Gen3`, `DodgeBall`, `POI_Smoking`, `IDLE_DOUT_C`, `NIS_5_02`; `Act/Conv/5_02.act` (Player01–03, Vance01/02, RatPacking set, GasolinePour start/stop, StandingSmoke/StubItOut, ObserverLoops, MoshOut, StepBackMoshOut). Photo API set: `PhotoTargetInFrame` (point or ped), `PhotoSetValid`, `PhotoHasBeenTaken`, plus the file's own sticky-latch wrapper.

### Props / pickups / weapons

- Persistent simple-entity props: `RatCrateWH` (warehouse), `5_02_inTrophyPile` (docks) — `CreatePersistentEntity`, deleted in cleanup.
- Bonfire: 3 flame-trigger fires (`GymFire` profile) + random-palette smoke stacks + big smoke; trophy pile PAnim deleted with the bonfire.
- Destroyable registry: ~250 `iware_*` props (warehouse/prep-estate/docks families) hash-registered to event 3 with a shared break counter — data payload for the engine port (name-hash → breakable event subscription).
- Weapons loaded: 311 (bonfire crew retaliation), 339.

### Speech

`M_5_02` scripted ids: 8, 10, 12, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26 — **14 scripted lines** (16–23 are the packers' ambient work loop, cancellable); ambient `CHASE` barks on stealth sight. Music: `MS_Confrontation_NIS.rsm`, running set (`MS_RunningLow02`/`MS_RunningMid`/`MS_FightingDropouts`), `MS_PunishmentDetention.rsm`, stealth set (`MS_StealthLow/Mid/High`); pass/fail stingers (track 10).

### Named world data to extract from `5_02.dat`

- POINTLISTs: `_5_02_PLAYER_START`, `_5_02_GREASERBLIP`, `_5_02_GREASERS`, `_5_02_PLAYERGREASERNS`, `_5_02_WAREHOUSEBLIP`, `_5_02_RATS`, `_5_02AMBIENTRATS`, `_5_02RATTETHER`, `_5_02_RATPATROL`, `_5_02_WHDROPOUTS`, `_5_02_WHDROPOUTS02`, `_5_02_PLAYERAFTERRPNIS`, `_5_02_DOPARTIERS02`, `_5_02_DOFIREBARREL01`, `_5_02_BONFIREDROPOUTS`, `_5_02_BONFIREOBSERVERS`, `_5_02_DARBYBOXING`, `_5_02_WALKOFFBEG`, `_5_02_WALKOFF`, `_5_02_PLAYERENDPOINT`, `_5_02_PORTBLIP`.
- TRIGGERs: `_5_02_GREASERHANGOUT`, `_5_02_STARTRATPHOTO`, `_5_02_WAREHOUSEUPSTAIRS`, `_5_02_NEARNIS`, `_5_02_DEPOPULATEDOCKS`, `_INDUSTRIAL_DOCKS`, `_5_02_DOCKPARTYCREATE`, `_5_02_BONFIRETRIGGER01`, `_5_02_BonfireTrigger02`, `_5_02_ONBARGE`, `_5_02_GAMEAREA`, `_5_02_FAILTRIGGER`, `_5_02_PORTEXIT`, `_5_02_FINALNIS`, `_5_02_FLAME01/02/03`, `_5_02_BIGSMOKE`, `_5_02_TROPHYPILE`.
- PATHs: `_5_02RATPATROLA/B`, `_5_02_LEONEXITPATH`, `_5_02_DUNCANEXITPATH`, `_5_02_DOCKLEONPATH`, `_5_02_DOCKDUNCANPATH`.
- Hardcoded: rat-crate point (−589.739, −166.915, 7.06956), trophy-pile point (353.588, −240.294, 2.92264), bonfire audio origin (352.267, −240.083, 2.477), 4 fixed camera pairs.
- Area codes seen: **0**, **54** (warehouse).
- **Trigger data (DAT): YES** — `DATLoad("5_02.DAT", 2)`; original lives in `Trigger.img` per `trigger-format.md`. Not extracted here (by design).

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `skippable_scene_chain` (S1 per-line skip), `speech_sequence`, `npc_respawn_fresh` (S1 pre-fought greasers), `npc_follow_path` + path-end callbacks (S5 exit/warp, S6 dock walk), `stealth_spotter` + `npc_patrol_cycle` (S3/S4 patrols, sight callbacks), `object_destroyed_noise_alert` (2_08 — S3/S4 prop watch; **variant: 2-break allowance before aggro**, a budget counter rather than first-break), `violence_fail_watch` (packers/bonfire peds hit), `photo_evidence_stage` + `photo_capture_condition` (3_S03/3_01 — sticky in-frame latch per 4_01's parameter variant), `bark_dialog_loop` (packer work chatter with cancel-on-alert), `socializing_guard_pair` (dock chat pair), `npc_asleep_staging` (S7 NIS sleep), `fire_prop_system` (2_R04) + `effect_spawn_at_points` (3_S11) (bonfire + smoke), `persistent_fade_entity` (`CreatePersistentEntity` crate/trophy pile), `population_trigger_toggle` + `ambient_vehicle_override` (dock depopulation), `radar_set_range`, `leave_area_fail` (**variant: two-tier — warn zone then fail zone**), `faction_attitude_matrix_override` (prep hostility snapshot/restore), `respect_delta`, `completion_banner`, `dynamic_blip`, `weapon_strip` (greaser weapon clear for the NIS).

| Beat | Covered by | New primitives needed |
|---|---|---|
| S4/S7 evidence photos | `photo_evidence_stage`, `photo_capture_condition` | **photo_multi_subject**: valid-shot = named prop/point in frame AND ≥1 live ped from a required group in frame (per-ped sticky latch; `PhotoSetValid` HUD flag while all requirements are met). Extends 3_S03's single-ped photo |
| S3 noise budget | `object_destroyed_noise_alert` | none (budget variant: N breaks before aggro — add a count field) |
| S7 leave-docks fail | `leave_area_fail` | none (two-tier variant: inner warn trigger + outer fail trigger) |
| S7 barge fail | zone condition + aggro + fail composition | none |
| S5 path-end warp | `npc_follow_path` + callback | none (variant: on-last-node warp-and-restation instead of despawn) |
| S6/S7 fire + smoke | `fire_prop_system`, `effect_spawn_at_points` | none |

Genuinely NEW names introduced here: **photo_multi_subject**. (The ~250-entry destroyable registry is a data-ingest note for the engine port, not a mission primitive; `PlayerRecordAttackTime` is an engine call note.)

## 6. Open questions

- `PhotoHasBeenTaken`/`PhotoVerifyGoodShot` semantics: the sticky `bWasInFrame` latch means a subject visible in the previous check still counts — is that a one-frame tolerance or a permanent latch for the stage? The code only ever or's it in; reads as permanent-per-stage. **OPEN** (affects remake photo UX).
- Weapon ids 311/339 decode (311 = the bonfire crew's retaliation weapon; fireworks family?, **INFERRED** only) — pending `default.ide` cross-check. **OPEN**.
- Rats spawned with `PedSetFaction(rat, 2)` — faction 2 is jocks in the ch4 decode; here it presumably just keeps vermin neutral to everyone. Confirm faction-2 side effects (jock NPCs ignoring rats?) before copying. **OPEN**.
- `RatPatrolFollowedPlayer()` duplicates each ped/trigger check twice (copy-paste bug, harmless). The `RegisterHitCallback(gLeon, ...)` in `RatPackersCreate` is registered twice and Duncan never gets one directly — intended asymmetry or bug? **OPEN** (affects whether Duncan can be attacked without failing).
- The S1 attacked-greasers path: `bGreasersAttacked` is set by any of the three greasers' event-0 handler — which engine event is id 0 (hit? aggro?)? Assumed "ped hit/aggroed" per usage; verify against the event table. **OPEN**.
- `MissionEndFollowCamera`-style dead writes seen in 5_01 don't appear here; `gObserver` initializers (`nil, nil`) and other dead assignments suggest trimmed features (a cut observer role?). **OPEN** (cosmetic).
