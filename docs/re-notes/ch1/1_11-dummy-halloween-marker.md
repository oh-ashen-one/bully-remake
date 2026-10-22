# 1_11_Dummy — Halloween-night end marker (no gameplay)

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** / **INFERRED** / **OPEN**.

Sources (read-only, nothing copied): `Bully-SE-scripts/scripts/chap1/1_11_Dummy.lua` (10 lines, complete), `SMissPass.lua` (1_11_Dummy block), `main.lua:907-932` (`F_CreateHalloweenItems`), `SMissPass.lua` 1_B block. No game text reproduced; script names are citations only.

## 1. Identification

Not a catalog mission — a **zero-content marker mission** whose completion flag means "Halloween night is over". Confidence: **VERIFIED** that it has no content; **INFERRED** that its purpose is purely the completion flag.

Evidence: the whole script is `main() { MissionSucceed(false,false,false) }` with empty setup/cleanup. Its `SMissPass.lua` pass handler does all the work (below). Every other Halloween script gates its dressing/behaviour on `not IsMissionCompleated("1_11_Dummy")`:

- `SMissPass.lua` 1_06_01 / 1_09 / 1_11x1 / 1_11xp blocks: only load `Halloween1/2/3` special entities or set the just-finished flags while Dummy is incomplete.
- `main.lua` `F_CreateHalloweenItems`: bails out entirely (and deactivates `gHCriminalsActive`) once Dummy is complete.
- `main.lua:322`: Gary+Petey ally respawn between 1_11xp and 1_11x2 is gated on Dummy being incomplete.

What completes it is not in any script read here — presumably the mission table fires it when the night window ends (Halloween night is time-boxed; the 02:00-08:00 wake-up logic in 1_11x2's cleanup hints the night ends around then). **OPEN**.

## 2. Mission flow

| # | Beat | Logic |
|---|---|---|
| D0 | Setup | `MissionDontFadeIn`, nothing else. |
| D1 | main | Immediate `MissionSucceed(false, false, false)`. |
| D2 | Pass side-effects (`SMissPass.lua`, 1_11_Dummy block) | `ClothingGivePlayerOutfit("Halloween")` (guarantees the costume is owned even if the x1 grant was somehow missed), `AreaLoadSpecialEntities` for `Halloween1`, `Halloween2`, `Halloween3` all set **false**, then ensure-created — i.e. strips every Halloween dressing layer from the world. |

No objectives, no fail path, no cutscenes, no named world data (no DAT loaded).

## 3. Cutscene points

None.

## 4. NPCs and assets needed

None. Speech: 0. Models: 0. The only assets touched are the `Halloween1/2/3` special-entity dressing sets (turned off) and the `Halloween` outfit definition (granted).

## 5. Framework gaps

One new concept, cheap to build:

1. **marker mission / mission-as-flag** — a mission with no stages that exists so other content can gate on "completed(X)". Our JSON framework needs either a `type: marker` mission or the equivalent as a plain `set_flag` on a timer/event; the important part is that **world-state queries must be able to read mission-completion flags** (`IsMissionCompleated` equivalents) — needed by the whole Halloween chain anyway.

Also note the pass-hook pattern: rewards/dressing changes live in a per-mission-id pass table (`SMissPass`), separate from the mission script. Our equivalent: an `on_pass` block in mission JSON.

## 6. Open questions

- What fires 1_11_Dummy (time? next morning wake-up? another mission's pass?). Candidates: the wake-up logic family (`F_ProcessWakeUpMissionBasedLogic`) or the mission table. Worth one grep across the full script tree when we implement the chain.
- Whether Dummy is player-visible at all (probably never; likely launched silently).
