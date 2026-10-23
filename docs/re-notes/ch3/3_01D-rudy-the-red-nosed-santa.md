# 3_01D — Rudy the Red Nosed Santa: recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_01D.lua` (1000 lines, complete). `SMissPass.lua` has **no** 3_01D case (**VERIFIED** by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_01D = "Rudy the Red Nosed Santa"** (canonical Chapter 3 list, #31, 12PM–6PM; SE-exclusive Christmas mission) — **VERIFIED** by content against the [Bully wiki page](https://bully.fandom.com/wiki/Rudy_the_Red_Nosed_Santa): Rudy (252, the same hobo-Santa as 3_01A "Balls of Snow") runs a shabby Santa's Court in a Bullworth Town alley and sends Jimmy after "any garbage that's sparkly" — this script's five ornament pickups (521) under a timer — with elf+dog ambushes after the 2nd and 4th pickup (**VERIFIED** beat-for-beat), then a second half photographing five named kids (Pedro, Sheldon, Gloria, Melody, Karen — the wiki's cast) on Santa's lap, timed so each kid is looking and smiling — this script's tree-driven `bGoodFace` pose windows. Two wiki details confirm engine specifics seen here: "if he hasn't already earned the digital camera, he's given one for this mission" (the force-arm of weapon 426 at M3) and the colour-camera quirk (matches `HUDPhotographySetColourUpgrade(true)`). First draft of this doc mis-filed it as "Miracle on Bullworth St." — that mission is **3_XM** (the Santa's Court queue event), corrected in the same pass.

The grotto location: Bullworth Town shopping area, area 0 only (the locked door list `DT_tbusines_*`/`DT_tpoor_Barber` is all Bullworth Town storefronts). Debug prints call it "3.10D" (same internal 3_10 family as 3_01A). Shares only the id prefix with 3_01.

No external Lua references to "3_01D" (**VERIFIED** by grep) — offering mechanism **OPEN** (world mission table, outside the scripts; presumably post-`3_XM`).

## 2. Mission flow

Driver: `MissionSetup()` → `main()` — two linear halves (timed collect, then photo session), mostly straight-line code with polling loops rather than stage machinery.

