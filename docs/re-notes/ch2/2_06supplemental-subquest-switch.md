# 2_06supplemental — subquest-switch helper: recon notes

Status: 2026-10-22, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED**, **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap2/2_06supplemental.lua` (63 lines, complete). Companion to `chap2/2_06.lua` (see `2_06-movie-tickets.md`).

## 1. Identification and content

Not a playable mission — a **helper/condition script** attached to Movie Tickets' world presence. It defines:

- `F_AmOnMission1/2/3/4()` — four byte-identical condition functions, each returning 0 when `shared.g206Subquest ~= -1`, else 1. The four copies differ only in name and debug prints (**VERIFIED**; likely registered as availability conditions for four distinct world mission markers that all feed 2_06 — **INFERRED**).
- `F_SetMission(subq)` — sets `shared.g206ChangeSubquest = true` and `shared.g206Subquest = subq`, returns 0. Called from outside this file (mission table/launcher) to select the subquest variant.
- `main()` — waits until the player is not in a conversation, then returns. That is the entire runtime body.

`shared.g206Subquest` / `shared.g206ChangeSubquest` are consumed outside this file (the mission launcher / main loop); neither symbol is written anywhere else in what we read for this note, so the full handshake is **OPEN**.

## 2. Framework implication

One small primitive, reusable for any mission with multiple world markers:

- **mission_variant_select**: a launcher-side hook that writes a shared variant id (`set_shared(flag, value)` — already implied by ch1 flag machinery) plus **availability_condition functions** exposed to the mission table. No world content, no peds, no cutscenes, no DAT.

## 3. Open questions

- Where the four mission markers for 2_06 live (mission table is outside the Lua, as with all chapters) and what distinguishes them (time windows 8-20 in the canonical list suggest a single window — so the four entries may be retry/chain variants, **OPEN**).
- Whether any other chapter-2 mission uses the same `*supplemental.lua` pattern (worth a sweep before generalizing the primitive).
