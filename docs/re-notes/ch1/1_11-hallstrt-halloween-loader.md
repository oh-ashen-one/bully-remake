# 1_11_HallStrt — Halloween dressing loader (no gameplay)

Status: 2026-10-22, read-only recon. Confidence tags as in `opening-slice-recon.md`: **VERIFIED** / **INFERRED** / **OPEN**.

Sources (read-only, nothing copied): `Bully-SE-scripts/scripts/chap1/1_11_HallStrt.lua` (12 lines, complete), `Test/Missions/Test1_11_HallStrt.lua` exists in the same tree (test launcher). No game text reproduced; script names are citations only.

## 1. Identification

Not a catalog mission — a **one-shot helper mission that turns on the `Halloween3` dressing layer and instantly succeeds**. The name reads as "Halloween start" (**INFERRED**). Confidence: **VERIFIED** for what it does; **INFERRED** for when it's used.

Evidence: `main()` is three calls — `AreaLoadSpecialEntities("Halloween3", true)`, `AreaEnsureSpecialEntitiesAreCreated()`, `MissionSucceed(false,false,false)`. It has no `SMissPass.lua` entry, loads no DAT, spawns nothing, sets no flags.

Purpose in the chain (**INFERRED**): `Halloween3` is the full-night dressing (also activated by 1_11x1's setup and by `main.lua:F_CreateHalloweenItems` once 1_11x1 is complete). HallStrt exists so the dressing can be re-applied in one step from a save-load or debug/test path without running 1_11x1 — consistent with the test launcher `Test1_11_HallStrt.lua`.

## 2. Mission flow

| # | Beat | Logic |
|---|---|---|
| H0 | main | Load `Halloween3` special entities, ensure created, succeed immediately. |

No objectives, fail path, cutscenes, or world data.

## 3. Cutscene points

None.

## 4. NPCs and assets needed

None. Only the `Halloween3` special-entity dressing set (pumpkins/decorations population swap, contents **OPEN** — defined in the area data, not Lua).

## 5. Framework gaps

Same single primitive as `1_11_Dummy`: **marker/utility mission** with an `on_pass`-style world effect, here inverted — the effect happens in `main()` and the mission is just the vehicle. For our framework a **dressing-set action** (`set_dressing "Halloween3" on`) callable from mission JSON or from a load-game hook covers it. No mission machinery needed at all if our save/load applies dressing sets directly.

## 6. Open questions

- Whether anything in the live (non-test) game actually launches 1_11_HallStrt. Only the test file references it by name in this tree; treat as debug/utility unless the mission table says otherwise.
- Exact contents of the `Halloween3` dressing set (pumpkin destroyables and decor per the collectible rules around this night — needs the area-data read, not Lua).
