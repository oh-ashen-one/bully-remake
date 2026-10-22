# 1_11xp — "Halloween", part 2 (the prank collection)

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Sources (read-only, nothing copied): decompiled Lua `Bully-SE-scripts/scripts/chap1/1_11xp.lua` (2120 lines), cross-refs `SMissPass.lua`, `main.lua` (lines ~322, ~907-932), `SGlFunc.lua` (`F_SetupHallowenPeds`). This document describes structure and logic in our own words; no dialogue, cutscene script or narrative text is reproduced. Script names are citations only.

## 1. Identification

**This is the second half of the catalog mission "Halloween"** — the free-roam prank section that follows `1_11x1`. Confidence: **VERIFIED**.

Evidence: same `MS_HalloweenLow/Mid/High` music as x1; Gary (160) and Petey (165) recruited as allies in Halloween costumes; the game clock is paused for this mission and unpaused in `MissionCleanup` only if pranks were completed (the "time-stopped" property of the catalog entry); `SMissPass.lua` treats 1_11xp as the mission between 1_11x1 and 1_11x2, and `main.lua:322` spawns Gary+Petey as allies on load when xp is complete but 1_11x2/1_11_Dummy are not — i.e. after this mission the pair waits around for the player to start The Big Prank. The `xp` suffix reads as "extra pranks" (**INFERRED**); there is no separate catalog mission for it, so x1+xp together map to "Halloween".

The catalog's "Halloween" availability is "time-stopped"; this script is why (**VERIFIED**: `PauseGameClock` in `main.lua:F_CreateHalloweenItems` once 1_09 is complete and 1_11xp is not; `UnpauseGameClock` here in cleanup).

## 2. Mission flow

Not a staged mission — a hub loop over independent prank errands. `main()`:

| # | Beat | Logic (functional) |
|---|---|---|
| P0 | **Setup** | Player transitioned to `POINTLIST._1_11XP_PLAYERSTART` (area 0). Objective "go do pranks" + HUD counter `0 / 5` (`CounterSetMax(5)`). Weather set to id 3 (**OPEN** which look). All patrol paths disabled. `PedSetUniqueModelStatus(159, 2)`; weapon 411 loaded for Gary. |
| P1 | **Allies** | If the player has no ally follower: Gary (160) and Petey (165) spawn at the player start, Gary given weapon 411, Gary recruited as the player's ally, Pete recruited as Gary's ally; both marked mission characters, both get map blips. Faction attitudes are overridden for the night so student types (ids 11, 2, 5, 4, 1, 6) are friendly toward type 13 (player/allies — **INFERRED**). Punishers respond only to the player. |
| P2 | **Six prank spots live** | Each playable prank has a giver ped id, a spawn point, and an enter/exit trigger pair. Entering a spot's trigger (as the player) loads that prank if none is currently active: giver ped spawns, gets a blip, is mission-flagged, walks up to the player. Leaving the trigger unloads an un-started prank (deleted if the player can't see it, else made ambient) and restores its point blip. |
| P3 | **Prank accept flow** (identical per prank) | Giver walks to the player and enters a scenario-dialog task; when the dialog node is reached, the prank goes active: control off, giver plays a "hand over" anim, the prank weapon + ammo is granted, control back, all other prank blips removed, a global ped-hit event handler is registered, objective counter text shown. |
| P4 | **Do the prank** | Hit the required number of *unique* victims with the granted weapon. Hits are detected by a global event handler (event type 7) checking "last hit weapon = prank weapon" and "last attacker = player"; a per-prank dead-pool table dedupes repeat victims. Victims get stat overrides (speed/fear-style stats 6,7→100, 14→10) and flee the player. Remaining-count text updates on each valid hit. |
| P5 | **Prank done** | Counter incremented (`prankCount`, drives the HUD counter), giver ends his scenario and becomes ambient, other prank blips restored, and that prank gets a re-offer cooldown (15-60 s depending on prank). |
| P6 | **Pass** | When 7 of the 8 table entries are complete → mission objective complete, pass card (`M_PASS` with unlock-string id `1_11_XP_UNLOCK`), `MissionSucceed`. Clock unpaused in cleanup. |

