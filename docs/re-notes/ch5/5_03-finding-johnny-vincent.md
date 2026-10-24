# 5_03 — Finding Johnny Vincent: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/5_03.lua` (1653 lines, complete) + cross-references in `SFaction.lua`, `AreaScripts/Asylum.lua`, `AreaScripts/Bdorm.lua`, `SInitGl.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**5_03 = "Finding Johnny Vincent"** (canonical Chapter 5 list, #57 in `docs/story-mode-catalog.md`, 8AM–11PM) — **VERIFIED**: the [Bully wiki Chapter V page](https://bully.fandom.com/wiki/Chapter_V) lists "Finding Johnny Vincent `5-03`" by script id; the [Finding Johnny Vincent wiki entry](https://bully.fandom.com/wiki/Finding_Johnny_Vincent) matches the script beat-for-beat (Lola and Norton point Jimmy at the asylum; sneak past the orderlies; find Johnny in his cell; steal an orderly uniform from the laundry; con the control-room orderly into walking off; throw the master switch; escort Johnny out through the riot). Pass block matches: `MinigameSetCompletion("M_PASS", true, 3000)` ($30), respect message `MRESPECT_GP25`, `SetFactionRespect(4, GetFactionRespect(4) + 25)` (delta form, `respect_delta`), and `ClothingGivePlayerOutfit("Orderly")` — the permanent orderly-uniform unlock the wiki lists. Clique decode: faction 4 = greasers, 0 = authority/orderlies, 3 = patients-as-dropouts, 9 = mission-ally faction, 13 = player (4/9/13 **VERIFIED** by in-file use; 0/3 **INFERRED** from attitude rows).

Setting: starts in the open world (area 0) at Lola/Norton's mark, then the asylum grounds and the asylum interior (area 38). Downstream (**VERIFIED** in the clone):

- `AreaScripts/Bdorm.lua:178`: once 5_03 is complete, dorm trophy #17 exists (the orderly uniform mounted on the wall — room-trophy count +1).
- `SFaction.lua`: unique models 25/96/29/201 suppressed while 5_03 is available (25 = Lola, 29 = Norton — the mission also suppresses them itself); 25/29 restored on completion. Greasers' default attitude to the player becomes 2 after 5_03 (until `6_B` later sets 4).
- `AreaScripts/Asylum.lua`: ambient asylum orderlies and door locks load only when `shared.bAsylumPatrols` is true; 5_03 clears that flag at setup and restores it on cleanup (`area_script_shared_state`).
- `SInitGl.lua:206` declares `shared.g5_03_A_Passed = false` — never written anywhere in the clone (**OPEN**, vestigial).

## 2. Mission flow

Driver: `MissionSetup()` (no fade-in, all peds cleared, baked intro `5-03`, `DATLoad("5_03.DAT", 2)`, teleport to `_5_03_PLAYERSTART1`) → `main()` runs four blocking phases in sequence: `F_GetToAsylum` → `F_GetInAsylum` → `F_GetToJohnny` → `F_GetOut`. The bust/eject cycle (`F_PlayerSpotted` + `F_EjectPlayerFromAsylum`) is a soft-fail loop inside phases 2–3, not a mission fail. Keyed fails: `5_03_AREAFAIL` (out-of-bounds), `5_03_JOHNNYKO` (escort target down). Interactive music: running set outside, `MS_SearchingLow.rsm` locked inside, action set once disguised.

