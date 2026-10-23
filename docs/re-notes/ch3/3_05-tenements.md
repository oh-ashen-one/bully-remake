# 3_05 — Tenements: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_05.lua` (786 lines, complete) + `SMissPass.lua` (no `3_05` branch — verified by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_05 = "Tenements"** (canonical Chapter 3 list, mission #41, 8AM–11PM) — **VERIFIED** by content: the whole mission plays inside the New Coventry tenements interior (area **36**, anim group `Area_Tenements`, music `MS_Tenements*.rsm`, window traverse `TRIGGER._DT_TPOOR_TENWINDOW`); the player collects Lola's (model 25) five belongings, fights Norton (29) who smashes through a wall with a sledgehammer (weapon 324), and returns the bundle to Lola for the kiss pass scene. Catalog cross-check: `docs/story-mode-catalog.md` #41. The `1_G1_TheDiary` anim group and `Act/Conv/1_G1.act` loads are for the give-gift anim reused from ch1's Diary side mission.

Dead/loose ends in the file: `bDebug = true` is set but never read; `tblPickupModels`/`tblWeaponModels` load ids 521, 362, 339 that have no pickup/action use on the live path (339 requested, never armed, **OPEN**); `nNumberPickups = 16` / `nNumberHighPickups = 11` are stale leftovers (the live counter max is 5); `F_ExecuteAnimationSequence` is defined but never called; `F_SmokerCallback`/`bSmokerSpotted`, `bExited`, `bReachedEndPath`, `bCatwalkFight(Started)`, `bSleeperAwake` (set, never gating) are vestigial; `TRIGGER._AMBIENTEVENT2`-style duplication bug: `T_HammerwallHints` checks `_3_05_HAMMERWALL03` twice (05 is checked too; a 04 may exist in the DAT, **OPEN**). `TESTblip` is a dev-named but live blip on the entry window.

## 2. Mission flow

Driver: `main()` is a straight sequential pipeline (no stage objects): `F_IntroCinematic()` → `F_FirstFloor()` → `F_CatWalk()` → `F_SecondThirdFloor()` → `F_NortonFight()` → `F_Escape()` → `F_Return()`, each blocking on its own polls. Two service threads: `F_tPickupMaintenance` (pickup/counter watcher) and `T_HammerwallHints` (contextual hint). The game clock is paused for the whole interior and unpaused before the return leg.