The catch in P6: **two of the eight prank entries ship pre-completed** (`bComplete = true` at init) and their enter/exit triggers are never registered — Dead Rat and Firecracker are disabled content in this build. **VERIFIED**. Quirk: their function↔point bindings are swapped (the Dead Rat function is bound to the `_1_11XP_FIRECRACKER` point/trigger and the Firecracker function to `_1_11XP_DEADRAT`). So the player must complete **5 of the 6 playable pranks** (matches the HUD counter max 5); one playable prank may be skipped entirely.

### The six playable pranks

| Prank | Giver model | Spot trigger/point | Weapon granted (id × ammo) | Goal (functional) | Fail/abort path |
|---|---|---|---|---|---|
| Egg | 164 | `_1_11XP_JOKECANDY` | 312 ×6 | Egg 3 unique people (dogs excluded; egged victims flee) | Out of ammo → 5 s grace → return to giver, button combo (7+10) within ~2 m for 6 more |
| Fountain firework | 162 | `_1_11XP_F4000` | 397 ×1 | Light and plant the fountain so it hits someone (hit with weapon 397) | Dropped/unlit 15 s or lit 17 s without a hit → can fail the errand (`AS_PFAIL`) or restock at the giver |
| Itching powder | 163 | `_1_11XP_ITCHPOWDER` | 394 ×5 | Powder 3 unique people; splash counts — peds near a victim playing the itch reaction also count | Same restock loop at the giver |
| Kick-me sign | 159 | `_1_11XP_KICKME` | 372 ×1 | Stick the sign on someone — completion flag `bNoteStuckOn` is set by `F_PlayerAttachedSign()`, which is called from outside this file (action tree/weapon callback, **OPEN** where wired) | Sign dropped 10 s + no ammo → restock loop; `AS_PFAIL` possible |
| Marbles | 161 | `_1_11XP_MARBLES` | 349 ×5 | Make 1 unique person slip (victim must be playing the marbles hit reaction) | Restock loop |
| Stink bomb | 186 | `_1_11XP_STINKBOMB` | 309 ×5 | Stink 3 unique people; area effect — anyone within ~4 m of a hit victim playing the stink reaction also counts | Restock loop |

Disabled (present but pre-completed, triggers unregistered): Dead Rat (model 169 giver; would spawn two rat peds model 136, weapon 346, hit 1 girl; has its own tutorial id `GirlRats`) and Firecracker (model 161 giver, weapon 301 ×10, hit 3 people).

**Fail / rub-out behavior (whole mission):** there is **no mission fail state at all** in 1_11xp — no `bMissionFailed` path is ever set. Individual pranks can be "rubbed out": if a giver ped dies or enters combat while his prank is inactive/active (and the player was the attacker), the prank aborts with an errand-fail card (`AS_PFAIL`), the giver resets, and the prank can be re-attempted after its cooldown. `F_PrankVaild` enforces this per frame. **VERIFIED**.

**Pass side-effects:** `SMissPass.lua` (1_11xp): if 1_11x1 complete and 1_11_Dummy not complete → Halloween2 + Halloween3 dressing (re)loaded; sets `shared.g1_11XpJustFinished`. No weapon/outfit/collectible reward for xp itself; the unlock card text id suggests a cosmetic unlock message but nothing else is granted in script (**OPEN** what `1_11_XP_UNLOCK` represents — possibly just the pass banner). `main.lua:322`: after xp (and before x2/Dummy), every save-load respawns Gary (160) and Petey (165) as allies near the school/dorm with a shared health bar, keeping The Big Prank available.

## 3. Cutscene points

None — no baked cutscenes, no camera work, no letterbox anywhere in the file. The giver "hand over" moments are gameplay pauses (control off, 1 s give anim). Placeholder: our `interact` + short `say` + a give/receive anim is the whole thing.

## 4. NPCs and assets needed

| Role | Model id | Notes |
|---|---|---|
| Gary (costume) | 160 | ally, weapon 411 |
| Petey (costume) | 165 | ally under Gary |
| Egg giver | 164 | costume variant, **OPEN** name |
| Fountain giver | 162 | **OPEN** |
| Itching-powder giver | 163 | **OPEN** |
| Kick-me giver | 159 | unique-spawn status 2 (**OPEN**) |
| Marbles giver | 161 | **OPEN** |
| Stink-bomb giver | 186 | **OPEN** |
| (disabled) Dead-rat giver | 169 | plus rat peds 136 |
| (disabled) Firecracker giver | 161 | same id as marbles giver |
| Costume-population roster | 161-174 enabled, ~20 normal student models disabled | via `F_SetupHallowenPeds` in `SGlFunc.lua` |

