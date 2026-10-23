# 2_R03 — Paper Route (repeatable delivery job): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/2_R03.lua` (1520 lines) + `secnd/2_R03_Conv.lua` (see its own note) + `SMissPass.lua` (no 2_R03 case — **VERIFIED** absence) + `SZone.lua` (weapon ids). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**2_R03 = the "Paper Route" side job** (newspaper delivery by bike in the rich neighborhood) — **VERIFIED** by content: mailboxes (`_RICH_MAILBOX01-25`), the thrown-paper weapon **320 = "newspaper"** (VERIFIED name via `SZone.lua`), a per-delivery money reward, tier scaling from `GetMissionSuccessCount("2_R03_X")` (the repeatable variant's id), anim group `2_R03PaperRoute`, music `MS_BikeFun*`, and hazards (dogs, cars, rival paperboys, egg-throwing bullies).

**IDENTIFICATION SURPRISE:** the assignment guessed "likely race missions like Race the Vale" for the `2_R0x` files — **wrong for both 2_R03 and 2_R04**. Race the Vale is `2_04` (see sibling doc `2_04-race-the-vale.md`); the `2_R*` ids are repeatable **town jobs**, not races. Also note the script is one job with two entry tiers: `TIER_TUTORIAL` (first run, in-engine intro NIS) vs `TIER_REPEATABLE`, chosen by an external `F_SetPaperRouteTier(tier)` call before `main()` (launcher-owned — **OPEN**).

Setting: entirely outdoors (area 0), rich neighborhood. Population overridden to mostly-zero with two categories at 2, ambient vehicles overridden (2,1,1,0), bus stops disabled, nav excluders `_PR_HILLTOPEXCLUDER`/`_PAPERROUTE_EXCLUDER` armed.

## 2. Mission flow

Driver: `main()` waits for the tier, runs `PaperRouteInit(tier)`, then polls `gCurrentStage` (`Stage01_GetToHillInit` → tutorial/job loop → `Stage02_DeliverPapersInit/Loop`) under `MISSION_RUNNING/PASS/FAIL`. Threads: `T_DogHazardsMonitor`, `T_CarHazardsMonitor`, `T_MailManHazardsMonitor`, `T_OldLadyHazardSpawn`, `T_JoggerHazardSpawn` (per tier).

| # | Beat | Logic (functional) |
|---|---|---|
| R0 | **Setup** | `PaperRoute.DAT` + `Mailboxes_Rich.DAT`; radar clamped (30/65/30); ambient mailboxes cleared (`shared.MailboxesRespawn` = DELETE + forced area reload); interactive music off; models loaded (cars 293/294, dogs 219/220, customers {101,100,144,148,149,135}, drivers 78-81, vendor 116, mailman 127, old lady 185, orderly 53, jogger 55, egger 30); weapon 320 requested; the player's current bike is **deleted** and replaced by the mission bike (281 at `_2_R03_BIKESTART`); mission mailbox props created **targetable** with streaming disabled; per-tier mailbox/hazard tables built. |
| R1 | **Intro NIS (tier 0 only)** | In-engine, control off, widescreen: news vendor (116) at `_2_R03_VENDOR`; three speech-node exchanges (`M_2_R03` 1-3) over three fixed camera/FOV setups; vendor goes ambient. |
| R2 | **Get to the hilltop (35 s)** | Objective `2_R03_OBJ01` + blip on `_2_R03_HILLTOP`; player auto-mounted on the bike (`PlayerPutOnBike`). Timer 35 s: timeout fails (`2_R03_FAIL04` tutorial / `2_R03_FAIL05` job). Tutorial tier: reaching `_PR_HILLTOP` plays `NIS_Tutorial` (camera path `_2_R03_INTROCAM` aimed at mailbox 03, tutorial text, `FollowCamSetVehicleShot("PaperRoute")`) and starts the objective with a param counter (`2_R03_OBJ02`, min papers); job tier: entering `_PR_PAPERPICKUPTRIGGER` (where the fake paper-stack pickup sits) starts deliveries directly. |
| R3 | **Deliver papers (timed)** | Player given weapon **320 with 30 rounds** (safety-granted if missing). Mailboxes blipped (short-ranged). A delivered paper = a hit on a mailbox prop (`cbMailBoxHit` — registration path **OPEN**, likely the mailbox prop tree): +1 counter (icon `mailbox`), **+200 cents**, blip off, prop untargetable; if the mailbox has a resident ped (points `_PR_MAILBOXPED09/12/15/21`), the resident plays a thank-you anim and goes ambient. **Hazards per tier** (see table below). Win when delivered ≥ tier minimum; fail when the tier timer expires short of the minimum (`2_R03_FAIL02`) or the player has been out of papers for a 4 s grace (`2_R03_FAIL03`). |
| R4 | **Pass** | Widecreen, `StatAddToInt(171, gReward)` (money stat, **INFERRED**), pass music, **completion banner carrying the money amount** (`MinigameSetCompletion("M_PASS", true, gReward, bonus-msg)` — the bonus message varies by tier), `MissionSucceed(false, false, false)`. Cleanup destroys weapon 320, restores ambient mailboxes/population/POI/bus stops, removes hazards, restores the default vehicle camera. |

