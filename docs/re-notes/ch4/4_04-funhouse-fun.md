# 4_04 — Funhouse Fun: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_04.lua` (1776 lines, complete). No `SMissPass.lua` branch exists for `4_04` — all pass effects are in-file (**VERIFIED**, SMissPass read). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_04 = "Funhouse Fun"** (canonical Chapter 4 list, mission #47, 8AM–11PM) — **VERIFIED** by the loaded animation group `"4_04_FunhouseFun"` (mission title embedded in the group name), by content (nerds trapped in the carnival funhouse; jocks hunting them; the player escorts them out through the graveyard/reaper room, the mirror-maze-style trapdoor maze and the mining-crusher room), and cross-checked against the [Bully fandom wiki Chapter IV page](https://bully.fandom.com/wiki/Chapter_IV) ("Funhouse Fun 4-04"). Catalog cross-check: `docs/story-mode-catalog.md` #47.

Dead/loose ends in the file: `F_DropLadder`/`F_RaiseLadder` (defined, never called — the funhouse ladder prop is managed elsewhere, **INFERRED** cut ladder beat), `F_GraveExit` (never called), the `T_WitchMonitor` steam-effect table's `nextTime` re-trigger fields (set but never used — the loop just sleeps until `bStopWitches`), and `F_ControlRoom` (a one-line stage redirect never referenced by the main path — registered as an external hook for the area script, **INFERRED**). Debug stage entry points behind `bDebug`/`gStage` skip to graveyard/maze/mine/exit.

Cross-file hooks (called from outside, most likely the area-37 funhouse area script and prop scripts, **INFERRED**): `F_ButtonWasActivated` (maze switch), `F_SwitchMonitor01/02/03` (crusher-room switches), `F_ControlRoom`, `F_InReaper`/`F_InMines` (state queries). Same `area_script_shared_state` pattern as ch3.

## 2. Mission flow

Driver: `main()` runs a `gCurrentStage` function-pointer loop (`F_SetupGoToFunHouse` → … → `F_MoveToExit`), polling every frame; the reaper room (`F_SetupReaper`) is a blocking sub-loop with its own camera. No checkpoints: any fail replays from the top; on fail the player is kicked out of the funhouse to `_4_04_FAILEXIT` (area 0).

