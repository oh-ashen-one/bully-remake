# 2_S06 — Panty Raid: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_S06.lua` (1584 lines) + `SMissPass.lua` (2_S06 case) + `SZone.lua` (item ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_S06 = "Panty Raid"** (canonical Chapter 2 list, optional, 19-24) — **VERIFIED** by content: five collectible **item 484 = "undie"** pickups (VERIFIED name via `SZone.lua`) consolidated into **item 515 = "BUNCHOFPANTIES"** (VERIFIED), the mission giver Mr. Burton (model 229 — PE teacher, **INFERRED** from the name `pedBurton` and role; 229 not anchored in prior docs), the girls' dorm interior with a lattice climb-in, a stealth/collect stage with a patrolling headmistress and librarian, and a fire-alarm escape. No surprise.

Setting: outdoors (area 0) for the briefing/lattice and the getaway; the dorm interior is **area code 35** (`AreaGetVisible() == 35` gates the bust logic — **INFERRED**: 35 = girls' dorm `isc_gdorm`; new code beyond the known list). Doors referenced: `DT_TSCHOOL_GIRLSDORML`, `GDORM_UPPERDOOR`, `GDORM_UPPERDOORSTORAGE`.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → `F_Stage1()` → `F_Stage2()` → `F_Stage3()` sequential stages (debug entry `F_StartAtStage3`). Threads: `T_Stage1_Objectives`, `T_MonitorPanties`, `T_MonitorStage2Triggers`, `T_MonitorStage2Girls`, `T_ShowerTalk`, `T_PlayerLeftDorm`, plus speech threads.

| # | Beat | Logic (functional) |
|---|---|---|
| P0 | **Baked cutscene "2-S06"** | `PlayCutsceneWithLoad("2-S06", true)` in `MissionSetup`; `2_S06.DAT` + `2_S06b.DAT` loaded. Retry-skip not explicit — **OPEN**. |
| P1 | **Briefing + lattice climb (outdoors)** | Player placed at `_2_S06_SPAWNPLAYER`; Burton (229, faction 9, mission-critical) delivers the brief (speech 1) and walks `PATH._2_S06_BURTONLEAVE` (despawned at node 2). Objective 1 (reach the dorm top — **paraphrase**) + blip on `_2_S06_TOPLATTICE`; dorm door locked. A scripted gag: entering `_2_S06_TRIGGERLOADSCHOOL` spawns a prefect (51) + jock (16); entering `_2_S06_STG1_BUSTED` has the prefect haul the jock off along `_2_S06_STG1BUSTED` (both despawn at node 4). Entering `_2_S06_TUTLATTICE` while the trellis climb-on anim plays latches a tutorial flag. Entering `_2_S06B_ATTICENTRY` (the attic, reached by the lattice) → stage 2. |
| P2 | **Stealth collect: 5 panties (area 35)** | Punishment zeroed; dorm dressed: librarian (62) seated with **stealth behavior** (`PedSetStealthBehavior(1, F_BustPlayer)`), two shower girls (96, 182) stationary/invulnerable/untargetable with shower-steam effects, "sexy girl" (67) + "asian girl" (39) socializing, Eunice (74) upstairs; five **permanent pickups 484** at `_2_S06B_PANTIES` elements 1-5 with blips; HUD counter 0/5 (icon `HUDIcon_undies`); stealth music `MS_StealthLow`. Player flag 97 set (stealth-mission flag, semantics **OPEN**). Scripted beats: `_2_S06B_LAUNCHASIAN` sends 39 walking + a 7-line two-girl conversation (15-21) + tutorial `TUT_GIRLD1`; `_2_S06B_TRIGGERSHOWERDIALOGUE` starts the shower small-talk loop (ids 11/12 alternating every 10 s); `_2_S06B_TRIGGER03` sends Eunice walking out (`_2_S06B_ROUTEGIRLS_E`, line 22, deleted at node 14); `_2_S06B_ANGIECHRISTYROOM`/`_MANDYROOM` spook beats; `_2_S06B_SHOWERWARN` flashes a warning line. **The first panty pickup locks the storage-room door behind the player** (`GDORM_UPPERDOORSTORAGE`). |
| P2x | **Bust → eject (soft fail, not mission fail)** | If the **headmistress** (`shared.gdormHeadID` — an ambient-system ped, see gaps) or the **librarian** sees the player or is hit: bust cam on the spotter, a line (headmistress 45 / librarian ambient trespass warning), then `F_CutBootedOut` (fade, player disarmed and moved to `_2_S06B_CUTBOOTJIMMY`, headmistress to `_CUTBOOTHEAD`, player marched out along `_2_S06B_ROUTECUTBOOT` under speech 40), then `F_EjectPlayerFromDorm`: teleport to `_2_S06_EJECTPLAYER` (area 0), objectives reset to stage 1 (re-climb), uncollected panty blips removed. Re-entering `_2_S06B_ATTICENTRY` → `F_ResetDorm` rebuilds stage 2 exactly (peds, steam, remaining blips). **Punishment +5** when a girl is hit and flees (`F_GirlFlee`: she runs to `_2_S06B_POINTFLEE`, headmistress aggros with infinite sprint + stat override 3→12). Also: entering `_2_S06B_TRIGGER02` forces the shower-scream beat and a **guaranteed bust** (scripted, not avoidable — **INFERRED** as a scripted story beat). |
| P3 | **All 5 → alarm escape** | Fifth pickup: 484s zeroed, player given **515 = BUNCHOFPANTIES**; busting disabled; `F_LaunchAngryBeatrice` — in-engine cut: Beatrice (137) runs in (one of five cameras picked by *which* panty was last) and freaks out (speech + `BeatriceFreakOut` node); then `F_NewStage3Cut` (skippable, button 7): dorm redressed to panic (girls wandering, art teacher 63 yelling at `_2_S06B_ARTTEACHERYELL`, Pinky-UW 94 wandering), Beatrice walks `_2_S06B_BEATRICEPANIC` and at node 4 **hits the fire alarm** (`HitFireAlarm02` node) — alarm sound + 2D "Sprinklers" + **13 `FireSprinkler` effects** spawned at `_2_S06B_FIRE_01..13`. Control back; objective 3 (get out — **paraphrase**); stealth music up to `MS_StealthHigh`. |
| P4 | **Escape + delivery (area 0)** | Leaving the dorm: dorm cleaned up, objective 4 (deliver to Burton — **paraphrase**), five girls spawned outside (`_2_S06_STG3_GIRL01-05`: three posed in freak-out/impatient loops, two fleeing on `_2_S06_GIRLFREAK01/02`), Burton stage-3 instance (229) posted with a blip, social-gift override armed. **Eunice ambushes at the front door** (`_2_S06B_EUNICEFRONTDOOR`: line 33 + attack). 8 s after exit the **headmistress (54) spawns** and walks to `_2_S06_STG3_PREFECT`; if she catches the player in `_2_S06_STG3LEFTDORM`: line 45, attack, **+200 punishment points** (not a mission fail — the punishment system owns the bust outcome, **OPEN**). `_2_S06_MANDYSPEAK` trigger: two-girl exchange (32, 34) and the posed girls face the player while he's inside it. |
| P5 | **Hand-off to Burton → pass** | Player within 3.5 m of Burton: invulnerable pair, control off, widescreen, dismount; Burton walks to 0.8 m; gift-give flow (social override 32, player `Give` anim, speech 50 then Burton's 51 via the gift callback); **pass banner `M_PASS` (3 s)**, pass music; Burton walks off ambient; `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Burton hit/KO'd (either instance, mission-critical) | `F_MissionCriticalBurton` | `2_S06_FAIL_01` |
| Player pulls the fire alarm inside the dorm | player `FrAlrm` idle anim playing → 3 s grace → fail | `2_S06_FAIL_02` |
| Being spotted | bust → ejection loop (P2x) | **not a fail** |

Retry replays from P0; no in-file attempt-count cutscene skip — **OPEN**. The eject loop is the mission's signature: detection costs progress, not the mission.

### Rewards / pass side-effects

- `SMissPass.lua` 2_S06 case: `DebugMissionPassDependant("Chapt1Trans")` only (a debug helper, **VERIFIED** — no player-facing effect).
- In-script: none beyond the pass banner. Items 484/515 zeroed in cleanup. **No class, chapter, faction or outfit effects** (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | P0 `"2-S06"` | baked | `fade` + `letterbox` + 1-2 original `say` beats (Burton's proposal) or title card |
| CS-2 | P2x bust/boot | in-engine, ~8 s | `fade` + bust `camera` on the spotter + `say` + scripted player walk-out (`npc_follow_path` on the player) — keep, it's the loop's core feedback |
| CS-3 | P3 Beatrice freakout | in-engine, ~6 s | 5-way camera pick + `say` + anim |
| CS-4 | P3 alarm/panic cut | in-engine, skippable (button 7), ~10 s | `letterbox` + 2 `camera` setups + panic `npc_follow_path` + `timer` to the alarm beat + `effect` sprinkles |
| CS-5 | P5 delivery | in-engine, ~8 s | `letterbox` + `pair_anim` (give) + 2 `say` + `completion_banner` |

Cutscene-only anim groups: `IDLE_SEXY_C`, `NPC_Shopping` (loaded late for the panic stage), plus `GEN_Social`, `F_Girls`, `F_Pref`, `Px_RedButton` (alarm lever, **INFERRED**).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 229 | **Mr. Burton** (giver/recipient) | two instances (briefing, delivery); faction 9; mission-critical |
| 51 | prefect (stage 1 gag) | `PFH2_Edward` per opening doc |
| 16 | jock (stage 1 gag) | hauled off by the prefect |
| 62 | librarian | seated, stealth spotter |
| 63 | art teacher | panic-stage yeller (Ms. Phillips per 1_S01) |
| 54 | headmistress (stage 3 spawn) | + the ambient `shared.gdormHeadID` instance during stage 2 |
| 96, 182 | shower girls | stationary, invulnerable, untargetable |
| 67, 39 | social girls ("sexy"/"asian") | conversation chain 15-21; hit → flee/aggro |
| 94 | Pinky UW variant | panic-stage wanderer (94 also used as a stage-3 outside girl) |
| 137 | Beatrice | alarm-hitter |
| 74 | Eunice | walk-out beat; front-door ambush (attacks player) |
| 14 | stage-3 outside girl (Mandy, **INFERRED**) | posed freak-out |
| 3 | loaded in the model list, never spawned | dead load (**INFERRED**) |

### Animation groups / action trees

`LoadAnimationGroup`: `GEN_Social`, `F_Girls`, `Px_RedButton`, `F_Pref`, `IDLE_SEXY_C`, `NPC_Shopping`.
`LoadActionTree`: `Act/Conv/2_S06.act` (all mission nodes: shower loops, freak-outs, alarm hit, give); social overrides `Mission/2_S06Follow.act`, `Mission/2_S06PlayerGift.act`; player give via `Act/Player.act` `Social_Actions/Give/UseOverride`.

### Props / pickups / weapons / effects

- Pickups: **484 = undie** ×5 (PermanentMission), consolidated to **515 = BUNCHOFPANTIES** (both VERIFIED names). Cleanup zeroes both.
- Weapon 430 requested (`WeaponRequestModel(430)`) — not handed out in-script; **OPEN** what it is (not in the `SZone` table read).
- Doors: `DT_TSCHOOL_GIRLSDORML` (locked during), `GDORM_UPPERDOOR` (opened stay-open), `GDORM_UPPERDOORSTORAGE` (locks behind the player after panty 1; unlocked on eject).
- Effects: `ShowerSteam2` ×2 + `SteamRoom` ×1 (stage 2 dressing), `FireSprinkler` ×13 at `_2_S06B_FIRE_01..13` (alarm beat); audio `SoundStartFireAlarm`/`SoundPlay2D("Sprinklers")`.

### Speech

`M_2_S06` ids: 1, 2, 11, 12, 15, 16, 17, 18, 19, 20, 21, 22, 29, 30, 32, 33, 34, 35, 40, 42, 45, 50, 51, 75, 76 — **25 distinct scripted lines**, plus ambient events `WARNING_TRESPASSING`, `FREAK_OUT_GIRL_DORM`. Music: `MS_StealthLow` → `MS_StealthHigh` locked streams.

### Shared-state contract

`shared.gdormHeadID`, `shared.gdormHeadSpottedPlayer`, `shared.gdormHeadStop`, `shared.gdormHeadStart`, `shared.gdormHeadCanMove` — the **ambient girls'-dorm headmistress is owned by another script** (an area/ambient system script; not in this file — **OPEN** which). This mission pauses her (`gdormHeadStop`), drives her for busts, and resumes her (`gdormHeadStart`).

### Named world data to extract from `2_S06.DAT` + `2_S06b.DAT`

- POINTLISTs: `_2_S06_SPAWNPLAYER`, `_2_S06_SPAWNBURTON`, `_2_S06_STAGE3_SPAWNBURTON`, `_2_S06_STG1_PREFECT`, `_2_S06_STG1_JOCK`, `_2_S06B_SPAWNARTTEACHER`, `_2_S06B_SPAWNLIBRARIAN`, `_2_S06_STG3_TEACHER`, `_2_S06B_SPAWNSHOWER` (elements 1-4), `_2_S06B_SPAWNGIRLS_B` (1-2), `_2_S06B_SPAWNGIRLS_C` (2), `_2_S06B_SPAWNGIRLS_D` (1), `_2_S06_STG3_GIRL01`..`GIRL05`, `_2_S06B_SPAWNEUNICEUPSTAIRS`, `_2_S06B_SPAWNEUNICEDOWNSTAIRS`, `_2_S06B_FIRE_01`..`_13`, `_2_S06_BLIPLATTICE`, `_2_S06_TOPLATTICE`, `_2_S06B_PANTIES` (1-5), `_2_S06B_PANICASIANGIRL`, `_2_S06B_PANICSEXYGIRL`, `_2_S06B_PANICARTTEACHER`, `_2_S06B_PANICPINKYUW`, `_2_S06B_CUTBEATRICE`, `_2_S06B_BEATRICERUNIN` (1-5), `_2_S06B_POINTFLEE`, `_2_S06_EJECTPLAYER`, `_2_S06B_BUSTPLAYER`, `_2_S06B_HEADWARP`, `_2_S06B_CUTBOOTJIMMY`, `_2_S06B_CUTBOOTHEAD`, `_2_S06B_ARTTEACHERYELL`, `_2_S06B_STEAMROOM`, `_2_S06_BURTONLEAVE`, `_2_S06_STG3_PREFECT`, `_2_S06B_EJECTEDGIRLSB`.
- PATHs: `_2_S06_BURTONLEAVE`, `_2_S06_STG1BUSTED`, `_2_S06_GIRLFREAK01`, `_2_S06_GIRLFREAK02`, `_2_S06B_ROUTEGIRLS_B`, `_2_S06B_ROUTEGIRLS_E` (node 14 = despawn), `_2_S06B_BEATRICEPANIC` (node 4 = alarm hit), `_2_S06B_ROUTECUTBOOT`.
- TRIGGERs: `_2_S06_TRIGGERLOADSCHOOL`, `_2_S06_TUTLATTICE`, `_2_S06_STG1_BUSTED`, `_2_S06B_ATTICENTRY`, `_2_S06B_LAUNCHASIAN`, `_2_S06B_MAKEGIRLSSTUDY`, `_2_S06B_TRIGGER02`, `_2_S06B_TRIGGER03`, `_2_S06B_TRIGGERSHOWERDIALOGUE`, `_2_S06B_ANGIECHRISTYROOM`, `_2_S06B_MANDYROOM`, `_2_S06B_SHOWERWARN`, `_2_S06B_EUNICEFRONTDOOR`, `_2_S06_MANDYSPEAK`, `_2_S06_STG3LEFTDORM`, `_GDORM_UPPERDOOR`, `_GDORM_UPPERDOORSTORAGE` (+ door id strings `DT_TSCHOOL_GIRLSDORML`, `GDORM_UPPERDOORSTORAGE`).
- Hardcoded cameras: stage-3 cut (-448.54, 310.63, -1.66 …), Beatrice-run-in ×5 (around (-416..-445, 300..322, -1.1..-0.4)), bust cam (-433.61, 302.27, -1.01), boot cam (-437.95, 313.58, -5.99), panic cams (-447.17, 309.03, -1.58), (-455.10, 307.93, -1.25), (-446.60, 298.48, 0.68).
- Area codes seen: **0** and **35** (girls' dorm, INFERRED).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| P1 lattice | `player_action_state` (trellis climb-on anim → tutorial latch), `blip`, `trigger_enter` | — |
| P2 stealth collect | `stealth_spotter`, `collect_counter`, `pickup_spawner`, `ped_hit_event` (hit girl → flee + authority aggro), `door_prop_control` (storage lock-behind) | **shared_ambient_npc_hook** (mission borrows/pauses/drives an ambient-system NPC through `shared.*` handles: stop/start/can-move/spotted flags) |
| P2x bust loop | `fade`, `camera`, `say`, `player_follow_path` (walked out) | **soft_fail_eject_reset** (caught → bust scene → teleport outside → objective rollback → full stage re-setup on re-entry; mission does NOT fail), **punishment_points_add** (+5/+150/+200 events) |
| P2 ambience | `conversation_beat_machine` (7-line two-girl chain), `bark_loop` (shower talk 10 s alternation) | **effect_spawn_at_points** (named effects — shower steam ×3, sprinklers ×13 — from point lists, with kill-on-cleanup) |
| P3 alarm | `prop_set_state`-like anim, `player_action_state` (player alarm-pull → fail) | — |
| P4/P5 escape/delivery | `npc_follow_path`, `give_item_scene` (social give + required gift 15), `completion_banner`, `mission_critical_npc` | — |

## 6. Open questions

- Which script owns the ambient girls'-dorm headmistress (`shared.gdormHeadID` et al.) — an `AreaScripts`/POI file, not yet identified.
- Weapon 430's identity and purpose here (requested but never granted in-script).
- The exact cone/radius of `PedSetStealthBehavior` modes 0/1 (engine-side, undecoded — same as 1_S01).
- Ped flag 97 (stealth mission player flag) and 113 semantics.
- Whether a prefect-bust during stage 2 (punishment system) interferes with the mission's own eject loop (both zero punishment; ordering **OPEN**).
- Why the guaranteed-bust `_2_S06B_TRIGGER02` exists (forces the boot scene once per run? — reset by ejection, so it can re-fire; **INFERRED** scripted comedy beat).
