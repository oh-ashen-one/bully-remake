# 3_XM — Miracle on Bullworth St.: recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md`: **VERIFIED** / **INFERRED** / **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_XM.lua` (1350 lines, complete). Structure described in our own words; no dialogue or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_XM = "Miracle on Bullworth St."** (canonical Chapter 3 mission #29, available 6PM–10PM) — **VERIFIED** by content and reward: a winter-evening mission at the town-square "Santa's Grotto" set where Jimmy wrecks the rival Santa's display while elf-costumed townies intermittently attack; pass grants the `Elf` outfit (`ClothingGivePlayerOutfit("Elf")`), matching the canonical reward (Elf Costume). The script force-sets the clock to 19:30 at setup, inside the canonical window. Setting: New Coventry town square in front of City Hall (canonical); the grotto set itself is a special-entity set `"Miracle"` loaded only for the mission.

Quirk: the baked intro cutscene is `"3-01BA"` and every scripted speech event is in the `M_3_01B` group — the mission's internal production id was evidently **3_01B** before it was renumbered 3_XM (XM = Xmas Miracle, **INFERRED**). Useful when hunting audio/cutscene assets.

Trigger data: **YES** — `3_XM.DAT` (`DATLoad("3_XM.DAT", 2)`); all points/spawns below come from it.

## 2. Mission flow

Driver: linear `main()` for setup, then **five concurrent threads** (`F_RunStartDialog`, `F_CheckInArea`, `F_UpdateSantaBar`, `F_UpdateElves`, `F_CheckMissionSuccess`, plus the queue thread `F_CheckSantaQueue` started earlier) while `main()` itself walks a fixed sequence of six destruction objective stages. Threads are stopped by flags at mission end. This is the most thread-heavy mission recon'd so far.

Constants (all **VERIFIED**): kid-with-Santa beat 20 s; out-of-area grace 10 s cumulative; max distance from Santa 30 m; get-to-square gate 18 m / 35 s timer; grotto bar 100 points; 20 destroyable objects (5 points each); elf attack wave delay 60 s; family arrival cadence 10 s; max 5 families / 4 lineup slots / 5 parent spots; 4 elf slots.

### Stage flow

