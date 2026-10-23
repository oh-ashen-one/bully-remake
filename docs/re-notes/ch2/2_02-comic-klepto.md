# 2_02 — Comic Klepto: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_02.lua` (467 lines) + `chap2/2_02_Conv.lua` (see its own note) + cross-refs `SMissPass.lua` (no 2_02 branch), `SFaction.lua` (unique-ped availability), `SStores.lua` (comic/bike shop setup), `SInitGl.lua` (flag init). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_02 = "Comic Klepto"** — **INFERRED (strong)**, and this is the assignment's **identification surprise**: 2_02 loads **no** title-embedding animation group (unique among the ch2 scripts checked — 2_01/2_05/2_07/2_08/2_G2/2_B all embed their titles) and its name does **not** appear in the canonical Chapter 2 list provided for this recon. Evidence, all in-script or one hop away:

- A ped literally named `pedKlepto` (model 70, a unique model — `SFaction.lua` suppresses 70 **and** 235 while 2_02 is available and restores 70 once it's completed) steals an item and flees **on a bike** along `_2_02_NEWESCAPE`.
- The stolen item is 504: KO-drop from the klepto at 100%, tracked by variables `bComicSpawned`/`blipComic`, returned in a hand-off anim node named `GiveComicOwner` — 504 = comic book (**VERIFIED** by naming + setting).
- The victim/quest-giver is model 84 at `_2_02_OWNER`, and the pass walk-off targets `_TBUSINES_COMICSHOPDOOR1`; `SStores.lua` confirms 84 is the comic-shop clerk (`ShopSetShopKeepInfo(84, POINTLIST._STORE_DT_COMIC_CLERK)`) — **VERIFIED**.
- Conv glue (`2_02_Conv.lua`) is about a comic being got/returned.

Setting: entirely outdoors (area 0) in/around the Vale business district (`tbusines`) with the fail boundary at the school parking lot; no interior areas are referenced. It is a bike chase, not a foot mission.

## 2. Mission flow

Driver: `main()` → `F_SetupMission()` → `F_Stage1()` (setup + loop) → `F_Stage2()` (setup + loop), plus threads `T_MonitorKlepto` (distance, music, death) and one-shot `T_ShowBikeCombatTutorial`.

| # | Beat | Logic (functional) |
|---|---|---|
| B0 | **Baked cutscene "2-02"** | `PlayCutsceneWithLoad("2-02", true, false, false, false)` in `MissionSetup`, `2_02.DAT` (slot 2), no fade-in, `MS_BikeActionMid` interactive stream locked on. No attempt-count check in the file — retry-skip **OPEN**. Debug launches (`IsMissionFromDebug`) transition the player to `_2_02_PLAYER`. Models: peds {84, 70}, weapon 301, vehicles {282, 273} (weapon model 333 is loaded but never used — dead load, **INFERRED**). |
| B1 | **Chase start** (`F_Stage1_Setup`) | All ambient vehicles cleared, then every bike within 100 m of the player deleted. Klepto (70) spawned at `_2_02_KLEPTO` and mounted on his bike (282, `_2_02_KLEPTOBIKE`): ignore-stimuli, ignore-attacks, locked onto the player, ten stat overrides (stats 9/10/24/26/27/28/29/30/35/37 — meanings undecoded, **OPEN**), weapon 301 with 20 ammo, health ×1.5 and that value locked as max, named health bar (`2_02_KLEPTONAME`), KO-drop registered (item 504, 100%). Shop owner (84) spawned at `_2_02_OWNER`, mission-critical (hit → fail), flag 113 ("don't care about trouble", as in 1_G1). Player's bike (273, `_2_02_PLAYERBIKE`) created and owned by the player, who is auto-mounted; brief fixed camera at (517.1, -58.6, 7.8), then control. Klepto flees **mounted** along `PATH._2_02_NEWESCAPE`; owner plays his one scripted line (`M_2_02` id 26); objective `2_02_INSTRUC01` (chase him, paraphrase). |
| B2 | **Chase rules** (`Stage1_Objectives` + `T_MonitorKlepto`) | Distance leash (2D): ≥ 50 m → one-shot warning text (`2_02_WARN`); ≥ 75 m → fail (message 1); dropping back under 50 m re-arms the warning. Music follows distance too: Mid ↔ High locked streams switch at 45 m with 500 ms crossfades. If the klepto is **knocked off his bike** (`PedIsInAnyVehicle` false): weapons cleared, attitude hostile, he attacks the player **on foot** (one-way transition). If he reaches `TRIGGER._2_02_SCHOOLPARKING` (the school lot): scripted escape beat — he stops, drops item 362 (**OPEN**, same mystery KO-drop id as 1_03/1_10), flees, goes ambient, both bikes go ambient — then fail message 1. Five seconds after the player first mounts, a bike-combat tutorial (`BIKECOMBAT_TUT01`) shows if still mounted. |
| B3 | **KO → comic recovery** | When the klepto dies: a blip marks his position, objective `2_02_INSTRUC01` completes, `2_02_INSTRUC03` added (grab the comic, paraphrase); the owner is **deleted and respawned** at his point — stationary, ignore-stimuli, still mission-critical (waits for the return). Stage 2 starts when the player holds item 504. |
| B4 | **Return the comic** (`F_Stage2` + `F_DoFinalNIS`) | Objective `2_02_INSTRUC04` + owner blip; music stopped. Entering `TRIGGER._2_02_RETURNCOMIC`: owner and player invulnerable, control off, widescreen, NIS-safe, player dismounted, owner made mission-non-critical with flag 128 set on both (scene guard — exact meaning **OPEN**); mutual lock; player plays `/Global/2_02_Owner/Anims/GIVE/GiveComicOwner` (`Act/Conv/2_02.act`) to completion; pass banner (`M_PASS`, 3 s) + pass music. Owner unlocked, walks to `_TBUSINES_COMICSHOPDOOR1`, made ambient; flags/invulnerability restored; `bMissionPassed`. |
| B5 | **Pass** | Fade out, camera reset/returned, `MissionSucceed(false, false, false)`, fade in, control on. Cleanup: all 504 pickups removed and inventory 504 zeroed, owner restored to a normal ambient ped (flag 113 off, invulnerable off), speech-by-action-tree re-enabled, health bar hidden, interactive music stopped, DAT slot 2 unloaded. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Klepto escapes | enters `_2_02_SCHOOLPARKING`, **or** 2D distance ≥ 75 m after the 50 m warning | `2_02_FAIL_01` |
| Owner attacked | mission-critical callback `F_MissionCriticalShopKeeper` (owner made ambient first) | `2_02_FAIL_02` |
| Generic | any other failure path | none (`MissionFail(false)`) |

On fail: `2_02_EMPTY` text beat + fail music + `MissionFail(false, true, key)` — retry allowed, no checkpoints; retry replays from B0/B1.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | B0, before control | baked (`2-02`) | `fade` in on the shop front + `letterbox` + one `say` beat (the theft), or straight to B1's auto-mount |
| CS-2 | B2 escape beat | in-engine fail vignette (scripted, no camera work) | `event` + `say`-less: drop, flee, `fade` to fail banner |
| CS-3 | B4 comic return | in-engine, ~5 s, control off + give anim | `letterbox` + `give_item_scene` (pair anim) + `completion_banner`; trivially compressible |

No `NIS_2_02` group is loaded and no `LoadAnimationGroup` call exists in the file at all (**VERIFIED**) — all motion is gameplay or the single give node in `Act/Conv/2_02.act`.

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 84 | comic-shop clerk (quest giver) | **VERIFIED** via `SStores.lua`; mission-critical both stages; deleted+respawned stationary at stage change |
| 70 | the klepto (chase target) | unique model; mounted flee → on-foot attack when dismounted; health bar, ×1.5 health, stat overrides, weapon 301 ×20 |
| 235 | — | suppressed while 2_02 is available (`SFaction.lua`) but never spawned by the mission — role **OPEN** |

### Vehicles / weapons / items

- Vehicles: 282 (klepto's bike), 273 (player's bike) — both appear in the bike shop's stock list (`SStores.lua` `DTBikeStoreLoad` loads 282/280/273) → BMX-style bikes, **INFERRED**; player's bike is mission-owned.
- Weapons: 301 (klepto, 20 ammo — identity **OPEN**); 333 loaded, never used (dead load).
- Items: 504 comic (KO-drop, 100%; removed on cleanup), 362 escape-drop (**OPEN**; also the unexplained KO drop in 1_03/1_10).

### Animation groups / action trees

No anim groups. `LoadActionTree`: `Act/Conv/2_02.act` (give node `2_02_Owner/Anims/GIVE/GiveComicOwner`; the conv glue is `2_02_Conv.lua`).

### Speech / music

One scripted line: `M_2_02` id 26 (owner, chase-start). Text keys: `2_02_INSTRUC01/03/04`, `2_02_WARN`, `2_02_FAIL_01/02`, `2_02_EMPTY`, health-bar name `2_02_KLEPTONAME`, tutorial `BIKECOMBAT_TUT01`. Music: `MS_BikeActionMid/High` locked interactive streams, distance-switched (45 m, 500 ms fades); pass/fail stingers.

### Named world data to extract from `2_02.DAT`

- POINTLISTs: `_2_02_PLAYER` (debug start), `_2_02_OWNER`, `_2_02_KLEPTO`, `_2_02_KLEPTOBIKE`, `_2_02_PLAYERBIKE`, `_TBUSINES_COMICSHOPDOOR1` (shared comic-shop door point).
- PATHs: `_2_02_NEWESCAPE` (the mounted flee route).
- TRIGGERs: `_2_02_SCHOOLPARKING` (escape boundary), `_2_02_RETURNCOMIC` (return hand-off).
- Hardcoded coords: start camera (517.11, -58.57, 7.82) → look (517.78, -57.87, 7.58).
- Area codes seen: **0 only** (outdoors; debug transition). No interior areas referenced; the comic shop door is an exterior point.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by (ch1 set) | New primitives needed |
|---|---|---|
| B0 cutscene + retry | `fade`, `letterbox`, `cutscene_skip_on_retry` | — |
| B1 spawns | `spawn_npc` flag bundles (ignore-stimuli, ignore-attacks, mission_critical, flag 113), `npc_stat_override`, `health_bar(name)`, `npc_drop_item_on_ko`, `npc_invulnerable` | **vehicle bundle**: `spawn_vehicle(model, point, owner, blip_on_vehicle)`, `mount_player`/`mount_npc` (`PutOnBike`), `clear_ambient_vehicles(radius)` (parallel to `clear_ambient_peds`) |
| B2 chase | `ped_hit_event` (KO poll), `timer` (tutorial delay) | **npc_flee_path_mounted** (mounted flee along a path with focus-target); **npc_dismounted_event** (npc leaves vehicle → swap behavior: disarm + on-foot attack); **distance_leash two-tier** extension of ch1 `npc_distance_leash`: warn threshold (one-shot text) / fail threshold / recover-to-rearm, instantaneous rather than time-held |
| B2 music | — | **music_intensity_by_distance** (locked interactive Mid/High streams switched on target distance with crossfade-ms params; generalizes the interactive-music tiers armed in 2_01) |
| B2 escape / fail | `leave_area_fail`-style trigger check | fold the escape vignette into the fail path: `npc_flee` + `vehicle_make_ambient` + per-cause fail message (1_11x2 pattern) |
| B3 stage change | `event`, `blip_on_npc`/pickup blip | **npc_respawn_fresh** (delete + recreate same model/point with a new flag bundle) — thin composition of delete + `spawn_npc`, listed for the stage-change idiom |
| B4 return | `give_item_scene` (pair anim + scene guard), `completion_banner`, condition `player_has_item` + `trigger_enter` | — |
| B5 pass/cleanup | `reward` (none in Lua), cleanup actions | **inventory_zero(items)** on cleanup (also used by 2_01) |

## 6. Rewards and unlocks

- **No 2_02 branch exists in `SMissPass.lua`** (**VERIFIED** by absence): no item/weapon/outfit grants, no force-completes, no chapter set, no class unlocks from the pass handler.
- `SFaction.lua`: unique models 70 **and 235** are suppressed while 2_02 is available; 70 is re-enabled once 2_02 is completed (availability bookkeeping, not a player reward).
- Any money/trophy reward would come from the world mission table (outside the Lua) — **OPEN**. (In the retail game the returned comic ends up as a dorm-room decoration; nothing in the Lua confirms this — **OPEN**.)

## 7. Open questions

- Retry behavior for the baked cutscene (no attempt check).
- How the mission is offered at the comic shop (mission table outside Lua; the conv flags in `2_02_Conv.lua` hint at shop-dialogue integration).
- Weapon 301's identity and whether the klepto ever actually uses it (he's ignore-attacks until dismounted, and dismounting clears his weapons).
- What item 362 is (recurring mystery drop id, also in 1_03/1_10).
- Meaning of the ten `PedOverrideStat` values on the klepto (stat ids undecoded) — they presumably tune his chase/bike AI and tankiness.
- Who ped 235 is and why it shares 2_02's availability suppression.
- Flag 128 semantics (set on owner+player during the hand-off scene).
