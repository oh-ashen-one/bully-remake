# 3_07 — Christmas Is Here (intro beat): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_07.lua` (72 lines, complete) + cross-refs `chap2/Chapt2Trans.lua`, `SGlFunc.lua`, `main.lua`. Structure described in our own words; no dialogue text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_07 = the opening beat of "Christmas Is Here"** (canonical Chapter 3 mission #27, SE-exclusive, auto-fired at chapter start) — **INFERRED**, because no other Lua file ever references a mission id `"3_07"` (grepped the whole corpus: the only outside reference is `Chapt2Trans.lua` loading `3_07.DAT` for its player-spawn point). The content matches the known shape of the mission: the chapter-2→3 transition (`Chapt2Trans`) sets the winter world, then a wake-up beat places Jimmy in the boys' dorm (area 14) where three younger students chat about the holidays; the canonical mission then continues with the PA summons and the principal's-office scene, which live in `SGlFunc.T_ChristmasdLogic` and `3_08.lua` respectively. It is plausible `3_07.lua` is the script behind the `"3_08_Launch"` mission id that `SGlFunc.lua` force-starts right after `Chapt2Trans` — that id has no same-named script file in the dump — but registration is engine-side, so this stays **OPEN** (see §6).

Setting: boys' dorm interior, area code **14** — **VERIFIED** via `AreaTransitionPoint(14, ...)` and the `SGlFunc` alarm table, which maps area 14 to the dorm coordinates. Mission triggers/geometry live in `3_07.DAT` — **trigger data present: YES** (`DATLoad("3_07.DAT", 2)`; the same DAT is also loaded by `Chapt2Trans`, so it doubles as the chapter-start point file).

## 2. Mission flow

Driver: single linear `main()` with one polling loop. No objectives, no blips, no fail path.

| # | Beat | Logic (functional) |
|---|---|---|
| M0 | **Splash card + dressing** (`MissionSetup`) | Full-screen splash displayed ~2.5 s (`SplashScreenDisplay("Chapter2", "Chapter2")`) between fade-out/in; given `ChapterSet(2)` in `Chapt2Trans` is the zero-based index for Chapter 3, the "Chapter2" texture is **INFERRED** to be the Chapter III title card shown on wake-up. Loads anim group `Hang_Talking`, loads + inits `3_07.DAT`, teleports the player to `POINTLIST._3_07_PLAYER_SPAWN` (area 14, dorm). No fade-in suppression — the mission fades itself in. |
| M1 | **Spawn the chat group** | Three peds created at named points: Pedro (model 69, `_3_07_PEDRO`), Justin (model 34, `_3_07_JUSTIN`), Trevor (model 73, `_3_07_TREVOR`). Names come from the script's variable names (**VERIFIED** as labels); model-id-to-character identity unchecked against `default.ide`. Then `main()` blocks until the player enters `TRIGGER._3_07_STARTCONV` — the "walk up to the group" gate. |
| M2 | **The Christmas chat** | `F_ChristmasChat` queues six timed subtitle/speech lines (`QueueText("3_07_Speech01..06", 3/3/3/5/5/3 s, ...)`), the last with completion callback `CB_ENDIT`. Justin plays the talking node, Pedro and Trevor listening nodes (all from `Act/Conv/3_07.act`). `UpdateTextQueue()` is pumped in the main loop. Functionally: the player overhears the boys discussing Christmas — no interaction. |
| M3 | **End conditions (all PASS)** | The loop exits when **any** of: (a) the queue finishes (`CB_ENDIT` fires after a 3 s tail), (b) any of the three peds is hit by the player (`PedIsHit(ped, 2, 1000)` — hit type 2, 1 s window), or (c) the player leaves the dorm (`AreaGetVisible() == 0`). **Note: hitting the boys or walking out still succeeds the mission** — these are early-out scene breakers, not fails. There is no `MissionFail` call anywhere in the file — **VERIFIED** fail-free. |
| M4 | **Pass side effects** | Text queue cleared; all three peds switched back to the generic AI tree (`/Global/AI`, `Act/AI/AI.act`) and released to ambient (`PedMakeAmbient`); `PlayerSetScriptSavedData(17, 1)` (persistent save slot 17 — consumer unknown, **OPEN**; likely a "chapter-3 intro seen" flag, cf. slot 15 in 1_03, slot 23 checkpoint in 2_04); mission-end music stinger 10; `MissionSucceed()`. |