| # | Beat | Logic (functional) |
|---|---|---|
| X0 | **Setup** (`MissionSetup`) | `WeatherSet(2)` (snow), baked intro cutscene `3-01BA`, no auto fade-in, clock to 19:30. Anim group `NPC_Adult`; ped models: drunk/"real" Santa (252, var `bum`), rival grotto Santa (253), kids (138/137/66/69), parents (80/81/76), elves (`MODELENUM._TO_ElfM`/`_TO_ElfF`); weapon models 300 + 323; `3_XM.DAT`; conv tree `Act/Conv/3_XM.act`; **interactive music**: `MS_XmasJingleMiracle` Low/Mid/High streams registered. |
| X1 | **World dressing** | Player control off, teleport to `_3_XM_PLAYERSCENE` (area 0), load `"Miracle"` special entities (the grotto), five `AreaSetPathableInRadius(..., false)` calls block navigation discs around grotto props. Six destruction tables are built in Lua (candles ×6, candy canes ×3 — one entry is actually a chair prop `x_chair`, presents ×6 — incl. a teddy `x_tedy`, snowman ×1, sleigh ×1, castle pieces ×3; 20 total) with hardcoded positions. Eight permanent pickups of weapon 300 placed at `_3_XM_WEAPONS` (resupply pile). All models `RequestModel`-spun until ready. |
| X2 | **Cast** | Bum Santa (252) at `_3_XM_BUMSCENE`, emotion 7 toward player. `F_SetupSantaEvent`: player to `_3_XM_PLAYERSTART`, bum repositioned, ambient world emptied (patrol paths disabled, ambient peds ignore stimuli, POI off, peds + vehicles cleared, population and ambient vehicles zeroed); grotto Santa (253) created at `_3_XM_SANTASTART`, **untargetable**, stimulus-blind. `F_SetupElves`: four elves at `_3_XM_MIDGETSTART` nodes 1–4; social action 30 disabled against each; ignore-stimuli; ped flag 68 set (**OPEN** semantics — likely "no grapple"/"no bust" style immunity); elf slots tracked in a table with fixed home positions. `F_SetupSantaQueue`: three initial kid+parent families at `_3_XM_KIDSTART`/`_3_XM_PARENTSTART`, all untargetable, lineup/parent-spot tables seeded, queue thread started. |
| X3 | **Get to the square** (`F_GetToSquare`) | Control on; player given weapon 300 (a thrown projectile; identity **OPEN** — id 300 recurs in ch1/ch2 docs as unidentified, and note the snowball is id **313**, **VERIFIED** by the 3_01A Rudy doc's weapon-filtered hit check); objective `3_XM_SQUARE` + blip at Santa's post + **visible 35 s countdown**; poll until the player is within 18 m; timer expiry → `MissionFail("3_XM_FAIL_TIME")`. On arrival the bum goes ambient (his intro role is done). |
| X4 | **The wrecking spree** (six sequential objective stages in `main`) | In fixed order: snowman (1/1) → sleigh (1/1) → candles (3 of 6) → presents (3 of 6) → candy canes (2 of 3) → castle (3 of 3). Each stage: add objective + text, blip every undestroyed prop of that group (blip type 4; castle uses type 1), then block until the group's count is met. **Because the damage meter (X5) watches all six tables continuously, props smashed early still count** — a stage can complete instantly on entry. The mission's overall success gate is the **castle fully destroyed** (`F_CheckMissionSuccess` thread), not the bar reaching zero. |
| X5 | **Grotto damage meter** (`F_UpdateSantaBar` thread) | A general HUD bar starts at 100; every 500 ms it recomputes 100 − 5 × (destroyed count across all tables) via the PAnim breakable-prop API (`PAnimGetPoolIndex`/`PAnimMakeTargetable`/`PAnimIsDestroyed`). On each *new* point of damage: first damage spawns the Santa-behavior thread (X7); all queueing families are scattered (`F_ClearAllFamilies` — kids/parents bark and flee; any kid entering a flee task also triggers this from the queue thread); Santa plays a shake-fist react anim at the player; escalation thresholds fire (X6); per-prop first-break one-shot barks play (`F_RunBrokenSpeech`). Bar hides at 0. |
| X6 | **Escalation by meter** (`F_UpdateSpawnersAndSpeech`) | Threshold crossings (<95/<80/<60/<40: concurrent elf attackers allowed = 1/2/3/4 and the 60 s wave clock resets; <90/<70/<30: max queueing families = 3/2/1; every crossing: Santa barks at the elves). So the display fights back harder the more you smash. |
| X7 | **Santa behavior** (`F_UpdateSanta` thread) | Once damage starts: if Santa drifts >5 m from his post he walks back; otherwise he re-faces the player (with a restart anim beat). Pure theater — he never attacks. |
| X8 | **Elf pool** (`F_UpdateElves` thread, 1 s tick) | Dead elves go ambient and their slot re-spawns (random of 3 spawn points, alternating M/F model, walks to its home node). Wave scheduler: once the 60 s `elfAttackDelay` has accumulated and live attackers < allowed (X6), the **closest idle elf to Santa** is sent at the player (`PedAttackPlayer`, flag 68 cleared). Elves are therefore passive grotto dressing until the first damage, then trickle-fed attackers. |
| X9 | **Santa queue ambience** (`F_CheckSantaQueue` + `F_SeeSanta` + `F_SetupArrivingFamily`) | Every 10 s: if Santa is free and the line is non-empty, the front kid walks to `_3_XM_SEESANTA`, the line shuffles up, and a 20 s "kid on Santa's knee" beat plays (three speech thirds, ids 9/7/8 — shortened to a flat wait while the elves' opening barks are still running, though the `bRunningStartDialog` flag that gates this is never set true in-file — **VERIFIED** dead branch); then the family is released (parent recruits kid as ally, both wander off ambient). If under the family cap, a new kid+parent pair spawns at one of 3 spawn points, walks to a lineup slot, splits up on arrival (parent to a parent spot, kid appended to the line). Facing maintenance: parents/kids standing at their spots are kept facing Santa. One scared kid (explosion/flee reaction) scatters **all** families — no fail, just ambience collapse. |
| X10 | **Pass** (`F_MissionFinished`) | Player safe-mode on; threads flagged off; bar hidden; interactive music stopped; Santa's current line cut, one final scripted line (id 38); completion banner `M_PASS` with reward text key `3_XM_REWARD` (no money value — **VERIFIED** no cash/respect here); mission-end stinger 10; wait for banner; **`ClothingGivePlayerOutfit("Elf")`**; `MissionSucceed(true, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Too slow reaching the square | 35 s visible timer in X3 | `3_XM_FAIL_TIME` |
| Leaving the grotto area | >30 m from Santa's post; warning text re-printed while outside; **cumulative** 10 s out-of-area budget | `3_XM_FAIL_AREA` |

No violence/KO fail paths (kids and parents are untargetable; elves attacking the player is expected). No checkpoints — a retry replays from X0, including the baked intro. Cleanup (`MissionCleanup`): clock unpaused, `"Miracle"` entities unloaded, population/patrols restored, bar hidden, bum released ambient if valid, grotto Santa made to flee and deleted, anim group unloaded, weather released, DAT unloaded, widescreen off, player safe-mode off. `F_RunIntroScene` is defined but never called — dead code, its beats folded into the baked `3-01BA` cutscene (**VERIFIED**).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | X0 intro | baked (`3-01BA`) | fade in at `_3_XM_PLAYERSCENE` + letterbox + original VO beat |
| CS-2 | X5 Santa reacts | in-engine micro-beats | `npc_face` + one `ped_action_node` (shake fist) on a 2 s timer; no cut needed |
| CS-3 | X10 outro | engine banner | final `say` beat + `completion_banner` + `give_clothing` (outfit) |

Anim groups: `NPC_Adult`. Action tree: `Act/Conv/3_XM.act` (incl. `/Global/3_XM/Anims/ShakeFist` and `/Restart`).

## 4. NPCs and assets needed

### Peds (model ids as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 252 | bum/"real" Santa (quest giver) | emotion 7 toward player; ambient after X3; made ambient again in cleanup |
| 253 | grotto Santa (rival) | untargetable, stimulus-blind; theater-only behavior (X7); flees + deleted in cleanup |
| 138/137/66 (+69 unused at spawn) | lineup kids | untargetable; flee-cascade triggers family scatter |
| 80/81/76 | lineup parents | untargetable; recruit own kid as ally when leaving |
| `_TO_ElfM`/`_TO_ElfF` (`MODELENUM`) | elf guards ×4 pool | social action 30 disabled; flag 68 (**OPEN**); respawn-to-pool; escalate per meter |

### Vehicles / weapons / props

- Weapon **300** ×8 permanent pickups + one granted to the player (thrown projectile, identity **OPEN**; snowball = 313 per 3_01A); weapon **323** loaded, never used in-file — **OPEN**.
- 20 breakable PAnim props across 6 groups (positions hardcoded in Lua, listed in §2 X1): `x_cndl` ×6, `x_ccane` ×2 + `x_chair`, `xmas_prsnt1/2` ×5 + `x_tedy`, `DPE_Snowman`, `x_sleigh`, `x_cas1/2/3`.
- No vehicles.

### Speech

Scripted `M_3_01B` ids: 9/7/8 (Santa-with-kid beats), 15/19/14 (elf opening barks), 21 (Santa-to-elves, every escalation), 39 (sleigh), 45 (teddy), 46 (candles), 16 (candy cane, via closest elf) + 43 (candy cane, Santa), 47 (castle), 10 (default first-break), 38 (outro). Ambient: `BUMP_RUDE` on family scatter. **Script bugs worth noting (VERIFIED):** the candy-cane speech branches test `name == "ccane"` but the table stores `"x_ccane"` — ids 16/43 never fire; the castle branch lists `x_cas2` twice (never `x_cas3`) — harmless given the one-shot flag. Music: interactive `MS_XmasJingleMiracle` low/mid/high streams; pass stinger 10.

### Named world data to extract from `3_XM.DAT`

- POINTLISTs: `_3_XM_PLAYERSCENE`, `_3_XM_PLAYERSTART`, `_3_XM_BUMSCENE`, `_3_XM_BUMSTART`, `_3_XM_SANTASTART`, `_3_XM_MIDGETSTART` (4), `_3_XM_KIDSTART` (3+), `_3_XM_PARENTSTART` (3+), `_3_XM_KIDSPAWN` (3), `_3_XM_PARSPAWN` (3), `_3_XM_ELFSPAWN01/02/03`, `_3_XM_SEESANTA`, `_3_XM_LINEUP` (4), `_3_XM_WEAPONS` (8).
- PATHs: none used (all movement is point-to-point).
- Special-entity set `"Miracle"` (grotto geometry); nav-block discs hardcoded at (592.2, −84.9)…(590.7, −94.0) r0.5, layer 5.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| X0 weather/clock/intro | `weather_force`, `clock`, `fade`, baked-cutscene handling | none new |
| X1 special-entity set | `world_state_swap` / `geometry_instance_toggle` (Halloween/Christmas precedents); `special_entity_set_load` (3_01C) | none new — same set-load pattern (`AreaLoadSpecialEntities("Miracle", on)`), new set id |
| X1 nav blocking | — | **nav_block_radius** (disable pathing in N small discs at hardcoded points for the mission's duration) |
| X0/X5 music | `music_intensity_by_distance` (ch2) is adjacent but different | **interactive_music_stream** (register low/mid/high intensity layers on mission start, stop on end; intensity driven by combat system) |
| X1 weapon resupply pile | `pickup_spawner`, `give_weapon` | none new |
| X2 untargetable/stimulus-blind cast | `spawn_npc` flags (`untargetable`), `ped_set_invulnerable` | **npc_ignore_stimuli** flag on `spawn_npc`; **ped_flag_set** generic (flag 68 here — semantics OPEN) |
| X2 social blacklist | `social_interact` (ch1) is the positive version; **social_action_blacklist** already proposed in 3_05 (`PlayerSocialDisableActionAgainstPed`, per-target disable of player social actions by id) | none new — same call as 3_05 (action id 30 here, semantics **OPEN**) |
| X3 timed reach | `countdown_fail_timer`, `reach_location`, `blip` | none new |
| X4 sequential destroy stages | `prop_destroy_threshold_event`, `weighted_destruction_meter` (2_05), `objective_checklist` | **blip_on_prop** (per-prop waypoint on a breakable prop, removed on break — we have `blip_on_npc`/`blip_on_vehicle` only) |
| X5 damage meter HUD | `weighted_destruction_meter`, `prop_health_bar` | none new — this *is* the weighted meter (20 × 5 pts) with a general-purpose bar; note the meter is **global across groups while objectives are sequential** (pre-damage counts) |
| X6/X8 elf escalation | `continuous_wave_spawner`, `npc_spawn_walk_then_attack` | **meter_threshold_escalation** (declarative threshold table: meter crossing → set max concurrent attackers / population caps / fire bark; today we'd hand-script per-stage actions) |
| X9 queue ambience | `npc_move_to`, `recruit_ally` | **npc_ally_pair** (ped-recruits-ped follower pairing: parent walks in with kid, splits at the line, re-pairs to leave); **queue_line_simulation** (slot-based line: arrivals claim slots, advance on service, served-at-front beat, scatter-all on a flee stimulus) — both are ambience-only; a v1 can fake this with static spawned families and skip the sim |
| X5/X9 flee cascade | `npc_flee_task_watch` (2_06) | none new — here it's **group_flee** (existing) triggered by any member's flee task |
| X5 per-prop one-shot barks | `contextual_commentary` | none new |
| area fail | `stay_in_zone_fail`, `zone_exit_countdown` | none new — variant note: **cumulative** out-of-zone budget (not continuous) + repeated warning text |
| X10 pass | `completion_banner`, `give_clothing`, `player_safe_mode` (3_08 note) | none new |

## 6. Open questions

- Ped flag 68 semantics (set on elves, cleared when they attack) and social action id 30 — value tables unknown.
- Weapon 323's purpose (loaded, unused); confirm 300 = snowball against `default.ide`/weapon data.
- `MODELENUM._TO_ElfM/_TO_ElfF` numeric ids (enum unresolved in the dump).
- Emotion id 7 (bum → player) — same table gap as 3_08.
- Whether the elf opening-bark flag (`bRunningStartDialog`) was meant to be set true somewhere (dead branch shortens the Santa-with-kid beat during the barks).
- PAnim pool semantics (`PAnimGetPoolIndex(name, x, y, z, 1)`) — how breakable-prop pools map to our destructible actors.
- Retry behavior of the baked `3-01BA` intro (no attempt-count gate in-file; framework-side default assumed, as with other chapters).
- Whether cops are genuinely excluded during the spree (canonical guides claim the grotto area is bust-free; nothing in-file disables law enforcement — possibly ambient-population zeroing covers it, **INFERRED**).
