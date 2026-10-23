# 3_08 (+ 3_08_Dummy) — Christmas Is Here (the sweater): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_08.lua` (255 lines, complete) + `chap3/3_08_Dummy.lua` (21 lines, complete) + cross-refs `SGlFunc.lua` (`T_ChristmasdLogic`, `F_MakePlayerSafeForNIS`), `main.lua`, `STimeCycle.lua`, `SMissPass.lua`. Structure described in our own words; no dialogue or cutscene text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_08 = "Christmas Is Here"** (canonical Chapter 3 mission #27, SE-exclusive, auto on chapter start) — **VERIFIED** by content and reward: the script is a single in-engine scene in the principal's office where the school secretary (model 59, loaded under the group name `"secretary"` — Miss Danvers, **INFERRED** from role) hands Jimmy a Christmas present from his mother and force-equips the `SP_XmsSweater` clothing item. Canonical sources ([StrategyWiki ch.3](https://strategywiki.org/wiki/Bully/Chapter_3:_Love_Makes_The_World_Go_Round), [Bully Wiki](https://bully.fandom.com/wiki/Christmas_Is_Here)) describe exactly this: start at the principal's office, reward = the reindeer sweater. The wake-up/PA-summons beats that canonical guides include in this mission are *not* in this file — they live in `SGlFunc.T_ChristmasdLogic` (PA announcement `PA_CHRISTMAS_JIMMY` + `GOTO_PRINCIPAL` prompt loop, **VERIFIED**) and probably `3_07.lua` (see its own note). Dead locals in 3_08 (`gPAAnnouncementTimer`, `bGetOutOfBed`/`F_PlayerGetOutOfBed`, `bPlayerGoToPrincipal`, `gMissionTimer`) are **VERIFIED** leftovers showing those beats were once planned in-file and were moved out to the global scripts.

Setting: principal's office interior. Trigger data: **YES** — `3_08.DAT` plus `PriOffice.DAT` (the shared office point file), both `DATLoad(..., 2)`.

`3_08_Dummy.lua` is a companion buffer mission for the same sequence, covered in §2b.

## 2. Mission flow

Driver: linear `main()` — `F_SecretarySetup()` → `F_StartNIS()` (the skippable in-engine scene) → sweater reaction anim → succeed. No objectives, no blips, **no fail path** (`F_MissionFail` exists but only writes an unused string state; `MissionFail` is never called — **VERIFIED** fail-free).

| # | Beat | Logic (functional) |
|---|---|---|
| M0 | **Setup** (`MissionSetup`) | No auto fade-in (`MissionDontFadeIn`), mission-name banner suppressed (`MissionSurpressMissionNameText`). Anim groups: `Try_Clothes`, `NIS_3_08`, `MINI_Lock` (last one never used in-file — dead load, **OPEN**). Unique model 59 suppressed so no ambient Danvers duplicate. `shared.gPrincipalCheck = false` (gates office-door/secretary ambient logic elsewhere, **INFERRED**). Loads ped model 59, weapon/pickup model **436** (the gift box, **INFERRED** from `PickupCreatePoint(436, _3_08_PRESENT)`), conv tree `Act/Conv/3_08.act`, and activates the school population trigger `_3_08_SCHOOLPOP`. |
| M1 | **Secretary setup** (`F_SecretarySetup`) | Danvers spawned at `POINTLIST._SECRETARY`, registered with `LibPed` as an NIS ped with an `cbAttacked` hook (`F_FireSecretaryNIS` — sets a flag nothing in this file consumes; the actual ped-watching loop `T_PedMonitor` comes from `Library/LibPed.lua`, **VERIFIED**). Emotion-toward-player set to 7 (friendly/max, **OPEN** semantics). She walks `_SECRETARYOFFICEPATH` until the scene takes over. |
| M2 | **Gift NIS** (`F_StartNIS` + `F_XmasNISCore`) | Control off, fade out. The gift pickup (436) is created permanent at `_3_08_PRESENT`. `F_MakePlayerSafeForNIS(true)` — the shared composite (from `SGlFunc.lua`): enter-NIS mode, speech tree off, weapon snapshot + strip, explosions/projectiles cleared, player invulnerable, punishment system disabled, ambient attacks stopped, ambient peds ignore stimuli. Punishment points zeroed, area peds cleared, player unequips and waits for it. Widescreen on; player placed at `_3_08_DESTINATION`, Danvers repositioned to `_3_08_DNISPOS`; fixed camera (FOV 80, hardcoded eye/look). Then the **fully skippable** scene core: (1) Jimmy walks in along `PATH._3_08_JNISPATH` (last-node callback gates progress); (2) Jimmy waves at her + scripted line (`M_3_08` id 2); (3) she gets up from her desk (id 1); (4) she hands over the gift (id 3); (5) Jimmy takes it (id 4); (6) gift-opening anim (id 5). Each step = camera cut (FOV 40, hardcoded positions) + `PedSetActionNode` + `SoundPlayScriptedSpeechEvent`, polled with `WaitSkippable(1)` — any skip input breaks straight to M3. |
| M3 | **Post-scene** | Fade out, gift pickup deleted, speech tree re-enabled. **The reward:** `ClothingPlayerOwns`/`ClothingGivePlayer`/`ClothingSetPlayer` + `ClothingBuildPlayer` — the `SP_XmsSweater` is granted *and force-equipped on the spot* (canonical "Cheerful Reindeer sweater"). Player made unsafe, teleported to `_3_08_AFTERPRES` facing 180, camera released, fade in, sound/music fades restored. |
| M4 | **Sweater reaction** | Jimmy plays `/Global/3_08/NewSweater` (a reaction anim — the canonical "students laugh at the sweater" beat, **INFERRED**); `main()` waits for the anim to finish, drops widescreen, sets the clock to **8:30**, and `MissionSucceed(false, false, false)`. |