### Tiers (difficulty = `GetMissionSuccessCount("2_R03_X")`)

| Tier | Time | Min papers | Mailboxes | Hazards added |
|---|---|---|---|---|
| 0 (tutorial) | 60 s | 7 | 10 (subset of 03-14) | 2 dogs, 1 car |
| 1, difficulty 1 | 120 s | 10 | 15 | 1 dog, 2 cars |
| 1, difficulty 2 | 120 s | 14 | 19 | 1 dog, 2 cars, 1 mailman, 1 egger |
| 1, difficulty 3 | 120 s | 19 | 25 | 2 dogs, 3 cars, 4 eggers (1 randomized), old-lady + jogger threads |
| 1, difficulty ≥ 4 | 180 s | 24 | 25 | 5 dogs / 5 cars / 3 mailmen (2 randomized each), 4 eggers (1 randomized), old lady + jogger at 50% each |

Hazard behaviors: **dogs** (219/220) tethered to yard points (radius 5), attack on trigger enter; **cars** (293/294) parked with a cheap, asleep, player-ignoring driver (78-81 random) — trigger enter → drive their path at 7 m/s, leaving the trigger → drive off to `_PR_CARMOVETO`, despawn 70 m+ from the player; **mailmen** (127 — rival paperboys, **INFERRED**) tethered, attack on trigger; **eggers** (30 — bullies with **312 = eggs**, VERIFIED name) posted in cover behavior vs. the player; **old lady (185) fleeing an orderly (53)** with infinite sprint + tether and a 5 s bark loop (`TAUNT_AGGRO` + random `BUMP_RUDE`/`TAUNT`/`INDIGNANT`/`BOISTEROUS`) — pure comic ambience; **jogger** (55) looping `_PR_JOGGERPATH`.

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Hilltop not reached in 35 s | mission timer | `2_R03_FAIL04` / `2_R03_FAIL05` |
| Timer out with < min papers | mission timer | `2_R03_FAIL02` |
| Out of papers (4 s grace) | `F_RanOutOfPapers` poll | `2_R03_FAIL03` |

All fails `MissionFail(true, true, key)` — fade + retry. No baked cutscene, so no skip logic needed; tier 0's NIS plays only on the tutorial tier.

### Rewards / pass side-effects

- **Money**: 200 cents per delivered paper, paid via the pass banner and stat 171 (**INFERRED** a job-earnings stat). No weapon/item/unlock. No `SMissPass` entry (**VERIFIED** absence).
- The difficulty ladder itself (success count of `2_R03_X`) is the progression.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | R1 vendor intro (tier 0) | in-engine, ~10 s | `letterbox` + 2-3 `camera` setups + 3 `say` beats |
| CS-2 | R2 tutorial cam | in-engine | `camera` path aimed at the first mailbox + tutorial `banner`; then `vehicle camera` preset |

Cutscene anim groups: `NIS_0_00A` (generic NIS set), `F_Adult`; job groups `2_R03PaperRoute`, `SBULL_S`.

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 116 | news vendor | intro NIS only |
| 281 | mission bike | replaces the player's current bike (deleted) |
| 293, 294 | hazard cars | random per spawn |
| 78-81 | car drivers | cheap, asleep, stimuli/attack-ignoring, untargetable |
| 219, 220 | dogs | unique-ified |
| 127 | rival mailman | tether + attack on approach |
| 30 | egger bullies | weapon 312 eggs ×4, cover behavior at fixed points |
| 101/100/144/148/149/135 | mailbox residents | tethered by their box; thank-you anim on delivery |
| 185, 53 | old lady + orderly gag | bark loop pair |
| 55 | jogger | path loop |

### Animation groups / action trees / props

`LoadAnimationGroup`: `NIS_0_00A`, `F_Adult`, `2_R03PaperRoute`, `SBULL_S`.
`LoadActionTree`: `Act/Conv/2_R03.act` (intro speech nodes + `ThankYou`), `Act/Props/MailBox.act` (mailbox hit behavior — the likely `cbMailBoxHit` registrar, **INFERRED**).
Props: 25 mailbox PAnims (`_RICH_MAILBOX01-25`), created targetable, streaming off. Fake paper-stack pickup (320) at `_2_R03_PAPERDROP` as route dressing.

### Speech / audio / text

