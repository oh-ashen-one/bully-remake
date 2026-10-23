# 4_G4 — Discretion Assured (Mandy poster tag-over): recon notes

Status: 2026-10-23, read-only recon. Confidence tags as in `format-survey.md` / `opening-slice-recon.md`: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/secnd/4_G4.lua` (1627 lines, complete) + `main.lua` (gift-requirement hook for Mandy, model 14, gated on `4_G4`). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**4_G4 = Discretion Assured** — catalog entry #51 (Chapter 4, 8AM–9PM). **VERIFIED** by content: mission-only tagging (`TaggingOnlyShowMissionTags(true)`) of Mandy posters (`CounterSetIcon("MandPost", …)`, target props named `…SCHOOLPOSTER…/…BUSPOSTER…`), Mandy (ped model 14) as the mission-critical quest-giver with a **kiss-scene reward** (`MakeOut` action nodes + `MRESPECT_NM5` respect message + completion key `4_G4_RESPECT`), pass-side `SetFactionRespect(1, GetFactionRespect(1) - 5)` (nerds −5), and `main.lua:532-533` installing a gift requirement on Mandy's model gated on mission `4_G4` (the kiss prerequisite chain, **INFERRED**). Cutscene `"4-G4"` on entry.

Structure: function-pointer stage machine (`gStageUpdateFunction`) with a `gMissionStage` string state (`"running" | "OutOfMoney" | "LeftTown" | "failed" | "passed"`) plus a numeric `nMissionPhase` (1 school → 2 business → 3 done-tagging) driving the pervert/vignette spawner. Persistent threads: `L_MonitorTags` (LibTagging), `T_MandyMonitor`, `MonitorPerverts`, `T_MonitorTimer`, `T_MonitorAttackedPervs`, later `T_AlgieTagCleanup`. Library imports: `LibTagging`, `LibTable`, `LibObjective`, `LibPed`.

Setting: school campus + Bullworth business district, **area 0** with interior checks (area **13** watched for Mandy despawn). Timer: **300 s** for the school leg, **+300 s bonus** added when the town leg starts (`MissionTimerStart(timeLeft + gBonusTime)`).

## 2. Mission flow

Driver: `MissionSetup()` (no fade-in; mission-tags-only; three-intensity interactive music `MS_SearchingLow/Mid/High`; baked `"4-G4"`; `4_G4.DAT` layer 2; **money floor 100** so the player can always buy spray) → `main()` → `InitialSetup()` → stage loop.

| # | Beat | Logic (functional) |
|---|---|---|
| G0 | **Setup** | Spray can 321 requested; 9 anim groups (graffiti minigame `MINIGraf`, hangout/talking/smoking/social/love sets); 8 poster PAnims pre-loaded; **unique-model suppression** (`PedSetUniqueModelStatus(id, -1)` for the 14 mission ped ids, saved + restored in cleanup); tag tables built (`F_PopulateTables`); valid tag faction set to **5**; school posters created; `4G4SchoolTags` registered with LibTagging; one comic-shop tag pre-marked done; transition to `_4_G4_PLAYERSTART`; persistent tagging started. |
| G1 | **Stage 1: school posters** | Player force-given spray + 3 cans if below 3; counter 0/**2** (Mandy-poster icon); objective `4_G4_OBJ1`; timer starts. Two school tags (`SchoolTagEntranceA`, `SchoolTagMain`) must be sprayed over. Library vignette: entering `_4_G4_LIBNERDS` spawns two nerds (4, 11, flag 122) who despawn when you leave; approaching → line id 199; hitting them or finishing a tag in their view → both attack, then wander off ambient. |
| G2 | **Spray watchdog** (all tagging stages) | `F_CheckSpray`: cans == 0 → if money < 100 and not already shopping → **fail "OutOfMoney"**; else objective swaps to `4_G4_SPRAYBUY` with a Yum Yum market blip (`_4G4_YUMYUM`) and all tag blips are suppressed; restocking restores the stage objective + blips. |
| G3 | **Tag resolution** | `F_OnTagDone` (LibTagging callback): tag validated as faction 5 (`L_TagIsFaction`) — valid → vandalism-tutorial flag cleared, counter +1, nearby "pervert" spectators react (per-entry `reaction`: flee-with-line or fight); invalid → tag auto-cleared (`L_ClearTag`). School counter full (2) → counter rebased 2/**4**, phase 2: timer extended +300, objective `4_G4_19`, business posters created, `4G4_BusTags` registered (cinema + statue tags). |
| G4 | **Algie counter-op cutscene** | First business-district tag completion (after any running speech finishes; player control briefly seized) → `F_DoAlgieCutscene`: in-engine NIS — player teleported to a fixed vantage, Algie (model 7) walks in, laments (line 22), and **posts a fresh poster** at the comic shop; that tag becomes live (`gTblComicBookTag`), counter max grows to **5**, player returned to their old position. |
| G5 | **Algie scrub patrol** | `T_AlgieTagCleanup`: Algie cycles paths between the three business tags (`_4G4_COMICTOBACKALLEY`, `_4G4_PITTOALLEY`, `_4G4_CINEMATOMEX`); on arrival at a **player-tagged** poster he scrubs/reposts it (`PlaceIt` anim, `4_G4_ALGIECLEAN` text): poster restored, **counter decremented**, tag re-enabled + re-blipped. Hitting Algie → cower + flee to `_4G4_THADRUN`; he resumes after 30 s or 40 m separation. A second provocation while seen/close (or another hit) → `F_DoAlgieRunNIS` (scream + cry line + flee NIS) and he quits for good (`PedMakeAmbient`, thread ends). |
| G6 | **Town leg + jock guards** | `F_Stage2`→`F_Stage3` on entering `_AMB_BUSINESS_AREA`. Tagging near the comic-shop trigger `_4_G4_NERDCOMICS` while the two resident jocks (models 18, 13) are alive → they attack (one-time, `bPissedOffJocks`). **Town tether** (`F_MonitorJimmy`): leaving the business area during the town leg prints a search warning; straying out of `_4_G4_LEFTTOWN` → fail "LeftTown". Pervert spectators: 8 tabled peds in two phases with buddy-chain retaliation (`T_MonitorAttackedPervs` — hit one and its buddy chain joins). |
| G7 | **All 5 tags → rendezvous** | Counter full → tagging phase 3: timer stopped, counter HUD off, objective `4_G4_OBJ2`, blip to Mandy at `_4G4_MANDYEND`. Mandy spawns stationary with flag 113 + mission-critical (`CB_MandyAttacked` → fail `4_G4_MANDHIT`); area-13 checks de/respawn her around interior transitions. |
| G8 | **Kiss reward + pass** | Player within 3 m → NIS-safe beat: mutual face/approach, her line 27, two-person camera offset, make-out action nodes (6 s), completion banner `M_PASS` + `4_G4_RESPECT` + `MRESPECT_NM5`, win music track 4; wait for kiss nodes to finish; Mandy made ambient. Pass tail: music off, **nerd respect −5** (`SetFactionRespect(1, current − 5)`), `MissionSucceed(false, false, false)`, `AreaEnsureSpecialEntitiesAreCreatedWithOverride("4_G4", 4)` (post-mission world dressing, **INFERRED**), fade back in. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Timer expires (before tagging done) | `T_MonitorTimer` (only fails while `nMissionPhase < 3`) | `4_G4_TIMEUP` |
| No spray and money < 100 | `F_CheckSpray` (money floor 100 at start prevents this at launch) | `CMN_STR_06` |
| Left the business district during town leg | `F_MonitorJimmy` warn → leave `_4_G4_LEFTTOWN` | `4_G4_FAILLEFT` |
| Mandy hurt by player | mission-critical callback / `T_MandyMonitor` | `4_G4_MANDHIT` |
| (Player death) | no in-script watch — engine default | — |

No checkpointing; retry = full restart. Cleanup restores unique-model caps, deletes all mission tags/posters per phase reached, stops persistent tagging, releases Mandy to ambient.

### Rewards / pass side-effects

- In-script only: kiss scene, nerd respect −5, special-entity set `"4_G4"` created. **No `SMissPass` entry** (VERIFIED absence). No money/weapon/unlock key beyond the completion banner.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | G0 opening | baked `"4-G4"` | `fade` + original establishing card |
| CS-2 | G4 Algie re-posting | in-engine (~8 s) | `camera` fixed + `npc_follow_path` + `ped_action_node` (PlaceIt) + `say` + prop spawn |
| CS-3 | G5 Algie scared off | in-engine (~6 s) | `camera` look-at + `ped_action_node` scream + `say` + flee |
| CS-4 | G8 kiss | in-engine pair scene | `pair_anim` (MakeOut nodes) + `camera` two-shot offset + `completion_banner` |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 14 | **Mandy** | quest-giver/reward; mission-critical; flag 113; attitude 3 (adoring) → ambient after kiss |
| 7 | **Algie** | counter-op scrubber; cower/flee on hits; permanent flee on second provocation |
| 4, 11 | library nerds ×2 | trigger-gated despawn/respawn; witness→attack on tagging in view |
| 18, 13 | comic-shop jocks ×2 | one-time attack when the player tags near `_4_G4_NERDCOMICS` |
| 85, 99, 146 / 76, 144 / 149 | spectator "perverts" | phase 1 school trio (buddy chain a→b→c); phase 2 cinema pair + statue solo; per-entry flee/fight reaction on tag completion |
| 15, 20 | loaded, unused in-file tables | **dead cast** (in the suppress list; no spawn entries — **OPEN**) |

### Animation groups / action trees

`MINIGraf`, `Hang_Jock`, `Hang_Talking`, `NPC_Love`, `NPC_NeedsResolving`, `POI_Smoking`, `GEN_Socia`, `IDLE_SEXY_C`, `SGIRL_F`; `Act/Conv/4_G4.act` (talk loops, cower, scream, PlaceIt, Mandy beats); `Act/Props/TagSmall.act` (tag states); `Act/Player.act` (MakeOut).

### Props / pickups / weapons

- 8 poster PAnims (`_4_G4_SCHOOLPOSTER02/03/04`, `_4_G4_BUSPOSTER02/03/04/06A/08`) + their tag PAnims; tags are faction-5-validated LibTagging targets with per-tag callbacks (`OnTagDone`, `OnClean`, `count` 30).
- Weapon 321 (spray can; force-issued to 3 cans at start if low).

### Speech

`M_4_G4` ids 5, 13, 14, 15, 22, 27, 199; event barks `FIGHTING`, `SEE_VANDALISM`, `VICTIMIZED`, `TAUNT_RESPONSE_CRY`, `WTF_TV` (queued). Roughly **10 distinct scripted lines** + barks. Music: three-intensity set `MS_SearchingLow/Mid/High.rsm`; end music track 4.

### Named world data to extract from `4_G4.DAT` (trigger data **VERIFIED present** — `DATLoad("4_G4.DAT", 2)`; not yet extracted to `imported/triggers/`)

- POINTLISTs: `_4_G4_PLAYERSTART`, `_4_G4_PEDALGIESTART`, `_4_G4_ALGIECUT` (3), `_4_G4_SCHOOLTAG3A` (3), `_4_G4_BUSTAG2A` (2), `_4_G4_BUSTAG4A` (2), `_4_G4_BUSTAG8A`, `_4G4_NERDLIBRARY` (2), `_4G4_THADRUN`, `_4G4_MANDYEND`, `_4G4_YUMYUM`.
- PATHs: `_4G4_COMICTOBACKALLEY`, `_4G4_PITTOALLEY`, `_4G4_CINEMATOMEX` (Algie legs), `_4G4_ALGIEPATH`, `_4G4_PERV1`.
- TRIGGERs: `_4_G4_SCHOOLTAG2/3/4`, `_4_G4_TILTTAG1`, `_4_G4_BUSTAG2/4/8`, `_4_G4_SCHOOLPOSTER02/03/04`, `_4_G4_BUSPOSTER02/03/04/06A/08`, `_4_G4_LIBNERDS`, `_4_G4_NERDCOMICS`, `_AMB_BUSINESS_AREA`, `_4_G4_LEFTTOWN`.
- Hardcoded cameras: Algie cutscene (512.33, -64.18, 6.17 → …; player teleport 516.14, -70.54, 4.64), Algie flee offset cam.
- Area codes seen: **0** (overworld), **13** (interior watched for Mandy handling).

## 5. Framework gaps (beat → primitives)

| Beat | Covered by | New primitives needed |
|---|---|---|
| G0 setup | `mission framework`, `unique_ped_suppress`, `money_floor_ensure` (pool), `tag_prop_system` (3_S10 pool) | reuse; note tag **faction validation** (`L_TagIsFaction`, invalid → auto-clear) as a `tag_prop_system` parameter |
| G1 witness nerds | `npc_witness_event` (pool, partial) | reuse + variant: trigger-gated despawn-on-leave pair who attack if you tag in view (fold into `npc_witness_event`) |
| G2 spray watchdog | `consumable_watchdog`, `money_floor_fail` (pool) | reuse; variant note: objective-swap + shop blip while dry |
| G3 tag counting | `collect_counter` (pool) | reuse |
| G4/G5 Algie | `npc_follow_path` + node cb, `ped_hit_event` (pool) | **rival_undo_patrol** (rival NPC cycles between objective props and un-does completed ones: poster restored, counter decremented, tag re-armed; hit → cower+flee; resume after 30 s / 40 m; second provocation → permanent flee NIS + thread end) |
| G4 counter growth | — | **counter_max_growth** (new objective props added mid-mission raise the counter max) — fold into `collect_counter`? flag the dynamic-max difference |
| G6 town tether | `leave_area_fail` (pool) | reuse + variant: warn-text grace step before the leave-zone fail (two-trigger arming) |
| G6 buddy retaliation | `brawl_pair_spawn` (pool, partial) | **buddy_chain_retaliation** (hit one spectator → its named buddy (and the buddy's buddy) also attacks) — could fold into `crowd_ring`; flag the chain-table difference |
| G7/G8 reward | `mission_critical_npc`, `social_reward_kiss` (pool) | reuse |
| pass | `set_faction_respect` (delta form), `special_entity_set_load` (pool) | none new (`respect_delta` already pooled) |
| timer extend | `timer_restart` (pool) | reuse + variant: **add remaining + bonus** rather than reset |

## 6. Open questions

- `L_TagLoad`/`L_TagExec`/`L_TagIsFaction`/`L_ClearTag`/`TaggingStartPersistentTag` internals (LibTagging) — what `tagType` 5 means and how "faction" tags differ visually.
- Why the mission suppresses 14 unique ped models but only spawns ~11 of them (15/20 unused) — cut content?
- `F_VandalismTutorial` returns 1 while `bVandalizingTutorial` is set, but nothing in-file calls it — presumably LibTagging queries it as a Lua callback (**INFERRED**); verify if reimplementing tagging.
- `gMissionStage = "failed"` via `bMandySlapped` in `T_MandyMonitor` has a 4 s flee delay but no distinct fail key — falls through to generic `MissionFail()`.
- `BleacherPervAttack`, `F_OnClean`, `F_SetPlayerLoc`, `tblDress/tblPerfume/...` self-assignments and `local tblX = tblX` shadows — **dead code / decompile artifacts** (VERIFIED inert); `PedModelCreateGiftRequirement` for model 93 gated on 4_G4 (`main.lua:533`) — model 93's identity unknown.
- `AreaEnsureSpecialEntitiesAreCreatedWithOverride("4_G4", 4)` on pass — which world props the `"4_G4"` set swaps in (residual posters?).
