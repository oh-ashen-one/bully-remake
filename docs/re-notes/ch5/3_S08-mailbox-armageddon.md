# 3_S08 — Mailbox Armageddon (mailbox-smashing bike ride, Old Bullworth Vale): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap5/3_S08.lua` (848 lines, complete) + `SMissPass.lua` (no 3_S08 case — **VERIFIED** absence) + `SZone.lua` (weapon ids) + `AreaScripts/MainMap.lua` (mailbox ambient consumer). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_S08 = "Mailbox Armageddon"** — the Scholarship/Anniversary-exclusive Chapter 5 side mission: grab a bat, get on a bike, and smash the rich neighborhood's mailboxes against a clock. **VERIFIED** by content (vehicle 281 = bike spawned at `_3_S08_BIKESTART`, the player force-mounted; weapon model 300 = "bat" per `SZone.lua`'s weapon table; 20 `_RICH_MAILBOX*` PAnim props counted via `PAnimIsDestroyed`; counter HUD icon `mailboxb`; dog hazards; Old Bullworth Vale population/vehicle overrides) and externally: the [fandom Chapter V page](https://bully.fandom.com/wiki/Chapter_V) lists "Mailbox Armageddon `3-S08`" as an SE/Anniversary-exclusive Chapter V non-storyline mission, and [TCRF's SE unused-audio page](https://tcrf.net/Bully:_Scholarship_Edition_(Wii)/Unused_Audio) names the mission "Mailbox Armageddon" with intro dialogue from **Clint** telling Jimmy to get rid of the "rich people" problem.