Cleanup (`MissionCleanup`): unloads `3_07.DAT` and `Hang_Talking`. The peds are *not* deleted — they persist as ambient students, consistent with the dorm-hallway dressing.

### Sequence context (VERIFIED from cross-refs)

`Chapt2Trans` (chapter 2→3 transition) → winter world + `Christmas` special entities → `SGlFunc` force-starts `"3_08_Launch"` and runs `T_ChristmasdLogic` (PA announcement + "go to the principal" prompt loop) until `3_08` activates → `3_08` (office scene) → `3_08_PostDummy` marker strips the Christmas dressing. 3_07's beat fits between the transition and the office scene; exactly which id launches it is **OPEN** (§6).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | M0 splash | engine splash screen | `chapter_title_card` (existing, from ch1/ch2 transitions) |
| CS-2 | M2 chat | in-engine ambient, no cameras | three `spawn_npc` + `ped_action_node` (talk/listen pair) + a `speech_sequence`/timed-text queue; trigger gated on `trigger_enter` |

Anim groups: `Hang_Talking`. Action tree: `Act/Conv/3_07.act`.

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 69 | "Pedro" chat member | listening anim; hit-watch early-out |
| 34 | "Justin" chat member | the talker; same id as 2_06's prep Justin — do not assume same character, **OPEN** |
| 73 | "Trevor" chat member | listening anim; hit-watch early-out |

### Speech / text

Six queued lines `3_07_Speech01..06` (timed 3/3/3/5/5/3 s). Pass stinger id 10. No interactive music.

### Named world data to extract from `3_07.DAT`

- POINTLISTs: `_3_07_PLAYER_SPAWN` (area 14), `_3_07_PEDRO`, `_3_07_JUSTIN`, `_3_07_TREVOR`, `_3_07_PLAYERPOOR` (used by `Chapt2Trans`, area 0 — the same DAT serves the chapter transition).
- TRIGGERs: `_3_07_STARTCONV`. (Note: `Punishment/P_Snow.lua` also references `TRIGGER._SNOWPILE3_07` — a snowpile prop trigger namespaced to this file stem, likely part of the shared winter dressing rather than this mission's logic, **OPEN**.)

## 5. Framework gaps (beat → primitives)

Everything in this mission is covered by the known ch1/ch2 primitive set and the `MissionSubsystem.h` schema; **no new primitives required**.

| Beat | Covered by | New primitives needed |
|---|---|---|
| M0 splash card | `chapter_title_card` | none |
| M1 spawn + trigger gate | `spawn_npc`, `trigger_enter` | none |
| M2 timed chat + talk/listen anims | `speech_sequence` / `conversation_beat_machine`, `ped_action_node` | none |
| M3 hit early-out | `ped_hit_event` | none — but note the **variant**: hit/leave ends the scene and still *passes* (end-scene-and-succeed, not fail). Our stage machine needs a "complete stage on event X" usage of existing conditions, which `complete: {event}` already expresses |
| M3 area-change early-out | `area_visible_eq` (ch1 docs) | none |
| M4 release peds | `npc_despawn` / `persist_npc_across_mission` family; `npc_ai_tree_override` (3_05) for the `/Global/AI` tree swap | none |
| M4 save slot | `script_checkpoint` family (persistent save slots, 2_04) | none — slot 17 semantics **OPEN** |

## 6. Open questions

- Which mission id actually runs `3_07.lua`. No `ForceStartMission("3_07")`/star reference exists in the Lua corpus; `"3_08_Launch"` has no script file either. Best hypothesis: `3_07.lua` *is* `3_08_Launch` (id ↔ file mapping is engine-side, and the beat order fits), but it could equally be star/sleep-triggered through DAT data we haven't extracted.
- What consumes `PlayerSetScriptSavedData(17, 1)` (no reader found in the corpus).
- Whether the "Chapter2" splash texture is really the Chapter III card (zero-based naming) or a reused/mislabeled asset.
- Whether the player can simply never enter `_3_07_STARTCONV` (the mission would idle forever — no timeout; presumably the launch flow guarantees the spawn puts the player in front of it).
- The exact beat boundary vs. the dorm wake-up/PA beats owned by `SGlFunc.T_ChristmasdLogic` — i.e. whether 3_07's success is what enables the PA loop's exit condition (`MissionActiveSpecific("3_08")`).
