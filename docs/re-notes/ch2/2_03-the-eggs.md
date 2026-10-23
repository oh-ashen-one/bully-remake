# 2_03 — The Eggs: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_03.lua` (967 lines, complete) + `SMissPass.lua` (`F_SetupMissionRequirements`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_03 = "The Eggs"** (canonical Chapter 2 list, 8-23) — **VERIFIED** by content and naming: the player must wear a prep-shop sweater (`F_PlayerHasAquaberrySweater` checks torso clothing against `R_Sweater1`/`R_Sweater5`), buy eggs (weapon/item 312) and deliver them to Tad's house (door props `TRICH_TADGATES`/`TRICH_TADGATES01`, anim group `TadGates`); the handoff turns into a prep ambush in Tad's yard with egg-throwing defenders and a locked-gate/key escape. No anim group embeds the mission title here (only `NIS_2_03`), so the id rests on content — still unambiguous. No surprise.

Setting is Tad's house in the rich area plus a store trip; area-code checks reference **0 only** (`AreaGetVisible() == 0` gates the egg-store blip logic). No interior area transitions in the script.

## 2. Mission flow

Driver: `main()` → `MissionSetup()` → `CS_TadAssignsMission()` → `Stage_GetEggsInit`, then a `MissionStageRun` function pointer (`Stage_GetEggsLoop → Stage_AmbushLoop → Stage_TheGetawayLoop`), plus threads `T_Egg_Hint` and `T_ResetEggsForInfiniteEggers`.

**Entry gate (unusual):** `MissionSetup` checks the player's torso clothing. If the player wears `R_Sweater1` or `R_Sweater5`: fade, `DATLoad("2_03.DAT")`, and a money floor — if the player has under 150 money units it is topped up to 150 (egg money, **INFERRED** from the later affordability fail). If not: the mission name banner is suppressed, a wrong-clothes message (`BOX_SWEATER`) shows for 5 s, and the mission aborts with `MissionFail(false, false)` — i.e. the mission *refuses to start* rather than failing in the usual sense. This is a dress-code mission gate, unique so far in our recon.