The giver is **Clint** (townie, ped model 45 — **INFERRED**: TCRF attributes the intro/outro lines to Clint; the script's outro ped variable is misleadingly named `gEdgar`, a copy-paste fossil — the real Edgar is model 91 per 5_B).

**When offered:** Chapter 5, non-storyline (fandom listing); the repo catalog currently lacks a Mailbox Armageddon row entirely — worth a catalog correction. The star-marker window/giver spot live in the mission table outside the Lua — **OPEN**. Script lives in `chap5/` despite the 3_ prefix — same authoring-artifact family as 3_S03.

Structure: baked intro → a two-slot state machine (`tblStates = { F_SmashMailboxesInit, F_SmashMailboxes }`) run off a `MISSION_RUNNING/PASS/FAIL` enum, with a per-frame destroy poll (`F_CheckMailboxes`) doing the actual counting. Threads: `T_DogHazardsMonitor`. Baked cutscenes: `"3-S08"` (intro) only; the outro is in-engine.

## 2. Mission flow

Driver: `MissionSetup()` (world state + 3 DAT layers) → `main()` → `F_MailboxSmash_Intro()` (baked cutscene + mount) → state loop → `F_MailboxSmash_End()` (pass NIS) or `MissionFail`.

| # | Beat | Logic (functional) |
|---|---|---|
| M0 | **Setup** | Ambient mailboxes suppressed (`shared.MailboxesRespawn = DELETE` + forced area reload — consumed by `AreaScripts/MainMap.lua`'s `F_RichAreaMailboxesCreate/Remove`, VERIFIED consumer); three DAT layers loaded (`3_S08.DAT` + `PaperRoute.DAT` + `Mailboxes_Rich.DAT`); radar range set (30/65/30); anim groups `NIS_0_00A`/`F_Adult`/`2_R03PaperRoute`/`SBULL_S`; bat model 300 requested; dog models {219, 220} + mailbox model + 521 (broken-mailbox swap, **INFERRED**) loaded and made unique; nav nodes off inside `_PR_HILLTOPEXCLUDER`/`_PAPERROUTE_EXCLUDER`; population overridden (heavy adults/mailmen flavor — params `(8,0,…,10,0,2,…)`); ambient vehicles overridden `(2,1,1,0)`; all vehicles + peds cleared; bus stops disabled (`shared.gDisableBusStops`). |
| M1 | **Intro + mount** | Player NIS-safe; mailboxes + dogs set up per tier (see tier table — **`gDifficulty` is hardcoded 2**, so only tier 3 ships: 20 mailboxes, 180 s, 2 dogs); any player vehicle is deleted and replaced by bike 281 at `_3_S08_BIKESTART`; baked **`"3-S08"`**; player force-armed with the bat (`PedSetWeaponNow 300`) and **put on the bike** (`PlayerPutOnBike`, wait until mounted). |
| M2 | **Smash stage** | Objective with a live remaining-count param (starts at 20); every unsmashed mailbox gets a blip (anchor position query); counter HUD shown (icon `mailboxb`, current 0 / max 20); dog-hazard monitor thread starts; **mission timer 180 s**. Per frame, `F_CheckMailboxes` polls each mailbox's PAnim pool slot for `PAnimIsDestroyed`: on a fresh break → count++, bat re-equipped (auto-recover after a swing, **INFERRED**), counter incremented, objective param decremented, blip removed, 450 ms later the prop is deleted (leaving the broken stand-in, **INFERRED** from model 521 + `PAnimDelete`). |
| M3 | **Dogs** | Per tier, tethered dogs (219/220) spawn at `_PR_MAILBOXDOG0x` points; player entering a `_PR_DOGSTART0x` trigger → that dog is untethered and set on the player (one-shot each). Tier 4's `HazardsRandomize` (50% roll, cap 2) exists but ships unreachable. |
| M4 | **End condition** | Timer expires or quota met → timer stops; quota met → PASS else FAIL. |
| M5 | **Pass NIS (in-engine)** | Bat destroyed; control off; HUD counter off; fade; bike deleted; Clint (45) spawned facing the player at `_3_S08_SCENEPLAYER`/`_3_S08_SCENEEDGER` (dev typo for "Edgar" in the point name — VERIFIED); widescreen FOV-70 two-shot; Clint bark (`M_3_S08` id 0, supersize), Clint put to sleep, player reply (id 0); fade out/in; pass music 8; completion banner `M_PASS` with **2000 = $20**; Clint deleted; cameras/FOV restored; `MissionSucceed(false, false, false)`. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| 180 s timer expires below quota | `MissionTimerHasFinished` in `F_SmashMailboxes` | none — `MissionFail(true, true)` generic + fail music 8 |

No other fail paths (no bust/death handling in-file; authority response is the punishment system's, same posture as 2_S02 — **OPEN**). Retry allowed; full restart from the intro.

### Rewards / pass side-effects

- In-script: **$20** banner money (2000). No respect, no unlocks, no `SMissPass` case (**VERIFIED** absence).
- Ambient state fully restored in cleanup (mailboxes respawn, population/vehicles/nav/bus stops reverted).

### Dead / unused content (VERIFIED unless noted)

- **Difficulty ladder unreachable**: `gDifficulty = 2` never changes, so tiers 0 (15 boxes / 600 s / 1 dog), 1 (19 boxes / 120 s / 1 dog) and 3+ (25 boxes / 180 s / up to 2-of-5 randomized dogs) are dead. The shipped tier 3 is 20 boxes / 180 s / 2 fixed dogs.
- `F_SmashMailboxesRide` / `F_SmashMailboxesRide_Loop` + `gMinRideTime` (120 s ride-to-the-neighborhood phase with its own blip/objective) are **not wired into `tblStates`** — dead. The shipped mission starts you already at the mailboxes.
- `cbMailBoxHitBat` (bat-hit callback with a first-smash `BUSTPREFNIGHT` tutorial and `MailBoxPedThank`) has no registration in this file — either an engine naming-convention callback (**OPEN**) or dead; the live counter is the `PAnimIsDestroyed` poll. Its `MailBoxPedThank` call would be nil-safe-dead anyway: `mailbox.ped` is never set in 3_S08 (the helper lives in `2_R03.lua`, the paper route).

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | M1 intro (Clint's offer) | baked `"3-S08"` | `fade` + original establishing card |
| CS-2 | M5 pass outro (Clint exchange) | in-engine, one fixed two-shot | `camera` + `speech_sequence` (2 lines) + `npc_asleep_staging` gag |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 45 | **Clint** (townie giver) | outro only; `gEdgar` var name is a fossil |
| 219, 220 | dogs | tethered hazards; untether + attack on trigger breach; made unique models for the mission |

### Props / vehicles / weapons

- Vehicle 281 (bike — spawned, force-mounted, deleted on both outcomes).
- Weapon 300 = baseball bat (`SZone.lua` weapon table, VERIFIED name "bat") — force-equipped at start, re-equipped after each smash, destroyed at end.
- `MODELENUM._MAILBOX` + model 521 (intact/broken pair, **INFERRED**); up to 25 `_RICH_MAILBOX01..25` PAnim props, created targetable with streaming pinned.

### Speech / audio

`M_3_S08` id 0 (both sides of the outro). Counter icons `mailboxb`/`mailboxb_x`; tutorial `BUSTPREFNIGHT` (in the maybe-dead bat callback); end music track 8. No interactive music control in-file (cleanup re-enables it defensively).

### Named world data to extract (trigger data **VERIFIED present** — three layers: `DATLoad("3_S08.DAT"/"PaperRoute.DAT"/"Mailboxes_Rich.DAT", 2)`; none yet extracted to `imported/triggers/`; note it shares PaperRoute/Mailboxes_Rich layers with `2_R03`)

- POINTLISTs: `_3_S08_BIKESTART`, `_3_S08_SCENEPLAYER`, `_3_S08_SCENEEDGER`, `_3_S08_MAILBOXES`, `_PR_CARMOVETO`, `_PR_MAILBOXDOG00..04`.
- TRIGGERs: `_RICH_MAILBOX01..25` (mailbox anchors), `_PR_DOGSTART00..04` (dog aggro zones), `_PR_HILLTOPEXCLUDER`, `_PAPERROUTE_EXCLUDER` (nav exclusion).
- Hardcoded cameras: intro (283.34, -460.47, 5.45 → …), outro (288.34, -461.30, 6.20 → 287.37, -461.36, 5.96).
- Area codes seen: 0 only.

## 5. Framework gaps (beat → primitives)

Deduped against the ch1–ch4 gap pools and the `MissionSubsystem.h` schema.

| Beat | Covered by | New primitives needed |
|---|---|---|
| M0 world swap | `area_script_shared_state` (the `shared.MailboxesRespawn` flag + area reload — same mechanism) + `population_override`, `ambient_vehicle_override`, `nav_block_radius` family | none new (note the pattern: mission-critical prop set replaces the ambient set for the duration) |
| M0 radar | `radar_set_range` (pool) | none new |
| M1 mount | `vehicle_grant` (pool), `player_mounted` (pool, condition) | **forced_vehicle_mount** (despawn the player's current vehicle, spawn the mission vehicle, put the player on it, wait until mounted; teardown deletes it — glue between grant and gameplay) |
| M1 arming | `give_weapon` + `weapon_hud_lock` (pool) + `weapon_equipped` (pool) | **weapon_force_equip_rearm** modifier (force-now + silent re-equip after each prop smash + destroy on exit; the re-arm-on-smash is the new bit) |
| M2 prop set | `destructible_prop_objective` / `prop_destroy_threshold_event` (pool, partial) | **targetable_prop_registry** (create PAnim props at DAT trigger anchors, make targetable, pin streaming, per-prop anchor position query) |
| M2 counting | `collect_counter` / `counter_threshold_meter` (pool, partial) | **prop_destroy_poll_counter** (per-frame `PAnimIsDestroyed` pool poll per registered prop; on fresh break: count++, HUD counter increment, blip cleanup, staged prop delete; quota → pass event) |
| M2 objective text | `objective_checklist` (pool, partial) | **objective_countdown_param** (`MissionObjectiveUpdateParam` — live "N remaining" number inside the objective text) |
| M2 timer fail | `countdown_fail_timer` (pool) | none new |
| M3 dogs | `hazard_randomizer` (pool — the 50%/cap-2 roll ships dead) | **tethered_dog_ambush** (dog spawned tethered to a point; player enters the paired trigger → untether + attack, one-shot per dog) |
| M5 outro | `camera` + `speech_sequence` + `npc_asleep_staging` (pool) | none new |
| pass | `completion_banner` + `reward` (money) | none new |

## 6. Open questions

- Star-marker window, giver spot and exact unlock point in Chapter 5 — mission table, outside the Lua.
- Whether `cbMailBoxHitBat` is engine-dispatched (naming convention on bat hits) or dead; if dead, the `BUSTPREFNIGHT` tutorial never shows and counting is purely the destroy poll.
- Model 521's role (broken-mailbox stand-in?) and whether `PAnimMakeTargetable` + bat swings are what flip `PAnimIsDestroyed` — mechanic confirmation needs the prop data.
- Dog models 219/220 identities (which dogs) and why they are made unique.
- The dead ride-in phase (`F_SmashMailboxesRide`) and tier ladder look like a cut "get there in 2 minutes" opening plus difficulty scaling — confirm against the baked intro whether any of it survives visually.
- Population tuple `(8,0,0,0,0,0,0,0,10,0,2,0,0)` slot meanings (mailmen/joggers flavor per the pool docs — needs the population-slot legend).
