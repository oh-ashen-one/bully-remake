# 1_G1 — The Diary: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/1_G1.lua` (413 lines) + `SMissPass.lua`. Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**1_G1 = "The Diary"** — **VERIFIED** by the loaded animation group `1_G1_TheDiary` and by content: sneak into the school after hours through a window, take back a confiscated diary (item 483) from the staff room, and return it to Beatrice (ped model 3) outside. Matches the canonical list entry "The Diary (optional, 19-23)" — the evening availability fits the locked-doors/stealth design. No surprise.

Note the DAT file is named **`1_G1_new.DAT`** (not `1_G1.DAT`) — use that name when extracting from `Trigger.img`.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` running a linear objective-function chain `O_EnterWindow → O_GetDiary → O_LeaveSchool → O_FindBeatrice → O_KissBeatrice`, plus threads `T_MissionFail` and `T_WindowBlip`.

| # | Beat | Logic (functional) |
|---|---|---|
| D0 | **Baked cutscene "1-G1"** | `PlayCutsceneWithLoad("1-G1", true, true)` in `MissionSetup`. `1_G1_new.DAT` loaded, no fade-in. Anim groups loaded: `MINI_Lock` (twice, as scripted), `1_G1_TheDiary`, `1_03The Setup`, `1_07_SaveBucky` (the last two **INFERRED** as shared struggle/social anim reuse). Item models 483 (diary) and 432 (**OPEN**) requested; Beatrice's model (3) made unique. No attempt-count guard in this file; retry-skip **INFERRED** to the global cutscene manager. |
| D1 | **Evening setup** | Social override trees loaded (`Mission/1_G1WantGift.act`, `Mission/1_G1Follow.act`). Control off, player transitioned to area 0 at `_1_G1_PLAYERSTART`. Stealth music streams armed (`MS_FootStealth*`). **All school doors locked** (front, both back, both side-front — `DT_TSCHOOL_SCHOOLFRONTDOORL`, `SCHOOLLEFTBACKDOOR`, `SCHOOLLEFTFRONTDOOR`, `SCHOOLRIGHTFRONTDOOR`, `SCHOOLRIGHTBACKDOOR`, plus `DT_tschool_GirlsDormL`); the window doors are opened instead (`shared.gWindowsOpen` gates `DT_tschool_ExtWind` / `DT_tschool_HallWind`). |
| D2 | **Break in** (`O_EnterWindow`) | Objective 1 (`1_G1_OBJ1` — get inside, **paraphrase**), blips on the climbable lattice `_1_G1_LATTICE` and the window `_1_G1_WINDOWENTRY`. Beatrice (3) spawned at `_1_G1_BEASPAWN` and socialized: mission-critical with a fail callback, warm emotion toward the player, required gift = the diary (gift id 19), social responses overridden. Control on, fade in. `T_WindowBlip` swaps the guidance blip between the window (outdoors) and the current objective point (indoors) until the diary is held. Objective completes when the visible area becomes 2 (`ischool`). |
| D3 | **Get into the math room** (`O_GetDiary`, part 1) | Objective 5 (`1_G1_OBJ5`), blip on `_1_G1_MATHCLASS`; if the player has been spotted by authority (ped type 3 — prefect/teacher, **OPEN** exact type), all ambient peds are cleared. A prop proxy `PAnimCreate(TRIGGER._1_G1_MATHPROXY)` gates the room; using it sets `bDoorUsed` via `F_ProxyActivated` and completes the objective. (`MINI_Lock` + proxy ⇒ **INFERRED** a lock/pry interaction on the classroom door or window; no lockpick callback is wired in this file.) |
| D4 | **Take the diary** (`O_GetDiary`, part 2) | Objective `1_G1_OSTAFF`; the staff-room door `DT_ischool_Staff` unlocks; blip on `_1_G1_DIARY`. At the diary point, interact (button 9) plays a short in-engine pickup beat: player snapped to (-645.8, 222.25) with a fixed camera, `GetDiary` anim from `Act/Conv/1_G1.act`, weapon 432 destroyed, the diary HUD element turned on (`HUDDiaryVisible`), item 483 given. Staff door re-locks behind the player. Objective completes. |
| D5 | **Get back out** (`O_LeaveSchool`) | Objective 3 (`1_G1_OBJ3` — leave the school, **paraphrase**); stealth music stopped; Beatrice gets a "don't care about trouble" flag (113) and a character blip. Objective completes when the visible area returns to 0. |
| D6 | **Return the diary** (`O_FindBeatrice`) | When the player is within ~2-3 m of Beatrice holding the diary: both made invulnerable for the scene, `bPlayerAtEnd` set (fails now ignored), control off, widescreen, bike dismount, mutual facing/lock, the give anim (`Give/GiveBeatrice1_G1`) plays. Pass banner `M_PASS` with value 2000 (**INFERRED** $20.00 reward) and pass music. The script then waits out the make-out/kiss anim chain on both peds (**INFERRED**: the diary return ends in the kiss that marks Beatrice as a girlfriend — social flags 84/132 and emotion changes support this). |
| D7 | **Pass** | `O_KissBeatrice` releases Beatrice to ambient wander; fade, camera reset, `MissionSucceed(false, false, false)`, control back. Cleanup: staff door + both window doors re-locked, mission doors unlocked, diary item cleared, social overrides/gift requirement removed, model 3 uniqueness restored. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Beatrice hit/KO'd (`F_BeaHit`) | mission-critical callback; she is made ambient first | `1_G1_BEAHIT` |
| Generic | `F_FailMission` / `T_MissionFail` thread (no other setter of `bMissionFailed` in the live path) | `MissionFail(false, true)` no key |

`T_MissionFail` ignores failures once the end scene has started (`bPlayerAtEnd`). There is no spotted-by-authority fail in the Lua — being seen only clears ambient peds (D3); what a prefect bust does to the mission is engine/punishment behavior — **OPEN**. Retry replays from D0; whether the baked cutscene is skipped on retry is not visible in this file (**INFERRED** global handling).

### Rewards / pass side-effects

- **No `SMissPass.lua` entry for 1_G1** — no weapon, outfit, chapter, collectible or force-completed missions on pass (**VERIFIED** absence).
- In-mission: `M_PASS` banner value 2000 (**INFERRED** money), the diary HUD element, and — by social-system side effect — Beatrice becomes kissable/a girlfriend (**INFERRED** from the make-out anim wait and emotion/flag changes; the persistent unlock itself would live in the social system, **OPEN**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | D0, before control | baked (`1-G1`) | `fade` in at night + `letterbox` + 1-2 original `say` beats (Beatrice explains the diary), or straight to D2 |
| CS-2 | D4 diary pickup | in-engine (fixed camera + anim) | `interact` → `camera` fixed + pickup anim + `fade`; already our primitives |
| CS-3 | D6 return + kiss | in-engine (pair anims + banner) | `letterbox` + mutual `teleport`/facing + give anim + `say` beat + `banner` (pass); kiss optional pair anim |

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 3 | Beatrice | unique-ified; mission-critical; stationary outside until the handoff; social overrides `1_G1WantGift` / `1_G1Follow` |

No other mission peds are created; the empty school is the point (all ambient peds cleared if the player is spotted).

### Animation groups / action trees

`LoadAnimationGroup`: `MINI_Lock` (loaded twice, as scripted), `1_G1_TheDiary`, `1_03The Setup`, `1_07_SaveBucky`.
`LoadActionTree`: `Act/Conv/1_G1.act`; social overrides `Mission/1_G1WantGift.act`, `Mission/1_G1Follow.act`.

### Props / pickups / weapons

- Item **483 = the diary** (VERIFIED by context: `GetDiary` anim + `HUDDiaryVisible`).
- Model 486 requested before Beatrice spawn (`RequestModel(486)`) — prop, identity **OPEN**.
- Weapon/item **432** requested and explicitly destroyed at the pickup beat — **OPEN** (a carried item the scene removes; night-stealth flashlight guess **INFERRED** only).
- Window/door locks: `DT_tschool_ExtWind`, `DT_tschool_HallWind`, `DT_ischool_Staff`, plus the six exterior school doors listed in D1.

### Speech

**0 scripted speech events in the Lua** — all VO lives in the baked `1-G1` cutscene; the end scene plays no scripted lines (only anims + banner). Music: `MS_FootStealth*` interactive streams.

### Special entities

None (`AreaLoadSpecialEntities` not used). The `shared.gWindowsOpen` global toggles the climbable-window door state.

### Named world data to extract from `1_G1_new.DAT`

- POINTLISTs: `_1_G1_PLAYERSTART`, `_1_G1_LATTICE`, `_1_G1_WINDOWENTRY`, `_1_G1_BEASPAWN`, `_1_G1_MATHCLASS`, `_1_G1_ROOMOBJECTIVE`, `_1_G1_DIARY`, `_1_G1_SECONDFLOORWINDOW`.
- TRIGGERs: `_1_G1_MATHPROXY` (prop proxy); door ids (live in the door dats): `DT_TSCHOOL_SCHOOLFRONTDOORL`, `DT_TSCHOOL_SCHOOLLEFTBACKDOOR`, `DT_TSCHOOL_SCHOOLLEFTFRONTDOOR`, `DT_TSCHOOL_SCHOOLRIGHTFRONTDOOR`, `DT_TSCHOOL_SCHOOLRIGHTBACKDOOR`, `DT_tschool_GirlsDormL`, `DT_ischool_Staff`, `DT_tschool_ExtWind`, `DT_tschool_HallWind`.
- PATHs: none referenced.
- Hardcoded coords: diary pickup player (-645.80054, 222.25, -0.39) + camera (-645.1992, 221.4531, 1.163621 → -645.90344, 222.15561, 1.064427) — area 2.
- Area codes seen: **0** (start, Beatrice) and **2** (school interior).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| D1 door lockset | — | **door_lock_set** (mission-scoped lock/unlock list, restored on cleanup) incl. window/traverse doors |
| D2 window entry | `trigger_enter` (on area change) | **traverse_interact** (lattice→window climb) if we want the climb readable; area-change condition (**area_visible_eq**) |
| D2 blip swapping | `event` | **dynamic_blip** (re-point objective blip by area/state) — or just two sequential objectives |
| D3 proxy interaction | `interact` | **prop_interact** with callback (PAnim proxy); optional **minigame_launch** if we keep a lock/pry beat; **clear_ambient_peds** action; **spotted_by_authority** event |
| D4 item pickup beat | `interact`, `camera`, `fade` | **give_item** + **hud_item_visible** (diary HUD) + **destroy_carried_item** |
| D6 handoff + kiss | `letterbox`, `say`, `banner` | **give_item_scene** (pair anim: give/receive), optional **pair_anim** (make-out), **ped_set_invulnerable** scene guard |
| fails | `event` | **mission_critical_npc** hit/KO → fail with key; **fail_suppressed_after(flag)** for the end scene |

## 6. Open questions

- The `_1_G1_MATHPROXY` interaction: lockpick minigame, pry anim, or plain door — `MINI_Lock` is loaded but no lockpick callback is wired in this file; the proxy's own action tree is undecoded (**OPEN**).
- What being spotted actually costs (only `AreaClearAllPeds` is scripted; bust → mission fail? punishment flow owns it — **OPEN**).
- Identity of item/model 432 and 486; why `1_03The Setup` and `1_07_SaveBucky` anim groups are loaded (shared social/struggle clips — **INFERRED**).
- The persistent effect of the kiss (Beatrice girlfriend flag location — social system, **OPEN**).
- Retry cutscene-skip behavior and how the mission is offered (evening window 19-23 comes from the mission table, not the Lua — **OPEN** mechanism).
- `nRadarRange = 30` is declared but never used (**dead var**).