| # | Beat | Logic (functional) |
|---|---|---|
| E0 | **Baked cutscene "2-03"** | `PlayCutsceneWithLoad("2-03", true)` inside `CS_TadAssignsMission`. Loads anim groups `TadGates`, `NIS_2_03`, `MINI_Lock` and action trees `Act/Conv/2_03.act`, `Act/Anim/G_Ranged_A.act`; player transitioned to `_2_03_PLAYERSTART_NEW` (area 0). No attempt-count check in the file — retry skip behavior **OPEN** (engine attempt count, pattern from ch1). |
| E1 | **Errand: buy eggs** (`Stage_GetEggsInit/Loop`) | Tad (31) spawned sitting (`SitHigh` ambient node) at `_2_03_TADSTART` with Gord (30) and Chad (32) standing by; all three stationary/non-targetable/min-health-floored with hit-event registration (`cbTadGroupAttacked`). `shared.g2_03 = true` while the errand is unresolved (world flag; purpose **INFERRED**: marks the shopping beat for other systems). Objective switches between a buy-eggs entry (`2_03_OBJ1B`) and a deliver-eggs entry (`2_03_OBJ1C`). `T_Egg_Hint` blips the egg store point `_2_03_EGGSTOREBLIP` when the player has no eggs (with a one-time 4 s tutorial), and blips Tad once the player has eggs and is outdoors. Affordability watchdog `F_CheckMoney`: money < 150 + zero eggs + not currently in a shop → fail (`CMN_STR_06`, a generic broke message). |
| E2 | **Tad's gates auto-open** | `OpenTadsGatesMonitor` (polled in the loop): entering `TRIGGER._2_03_TADSBLOCK` unlocks and opens both gate props (`TRICH_TADGATES`, `TRICH_TADGATES01`; `PAnimOpenDoor` on the latter), once. |
| E3 | **Delivery or rebuff** | When the player enters `TRIGGER._2_03_TADNIS` **or hits any of the Tad trio**: if Tad is dead → fail `2_03_FAIL01`. If the player has eggs → E4. If not → `NIS_TadRebuffNoEggs`: control off, fade, trio frozen back at `_2_03_TADSTART`, bike dismount, player placed at `_2_03_PLAYERREBUFFED` (element 2), fixed camera (445.45, 488.47, 24.41 → 444.75, 487.76, 24.37), Tad plays one conv-tree dialog node (`TadRebuff`), 2 s beat, fade; Tad re-seats, player moved to element 1, control restored. Repeatable — the player can be rebuffed any number of times. |
| E4 | **Baked cutscene "2-03b" — the ambush sprung** (`CS_NemesisAppears`) | Widescreen, control off, fade, bike dismount, any vehicles within 20 m of `_2_03_PLAYER_AFTER_EGGS` deleted. `PlayCutsceneWithLoad("2-03b")`. After it: 31 cover points (`_2_03_CP_01..31`) registered with `PedAddCover`, ambient peds cleared (population and ambient vehicles zeroed), player placed at `_2_03_PLAYER_AFTER_EGGS`, **player's eggs (312) destroyed**, the trio repositioned to `_2_03_TAD_AFTER_EGGS`/`_CHAD_AFTER_EGGS`/`_GORD_AFTER_EGGS` facing the player and un-frozen. Gord walks `_2_03_EGGER1COVERPATH` (speech 16 at node 1; drop item 362 with second arg 75 — **INFERRED** 75% drop chance), Chad walks `_2_03_EGGER2COVERPATH` (drop 362); at the last node each becomes an egg-chucker (below). Six window eggers created (see §4). Fade in, control on. |
| E5 | **Yard fight + locked gates** (`Stage_AmbushInit/Loop`) | Music `MS_ActionHigh.rsm`; objective `2_03_OBJ3`; Tad turns hostile and attacks; **both gates closed and locked** (to player and peds). Two "keymaster" preps (model 34) spawn at `_2_03_FRONT_GATE_LOCKER` and `_2_03_BACK_GATE_LOCKER` playing a lock-picking idle loop, with player-hit events armed. Egg-chuckers (Gord, Chad, window eggers) run the `G_Ranged_A` ranged tree with eggs, auto-cover against the player, restricted to the `_2_03_TADHOUSE` trigger; a restock thread re-hands 100 eggs to any chucker who runs dry (infinite-ammo siege). |
| E6 | **Pick a keymaster** (`KeyMasterMonitor`/`GateExitCheck`) | Entering `_2_03_FRONTGATEAREA` or `_2_03_REARGATEAREA` — **or hitting either keymaster** — commits to that side's keymaster: he stops picking (`Lock_Picking_Loop_To_Idle`), gets stat overrides (34→15, 13→6, 8→90, 6→70, 7→60 — meanings undecoded), a blip, an attack order, and a jumbo speech line (id 12); the *other* keymaster is made ambient and disappears from the mission. Objective `2_03_OBJ6`. If the committed keymaster is already dead at choice time, the key drops immediately. Taunt loop: speech ids 12/13 at random every ~5-7 s while he lives. If the player slips through `TRIGGER._2_03_BACK_GATE` without the key, the back gate closes behind them (`bBackGateClosed`). Standing in `_2_03_BACK_UNLOCK`/`_2_03_FRONT_UNLOCK` without the key re-prints the locked-gate hint. |
| E7 | **Key drop** | When the committed keymaster dies: pickup **494** created at his position (`PermanentButes`) with a pickup blip. `PlayerHasKey()` = item 494 count > 0 → next stage. 494 is the gate key — **INFERRED** from context, no name in script. |
| E8 | **Getaway** (`Stage_TheGetawayInit/Loop`) | Objective completed, escape text (`2_03_11`), both gates unlocked, blips put on both gate props, prep-to-player type attitude (types 5 vs 13) set to 0 (0 = hostile here, **INFERRED** — same call is used elsewhere when turning enemies on). Pass condition: the player leaves `TRIGGER._2_03_TADHOUSE`. |
| E9 | **Pass** | Widescreen, speech action tree disabled, `SetFactionRespect(5, respect - 10)` (faction 5 = preppies, **INFERRED** from the all-prep cast), pass banner `M_PASS` with tag `2_03_UNLOCK`, respect message `MRESPECT_PM10`, pass music, `MissionSucceed(false, false, false)`. `shared.gHasTadKey = (success and gotKey)` persists whether the player escaped *with* the key. No chained mission start — next mission comes from the world mission table (**OPEN**). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Wrong clothes at start | torso isn't `R_Sweater1`/`R_Sweater5` | `BOX_SWEATER` (aborts; `MissionFail(false, false)`) |
| Broke and eggless | money < 150, no eggs, not in a shop | `CMN_STR_06` (generic) |
| Tad dies | checked at the delivery gate | `2_03_FAIL01` |

