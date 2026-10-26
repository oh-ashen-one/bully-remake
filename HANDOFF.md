# HANDOFF — bully-remake (phase2: game models for rides + weapons, riding/throw anims, FX + SFX, town life, door links)

Date: 2026-10-20 · Branch: `phase2` = `main` (**merged to main 2026-10-20 on the owner's go**, fast-forward) · Host: Studio (`ssh studio`; checkout
`~/bully-remake` on `phase2`) · Laptop checkout `~/bully-remake` also on `phase2` (edit/commit/push there, pull on the Studio).
Status: **built + headless-verified + rendered stills reviewed; owner feel-playtest pending.**

---

## story-mode push (2026-10-24–25, integration branch `story-mode`) — paused, Claude weekly limit

Goal from Hari: launch the remake and play story mode for the first ~20 minutes of Chapter 1 (arrival → day_one →
1_02B → 1_02C → 1_03 → 1_04). Chapters 1–2 should be genuinely playable, and the rest of Kimi K3's work goes in as-is.
Shared agent brief (goal, ownership, build/test recipes, GPU safety, public-repo content rule): `docs/story-mode-brief.md`.

| Branch (worktree `~/bully-remake-wt/<branch>`) | State |
| --- | --- |
| `story-mode` | Integration branch off `kimi-backend` 44b8930. `sm-playloop` merged, plus coordinator fixes (town life on in `tools/play_story.sh`; fists-only start, weapons through campaign grants). Story smoke re-PASSed after those fixes (per the Claude session). |
| `sm-playloop` | DONE + merged: `-BRStory` title screen, USaveGame autosave/continue, Save & Quit, free-roam guidance, launcher. Headless only, no attended playtest. |
| `sm-humandrive` | **r2 done (Devin, 2026-10-25), not merged.** Real-input playthrough driver (move/look/punch/aim/fire/interact only, no teleports) PASSes arrival → 1_04; see "sm-humandrive r2" below. |
| `sm-visual` | **WIP, unverified, may not compile.** Black-frame fixes, Ch1 interior lighting, scripted-camera solver (`MissionSubsystemVisual.cpp`, `BullySceneFixups.*`, `tools/ue/shot_metrics.py`). Its upstream used to point at `origin/kimi-backend`; it now tracks `origin/sm-visual`. |

| `sm-humandrive` | **WIP, unverified.** Real-input playthrough driver (move/punch/press only) for Ch1–2. Agent stopped while adding a path-probe flag. Tip commit `WIP sm-humandrive` saved by Devin. |
| `sm-visual` | **Round 2 (Devin, 2026-10-25): built, `origin/story-mode` merged in, rendered; final-binary smokes arrival/day_one/1_02B PASS, 1_02C/1_03/1_04/campaign/story pending (a system dialog on the Studio blocked launches at 16:50).** Black-frame fixes (`BullySceneFixups`: race-track scenes hidden, indoor characters on lighting channel 1), Ch1 interior lighting (`tools/ue/lighting_variants.json`; **re-run `tools/ue/run_lighting.sh` in any checkout after merging** — the rig lives in the git-ignored map), runtime speech-camera framing (`MissionSubsystemVisual.cpp`: camera `frame` + `at_cm` original NIS spot, fade `place`). |
| `sm-cutscenes` | **WIP, unverified.** Original cutscene audio/subtitles imported at build time; opening cutscene playback in progress (was rendering 1-1-1 frames). |
| `sm-models` (Devin, 2026-10-25) | Original Ch1 cast instead of stand-ins: Gary, Russell, Algie, Eunice, Karl, Edward, Peter, Coach Burton, Mrs. McRae (arrival greeter + day_one office), the 1_04 football jocks, the 1_03 wave bullies, a day_one crowd student. Table `Config/PedModels.json` / `docs/re-notes/ch1/cast-models.md`; import `tools/ue/run_import_npc.sh --cast` (one engine run); missing SK_ falls back to the old stand-in. Also fixes `ped.py` skin placement (Edward/Karl sank 1 m, Russell 8 m off) and ped bone-translation retargeting. Headless-verified: the six mission smokes, campaign 5/5, story 5/5 + 3/3, and 1_04 with two models hidden (stand-ins load). No rendered check yet (two renderers were busy). Gotcha: the smoke wrappers' `--seconds-limit` includes GPU-slot queue time, so pass `--seconds-limit 2400` when the slot is contended. |

sm-visual round 2 detail. Stills: `tools/ue/run_mission_shots.sh <id>` then `tools/ue/shot_metrics.py <dir>` (mean luma < 20 =
DARK; `shots_meta.jsonl` sidecar lists who is on screen). Ch1 opening (arrival, day_one, 1_02B, 1_02C, 1_03, 1_04): **16/64
dark before → 0/64 after**. Interiors (luma, before → after): main halls ~60 → 75-95, principal's office 31 → 58, dorm hall
47 → 65-69; fixed probes `tools/ue/shots_ch1_interiors.json` (via `tools/ue/run_shots.sh`) also cover the chemistry room
and Constantine's bathroom. Frame cameras never sit in/against a character (capsule clearance, body occlusion, foreground
wall share), prefer the speaker's face, fall back to a single on the speaker, then to no cut; a frame camera issued during
a teleporting fade solves after the cast lands. Known gaps: `TO_Cop` is imported ~1.5 m under its root (bounds centre
z -49 cm vs ~+100 for other peds) so every TO_Cop npc renders buried — 1_04's Burton now uses `TO_Construct01`; 1_11x2,
2_S02 and 3_06 still use TO_Cop (fix the importer). Floating chrome/wire props in the school halls and bathroom are
importer artifacts, not lighting. Evidence: `~/bully-remake-evidence/story-mode/sm-visual/r2/`.

Not done: `sm-humandrive` and `sm-cutscenes` have not been built or smoke-tested since the agents stopped; `sm-visual`
was finished in round 2 (above). None of them is merged into `story-mode`. `story-mode` is not merged to `main`. Hari has not done a feel playtest.
Next: in each WIP worktree, build, run its smokes per the brief, finish or trim it, then merge into `story-mode`. Then run
the regression bar (day_one, arrival, 1_02B, 1_03, campaign and story smokes) and get Hari's playtest before `main`.
Evidence: `~/bully-remake-evidence/story-mode/` (not committed).

### sm-campus (Devin, 2026-10-25): campus life at Bullworth Academy

`UBullyCampusSubsystem` (`BullyCampusSubsystem.*`, config `Config/Campus.json`, notes `docs/re-notes/campus-life.md`)
on top of the ambient walkers, the clock and campaign respect. Import once per checkout (git-ignored output):
`python3 -m tools.importers campus --transcribe` (cast voices from default.ide, ~2.5k ambient speech takes, the
SB_Int bell, the campus trigger files) and `tools/ue/run_import_npc.sh --peds PRH1_Gord,PRH1a_Tad,PRGirl_Pinky,GRH1_Lefty,GRH1a_Vance,GRGirl_Lola,JKH1a_Kirby,JKH2_Dan,JKGirl_Mandy,NDH2_Thad,NDGirl_Beatrice,PF2nd_Max,GN_SexyGirl,GN_WhiteBoy`.
- Schedule: classes 9:00-11:30 / 13:00-15:30 (MainMap.lua / SchoolHallways.lua spawner and docker times), students
  head in at 8:45 / 12:45 (run after the bell), stream out after; bell at start and end; truancy (after 1_02C) two
  game minutes after the bell; F_AttendedClass / F_MissedClass / skipping punishment; curfew 23-7. The day is held
  at MORNING until 1_02C (its 9:00 set is the first bell), because our boot chain does not pause the clock.
  Attending a class = standing at a classroom door in class time (fade, clock to the period end).
- Trouble meter (0-400) replaces the 1_02B "punishment-lite" stub: MissionSubsystem `heat_set` / `hide` forward to it
  (same contract). Offences: truancy, curfew, hitting students / prefects, trespass (principal's office). Prefects
  (pedstats cone 100 deg / 12 m) warn, chase (5.6 m/s), bust; bust outcome per main.lua F_PlayerBusted (sent to class,
  principal's office with the PriOff lecture lines, or released). HUD: clock/period line + trouble bar, bottom left.
- Factions: SFaction.lua attitude rows shifted by campaign respect (1_01's starting respect is seeded when day_one
  passes on a fresh game; coordinator may prefer moving that into Campaign.json). Greet / taunt / bump / picked
  fights / faction mates joining / non-fighters fleeing; ambient barks and two-student conversations by context code.
- Cast: 28 walkers (front lawn loops + 5 hangout cliques + 2 prefects; halls 6 students + 2 prefects), dormant beyond
  110 m. Missions: rules stand down, walkers become bystanders (pass-through, untargetable), walkers whose model the
  mission names are hidden. Harness runs keep campus life off unless `-BRCampusLife` (story play turns it on).
- Smoke: `tools/ue/run_campus_smoke.sh` (`-BRCampusSmoke`). Gaps: no detention chores, no class minigames (sm-minigames),
  no teachers, hall truants, cops or tattling; per-offence points / chase line / drain are ours; walkers walk straight
  lines (a hand-placed lane round the front terrace), no navmesh.

### sm-humandrive r2 (Devin, 2026-10-25)

`origin/story-mode` merged in cleanly. Human drive: `tools/ue/run_mission_smoke.sh <id> --human --seconds-limit 1500`.

**1_04 `snipe` root cause (a game bug: a real player hit the same wall).** Every slingshot pellet fired from the tree
stopped 4 m out on `DL_SC1d_N`. That mesh is the football field's floodlight shine: one additive light-card mesh,
349 x 93 m. The importer makes every render triangle collision, so the shine was solid. The 30 pellets ran out, the
player had to leave the perch, and the 15 s tree timer failed the stage. The stage retry then restarted with an empty
slingshot. The 7.4 m "stuck" in the old report was the fists fallback after that, not a nav problem. Offline proof is a
ray-vs-imported-triangles test (`r2/raycast.py`). It also found an `iMGRaceC` race-track wall on the same line of fire.
Fixes:
- `BullyCollisionFixups.{h,cpp}` (new world subsystem). At begin play it switches collision off on all-additive light
  cards (glow_beam/lightbloom/sunbeams/halo/corona textures, 53 meshes). It also hides the race-track scenes
  (iMGRace*) and switches their collision off, which used to be done by the human harness only. Additive
  grass/water/court-line decals keep collision because they are the floor. Mixed meshes keep collision. Switch it off
  with `-BRNoCollisionFixups`. sm-visual's `BullySceneFixups` duplicates the race-track half (both idempotent); drop
  that half here when sm-visual lands.
- Weapon free aim (`AimPoint`): the crosshair ray now also stops on pawns. Capsules ignore Visibility, so long free-aimed
  shots went past the target.
- A stage retry restores the ammo the player had when the stage began (`StageStartAmmo`).
- `spawn_wave` `hits_ranged_only`: 1_04 jocks have the original's 22 hp. One pellet tags a jock. Punching one down
  takes a real beating.
- `origin/story-mode` merged again after sm-cutscenes landed (one conflict in `RetryNow`; both sides kept). The human
  driver now watches the original cutscenes (input is ignored meanwhile) instead of failing "STUCK idle" after 60 s.
  Enter skips them with `--skip-speech`. To test that path headlessly, the importer
  (`python3 -m tools.importers cutscene build`) wrote this worktree's git-ignored `imported/cutscenes`.
- The regular smoke driver presses interact every 0.4 s instead of every frame. The 1_02B `hide_bin` stage (interact,
  then wait for heat to decay) used to take 5+ min of wall time, and runs that queued for a GPU slot hit the wrapper's
  limit. It now takes 8 s.

**Known gaps a player of the original would notice (Ch1 opening slice):**
- All: stand-in meshes and voices (Burton is TO_Cop, the jocks are GN_Bully03 and so on). Baked cutscenes play from
  the owner's install when imported (sm-cutscenes: audio tier = camera + subtitles + voice; full tier also needs the
  actor import). Otherwise they fall back to title cards plus speech beats. Wording is original, not the game's lines. Retries restart the stage, not the mission (the
  original has no checkpoints). Some light effects with a solid part (a neon tube plus its beam) still collide.
- arrival: the original 1-1-1 cutscene plays first when it has been imported. The in-engine beats after it still use
  a faculty stand-in instead of the real characters.
- day_one: the 1_01 and 1_02 recon simplifications apply. The dorm door is a fade-teleport, not a door transition.
  The tutorial fight is guided by events.
- 1_02B: the lockpick is a timed hold bar, not the dial. Heat is "punishment-lite". pair_anim is a generic clip. The
  bust scene is a speech beat. Chocolates can only be paid for (no humiliate or beat-up route). The 1-02D cafeteria
  cutscene is a card.