| # | Beat | Logic (functional) |
|---|---|---|
| F0 | **Baked cutscene "4-04"** | `PlayCutsceneWithLoad("4-04", true)` in `MissionSetup`; `MissionDontFadeIn`, `DATLoad("4_04.DAT", 2)`, conv tree `Act/Conv/4_04.act`; jock models {20,13,15,16,17,18} and nerd models {8,5} loaded; one faction-attitude call `PedSetTypeToTypeAttitude(2, 13, 1)` (type 2 toward player-type 13; slot semantics per ch3 notes, **OPEN**). No in-file attempt-count skip — **OPEN** (ch1 pattern suggests engine handles retry skip). |
| F1 | **Get to the funhouse (timed)** | Player placed at `_4_04_PSTART`; blip on the funhouse entrance anchor; objective `4_04_INSTRUC00A`; **300 s mission countdown** (`MissionTimerStart(300)`, `F_MissionTimerOut` → fail). Reaching `TRIGGER._4_04_REACHED_FUNHOUSE` stops the timer and sets `shared.g404GotKey = true` (carnival entry handshakes live outside, **OPEN** consumer). |
| F2 | **Upside-down room: rescue brawl** | Once area 37 is loaded and the player enters `_FUNHOUSE_UPSIDEDOWN`: the fun-book prop is set unusable; two jocks (18, 13) with stat overrides (stat 0 → 362, stat 1 → 100 — slot meanings **OPEN**, same bundle as 3_04's tableau) spawn and auto-attack the two nerds (8 = Bucky, 5 = Fatty — see model-vs-label note in §6), who fight back. Nerds get 2.5× health and 0.5 damage-taken on all four channels and are mission-critical (death callbacks `CB_NerdThadDied`/`CB_NerdFattyDied`). On the player entering `_4_04_JOCKNIS`: objective swap to `4_04_INSTRUC00` and a **dual health bar** for the nerd pair. When both jocks are dead → in-engine cutscene (`F_NerdsTalkToPlayer`): fade, everyone teleported to `_4_04_FIRSTROOMNIS`, nerds stationary facing the player, widescreen, two fixed cameras, one speech beat (`M_4_04` id 69). Afterwards nerds are tethered to `_4_04_NERDTRIG` and objective `4_04_INSTRUC01` (go on alone, **paraphrase**) shows. |
| F3 | **Graveyard room** | On `_FUNHOUSE_U_TO_G_TRANS` (or debug): fade, nerds despawned, player ladder/climb flags reset, player moved to `_GRAVEYARD_START`; the graveyard **relative camera rail** arms (`CameraSetRelativePath(PATH._GRAVE_RELATIVE_CAM, POINTLIST._GRAVE_RELATIVE, 350, true)` + `CameraLookAtPlayer`, change locked). Door `_DT_IFUNHOUS_FMDOOR` unlocked, `_IFUNHOUS_CTRLROOM` locked. All six reaper props set `Active`. A persistent geometry instance `fun_coffinExit` is spawned (blocker over the coffin exit, **INFERRED**) and recorded for cleanup. `T_WitchMonitor` starts two looping steam effects + 3D cauldron audio at `_WITCH_STEAM_01/02`. Objective `4_04_INSTRUC02`, blip `_4_04_GRAVEBLIP1`. A ladder-climb camera guard locks the camera while the player is on a ladder (`PedMePlaying` poll on Ladder/Climb_ON_BOT/TreeCheck). |
| F4 | **Control room + the reaper minigame** | Walking through the graveyard door corona (door-use anim playing at `_4_04_GRVDOORCORONAS`) triggers a scripted door link: control off, fade, teleport to `_4_04_CONTROLROOM`, objective `4_04_ACTIVATEREPR`. F_ProcessMonitor (fired externally, **INFERRED** via the control-room console prop): coffin-exit geometry removed, jock table counted (`nJocksLeft` = live first-room jocks — always 0 here, they're dead) then cleared, HUD swaps to the **monitor panel** (component 39 on, component 11 off), reapers set `PowerDown`, player idles at the console. Then `F_SetupReaper`: nerds respawned at `_4_04_RPRPATH1(/B)` and sent walking the reaper-path pair while three jocks (Juri 20, Dan 15, Casey 17) spawn at `_4_04_JOCKRESTARTPOINTS`. One jock at a time walks `PATH._4_04_JOCKFULLPATH` through six reaper trigger zones (`_4_04_REAPER01-06`); the camera sits at a fixed side view and pans with the walking jock (`CameraSetXYZ` re-aimed at the jock's offset each frame). **The player fires the currently armed reaper with button 7** (`IsButtonPressed(7, 0)`): a reaper plays its `Oneshot` trap anim and can be fired once per jock pass; it (re)arms when the jock enters its trigger and the oneshot isn't playing. Monitor line 0 shows the use-prop prompt and greys out when the armed reaper isn't usable. A jock reaching the second-to-last node survives: he's recycled to his restart point and the next jock is sent. A jock dying marks his model dead (`bJuriDead`/`bDanDead`/`bCaseyDead`) — these flags carry into the maze. When all three are resolved, the trap loop ends; if all six reapers are somehow stuck in `Oneshot/Stay` the loop also breaks (failsafe). Player repositioned to `_4_04_AFTERREAPER`, HUD restored, audio focus back on the player. |
| F5 | **Trapdoor maze: find both nerds** | On `_4_04_GRAVETOMAZE` approach → fade, reapers powered down, witch effects killed, trapdoor PAnim set loaded (8 doors `FMTRAPDr00-03 L/O` + 16 switches `FMTRAPSW…`), all doors initialized (left doors closed, right doors open), all switches `Inactive`. Nerds respawned at `_4_04_MAZENERDS(/2)`. **Surviving reaper jocks patrol the maze**: `nJocksLeft` (0–3) survivors spawn on patrol paths `MAZPATHJK1-3` with look-around node anims and the stat bundle; each fights the player on sight (`PedCanSeeObject`) or on being hit. The maze doors flip as a group: `F_ButtonWasActivated` (external switch hook) toggles the shared state — state 1 opens the left set / closes the right, state 2 the inverse. The nerds cry for help on a 6 s bark cooldown (random 50/50 pick of whichever nerd is still unrecruited). Entering `_4_04_MAZE_HALL_05` recruits Bucky (player-ally, thanks bark, single health bar, find-Fatty objective); `_4_04_MAZE_HALL_07` recruits Fatty symmetrically. With both recruited they ally to each other, the dual bar returns, and the objective becomes `4_04_INSTRUC04`. When **both** nerds stand in `_4_04_MAZE_HALL_08` the mine door `_IFUNHOUS_MZEMINE` unlocks and the exit blip appears; if either leaves before the player uses it, it re-locks. Player using the door → fade, stage advance. |
| F6 | **Crusher room: three switches** | Player placed at a hardcoded point (−764.6, −451.7, 15.5). Nine crusher props (`FunMiner` B/D/G/H/I/X1-X4) set `Active`; three monitor jocks (16, 18, 13) play use-monitor loops with the stat bundle; each fights the player when hit or when the player enters `_4_04_MONITOR01/02/03`. Nerds respawned (health carried over via `gNerds[i].health`) and walk `PATH._4_04_NERDR0`/`NERDL0`, facing a marker at path end. Three blips mark the crusher groups. `F_SwitchMonitor01/02/03` (external switch hooks) arm the per-group deactivation; the stage poll consumes the armed flag and runs `F_DoSwitchNIS(n)`: fade, control off, enemies put to sleep, widescreen, camera on the crusher pair, the pair plays `HoldOpen` then `ShutOff`, blip removed, nerds advance along `MINEPATHR/L{n}` if they're in the matching zone, one random nerd thanks the player, fade back. Progressive objectives: `4_04_DISABLEREST` after the first switch, `4_04_DISABLELAST` after the second. All three → the end door `_IFUNHOUS_MINEEND` unlocks, objective `4_04_INSTRUC05B`, blip on `_4_04_MINE_DEST_09_END`. Player using the end door → fade, stage advance. |
| F7 | **Final room: find the nerds, open the curtains** | Crushers shut off, curtains `_FUNCURTN(/01)` closed, nerds respawned at `_4_04_FINALROOMNERD1/2`, objective `4_04_FINDNERDS`. When the player can see each nerd (`PedCanSeeObject(gPlayer, nerd, 2)`) that nerd plays a dance anim and is recruited (player-ally, then pair-ally + dual bar once both are dancing). With both dancing, the player entering `_FUNHOUSE_CURTAIN_OPEN` opens both curtains; objective `4_04_INSTRUC05` + exit blip. |
| F8 | **Exit + pass** | Player using the exit door (`_4_04_EXIT` + door anim): fade, nerds moved to `_4_04_NERDSEND1/2`, area transition to 0 (`_4_04_PLAYEREND`), POI disabled, area peds cleared, widescreen, speech disabled, player made NIS-safe. Outro: fixed FOV-80 camera at the carnival, Fatty's line (`M_4_04` id 1), paired give/take conv anims on the player and Fatty, then completion banner `M_PASS` (2500 ms) with respect messages `MRESPECT_NP5` / `MRESPECT_JM10`, pass stinger 4, **nerds +5 / jocks −10 faction respect**, both nerds sprint off to `_4_04_NerdsEndDest` and go ambient. `MissionSucceed(true, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| 300 s entry timer expires | `MissionTimerHasFinished` in the stage-1 poll | `4_04_FAIL03` |
| Bucky (gNerds[1]) KO'd at any point after spawn | mission-critical callback / health poll | `4_04_THADIED` (key says THAD — see §6) |
| Fatty (gNerds[2]) KO'd | mission-critical callback / health poll | `4_04_FATTYDIED` |
| Any other failure path | generic | `MissionFail(true, true)` |

Fail music stinger 4. On fail the player is transitioned to `_4_04_FAILEXIT` (area 0). No checkpoints — full replay. Retry skip of the baked intro is not handled in-file (**OPEN**).

### Pass-side effects

- In-file only: jocks −10 / nerds +5 respect, banner + two respect messages, stinger 4.
- **No `SMissPass.lua` branch for `4_04`** (**VERIFIED**) — no weapon/clothing/class grants. (For reference, per fan wikis Funhouse Fun's tangible reward is the Grim Reaper trophy room item, which is not granted here — **OPEN**, likely a collectible placement side effect outside the mission script.)

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | F0 intro | baked (`4-04`) | `fade` in at `_4_04_PSTART` + `letterbox` + one `say` beat |
| CS-2 | F2 post-brawl talk | in-engine, ~5 s | `letterbox` + 2 fixed `camera` + one `say` + stationary pair facing player |
| CS-3 | F4 reaper minigame | live gameplay, fixed side camera | **timed_trap_gauntlet** (below) — not a cutscene; camera pan is part of the primitive |
| CS-4 | F6 crusher switches ×3 | in-engine micro-NIS each | `inline_cutscene_chain`: `fade` + fixed `camera` on the prop pair + prop `HoldOpen`/`ShutOff` states + one random `say` |
| CS-5 | F8 outro | in-engine, ~6 s | `letterbox` + fixed `camera` + `say` + `pair_anim` give/take + `completion_banner` + `set_faction_respect` |

Cutscene-adjacent anim groups: `4_04_FunhouseFun`, `Hang_Jock`, `NPC_Adult`, `NPC_AggroTaunt`, `Ambient`. Music: `MS_CarnivalFunhouseAmbient.rsm` (area-37 loop); stinger id 4 both ways.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 8 | Bucky (gNerds[1]) | escortee; 2.5× health, 0.5 damage-taken ×4 channels, health persists across room respawns via a saved-health field; mission-critical. Health-bar string keys label him "4_04_THAD" — see §6 |
| 5 | Fatty (gNerds[2]) | escortee; same bundle; mission-critical |
| 18, 13 | upside-down-room jocks | auto-attack the nerds; stat bundle (stat 0 → 362, stat 1 → 100); cleared after F2 |
| 20, 15, 17 | Juri, Dan, Casey — reaper targets | killed models stay dead for the maze patrol (`F_GetAvailableJock` skips dead flags) |
| 16, 18, 13 | crusher-room monitor jocks | use-monitor loops; proximity/hit fight triggers |

No vehicles, no weapons given to the player. No `PickupCreatePoint` in-file.

### Props / world systems

- PAnim sets: 6 reapers (`_IFUNHOUS_REEPER00/00B/01/01B/02/02B`, `FunReapr.act`: Active / PowerDown / Oneshot / Oneshot/Stay), 9 crushers (`FunMiner.act`: Active / HoldOpen / ShutOff), 8 maze trapdoors + 16 switches (`FMTrapDr.act`, `Switch.act`), 2 curtains (`FunCurtn.act`), fun-book (`FunBook.act`), ladder (`Ladder.act`).
- Doors: `_FUNTEETH` (funhouse entry — forced open at mission start, re-locked on cleanup), `_DT_IFUNHOUS_FMDOOR`, `_IFUNHOUS_CTRLROOM`, `_IFUNHOUS_MZEMINE`, `_IFUNHOUS_MINEEND`, `_IFUNHOUS_ENDMINE`.
- Persistent entity: `fun_coffinExit` geometry instance (created in F3, deleted in F4 or cleanup).
- Effects/audio: `steam_Shower` ×2 + `SoundPlay3D "WitchCauldren"` (graveyard dressing).
- Speech: `M_4_04` ids 1 and 69 only, plus generic banks (`SCARED`, `FIGHT_INITIATE`, `FIGHT_BEATEN`, `ALLY_HELP_ME`, `THANKS_JIMMY`, `THANK_YOU`). Small category.

### Named world data to extract from `4_04.DAT` (present — `DATLoad("4_04.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_4_04_PSTART`, `_4_04_ENTRANCE`, `_4_04_UDR_JOCK_01/02`, `_4_04_UPSIDENERDS(/2)`, `_4_04_FIRSTROOMNIS`, `_GRAVEYARD_START`, `_4_04_GRAVESTARTDBG`, `_4_04_GRAVEBLIP1/2`, `_4_04_GRVDOORCORONAS`, `_4_04_CTRLROOMCORONAS`, `_4_04_CONTROLROOM`, `_4_04_DEBUGGRAVE`, `_4_04_AFTERREAPER`, `_4_04_GRAVE_ESCAPE`, `_4_04_RPRPATH1(/B)`, `_4_04_JOCKRESTARTPOINTS`, `_4_04_GRAVETOMAZE`, `_4_04_MAZESTART`, `_4_04_MAZENERDS(/2)`, `_4_04_MAZEJOCKS`, `_4_04_ENDMAZEDOOR`, `_4_04_MINESTARTPOS`, `_4_04_MINEJOCKS` (3), `_4_04_MINENERDS1/2/3`, `_4_04_FACEPOINTMINE`, `_4_04_MINE_DEST_09_END`, `_4_04_FINALROOMPLYR`, `_4_04_FINALROOMNERD1/2`, `_4_04_ENDFUNHOUSE`, `_4_04_NERDSEND1/2`, `_4_04_PLAYEREND`, `_4_04_NerdsEndDest`, `_4_04_FAILEXIT`, `_4_04_MINEJOCKS`, `_WITCH_STEAM_01/02`, `_GRAVE_RELATIVE`.
- PATHs: `_GRAVE_RELATIVE_CAM`, `_4_04_JOCKFULLPATH`, `_4_04_RPRPATH1(/B)`, `_4_04_MAZPATHJK1/2/3`, `_4_04_NERDR0`, `_4_04_NERDL0`, `_4_04_MINEPATHR1/R2/R3`, `_4_04_MINEPATHL1/L2/L3`.
- TRIGGERs: `_4_04_REACHED_FUNHOUSE`, `_FUNHOUSE_UPSIDEDOWN`, `_4_04_JOCKNIS`, `_4_04_NERDTRIG`, `_FUNHOUSE_U_TO_G_TRANS`, `_4_04_REAPER01-06`, `_4_04_MAZE_HALL_05/07/08`, `_4_04_MONITOR01/02/03`, `_4_04_ENDMINE`, `_FUNHOUSE_CURTAIN_OPEN`, `_4_04_EXIT`.
- Hardcoded coords in-file: graveyard camera, control-room cameras, reaper side camera + pan lerp, three crusher-NIS cameras, outro camera, crusher-room player spawn.
- Area codes seen: **0** (carnival exterior) and **37** (funhouse interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1/ch2/ch3 set | New primitives needed |
|---|---|---|
| F0 baked cutscene | `fade`, `letterbox`, `cutscene_skip_on_retry` | none new |
| F1 timed entry | `countdown_fail_timer` (3_01A), blip on point | none new |
| F2 rescue brawl | `spawn_npc` flag bundles, `npc_stat_override` (2_09), `mission_critical_npc`, `npc_health_bar` | **dual_npc_health_bar** (one bar widget showing two NPCs' names/health — `PedShowHealthBar` two-ped form; small variant of `npc_health_bar`) |
| F3 graveyard camera | `camera`, `vehicle_camera_preset` (2_R03) is the nearest rail | **camera_relative_rail** (`CameraSetRelativePath`: camera rides a path positioned relative to a moving anchor point while tracking the player, change-locked; the graveyard room camera). Also the ladder-climb camera guard folds in as a `player_action_state`-gated variant |
| F3/F5/F6/F7 room links | `traverse_interact` (1_G1) is climb-specific; `door_lock_set` | **scripted_door_link** (player plays the door-use anim at a corona point → control off, fade, teleport to the linked point in the same interior, optional camera-preset swap + objective/blip advance; used 4× here — graveyard in/out, maze→mine, mine→final, final→exit) |
| F4 reaper minigame | `prop_set_state`, `npc_follow_path` node events (1_03) | **timed_trap_gauntlet** (player-as-operator trap defense: NPC targets walk a path through an ordered list of trap props; the current trap arms while a target is inside its trigger and its one-shot isn't playing; a single fire button plays the trap one-shot; trap kills register per-target; targets reaching the path end survive and are recycled/sent onward; carries per-model death flags into later stages) + **hud_monitor_lines** (`MonitorSetText` 4-line monitor panel + `MonitorSetGreyed` + `ToggleHUDComponentVisibility` component swap — the "use the prop" prompt UI) |
| F5 maze doors | `prop_set_state`, `door_prop_control`, `area_script_shared_state` (external switch hook) | **prop_group_toggle** (a switch event flips a named set of props between two complementary states — 8 trapdoors open-left/open-right as one shared state; switches themselves re-arm to `Inactive`) — small; could also be data over `prop_set_state` |
| F5 survivor patrols | `npc_patrol_cycle` (2_08), `npc_sees_player`, `ped_hit_event` | none new (per-model death flags carrying across stages = mission flags, data) |
| F6 crusher switches | `prop_set_state`, `inline_cutscene_chain`, `npc_asleep_staging` (3_05) for `PedSetAsleep` during the NIS | none new (per-switch NIS = `inline_cutscene_chain` + prop states) |
| F7 dance recruit | `recruit_ally`, `npc_sees_player` | none new |
| F8 pass | `completion_banner`, `set_faction_respect`, `group_flee`-adjacent walk-off | none new |
| external hooks | `area_script_shared_state` (3_05) | none new |
| fails | `mission_critical_npc`, `countdown_fail_timer` | none new |

## 6. Open questions

- Model-vs-label mismatch: `gNerds[1]` is **model 8** (Bucky per the 4_05/4_06 tables, and the mission's story is Bucky + Fatty in the funhouse), yet the health-bar keys and fail keys say `4_04_THAD`/`4_04_THADIED`. String-key reuse or a mislabel in the original; model 8 = Bucky is the stronger reading (**INFERRED**; fandom walkthroughs name Bucky and Fatty).
- `PedSetTypeToTypeAttitude(2, 13, 1)` slot semantics (same standing OPEN as 3_04's attitude calls).
- `GameSetPedStat(ped, 0, 362)` / `(ped, 1, 100)` stat-slot meanings (recurring jock bundle; belongs to a stat-table note).
- HUD component ids 39 and 11 (monitor panel vs. default HUD?) and `MonitorSetText` line indices 0–3.
- Whether button 7 in the reaper room is a generic interact or a prop-scoped binding; and the reaper "kill" path — the trap one-shot presumably damages the jock engine-side (no `PedSetHealth`/kill call in-file), **OPEN** how a `FunReapr` Oneshot hurts a ped (prop-damage volume, **INFERRED**).
- The all-six-reapers-`Oneshot/Stay` failsafe break in `F_SetupReaper` looks unreachable on a normal pass.
- Consumer of `shared.g404GotKey` (carnival gate free-entry?); set at F1, read elsewhere.
- Who calls `F_ControlRoom`, `F_SwitchMonitor01-03`, `F_ButtonWasActivated`, `F_InReaper`/`F_InMines` — presumed the area-37 script / prop scripts (same pattern as `area_script_shared_state`); confirm when area scripts are surveyed.
- Whether retry skips baked `4-04` (no attempt check in-file).
- All trigger geometry, switch binding order, and the `GRAVE_RELATIVE` camera anchor (pending DAT extraction).