| # | Beat | Logic (functional) |
|---|---|---|
| S0 | **Setup + intro** | Ped models 53/158/125/23/150/154/129/25/29/153 loaded; `Act/Conv/5_03.act`, `Crazy_Basic.act`, groups `Hang_Talking`/`AsyBars`/`G_Striker`/`F_CRAZY`; asylum front gate locked; POI disabled; Lola (25) + Norton (29) unique statuses snapshotted and suppressed; `shared.bAsylumPatrols = false`; clothing backed up. Lola and Norton are spawned at intro marks and **deleted when the player reaches the asylum grounds** (intro dressing only). Greasers→player attitude forced to 3 for the mission. |
| S1 | **Get to the asylum** (`F_GetToAsylum`) | Objective `5_03_94` + tree blip; crossing `_5_03_ASYLUM_GROUNDS_SETUP` deletes Lola/Norton, unlocks the asylum front door, and spawns the two grounds orderlies (53/158) on patrol paths with vision-range override (stat 3 = 8 m) and stealth-sight callbacks. On-grounds trigger swaps the blip to the door. |
| S2 | **Bust / eject loop** | Any orderly sighting (`cbNull` sets spotted) or hitting an orderly (global handler 7, `F_CheckOrderlyHit`) while the player lacks the outfit → `F_PlayerSpotted`: letterbox-style vignette, the spotter locks on, plays the `LE_Orderly_A/Busted` node and a busted bark (line 38 for model 53, 99 otherwise) under an offset camera; then `F_EjectPlayerFromAsylum`: fade, blips/objectives rolled back, all interior peds deleted, teleport to `_5_03_FAILURE` outside, grounds orderlies re-created, punishment points zeroed, all setup flags reset (`bBustedPlayer` marks "has been inside" so re-entry re-runs a shorter setup). Infinite retries; this is the stealth fail loop, **not** a mission fail. |
| S3 | **Enter the asylum** (`F_GetInAsylum` → area 38) | First entry plays the entering NIS: front door opened, player teleported to the crouch mark with the sneak flag set (`PedSetFlag(gPlayer, 2, true)`), front-desk orderly spawned on the front patrol path, one fixed camera until the orderly hits path node 1. Then the **door lock matrix** (~30 `AreaSetDoorLocked`/`...ToPeds` calls: cells 12–21 locked, escape door locked, rec-room doors configured), five `ASY_AlarmLight*_OFF` persistent props created, and the interior crew spawned: front orderly, two block-A orderlies (158/53), and — on repeat entries after the talk beat — patrol variants. Objective `5_03_37` (find Johnny) + Johnny blip. `PunishersRespondToPlayerOnly(true)`. |
| S4 | **Block A color** | Approaching `_5_03_ORDERLY_CS1_1` makes the two block-A orderlies stop, face each other, and chat (lines 2/4) — interruptible by a bust; afterwards one goes ambient on a side path, the other resumes the block patrol with a **look-into-rooms behavior** (thread `T_F1_O1_To_Look`: at the hall trigger, stop, face patient-room points, wait 1–5 s, resume at path nodes 2/6/10). Once the talk happened, a **sleeping orderly** spawns in the rec room (vision 0, sleep node); if the player comes within ~8 m while he's alerted, he wakes (stat overrides restored) and starts patrolling. |
| S5 | **B block: find Johnny** | At `_5_03_GEN_JOHNNY`: B-wing orderly, shower-patrol orderly, station orderly (vision 6), stationary control-room orderly (vision 6), Johnny (23) spawned in his cell, and four patients (Fenwick 125, thugs 154/150/153) spawned cheap with flags 117/108 (render/behavior trims, **INFERRED**) and ignore-lists against the orderlies. A yelling thread barks Johnny line 5 every 15 s and re-blips him. Reaching `_5_03_JOHNNY_TRIG1` → found-Johnny NIS (5 lines, 6–10, fixed camera), then objective `5_03_39` + laundry-room blip, and **pickup 510 spawned** in the laundry (`PermanentButes`). |
| S6 | **The disguise** | `ItemGetCurrentNum(510) == 1` → outfit sequence: fade, clothing backup, `ClothingSetPlayerOutfit("Orderly")` + rebuild, punishment points zeroed, **all stealth peds de-flagged** (the disguise neutralizes every spotter), attitudes rewritten (orderlies tolerate the "orderly" player), music swaps to the action set. The control-room orderly is respawned on faction 9 with a **greet-only social rig**: greeting override set 18 loaded, social action 27 disabled against him, stimulus 10 → response 18, and a player-greet callback registered. Objective `5_03_TALK` + talk-zone blip. Attacking any orderly now (`F_CheckHit`) flips orderlies→player to hostile — cover blown (no fail, just combat). |
| S7 | **The switch + the riot** (`F_Open_Cell_Doors2`) | Greeting the control orderly (proximity + greet callback) sends him walking the control-room path (door-node callback re-paths him inside); the player gets objective `5_03_OPENDOOR` + a switch blip, and the FM doors unlock. When the player stands near the switch while the `AsySwtch` prop's active node plays: punishment system disabled globally, alarm lights swapped OFF→ON (persistent-prop swap), 2D alarm loop on, cell doors 19 + 17/18/20/21 opened and pinned, patients released (cheap flags off, `Crazy_Basic` AI trees, infinite sprint, tethered to the block, aggro stats 362/100) with the orderlies re-tethered to fight them; Johnny's cell opens, he gets health 500, the `G_Striker` fight tree, a **health bar** (`5_03_025` label), blip, and a scripted two-hop move out of the cell; objective `5_03_024` (get him out). Interior orderlies go ambient — the asylum belongs to the riot now. |
| S8 | **Escort out** (`F_GetOut`) | Johnny-KO watch → fail `5_03_JOHNNYKO` (M_FAIL banner variant). Player near Johnny → line 24, fake-exit blip, Johnny moves to `_5_03_JOHNNY_GOTO1`; orderlies attack patients. Johnny at GOTO1 → doors made ped-pathable; Johnny follows his exit path; the asylum back exit locks. Player in the rec-room trigger → **door-close NIS**: ally dismissed, both teleported to the NIS door marks, B-block peds nuked, rec doors closed/locked, bars `Close5_03` choreography ×2 then `CloseBreak` on the main bar, crazy-door prop opened; then Johnny is **recruited as an ally** (health bar re-shown), FM doors 07/08 opened for peds, exit blip + objective `5_03_039`, attitudes settled (patients/orderlies/greasers neutral to player). |
| S9 | **Outro + pass** | Player at the exit point → alarm off, transition to area 0 (`_5_03_PLAYER_END`), Johnny deleted and respawned fresh, speech system switched to cutscene mode; six-line outro (Johnny 32/34/36, player 33/35/37) over `5_03_NIS_Outro` nodes with three camera setups; Johnny's AI reset to ambient, infinite sprint, runs off to a hardcoded point. `M_PASS` + $30 + `MRESPECT_GP25` + greasers +25; `ClothingGivePlayerOutfit("Orderly")` (permanent unlock); `MissionSucceed(true, false, false)` (save prompt, **INFERRED**). |