Retry: `MissionFail(true, true, msg)` (fade + retry) on the live path, and `ForceMissionAvailable("2_03")` re-offers the mission. On fail, cleanup removes item 494 from the player. No partial checkpointing: retry replays from E0/E1.

### Pass effects (`SMissPass.lua`, 2_03 block)

`ClothingGivePlayer("R_Sweater1", 1, false)` if the player doesn't already own it — **VERIFIED** (the very sweater the mission dress-code demands; mildly redundant, so it reads as an ownership guarantee for later content). No force-completes, no chapter set. In-script: prep respect −10, pass banner tag `2_03_UNLOCK`.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | E0, mission intro | baked (`2-03`) | `fade` in at `_2_03_PLAYERSTART_NEW` + `letterbox` + one `say` beat of original VO; or straight to E1 |
| CS-2 | E3 rebuff | in-engine micro-scene (fixed camera, one dialog node, ~3 s) | keep the shape: control off + fixed `camera` + `say` + `fade`; fully authorable with existing primitives |
| CS-3 | E4, ambush sprung | baked (`2-03b`) | `fade` + `letterbox` + short original reveal beat, then the reposition/arena setup the script already does in-engine |
| CS-4 | E9 pass banner | engine UI | `completion_banner` + respect line (existing pattern) |

Note: `MINI_Lock` anim group is loaded but **no lockpick minigame is launched** in this file — the lock-picking is pure anim dressing on the keymasters (**VERIFIED** no minigame call; do not schedule `minigame_launch` for this mission).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 31 | Tad | sits for the errand beat; hostile attacker in the ambush; death fails E3 |
| 30 | Gord | egg-chucker after cover path; drops 362 (75%?, **INFERRED**) |
| 32 | Chad | egg-chucker after cover path; drops 362 |
| 34 | keymaster ×2 | lock-picking idle; the chosen one turns attacker, drops key pickup 494 on death |
| 30/35/40 | window eggers ×6 (random model per slot) | stationary ranged attackers at window points; heavy stat overrides (incl. stat 3→1000, 13→0, 34→0) |
| 40 | also in the `enemyModels` load list | loaded, used by eggers |

Item 362 = KO drop — contents **OPEN** (same id dropped by marked bullies in 1_03; likely a generic pickup).

### Animation groups / action trees

`LoadAnimationGroup`: `TadGates`, `NIS_2_03`, `MINI_Lock`. Nodes used from shared trees: sit node from `Act/Anim/Ambient.act`; lock-picking loop + loop-to-idle from `Act/Conv/2_03.act`; rebuff dialog node from same; ranged AI `Act/Anim/G_Ranged_A.act` (shared egg-thrower tree).

### Props / pickups / weapons