Cleanup (`MissionCleanup`): school population trigger deactivated, default population restored, clock unpaused, Danvers released to ambient **and persisted** via `shared.gSecretaryID` (reused by later scripts, **INFERRED**), `shared.gPrincipalCheck = true`, anim groups unloaded, DAT unloaded.

### Fail conditions and retry

None. No violence gate, no timer, no area fail — interrupting isn't possible because control is taken before the scene and the secretary is invulnerable during it. Retry semantics irrelevant (no `MissionFail` path); the `LibPed` `cbAttacked` hook would only matter if the player could attack her before control is removed — a razor-thin window that sets a dead flag. **VERIFIED** as written.

### Pass side effects (VERIFIED from cross-refs)

- `SGlFunc`: on 3_08 completion, `"3_08_PostDummy"` is force-completed and the `Christmas` special-entity set is unloaded — **the winter/Christmas world dressing ends here**.
- `STimeCycle.lua`: while 3_08 is complete but `3_08_PostDummy` is not, class start/end/attend/miss hooks all early-return — the class/curfew system is suppressed during the Christmas interstitial.
- `SMissPass.lua` (2_B branch): passing the chapter-2 boss sets chapter index 2 and loads the `Christmas` entities only if 3_08 is not yet complete — i.e. 3_08 completion is the system's "Christmas is over" flag.

## 2b. 3_08_Dummy — the launch buffer

21 lines, no DAT, no peds. `MissionSetup`: if `"3_08_Launch"` is currently active, make the player safe-for-NIS, drop control, pause the game clock. `main`: widescreen off, fade in, wait ~2.5 s, succeed silently. `MissionCleanup`: control back, safety off. **INFERRED** purpose: a short marker/buffer mission that holds the player in a protected state during the Christmas launch handoff (between `Chapt2Trans` and 3_08 proper) so the wake-up chain can't be interrupted by ambient systems; the `"3_08_Launch"` id it keys on has no script file in the dump (see the 3_07 note, §6 there — registration is engine-side, **OPEN**). Note its cleanup runs even on abort, so it doubles as a safety-reset.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | M2 walk-in + gift exchange | in-engine, 6 fixed cameras, skippable | `letterbox` + `player_follow_path` + `camera` cycle + `say`/per-line speech gated on speech-end; **needs a skip-input abort** on the whole beat chain (`sequence_skip_guard` family) |
| CS-2 | M4 sweater reaction | in-engine, no camera work | one `ped_action_node` on the player + `timer`; trivial |

Anim groups: `Try_Clothes`, `NIS_3_08` (`MINI_Lock` dead). Action tree: `Act/Conv/3_08.act`. Hardcoded cameras (game coords): (−630.64, −290.56, 7.53)→(−631.43, −290.01, 7.27) FOV 80; then FOV 40 shots at (−632.57, −284.86, 7.58)→(−632.93, −285.76, 7.32), (−632.70, −291.19, 7.31)→(−633.01, −290.25, 7.18), (−632.68, −286.58, 7.32)→(−633.05, −287.48, 7.13).

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 59 | school secretary (Miss Danvers, **INFERRED**) | invulnerable during NIS; path walker pre-scene; persists post-mission via `shared.gSecretaryID` |

