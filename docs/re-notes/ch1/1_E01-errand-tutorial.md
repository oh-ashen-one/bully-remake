# 1_E01 — errand-system tutorial ("Petey's errand") (recon)

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED**, **INFERRED**, **OPEN**.

Sources: decompiled Lua `Bully-SE-scripts/scripts/chap1/1_E01.lua` (316 lines), `SMissPass.lua`. Functional description in our own words only; script/file/native names are citations.

## 1. Identification

**1_E01 is NOT any entry in the canonical Chapter 1 story list — it is the errand-system tutorial pseudo-mission. VERIFIED by content, name INFERRED.** Evidence:

- The `E` prefix, the `DoSocialErrands(...)` calls with the key `AS_PETEY_OBJECTIVE`, `MinigameSetErrandCompletion(...)`, and tutorial text ids `TUT_ERR1..6` all mark this as the tutorial that teaches the errand (social-favor) system.
- Content: Petey (ped model 134, made unique) in the boys' dorm (area 14) asks the player to deliver an item (id 521) to Beatrice (ped model 3), who waits outside (area 0) at (276.5, -36.2, 6.2).
- It has no baked cutscene, no DAT point lists (all coordinates are raw literals in the Lua), and no entry of its own in `SMissPass.lua`'s reward table — instead **passing 1_07 (Defend Bucky) force-completes 1_E01** (`MissionForceCompleted("1_E01")`, `MissionSuccessCountInc("1_E01")`), i.e. it is optional side content that the game auto-marks done if skipped.
- `MissionCleanup` references `IsMissionAvailable("1_11x1")` to lock the clothing manager — situating it early in the chapter, before the Halloween chain.

This is the identification surprise of the batch: it is not The Diary / The Big Prank / Character Sheets (those are 1_G1 / 1_11x-family / other ids).

## 2. Mission flow

`MissionSetup`: no fade-in; Petey's model (134) set unique. `main`: POI disabled, population zeroed, clock paused (and only unpaused in cleanup if it wasn't already paused), blip placed on Petey's dorm position, then a five-phase state chain driven off `F_CheckConditions`.

| # | Beat | Function | Logic (functional) |
|---|---|---|---|
| E0 | Setup (only inside the dorm) | `F_SetupErrand` | Does nothing unless the visible area is 14. Control off; if the player holds weapon 359 it is destroyed (**OPEN** what 359 is); player made NIS-safe; all peds cleared; Petey spawns at (-514.29, 318.55, 31.47); player placed at (-499.8, 318.3, 31.4); Petey faces the player, is flagged (110, 108, 19), ignores stimuli; social actions 28/29/35 vs Petey disabled; a green blip marks him. A fixed camera (-510.9, 317.8, 33.2) fades in, tutorials `TUT_ERR1`/`TUT_ERR2` play (4.5 s each), Petey walks to the player. |
| E1 | Greeting | `F_OnGreeting` | Waits for Petey's scenario-seek to reach the player (action-tree state check). Then control returns with all buttons locked except id 10 (the social/greet button, **INFERRED**), tutorial `TUT_ERR3` shows persistently. A second fixed camera (-496.7, 317.7, 33.3) frames the greeting while Petey approaches. |
| E2 | Explanation | `F_OnDialog` | When Petey enters the dialog scenario state: his ambient speech event `HELP_EXPLANATION` plays; tutorials `TUT_ERR4` then `TUT_ERR5`; input unlock extends to button 7 and social action 35 vs Petey is re-enabled (the "accept" action, **INFERRED**). |
| E3 | Accept the errand | `F_AcceptScenario` | When the player performs the accept scenario: control off; Petey becomes mission-critical; player and Petey lock onto each other and the player plays the gift-giving pair anim `/Global/Player/Gifts/Errand_SCH_Tutorial2` (`Act/Player.act`); camera returns; player receives item 521 ×1; tutorial `TUT_ERR6`; the dorm exit door `DT_DormExitDoorL` is unlocked; control and population/POI restored; a blip is set on Beatrice's outdoor position. |
| E4 | Go outside, spawn Beatrice | `F_ScenarioGoals` | When the visible area becomes 0: Beatrice (model 3) spawns at (276.5, -36.2, 6.2) facing the player and gets the objective blip. |
| E5 | Deliver | `F_ObjectiveMet` | When the player is within 3 m of Beatrice with player flag 1 set (**INFERRED**: "carrying the errand item" flag): Beatrice goes invulnerable, control off, widescreen, player dismounts any bike, walks up to her, mutual lock-on, the delivery pair anim `/Global/Player/Gifts/Errand_SCH_Tutorial` plays out; then errand completion `AS_COMPLETE` with reward 500 (cents → $5.00, **INFERRED**), Petey's flag 110 cleared, blips removed, `MissionSucceed`. |

There is no `F_MissionSpecificCheck` logic (stub returns true) — the flow is purely the chain above.

Fail conditions (`F_CheckConditions`, all with errand fail text ids `AS_FAIL` + `AS_TRY_AGAIN`):

| Fail | Cause |
|---|---|
| Beatrice hurt/KO'd | `F_PedIsDead(Beatrice)` **or** `PedGetWhoHitMeLast(Beatrice) == gPlayer` |
| Petey KO'd | `PedIsDead(Petey)` |
| Left the dorm early | visible area ≠ 14 before the errand is accepted (`bActive`) |

