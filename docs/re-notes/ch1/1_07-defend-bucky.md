# 1_07 — Defend Bucky: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_07.lua` (actual file is 1336 lines; the task brief's 1857/1336 line counts for 1_07/1_08 are swapped), `SMissPass.lua` (`F_SetupMissionRequirements`), `main.lua` (~300-340). Functional logic in our own words; no dialogue or objective text reproduced. Script/function/native names are citations only.

Area codes seen: 0 = outdoors (`tschool`) only. The arena is the school garage / auto-shop yard (`DATLoad("tschool_garagedoors.DAT")`, door triggers `_SCGRDOOR01/02`).

## 1. Identification

**1_07 = Defend Bucky — VERIFIED.** Evidence:

- `LoadAnimationGroup("1_07_SaveBucky")` and `LoadAnimationGroup("1_07_Sk8Board")` (`F_MissionSetup`).
- The protected NPC is created from ped model 8 into variable `idBucky` (`F_CreateBucky`), gets a health bar, an ally link to the player, and cower/flee behaviour; the mission fails when he is KO'd.
- The reward path is the canonical skateboard: a persistent world entity `SC_SK8Board` is placed at (154.684, 1.29007, 6.77178) during the mission, deleted in the ending NIS, and the player walks out with weapon 437 equipped (`PedSetWeaponNow(gPlayer, 437, ...)`); `SMissPass.lua` gives weapon 437 again on pass (`GiveWeaponToPlayer(437, false)`). Weapon 437 = skateboard (**VERIFIED** by the entity name + `1_07_Sk8Board` group + `SK8_Examine` examine node).
- The completion text key is `1_07_UNLOCK` and the give-back animation tree is `Act/Gifts/Give107.act`.
- Setting is the school garage (bus door `_BUSDOORS`, hobo ped model 87 opens the side gate in the ending), matching the canonical mission's auto-shop location.

Naming caveat: Lua variables call the attackers "greasers" (`group1 = "greaserOne"` etc.), but the model ids (102, 99, 85, 145, 146) are the same ids `opening-slice-recon.md` maps to the `GN_Bully01-05` bully models. Treat the attackers as bullies; the variable names are leftovers (**INFERRED**).

## 2. Mission flow