### Weapons / items / clothing

- Pickup/weapon model **436** = the Christmas present prop (**INFERRED**; created at `_3_08_PRESENT`, `PermanentButes` persistence, deleted after the scene).
- Clothing `SP_XmsSweater` — granted + force-equipped on pass (**the** reward; no money, no respect, **VERIFIED** by absence).

### Speech

Scripted `M_3_08` ids 1–5 (secretary 1/3/5, Jimmy 2/4). Ambient PA event `PA_CHRISTMAS_JIMMY` lives in `SGlFunc`, not this file. No music streams; pass/fail stingers n/a (fail-free).

### Named world data to extract from `3_08.DAT` / `PriOffice.DAT`

- POINTLISTs: `_SECRETARY`, `_3_08_PRESENT`, `_3_08_DESTINATION`, `_3_08_DNISPOS`, `_3_08_AFTERPRES`.
- PATHs: `_SECRETARYOFFICEPATH`, `_3_08_JNISPATH`.
- TRIGGERs: `_3_08_SCHOOLPOP` (population activation trigger).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| M0 presentation control | — | **mission_presentation_flags** (suppress auto fade-in and the mission-name banner on start; both are per-mission engine options) |
| M0 population trigger | `population_override` | **population_trigger_toggle** (activate/deactivate a *named* population trigger, vs. the wholesale override we already have) |
| M1 NIS-ped registration | `mission_critical_npc`, `ped_action_node` | none new — but the LibPed pattern (bNISPed + cbAttacked + monitor thread) is a reusable **npc_scene_guard** bundle: watch a scene NPC for player hits during setup and fire a callback; here the callback is dead, so a flag-only version suffices |
| M2 player safety composite | `ped_set_invulnerable`, `weapon_strip`, `weapon_inventory_snapshot`, `heat_set`(off) | **player_safe_mode** (one composite action: control lock + invulnerable + ignore-stimuli + weapon snapshot/strip + clear projectiles + disable punishment + hush ambient peds — mirrors `F_MakePlayerSafeForNIS`, used by 3_08, 3_08_Dummy, 3_XM and later missions). Overlaps 3_01A's **cinematic_lock** (clock pause + punishment disable + untargetable + widescreen): `player_safe_mode` is the fuller library composite; likely one primitive with option flags |
| M2 skippable scene core | `camera`, `say`, `player_follow_path`, `letterbox` | **skippable_scene_chain** — extend `sequence_skip_guard`: every beat polls skip input and aborts the whole remaining chain to a recovery point (here M3); not per-line skip like `speech_sequence` already has |
| M2 scripted per-ped speech lines | `speech_sequence`, `conversation_beat_machine` | none new (`SoundPlayScriptedSpeechEvent` + `SoundSpeechPlaying` poll = the same shape) |
| M2/M3 camera-linked audio | — | **camera_linked_audio_fade** (duck/restore sound + music with the cutscene fades; one flag on `fade`/`camera` would do) |
| M3 clothing grant + force equip | `give_clothing` (ch2), `temp_outfit_swap` | none new — `give_clothing` plus an **equip-now** option and model rebuild |
| M3 punishment zeroing | `punishment_points_add` | none (add a clear/zero variant) |
| Dummy: cross-mission active gate | — | **mission_active_query** (condition/action branching on `MissionActiveSpecific(<id>)` — the Dummy is inert unless `3_08_Launch` is concurrently active) |
| Dummy: clock pause | `clock` action (schema) | none |
| pass side effects | `force_complete`, `world_state_swap`/`geometry_instance_toggle`, `special_entity_set_load` (3_01C) | none new — "Christmas over" = force-complete marker id + unload a special-entity set + a campaign flag other systems query |

## 6. Open questions

- What the `MINI_Lock` anim group was for in this mission (lockpicking? door unlock anim during the scene?) — loaded, never referenced.
- Emotion id 7 semantics (`PedSetEmotionTowardsPed`) — value table unknown.
- `shared.gPrincipalCheck` consumers — presumably the principal's-office door/secretary ambient logic (area scripts), not yet traced.
- `F_FireSecretaryNIS`/`bStartSecNIS`: dead in-file; was there an attack-the-secretary alt branch in a cut build?
- Whether `3_08_Launch` maps to `3_07.lua`, a missing script, or an engine-generated launcher — same OPEN as the 3_07 note.
- The exact star/availability wiring that activates `3_08` at the office door (the PA loop exits on `MissionActiveSpecific("3_08")`, so something enables that star — likely DAT/data-side).
