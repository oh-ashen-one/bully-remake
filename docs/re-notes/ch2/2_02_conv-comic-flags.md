# 2_02_Conv — Comic Klepto conversation glue: recon note

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_02_Conv.lua` (7 lines, complete) + cross-refs `SInitGl.lua` (flag init), `chap2/2_02.lua` (loads `Act/Conv/2_02.act`). Names are citations only.

## 1. What this file is

Not a mission — the **conversation-tree glue** for 2_02, in the established `*_Conv.lua` pattern: small named Lua functions that an `.act` conversation tree calls as **condition** (returns 1/0) or **action** callbacks. Two functions:

| Function | Kind | Effect |
|---|---|---|
| `F_PlayerGotComicCond` | condition | returns 1 if `shared.g2_02_GotComic == true`, else 0 |
| `F_PlayerSetReturnedComic` | action | sets `shared.g2_02_ReturnedComic = true` |

## 2. Flag lifecycle (VERIFIED from the corpus)

- Both flags are initialized `false` in `SInitGl.lua` (`shared.g2_02_GotComic`, `shared.g2_02_ReturnedComic`).
- `g2_02_GotComic` is **never set true anywhere in the Lua corpus** — no writer exists. Either the engine/mission system writes it (e.g. on mission pickup/completion of 2_02), or the condition is vestigial. **OPEN**.
- `g2_02_ReturnedComic` is set true **only** by `F_PlayerSetReturnedComic` (i.e. from inside a conversation tree) and is **never read** anywhere else in Lua — its consumer is the conv tree itself or external data. **OPEN**.
- `2_02.lua` loads `Act/Conv/2_02.act` and uses it for the final hand-off anim node (`GiveComicOwner`); whether these two glue functions hang off that same tree (mission dialogue branches) or off the comic shop's ambient dialogue (`DTComics.act` in `SStores.lua`) is **OPEN** — **INFERRED**: shop-dialogue branches that change the clerk's lines depending on whether the comic has been recovered/returned.

## 3. Framework note

No new primitives. This is one more instance of the established pattern from ch1 recon: **Lua condition/action functions registered by name for `.act` trees** (same family as 1_02's tutorial gating functions and 1_03's `F_*` tree conditions). For our port, conv trees become data: a two-entry registry (`got_comic` condition reading a shared flag, `set_returned_comic` action writing one). The flags themselves map to a mission-state blackboard entry keyed `2_02`.

## 4. Open questions

- Who writes `g2_02_GotComic = true` (engine-side mission bookkeeping vs dead condition).
- Which `.act` tree consumes these functions, and what dialogue branches they gate.
- Whether `g2_02_ReturnedComic` gates anything post-mission (shop stock, ambient lines) or is write-only bookkeeping.