### Fail conditions and retry

- Johnny reduced to 0 health after being freed → `5_03_JOHNNYKO` (with `M_FAIL` banner).
- Out-of-bounds: `_5_03_LEAVING` trigger → "get back" warning text; `_5_03_GAMEOVER` / `_5_03_LEFT` → `5_03_AREAFAIL`.
- Player KO → generic engine fail.
- Busts (S2) are the soft-fail loop, not fails. On any real fail after the disguise, cleanup strips pickup 510 and restores the player's backed-up clothes; alarm-light ON props are deleted only if the riot started; `DisablePunishmentSystem(false)`, `bAsylumPatrols = true`, unique models restored, POI re-enabled. No checkpoints: retry replays from S0. `MissionResetRespawnOverrides` runs on cleanup (respawn overrides set nowhere in-file — **OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | intro (Lola/Norton) | baked (`5-03`, extra args `true, true, true`) | `fade` + `letterbox` + `say` |
| CS-2 | S3 entering the asylum | in-engine (1 fixed camera, ends on path-node callback) | `camera` + `npc_follow_path` (callback-gated) |
| CS-3 | S2 busted vignette | in-engine (offset camera from spotter, busted node + bark) | `scripted_bust_vignette` |
| CS-4 | S5 found Johnny | in-engine (1 fixed camera, 5 lines) | `letterbox` + `camera` + `speech_sequence` |
| CS-5 | S8 door-close | in-engine (4 camera setups, bars/door prop choreography) | `inline_cutscene_chain` + `door_prop_control` + `camera` |
| CS-6 | S9 outro | in-engine (3 camera setups, 6 lines, run-off) | `camera` + `speech_sequence` + `npc_move_to` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 25 (Lola), 29 (Norton) | intro dressing | deleted on arrival at the grounds; unique-suppressed |
| 53, 158 | orderlies ×N | grounds patrol ×2, front desk, block A ×2, sleeper, B wing ×2, station, control room; vision stats 6/8 m; stealth callbacks |
| 23 (Johnny) | escort target | cell NIS → freed: 500 hp, `G_Striker`, health bar, ally recruit; KO = fail |
| 125 (Fenwick), 154/150/153 | patients ×4 | cheap+flagged in cells; riot-released with `Crazy_Basic`, infinite sprint |