| # | Beat | Logic (functional) |
|---|---|---|
| T0 | **Baked cutscene "3-05"** | `PlayCutsceneWithLoad("3-05", true)` in `MissionSetup`; `shared.gWindowsOpen = true` (consumed by area scripts, **OPEN**); `DATLoad("3_05.DAT", 2)`; Lola unique model suppressed; `MissionDontFadeIn`. |
| T1 | **Approach + enter the tenements** | Attitude greasers(4)→player-type(13) set hostile (0); the tenement window traverse door `_DT_TPOOR_TENWINDOW` unlocked. Player transitioned to `_3_05_CINESTART` (area 0), peds cleared, objective `3_05_OENTER` + blip on `_3_05_TENWINDOW`. The poll waits for `AreaGetVisible() == 36` — i.e. the player climbs through the window and the area transition loads the interior (**VERIFIED** the traverse itself is engine/area-side). On entry the window prop is set unusable (`/Global/ExtWind/NotUseable`) for the duration. |
| T2 | **First floor: guarded lounge** | Interactive stream `MS_Tenements{Low,Mid}.rsm`. Six objectives registered (four item objectives + laundry + umbrella `3_05_GETITEMS`) and a HUD counter (icon `lola`, max 5). Six greasers from `_3_05_FIRSTFLOORGREASE` with stat overrides (stat 1 → 50, stat 0 → 362 — health 362, **INFERRED**); three of them loop smoking idles; two carry slingshots (303 ×100) and alternate generic chatter barks every 9 s until the player enters `_3_05_STARTSHOOTING` or hits one of them — then a taunt bark, both take **cover points** 1/2 (`PedCoverSet`, aimed at the player), and the whole list goes ambient. |
| T3 | **Catwalk gauntlet** | A runner (22) spawns at `_3_05_CATRUNNERS`; the mission waits until he **can see the player** (`PedCanSeeObject`), is hit, or the player trips `_3_05_STARTCATWALK` — then he flees along `PATH._3_05_HALRUN` with an alert bark (id 10). At `_3_05_CATWALK2` he turns and attacks (bark id 9); a second greaser (26) attacks at `_3_05_CATWALKB`, a third (27) at `_3_05_2CATWALKSTART`/`_3_05_CATWALKA`; the beat ends at `_3_05_SECONDFLOOR`. |
| T4 | **Second/third floor: second guard group** | Same pattern as T2 with `_3_05_SECONDFLOORGREASE`: three slingshot greasers chatter on the 9 s alternation until `_3_05_STARTSHOOTING2` or a hit → taunt + cover points 3/4/5. |
| T5 | **Pickup maintenance (runs throughout)** | `F_tPickupMaintenance` polls the five item pickups (diary 509, perfume 490, key 496, lipstick 508, laundry 507, all `PermanentButes` at `_3_05_LOLAPICKUPS` elements 1-5, each blipped); each pickup completes its sub-objective, drops out of the table and bumps the counter. Two extra throwable pickups (315 ×2 at `_3_05_LID`). Pickups 4 (diary/perfume/key/lipstick) are bound to their objective entries in `F_SpecialCaseSetups(1)`. |
| T6 | **Boss: Norton breaks in** | Triggered by picking up the **laundry basket** (507, `tblPickups.nortontrig`) — the fifth pickup doubles as the boss trigger. All greaser lists cleared. `F_NortonIntro`: fade, Norton spawned at `_3_05_NORTKNOCK` with sledgehammer 324; player teleported to `_3_05_JIMKNOCK` facing −273; widescreen; a camera path + look-at path pair (`_3_05_NORTONNISCAM` / `_LOOKAT`), four dialogue beats (ids 18-21) over scripted player/Norton anims culminating in Norton smashing through the wall — a broken-pipes persistent entity (`TennBRkPipes01`) is created at hardcoded coords in area 36 and both combatants teleport to `_3_05_BOSSBATTLEJASON` nodes 1/2. Norton runs a custom AI tree (`/Global/NortonAI`, `Act/AI/AI_Norton.act`) + action tree (`Act/Anim/3_05_Norton.act`), stats type `STAT_3_05_NORTON`, damage-taken multipliers 0.5 (classes 0 and 3), combat zone mask restricted, boss health bar `3_05_BOSSNAME`, locked high-intensity stream; player social actions 35/23/30/29/28 disabled against him (no grapple/humiliation family, **INFERRED**). Objective `3_05_ONORTON` + blip. |
| T7 | **No-KO boss retry loop** | The player is **immortal** (`PedSetImmortalFlag`) for the Norton fight: if player health hits ≤0, `F_RestartNorton` runs instead of a mission fail — fade out, all 324 pickups removed, Norton deleted and respawned fresh at the arena point, player healed to max and repositioned, health bar re-shown, fight restarts. Norton's own death ends the loop. |
| T8 | **Take the sledgehammer** | On Norton's KO: his carried 324 is destroyed (`PedDestroyWeapon` — drops as a world pickup, **INFERRED** same family as 2_07's trophy), objective `3_05_GETSLEDGE`, poll until `WeaponEquipped(324)` — the player must actually equip it, not just hold it. Then `3_05_GOTHAMR` text; stream drops back to Low/Mid. |
| T9 | **Escape** | Waits until the counter hits max (all five belongings). Window prop reset (`/Global/ExtWind/Reset`), counter HUD hidden, blip on `_3_05_PSTART`, objective `3_05_OLEAVE`; poll until `AreaGetVisible() == 0` (climbed back out). On exit the five item pickups are stripped from the player inventory. |
| T10 | **Return to Lola + pass** | Sub-objectives removed; the five collected item counts are zeroed and item 521 set to 1 (a "Lola's bundle" gift item, **INFERRED**). Lola spawned at `_3_05_LOLAEND`: stationary, ignore-stimuli/attacks, flag 113, mission-critical with a hit-sensitive fail callback, and a **registered gift social callback** (action 32 → `F_PlayerGiveGiftCallback`, which plays the 1_G1 give anim and clears her criticality). Player coming within 3.5 m makes both invulnerable and locks the scene: control off, widescreen, mutual face/lock, the give anim (`/Global/3_05/Animations/Give/GiveLola3_05/`), then the make-out social sequence (`GrappleAttempt/Kisses`) plays out; banner `M_PASS` + `MRESPECT_GM10` mid-kiss; pass stinger 0; greaser respect −10 (faction 4); player teleported back to `_3_05_CINESTART`; `MissionSucceed(false,false,false)`; fade back, control on. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Lola hit/KO'd during the return leg | `PedSetMissionCritical(idLola, true, F_Fail, true)` — the trailing `true` arms the hit (not just KO) variant, **INFERRED** from the `3_05_HITLOLA` key name | `3_05_HITLOLA` |

