# 2_S07 — Small Offences (garden-gnome hunt unlock; carnival): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/2_S07.lua` (67 lines, complete) + `SMissPass.lua` (no 2_S07 case — **VERIFIED** absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_S07 = "Small Offences"** — the Scholarship/Anniversary-exclusive non-storyline mission that introduces the garden-gnome collectible hunt. **VERIFIED** by content (the entire mission body is a baked cutscene plus `CollectiblesSetTypeAvailable(0, true)` with the dev print "Gnomes Have Started"; collectible type 0 = garden gnomes, **INFERRED** from that print + external match) and externally: the [fandom Small Offences page](https://bully.fandom.com/wiki/Small_Offences) describes exactly this — Jimmy finds Brandy at the carnival smashing ceramic gnomes with a baseball bat, she asks him to smash every gnome in Bullworth, and "this mission itself is completed as soon as the cutscene ends". The [fandom Chapter II page](https://bully.fandom.com/wiki/Chapter_II) files it as a Chapter II non-storyline mission with script id `2-S07`.

**NAME COLLISION WARNING (cross-doc correction):** `ch2/2_S02-small-offenses.md` identifies 2_S02 (the follow-Hattrick-home yard-vandalism mission) as "Small Offenses" by elimination. The wiki evidence says **Small Offences = 2_S07 (gnomes)**, and TCRF notes a cut Chapter II side mission that "would later partially reappear on Scholarship Edition as Small Offences" — i.e. 2_S02 is the scrapped-PS2-mission remnant and the SE gnome mission inherited the name. The ch2 doc's identification should be revised (left as a note here; not edited in this pass).

**When offered:** fandom files it under Chapter II; the SDA all-missions route does it around Carnival Date (the start is at the carnival). The actual star-marker window lives in the mission table outside the Lua — **OPEN**; treat "available from Chapter 2 at the carnival" as the working rule. The script's presence in the `chap5/` folder is an authoring artifact (same family as 3_S03/3_S08); note that full gnome *completion* is impossible until Chapter 5 anyway (one gnome sits on a Blue Skies barge — wiki), which may explain the folder parking.

Structure: the smallest mission class in the game — `MissionSetup()` plays the baked cutscene with load, `main()` enables the collectible and instantly succeeds. No stages, objectives (beyond a 5 s text note), threads, or fail path.

## 2. Mission flow

| # | Beat | Logic (functional) |
|---|---|---|
| G0 | **Setup** | Ped model 140 loaded (the `gFMidget` var + `_2_GN_MIDGETHANG` point — the gnome-smashing carny, Brandy per fandom); no fade-in; special entity set `"2s07"` loaded and ensured (**INFERRED**: the tent's gnome/bat dressing); baked cutscene **`"2-S07"`** played with load. |
| G1 | **Body (seconds)** | `2_S07.DAT` (layer 2); punishment system off; model 140 pinned unique (`PedSetUniqueModelStatus(140, -1)`); ped 140 spawned at `_2_GN_MIDGETHANG` and the player placed at `_2_GN_MissionEnd`; control off; **`CollectiblesSetTypeAvailable(0, true)`** — gnomes become smashable world collectibles. Fade out; the ped is deleted again (spawn-then-delete is vestigial — VERIFIED sequence, probably a model-keepalive leftover); player re-placed at the mission-end point; camera reset; fade in; a 5 s text note (`2_S07_MOBJ_01` — paraphrase: the gnomes are now fair game); `MissionSucceed(false, false, false)`. |
| G2 | **Cleanup** | `"2s07"` entities unloaded; game clock unpaused; controller history un-ignored; player target lock cleared; model 140 uniqueness restored (1); population reverted; counter HUD hidden; widescreen off; control on; interactive music back; weapon HUD unlocked; camera change allowed; targeting restored; DAT layer 2 unloaded. Most of these restore systems the mission never disabled — copy-paste cleanup from a larger mission (**INFERRED**). |

### Fail conditions and retry

None — no fail path exists.

### Rewards / pass side-effects

- In-script: **garden-gnome collectible type enabled** (the entire reward; the gnome hunt itself pays out separately — gnome outfit/trophy per the 100% guides).
- No money, no respect, no `SMissPass` case (**VERIFIED** absence).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | G0 intro (Brandy and the gnomes) | baked `"2-S07"` | `fade` + original establishing card — the whole mission |

## 4. NPCs and assets needed

### Peds

| Model id | Role | Notes |
|---|---|---|
| 140 | Brandy, the gnome-smashing carny (fandom name; script var `gFMidget`, point `_2_GN_MIDGETHANG`) | spawned and deleted inside one fade — cutscene-only presence |

### Props / entities

- Special entity set `"2s07"` (tent dressing — gnome props + bat, **INFERRED** from `AreaLoadSpecialEntities`).
- Garden gnomes themselves are world collectibles owned by the collectible system, not this mission.

### Speech / text

None in the Lua beyond `2_S07_MOBJ_01` and dev prints; any dialogue lives in the baked cutscene.

### Named world data to extract from `2_S07.DAT` (trigger data **VERIFIED present** — `DATLoad("2_S07.DAT", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_2_GN_MIDGETHANG`, `_2_GN_MissionEnd` (the dev's `_2_GN_` prefix = "gnomes").
- No PATHs or TRIGGERs referenced.

## 5. Framework gaps (beat → primitives)

Deduped against the ch1–ch4 gap pools and the `MissionSubsystem.h` schema.

| Beat | Covered by | New primitives needed |
|---|---|---|
| G0 intro | baked-cutscene-at-setup pattern (1_10/2_S02 family) | none new |
| G1 unlock | `unlock_collectible` (pool), `special_entities_load` (pool), `unique_ped_suppress` (pool) | **cutscene_only_mission** (mission class: intro cutscene → one world-state flag → instant success + short text note; no stages. Trivial, but the campaign spine should know the class exists — 2_S07 is its purest instance) |
| G2 cleanup | stock teardown | none new |

## 6. Open questions

- The star-marker window and giver location (carnival tent) come from the mission table — **OPEN**, wiki says Chapter II.
- Whether collectible type 0's availability also gates gnome *map markers* (Geography 4 per the class table) or only smashability — data-side question for the collectible system.
- `F_Socialize` is defined but never called — dead code (VERIFIED); it's a copy of the social-action blacklist helper seen in other scripts.
- Whether `"2s07"` special entities include an ambient gnome prop that persists after the mission (the load is reverted in cleanup, so presumably not).