### Animation groups / action trees / AI

`Hang_Talking`, `AsyBars`, `G_Striker`, `F_CRAZY`, `NIS_5_03`; `Act/Conv/5_03.act` (`5_03_Stand_Talking`, `5_03_Sleeping_Orderly` sleep/wake, `5_03_Johnny_In_Cell`, `5_03_NIS_Gateclose`, `5_03_NIS_Outro`, `5_03_Crazy_Door`, `5_03_Reset`); `Act/Anim/Crazy_Basic.act`; `Act/Anim/LE_Orderly_A.act` (`Busted` node); `Act/Anim/G_Striker_A.act`; `Act/Props/AsyBars.act` (`Open` / `Close5_03` / `CloseBreak`); social override `Mission/5_03_Greeting.act` (set 18).

### Props / pickups / weapons

- Door matrix: `ASYDOORB`, `ASYDOORS11/12/13/14/15`, `CELLDOOR`, `CELLDOOR12..21`, `FMDoor02/03/07/08`, `_FMDOOR02`, `_DT_ASYLUM_FRONT_DOOR`, `_DT_ASYLUM_BACK_EXIT`, `_ESCDOORR` — lock/pathability/stay-open choreography.
- Bar props `_ASYBARS`/`_ASYBARS01`/`_ASYBARS02` (PAnim open/close nodes).
- Switch prop `AsySwtch` at (−690.115, 493.604, 2.00141) — watched via `PAnimIsPlaying(..., "/Global/AsySwtch/Active")`.
- Alarm lights: `ASY_AlarmLightA..E_OFF` → `_ON` persistent-prop swap at 5 hardcoded points.
- Pickup 510 = orderly uniform (`PermanentButes` table, laundry room) — the only confirmed item id.
- `F_Create_Fire` (five `_5_03_FIRE01..05` triggers) is defined but **never called** — cut riot fires, **OPEN**.

### Speech

`M_5_03` scripted ids: 2, 4, 5, 6, 7, 8, 9, 10, 18, 20, 22, 24, 26, 32, 33, 34, 35, 36, 37, 38, 39, 99 — **22 scripted lines**. Music: `MS_RunningLow02/MS_RunningMid`, `MS_SearchingLow.rsm` (locked), action set (`MS_ActionLow/Mid/High`), alarm loop `AsyAlrm`; pass/fail stingers (track 4).

### Named world data to extract from `5_03.dat`

