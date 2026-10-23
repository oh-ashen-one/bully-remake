# 2_R03_Conv — Paper Route conversation flag (companion script): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/2_R03_Conv.lua` (11 lines, complete) + `SInitGl.lua` (global init). Structure described in our own words. Script names are citations only.

## 1. Identification

**2_R03_Conv = a one-flag conversation companion for the Paper Route job (2_R03).** **VERIFIED** by name and content: the whole script is (a) `F_ConversationOver()`, which sets `shared.gConversationOver_2_R03 = true`, and (b) a `main()` that waits until `PedInConversation(gPlayer)` goes false. `SInitGl.lua` initializes `shared.gConversationOver_2_R03 = false` at global init (VERIFIED).

## 2. What it does (complete)

| # | Beat | Logic (functional) |
|---|---|---|
| X1 | init | nothing — no DAT, no models, no objectives |
| X2 | wait | `main()` spins until the player is no longer in a conversation, then returns |
| X3 | flag | `F_ConversationOver` (called from outside this file — presumably a conversation/action-tree node callback, **INFERRED**) latches the shared flag |

**Consumer of the flag: not found in `2_R03.lua`** (grep: the only references are this file and the `SInitGl.lua` initializer). The flag is presumably read by the conversation tree `Act/Conv/2_R03.act` or another job script — **OPEN**. Its semantic is "the paper-route intro conversation has finished at least once" — likely gating repeat-run behavior (**INFERRED**).

## 3. Replacement on our side

One boolean on the conversation system (`conversation_finished("2_R03")` or a mission-shared flag). No primitives, no assets, no world data. Nothing to extract from DAT files (this script loads none).

## 4. Framework gaps

None — reuse whatever flag/channel mechanism the mission framework already has. Listed only so the inventory of `secnd/2_R03*` files is complete.