- Weapons: 312 (eggs), 397 (fountain firework), 394 (itching powder), 372 (kick-me sign), 349 (marbles), 309 (stink bombs), 411 (Gary), plus disabled 346 (rat), 301 (firecrackers). Names **INFERRED** from prank context where listed; otherwise numbers only.
- Animation groups: none loaded by this file directly (givers use shared `Act/Anim/Ambient.act` `ScenarioGive` and the AI `ScenarioDialog` task tree; victims use `HitTree` ranged reactions `ItchingPowder`, `Marbles`, `Bomb/Stinky`). Our import needs those reaction clips for victims.
- Speech: no scripted mission speech; each giver plays one ambient `HELP_EXPLANATION` event (6 playable). Dialog text itself is engine scenario dialog, **OPEN** (lives in the AI tree, not this Lua).
- Music: `MS_HalloweenLow/Mid/High.rsm`.

Named world data to extract later (from `1_11XP.DAT`):
- Points: `POINTLIST._1_11XP_PLAYERSTART`, `_1_11XP_JOKECANDY`, `_1_11XP_F4000`, `_1_11XP_ITCHPOWDER`, `_1_11XP_KICKME`, `_1_11XP_MARBLES`, `_1_11XP_STINKBOMB` (+ `_1_11XP_DEADRAT`, `_1_11XP_FIRECRACKER` for the disabled pair).
- Triggers: same eight names under `TRIGGER._1_11XP_*` (enter + exit handlers; only six registered).
- Areas seen: 0 only.

## 5. Framework gaps

Covered by existing primitives: teleport, spawn (as spawn_npc), hint/banner (objective + count text), reward (none needed), flag, timer (cooldowns), event (generic), trigger_enter/exit (prank spots).

New primitives/conditions needed:
1. **social-errand giver loop** — giver NPC walks to player → dialog → accept → grant weapon/ammo → wait for goal → success/fail card → giver resets with cooldown; abort if giver attacked/killed (without failing the parent mission).
2. **ped_hit_by_weapon event** — global hit event with attacker attribution + last-weapon-id filter (the original's event type 7 + `PedGetWhoHitMeLast`/`PedGetLastHitWeapon`).
3. **unique-victim counter** — dedupe set per goal ("hit 3 different people"), with remaining-count HUD text and a total counter widget (CounterSetMax/Current).
4. **npc_reaction_state condition** — branch on a victim playing a specific hit-reaction state (itch/marbles/stink) so splash/AoE credit works.
5. **victim flee reaction** — stop + stat override + flee-from-player as one canned reaction.
6. **out-of-ammo restock flow** — ammo-empty condition, blip back to giver, proximity + button-combo interact to restock.
7. **visibility-gated despawn** — delete roaming NPC only if the player can't see it, else make ambient (trigger-exit unload rule).
8. **faction/attitude matrix override** — per-mission type-to-type attitude rows, restored on cleanup.
9. **punishment-system switch** — disable prefect/punisher response or restrict to player-only for the mission duration.
10. **clock pause/unpause** — mission-scoped game-clock control.
11. **dog exclusion** — model-class filter on hit credit (models 219/220 excluded). Can fold into (2) as a filter field.
12. **errand-pass/fail cards** — `MinigameSetErrandCompletion`-style transient cards distinct from mission pass/fail (banner variants).
13. **recruit_ally chain** — as in 1_11x1 doc (player→Gary→Pete), with mission-char flagging and blips.

## 6. Open questions

- Where `F_PlayerAttachedSign()` is wired for the kick-me sign (likely an action-tree/weapon callback for weapon 372) — our framework needs an equivalent "sign attached to NPC" event.
- The exact accept/decline UX of the scenario dialog (lives in `Act/AI/AI.act`, undecoded): we only see the task-node checks.
- What `1_11_XP_UNLOCK` actually unlocks (nothing is granted in Lua — likely just banner text).
- Cooldown variety (15/30/60 s per prank) — data, fine, but whether it's player-facing anywhere is unknown.
- Whether disabled Dead Rat / Firecracker should be re-enabled in our remake (they're fully implemented; only the table init + trigger registration disable them). Design decision, not a code question.