| # | Beat | Logic (functional) |
|---|---|---|
| M0 | **Setup + baked cutscene "3-01DA"** | `DATLoad("3_01D.DAT", 2)`; player weapon inventory **snapshotted** (`PedSaveWeaponInventorySnapshot` — restored in cleanup); population overridden to a trickle (6 of category 0), vehicles cleared, **all named town stores locked** (`LockStores`: bike shop, two general stores, clothing, comic, two barbers), photo colour upgrade HUD flag on, yearbook-photo callback replaced with a dummy (grotto photos must not feed the yearbook), special entity sets `Rudy1` + `Rudy3` loaded, a 1×2 m pathing hole closed at (489.428, -115.542, 5.4376) (reopened on cleanup — keeps AI off the grotto set, **INFERRED**). |
| M1 | **Ornament hunt** | Rudy spawned asleep/invulnerable at `_3_01D_RUDYSTART` (emotion-towards-player 7 — annoyed/drunk, **INFERRED** enum). Five ornament pickups (model 521) created at `_3_01D_OBJECT1..5` with blips (`PermanentButes`). HUD counter icon `rudy`, 0/5. **Mission timer 185 s.** Per pickup: `Gong` sting, counter +1. **Escalation: at 2 pickups → `F_ElfAttack` (a dog, 219, + elf M + elf F spawn at `_3_01D_JIMMYPHOTO`/`_ELF1`/`_ELF2` and attack, infinite sprint); at 4 pickups → `F_ElfAttack2` (two more elves).** Timer expiry before 5 → fail `3_01D_FAIL_01`. |
| M2 | **Return to Rudy** | At 5/5: objective swaps to the return entry, counter hidden, Rudy blipped; standing within 3.5 m of Rudy latches `bReturnedRudy`: fade, control off, timer stopped, elves deleted, **`PlayCutsceneWithLoad("3-01DB")`** (grotto opens — entity set `Rudy2` swapped in). |
| M3 | **Santa's-lap photo session** (`F_SetupCameraWork`) | Player's vehicle (if any) deleted; Rudy moved to `_3_01D_RUDYTHRONE`, player to `_3_01D_JIMMYPHOTO`; all ambient peds cleared; a mom ped (80, `MomTalker`) spawned at `_3_01D_MOTHER` for reaction barks. Player force-armed with weapon **426** (the camera, **INFERRED** — photography API + colour-upgrade flag), camera preset 2 with rotation limits and no exit prompt, all buttons locked except the shutter (button 12). Counter switches to photos (`HUDIcon_photos`, 0/5). |
| M3a | **Kid cycles** (`F_CfgAnimations` + `F_RunAnimCycle`/`F_KidTransition`) | Five kids (models 69, 66, 68, 137, 138; a sixth model 159 is loaded but unused — **VERIFIED** spare) each own a scripted **sequence of paired Santa/kid anim states** (GOOD_POSE, KID_TALK, SANTA_TALK, STRUGGLE, MAKE_FACE1/2, SIDE_LOOK_SMILE/FROWN, ALMOST_GOOD_POSE1/2 — 8-13 beats per kid) with per-beat speech ids (kid vs Rudy). States advance one per "photo beat" while the kid sits on the lap (grapple-target linked to Rudy). The kid anims call back into the script (`F_SetFaceOn`/`F_SetFaceOff` — **tree-driven good-pose window**, the ch1 conv-tree pattern) setting `bGoodFace`. Between kids: SANTA_END outro beat, kid despawned, next kid spawns at `_3_01D_LINE6`, walks on with SANTA_START + an intro line. After all 5 kids, a **round two** begins: dialogue tables swapped to `speech2` (`F_CfgDialogue2`) and the queue repeats. |
| M3b | **Taking the shot** (`F_CheckGoodPhoto`) | On shutter press: valid iff Rudy is in frame (`PhotoTargetInFrame` at his position + 1 m) **and** `bGoodFace` is on. Valid → counter +1, per-shot caption text (`3_01D_PHOTO_01..05` via `PhotoSetValid(true, …)`), current kid marked passed, kid transition. Invalid → one of two nag captions at random. Mom barks on events (Pass/Nice/Fail/Hurry/Behave categories, random picks). At 5/5 photos → inputs locked, session ends. |
| M4 | **Pass** | Camera freed; if not failed: baked outro **"3-01DC"**, `MissionSucceed(true, true, false, 50)` + `M_PASS` banner 5000 — the [StrategyWiki walkthrough](https://strategywiki.org/wiki/Bully/Chapter_3:_Love_Makes_The_World_Go_Round) lists this mission's reward as **$50**, so banner 5000 = cents and the trailing `50` is almost certainly the same grant in dollars (**INFERRED**, engine signature still **OPEN**); pass music. No respect/clothing/chapter effects; no SMissPass case. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| 185 s elapse before 5 ornaments | M1 timer | `3_01D_FAIL_01` |
| Two full kid rounds without 5 good photos | `F_KidTransition` round-two exhaustion | `3_01D_FAIL_04` (fade + retry) |

Retry: `MissionFail(false, true, …)` / `(true, true, …)` — retry allowed; the round-two fail uses the fade variant. No checkpoints: retry replays from M0 (the photo half is not separately checkpointed).

### Pass effects

In-script only: `M_PASS` 5000 + `MissionSucceed(..., 50)` = **$50** (walkthrough-listed reward, **INFERRED** decode as above). Weapon snapshot restored; camera 426 explicitly destroyed at cleanup (`PedDestroyWeapon(gPlayer, 426)`).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | M0 intro | baked (`3-01DA`) | `fade` + `letterbox` + `say` beat at `_3_01D_PSTART` |
| CS-2 | M2 grotto opens | baked (`3-01DB`) | `fade` + entity-set swap (`Rudy2`) |
| CS-3 | M3 kid beats | in-engine paired anim + dialogue state machine | the anim/dialogue tables are data; needs `pair_anim` + tree-driven callback flags |
| CS-4 | M4 outro | baked (`3-01DC`) | `fade` + `completion_banner` |

Dead code: `F_Scene1` (an unused 6 s fixed-camera Rudy intro, superseded by the baked `3-01DA` — **VERIFIED** uncalled), `F_ModelCleanup` (uncalled).

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 252 | Rudy (Santa) | asleep until M2; photo anchor; grapple-linked to each kid |
| 69, 66, 68, 137, 138 | lap kids ×5 | each with own anim sequence + speech table |
| 159 | (spare kid model) | loaded, never spawned (**VERIFIED**) |
| 80 | MomTalker | reaction barks; flag-113 speech gating |
| `TO_ElfM`/`TO_ElfF` (MODELENUM) | elf attackers ×4 | two ambush pairs at 2 and 4 ornaments |
| 219 | dog | attacks with the first elf wave |

### Animation groups / action trees

`LoadAnimationGroup("Santa_lap")`; `Act/Conv/3_01D.act` holds the full paired state set: `Animations/SANTA_{CYCLE,CYCLE2,LISTEN,TALK,STRUGGLE,ANNOYED,START,END}` × `KidAnimations/KID_{SMILE,NEUTRAL,ANGRY,TALK,LISTEN,LISTEN_SMILE,STRUGGLE,MAKEFACE1,MAKEFACE2,SMILE_SIDE,FROWN_SIDE}`.

### Props / pickups / weapons

- Pickup **521 = ornament** ×5 (`PermanentButes`, blipped).
- Weapon **426 = camera** (forced during M3; destroyed on cleanup).
- Special entity sets `Rudy1`/`Rudy2`/`Rudy3` (grotto states, **INFERRED**: 1 = pre-mission square, 2 = active grotto, 3 = dressing).
- Store doors locked by name: `DT_tbusines_BikeShopDoor`, `DT_tbusines_ClothDoor`, `DT_tbusines_ComicShopDoor`, `DT_tbusines_GenShop1Door`, `DT_tbusines_GenShop2Door`, `DT_tbusiness_Barber`, `DT_tpoor_Barber`.

### Speech

The largest scripted-speech table seen so far: per-kid `speech1`/`speech2` dialogue ids (roughly ids 0–73, 134–135), Rudy intro/outro ids (3–41), mom categories Intro/Fail/LastChance/Hurry/Behave/Nice/Pass (ids 74–132) — **dozens of distinct `M_3_01D` lines**. Player camera bark (`PLAYER_CAMERA_GOOD`, incl. the kid-1-specific `F_GoodCameraPedro` gate — note: `F_GoodCamera` itself is never called in-file, **VERIFIED** dead). Music: `MS_XmasComeRudy{Low,Mid,High}` (hunt), `MS_KidsPlay` (session); `Gong` pickup sting; pass/fail stingers.

### Named world data to extract from `3_01D.dat`

- POINTLISTs: `_3_01D_PSTART`, `_3_01D_RUDYSTART`, `_3_01D_OBJECT1..5`, `_3_01D_RUDYTHRONE`, `_3_01D_JIMMYPHOTO`, `_3_01D_ELF1`, `_3_01D_ELF2`, `_3_01D_MOTHER`, `_3_01D_LINE6`.
- PATHs/TRIGGERs: none referenced in the Lua (the hunt is pickup blips; the session is one arena) — **unlikely but unproven** until extraction.
- Hardcoded: pathing hole (489.428, -115.542, 5.4376, r=1, h=2), dead-code scene camera (492.5634, -117.63233, 6.968617 → 491.74142, -117.06345, 6.941158).
- Area codes seen: **0 only**. DAT `3_01D.DAT` referenced; not in the Lua-only clone.

## 5. Framework gaps (beat → primitives)

Reuses: `pickup_spawner` + `collect_counter` + `Timer`/`countdown_fail_timer` (M1), `wave_group`/`summon_wave_from_doors`-adjacent (elf ambushes at count thresholds — `count_threshold` known), `weapon_inventory_snapshot` (ch2 — known), `temp_outfit_swap` n/a, `door_lock_set` (LockStores = composition over named doors), `pair_anim`, `tree_condition_function` (ch1 — Lua callbacks fired from action trees: `F_SetFaceOn/Off`), `camera` + `fade` + `letterbox`, `world_state_swap`-adjacent (`AreaLoadSpecialEntities` ×3 + pathing hole), `completion_banner`, `spawn_npc` flags (asleep, invulnerable, infinite sprint), `unique_ped_suppress` n/a here, `blip_on_npc`. Photo base layer is shared with 3_01's **photo_mode**.

| Beat | Covered by | New primitives needed |
|---|---|---|
| M3 photo session | **photo_mode** (3_01, same doc set) | **photo_pose_window**: valid shot = target-in-frame AND tree-signaled pose flag (F_SetFaceOn/Off); plus **photo_session_lock**: force camera weapon, camera preset + rotation limits, lock all buttons except shutter, hide exit prompt, per-shot caption strings, colour-upgrade toggle |
| M3a kid sequencer | `pair_anim`, `tree_condition_function` | **paired_anim_state_table**: per-actor-step table (santa state × kid state × optional speaker line) advanced by external events (photo taken), with enter/exit transition nodes and a round-two table swap — the structure is a data-driven cutscene machine, not combat |
| M1 store lockdown | `door_lock_set` | nothing new (seven named doors, one flag) |
| M1 elf ambushes | `wave_group`, `count_threshold` | nothing new (spawn-at-count on the collect counter; dog is just `spawn_enemy`) |
| M0/M4 yearbook gating | — | **yearbook_photo_suppress** (swap the yearbook callback to a no-op for the mission) — tiny; part of photo_mode config |
| M4 reward | `completion_banner`, `Reward` | nothing new once the `MissionSucceed` arg decode lands (**OPEN**) |

Genuinely NEW names introduced here: **photo_pose_window**, **photo_session_lock**, **paired_anim_state_table**, **yearbook_photo_suppress** (plus 3_01's **photo_mode**/**photo_capture_condition**, which this mission extends).

## 6. Open questions

- `MissionSucceed(true, true, false, 50)` argument decode (vs the usual all-false call) — **OPEN**, pending engine signature work.
- How `bGoodFace` timing actually feels: set/clear callbacks live in the kid anim tree (`3_01D.act`), so pose windows are authored in the clips — extraction of that tree is required to reproduce shot difficulty.
- `PhotoHasBeenTaken()` is read but the count is driven by raw button-12 presses — the photo system's real shutter semantics (does a "bad" press still consume film/show a flash?) — engine-side, **OPEN**.
- Whether ornaments can be collected in any order (yes — five independent pickups, **VERIFIED** structurally) and whether elf waves spawn even if the player rushes pickups 2 and 4 back-to-back (both queued, **VERIFIED** — no dedup guard).
- `PedSetEmotionTowardsPed(Rudy, gPlayer, 7)` — emotion enum 7 **OPEN**.
- Kid model 159 loaded but unused — cut sixth kid, **INFERRED**.
- Where the mission is offered (Christmas availability chain off `3_XM`) — **OPEN**, world mission table.