That is the only in-file fail path. No timers; the Norton fight cannot fail (immortal + retry loop, T7). Player death elsewhere falls to the engine default (**OPEN** how the engine's KO flow interacts with the paused clock — no in-file handling). On cleanup: clock unpaused, pipes entity deleted, items 521/496/507/508/490/509 removed, Lola restored to ambient, window door re-locked, counter/health-bar HUD hidden, attitudes reset, Lola unique status restored, all 324 pickups purged, and if the player is still inside area 36 and alive they are transitioned out to `_3_05_TENWINDOW`.

### Pass-side effects

- In-file only: banner `M_PASS` + respect message `MRESPECT_GM10`; greaser faction respect −10. No item/weapon/collectible grant.
- `SMissPass.lua`: **no `3_05` branch — VERIFIED by absence** (the collected belongings are stripped, not kept; there is no mechanical reward beyond respect).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | T0 intro | baked (`3-05`) | `fade` at `_3_05_CINESTART` + `letterbox` + one `say` |
| CS-2 | T6 Norton break-in | in-engine, ~15 s | `letterbox` + `camera_rail_pair` (chapt2trans primitive) + `speech_sequence` (4 beats) + `ped_action_node` on both + wall-break = `persistent_fade_entity` swap + double `teleport` to arena |
| CS-3 | T10 gift + kiss | in-engine, ~10 s | `give_item_scene` (1_G1/2_01) + `pair_anim` make-out (1_G1) + `completion_banner` + `fade` |

Cutscene-adjacent anim groups: `NIS_3_05`, `Area_Tenements`, `G_Grappler`, `POI_Smoking`, `Sitting_Boys`, `1_G1_TheDiary`.

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 25 | Lola (recipient) | stationary end-scene ped; mission-critical with hit-fail; gift social callback; unique model suppressed during the mission |
| 29 | Norton (boss) | custom AI + action trees, stats `STAT_3_05_NORTON`, sledgehammer 324, damage-taken ×0.5, social-action blacklist vs him, health bar |
| 24, 27, 22, 26, 28 | greaser guards / runners | cycled round-robin from `tblGreaserModels` {24,27,26,28}; health 362 via stat override; slingshots 303 on the shooters; smoking/sitting idle sets |

### Weapons / pickups

- **324 = sledgehammer** — **VERIFIED** by usage (Norton's weapon, the wall-break, the hammer-wall hint triggers, post-fight equip objective, cleanup purge). It doubles as the mission's "key item" for breakable walls (**INFERRED** — the `HAMMERWALL01-05` hint triggers print a hint only when 324 is *not* equipped, i.e. there are hammer-gated walls in the tenements).
- Mission pickups (all `PermanentButes`, all stripped on exit): 509 diary, 490 perfume, 496 key, 508 lipstick, 507 laundry basket (= boss trigger), 521 bundle (return-leg gift item, **INFERRED**), 315 throwable ×2 (`_3_05_LID`), 362 loaded but unused (**OPEN**).
- Weapon 303 slingshots on guards; 339 requested but never armed (**OPEN**).

### Speech

Scripted `M_3_05` ids used: 9, 10 (catwalk runner), 18-21 (Norton intro) — **6 distinct scripted lines** (small category); everything else is generic banks (`CONVERSATION_NEGATIVE_PERSONAL/_REPLY` chatter, `TAUNT_NEIGHBOURHOOD_GREASER`). Music: `MS_Tenements{Low,Mid,High}.rsm` (High locked during the boss); pass/fail stingers id 0.

### Named world data to extract from `3_05.DAT` (present — `DATLoad("3_05.DAT", 2)`, **flagged, not extracted**)

- POINTLISTs: `_3_05_CINESTART`, `_3_05_TENWINDOW`, `_3_05_LOLAPICKUPS` (5 elements), `_3_05_LID` (2), `_3_05_FIRSTFLOORGREASE` (6), `_3_05_SECONDFLOORGREASE` (≥4), `_3_05_CATRUNNERS`, `_3_05_CATWALKA/_B`, `_3_05_COVERPOINT1-5`, `_3_05_NORTKNOCK`, `_3_05_JIMKNOCK`, `_3_05_BOSSBATTLEJASON` (2), `_3_05_PSTART`, `_3_05_LOLAEND`.
- PATHs: `_3_05_HALRUN`, `_3_05_NORTONNISCAM`, `_3_05_NORTONNISCAMLOOKAT`.
- TRIGGERs: `_DT_TPOOR_TENWINDOW` (traverse door), `_DT_TENEMENT_WINDOW` (window prop), `_3_05_HAMMERWALL01/02/03(/04?)/05`, `_3_05_STARTSHOOTING`, `_3_05_STARTSHOOTING2`, `_3_05_STARTCATWALK`, `_3_05_CATWALK2`, `_3_05_2CATWALKSTART`, `_3_05_SECONDFLOOR`, `_TENEMENTS_FIRE_ESCAPE_CAMERA` (used only by the dead `F_tCatwalkCam` — never created as a thread, **INFERRED** cut fixed-camera beat; `_TENEMENTS_CAM_PATH2`/`_TENEMENTS_CAM_POINT_01` belong to it).
- Hardcoded coords in-file: Norton intro cameras (around (-533..-523, -42.6..-41.9, 41.1-42.0), i.e. the top-floor arena) and the pipes entity (-529.151, -40.751, 41.2078).
- Area codes seen: **0 and 36** (tenements interior = 36).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by ch1/ch2 set | New primitives needed |
|---|---|---|
| T0/T1 entry | `door_lock_set` (1_G1), `prop_set_state`/`door_prop_control` (2_05) for the window, `area_visible_eq` (2_01) for the area-36/0 waits, `area_script_shared_state` (`gWindowsOpen`) | none new |
| T2/T4 guard groups | `spawn_npc` bundles, `npc_stat_override`, `npc_cover_behavior` + `cover_point_register` (1_03/2_03), `conversation_beat_machine`/`npc_bark_timer` (the 9 s alternating chatter), `ped_hit_event`, `clear_ambient_peds` | none new |
| T3 catwalk | `npc_follow_path`, `ped_hit_event`, `stealth_spotter` (perception, 2_S06) partially | **npc_sees_player** as a plain non-stealth LOS condition (`PedCanSeeObject` gate) — fold-able into `stealth_spotter`'s perception check with a no-alarm mode; call it out as a condition variant |
| T5 collect | `collect_counter` (1_S01), `pickup_spawner`, `objective_checklist` (2_S04) | none new (5 pickups ticking 5 named sub-objectives + counter = exactly that composition) |
| T6/T7 boss | `boss_profile` (1_B: stats, multipliers, `health_bar`), `persistent_fade_entity` (wall-break pipes), `camera_rail_pair`, `boss_drops_carried_weapon` (2_07) | **boss_fight_retry_loop** (player immortal for the fight — 0 HP ≠ KO; on 0 HP: fade, boss despawn + fresh respawn at arena points, player healed + repositioned, fight restarts, no mission fail); **social_action_blacklist** (per-target disable of player social actions by id — different from `social_override_set`'s replacement defs); **npc_ai_tree_override** (swap both AI tree and action tree on a spawned boss — extends `boss_profile`); **combat_zone_mask** (`PedSetCombatZoneMask` arena restriction flags, **OPEN** semantics) |
| T8 equip gate | `player_has_weapon` (2_07) partially | **weapon_equipped** condition (`WeaponEquipped(id)` — equipped, not merely held; small new condition) |
| T2-hints hammer walls | `hint` | **hint_when_unarmed_in_zone** (re-printing hint while player is inside any of a trigger set without weapon X equipped — small condition+action composite) |
| T9/T10 inventory | `give_item`/`take_item` | none new (`ItemSetCurrentNum(x, 0)` batch-zero = `take_item` with n=all; set-521 = `give_item`) |
| T10 gift + kiss | `give_item_scene` (1_G1/2_01), `pair_anim` (make-out variant, 1_G1), `mission_critical_npc` (hit variant), `set_faction_respect`, `completion_banner` | **social_gift_callback** (`PlayerRegisterSocialCallbackVsPed` — engine social action fires a mission callback; here it clears criticality + plays the give anim; a hook on `social_interact` rather than a new flow) |
| fails | `mission_critical_npc` | none new — one keyed variant |
| world control | `population_override` not used here; `clear_ambient_peds` on entry; clock | none new (`PauseGameClock`/`UnpauseGameClock` = `clock pause/unpause` in the schema) |

## 6. Open questions

- Weapon 339 and pickup 362: loaded, never used — cut content or DAT-side use.
- `shared.gWindowsOpen` consumer (presumably area scripts for the tenement windows); whether `_3_05_HAMMERWALL04` exists in the DAT (the hint thread skips it — duplicate 03 check looks like a bug in the original).
- `PedOverrideStat(entry, 0, 362)` — stat 0 as max-health is **INFERRED**; 362 as the value is coincidentally also an unused pickup id here.
- The fire-escape fixed-camera beat (`F_tCatwalkCam`, `_TENEMENTS_FIRE_ESCAPE_CAMERA`) is fully wired but its thread is never created — cut, or created elsewhere, **OPEN**.
- Social action ids 35/23/30/29/28 disabled on Norton and gift action 32 on Lola — numeric social-action table undecoded.
- The exact rule linking the laundry pickup to Norton (it is just the 5th pickup by list order — the trigger is the pickup itself, not "last one collected": picking laundry early starts the boss with items outstanding, **VERIFIED** from `F_NortonFight` blocking on `tblPickups.nortontrig`).
- Where the player-KO-during-interior case lands (no in-file fail key; engine default presumed).
- Full trigger/point geometry, including the five `HAMMERWALL` breakable-wall associations (pending DAT extraction).
