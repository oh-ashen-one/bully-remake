# 1_11x1 — "Halloween", part 1 (dorm, costume, walk out)

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources (read-only, nothing copied): decompiled Lua `Bully-SE-scripts/scripts/chap1/1_11x1.lua` (302 lines), cross-refs `SMissPass.lua` (`F_SetupMissionRequirements`), `main.lua` (`F_CreateHalloweenItems` ~line 907, load-time ally respawn ~line 322), `SGlFunc.lua` (`F_SetupHallowenPeds` ~line 1097). This document describes structure and logic in our own words; no dialogue, cutscene script or narrative text is reproduced. Script names are citations only.

## 1. Identification

**This is the first half of the catalog mission "Halloween"** (the time-stopped Halloween-night mission), paired with `1_11xp` (the prank half). Confidence: **VERIFIED**.

Evidence:
- Baked cutscene id `"1-11"` (mission 1_11 naming), `PlayCutsceneWithLoad("1-11", ...)`.
- Player outfit `"Halloween"` is force-equipped during the mission (`ClothingSetPlayerOutfit` + `ClothingBuildPlayer`) and permanently granted on pass (`ClothingGivePlayerOutfit` in `MissionCleanup`).
- Interactive music streams `MS_HalloweenLow/Mid/High.rsm` throughout.
- Setting is the boys' dorm at night (area 14), Gary (model 160, a Halloween-costume variant — see NPCs) recruited as ally; Halloween dressing set `Halloween3` activated.
- Placement in the unlock chain (**VERIFIED**, `SMissPass.lua` + `main.lua:907-932` `F_CreateHalloweenItems`): `Halloween1` dressing after 1_07+1_08; `Halloween2` dressing + **game clock paused** after 1_09 (The Candidate); 1_11x1 becomes available after that; passing 1_11x1 sets `shared.g1_11X1JustFinished` and `gHalloweenActive`.

Chain position: `1_09 (The Candidate)` → **1_11x1** → `1_11xp` → (optional) `1_11x2` → `1_11_Dummy` marks the night over. The launch of 1_11xp after x1 passes is driven outside the Lua (mission table / main loop); `MissionCleanup` calls `MissionDontFadeInAfterCompetion()`, consistent with a direct hand-off. **INFERRED** for the chaining mechanism.

## 2. Mission flow

Stage structure is unusual: `main()` runs `F_SetupMission()` then `F_Stage2()` directly — there is no stage 1 in this file, and a thread `T_MissionStartText` (objective `1_11X1_MOBJ01`) exists but is never started. Treat stage 1 / objective 01 as cut content; only the flow below is live. (**VERIFIED** dead code.)