- Gate door props `TRICH_TADGATES`, `TRICH_TADGATES01` (area door lock/open + `PAnimOpenDoor`; blippable props).
- Weapon/item **312 = eggs** (player errand item, destroyed at the ambush; also the defenders' weapon, 100 ammo, auto-restocked).
- Pickup **494 = gate key** (**INFERRED**), `PermanentButes`.
- Item 362 KO drops (**OPEN**).
- Money floor 150; 31 `PedAddCover` points for the yard fight.

### Speech

`SoundPlayScriptedSpeechEvent("M_2_03", id, ...)` ids used: 11 (Tad, ambush start), 12/13 (keymaster choose + taunts), 16 (Gord, cover path) — **4 distinct scripted lines**, plus conv-tree dialog nodes (rebuff) and the two baked cutscenes' audio. Music: `MS_ActionHigh.rsm` interactive stream for the ambush; pass/fail stingers.

### Named world data to extract from `2_03.dat`

- POINTLISTs: `_2_03_PLAYERSTART_NEW`, `_2_03_TADSTART` (3), `_2_03_TAD_BOXING` (3), `_2_03_TAD_AFTER_EGGS`, `_2_03_CHAD_AFTER_EGGS`, `_2_03_GORD_AFTER_EGGS`, `_2_03_PLAYER_AFTER_EGGS`, `_2_03_PLAYERREBUFFED` (2), `_2_03_EGGSTOREBLIP`, `_2_03_FRONT_GATE_LOCKER`, `_2_03_BACK_GATE_LOCKER`, `_2_03_WindowEgger1..6`, `_2_03_CP_01..31` (cover).
- PATHs: `_2_03_EGGER1COVERPATH`, `_2_03_EGGER2COVERPATH`.
- TRIGGERs/props: `_2_03_TADSBLOCK`, `_2_03_TADNIS`, `_2_03_TADHOUSE`, `_2_03_BACK_GATE`, `_2_03_FRONTGATEAREA`, `_2_03_REARGATEAREA`, `_2_03_BACK_UNLOCK`, `_2_03_FRONT_UNLOCK`, `_TRICH_TADGATES`, `_TRICH_TADGATES01`.
- Hardcoded camera: (445.4474, 488.4728, 24.4089) → (444.7502, 487.7572, 24.3685).
- Area codes seen: **0 only**.

## 5. Framework gaps (beat → primitives)

Reuses ch1 vocabulary (per docs/re-notes/ch1/*.md): `fade`, `letterbox`, `say`, `camera`, `spawn_npc` flag bundles, `npc_follow_path` + `ped_reached_path_node`, `ped_hit_event`, `npc_drop_item_on_ko`, `door_prop_control`/`door_lock_set`, `pickup_spawner`, `player_has_item`/`player_has_weapon` conditions, `population_override`, `completion_banner`, `set_faction_respect`, `mission_critical_npc` fail, `bark_loop`, `cutscene_skip_on_retry`.

| Beat | Covered by | New primitives needed |
|---|---|---|
| dress-code gate | `player_has_item`-adjacent | **player_wearing(slot, set)** start-condition (mission refuses to start, distinct from fail) |
| E1 errand + affordability | `pickup_spawner`, `dynamic_blip` (ch1 1_G1) | **money_ensure** (floor) + **money_below + not item + not shopping** fail condition (extends ch1 `money_ensure/money_take`) |
| E1/E3 sit/stand + rebuff | `camera`, `say`, `fade` | **npc_sit_at_poi** (ch1 1_04) reuse; rebuff loop = repeatable in-engine scene, no new primitive |
| E4 ambush arena | `spawn_enemy` | **cover_point_register** (31 named cover points) + **npc_cover_behavior** (ch1 1_03) + **npc_weapon_restock_loop** (infinite-ammo siege thread) |
| E5/E6 keymaster | `ped_hit_event`, `spawn_npc` | **pick_a_carrier** pattern: two idle decoys, proximity-or-hit commits one, other despawns; carrier death → `drop_pickup_at_ped` (ch1 2_S04); **npc_action_loop_until(event)** (lock-picking idle → interrupt) |
| E6/E8 gates | `door_prop_control`, `corona_interact`-adjacent | none new (lock/unlock/open + prop blips + auto-open-on-trigger) |
| E8 getaway pass | `trigger_exit` | **leave_zone_pass** (pass when player exits trigger — complement to ch1 `leave_area_fail`/`zone_exit_countdown`) |
| pass | `reward`, `completion_banner` | **give_clothing** (SMissPass sweater grant — ch1 1_02B already named it), `set_faction_respect` |

Genuinely NEW names introduced here: **player_wearing**, **npc_weapon_restock_loop**, **npc_action_loop_until**, **leave_zone_pass**, plus the money-floor variant of `money_ensure` as a mission-start action. Everything else maps onto the ch1 set.

## 6. Open questions

- Whether retry skips "2-03"/"2-03b" (no attempt check in file; engine attempt-count assumed, **OPEN**).
- Item 362 contents (KO drop, id only).
- Exact gate/cover/trigger geometry pending `2_03.dat` extraction.
- What `shared.g2_03` and `shared.gHasTadKey` feed downstream (**INFERRED**: shop/world state and later Tad content; consumers live outside this file).
- Keymaster stat-override meanings (ids 6/7/8/13/34 undecoded).
- `F_IgnorePlayer`'s `PedSetPedToTypeAttitude(ped, gPlayer, 4)` — attitude value 4 = ignore/neutral (**INFERRED**).