Retry/restart: no cutscene exists; restart re-runs `F_SetupErrand` (which requires the player to be back in area 14). The errand framework itself (`DoSocialErrands`, `MinigameSetErrandCompletion`) handles the world-side errand marker.

Cleanup notes: item 521 count forced to 0 (undelivered parcel removed), door `DT_DormExitDoorL` unlocked, Petey's unique-model status released, clock unpaused only if this mission paused it; if `1_11x1` is available the clothing manager is locked (`shared.lockClothingManager = true`). Script quirk: cleanup checks `PedIsValid(Peter)` — an undefined global (nil), so the Petey-ambient release never runs; harmless but worth not reproducing. **VERIFIED** (variable is `Petey` everywhere else).

Chains to: nothing directly. Reverse dependency: 1_07's pass force-completes 1_E01.

## 3. Cutscene points

None baked. Two in-engine fixed-camera beats:

| Marker | Where | Type | Suggested replacement |
|---|---|---|---|
| 1_E01-CS1 | E0 | fixed cam + fade + 2 tutorial cards | `camera` + `fade` + `hint` ×2 |
| 1_E01-CS2 | E1 | fixed cam framing the approach | `camera` (static) or drop |
| 1_E01-CS3 | E3/E5 | letterbox-free pair-anim moments (control off, lock-on, gift anims) | `letterbox` + paired-anim action + `say` stub |

## 4. NPCs and assets needed

**Ped models:** 134 (Petey, unique), 3 (Beatrice).

**Animation groups:** none loaded via `LoadAnimationGroup`. Pair anims come from `Act/Player.act`: `/Global/Player/Gifts/Errand_SCH_Tutorial2` (accept) and `/Global/Player/Gifts/Errand_SCH_Tutorial` (deliver).

**Items/weapons:** 521 (the deliverable parcel — **OPEN** what it represents; given on accept, zeroed on cleanup), 359 (destroyed at setup if held — **OPEN**; possibly the camera, since this is a social tutorial).

**Props/special:** none. Door `DT_DormExitDoorL` (dorm exit) unlocked mid-mission.

**Speech usage (counts only):** 1 ambient speech event (`HELP_EXPLANATION` on Petey); 0 scripted mission line ids. Six tutorial text ids `TUT_ERR1..6` (we write our own).

**World data:** no `1_E01.DAT` usage — every coordinate is a raw literal (Petey spawn (-514.29, 318.55, 31.47, area 14), unused trigger tuple (-502.25, 318.4, 31.46, r 3.75), Beatrice (276.5, -36.2, 6.2, area 0), player setup (-499.8, 318.3, 31.4), two fixed cameras). **Area codes used:** 14 (isc_dorm), 0 (tschool outdoors).

## 5. Framework gaps

| Beat | Covered by | NEW primitive(s) |
|---|---|---|
| E0 area-gated setup | trigger_enter / flag (partial) | `condition: area_visible(code)` |
| E0 spawn + face + blip | spawn_enemy is hostile-only | `action: spawn_npc` (friendly, at point, with flag sets: ignore-stimuli, unique-model) + `action: set_blip` |
| E0 weapon strip | none | `action: destroy_weapon_if_held` (or fold into mission-setup rules) |
| E1/E2 scenario-state waits | none | `condition: npc_ai_state(path)` — action-tree state check; for our port, replace with `npc_reached_point` + a `say` line (**INFERRED** acceptable simplification) |
| E1/E2 partial input locks | none | `action: lock_inputs` (except-list) |
| E3 accept gesture | interact (partial) | `system: social_accept` — the errand accept flow (approach, prompt, pair anim, item grant). Large; stub as `interact` + `paired_anim` + `give_item` |
| E3/E5 pair anims | none | `action: paired_anim` (give/receive halves, alignment snap — same need as 1_02's humiliation) |
| E3 item grant / E5 delivery | none | `action: give_item(id, n)` / `remove_item`, `condition: has_item` + reach-NPC, i.e. a `deliver_item` composite |
| E4 spawn-on-area-change | event + area_visible | fine as primitives once `area_visible` exists |
| E5 reward | reward (partial) | errand completion payload: money in cents + completion message (`mission_complete_screen` lite) |
| Fails: hit-watch on Beatrice | none | `condition: ped_hit_by(player)` / `condition: npc_ko(target)` → `mission_fail` with message |
| Fail: leave-area-early | trigger_exit / area_visible | covered once `area_visible` exists |
| Clock pause hygiene | none | `action: clock_pause`/`clock_unpause` with "was already paused" memory |

## 6. Open questions

- What items 521 and 359 are; whether 521's delivery requires the player flag 1 (item-carry flag) or proximity alone would do.
- The exact accept UX: which button ids 10/7 are, and how `DoSocialErrands` presents the errand marker in-world (the social-errand system is a whole subsystem; this script only rides it).
- Why Petey is flagged 19/108/110 (ped flag semantics are not decoded; 110 is cleared on completion).
- Whether the mission is offered via a world blip at the dorm (a `PeteyBlip` is added at his position before setup — but setup removes it immediately when in area 14; the pre-setup blip suggests an exterior entry trigger exists elsewhere, **OPEN**).
- Where 1_E01 sits in the availability table (mission table outside the Lua); only the 1_07 force-complete pins it as "early chapter, optional".
