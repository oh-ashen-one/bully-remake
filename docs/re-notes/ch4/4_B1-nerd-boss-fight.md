# 4_B1 — Nerd Boss Fight (Earnest, observatory boss): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap4/4_B1.lua` (700 lines, complete) + `SMissPass.lua` (4_B1 case) + `main.lua` (bulletin hook). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_B1 = Nerd Boss Fight**, the Earnest Jones boss battle on the observatory catwalks. **VERIFIED** by content: boss ped `pedEarnest` model 10 with dedicated action tree `Act/Anim/N_Earnest.act`, the `"Earnest"` animation group, boss music `MS_NerdBossBattle.rsm`, pass-side rewards spud gun (`GiveWeaponToPlayer(305)` + 10 ammo) + `SetFactionRespect(1, 50)` (faction 1 = nerds, per prior docs) + `UnlockYearbookPicture(10)` + completion key `4_B1_UNLOCK` / respect message `MRESPECT_NP50`. External cross-check: Bully Wiki lists "Nerd Boss Fight" as unlocked by "Stronghold Assault" with rewards "Nerds +50 / Spud Gun" — matches the script exactly ([bully.fandom.com/wiki/Nerd_Boss_Fight](https://bully.fandom.com/wiki/Nerd_Boss_Fight), [bully.fandom.com/wiki/Stronghold_Assault](https://bully.fandom.com/wiki/Stronghold_Assault)). Catalog entry #45 "Stronghold Assault" is the *preceding* mission (`4_02` — VERIFIED by TCRF unused-dialogue filenames `Fatty_4-02_*`/`Corneli_4-02_*` for that mission, and by `SMissPass.lua`'s 4_02 case granting weapon 305 as a safety net).

**This corrects the working assumption that 4_B1/4_B2 are two halves of one jock/Ted boss:** they are two separate Chapter 4 boss scripts. 4_B1 is the *nerd* boss (Earnest); 4_B2 is the *jock* chapter boss (Ted) — see `4_B2-jocks-boss-fight.md`. The `_B` prefix just marks boss-style missions this chapter. Chain (mission table is outside the Lua — **INFERRED** from wiki unlock order + `main.lua` bulletin keys `BB_22`/`BB_25`): `4_02 Stronghold Assault` → `4_B1 Nerd Boss Fight`; `4_06 The Big Game` → `4_B2 Jocks Boss Fight` → `Chapt4Trans`.

Structure: three sequential stages (no `shared.*` checkpoint — a fail restarts the whole mission; debug-only `gDebugLevel` jump-ins `F_StartAtStage2/3` exist but are disabled). Each stage = destroy the two currently-targetable motor props (`PAnim` "OBSMotor") while Earnest attacks with a per-stage weapon pattern; a scripted platform-collapse NIS separates stages.