- POINTLISTs: `_5_03_PLAYERSTART1`, `_5_03_LOLA`, `_5_03_NORTON`, `_5_03_TREEBLIP`, `_5_03_ASYLUM_BLIP`, `_5_03_GROUNDS_ORDERLY1/2`, `_5_03_PLAYERCROUCH`, `_5_03_CLERK_ORDERLY`, `_5_03_1STFLOOR_ORDERLY1/2`, `_5_03_1F_RECRM_ORDERLY2`, `_5_03_PATIENT_ROOM1`, `_5_03_1STFLOOR_DO3/DO4`, `_5_03_1F_BW_ORDERLY1/3`, `_5_03_STATION1_ORDERLY`, `_5_03_CR_ORDERLY_1`, `_5_03_JOHNNY_START`, `_5_03_PATIENT01/02`, `_5_03_DO_THUG02/03`, `_5_03_LAUNDRY_RM_BLIP`, `_5_03_CONTROLSWITCH`, `_5_03_JOHNNY_MOVE_FREE`, `_5_03_JOHNNY_OUT1`, `_5_03_JOHNNY_FREE1`, `_5_03_JOHNNY_GOTO1`, `_5_03_FAKE_EXIT`, `_5_03_NIS_DOORS`, `_5_03_EXIT_BLIP`, `_5_03_PLAYER_END`, `_5_03_JOHNNY_END`, `_5_03_FAILURE`, `_5_03_FRONTDESKSUPPORT`.
- TRIGGERs: `_5_03_ASYLUM_GROUNDS_SETUP`, `_5_03_ONGROUNDS`, `_5_03_ORDERLY_CS1_1`, `_5_03_1ST_FLOOR_1ST_HALL`, `_5_03_GEN_JOHNNY`, `_5_03_JOHNNY_YELLING`, `_5_03_JOHNNY_TRIG1`, `_5_03_PLAYER_IN_CR`, `_5_03_REC_ROOM`, `_5_03_JOHNNY_GOTO1`, `_5_03_DOW_1`, `_5_03_BLOCKB`, `_5_03_LEAVING`, `_5_03_GAMEOVER`, `_5_03_LEFT`, `_5_03_FIRE01..05` (unused), plus the door/bar trigger names above.
- PATHs: `_ASYLUMPATH1/2`, `_ASYLUM_PATROL_FRONTDESK`, `_ASYLUM_PATROL_ABLOCK`, `_5_03_1F_ORDERLY2_PATH1`, `_ASYLUM_PATROL_BBLOCK`, `_ASYLUM_PATROL_SHOWER`, `_5_03_1F_RECRM_ORDERLY2_PATH1`, `_5_03_CONTROL_ORDERLY_PATH`, `_5_03_JOHNNY_GOTO_PATH2`.
- Hardcoded: switch point, alarm-light points ×5, Johnny's run-off destination (−61.96, −315.67, 4.26), ~10 fixed camera pairs.
- Area codes seen: **0**, **38** (asylum interior).
- **Trigger data (DAT): YES** — `DATLoad("5_03.DAT", 2)`; original lives in `Trigger.img` per `trigger-format.md`. Not extracted here (by design). Ambient asylum data (`iasylum.DAT`, `SP_Asylum.DAT`) belongs to `Asylum.lua`, not this mission.

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say`, `camera`, `speech_sequence`, `spawn_npc` flag bundles, `stealth_spotter` + `npc_patrol_cycle` (all orderly patrols; vision-range = `npc_stat_override`), `vision_cone_eject_loop` (3_S11) + `scripted_bust_vignette` (3_B) + `soft_fail_eject_reset` (2_S06) — the S2 bust/eject cycle is exactly this trio with objective rollback, `npc_ignore_list` (patients↔orderlies, 3_03), `npc_asleep_staging` (3_05 — **variant: wakes on player proximity/alertness rather than script cue**), `npc_follow_path` + node-resume callbacks (look-into-rooms patrol), `door_lock_set` / `door_prop_control` (lock matrix, stay-open, pathability), `persistent_fade_entity` / `world_entity_swap` (alarm-light OFF→ON swap), `ambient_emitter_toggle` (alarm loop), `prop_used_poll` (the `AsySwtch` node watch), `npc_ai_tree_override` (Crazy_Basic / G_Striker swaps), `npc_tether_to_zone` (riot tethers), `recruit_ally` + `npc_health_bar` + `mission_critical_npc` (Johnny escort + KO fail), `social_override_set` + `social_action_blacklist` + `social_action_callback` + `gated_social_branch` (control-orderly greeting rig), `temp_outfit_swap` + `give_clothing` (outfit backup/restore + permanent grant), `pickup_spawner` (uniform pickup), `player_stance_flag` / `ped_flag_set` (sneak flag 2), `punishment_points_add` (zeroing variant per 3_08), `area_script_shared_state` (`bAsylumPatrols`), `unique_ped_suppress` (Lola/Norton), `leave_area_fail` (two-tier warn/fail bounds), `respect_delta`, `completion_banner`, `poi_enable`, `interactive_music_stream`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| S6 disguise | `temp_outfit_swap`, `player_wearing`, `give_clothing` | **disguise_outfit**: wearing the outfit sweeps `PedSetIsStealthMissionPed(false)` over every spotter (stealth disabled while worn), zeroes punishment, and rewrites attitudes; hitting a guard breaks cover (hit handler flips attitude, no fail); confiscated on cleanup if not passed, permanently granted on pass |
| S3/S7 punishment scoping | `punishment_points_add` (partial) | **punishment_system_toggle**: global disable/enable (`DisablePunishmentSystem`) plus player-only responder scoping (`PunishersRespondToPlayerOnly`) for infiltration vs riot phases |
| S4 sleeper wake | `npc_asleep_staging` + `stealth_spotter` | none (variant: proximity+alertness wake instead of script cue) |
| S7 cell-block release | `door_prop_control`, `npc_ai_tree_override`, `npc_tether_to_zone`, `npc_ignore_list` | none — the riot is composition |
| S8 escort | `recruit_ally`, `npc_health_bar`, `npc_move_to` chains, KO-fail | none |
| S2 bust loop | `scripted_bust_vignette` + `soft_fail_eject_reset` | none |

Genuinely NEW names introduced here: **disguise_outfit**, **punishment_system_toggle**. (`PedSetCheap`, flags 117/108/129, `SoundDisableSpeech_ActionTree`, `AreaDisableCameraControlForTransition` are render/audio/transition engine-call notes for the port, not mission primitives.)

## 6. Open questions

- `F_Create_Fire` (five fire triggers) is defined but never called — cut riot dressing, or fired from a data/area hook? **OPEN**.
- `T_Open_Doors_Line` (control-room hint text) is defined but never `CreateThread`ed; `F_IntoControlRoomAction`, `F_DOWing1Action`, `CB_F1_O1_Path`, `F_Social_Greet_Orderly` are empty; `CB_Station_Orderlies` / `CB_Front_Orderly` (punishment 400 + front-desk support move) are registered nowhere in-file — dead hooks or DAT-wired callbacks? **OPEN** (the front-desk support logic has live state, `bFrontDeskSupport`, consumed by the block-A beat).
- `MissionResetRespawnOverrides` on cleanup — where were respawn overrides set? Presumably engine-side on bust/eject. **OPEN**.
- `PedSetFlag` ids 117/108 (patients, off→on at release) and 129 (Johnny, outro) — flag-table decode pending; reads as "cheap/inert in cell" vs "freed". **OPEN**.
- Attitude matrix in S7: the nine `PedSetTypeToTypeAttitude` rows rewire orderlies/patients/greasers/player en masse; per-row semantics need the faction-table decode (0/3/4/9/13) before authoring. **OPEN** (rows read consistently as: orderlies hostile to patients, patients hostile to orderlies, everyone neutral to the disguised player).
- The found-Johnny NIS deletes and respawns `gBlockBOrderly01` mid-scene (continuity reset, like 5_02's greasers) — confirms `npc_respawn_fresh` as the pattern; whether the respawn is visible to the player is **OPEN** (camera placement suggests hidden).
- Wiki notes the orderly uniform can also be kept by failing out at specific points; the script strips 510 on any non-pass cleanup — the wardrobe-glitch claim is **OPEN** and out of remake scope.