| # | Beat | Logic (functional) |
|---|---|---|
| A0 | **MissionSetup** | Loads `1_11X1.DAT` (events), deletes any persisted Gary/Petey allies (`shared.gGary/gPetey`). If `shared._1_11X1entereddorm` is set (player re-entered the dorm — set outside this script, **OPEN** where), the player is respawned downstairs at `POINTLIST._1_11X1_SPAWNPLAYERDOWNSTAIRS` (area-transition into area 14 if needed). No fade-in. |
| A1 | **World/night setup** (`F_SetupMission`) | All patrol paths disabled; `gHalloweenActive = true`; `F_SetupHallowenPeds(false)` swaps the population roster via `PedSetUniqueModelStatus` (enables Halloween-costume variants 161-174 at various spawn counts, disables ~20 normal student models incl. 69, 26, 11, 40, 17, 39, 38, 12, 139, 102, 73, 27, 7, 66, 68, 74, 71, 70, 137, 138 — **VERIFIED** list); PA announcements stopped; `Halloween3` special entities loaded; Gary's model (160) loaded; player clothing backed up. |
| A2 | **Costume + intro cutscene** (`F_Stage2_Setup`) | Halloween interactive music starts (low/mid/high layers). Clothing manager locked; player forced into the `Halloween` outfit and rebuilt. Blip placed on `POINTLIST._1_11X1_BLIPEXIT`. Baked cutscene `1-11` plays (first-attempt-only behavior not visible here, **OPEN**). Player moved to `POINTLIST._1_11X1_SPAWNPLAYER`. Models requested: peds 165, 170, 75; weapons 411, 372, 397; anim group `3_04WrongPtTown` (reused laugh/shove anims, **INFERRED**). Gary, Pete, and the two laughing bullies are spawned (see NPCs). `gHCriminalsActive` set true (enables Halloween-night mischief peds elsewhere, per `main.lua:910`). Arcade screens toggled. Fade in. |
| A3 | **Objective: head out with Gary** | Objective `1_11X1_MOBJ02` + blip on the exit point; Gary plays his opening line (speech event 7). Gary is a recruited ally carrying weapon 411 and is mission-critical. Pete sits invulnerable in a trash-can idle loop at `POINTLIST._1_11X1_SPAWNPETE`. Two bully peds (models 170 and 75) loop laughing/shoving anims at `SPAWNLAUGH01/02`. |
| A4 | **Laughing pair released** | When the player enters `TRIGGER._1_11X1_BULLIES`, both laughing peds are cleared of their action nodes and made ambient (they stop the scripted mockery and become normal peds). One-way, no objective attached. |
| A5 | **Pete joins** | When the player enters `TRIGGER._1_11X1_PETE` (and Gary is alive), a thread runs: Pete line (event 8) + trash-can exit anim, Pete made vulnerable, Pete recruited as Gary's ally; then a four-line exchange (Pete 8 → Gary 9 → Pete 10 → Gary 6), each gated on the previous line finishing. |
| A6 | **Pass: reach the outdoors** | The moment `AreaGetVisible() == 0` (player has left the dorm into the campus exterior), Gary and Pete are made ambient and the mission passes. No end trigger — the completion check is just "area 0 visible". **VERIFIED** and unusual. |
| A7 | **Pass side-effects** | Fade out, `MissionSucceed`. Rewards: outfit `"Halloween"` permanently granted (script), plus `SMissPass.lua`: ammo 394 ×5 and 397 ×5 if the player holds none (both are prank weapons used in 1_11xp — itching-powder and fountain-firework family, names **INFERRED** from xp usage; treat as numbers otherwise). Sets `g1_11X1JustFinished`; re-creates special entities. |

**Fail conditions:**
- Gary KO'd (mission-critical callback `F_MissionCriticalGary`) → fail message variant `1_11X1_FAIL_01`. That is the only fail path. **VERIFIED**.
- On fail cleanup: player weapon 411 destroyed if held, all clothing-slot locks released, backed-up clothing restored, `shared.b1x11_failed = true`, player respawned at `POINTLIST._BOYSDORM_1_11_RESPAWNPLAYER` facing 315°, camera reset.

**Retry/restart:** `MissionDontFadeIn` on setup; no attempt-count cutscene skip logic in this file (**OPEN** whether the `1-11` cutscene is skipped on retry — likely handled by the cutscene system, not Lua).

## 3. Cutscene points

| Marker | Where in the flow | Type | Replace with |
|---|---|---|---|
| CS-H1 | A2, cutscene `1-11` (Gary gives the costume / sets up the night) | baked | Original: letterbox + fade in on Gary in the dorm, 1-2 `say` lines, fade out. Player already in Halloween outfit afterwards (do the outfit swap under the fade). |
| CS-H2 | A5 Pete trash-can exchange | in-engine (anims + speech, no camera moves) | Keep as gameplay-with-VO: `say` sequence + a trash-can prop anim; or a single static `camera` + `letterbox` if the prop anim isn't built. |

## 4. NPCs and assets needed

Ped models (numbers as scripted, names only where context makes them obvious):

| Role | Model id | Notes |
|---|---|---|
| Gary (Halloween costume variant) | 160 | **VERIFIED** as Gary (`shared.gGary`, recruited ally, mission-critical). Costume-variant id; base Gary is a different id. Carries weapon 411. |
| Pete/Petey (Halloween costume variant) | 165 | **VERIFIED** as Petey (`shared.gPetey`). Trash-can idle/exit anims. |
| Laughing bully A | 170 | Costume variant; id only, **OPEN**. |
| Laughing bully B | 75 | Russell's model id per `default.ide` mapping in the opening-slice doc — **INFERRED** this is Russell mocking the costume. |