Driven by `Library/LibObjective.lua`: two chained objective records (`objReachBucky` → `objDefeatEnemies`) plus a wave-monitor thread (`T_MonitorGreasers`) with a wave counter `nWaveBeat` 0→4.

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **Setup + baked intro** | `MissionSetup`: action music streams (`MS_ActionLow/Mid/High`), `PlayCutsceneWithLoad("1-07", true)` (baked cutscene; skip-on-retry behaviour is framework-side, **INFERRED** first-attempt-only as in 1_01/1_02), loads `1_07.DAT` + `tschool_garagedoors.DAT`, weapon models {319, 437}. `F_MissionSetup`: anim groups (below), weapon models 300/301/437, ped models {102, 99, 85, 145, 146, 147, 8, 87}, bullies' faction set hostile to the player type, gate PAnim `_1_07_GATE_T` and toolbox PAnim `_1_07_TOOLBOX01` created, persistent skateboard prop placed. |
| A1 | **Reach Bucky** | Player placed at `_1_07_PSTART` (area 0). Bucky (model 8, health 300, health bar shown, blip) stands stationary at `_1_07_BUCKY`; two bullies are posted at `_1_07_G1`/`_1_07_G2` (leader model 146, second model 99 armed with weapon 300). Objective 1 (text key `1_07_command`): get to Bucky. While the player approaches, Bucky's health is **scripted to drain 25 hp every 5 s** (`F_RunningToBucky`: tick = (400/2)/8) — 300 hp reaches 0 in ~60 s unattended. The named constant `REACH_BUCKY_TIME = 90000` is computed but never compared — vestigial (**VERIFIED** unused). |
| A2 | **Gate trigger: the fight starts** | If the player enters trigger `_1_07_GATE_T` early, the two posted bullies start attacking Bucky for real. Reaching `_1_07_GATETRIG` or `_1_07_FIGHTTRIGGER` completes objective 1 (`F_ReachedBucky`) and fires six complete-actions: bullies face/blip the player and go hostile-social (`F_GateBully`); the gate prop closes and becomes unpathable (`F_FocusOnGate`); flag `bGateCreated` (`F_CreateAnim`); two more bullies spawn at `_1_07_SPAWNPOINT01/03` (models 85 + 102, one armed with 300) (`F_CreateBullies`); a short taunt exchange plays (speech events `M_1_07` #3 and #17), then both pairs are ordered to attack Bucky *and* the player (`F_AskForHelp`); Bucky starts random cower-path fleeing (`F_BuckyGoCowerRandomly`). |
| A3 | **Wave 1** (`nWaveBeat` 0→1) | Kill the two posted bullies. Bucky AI: picks a random cower path among `PATH._1_07_BUCKY_COWERS`/`_PATH2`/`_PATH3`; re-flees if hit or if an enemy is within ~2 m; at path end he cowers (`cbBuckyCower`). One spawned bully is designated dropper: on KO he drops pickup 502 (`PickupCreateFromPed(502, ..., "HealthBute")` — a health pickup, **VERIFIED** by name). Spawned bullies attack player/Bucky in random priority and get blips. |
| A4 | **Wave 2** (→2) | Kill the two gate bullies. On wave-1 clear: Bucky is recruited as the player's ally, his health bar is hidden, garage door `_SCGRDOOR02` is closed, objective 2 (key `1_07_Objective2`, blip on `_1_07_BUCKYOBJ`). On wave-2 clear: the garage door opens (door monitor thread `T_DoorMonitor` watches `_1_07_GARAGECORONA`; the open event itself arrives via callback `cbDoorOpened`, whose registration site is not in this file — **OPEN**, likely PAnim/DAT-bound), Bucky walks `PATH._1_07_TOGATEWAIT`. Bucky cowering uses paths `_1_07_SCNDPATH1/2` in these waves. |
| A5 | **Toolbox NIS + wave 3** (→3) | When the door-open callback fires: two more bullies spawn at `_1_07_SPAWNPOINT04/05` (models 145 + 102, one armed 300), control off, in-engine NIS `F_DoBuckyGrabbingNIS`: fade, widescreen, fixed camera (FOV 40 at ~(127.3, 11.5)), Bucky walks `PATH._1_07_TOTOOLBOX`/`_TOTOOLBOXF` to the toolbox prop and plays a prop-interact take (toolbox PAnim Useable → NotUseable), control back. Objective 3 (key `1_07_Objective3`, blip `_1_07_PLYRDOOR`). Bucky cower paths `_1_07_THRDPATH1/2/3`. |
| A6 | **Rendezvous** (→4) | Wave 3 dead → objective 4 (key `1_07_Objective4`); when the player stands within 0.8 m of `_1_07_PLYRDOOR` (`F_MissionSpawnerFinished`), control off and the ending begins. |
| A7 | **Ending NIS (in-engine, ~25 s)** | `F_CompleteMission`: peds/population cleared; `F_Peanut_Confontation` — fade, player and Bucky moved to `_1_07_PLYRDOOR` slots, the world skateboard prop deleted, hobo (model 87) spawned at `_1_07_BUM` and speaks (`M_1_07` #28); `F_BuckyThanksThePlayer` — the hobo opens the side gate (`_1_07_GATE_T` PAnim + `_BUSDOORS` opened) and walks off along `PATH._1_07_TOBUSPATH`; player line (`M_1_07` #26); Bucky crosses to `_1_07_NIS_SK8PASS`. Then the give-back: player plays `Give_Attempt` from `Act/Gifts/Give107.act`, Bucky plays a `THANKS_JIMMY` speech event, Bucky walks to `_1_07_BULLYENDDEST`; player is given the skateboard (437) and examines it (`SK8_Examine`, second fixed camera). Completion screen `M_PASS` with unlock key `1_07_UNLOCK` plus two respect messages (`MRESPECT_NP5`, `MRESPECT_BM10`); faction respect set (`SetFactionRespect(11, 0)`, `SetFactionRespect(1, 65)` — **INFERRED**: faction 11 = Bullies, 1 = Nerds, since setup makes 11 hostile to the player and to faction 1); `MissionSucceed`. |

**Fail conditions**

| Cause | Handling |
|---|---|
| Bucky health ≤ 0 (drain or combat) | `F_FailMission`. If the gate sequence had already closed the gates (`bGatesClosed`): full fail NIS `F_BuckWasKnockedOutNIS` — fade, widescreen, fixed camera, Bucky deleted, surviving bullies (or fresh spawns 145/102 if none) posed at `_1_07_BULLYENDNIS`, player placed with them, gate re-opened (`OpenTSGate`), bullies walk off on `PATH._1_07_JIMMYPATH`/`_1_07_BUCKYPATH`, fail screen with key `1_07_BUCKYKO`. Otherwise: Bucky plays a `KO_COLLAPSE` node, fail screen `1_07_BUCKYKO`. |
| Player health ≤ 0 | `F_MonitorHealth` reports failure, but `F_FailMission` only calls `MissionFail` when Bucky is dead — player KO presumably falls through to the game's default KO/bust handling (**INFERRED**). |

**Retry/restart:** full restart from A0; no checkpoints. Cutscene skip on retry is framework-side (**INFERRED**, same convention as the opening missions). Debug helpers `F_SetupNISOne` etc. set `gDebugging` (disables fails, Bucky health pinned) — not live.

**Chains to:** no next mission. On cleanup, if mission `1_08` is already complete, `AreaLoadSpecialEntities("Halloween1", true)` — i.e. completing the *second* of the 1_07/1_08 pair turns on the Halloween dressing (**VERIFIED**; wider chain: `SMissPass.lua` loads Halloween1 for 1_05+1_06_01, Halloween2 after 1_09, Halloween2+3 after 1_11xp, and `1_11_Dummy` grants outfit "Halloween" and removes all three).

**Unlocks on pass** (`SMissPass.lua`, mission id `1_07`): weapon 437 (skateboard, **VERIFIED**); `MissionForceCompleted("1_E01")` + success count — an errand is force-completed (which errand 1_E01 is: **OPEN**). Faction respect changes as above. No outfit, no chapter set, no collectible toggle.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder |
|---|---|---|---|
| CS-A | Before A1 (`1-07`) | baked | fade in + letterbox + one `say` exchange; ~10 s original establishing shot |
| CS-B | A5 toolbox NIS | in-engine | camera + letterbox + npc path + prop-interact anim; no text needed |
| CS-C | A7 ending (hobo opens gate, give-back, skateboard examine) | in-engine, ~25 s, 2 fixed cameras + FOV changes | sequence of camera/letterbox/say/fade + npc_play_anim; can be compressed to gate-open + give + fade |
| CS-D | Fail NIS (`F_BuckWasKnockedOutNIS`) | in-engine | camera on the scene + fade to fail banner; stub-able |

## 4. NPCs and assets needed

Ped model ids (as scripted; `Objects/default.ide` not at hand — names only where context proves them):

| Role | Model id(s) | Notes |
|---|---|---|
| Bucky | 8 | protected NPC; ally; health bar; `GameSetPedStat` 15→0; social actions 23/24/25/28/29/35 disabled against him |
| Hobo | 87 | ending NIS only; opens gate, walks to bus |
| Bullies | 102, 99, 85, 145, 146 | waves: (146+99) posted, (85+102) gate spawn, (145+102) third spawn; 146/99/85 get one speech line each on spawn |
| — | 147 | requested, never spawned in the live path (**OPEN**/unused) |
| — | 21 | only in the dead `Peanut` table (`_1_07_PEANUT_SPAWN` etc.) — never spawned |

- **Animation groups:** `1_03The Setup`, `1_07_SaveBucky`, `TSGate`, `F_Nerds`, `Hang_Jock`, `NPC_AggroTaunt`, `Hang_Talking`, `1_07_Sk8Board`, `NIS_1_07`. **Action trees:** `Act/Conv/1_07.act`, `Act/Gifts/Give107.act`, `Act/Props/ToolBox.act`.
- **Weapons/pickups:** models 300 (equipped on one bully per pair — identity **OPEN**), 301 (requested, never equipped — **OPEN**), 319 (loaded in `MissionSetup` only — **OPEN**), 437 (skateboard); pickup 502 = health drop ("HealthBute").
- **Props/special entities:** PAnim gate `_1_07_GATE_T`, toolbox `_1_07_TOOLBOX01`, garage doors `_SCGRDOOR01`/`_SCGRDOOR02`, bus doors `_BUSDOORS`; persistent entity `SC_SK8Board` at (154.684, 1.29007, 6.77178); conditional `AreaLoadSpecialEntities("Halloween1")`.
- **Speech:** 9 `SoundPlayScriptedSpeechEvent` call sites (ids 3, 4, 6, 17, 26, 28 + `THANKS_JIMMY`), 0 `PedStartConversation`, 1 ambient helper (unused). Music: `MS_ActionLow/Mid/High` interactive streams; mission pass/fail stingers.
- **DAT:** `1_07.DAT`, `tschool_garagedoors.DAT`.

**Named world data to extract from `1_07.dat` (all area 0):**
POINTLIST: `_1_07_PSTART`, `_1_07_PEND`, `_1_07_BUCKY`, `_1_07_BUCKY_PEANUTCUT`, `_1_07_BUCKYOBJ`, `_1_07_G1`, `_1_07_G2`, `_1_07_SPAWNPOINT01`, `_1_07_SPAWNPOINT03`, `_1_07_SPAWNPOINT04`, `_1_07_SPAWNPOINT05`, `_1_07_PLYRDOOR`, `_1_07_BULLYENDDEST`, `_1_07_BULLYENDNIS`, `_1_07_NIS_SK8PASS`, `_1_07_BUM`, `_1_07_GARAGECORONA`, `_1_07_BYTHETOOLBOX`, `_1_07_PEANUT_SPAWN` (dead).
PATH: `_1_07_BUCKY_COWERS`, `_1_07_PATH2`, `_1_07_PATH3`, `_1_07_SCNDPATH1`, `_1_07_SCNDPATH2`, `_1_07_THRDPATH1`, `_1_07_THRDPATH2`, `_1_07_THRDPATH3`, `_1_07_TOGATEWAIT`, `_1_07_TOTOOLBOX`, `_1_07_TOTOOLBOXF`, `_1_07_TOBUSPATH`, `_1_07_JIMMYPATH`, `_1_07_BUCKYPATH`, `_1_07_PEANUT_COMES`, `_1_07_PEANUT_GOES` (dead).
TRIGGER: `_1_07_GATE_T` (gate PAnim + ped trigger), `_1_07_GATETRIG`, `_1_07_FIGHTTRIGGER`, `_1_07_NOAMBIENTPEDS`, `_1_07_TOOLBOX01`, `_SCGRDOOR01`, `_SCGRDOOR02`, `_BUSDOORS`.

## 5. Framework gaps

Covered by existing primitives: stage gating via `flag`/`all`; `reach_location`/`trigger_enter` (gate/fight triggers); `spawn_enemy` (waves, with model+point); `teleport`; `fade`, `letterbox`, `say`, `camera`; `timer` (drain cadence); `reward` (needs weapon support); `set_flag`/`clear_flag`; `event`/`log`/`hint`/`banner`.

NEW (1_07):

| Primitive | Used for |
|---|---|
| `defend_npc` / protect-health-threshold | Bucky: health bar, fail-on-KO, KO-specific-target fail branch with its own scene |
| `npc_health_drain` (timed ticks until cancelled) | the 25 hp / 5 s pressure during A1 |
| `wave_spawner` (all-of-group dead → advance + callbacks) | `T_MonitorGreasers` waves 0-4 |
| `ally_recruit` / `ally_dismiss` | Bucky between waves; also dismiss during NIS |
| `npc_cower_flee` (random path from set, re-flee on hit or enemy proximity, cower at end) | Bucky's survival behaviour |
| `ped_reached_point` (radius check on any ped, not just player) | A6 rendezvous; Bucky path-end callbacks |
| `path_node_callback` (per-node + last-node) | cower paths, toolbox interact trigger |
| `prop_anim` + door pathable/locked flags | gate, garage doors, bus doors, toolbox states |
| `pickup_drop_on_ko` (from specific ped) | the health-pickup dropper |
| `persistent_prop` create/delete by model name | `SC_SK8Board` |
| `npc_set_weapon` | armed bully per pair |
| `give_weapon` / `equip_weapon` | skateboard reward + ending equip |
| `faction_respect` + completion-message list | respect changes, `MRESPECT_*` lines |
| `music_streams` (low/mid/high intensity set) | action music |
| `npc_invulnerable` / `make_ambient` / `infinite_sprint` / `ignore_stimuli` flags | fail-NIS posing, cleanup |
| `speech_event` (id + volume + wait-until-done) | extends `say` to the speech-id system |
| `social_action_lock` (disable player actions vs a ped) | Bucky protections |

## 6. Open questions

1. `cbDoorOpened` registration: no call site in the Lua; presumably bound by the PAnim/DAT system. We can replace with a plain "door opened" event from `prop_anim`.
2. Weapon ids 300/301/319 and ped 147/21: unused or unidentified in this file.
3. Whether `PlayCutsceneWithLoad("1-07", true)` skips on retry is framework-side (attempt count), same convention as 1_01/1_02 but not read here.
4. `LibObjective` semantics (`stopOnFailed`, `stopOnCompleted`) are library internals; behaviour assumed from usage.
5. The scripted drain (25/5 s from 300 hp) implies ~60 s of leeway before the gate fight; real combat DPS on Bucky after A2 is action-tree data, undecoded.
6. The `T_ManageDoors` thread re-opens the gate PAnim every frame while area 0 is visible and `bGateOpen` false — looks like belt-and-braces against the trigger callbacks; exact gate state machine across NIS/fail paths is worth a playthrough check.