- `M_2_R03` ids 1, 2, 3 (intro) — **3 scripted lines**; ambient bark sets `TAUNT_AGGRO`, `BUMP_RUDE`, `TAUNT`, `INDIGNANT`, `BOISTEROUS`.
- Music: `MS_BikeFunLow/High` interactive streams; pass/fail stingers (bank 8).
- Tutorials: `2_R03_05`, plus reused engine tutorial keys `BUSTPREFNIGHT` and `HIDING2X` (odd recycled ids — **INFERRED** generic "throw to deliver" tutorials; names don't match content).
- Text keys: objectives `2_R03_OBJ01/02`, pass banners `2_R03_PASS01/02/03`, fails `2_R03_FAIL02..05`.

### Shared-state contract

`shared.MailboxesRespawn` (DELETE/CREATE around the job — the ambient mailbox system is owned elsewhere), `shared.gDisableBusStops`, `shared.gConversationOver_2_R03` (see the `2_R03_Conv` note; **not read by this file**).

### Named world data to extract from `PaperRoute.DAT` + `Mailboxes_Rich.DAT`

- POINTLISTs: `_2_R03_PLAYERSTART` (1-2), `_2_R03_BIKESTART`, `_2_R03_VENDOR`, `_2_R03_PAPERDROP`, `_2_R03_HILLTOP`, `_PR_CARMOVETO`, `_PR_MAILBOXPED09/12/15/21`, `_PR_MAILBOXDOG00-04`, `_PR_CARSPAWN01-05`, `_PR_DRIVERSPAWN`, `_PR_MAILMAN01-03`, `_PR_EGGER01-04`, `_PR_OLDLADYNORTH`, `_PR_OLDLADYSOUTH`, `_PR_ORDERLYNORTH`, `_PR_ORDERLYSOUTH`, `_PR_JOGGER`.
- PATHs: `_2_R03_INTROCAM`, `_PR_CARPATH01-05`, `_PR_OLDLADYNORTHPATH`, `_PR_OLDLADYSOUTHPATH`, `_PR_JOGGERPATH`.
- TRIGGERs: `_RICH_MAILBOX01`..`_RICH_MAILBOX25` (props), `_PR_HILLTOP`, `_PR_PAPERPICKUPTRIGGER`, `_PR_DOGSTART00-04`, `_PR_CARSTART01-05`, `_PR_MAILMANSTART01-03`, `_PR_OLDLADYNORTHSTART`, `_PR_OLDLADYSOUTHSTART`, `_PR_JOGGERTRIGGER`, `_PR_HILLTOPEXCLUDER`, `_PAPERROUTE_EXCLUDER`.
- Hardcoded cameras: intro (409.29, 244.62, 10.36), (410.26, 238.30, 10.30), (409.89, 242.38, 10.38) with FOVs 70/50/50.
- Area codes seen: **0** only.

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| Tiers | mission framework | **repeatable_job_tiers** (difficulty table indexed by `GetMissionSuccessCount` of the repeatable id: time/quota/mailbox set/hazard sets; tier chosen by launcher param) |
| R3 deliveries | `collect_counter` (icon + max), `pickup_spawner`, `dynamic_blip` (short-ranged) | **deliverable_target_props** (targetable prop set; projectile hit on prop = delivery: counter +1, payout +N, resident thank-you anim, blip off, untargetable) |
| R3 payout | `completion_banner` | **money_reward_accumulator** (per-event payout summed, shown as the banner's numeric param, added to a stat) — may already exist in ch1 errand docs; if so reuse |
| hazards | `spawn_npc` bundles, `npc_tether_trigger`, `npc_cover_behavior` (eggers), `npc_follow_path` | **hazard_randomizer** (activate a random 50% subset of a hazard list with a cap); **vehicle_hazard_controller** (parked car + asleep driver; trigger → drive path; exit trigger → drive to despawn point; delete beyond 70 m) |
| bike | `dismount_vehicle` | **vehicle_grant** (delete player's current vehicle, spawn the mission vehicle, auto-mount) + **vehicle_camera_preset** (`FollowCamSetVehicleShot("PaperRoute")`, restored in cleanup) |
| old lady gag | `npc_follow_path`, `bark_loop`, tether | — |

## 6. Open questions

- `cbMailBoxHit` registration path (probably `Act/Props/MailBox.act` or a library — behavior clear, wiring **OPEN**).
- How the launcher chooses tier and where the job is started in the world (mission table outside the Lua).
- Stat 171 semantics (**INFERRED** job earnings).
- Whether missing papers can be restocked mid-run (no resupply point in-script — running dry fails; the 30-round start with min ≤ 24 gives ~6 misses of slack, tier-dependent).
- The recycled tutorial keys (`BUSTPREFNIGHT`, `HIDING2X`) — real intent unconfirmable without the text banks.