Setting: observatory catwalks — **area 40** on start (`AreaTransitionPoint(40, POINTLIST._4_B1_PLAYER_STAGE01)`), player returned to **area 0** at pass. Baked cutscene `"4-B1"` on entry, `"4-B1D"` on win.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → `F_SetupMission()` → `F_Stage1/2/3` (setup + poll loop each). Stage loops call `StageN_Objectives()`; globals `bGoToStage2/3`, `bMissionFailed`, `bMissionPassed` drive transitions. `T_MonitorPlayerHealth` thread is the only fail watch (player death → fail).

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Setup** | No fade-in; `4_B1.DAT` layer 2; transition to area 40; baked `"4-B1"`; weapon inventory snapshot (restored on fail, deleted implicitly on pass path by cleanup). Punishment off; player HP floored to 200; player ped flag 58 (cheap-KO immunity); all damageable PAnims reset; music `MS_NerdBossBattle.rsm`; weapons 301/305 pre-requested; the three `*Pla_BROKEN` geometry variants pre-hidden; crate loot override `DPI_CrateBrk` → `PickupListCrateHEALTH`; `Act/Conv/4_B1.act` loaded; control off. |
| B1 | **Stage 1: spud-cannon turret** | Platforms (`TRIGGER._PLAT_NORTH/SOUTH/WEST`) and turret (`_TURRET_TOP`) created as PAnims with prop flag 2; six `OBSMotor` props get flag 2 and per-stage targetability (stage 1: the north pair targetable). Earnest spawned invulnerable/untargetable/ignore-stimuli; camera dummy ped (model 136, stationary/asleep/no gravity) is the `CameraSetSecondTarget` anchor for the `FollowCamSetFightShot("4B1")` boss cam. Intro NIS-lite: Earnest bark `M_4_B1` id 3, walks `_4_B1_TURRETPATH`, mounts the cannon (`GetOnCannon` node, spin-locked until playing), locks onto the player; control handed over; Earnest's AI task set to `UseSpudCannon` (fires the turret at the player). Objective `4_B1_MOBJ_01` + blips on the two live motors. Player shoots/strikes the motors. |
| B2 | **Stage 1 → 2 NIS: north platform collapse** | Both stage-1 motors destroyed (`PAnimIsDestroyed` polls; each kill breaks a `JanMotorLight02` light prop, removes the blip, plays `MotorDamge03`) → `F_NIS_01()`: explosions/projectiles cleared, Earnest emptied + walks `_4_B1_NIS_01`, turret plays `Fall`, north platform plays `RumbleNorth` (2.5 s rumble loop) then `CollapseNorth` + `BalconeyFall01`. → stage 2. |
| B3 | **Stage 2: bomb lobbing** | Fade; turret PAnim deleted; `NorthPla_BROKEN` becomes a persistent entity; five observatory damage-state geometry props (`0iobserv01/06/07/11/13`) toggled on; motor targetability rotated to the west pair. Player/Earnest/camera teleported to stage-2 points; player keeps pre-stage weapon (saved + restored around the transition). Earnest tethered to `TRIGGER._4_B1_EARNEST_PATTERN02`, flag 2 set / flag 1 cleared. `T_EarnestStage2`: 6-second cycle — give weapon **417** ×8, play `N_Earnest/Offense/ThrowBombs`, 2 s hold, re-arm flags. (`BombRandomizeLocations`/`BombSetLocation01..08` — a 9-point bomb-target randomizer driven by `PedSetProjDest` — exist but are **dead code**: never called. **VERIFIED**.) Two blips on the live pair. |
| B4 | **Stage 2 → 3 NIS: west platform collapse** | Both stage-2 motors destroyed → `F_NIS_02()`: Earnest emptied, flag 2 cleared, tether cleared, walks `_4_B1_NIS_02`; west platform `RumbleWest` → `CollapseWest` + `BalconeyFall02`. → stage 3. |
| B5 | **Stage 3: spud-gun duel** | `WestPla_BROKEN` persistent entity; motor targetability rotated to the south pair (last two). Earnest re-positioned to gun point 2 and armed with weapon **305** (spud gun) ×100. `T_EarnestStage3`: ~3-second cycle — pick a random one of 3 gun points (`_4_B1_STAGE03_GUN01..03`), run to it, face the player, re-arm 305 ×20, play `FireSpudGun`. |
| B6 | **Win** | Both stage-3 motors destroyed → fade, control off, explosions/projectiles cleared, `SouthPla_BROKEN` swap, `"Earnest"` group unloaded, baked `"4-B1D"`, pass path. |
| B7 | **Pass** | Player NIS-proofed; speech trees muted; **crate loot overrides flipped to bottle rockets** (`DPI_CrateBrk` + `DPE_CrateBrk` → `PickupListCrateBRockets` — in-script; `SMissPass` adds `DPI_CardBox` → `PickupListMailBoxSprayCan`, all guarded by `shared.bCrateStateSetup`); player transitioned to area 0 `_4_B1_PLAYER_END`; **spud gun granted** (`GiveWeaponToPlayer(305)`, 10 ammo, equipped); fixed victory camera; `SetFactionRespect(1, 50)`; completion banner `M_PASS` + unlock key `4_B1_UNLOCK` + respect message `MRESPECT_NP50`; end music (win, track 4); yearbook 10 + `F_UnlockYearbookReward()`; `MissionSucceed(true, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Result |
|---|---|---|
| Player dead | `T_MonitorPlayerHealth` poll per stage | crate override `DPI_CrateBrk` → `PickupListCrateSprayCan`, weapon snapshot restored, fail music (track 4), `MissionFail()` |

No stage checkpointing, no arrest/timer/objective fails. Retry = full mission restart (the three stage functions re-run from `F_Stage1`).

### Rewards / pass side-effects (`SMissPass.lua` 4_B1 case + in-script)

- In-script: spud gun 305 (+10 ammo, equipped); nerd respect set to 50; yearbook picture 10; crate loot tables → bottle rockets.
- `SMissPass` 4_B1: once-only crate/mailbox overrides (`shared.bCrateStateSetup`, shared with 4_03's case); `MissionForceCompleted("C_Photography_4")` + success-count increment. **No `ChapterSet`** (unlike 3_B — the chapter roll is deferred to 4_B2/Chapt4Trans).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0 opening | baked `"4-B1"` | `fade` + original establishing card |
| CS-2 | B1 turret mount | in-engine (walk + mount + lock-on) | `npc_follow_path` + `ped_action_node` + 2 fixed `camera` shots + `say` |
| CS-3 | B2/B4 platform collapses | in-engine (~4 s each) | **platform_collapse_nis** (rumble loop → collapse node + crash sfx) + `letterbox` |
| CS-4 | B6 victory | baked `"4-B1D"` | `fade` + `completion_banner` + original outro card |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 10 | **Earnest** (boss) | invulnerable + untargetable all mission; tree `Act/Anim/N_Earnest.act`; stage patterns: turret task (`AI.act/UseSpudCannon`), bomb-lob cycle (weapon 417), spud-gun point-hop (weapon 305) |
| 136 | camera dummy | stationary/asleep/no-gravity ped used as fight-cam second target |

### Animation groups / action trees

`LoadAnimationGroup("Earnest")`; `LoadActionTree("Act/Conv/4_B1.act")` (turret fall, platform rumble/collapse, Earnest empty-walk); referenced `Act/Anim/Ambient.act` (`GetOnCannon`), `Act/AI/AI.act`, `Act/Anim/N_Earnest.act`.

### Props / pickups / weapons

- 6× `OBSMotor` damageable props (hardcoded XYZ; per-stage targetability rotation), each paired with a `JanMotorLight02` breakable light + `MotorDamge03` sfx.
- Platforms: `TRIGGER._PLAT_NORTH/WEST/SOUTH` PAnims + `NorthPla_BROKEN`/`WestPla_BROKEN`/`SouthPla_BROKEN` geometry-instance swaps (persistent entities on break) + 5 `0iobservXX` damage props.
- Crate loot overrides: HEALTH during mission, BRockets after pass, SprayCan on fail (fail-path override looks like a quirk — **INFERRED** unintended).
- Weapons: 301 (requested, unused in-file), 305 (spud gun: stage-3 boss weapon + player reward), 417 (Earnest's stage-2 bombs).

### Speech

`M_4_B1` ids 1-3 (one per stage; only id 3 is actually wired to the intro via `Stage03Speech`-style helpers — `Stage01/02/03Speech` are defined but uncalled, **dead code**). Roughly **1-3 scripted lines** + motor/collapse sfx. Music: `MS_NerdBossBattle.rsm` single stream.

### Named world data to extract from `4_B1.DAT` (trigger data **VERIFIED present** — `DATLoad("4_B1.DAT", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_4_B1_PLAYER_STAGE01/02/03`, `_4_B1_PLAYER_END`, `_4_B1_EARNEST_STAGE01/02`, `_4_B1_CAMERA01/02/03`, `_4_B1_BLIPS1M1/1M2/2M1/2M2/3M1/3M2`, `_4_B1_STAGE03_GUN01/02/03`, `_4_B1_STAGE02_BOMB01..09` (dead).
- PATHs: `_4_B1_TURRETPATH`, `_4_B1_NIS_01`, `_4_B1_NIS_02`.
- TRIGGERs: `_PLAT_NORTH/SOUTH/WEST`, `_TURRET_TOP`, `_4_B1_EARNEST_PATTERN02`.
- Hardcoded cameras: intro (-699.89, 78.73, 26.25 → …), NIS-01 (-702.56, 73.68, 24.11 → …), NIS-02 (-696.98, 80.35, 23.65 → …), pass cam (43.57, -133.70, 3.93 → …).
- Area codes seen: **40** (observatory), **0** (pass teleport).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| B0 setup | `mission framework`, `weapon_inventory_snapshot` (2_B/3_B pool), `player_buff` (HP floor) | none new |
| B1 turret boss | `spawn_npc` bundles, `npc_invulnerable_to_player`, objective + blips | **destructible_prop_objective** (N marked PAnim props as the objective: `PAnimIsDestroyed` watch → paired light-break prop + sfx + blip-off; stage advance when the set is cleared); **turret_task_boss** (boss ped driven by an AI task node — `UseSpudCannon` — instead of melee; umbrella for "boss operates a fixed weapon emplacement") |
| boss fight cam | — | **boss_fight_camera** (`CameraSetSecondTarget` on a stationary dummy ped + `FollowCamSetFightShot`/`CameraSetShot` preset; 4_B2 reuses with `"4B2"`) |
| B2/B4 stage transitions | `geometry_swap` / `geometry_instance_toggle` (pool), `persistent_fade_entity` (pool) | **platform_collapse_nis** (rumble-loop → collapse anim node + crash sfx + broken-variant geometry swap + boss evacuate path; two variants N/W, a third S inline at B6) |
| targetability rotation | — | **prop_target_rotation** (same prop set re-targetabled per stage: flag 2 + `PAnimMakeTargetable` matrix by stage) |
| B3 bomb cycle | `npc_weapon_restock_loop` (pool, close) | **npc_bomb_lob_cycle** (timed cycle: grant weapon+ammo → action node → re-flag; 6 s cadence) — could fold into `npc_weapon_restock_loop`; flag the cadence-with-node difference |
| B5 gun-point duel | `npc_stat_override` | **npc_gun_point_cycle** (random point from a set → run → face player → re-arm → fire node; ~3 s cadence) |
| crate loot | `pickup_table_override` (3_B pool) | reuse (3 states used: HEALTH / BRockets / SprayCan) |
| B7 pass | `completion_banner`, `yearbook_unlock`, `give_weapon`, `set_faction_respect`, `force_complete` (pool) | none new |

## 6. Open questions

- Weapon 417 identity (Earnest's stage-2 bomb — spud bomb vs. B-rifle-style projectile) and weapon 301 (requested but never given in-file).
- `GeometryInstance(name, bool, x, y, z, bool)` argument semantics (create/enable vs. visible) — needed to reproduce the broken-platform swaps exactly.
- Whether the fail-path `PickupListCrateSprayCan` override persisting after a failed attempt is intentional (fail leaves the override live until a pass flips it to BRockets).
- Mission-table wiring: exact launcher chain `4_02` → `4_B1`, and whether bulletin `BB_22` gates its start.
- Ped flag 2/flag 1 semantics on Earnest during the bomb cycle (set/clear pattern repeats with no in-file documentation).