- 1_02C: no bell audio. The C_Chem_1 class does not run after the door. The loot system is only a flag.
- 1_03: the school chase uses a player-walkable route, not the original flee path (that path drops off a parapet).
  The gate, garage-door and barrel-ladder choreography is skipped. Davis comes down and fights. The slingshot pickup
  anim is a stub.
- 1_04: **no tree climb**. The perch is a trigger at the tree base (the original is about 4.8 m up, with a climb
  anim). The bus windows are five stationary one-hit dummies near the lot, not the bus glass. Melee also breaks them,
  as in the original's wrong-weapon credit. The jocks stand still: no run paths, no jumping-jack cycle, no "yelled
  back to work" reaction. Burton is a stationary cone spotter (15 m, 60°) with no patrol path and no "busted" camera
  scene. No interactive music, no Gary pointing anim, and no radar-zoom clamp.

**Verified (headless `-nullrhi`, evidence `~/bully-remake-evidence/story-mode/sm-humandrive/r2/`):**
- `pre_merge2/batch.txt` was run before the second story-mode merge, with every 1_04 fix in. Human drive PASS 6/6:
  arrival 6/6, day_one 26/26, 1_02B 14/14, 1_02C 3/3, 1_03 7/7 (the bot is KO'd twice by the yard crew, then passes),
  and 1_04 8/8 (fails 0, 6 shots for 6 tags). Regular smokes: arrival, day_one and 1_02C PASS.
- After the merge: `batch.txt` has human arrival PASS 7/7 (it watches the original 1-1-1 cutscene).
  `check_smoke_1_02B` is a regular smoke PASS 15/15.
- **Not yet re-run after the merge:** human day_one through 1_04, the regular smokes, the campaign smoke and the
  story smoke. A macOS system dialog that is not ours (UserNotificationCenter, up since 16:50 local) blocks every
  launch through the wrappers' preflight. `r2/batch2.sh` waits up to 3 h for it to clear, then runs the rest into
  `r2/batch.txt`.

---

## kimi-backend (2026-10-22–24, branch `kimi-backend`): campaign spine → Chapters 1–2 DONE, Chapter 3 through wave E

The full story-mode campaign (design: `docs/campaign-spine.md`; goal: all six chapters per
`docs/story-mode-catalog.md`). All headless-verified on the Studio.

- **Wave 1 (spine)**: `UClockSubsystem` (game clock, pause/unpause/set, midnight-wrapping windows, hour delegate),
  `UBullyCampaignSubsystem` (Config/Campaign.json: 19-row Chapter 1 table from the recon docs — prereq chains, windows,
  SMissPass-style on_pass: give_weapon/give_ammo with numeric-id mapping, respect set-values, force_complete, chapter_set,
  money cents→wallet, clock), `AMissionStar` + `UMissionStarSubsystem` (world star markers, polled containment, harness-suppressed
  except its own `-BRCampaignSmoke`), MissionSubsystem glue (new actions + availability gate with dev override). Verified:
  `tools/ue/run_campaign_smoke.sh` **PASS 4/4**; day_one/side_errand/arrival regressions PASS.
- **Wave 2 (1_02B + 1_02C full missions)**: new framework primitives (all in MissionSubsystem, schema documented in
  MissionSubsystem.h): `spawn_npc` (friendly `ABullyScriptedNPC` at a point; id-reuse on retry), `npc_move_to`/`npc_lead`
  (arrive events; lead pauses when the player trails, snaps after 3 s of no progress), `npc_face`/`npc_despawn`, `pair_anim`
  (stub), `speech_sequence` (multi-line, Enter/pad-B skippable), `minigame_launch` (v1 = timed hold bar, not the dial),
  `give_item`/`take_item` + `money_ensure`/`money_take` (wallet + session item inventory in BullyProgressSubsystem),
  `heat_set`/`hide` (punishment-lite: decays while hidden → `heat:zero`), conditions `has_item` / `attempt` (retry-aware,
  gates 1_02C's tutorial cards) / `leave_area` (area bbox from the trigger file) / `social` (interact + money gate) /
  timer `"visible"` (HUD countdown) + stage `fail_text` + mission-root `fail`. Trigger-file loader reads the
  `imported/triggers/*.json` "entities" layout. Smoke driver: `:`-namespaced system events are never auto-fired (it skips
  speech beats, hold-drives minigames instead), `smoke_fail` found recursively, drive-current mode + chain continuation
  (`DriveSmoke("", false)`).
- **Missions**: `Config/Missions/1_02B.json` (14 stages: Gary lead-in, Russell apology+gift ($2 wallet gate), lockpick,
  prefect bust + bin hide, Eunice/chocolates ($1), cafeteria outro, chains to 1_02C), `1_02C.json` (3 stages: clock to
  9:00/paused, attempt-gated tutorial cards, bell + 57 s visible countdown, interact at the Chemistry door; pass unpauses
  the clock + sets the world-loot flag; C_Chem_1 class launch logged as out of scope). All wording original; points from
  `imported/triggers/1_02B.json`/`1_02C.json`; `ped_model_id` records the original model ids next to the stand-in meshes.
- **Verified** (Studio): `run_mission_smoke.sh 1_02B` **PASS 14/14** (incl. fail/retry), `1_02C` **PASS 3/3**,
  `run_campaign_smoke.sh` **PASS 4/4**, regressions day_one 26/26, side_errand 5/5, arrival 6/6 — all PASS, no windows.
- **Wave B (1_03 + 1_04 full missions)**: new primitives (MissionSubsystem): `spawn_wave` (hostile combat pawns at
  named points; `hostile:false` = stationary target dummies; `hits_to_beat` N = scripted hit-count health on the boss —
  real per-hit damage override in `UBullyCombatComponent::ReceiveHit`, so exactly N landed hits drop him; fires
  `wave:<group>` when `advance_at` members are down), `spotter` (cone + range + line-of-sight on a scripted npc →
  `spotted_by:<id>`), `leash` condition (chase fail: npc farther than R from the player for T seconds), `zone_timer`
  condition (exit-countdown that cancels on re-entry; the 1_04 tree perch), trigger-file **paths** (Trigger.img waypoint
  routes) + `via`/`path` routing and a `gait` string on npc_move_to/npc_lead, smoke-driver chase-follow (follows a leash
  npc instead of teleporting ahead) and wave drive (hits members through the real ApplyWeaponHit pipeline). Combat pawn
  gained `bStationary` (AI gate) + `HitsToBeat`.
- **Missions**: `1_03.json` (7 stages: intro, school chase on the original's SCHOOLFLEE/FleeToFountain paths, brag fight,
  garage chase, yard wave, Davis boss (3 hits), outro; pass grants via campaign on_pass), `1_04.json` (8 stages: slingshot
  hand-out, follow Gary, parking-lot beat, 3-of-5 window targets (stand-in dummies — the real breakables aren't drivable
  world props yet), to the field, tree perch, snipe under Burton's spotter + zone_timer, outro). Verified:
  `run_mission_smoke.sh 1_03` **PASS 7/7**, `1_04` **PASS 8/8** (both incl. fail/retry), campaign smoke **PASS 4/4**,
  day_one/side_errand/arrival/1_02B/1_02C all re-PASS.
- **Wave C (1_05 + 1_06_01 full missions)**: new primitives (MissionSubsystem): `npc_follow` (mission-scoped escort
  follow), `countdown_start`/`countdown_cancel` (mission-scoped visible countdown firing an event at 0; `resume` keeps
  remaining time across stage re-arms; re-arming un-latches its expiry event so retries are safe), `lesson_start`
  (fight-lesson framework v1: perform the named move N times within re-opening attempt windows; player moves report as
  `player_move:<Move>` mission events from `UBullyCombatComponent::StartMove`; HUD progress; fires `lesson:<id>_done`;
  window expiry re-prompts — the original's per-button sequence widget is a later wave). Smoke driver performs lesson
  moves through real `StartMove`.
- **Missions**: `1_05.json` (10 stages: intro, two courtyard packs, escort inside under the bladder countdown, bathroom
  fight, upstairs, ambush, privacy wait, locker walk, payoff; $5 + nerds/bullies respect via campaign on_pass; Algie's
  KO fail is latent until combat targeting supports NPC victims) and `1_06_01.json` (7 stages: intro, follow Gary & Pete,
  hobo meet, radio fetch (ground-level stand-in — no ladder traversal), hand-over, the jab lesson (clock paused), outro;
  transistors collectible via on_pass). Verified: `run_mission_smoke.sh 1_05` **PASS 10/10**, `1_06_01` **PASS 7/7**
  (both incl. fail/retry), campaign **PASS 4/4**, all six prior mission smokes re-PASS.
- **Wave D (1_07 + 1_08 full missions)**: **combat retargeting to NPC victims** (closes the 1_05 debt): `spawn_defender`
  (a combat pawn on the PLAYER's team — enemy swings can land on him, the player's can't; stationary, never attacks;
  tagged `br_npc:<id>` so his KO fires `npc_ko:<id>` via a tag hook in ReceiveHit; scripted drain pressure;
  stage-retry revive), `npc_drain`, spawn_wave `target` (every other member takes the defender as PreferredVictim —
  the pack splits like the original), `dialogue_choice` (HUD option list, J/K/L or D-pad, `requires_item` gates;
  the smoke driver picks the first option whose gate passes). Combat component gained `SetMaxHealth`/`Revive`; the AI
  honors `PreferredVictim`. Bugs the harness caught: wave spawns in a previous stage's on_complete are wiped by the
  stage-scoped cleanup (spawns belong to the stage that uses them — 1_07 fixed), and actor_tag interact targets were
  gathered before the stage's own on_start spawns ran (`RefreshStageActors` now re-collects on spawn). Missions:
  `1_07.json` (7 stages: drain pressure, three targeted waves, gate rendezvous, hobo + give-back outro; pass = 437
  (logged ride), force-complete 1_E01, respect bullies 0/nerds 65 via campaign on_pass) and `1_08.json` (9 stages:
  Gary's plan, Fatty meet, cafeteria chocolate run under the cook's spotter, pay-or-bully dialogue choice, gym locker
  rig under the prefect's spotter, Mandy payoff, deliver the notes to Beatrice; pass = 309 x5 top-up via on_pass).
  Verified: `run_mission_smoke.sh 1_07` **PASS 7/7**, `1_08` **PASS 9/9** (both incl. fail/retry), campaign **PASS 4/4**,
  all eight prior smokes re-PASS.
- **Wave E (1_09 + 1_10 full missions)** — pure data on Waves A–D primitives (deduped; no new C++):
  `1_09.json` (6 stages: the speech plays as latched speech_sequence parts alongside PreferredVictim waves on Earnest
  (a mission defender, KO = fail); clock sets 18:45 + pauses on auditorium entry like the original's A2; pass = 306 →
  slingshot, nerds 70 / jocks 45, clock paused into Halloween via campaign on_pass) and `1_10.json` (6 stages: night
  lead + parking ambush, basement fade-hop, switch beat (minigame), the dark walk to the Hole, audience staging for
  1_B; clock untouched — the night is time-stopped from 1_09, matching the original). Compressed per the recon gap
  tables: the 26-entry attacker schedule → 3 waves; the five puzzle rooms → one switch beat; crowd/jeers/speaker-breaks
  skipped. Verified: `run_mission_smoke.sh 1_09` **PASS 6/6**, `1_10` **PASS 6/6** (both incl. fail/retry), campaign
  **PASS 4/4**, all ten prior smokes re-PASS.
- **Wave F (the finale cluster: 1_11x1, 1_11xp, 1_11x2, 1_11_Dummy, 1_B, Chapt1Trans)**: `boss_bar` (the named
  one-member wave's health on the HUD progress bar + an under-half bark — the original's 50% anger flag as a readout);
  spawn_wave now applies `health` via SetMaxHealth (boss pools); a player-KO fail revives the player on retry
  (RetryNow). The costume force is a mission flag (no clothing system); the prank unique-victim counter maps onto
  spawn_wave dummies + advance_at (5 of 6 playable pranks, matching the original's HUD counter); the crowd ring is
  spawned spectators; 1_B chains into Chapt1Trans (new campaign row: chapterless, prereq 1_B, no star); 1_11_Dummy is
  a one-beat marker mission (the night-over flag). Verified: all six smoke **PASS** (1_11x1 4/4, 1_11xp 7/7, 1_11x2 7/7
  incl. the alarm countdown, 1_11_Dummy 1/1, 1_B 3/3 incl. the boss bar + half bark, Chapt1Trans 1/1), campaign
  **PASS 4/4**, all prior smokes re-PASS. **Chapter 1 is complete**: every mission in the campaign table has a real,
  headless-verified mission file.
- **Wave G (the side missions: 1_E01, 1_G1, 1_S01, 2_S04)** — pure data on the existing primitives (zero new mission
  C++): `1_E01.json` (Petey's Errand, 3 stages: clock-paused parcel carry from the boys' dorm to Beatrice outside;
  no DAT exists, so the points are the recon's raw literals), `1_G1.json` (The Diary, 5 stages: dorm window break-in
  hop, prefect spotter, diary minigame, get out, deliver), `1_S01.json` (Hattrick vs. Galloway, 5 stages: three
  bottles under three spotters — monitor/Edna/Hattrick — trophy-case hold minigame, deliver to Ms. Phillips),
  `2_S04.json` (Character Sheets, 6 stages: Melvin walk-off to the library, four guarded sheet stashes as
  reach+wave stages, return-and-hand-over; the rochambeau duel / Thad ally / fleeing-smoker chase are compressed
  per the recon gap table — they need qte_duel/recruit_ally/flee_loop primitives that don't exist yet). All wording
  original. Verified: all four smoke **PASS** (1_E01 3/3, 1_G1 5/5, 1_S01 5/5, 2_S04 6/6 — each incl. the
  fail/retry exercise), campaign **PASS 4/4**, full regression (all 19 prior missions) re-PASS.
- **Finale: rendered stills for every Chapter 1 mission.** `tools/ue/run_mission_shots.sh` needed two harness fixes
  first (MissionSubsystem): shots mode now optionally runs the smoke driver alongside (`-BRMissionShotsDrive`, on by
  default in the script; `--no-drive` opts out) so interactive stages actually advance — undriven runs stalled at
  the first interactive stage or inflated the shot count via fail/retry re-shots; and a pending shot now flushes at
  stage end (a fast driver beat the 1.9 s settle delay and overwrote pending shots) + the run quits on mission
  completion instead of burning to the time limit. Result: every Ch1 mission captured N/N — 1_02B 14/14, 1_02C 3/3,
  1_03 7/7, 1_04 8/8, 1_05 10/10, 1_06_01 7/7, 1_07 7/7, 1_08 9/9, 1_09 6/6, 1_10 6/6, 1_11x1 4/4, 1_11xp 7/7,
  1_11x2 7/7, 1_11_Dummy 1/1, 1_B 3/3, Chapt1Trans 1/1, 1_E01 3/3, 1_G1 5/5, 1_S01 5/5, 2_S04 6/6, day_one 26/26,
  arrival 6/6 — in `~/bully-remake-evidence/missions/shots_<id>/` (1920×1080, evidence dir, not committed).
  Post-harness-change smokes re-verified (day_one 26/26, 1_02B 14/14, campaign 4/4). Known content gap the stills
  expose: interior beats (e.g. the 1_03 garage) render near-black — the imported map has no interior lighting;
  exterior framings are golden-hour clean. **Chapter 1 is DONE** — story spine, side missions, campaign table,
  headless smokes and rendered stills all green.
- **Ch2 Wave A (2_01, 2_02 + 2_02_Conv; owner lifted the Ch1-only scope — goal is now the full story mode, chapters
  1-6 per docs/story-mode-catalog.md)**: `2_01.json` (Last Minute Shopping, 6 stages: Edna's cafeteria brief, fetch
  the bike, three Vale store stops, deliver under a 325 s visible mission countdown — the root `fail` now carries a
  composite any-of: Edna KO or timer expiry), `2_02.json` (Comic Klepto, 4 stages: shop-front alarm, sprint chase
  along a compressed 2_02_NEWESCAPE (first half of the 123-node loop, cornered at waypoint 54), corner fight with a
  boss_bar + hits_to_beat 3, return the comic). The 2_02_Conv.lua glue is ported as two mission-blackboard flags per
  the recon's framework note (set_flag `2_02_got_comic` / `2_02_returned_comic`; the .act conv-tree consumer is out
  of scope). Zero new C++ — both missions are pure data on the waves A-G primitives (compressions documented in each
  `_note`: no vehicle bundle, so Edna's bike and the klepto's mounted flee are reach/foot-chase beats; no bus or
  interactive-music systems). `Config/Campaign.json` grew the full Chapter 2 table (16 rows: the 12 catalog
  missions incl. 2_B's chapter_set 3 + level-2 class force-completes + yearbook_darby, Chapt2Trans with the scripted
  ch3 faction table on its on_pass, the 2_02/2_S05 engine-vs-catalog placement contradictions flagged like
  1_S01/2_S04, the 2_R03/2_R04 town jobs and no-DAT 2_05/2_07 without stars until points are extractable). Verified:
  `run_mission_smoke.sh 2_01` **PASS 6/6**, `2_02` **PASS 4/4** (both incl. fail/retry), campaign **PASS 4/4**, full
  regression (all 23 Ch1 missions + side missions) re-PASS.
- **Ch2 Wave B (2_03, 2_S02)**: `2_03.json` (The Eggs, 6 stages: Tad's errand with the money_ensure 150 floor,
  buy eggs, social delivery, the ambush (trio despawned → hostile wave + six stationary window-egger dummies),
  the front-gate keymaster fight → gate key, getaway reach outside the back gate) and `2_S02.json` (the Hattrick
  house-vandalism mission — the name-correction at the top of its recon doc applies: 'Small Offences' canonically
  belongs to 2_S07; script id stays 2_S02; 7 stages: Galloway's brief at a forced 18:30 clock, find Hattrick, the
  tail home, arrival beat, the 10 s yard dash, the smash, the escape). Zero new C++ again — the user's two flagged
  candidates deduped to data: the 2_03 dress-code gate is skipped (no clothing system exists for `player_wearing`
  to read; the sweater grant is the campaign row's `outfit_sweater_r1` collectible flag, SMissPass VERIFIED) and
  the 2_S02 weighted destruction meter + vehicle tail became eight stand-in target dummies at the trigger file's
  own yard-area points (the ~140 trich_* breakables aren't drivable world props, same posture as 1_04) + a sprint
  foot-tail on the real `_2_S02_FromSchool` route with a 70 m leash (the car's governor tops out at 12 m/s, so a
  sprint is the honest stand-in). Both `_note`s carry the full compression lists. Lessons: trigger-file
  trigger-volumes are NOT named points (a `blip`/`reach` on one fails mission load — give it an inline point);
  `CondText` prints `fail: none` for leash/zone_timer (cosmetic, they work); leave-zone passes stay
  `reach_location` beats because the smoke driver has no trigger_exit drive (2_03 getaway, 2_S02 escape). Verified:
  `2_03` **PASS 6/6**, `2_S02` **PASS 7/7** (both incl. fail/retry), campaign **PASS 4/4**, full regression (all
  23 Ch1 missions + 2_01 + 2_02) re-PASS.
- **Ch2 Wave C (2_04, 2_06 + 2_06supplemental)**: the first real race, and the social-errand chain. New primitive
  (the wave's one C++ addition): **`race_start`** (MissionSubsystem) — a minimal data-driven race: the course is
  any trigger-file path (2_04 uses the real 72-waypoint `2_04_RACE` loop), the player passes its waypoints in
  order (checkpoint radius) x `laps`, fires `race:<id>_win`; rivals are plain data (spawn_npc + npc_move_to on the
  same course), first rival home = `npc_arrived:<id>` = the stage's lose condition. Stage-scoped (retry re-arms);
  built for reuse by the 3_R08 14-race series. Smoke driver: race drive teleports checkpoint-to-checkpoint and sits
  ABOVE the 1.5 s drive-cadence gate (lesson learned: below it, a 72-checkpoint course needs ~110 s of drives and
  the 150 s driver budget dies at checkpoint 54). `2_04.json` (5 stages: Petey's intro, ride-to-the-beach on
  `_2_04_PetePath`, lineup, the race, podium) — `retry:"stage"` lands the original's saved-data race checkpoint
  for free; bikes/mount gate/rubber-band/barrier dressing are documented compressions. `2_06.json` (Movie Tickets,
  12 stages: pacifist chain — steal Gord's bike (reach beat), lure him to Lola's scene, the Yum Yum chocolate run
  (money_ensure 100 floor; the broke watchdog is moot with the floor, so no money_below condition was added),
  Eunice's escort/kiss, the Petey scare, the Pinky finale). The 2_06supplemental variant-launcher collapses to the
  single campaign star (g206Subquest handshake OPEN in the recon; `mission_variant_select` recorded as pending).
  Verified: `2_04` **PASS 5/5** (t=73 s — Petey's ride dominates, the race itself is seconds), `2_06` **PASS
  12/12** (both incl. fail/retry), campaign **PASS 4/4**, full regression (23 Ch1 + 2_01/2_02/2_03/2_S02) re-PASS.
- **Ch2 Wave D (2_08, 2_G2)**: two framework additions (small, in MissionSubsystem): **mission_start_item_gate**
  (root `requires_item` + `requires_consume` — StartMission soft-refuses with a HUD hint when the player lacks the
  item, matching the original's no-retry posture; harnesses/dev bypass via the same bDevOverride as the
  availability gate; flowers have no store source yet, noted) and **clock_fail_at** (`"time": "h:mm"` — a
  mission-long edge on the world clock, circular hours, fires `clock:fail` once; the root fail listens). `2_08.json`
  (Weed Killer, 11 stages: biology-room brief with the $12 money_ensure floor, two disguise purchases (the
  outfit-predicate checklist is clothing-system-pending), greet-Bif social gate (friendly path only), three floor
  fights, the pesticide grab, the flytrap as a boss_bar target dummy, the exit wave (ambusher + Bif rematch), the
  escape; the 2_08↔PrepHouse.lua shared-state split is documented as compressed away — our missions are
  self-contained). `2_G2.json` (Carnival Date, 8 stages: meet Pinky under the 240 s meet-timer + the 01:10
  clock_fail_at, arrival beat, the four booths as fixed-order timed-hold minigames paying 3/3/2/2 tickets (the real
  carnival scripts don't exist — the minigame_event_bridge is stubbed by npc_follow + per-booth barks, documented),
  the bear purchase, the give/kiss pair_anim pair). Also fixed a wave B/C units bug: mission money_* actions are
  whole dollars — 2_03's egg money was 150 dollars, now $2 (the original's 150 CENTS); 2_06's floor/take now $1.
  Verified: `2_08` **PASS 11/11**, `2_G2` **PASS 8/8** (both incl. fail/retry), campaign **PASS 4/4**, full
  regression (all 27 prior missions) re-PASS.
- **Ch2 Wave E (2_05, 2_07)** — the two no-DAT missions, pure data on the existing vocabulary (zero new C++).
  Neither `2_05_new.DAT` nor `2_07_BEACHA.DAT` was ever extracted, so both missions **borrow a neighbor's trigger
  file** (documented in each `_note`): 2_05 uses 2_03's (Tad's property is literally shared — same gate props per
  the recon) and 2_07 uses 2_04's (the beach/race geography; the chase runs RestartPlayer → FinishLine →
  BeachCheerChange, the low-z sand standing in for the dock). `2_05.json` (6 stages: Russell's egg brief, the
  on-foot npc_lead to Tad's gate (his bike escort is vehicle-bundle-pending), into the yard, six window stand-in
  dummies at the 2_03 file's own cover points under a patrol spotter + a real zone_timer stay-in-yard fail, the
  final three-prep wave, the escape; Tad's shutter work-queue and the egg-economy refills documented as skipped).
  `2_07.json` (8 stages: Ricky's rally, the Gord chase (npc_move_to + npc_follow), the siege taunt with
  blocker/cheerer color, three fixed two-prep waves compressing the continuous spawner (barks between waves), the
  Gord boss fight (hits_to_beat 3 + boss_bar, Parker/Bryce convert as a second wave), trophy hand-off, outro; no
  npc_ko fails anywhere — matching the original's single out-of-area fail, itself skipped pending trigger
  geometry). Verified: `2_05` **PASS 6/6**, `2_07` **PASS 8/8** (both incl. fail/retry), campaign **PASS 4/4**,
  full regression (all 29 prior missions) re-PASS.
- **Ch2 Wave F (2_S06, 2_S05 + 2_S05B)**: `2_S06.json` (Panty Raid, 10 stages: Burton's brief, the lattice
  reach + fade hop, five collect stages, the alarm beat, the Eunice front-door wave, the delivery). The girls'-dorm
  interior (area 35, `2_S06b.DAT`) was never extracted — the collect plays in the area-23 pocket already used by
  1_G1/2_S05 with five invented panty points (marked). The signature **soft-fail eject loop** is spotter +
  stage-scoped retry: `spotted_by:librarian` fails the collect stage (fail_text is the bust read), `retry:"stage"`
  rebuilds it with collected items kept — never a mission fail, matching the original's posture (the marched-out
  walk / re-climb reset / ambient-headmistress shared hook are documented compressions). `2_S05` is the two-half
  script — **stage-variant decision: two mission JSONs sharing the trigger file + two campaign rows** (2_S05 =
  Cook's Crush, 2_S05B = Cook's Date; our mission ids are file stems and the catalog lists the halves separately;
  the original's external F_SetStage is OPEN in the recon). `2_S05.json` (6 stages: Edna's fetch brief, the staff
  -room perfume (fire-alarm evacuation skipped), Fatty's candy (bribe branch only), the three trash cans as target
  dummies with a FIXED can 2 holding the bottle (randomized_loot_prop pending), the hobo wave, the return) and
  `2_S05B.json` (8 stages: the 30 s vantage dash, the dinner beat, four two-member gawker waves compressing the
  annoyance-meter defense (annoyance_meter pending), the outro). Campaign.json split the old combined 2_S05 row.
  Verified: `2_S06` **PASS 10/10**, `2_S05` **PASS 6/6**, `2_S05B` **PASS 8/8** (all incl. fail/retry), campaign
  **PASS 4/4**, full regression (all 31 prior missions) re-PASS. **Chapter 2's scripted missions are done except
  2_09/2_B/Chapt2Trans** (boxing gateway + Darby boss + the transition) and the 2_R03/2_R04 town jobs.
- **Ch2 Waves G+H (the finale: 2_09, 2_B, Chapt2Trans + the town jobs 2_R03, 2_R04)**: `2_09.json` (Boxing
  Challenge, 4 stages: the snowy-approach reach (snow is weather-force-pending), the gym fade hop, the weigh-in,
  the Bif match as hits_to_beat 8 + boss_bar — the whole Boxing_util round/stun/get-up/decision system is the
  documented compression). `2_B.json` (Darby boss, 5 stages: the opening flee on the real 2_B_DARBY_FLEE path (the
  original skips it on restart; our retry:stage does too), the two posted prep pairs, the ring-intro exchange, then
  Darby (hits_to_beat 6 for 300 HP x0.2-taken) + one summoned door pair — win needs BOTH down, matching 'Darby HP
  <= 0 with no preps alive'; boss_rest_cycle / summon_wave_from_doors / door_barricade_prop /
  projectile_dodge_window / bottle throws are the documented stubs). `Chapt2Trans.json` (1 stage: fixed aerial of
  the construction vignette (camera-rail playback pending), CHAPTER 3 card, three narration lines, clock 08:30,
  the dorm handoff with a wandering Pete; Christmas entities + the baked 3-0 intro logged as out of scope).
  Chains wired like 1_B: 2_09 on_complete -> start_mission 2_B -> start_mission Chapt2Trans. The campaign smoke
  grew check 5 (`5_ch2_arc`: 2_B pass -> chapter 3 + level-2 force-completes + preppies 100; Chapt2Trans -> the
  ch3 faction table) — **PASS 5/5**. Wave H (verification held green, so the jobs rode along): `2_R03.json` (Paper
  Route: single-tier v1 in the tier-0 shape — 35 s hilltop dash + 7 mailbox dummies across 2_04's borrowed
  beach-head geography (no PaperRoute.DAT), one $14 pass reward; repeatable_job_tiers + per-paper accumulator +
  all hazards documented pending) and `2_R04.json` (Firefighter: single-area v1 — the briefing, the call, three
  fire dummies on 2_01's borrowed business block (no 2_R04.DAT); fire_prop_system + the cross-run area ladder
  (shared.g2SS07MissionState / the 2_S07 wrapper, OPEN) documented pending; no payout, matching the original
  script). Verified: 2_09 **PASS 4/4**, 2_B **PASS 5/5**, Chapt2Trans **PASS 1/1**, 2_R03/2_R04 **PASS 3/3**
  (fail/retry where present), campaign **PASS 5/5**, full regression (all 34 prior missions) re-PASS.
  **Chapter 2 is DONE** — every catalog Ch2 mission + the jobs have real, headless-verified mission files, and the
  campaign arc demonstrably reaches chapter 3.
- **Ch3 Wave A (the Ch3 table + the opener chain + 3_01)**: Campaign.json grew the full Chapter 3 table (21 rows,
  59 total; the table _note now says story-mode Ch1-3 and documents the two deliberate gaps — 3_03 is CUT content
  (no row, see its recon doc) and Glass House (catalog §3 row 37) has no recon'd script id yet). Rows per the recon
  docs + catalog: the SE Christmas arc (3_07 wake-up + 3_08 sweater scene + the 3_08_PostDummy marker, auto-chained
  at chapter start; 3_01A/3_XM/3_01C/3_01D as the four holiday side missions with their outfit grants), the main
  spine (3_01 Jealous Johnny -> 3_02 Bait -> 3_04 Wrong Part of Town -> 3_G3 Lola's Race -> 3_05 Tenements -> 3_06
  The Rumble -> 3_B boss), 3_S10 Tagging off 3_02, 3_R05B Discreet Deliveries with the VERIFIED prereq-3_01
  job-board wiring (the MainMap id confusion is flagged), 3_R08/3_R09 starless notes for later waves, and
  3_S03/3_S11 placed at their CATALOG chapters (5 and 4, per the wave instructions — prereq Chapt4Trans/Chapt3Trans
  so they're correctly unavailable early). 3_B's on_pass carries chapter_set 4 + level-3 force-completes + greasers
  100 + yearbook_johnny; Chapt3Trans's carries the ch4 faction table. Missions: `3_07.json` (2 stages, fail-free
  like the original — no smoke_fail), `3_08.json` (2 stages, fail-free; the sweater grant + PostDummy marker ride
  the campaign on_pass), `3_01.json` (6 stages: Johnny's brief, find the couple, three photo beats at three fixed
  alley spots, the hand-off). Chapt2Trans now chains into 3_07 (its dorm point is the real _CHAPT2TRANS, found in
  3_07.json's extract). PHOTO PRIMITIVES call (documented in 3_01's _note): the photo capture is our timed
  hold-interact with camera framing text — photo_mode/photo_capture_condition (aim mode + reticle + pose-predicate
  latch) are pending real camera work; the date loop compresses to three fixed spots (killing the original's
  finite-opportunity soft-lock); the spook meter + non-hostile spotted break are skipped (spotted_pause_resume
  pending — and the walkthrough's 'spotted fails' claim is VERIFIED wrong per the script). Zero new C++ this wave.
  Verified: 3_07 **PASS 2/2**, 3_08 **PASS 2/2** (attempts 1 — both fail-free by design), 3_01 **PASS 6/6**,
  campaign **PASS 5/5**, full regression (all 40 prior missions incl. waves G+H) re-PASS.
- **Ch3 Wave B (the SE Christmas arc: 3_01A, 3_01C, 3_01D, 3_XM)** — four missions, zero new C++ (all campaign
  rows already existed from wave A). `3_01A.json` (Balls of Snow, 4 stages: Rudy's pitch, the ladder reach + fade
  hop to the stand, the gallery as ten stationary dummies at the file's own spawn/bike points under the 180 s
  countdown + a real leash-on-Rudy abandon fail; fixed_gallery_mode/target_gallery_spawner/ped_hit_with_weapon and
  the heat-mirror bust are documented pending). `3_01C.json` (Nutcrackin', 3 stages: Peters' brief + fade into the
  area-19 stage, the pageant as ONE long timed-hold with the cast as color — minigame_rhythm (ClassMusic.DAT charts
  were never extracted) + the out-score-the-AI rule documented pending; outfit + $20 via the campaign row).
  `3_01D.json` (Rudy, 12 stages: five ornament reaches under the 185 s countdown, the two elf ambushes as real
  waves at the count thresholds, the return, the Santa's-lap photo session as two 3_01-style photo stubs
  compressing the five kids' pose-window tables — photo_pose_window/photo_session_lock pending; $50 via the row).
  `3_XM.json` (Miracle on Bullworth St., 8 stages: the 19:30 clock set is real (snow pending), the timed
  get-to-square, three wrecking sets of stand-in dummies at the square's own points + six marked invented ones (the
  weighted 100-pt meter stays pending — 2_S02 posture), an elf wave per set boundary compressing the escalation
  ladder, zone_timer stand-in for the cumulative area budget, the queue sim cut to two color NPCs; Elf outfit via
  the row). One self-inflicted bug caught pre-smoke: elf waves were spawned in on_complete — waves are
  stage-scoped, moved to the elf stages' on_start (the wave-A rule). Verified: 3_01A **PASS 4/4**, 3_01C **PASS
  3/3**, 3_01D **PASS 12/12**, 3_XM **PASS 8/8** (all incl. fail/retry), campaign **PASS 5/5**, full regression
  (all 40 prior missions) re-PASS.
- **Ch3 Wave C (3_02, 3_S10)** — zero new C++. `3_02.json` (Bait, 6 stages: the intro, taunt-Gord social (the
  insult/hit OR-gate is npc_insult_event/ped_hit_event work — he's untargetable so the hit branch is latent), the
  cross-town lure with npc_follow Gord + the plain 120 s countdown, the warehouse fade hop (3_02_BMX.json, area
  62), the reveal, and the brawl as three preps vs defender-Johnny + two greaser defenders (spawn_defender from
  wave D; Johnny's KO fails). Smoke caught a real deadlock: leash-on-a-follow-target (driver chase-follows the
  leash NPC while npc_follow trails the player — neither advances) — the leash was dropped as redundant with the
  follow; lesson recorded). The bike chase/brawl are foot versions (bike_brawl_mode pending); the mission_spawner
  harassers skipped; the clean/dirty outro variant always plays clean (Gord is unhittable in town — the check is
  ped_hit_event-pending); the $10 clean-bait bonus pays via in-mission reward; the BMX park unlock rides the
  campaign row (main.lua F_BMXGarage gating VERIFIED). `3_S10.json` (Tagging, 10 stages: money floor + spray
  grant, into New Coventry, the tutorial tag as a timed-hold, the market restock, five tag spots — tag_prop_system
  v1 IS the hold-interact per tag; the two real tut-adjacent tag volumes + three invented points beside the A/B/C
  load triggers are marked (the per-tag coordinates aren't in the extract), the greasers outro with the $30
  accumulated-cash compression; consumable watchdog stubbed by the start grant; free tagging rides the campaign
  row). Verified: 3_02 **PASS 6/6**, 3_S10 **PASS 10/10** (both incl. fail/retry), campaign **PASS 5/5**, full
  regression (all 44 prior missions) re-PASS.
- **Ch3 Wave D (3_04, 3_05)** — zero new C++. `3_04.json` (Wrong Part of Town, 9 stages: the tableau (spawned color;
  npc_grapple_hold_scene pending), the rescue as defender-Cornelius + the attacker wave (his zone-ally toggle is
  always-on, ally_zone_recruit_toggle pending), the meet, the plan, **the signature back-of-the-bike chase
  compressed to Chad sprinting the real 49-node 3_04_bikeEscape route 1 with the player leash-following on foot**
  (player_rides_npc_vehicle is the wave's big pending primitive — PlayerFixToBackOfVehicle + locked chase camera +
  forced slingshot; routes 02-04 and their ~1.5 km are cut), the hill ambush at the real hill points
  (mounted_chaser_spawner/chase_camera_cuts pending), the walk-home with defender-Algie + mission-critical
  Earnest/Cornelius greeters, the outro; weapon 307 via the campaign row). `3_05.json` (Tenements, 13 stages: the
  intro with the real clock pause, the window fade hop into area 36, two guard-group waves at the real
  ambush/catwalk volume centers, the five belongings as per-item stages at invented-from-volume-center points (the
  five-element LOLAPICKUPS pointlist collapsed in the extract — marked; laundry deliberately last though the
  original triggers Norton on it in ANY order, VERIFIED there), the Norton intro, **the no-KO boss loop landing as
  the wave-F player-KO stage retry** (revive + fresh boss wave; the fail banner briefly shows — boss_fight_retry_loop
  documented), the sledgehammer hand-off (weapon_equipped pending), the escape with the real item strip +
  bundle swap, the Lola gift/kiss return). Verified: 3_04 **PASS 9/9** (t=131 s — the chase is long but fits the
  budget), 3_05 **PASS 13/13** (both incl. fail/retry), campaign **PASS 5/5**, full regression (all 46 prior
  missions) re-PASS.
- **Ch3 Wave E (3_06, 3_G3)** — zero new C++ (`race_start` from Ch2 wave C carries the race). `3_06.json` (The
  Rumble, 6 stages — the recon's name correction applies: this script was first filed as 'Bait'; 3_02 is Bait):
  title-card + speech intro; the paired ambient brawls compress to invulnerable color pairs + one hostile
  converter wave per brawl (brawl_pair_spawn pending); Peanut's bike bolt is an on-foot sprint along the real
  21-node PeanutPath (npc_within_radius pending; the 300 s chase budget is a real mission-long root countdown);
  the 15 s out-of-the-poor-area re-entry countdown skipped (the chase route leaves that volume by design); the
  cop-bust vignette is two invulnerable color cops at the AmbientEvent1 point (ambient_vignette_zone + parked
  siren car pending); Vance + the third greaser asleep pre-staging compresses to the fight-stage spawn
  (npc_asleep_staging pending); the alley NIS is a letterbox speech beat (the bLeftSide dead branch is the
  original's own — VERIFIED); the leave-the-fight two-step skipped; the pass is BARE like the original (no
  banner/respect/money — VERIFIED); the 3_08_PostDummy force-complete already rides 3_08's on_pass; the
  'Christmas' entity load on pass is the recon's suspicious-reuse OPEN — skipped. `3_G3.json` (Lola's Race, 3
  stages): the one-lap race on the REAL 88-node `_3_G3_RACE` path (2576 m) through New Coventry's poor district
  + rail yards vs Johnny/Lucky/Ricky (scripted npc_move_to riders; first rival home = `npc_arrived` stage
  fail), Lola running the countdown, the winner's-kiss pair_anim. Compressions: everyone on foot (bikes
  pending — the race runs mounted or not); the intro camera flyover + per-racer trash talk is a letterbox
  speech beat (camera_rail_pair/racer_trash_talk pending); flat sprint, no rubber-banding; the booby-trapped
  trees + proximity train cars SKIPPED per the wave instructions (boobytrap_prop_pair/proximity_prop_mover
  pending); the water watchdog/stay-on-bike/too-far rules are bike-system rules — n/a on foot, documented;
  Lola's teleport-to-the-finish + the four cheerers compress to the win-scene cast (race_rail_persona
  pending); greaser respect −20 omitted (respect_delta pending); `3_G3_UNLOCK` data-side (OPEN); no SMissPass
  case (VERIFIED absence). Verified: `3_06` **PASS 6/6**, `3_G3` **PASS 3/3** (both incl. fail/retry — attempts
  2 each), campaign **PASS 5/5**, full regression (all 59 mission smokes) re-PASS.
- **Ch3 remaining**: the finale wave — `3_B` (Fighting Johnny Vincent boss; its campaign on_pass already
  carries chapter_set 4 + level-3 force-completes + greasers 100 + yearbook_johnny) → `Chapt3Trans` (the
  chapter-4 handoff + the ch4 faction table) — then the side rows `3_R05B` (Discreet Deliveries, prereq 3_01),
  `3_R08` (bike race series), `3_R09` (clique challenges). `3_S03`/`3_S11` sit at their CATALOG chapters (5 and
  4) by design; `3_03` is CUT content (no row) and Glass House (catalog §3 row 37) has no recon'd script id;
  `3_08_PostDummy` needs no file — 3_08's on_pass force-completes it.
- **Session note (2026-10-24)**: the wave-E mission JSONs were left untracked when the authoring session
  stalled mid-wave; committed as `f6d5c8d` after confirming both smokes + the post-wave regression in
  `~/bully-remake-evidence/missions/` (59/59 PASS). The `ai-npc` worktree (`~/bully-remake-wt/ai-npc`) is 4
  behind `origin/ai-npc` — pull before resuming NPCChat work there.
- **Debts**: lead NPCs corner-cut walls on stuck-snap (no navmesh; `via` waypoints not built); social/pair_anim/lockpick/heat
  are v1 stubs; no bell cue in Sfx.json; tutorial cards replay on a fresh launch (attempt count is per-launch + retry).
  Chocolate humiliate/beat-up routes (recon §S9) not built. Karl's bust chase is scripted, not real pursuit AI.

## What this session did (owner ask: "do the rest — pull the models from the game like we've been doing")

Owner overruled the previous session's "no game models" line: game assets are pulled from the owner's own install exactly like
the world/characters already were — converted locally into `imported/` + `Content/Imported/` (both git-ignored); only original
code and config paths go in git.

- **Rides use the game's models** — `python3 -m tools.importers vehicle <model>` (new, `tools/importers/props.py`): body with
  damage-state parts (`*_dam`) and untextured helper shapes pruned, the IDE `cars` row's wheel model recentred on its hub, and a rig
  JSON (wheel hubs, seat/door points, ground offset, bounds). `ABullyVehicle` (`Vehicles.json` `model`) places body + 4 spinning
  wheels (front pair steers). bike, skateboard (`SK8Board`), go-kart (`GoCart` + `wheel_van`), car (`taxicab`), plus new parked/drivable
  variants police (`Policecar`), sedan (game's `Limo` = sedan body), pickup (`Truck`).
- **Rider poses from the game** (`V_Bike`, `Go_Cart`, `Skateboard` groups; `Vehicles.json` `rider`): ride/fast/coast/idle/turn/air clips,
  pedal clip rate follows speed. Cars use the kart seat pose (sits inside).
- **Weapons**: held game models on `Root-R-Hand` (slingshot, spudg, BROCKETLAUNCHER, eggs, stink bomb, itching powder), game projectile
  models (BROCKET with smoke trail, EGGPROJ/stinkbomb/W_Itch tumbling, marble), throw/fire clips from `W_Thrown`/`W_Slingshot`/`W_BRocket`
  on the **upper body** (new overlay in `UBullyAnimInstance`, Root-Spine down) with the shot leaving on the clip's release frame; launchers
  carry a hold pose (`BRKT_HOLD_IDLE`). Locked-on shots re-aim at the target on the release frame.
- **FX** (`ABullyFx`, no particle assets): dust / chunks / blast (fireball + flash light + sparks + smoke) / bang / egg smear / green gas
  cloud / itch powder puff / real marbles scattering; material `/Game/Imported/Fx/M_BullyFx` (lit translucent + Glow) built by the import.
- **SFX from the game's banks** (`Config/Sfx.json`, `FBullySfx`): 25 events — weapon fire/impacts, punch light/heavy, block, body fall
  (melee now has impact sounds = backlog #3), bike pedal / skate roll / kart + car engine loops pitched by speed, skids, horn (H).
- **Town life** (`UBullyTownLifeSubsystem`, `Config/TownLife.json`): townspeople (10 imported `TO_*` peds) walk loops and rideable cars
  park at the kerb, streamed in within 260 m. Loops/parking come from the game's nav grid `Config/GRIDS.DAT` (`python3 -m tools.importers
  townlife`; the WALKABLE meshes turned out to be stubs). Good in business strip / Old Bullworth Vale / New Coventry; carnival has loops but
  no parking; junkyard / BMX / go-kart / tGlobal have synthetic rings only.
- **Door links from the game** (`python3 -m tools.importers exits` → `imported/door_links_game.json`): `Config/dat/ExitLocs.dat` +
  `areabbox.dat` → 32 door pairs (64 links; 12 flagged "facing uncertain"; 6 pairs rejected for stale coordinates). Placed with
  `tools/ue/run_place_doors.sh --generated` (hand-made main door stays authoritative). = backlog #4.
- **Mission robustness** (backlog #2): 7 skippable stages got `any` fallbacks, min reach radius, `tools/ue/audit_mission_skips.py`, and
  `run_mission_smoke.sh <id> --wrong-way` (overshoot + skip-ahead teleports). Table: `docs/re-notes/mission-robustness.md`.
- **Locomotion start/stop** transitions (PLAYERWALK_START / PLAYERRUN_START / PLAYERWALK_STOP / RUNSTOP0), cut short by input (#7 debt).
- **Studio now has** GN_Bully01–05 imported (fight/gauntlet peds are real here too) and the 10 townspeople.
- **Evidence harness** `tools/ue/run_showcase_shots.sh` (`-BRShowcaseShots`): offscreen render of every ride (3/4 follow cam + chase cam)
  and every weapon (held / release frame / impact). Runner fix: the timeout timer's `sleep` no longer holds the caller's pipe open.

## Owner playtest fixes (2026-10-20, later)

- **Rides fell through the map** (car + go-kart): the ground check was one trace under the centre and falls moved
  without collision. Now: ground probes under the whole footprint (centre + 4 corners, from above the box), swept falls,
  and a rescue back to the last solid-ground spot if a ride ever drops under the map (`BR_VEHICLE ... fell under the map`).
  New test `run_smoke_nullrhi.sh --flag -BRSandbox --flag -BRRideStress` drives car + kart 2.4 km over the game's real
  streets (both ways from every parking spot): **old code FAIL 20/2100 under the map, new code PASS 0/2100, 0 rescues**.
- **Teleports landed on roofs** (G menu / number row / BRGo): they traced down from 20 m above and took the first hit.
  Now `FBullyLanding` picks open ground: outdoor spots snap to the nearest node of the game's sidewalk network
  (`GRIDS.DAT`, `gen_teleport_spots.py --install`, stored as `ground_cm` in InteriorSpots.json) and land within a storey of
  it; interiors land on the room's ground floor. Open patch around the landing must be clear of walls.
  Test `run_smoke_nullrhi.sh --flag -BRTeleportSmoke`: **50/50 spots standing on open ground**; outdoor landings now on
  the sidewalk (e.g. town 504, vale 920, carnival 603 cm vs. underpass/under-map before).

## Verification (Studio, 2026-10-20)

All on `phase2` @ the commit carrying this file, editor module built on the Studio and the laptop.

| Check | Result |
|---|---|
| `run_smoke_nullrhi.sh --flag -BRSandboxSmoke` (game models; also with `-BRTownLife`, and after door placement) | **PASS 19/19** |
| town life (`-BRTownLife`) | 8 districts / 45 loops / 30 parking spots loaded; business strip live: 12 walkers, 7 parked rides |
| `run_smoke_nullrhi.sh --flag -BRWeaponSmoke` (fire clips, release frame, re-aim, FX/SFX) | **PASS 8/8** |
| `run_combat_nullrhi.sh` (now with impact SFX) | **PASS** (7 + B7 checks) |
| `run_mission_smoke.sh day_one` / `1_02` / `1_01` / `arrival` / `side_errand` | **PASS** 26/26 · 20/20 · 6/6 · 6/6 · 5/5 |
| `run_mission_smoke.sh 1_01 --wrong-way` / `smoke_day_one --wrong-way` | **PASS** (3 / 2 skip-aheads, 0 stuck) |
| `run_place_doors.sh --generated` | 66 door links placed (64 game + 2 hand-made) |
| `run_showcase_shots.sh` (rendered, no window) | 46 stills reviewed: `~/bully-remake-evidence/phase2/showcase3/` (rides incl. parked police/sedan/pickup driving town, every weapon held / release / impact) |

Not verified: anything by hand-feel; doors walked through in-engine; perf on the M5 with town life; FX/SFX loudness.

## Not done / open

- **Town start spot** is a dim railway underpass (auto-picked near `town`); offer: move it (Sandbox.json `start` / search radius).
- Held-weapon offsets are identity on the right hand: the slingshot hangs (its band is skinned and in bind pose; the game holds it in
  the left hand), thrown items sit at the wrist. Tune `held_offset_cm` / `held_rot` in Weapons.json (or add a left-hand socket).
- Firecracker has no game model (sphere); pebble/potato projectiles are spheres (no game model found).
- Door links: placed and logged but **not walked in-engine**; 12 links are "facing uncertain" (see `imported/exits_report.json`).
- Town life: sedan/pickup/police parked cars + walkers only checked headless + in stills; perf with many walkers not measured on the M5.
- Still owner-only: feel (ride handling, weapon punch/fire rates, FX/SFX levels), footstep pitch by ear, standalone .app (Xcode 27 beta).

## How to run it (laptop: once, after pulling `phase2`)

```bash
cd ~/bully-remake && git fetch && git switch phase2 && git pull
export BULLY_INSTALL="<your game install>"
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" BullyRemakeEditor Mac Development -project="$PWD/unreal/BullyRemake/BullyRemake.uproject"
# game models, rigs, sounds, effects material, town data
python3 -m tools.importers vehicle bike SK8Board GoCart taxicab Policecar Limo Truck
python3 -m tools.importers prop slingshot spudg BROCKETLAUNCHER BROCKET EGGPROJ stinkbomb marble W_Itch
python3 -m tools.importers --out imported/extra_anims ped PLAYER --head B_Buzz --no-textures --root-motion inplace \
  --group V_Bike,Go_Cart,Skateboard,W_Thrown,W_Slingshot,W_BRocket,W_SpudGun,W_CherryBomb,W_Itchpowder \
  --anims RIDE BIKE_SPRINT BIKE_STANDING BIKE_TURN_L BIKE_TURN_R BIKE_SEATED_COAST BIKE_INAIR BIKE_BREAK BIKE_IDLE_A GOCART_RIDE \
  GOCART_TURN_L GOCART_TURN_R GOCART_REVERSE SK8_COAST_REG SK8_PUMP_REG SK8_TURNL_REG SK8_TURNR_REG SK8_IDLE_REG SK8_OLLIE_REG \
  SK8_BREAK_REG THROW THROW_UNDERHAND GENTHROW BALL_HOLD SLNG_FIRE_T SLNG_CHARGE_T SLNG_CHARGE_HOLD_T SLNG_2AIM_T BRKT_FIRE BRKT_AIM \
  BRKT_HOLD_IDLE BRKT_RELOAD SPUDG_FIRE SPUDG_RELOAD SPUDCANNON_AIM CHERRYBOMB_LIT CHERRYBOMB_IDLE SCRATCH_START
tools/ue/run_import_character.sh --imported "$PWD/imported/extra_anims" --anims-only
# sounds: the extract commands are listed in Config/Sfx.json's waves (python3 -m tools.importers audio extract <Bank> --names '^(...)$')
python3 -m tools.importers townlife && python3 -m tools.importers exits
tools/ue/run_import.sh --models bike,SK8Board,GoCart,taxicab,Policecar,Limo,Truck,wheel_van,wheel_cargreen,Police_wheel,slingshot,spudg,BROCKETLAUNCHER,BROCKET,EGGPROJ,stinkbomb,marble,W_Itch --sfx --townlife
tools/ue/run_import_npc.sh --peds TO_Business1,TO_Business2,TO_BusinessW1,TO_RichW1,TO_RichM1,TO_Mailman,TO_Poorwoman,TO_NH_Res_01,TO_Construct01,TO_Cop
tools/ue/run_place_doors.sh --generated
# Play: E ride · W/S A/D · Space handbrake · H horn · wheel = weapon · LMB fire · RMB lock-on · G teleport · T talk
```
(The `run_*.sh` engine harnesses are Studio-only — they go through `~/.cache/gpu-slot`.)

## Key files (new this session)

`tools/importers/props.py` (vehicle/prop), `townlife.py`, `exits*.py`; `tools/ue/run_showcase_shots.sh`, `audit_mission_skips.py`;
`Source/BullyRemake/BullyFx.*`, `BullySfx.*`, `BullyTownLifeSubsystem.*`, `BullyShowcaseShots.*`; overlays in `BullyAnimInstance.*`;
`Config/Sfx.json`, `TownLife.json`, `Vehicles.json` (`model`, `rider`, `sfx_*`), `Weapons.json` (`held_mesh`, `fire_clip`, `fx`, `sfx_*`).

## Gotchas (new)

- `git add -A` on the laptop picks up agent worktrees under `.claude/worktrees/` — now git-ignored.
- `run_import_character.sh --imported` needs an **absolute** path.
- Never move a capture camera in the same tick as a HighResShot (it renders a later frame).
- Unlit/emissive effects go black under the scene's auto-exposure — keep FX lit, use Glow only for fire.
- Editing a `run_*.sh` on the Studio while it runs breaks the running bash — don't `git pull` mid-run.

## Resume command

```
work out of ~/bully-remake on branch kimi-backend, read HANDOFF.md (kimi-backend section), pick up at "Ch3 remaining": the 3_B boss finale wave, then 3_R05B/3_R08/3_R09
```

---

# History (older sessions, kept for reference)


## ai-npc (MERGED to `main` 2026-10-19 with owner approval, plus fight-anybody + voice input; worktree `~/bully-remake-wt/ai-npc` on the Studio)

Talk to any NPC, LLM-driven, with per-NPC long-term memory and (pending credit) Fish Audio voices. Design header: `Source/BullyRemake/NPCChatSubsystem.h`; config `Config/NPCChat.json` (12 personas: GN_Bully01–05 Davis/Wade/Tom/Troy/Trent + ambient boy01/boy02/asiangirl/greekboy/nerd_bucky/prefect_seth + `*`).

- **Verified** (smoke4, `~/bully-remake-evidence/ai-npc/smoke4`, headless nullrhi): T-equivalent toggle opened a chat with walker `asiangirl` as Karen, real MiniMax reply in ~5 s in character, close fired the memory summarization, `Saved/NPCChat/asiangirl.json` landed with 10 first-person facts. `BR_NPCCHAT SMOKE OK`. Smoke harness: `BR_EXTRA_ARGS=-BRNPCChatSmoke tools/ue/run_walk_nullrhi.sh --script tools/ue/walk_npcchat_smoke.json --out <dir>` (smoke runs on wall clock and quits the engine itself).
- LLM: MiniMax `MiniMax-M2.7-highspeed` via api key file `~/.claude/secrets/npc-llm.key` (both machines). Reasoning `<think>` blocks are stripped; any OpenAI-compatible endpoint swaps in via config (local MLX server later).
- TTS: Fish Audio wired (`~/.claude/secrets/fish-audio.key`), **blocked on API credit = $0** (owner topping up at fish.audio/app/developers; API credit ≠ platform credit). All `voice` ids empty pending owner's voice picks; empty voice = silent subtitles by design.
- Not yet done: owner playtest with the interactive panel (type to NPCs in a real -game run), voice ids per ped, gestures on reply, mic input (whisper) later.
- Worktree Content/ is an APFS clone of the main checkout's imported content (regenerable, disposable).

## Map unlock + teleport (2026-10-19, Phase 1 of owner's "all map playable" ask)

Owner wants the whole world playable + unlocked, teleportable by a button, plus weapons & vehicles (real game rips kept
git-ignored; original code in-repo). Sequencing: town -> other areas -> teleport -> weapons -> vehicles.

- **Town imported & verified.** Importer gained `town`/`all` presets (`tools/importers/world.py`): `town` = academy +
  8 outdoor districts (business strip, Old Bullworth Vale, New Coventry, carnival, junkyard, BMX park, go-kart track,
  tGlobal); `all` adds ~27 interiors. `world --preset town` -> 30 scenes / 3,734 placements, 0 failures; `run_import.sh
  --scenes all` -> 1,694 meshes (0 failed, 7,835 collision slots bound), 3,604 placements placed, 0 errors. Geometry +
  collision + Lumen all came through (evidence: `~/bully-remake-evidence/town/` district + aerial shots).
- **Teleport menu.** Press **G** in-game -> Slate list of every area (all unlocked), click to warp, floor-snapped on
  arrival (`SBullyTeleportMenu` in `BullyPlayerController.cpp`, bound in `SetupInputComponent`; shares the settings-menu
  input envelope). Spots auto-generated for every imported scene by `tools/ue/gen_teleport_spots.py` (median XY, min z)
  into `Config/InteriorSpots.json` -> re-run after any world re-import. The old number-row 1..0 quick-teleport still works.
- **Interiors** imported too (57 scenes / 5,578 placements); the G-menu lists all 50 areas (8 outdoor + 42 rooms).

## Town sandbox + weapons + vehicles (2026-10-19, owner milestone: "play in the town, show the new stuff")

All original code; placeholder visuals (engine primitives) with a `mesh` path per weapon/ride for local models.
Game models are NOT extracted by the agent — drop your own into those paths.

- **Town sandbox start** (`UBullySandboxSubsystem`, `Config/Sandbox.json`): on a normal launch/PIE you start in the town
  on an open, flat street patch it finds automatically (rejects buildings, roofs, water), facing a lineup of rides, all
  weapons unlocked. Off for mission boots and any `-BR*` harness run; `-BRSandbox[=spot]` forces it, `-BRNoSandbox`
  disables it. If the town isn't imported on a machine it parks the rides next to you instead of dropping you in the void.
- **Weapons** (`UBullyWeaponComponent` + `ABullyProjectile`, `Config/Weapons.json`): slingshot, spud gun, bottle rockets,
  eggs, stink bomb, firecrackers, marbles, itching powder. Mouse wheel / D-pad cycles (Fists = melee), LMB fires, RMB
  lock-on aims. Hits go through `UBullyCombatComponent::ApplyWeaponHit`; effects: knockback, slip, gas cloud, burn, itch.
  HUD legend shows weapon + ammo; console `BRGiveWeapons`.
- **Vehicles** (`ABullyVehicle`, `Config/Vehicles.json`): bike, skateboard, go-kart, car — arcade handling written here
  (no Chaos plugin): throttle/brake/reverse/coast, speed-scaled steering, slope/kerb following, wall sliding, knocks
  pedestrians over, chase cam with mouse look. E near a ride to get on/off; W/S A/D, Space handbrake. G-menu while
  riding hops you off first. Console `BRSpawnVehicle <bike|skateboard|gokart|car>`.
- **Verified headless:** `tools/ue/run_smoke_nullrhi.sh --flag -BRWeaponSmoke` -> PASS 8/8 weapons hit;
  `--flag -BRSandboxSmoke` -> PASS 19/19 (town start, 4 rides, each accelerates/brakes/steers/mount/dismount, run-over).
- **To play it on the laptop:** pull main, rebuild, and (once) import the whole world locally:
  `python3 -m tools.importers world --preset all` then `tools/ue/run_import.sh --scenes all`. Then PIE -> you're in town.
- **Owner playtest asks (feel, not correctness):** handling of each ride (numbers in `Vehicles.json`), weapon punch /
  fire rates (`Weapons.json`), whether the auto-picked town start spot is a nice one (else set `start` in Sandbox.json or
  tune `search_radius_cm`), and streamed-town perf on the M5. Known gaps: no throw/shoot animation yet (the arm doesn't
  move), no projectile VFX/SFX, rides are primitives until you set mesh paths, NPC traffic/pedestrians in town = none yet.

## Verified state

- Repo `oh-ashen-one/bully-remake` created public 2026-10-15, seeded on `phase0` (README, CREDITS, settings, CI blocklist, LICENSE).
- **Game install verified:** *Bully: Scholarship Edition* (PC, Steam via CrossOver), 5.0 GB, complete data tree at:
  `/Users/midir/Library/Application Support/CrossOver/Bottles/Steam/drive_c/Program Files (x86)/Steam/steamapps/common/Bully Scholarship Edition`
  Key dirs: `Act/ Anim/ audio/ Coll/ Cuts/ DAT/ Data/ Graphics/ Models/ Objects/ Scripts/ Stream/ TXD/ ShaderBinaries/` plus `Bully.exe` (binary for Ghidra RE).
- Kickoff brief: `/Users/midir/Downloads/BULLY-KICKOFF-README.md` (planning session 2026-10-15).

## Phase 0 milestones (status)

1. Assets in — **DONE** (merged e1fbf74). `tools/importers world` (25 s) → `tools/ue/run_import.sh` (headless Interchange import, ~6 min cold / 25 s warm, idempotent) → `/Game/Imported/Maps/Bullworth` (2,553 placements, 8 master materials, Lumen rig). Evidence: 4×4K Lumen shots in `~/bully-remake-evidence/m1/` (front gates = the milestone shot). Known visual debts: lobby overexposed (sun leaks into interior pocket), fake-mirror floor doubles reflections, gloss/specular mapping guessed, normal-map green channel unverified. Interiors render in distant pockets and there is a mirrored under-ground copy — both match the original game's own tricks, not import bugs.
2. Interior in — **DONE** (merged): complex-as-simple collision, `BullyPawn`+`BullyGameMode` (placeholder tuning in tools/ue/README.md), `ABullyDoorLink` fade-teleport pairs from `tools/ue/door_links.json` (main east door ↔ lobby vestibule), scripted walk gates→lobby→back with 0 collision holes on the route (evidence `~/bully-remake-evidence/m2/`). Known: central-stairs landing stop (1 flagged failure), upper floors untested, interiors overexposed.
3. Jimmy moves — asset side **DONE** (merged): NIF skinning (all 5,724 models), HXD/AGR fully decoded (4,374 clips; `python3 -m tools.importers ped PLAYER --anims '*'`), measured speeds walk 2.05 / run 3.61 / sprint 8.01 m/s (docs/re-notes/anim-formats.md + locomotion-spec.md; rest pose faces backwards vs clips; Jimmy bald — hair mesh OPEN). UE side **DONE** (merged): `SK_Jimmy` + 439 clips via `tools/ue/run_import_character.sh`; `ABullyCharacter` + C++ `UBullyAnimInstance` speed blend; measured speeds applied, leg-speed synced to planted-foot speeds (sprint clip slides 2.9 m/s even in original data); camera eyeballed (4.5 m back, FOV 75, tunable via DefaultGame.ini). Evidence `~/bully-remake-evidence/m3/` incl. sequence.mp4. Glitches: residual sprint slide, fall-cam lag, no start/stop/turn anims, Jimmy bald (hair mesh OPEN).
4. Signs of life — **DONE** (merged): 6 student peds (shared C_Player clip bank — faction clip groups unchecked), `AAmbientWalker` × 9 on 6 loops (`Config/AmbientLoops.json`), capsule-swept paths, audio formats fully decoded (.SEA = XACT sound+wave bank, MS-ADPCM, ffmpeg-verified; docs/re-notes/audio-formats.md), footsteps + courtyard ambience wired (cement set only; cue graph undecoded). Evidence `~/bully-remake-evidence/m4/`. Gaps: walkers/player no mutual collision, no start/stop/turn anims.
5. The shot — **DONE, critic-accepted** after 3 rounds + camera iterations (CRITIQUE_R1/R2/R3/R3B/FINAL.md in `~/bully-remake-evidence/m5/`). Accepted: variant **B_raking_amber**, gates camera (32600, 7300, 625) → look-at (18600, 7300, 3400), FOV 64 (in `tools/ue/shots_m5.json`). Accepted frame: `~/bully-remake-evidence/m5/round3-try6/00_front_gates.png`. Cosmetic-only residuals: 2 grass blades at bottom (cull foliage near camera later), facade/path skies flatter than gates.
6. Proof capture — **our-build half DONE**: 90 s continuous 1080p30 run (gates→courtyard→jump→main door→lobby→out), walkers active, `~/bully-remake-evidence/m6/proof.mp4` + GLITCHES.md (student clips through camera ~11 s, black frame at each door cut, blown lobby-floor window reflections, flat grey backdrop through exit doorway). Fixed-timestep capture — NOT real-time perf evidence. ORIGINAL-game half pending: needs owner-attended CrossOver/Steam launch (dialog risk); drive the identical route per `docs/re-notes/proof-route.md`.

## polish1 (branch `polish1`, worktree `~/bully-remake-wt/polish`; pushed, NOT merged)

CPU-only polish while the owner played (no renderer launched; `-nullrhi` logic runs only). Details: `tools/ue/README.md` "Polish 1".

1. Player vs students: walkers are Pawn capsules (solid for the player), pass through each other, yield to the player. Headless: head-on contact on 3 loops, closest centre distance 60.0 cm (= capsules touching), 3 walker yields each, 0 STUCK in 120 s.
2. Door fade: frame-driven FadeOut -> black frame -> teleport -> settle -> FadeIn (BR_DOOR / -BRDoorTrace evidence); entrance backdrop (`DL_iSchoolEnt4`) self-lit, lobby floor glints toned (both estimates in `lighting_variants.json`, applied to the saved map with `run_lighting.sh`).
3. Hair: Jimmy's start haircut is the clothing head `B_Buzz` (painted-hair crop, reads nearly bald); `ped PLAYER --head B_Buzz` wires it; any HAIR row of Cloths.dat works (`docs/re-notes/ped-heads-hair.md`, contact sheet `~/bully-remake-evidence/polish1/hair_styles.png`).
4. Audio: XACT SDBK cue graph decoded (all banks), per-surface/role/gait footsteps with the cue pitch (`docs/re-notes/audio-formats.md` section 6).
5. Not done: grass blades near the hero camera are triangles inside the 134 m `DL_Grass02` ground mesh (needs a mesh split or a material near-fade); the floor/backdrop numbers need a by-eye check.

Merge-day checklist (rendering session, any time after merge): re-run the importers for the new head (`python3 -m tools.importers ped PLAYER --anims '*' --root-motion inplace --head B_Buzz`; `audio extract FeetAll FeetSCHL`; `audio footsteps`), then `tools/ue/run_import_character.sh`, `tools/ue/run_import_ambient.sh --steps audio`, `tools/ue/run_lighting.sh --variant B_raking_amber`; then `tools/ue/walk_polish_verify.json` (BR_EXTRA_ARGS=-BRDoorTrace tools/ue/run_walk.sh ...) and listen to footsteps on grass / path / lobby tile.

## Phase 0: COMPLETE (owner review pending)

All six milestones done with critic acceptance on the shot. Draft PR #1 open on `phase0` — owner decides the merge.

**Owner playtest:** `cd ~/bully-remake && export BULLY_INSTALL=<install path above> && tools/ue/run_import.sh` is already materialized here; open `unreal/BullyRemake/BullyRemake.uproject` in UE 5.8 and PIE the Bullworth map (boots into it), or watch `~/bully-remake-evidence/m6/proof.mp4` first. WASD+mouse / gamepad; Shift sprint; the main-door transition works both ways.

**Owner decisions pending:**
1. Merge `phase0` → `main` (PR #1)?
2. A session to capture ORIGINAL-game footage (CrossOver launch raises Steam/firewall dialogs — needs you at the desk once; ~20 min following `docs/re-notes/proof-route.md`), unlocking the side-by-side cut for episode 1.
3. Keep or trim `~/bully-remake-evidence/` (episode material: m1 gates 4K, m5 FINAL_front_gates.png + critique trail, m6 proof.mp4).

**For future sessions:** reference clones at `~/bully-remake-refs/` (re-clonable, disposable); UE Content/Imported + DDC left warm in this checkout for the owner's playtest (regenerable via run_import.sh, ~6 min).

## Rules in force

- `phase0` branch only; draft PR; **no merge to main without owner**.
- `imported/` is git-ignored and CI-blocked; game content never enters the repo.
- GPU slot protocol `~/.cache/gpu-slot`; cap 2 renderer-bearing engines; Unreal work routes through the `unreal-engine` brain skill; Epic native MCP only on a session-owned editor instance.

## Next steps

- Merge `phase0-ue` when the UE import + level + screenshots land; review screenshots.
- Milestone 2 (doors) + 3 (Jimmy): locomotion numbers live in Act.img action trees (see `docs/re-notes/locomotion-spec.md` — CAT_TO_MACT decode verified; param-name mapping OPEN).
- Original-game reference footage (milestones 3/5/6): Bully runs via CrossOver; capture needs a desktop-visible window + GPU slot — schedule when owner is not using the machine/streaming.

## Blockers / notes

- Ghidra is NOT installed on this machine (kickoff assumed a Ghidra MCP). When needed (anim formats, locomotion params, loader confirmation): `brew install --cask ghidra` and prefer headless `analyzeHeadless` scripts — no listener, no macOS dialogs. Not yet installed; not yet needed.
- Evidence convention: disposable captures → `~/bully-remake-evidence/<milestone>/` (outside repo); episode-worthy material noted here before cleanup.

## Polish pass (2026-10-15, merged to main)

Post-Phase-0 polish, verified + merged (owner granted standing merge authority for this repo):
- **Student collision**: walkers solid to player, yield+sidestep (60 cm closest, 0 stuck/120 s); walkers still pass through each other (blocking deadlocked them).
- **Door fade**: frame-driven, teleport at full black — the one-black-frame cut is gone. Exit-doorway backdrop (`DL_iSchoolEnt4`) made self-lit (gain 5000); verified render reads as exterior trees/sky, not flat grey.
- **Jimmy's hair**: his canonical `B_Buzz` head model is now imported (`ped PLAYER --head B_Buzz`); reads as a buzz-cut (near-bald by design — faithful). Other cuts one flag away; sheet in evidence.
- **Footsteps**: full cue graph decoded (11,075 cues); per-surface + per-gait via `Config/FootstepSurfaces.json`. **OPEN — needs owner by-ear check**: cue-graph pitch drops steps ~1 octave vs M4; tunable if wrong.
- **Lobby floor**: glints toned (roughness 0.45/spec 0.35); verified clean.
- Deferred: hero-shot grass blades are baked into the ground mesh, not cullable foliage.

Verify evidence: `~/bully-remake-evidence/polish1/` (render_verify/, logic logs, hair sheet). Merge-day checklist items that still want eyes/ears in a live playtest: footstep pitch, backdrop/floor brightness feel.

## Phase 1 — "Day One" opening slice (in progress, 2026-10-15)

Goal: ~10-min playable remaster of the game's opening (missions 1_01 + 1_02) to prove UE5 remastering. Recon: `docs/re-notes/opening-slice-recon.md`.

**Built + integrated on phase0 (four-system build links green; headless-verified):**
- Trigger.img importer → mission points/paths/volumes as JSON (`day_one.json`).
- Mission/objective framework + C++ HUD (`UMissionSubsystem`, `AMissionTrigger`, `BullyHUD`: objective, waypoint, E-prompt, banner). Data-driven missions in `Config/Missions/`.
- Melee combat (`BullyCombat*`): combos, block, grapple, ground kick, lock-on, enemy AI, KO; fires mission events. FEEL unjudged — needs owner hands-playtest; knobs in `docs/re-notes/combat.md`.
- Scripted NPCs (`BullyScriptedNPC`, `UScriptedNPCSubsystem`): gauntlet block/shove + fight crowd, mission-cue driven.
- Upstairs office reachable (Phase-0 "landing stop" was an old walk-script bug, not the level).
- Missions 1_01 (6 stages) + 1_02 (20 stages) authored (original objective wording); bully peds GN_Bully01-05 imported. Boot with `-BRMission=day_one` (or `1_01`). Headless driver run: 26/26 stages PASS incl. fail/retry.

**NOT done — 5 C++ framework gaps blocking seamless end-to-end play (data can't bridge; need small Source additions). Flagged, NOT fixed, per owner "flag real breaks, don't auto-fix":**
1. No data auto-start (only `-BRMission` cmdline — fine for launching).
2. No `start_mission` action → 1_01 doesn't auto-chain into 1_02 (headless driver issued it).
3. Mission can't spawn the fight opponent / set invuln (`Combat.SpawnEnemy`/`Invulnerable` console-only) → **the fight won't start on its own in a real playthrough.**
4. No fade action / no dorm `BullyDoorLink` → epilogue uses teleport + banner (rough).
5. Mission loader ignores trigger-file layout (points inlined — ok); string `blip` misparsed by `TryGetBoolField` → **mission waypoint blips dropped** (HUD regression for missions).

**Also pending:** combat juice (hit SFX/effects/camera shake — deferred to owner's feel-tuning pass); Jimmy's start outfit; a VISUAL verification render (never done for the mission HUD/combat/gauntlet — logs only so far). Best done together with closing the 5 gaps.

**Next session (on owner's go):** close the 5 C++ gaps → materialize content + visual render → rebuild + relaunch for owner playtest + combat feel-tuning. Nothing of main merged; phase0 is the slice branch.

## Phase A + B + arrival cutscene (2026-10-17, merged to main)

Branch `phase0` == `origin/main` (both at `b30aa5f` before this session's cutscene commit). Supersedes the "5 gaps NOT done / nothing merged" note above.

**Phase A — all five Phase-1 gaps closed, headless-verified, merged:**
1. Data boot mission (`UPROPERTY(Config) BootMission` in `DefaultGame.ini`; `-BRMission=` overrides).
2. `start_mission` chain action (1_01 → 1_02 auto-chains; fixed a latent `day_one` self-chain bug — `build_day_one.py` now strips `start_mission` when inlining).
3. `spawn_enemy` action (self-spawning sparring partner + invuln/attack/block gates) → schoolyard fight starts on its own.
4. `fade` cinematic action (fade-to-black → optional teleport at full black → fade-in) via `PlayerCameraManager`; dorm epilogue uses it.
5. `blip` parse fix (string/array/false now parse; waypoint restored).
Verified: `day_one` PASS 26/26 self-contained **and** as the chained pair, incl. the deliberate fail/retry.

**Phase B — objective → reward loop + an original side mission, merged:**
- `UBullyProgressSubsystem` (GameInstance) persistent wallet (money + respect); `reward` action pays it; top-right HUD wallet readout. Console: `Progress.Status/Money/Respect/Reset`.
- `Config/Missions/side_errand.json` "Campus Courier" (5 stages, original wording, academy courtyard). `tools/ue/run_mission_smoke.sh side_errand` → PASS 5/5, pays $75 + 15 respect.
- Town geometry NOT imported (only academy grounds/interiors) → open-world roam still needs an original-world-data import pass (owner-attended).

**Arrival cutscene (this session, in-engine, no video gen):**
- `Config/Missions/arrival.json` — kid dropped at the gates, faculty stand-in walks up and lays down rules, chains into `day_one`. Original dialogue + original characters.
- Three new reusable mission primitives: `camera` (scripted cutscene framings — blend/cut to an owned `ACameraActor` aimed at the player/point/spot, with FOV; `release` hands the view back), `letterbox` (cinematic bars) + `say` (one dialogue line, speaker + text). HUD: eased letterbox bars + bottom dialogue box (`BullyHUD`). The owned camera is released on `StopMission`/`CompleteMission`/`Deinitialize`, so gameplay never gets left looking through it.
- Faculty figure = existing `spawn_enemy` as a silent non-hostile stand-in (no friendly-NPC/branching-dialogue system yet).
- `CompleteMission` now suppresses the "MISSION COMPLETE" end-card when a mission chains (its `on_complete` has a `start_mission`) → clean hand-off into gameplay.
- Verified: `tools/ue/run_mission_smoke.sh arrival` → PASS 6/6, all six camera cuts + dialogue lines fire in order, camera released, no window, chains to `day_one`; `side_errand` re-smoked PASS 5/5 (no `CompleteMission` regression). Editor module rebuilds clean.

**Impact camera shake (2026-10-18, this session):**
- Pure-C++ additive shake on `ABullyCharacter`'s follow camera (decaying multi-frequency jitter on the camera's relative transform — no UMG, no `UCameraShakeBase` asset, matching the HUD's style). `AddCameraShake(Amplitude, Duration)`; a stronger shake overrides a weaker lingering tail. Config: `[/Script/BullyRemake.BullyCharacter]` `bCameraShake`/`ShakeTranslateScale` (7 cm)/`ShakeRotateScale` (1.3°)/`ShakeFrequency` (24).
- `UBullyCombatComponent` drives it: `ShakePlayerCamera()` on a connecting hit in `ApplyHitTo` (player lands) and `EnterReaction` (player takes), scaled by a per-reaction weight (flinch .5 … KO 1.6) × Config `ShakeOnHitLand` (.45) / `ShakeOnHitTaken` (1.0). Non-player fighters never shake the camera.
- New mission `shake` action (`{ "type": "shake", "amp": .., "seconds": .. }`) → cutscene impact beats; arrival uses a subtle one as the faculty member steps up.
- Verified: `run_combat_nullrhi.sh` → **PASS 7/7**, 28 shake kicks, amplitudes 0.16–0.85 scaling by reaction, empty window watch. `run_mission_smoke.sh arrival` → **PASS 6/6** (t=29.7s), `BR_MISSION shake amp=0.40` → `BR_CHAR shake` fires at the prefect beat, all six camera cuts + release intact. Editor module rebuilds clean.

**Rendered cutscene-composition pass (2026-10-18, this session):**
- New capture mode `-BRMissionShots=<id>` in `UMissionSubsystem` + harness `tools/ue/run_mission_shots.sh`: runs a mission in a real renderer (`-game -RenderOffScreen`, no window), waits out shader/distance-field compile + warm-up, then grabs one HighResShot a settle-beat (1.9 s) after each stage begins — real mission cameras, spawned characters, authored framings in pixels — and self-quits before a chaining mission starts (`BeginStage` hook fenced by `Count/TotalStages`). Same GPU-slot/window-watch/SIGTERM envelope as the other runners.
- **Result (arrival, 1920×1080, 6/6):** `~/bully-remake-evidence/missions/shots_arrival/0?_*.png`, empty window-watch. **Verdict:** the blind-authored cutscene camera framings hold up — no camera clips geometry or points at sky; `05_go_inside` is genuinely cinematic (symmetrical tree-lined approach, Lumen golden hour). **Defect found & FIXED this session:** the faculty/prefect stand-in rendered as a grey primitive (cylinder+sphere `BodyVis`/`HeadVis` placeholder) because the combat pawn's Config `PedMeshPath` points at `GN_Bully05`, which is **not** imported → runtime mesh load null. Fix: added an optional per-spawn `mesh` override to the `spawn_enemy` action (`FMissionAction::EnemyMesh` → `SpawnEnemyInFront(..., MeshOverride)` → `PedMeshPath` before `FinishSpawning`); arrival's faculty now uses the imported prefect ped `SK_PFH1_Seth` (36-bone skeleton, its own 10-clip anim bank → he animates). Re-render confirms a proper blazer-and-tie prefect walking up to the player (`03_prefect_arrives`). **Still open (combat render, separate):** the tutorial sparring partner uses the same missing `GN_Bully05` default, so it will also show the placeholder until `GN_Bully05` is imported or the Config default is repointed. Evidence PNGs show imported "B" crest / Jimmy likeness → they stay in `~/bully-remake-evidence/` only, never the repo.
- **Build gotcha (cost 1 wasted 2400 s render):** an incremental `Build.sh` can link a new hot-reload module dylib (`…-BullyRemake-000N.dylib`) **without rewriting** `Binaries/Mac/UnrealEditor.modules`, so the engine loads the *stale* dylib named by the manifest and your code never runs (symptom: 0 `LogBullyMission` lines, mission never starts, player idles at default spawn). **Fix:** after building, confirm the manifest names the newest dylib; if not, `rm` the numbered dylibs + `UnrealEditor.modules` and rebuild (regenerates a coherent pair). Verify the symbol is in the dylib: `nm <dylib> | grep <NewSymbol>`.

**Backlog correction:** **look-sensitivity defaults are already DONE** (not a gap) — `ABullyCharacter` has `LookSensitivity`/`LookSensitivityX` (.6) / `LookSensitivityY` (.5) / `LookBaseDegPerCount` (.30) / gamepad rates / invert, all Config=Game with live Esc-menu sliders (`SBullySettingsMenu`) and user-config persistence. Hit *reactions* are also already implemented (flinch/stagger/knockdown/ground/KO clips + hitstop in `UBullyCombatComponent`).

**Still unverified (headless can't settle, needs a rendered play pass / owner):** combat *feel* — impact **sound effects** (needs the right XACT cue + an audio pass) and the exact *magnitudes* of camera shake and hitstop (first-pass numbers; want eyes — a *motion* video pass, since the composition stills settle before the 0.35 s shake kick); a rendered play pass of the mission **HUD / combat / gauntlet** (the arrival **cutscene camera composition is now verified** via `run_mission_shots.sh` — framings good, one defect: grey-primitive faculty stand-in, see above); Jimmy's start outfit (needs a character import/render pass); the open-town import (owner-attended).

## Owner playtest session (2026-10-19, MacBook Pro M5 "Mac-324", merged to main)

First setup on a second machine (owner's laptop; Studio remains the dev box). UE 5.8.3 via Epic launcher, game files
copied from the Studio install, full importer + UE import chain ran clean (2,552 placements, 2 door links, 0 errors).
Owner played the arrival -> day_one slice hands-on for the first time. Fixes landed this session:

- **Block-tutorial soft-lock fixed**: new mission flag `combat_no_beaten` (set at `fight_intro`, cleared at `free_fight`
  in `day_one` + `1_02`): while set, a floor hit on the sparring partner reads as a normal hit instead of entering the
  terminal Beaten state (a Beaten enemy stops attacking -> `block_success` could never fire). `BullyCombatComponent::ReceiveHit`.
- **Controls legend**: always-on bottom-left HUD panel (move/walk/sprint/jump/interact/menu + attack/block/grapple/lock-on),
  hidden while letterbox is up (`BullyHUD.cpp`).
- **Stage soft-locks from thin triggers**: `leave_office` and `exit_building` now complete via `any` with fallbacks
  (`door_int` trigger OR arriving campus-side at new point `door_ext_out`). Root cause: the door-link teleport races the
  50 cm mission trigger box, and the stage blip then points at the interior pocket ~900 m away. Same pattern may lurk in
  other missions - see backlog.
- **Real fight opponents**: `GN_Bully05` imported with the full 439-clip bank (fight was a grey placeholder on any machine
  that never imported it; Config `PedMeshPath` default), gauntlet peds `GN_Bully01-04` + gestures via `run_import_npc.sh`.
- **Interior exploration cheats**: `BRGo <spot>` console exec on `ABullyCharacter` + number row 1..0 bindings
  (pool/audt/lib/chem/bio/art/auto/boil/clas/gdrm), spots in `Config/InteriorSpots.json` (derived from the manifest,
  floor-snapped at runtime). Temporary stand-in until real door links exist.

**Observed, not yet fixed:** a crowd student (GN_Asiangirl) renders with mangled arms during the fight-crowd gestures -
clip data verified complete (all 36 joints animated in every gesture clip), so it is a runtime blend bug in the
scripted-NPC gesture path; needs a repro render of the crowd beat (`run_mission_shots.sh` around stage 11+) or an
owner screenshot.

## Backlog (owner-reviewed 2026-10-19, in priority order)

1. **Crowd gesture arm corruption** - **FIXED 2026-10-19 (Studio); minor residual.** Runtime retargeting: the first
   time a scripted NPC plays a gesture, `EnsureGestureRetargeting` sets its skeleton's bone Translation Retargeting to
   root=Animation, all other bones=Skeleton (rotation-only, lengths from the target skeleton) — one-time per skeleton,
   runtime-only, so it travels via git with just a rebuild (no re-import). Before: GN_Asiangirl's forearms grossly
   stretched + detached; after: a natural cheer (evidence before/after in `~/bully-remake-evidence/crowd-repro/`). Residual:
   a small glove/elbow seam on some mid-swing frames (bone-orientation retarget, cosmetic) — chase later if it bothers in
   play. Applies to every scripted-NPC skeleton, so it also protects the male peds. **Earlier diagnosis (kept for
   reference):** Reproduced with a new rendered
   crowd-capture harness (`run_npc_smoke.sh --render <dir>` → `-BRNPCShotDir`; frames GN_Asiangirl close-up during the
   `crowd_1_02` cheer beat). Evidence: `~/bully-remake-evidence/crowd-repro/crowd_01_cheer.png` — body/dress/head fine,
   **forearms grossly stretched + detached** while she plays `VICTORY02`. **Root cause: bone translation retargeting,
   NOT the gesture code and NOT the clip data.** The crowd's gestures are shared C_Player (male-proportioned) clips; their
   absolute arm-bone *translations* are applied to her female skeleton (different arm lengths) → forearms stretch. Only
   GN_Asiangirl mangles because she's the only non-male-proportioned ped; male peds match the clip so look fine. **Fix
   belongs in the import pipeline** (`tools/ue/import_character.py`): set non-root bone **Translation Retargeting = Skeleton**
   (rotation-only; lengths from the target skeleton), root/pelvis = Animation/AnimationScaled — or strip non-root translation
   tracks from gesture AnimSequences. Then re-import (`run_import_npc.sh`) and re-capture. NB: the fix travels via git but the
   re-imported clips do not, so the owner re-imports on the laptop. (On the Studio the crowd bully meshes GN_Bully01-04 are
   also placeholders — not imported here; `run_import_npc.sh --peds GN_Bully01,...` to populate.)
2. **Mission robustness pass** - **DATA + CODE WRITTEN 2026-10-20 (branch `phase2-missions`, NOT built, NOT merged).** Audit table:
   `docs/re-notes/mission-robustness.md`. `use_door` (the same race as `leave_office`) plus `walk_in`, `get_past_bullies`,
   `reach_lobby`, `exit_building` got `any` fallbacks in 1_01 / 1_02 / day_one (regenerated) / smoke_day_one; parser floors reach
   radius 200 / z 150 cm. New `tools/ue/audit_mission_skips.py` (offline, run: 0 flagged) and `run_mission_smoke.sh <id> --wrong-way`
   (`-BRMissionSmokeWrongWay`, `UMissionSubsystem::TickSmoke`). TODO on an engine machine: compile, then run
   `run_mission_smoke.sh 1_01 --wrong-way` and `smoke_day_one --wrong-way` plus the normal smokes, and replay day_one by hand.
3. **Combat impact SFX**: extract punch/hit cues from the game's XACT banks (cue graph already decoded,
   docs/re-notes/audio-formats.md) and wire into `UBullyCombatComponent` hit events; then owner-tune shake/hitstop
   magnitudes (first-pass numbers in DefaultGame.ini).
4. **Real door links for interiors**: author doorway pairs (pool, library, auditorium, gym, dorms, labs...) in
   `tools/ue/door_links.json` + `run_place_doors.sh`; replaces the BRGo cheat.
5. **Interior lighting/material pass** for the never-walked pockets (owner exploration will surface the worst rooms).
6. **Town import** — **DONE 2026-10-19** (`world --preset all`; see the top of this file).
7. Known debts: sprint foot-slide, start/stop/turn anims, walker-vs-walker collision, Jimmy's start outfit,
   footstep pitch by-ear check.