- Animation groups: `3_04WrongPtTown` (loaded), `Act/Conv/1_11X1.act` action tree (nodes `/Global/1_11X1/empty`, `/Animations/PeteCan/Cycle`, `/Animations/PeteCan/GetOut`, `/Animations/Laugh_Shove/Laugh`, `/Animations/LaughCyclic/LaughCyclic`).
- Weapons referenced: 411 (Gary's prop weapon; player may hold it — destroyed on fail; identity **OPEN**), 372, 397 (requested models; both are prank items used in 1_11xp).
- Speech: **5 scripted events** (`M_1_11X1` ids 6, 7, 8, 9, 10), sequenced with wait-for-speech gates.
- Special entities: `AreaLoadSpecialEntities("Halloween3", true)` (+ ensure-created).
- Music: `MS_HalloweenLow/Mid/High.rsm` interactive streams.
- Props: trash can Pete sits in (likely part of `Halloween3` dressing or dorm geometry, **OPEN**); arcade-screen geometry toggles for area 14 (in `SGlFunc.lua`).

Named world data to extract later (from `1_11X1.DAT` / `BoysDorm.dat`, coordinates NOT yet read):
- Points: `POINTLIST._1_11X1_SPAWNPLAYERDOWNSTAIRS`, `_1_11X1_SPAWNPLAYER`, `_1_11X1_SPAWNGARYBED`, `_1_11X1_SPAWNGARY`, `_1_11X1_SPAWNPETE`, `_1_11X1_SPAWNLAUGH01`, `_1_11X1_SPAWNLAUGH02`, `_1_11X1_BLIPEXIT`, `_BOYSDORM_1_11_RESPAWNPLAYER`.
- Triggers: `TRIGGER._1_11X1_BULLIES`, `TRIGGER._1_11X1_PETE`.
- Areas seen: 14 (`isc_dorm`), 0 (outdoors/`tschool`).

## 5. Framework gaps

Covered by existing primitives: trigger_enter (A4, A5), teleport (A0/A2), fade, say (speech lines), letterbox, reward (outfit/ammo via SMissPass equivalent), set_flag (shared flags), spawn_enemy → generalize to spawn_npc.

New primitives/conditions needed:
1. **recruit_ally / dismiss_ally** — ally chain (player→Gary, Gary→Pete), ally follows leader.
2. **mission_critical_npc fail condition** — fail callback when a named NPC is KO'd.
3. **area_visible condition** — pass/fail/branch on which area code is visible (here: pass when area 0).
4. **outfit force + restore** — backup player outfit, force mission outfit, clothing-slot lock flags, restore on fail; permanent outfit grant on pass.
5. **population roster swap** — per-mission override of which ped models may spawn (`PedSetUniqueModelStatus` equivalents) and disable-all-patrol-paths.
6. **special-entity dressing sets** — named world-dressing packages (Halloween1/2/3) toggled on/off, idempotent ensure-created.
7. **interactive music layers** — low/mid/high intensity stream set for the mission, stopped in cleanup.
8. **npc_anim_loop / anim_once on NPC** — drive an NPC into a named looping or one-shot animation state from the mission tree.
9. **speech_sequence gate** — ordered multi-speaker VO with wait-for-finish between lines (we have `say`; need chaining + cancel-on-NPC-death guards).
10. **cutscene-skips-on-retry policy** — attempt-count awareness (which baked cutscene plays only on attempt 1). Exact retry behavior here is OPEN but the framework needs the slot.

## 6. Open questions

- How is 1_11x1 launched from the world, and what launches 1_11xp immediately after (mission table outside Lua)?
- Where is `shared._1_11X1entereddorm` set (dorm-entry script during Halloween night)?
- Is cutscene `1-11` skipped on retry, and by what mechanism?
- Identity of weapon 411 (Gary's carried item across all three Halloween scripts).
- Where does the dorm-exit actually hand control to 1_11xp (same frame? next frame?), given the pass check is just `AreaGetVisible() == 0` — a player who walks in/out of the dorm door during x1 could pass "early". Whether the mission locks the dorm exit until A3's objective is **OPEN** (no door lock is set in this script; the blip suggests the exit is the intended goal immediately).
