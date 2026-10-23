# Story Mode Catalog & Gap Analysis — Bully: Scholarship Edition

Date: 2026-10-22 · Branch: `kimi-backend` · Status: research baseline for the full story-mode build.

Purpose: the complete list of everything playable in the original game's story mode, checked
against what this repo already has, so we know exactly what's missing for a 100% story mode.

Sources (fan wikis/guides, behavior reference only — all shipped wording/content stays original
per the repo's legal line):

- [Bully Wiki (Fandom) — Missions](https://bully.fandom.com/wiki/Missions)
- [Steam — 100% Completion Guide for Bully: Scholarship Edition](https://steamcommunity.com/sharedfiles/filedetails/?id=1269231443)
- [TrueAchievements — Bully: Scholarship Edition story walkthrough](https://www.trueachievements.com/game/Bully-Scholarship-Edition/walkthrough/3)
- [GameFAQs — Bully: Scholarship Edition Guide and Walkthrough](https://gamefaqs.gamespot.com/xbox360/942199-bully-scholarship-edition/faqs/52370)

---

## 1. Story missions (gold star) — 52 total, 5 chapters + Endless Summer

Times are the in-game availability windows. (O) = optional/red-star mission — not required to
finish the story; several are required for 100%.

### Chapter 1 — "Making New Friends and Enemies" (14)
| # | Mission | Window |
|---|---|---|
| 1 | Welcome to Bullworth | intro |
| 2 | This Is Your School | chains from #1 |
| 3 | The Setup | 8AM–7PM |
| 4 | Slingshot | 8AM–7PM |
| 5 | A Little Help | 8AM–11PM |
| 6 | Save Algie | 8AM–7PM |
| 7 | That Bitch | 8AM–7PM |
| 8 | The Diary (O) | 7PM–11PM |
| 9 | Defend Bucky | 8AM–11PM |
| 10 | The Candidate | 8AM–6:30PM |
| 11 | Halloween | time-stopped special |
| 12 | The Big Prank (O) | 8PM–1AM; must be done Halloween night or it locks out |
| 13 | Character Sheets (O) | 8AM–9PM |
| 14 | Help Gary | 8AM–11PM (chapter boss: Russell) |

### Chapter 2 — "Rich Kid Blues" (12)
| # | Mission | Window |
|---|---|---|
| 15 | Last Minute Shopping (O) | 8AM–7PM |
| 16 | Hattrick vs. Galloway | 8AM–6:30PM (unlocks Photography) |
| 17 | Small Offenses | 8AM–12AM |
| 18 | The Eggs | 8AM–11PM |
| 19 | Movie Tickets | 8AM–8PM |
| 20 | Carnival Date | 4:30PM–11PM |
| 21 | Panty Raid (O) | 7PM–12AM |
| 22 | Race the Vale | 8AM–11PM |
| 23 | Beach Rumble | 8AM–11PM |
| 24 | Weed Killer | class-gap windows |
| 25 | Tad's House | 8AM–11PM |
| 26 | Boxing Challenge | 8AM–1AM (chapter boss: Bif/boxing) |

### Chapter 3 — "Love Makes the World Go Around" (17)
| # | Mission | Window |
|---|---|---|
| 27 | Christmas Is Here | auto on chapter start (SE-exclusive) |
| 28 | Balls of Snow | 8AM–7PM |
| 29 | Miracle on Bullworth St. | 6PM–10PM |
| 30 | Nutcrackin' | 12PM–6PM |
| 31 | Rudy the Red Nosed Santa | 12PM–6PM |
| 32 | Cook's Crush (O) | 8AM–5PM |
| 33 | Cook's Date (O) | 3:30PM–11PM |
| 34 | Comic Klepto (O) | 3:30PM–11PM |
| 35 | Jealous Johnny | 8AM–11PM |
| 36 | Bait | 8AM–11PM |
| 37 | Glass House (O) | 3:30PM–7PM |
| 38 | Tagging | 8AM–11PM (unlocks free tagging) |
| 39 | Wrong Part of Town | 8AM–11PM |
| 40 | Lola's Race | 8AM–11PM |
| 41 | Tenements | 8AM–11PM |
| 42 | Discreet Deliveries (O) | 5PM–11PM |
| 43a | The Rumble | 8AM–11PM (script `3_06`; Peanut chase, New Coventry brawls) |
| 43b | Fighting Johnny Vincent | chapter boss (script `3_B`; some lists fold 43a/43b into one "Rumble" entry) |

### Chapter 4 — "A Healthy Mind in a Healthy Body" (9)
| # | Mission | Window |
|---|---|---|
| 44 | Here's to You, Ms. Phillips | 3:30PM–7PM |
| 45 | Stronghold Assault | 8AM–9PM |
| 46 | Galloway Away | 8AM–7PM |
| 47 | Funhouse Fun | 8AM–11PM |
| 48 | Paparazzi | 8AM–7PM |
| 49 | Defender of the Castle | 8AM–11PM |
| 50 | Nice Outfit | 8AM–7PM |
| 51 | Discretion Assured | 8AM–9PM |
| 52 | The Big Game | 8AM–9PM (chapter boss: Ted/Jocks) |

### Chapter 5 — "The Fall and Rise of Jimmy Hopkins" (13)
| # | Mission | Window |
|---|---|---|
| 53 | Cheating Time | 8AM–7PM |
| 54 | Making a Mark | 8AM–11PM |
| 55 | Rats in the Library | 8AM–9PM |
| 56 | The Gym Is Burning | 8AM–9PM |
| 57 | Finding Johnny Vincent | 8AM–11PM |
| 58 | Revenge on Mr. Burton | 8AM–7PM |
| 59 | Smash It Up (O) | 7PM–11PM |
| 60 | The Collector (O) | 7PM–11PM |
| 61 | Preppies Vandalized | 8AM–11PM |
| 62 | Go See the Principal | chains from #61 |
| 63 | Busting In, Part I | 8AM–11PM |
| 64 | Busting In, Part II | chains from #63 |
| 65 | Complete Mayhem | 8AM–8PM (final boss: Gary) |

### Chapter 6 — "Endless Summer"
No missions: free-roam state after the story; all remaining stars/errands/classes stay available.

Note: mission numbering above (65 rows) counts optional red-star missions inline, matching the
walkthrough sources. The "52 story + 24 side = 76 stars" split in the 100% guides counts
differently (side missions include the optionals above plus the 6 Hobo transistor missions and
other one-offs); the exact gold/red classification per row needs one verification pass against
the game script dumps (`docs/re-notes/` + Bully-SE-scripts research) before authoring data.

### Chapter 1 verified script mapping (from `docs/re-notes/ch1/`, 2026-10-22)

Read of the decompiled scripts corrected several assumptions; the engine script ids are the
authoritative mapping for authoring:

| Script | Mission | Notes |
|---|---|---|
| 1_01 / 1_02 / 1_02A | Welcome to Bullworth / (opening walk) / dorm epilogue | already built (`day_one`) |
| 1_02B | This Is Your School | systems tour: apology, lockpicking, bust/hide, gift; was missing from the naive numbering |
| 1_02C | first-class connector | clock to 9:00, reach Chemistry, unpauses the world clock |
| 1_03 | The Setup | chase + hit-count Davis fight; grants slingshot (item 303) |
| 1_04 | Slingshot | bus windows + tree sniping with stealth spotter |
| 1_05 | **Save Algie** (not A Little Help) | escort + bathroom gauntlet |
| 1_06_01 | **A Little Help** (not Save Algie) | hobo lesson template for the whole transistor chain |
| 1_07 | Defend Bucky | three-wave defense; grants skateboard (437); force-completes 1_E01 |
| 1_08 | That Bitch | gift/social fetch + gym locker stealth |
| 1_09 | The Candidate | auditorium defense; grants super slingshot (306); pauses clock into Halloween |
| 1_10 | Halloween | night escort + basement puzzle rooms, ends at the Hole |
| 1_11x1 / 1_11xp | Halloween parts 1–2 | costume + prank-collection hub (5 of 6 playable pranks) |
| 1_11x2 | The Big Prank (O) | optional, Halloween night only; rewards Red Ninja outfit |
| 1_11_Dummy | "night is over" marker mission | strips Halloween dressing |
| 1_E01 | errand-system tutorial (Petey→Beatrice) | pseudo-mission, not on the canonical list |
| 1_G1 | The Diary (O) | evening infiltration; DAT file is `1_G1_new` |
| 1_S01 | Hattrick vs. Galloway (O) | bottle stealth fetch; grants camera (328). Canonical lists place it in Ch2; the script id says Ch1-era side mission — placement flagged OPEN |
| 2_S04 | Character Sheets (O) | 4-stash collection; engine ties its launch to the Ch1→2 transition (canon-vs-engine contradiction, engine authoritative) |
| 1_B | Help Gary (boss: Russell) | passing sets Chapter 2, force-completes 1_11x2 + level-1 classes |
| Chapt1Trans | chapter 1→2 interstitial | camera flyover + title card + faction seeding |

Trigger/point/path data for all of these was extracted from the owned install's `DAT/Trigger.img`
to `imported/triggers/<stem>.json` (local only, git-ignored like all converted game data).

## 2. Classes — 10 subjects × 5 levels each (SE)

Rotating 5-day schedule; classes run 9–11:30AM and 1–3:30PM. Passing level 5 of each is required
for 100%; failing any class 5 times grants the Dunce Hat.

| Class | Unlocks | Minigame type | Pass reward (headline) |
|---|---|---|---|
| Chemistry | Ch1 start | button-sequence | Chemistry Set → craft firecrackers/stink bombs/itching powder |
| English | Ch1 start | word scramble | apologize to bullies/prefects/police; better taunts |
| Art | Ch1 start | area-fill drawing | kiss health bonus 200%, no gift requirement, trophy |
| Gym | Ch1 start | wrestling tutorial + dodgeball best-of-3 | wrestling moves, weapon accuracy, trophy |
| Biology | Ch1 start | timed dissection sequence | clothing, trophy |
| Music | Ch1 start | rhythm | clothing sets, trophy |
| Shop | Ch2 | button + stick rotation | Red BMX (fastest bike), BMX frame |
| Math | Ch2 | timed quiz | clothing, trophy |
| Geography | Ch2 | flag-on-map | collectible map markers (bands/cards/transistors/gnomes), trophy |
| Photography | after Hattrick vs. Galloway | photo targets on location | photo album, Yearbook, digital camera, 2× carnival tickets |

## 3. Side content tied to story-mode progression

- **Errands** — 50 total (blue X givers), unlocked after Pete's first errand; 47 needed for 100%
  (40 for the Perfectionist achievement tier per TrueAchievements).
- **Hobo transistor missions** — 6 transistors found in the world (after "A Little Help"), each
  traded to the Hobo for a new fighting move; each counts as a mission.
- **Jobs (unlock after "Last Minute Shopping")** — Paper Route ×4 levels (10→24 clients, $134
  total); Lawn Mowing ×6 levels (3 parks + 3 houses, $15–$25 each).
- **Races** — Bike: 14 (7 Vale, 4 Town, 2 New Coventry, 1 Academy in Ch5); $475 + gear. Go-kart:
  8 (5 carnival → unlocks 3 town races; 8th needs Ch5); wins grant tickets, $75, personal go-kart.
- **Carnival** — 3 rides (Big Canyon Railway, Big Squid, Ferris Wheel), 4 games (High Striker,
  Strike Out, Splish Splash, Shooting Range), prize tent (300 tickets for everything; Scooter 75),
  3 arcade high scores (ConSumo, Future Street Racer/Monkey Fling, Nut Shots).
- **Sports** — Boxing: 5 matches at Glass Jaw Boxing Club (Ch2+, +punch damage each). Penalty
  Shots (vs. Constantinos, gym wall) and Keep-Ups (vs. Luis, football field): win once each.
- **Yearbook** — unlocks after Photography 2; photograph every student (some auto-added by
  missions); final reward $300 + Black Ninja Outfit.
- **Collectibles** — 75 Rubber Bands (→ Rubber Band Ball weapon), 40 G&G Cards (→ Grotto Master
  outfit), 6 Transistors, 25 Garden Gnomes (→ Gnome outfit), 27 Pumpkins + 19 Tombstones
  (Halloween-gated; relocate to basement/warehouse after).
- **Clothing** — buy out School Store, Worn In, Aquaberry, The Final Cut; 250+ items for the
  achievement tier, ~330 for full 100%.
- **Misc 100% trackers** — 100 sodas bought / 500 drunk; 100 km on foot / 100 km bike / 50 km
  skateboard; 20 fire-alarm pulls; 1000 hits landed; 200 KOs; 50 wedgies; 100 taunts; 50 stink
  bombs + 25 marble throws on victims; 25 cars egged; tag one graffiti per clique (Nerds,
  Preppies, Greasers, Jocks); beat the pirate on the island (Pirate Hat); break into lockers for
  the 2 Halloween masks; high-score all arcade games.
- **Bosses** — Russell (Ch1), Bif/boxing (Ch2), Johnny Vincent (Ch3), Ted + Jocks (Ch4),
  Gary (Ch5 finale); plus named minibosses (Davis, Norton, Mascot, Derby).

## 4. Systems a 100% story mode needs (engine/framework level)

Derived from the content above — these are the load-bearing systems behind the mission data:

1. **Campaign/chapter machine** — chapter state, mission unlock graph, gold/red star lifecycle,
   "chapter complete" and Endless Summer end-state.
2. **In-game clock + calendar** — mission time windows, class schedule (5-day rotation,
   morning/afternoon slots), curfew, Halloween special day, sleep/pass-time in the dorm.
3. **Overworld mission starts** — star markers on map/minimap at spawn points with time gating,
   walk-in-to-start, unavailable/locked presentation.
4. **Cutscene pipeline** — mission intro/outro cinematics. Placeholders for now: our existing
   `camera`/`letterbox`/`say`/`fade`/`shake` primitives cover a placeholder pass per mission.
5. **Dialogue system** — multi-speaker lines, player-skippable, hooked to `say` (current version
   is single-beat); subtitles + (later) voice.
6. **Mission archetypes beyond reach/fight** — escort/protect (Save Algie, Defend Bucky), follow
   undetected/tailing (Paparazzi, Cheating Time), timed delivery (The Eggs, Discreet Deliveries),
   races vs AI opponents, collect-N-items, defend-a-position (Stronghold Assault, Defender of the
   Castle), stealth/detection meters (Panty Raid, Tad's House), vehicle missions, photography
   targets, tag-spraying, boss encounters with phases.
7. **Respect/faction system per clique** — Nerds/Bullies/Preppies/Greasers/Jocks/Townies standing
   changed by missions; drives ambient hostility (current wallet has a single `respect` number).
8. **Class minigames** — the 10 subjects × 5 levels (each its own small game + rewards/unlocks).
9. **Detection/authority system** — prefects/police, trouble meter, trespassing/curfew rules,
   detention (mowing/shoveling mini-punishments); required by half the mission list and by
   free-roam rules (Red Ninja trespass immunity, English apologies hook into this).
10. **Economy/shops** — stores with buyable clothing/items, carnival tickets + prize tent, soda
    stock, bike storage.
11. **Collectible/stat trackers** — the 100% checklist items in §3, stats screen, room trophies.
12. **Save/load** — persistent story progress (current progress subsystem is wallet-only,
    session-scoped).
13. **Map/minimap markers** — stars, blue X errands, collectible overlays after Geography,
    waypoint polish.
14. **Character roster** — story NPCs (Gary, Pete, Russell, Crabblesnitch, Zoe, Derby, Johnny,
    Ted, Earnest, Edgar, clique lieutenants, teachers, Hobo) as scripted mission characters with
    gesture/dialogue beats (extends the scripted-NPC system).

## 5. What the repo already has (verified from HANDOFF.md + source, 2026-10-22)

- **Mission framework** (`MissionSubsystem.*`, JSON in `Config/Missions/`): stages, conditions
  (`reach_location`, `trigger_enter/exit`, `interact`, `event`, `timer`, `flag`, `all`/`any`),
  actions (`hint`, `banner`, `event`, `set_flag`/`clear_flag`, `log`, `teleport`,
  `start_mission` chaining, `spawn_enemy`, `fade`, `reward`, `letterbox`, `say`, `camera`,
  `shake`), boot mission config, smoke harness (`run_mission_smoke.sh`, `--wrong-way` skip
  audits), rendered shot harness (`run_mission_shots.sh`).
- **Missions present**: `arrival` (original in-engine cutscene), `1_01`, `1_02` (= day_one
  chain), `side_errand` (original side mission), `smoke_day_one` (test). → **2 of the original
  52 story missions exist** (Welcome to Bullworth ≈ 1_01 arrival/first steps; This Is Your
  School ≈ 1_02).
- **Combat** (melee stack, enemy AI, KO, camera shake, impact SFX) — covers fight stages.
- **Scripted NPCs** (gauntlet, fight crowd, gestures) + LLM chat (side experiment).
- **Progress** subsystem: money + single respect counter, `reward` action, HUD wallet.
- **World**: academy + town districts + interiors imported; door links; teleport menu.
- **Vehicles** (bike/skateboard/go-kart/cars), **weapons** (slingshot, spud gun, rockets, eggs,
  stink bombs, firecrackers, marbles, itching powder), town life (walkers, parked cars).
- **HUD**: objective text, waypoint, banners, E-prompts, letterbox, dialogue line, legend.

## 6. The gap, honestly

| Area | Have | Missing for 100% story mode |
|---|---|---|
| Story missions | 2/52 (+arrival framing) | 50 story missions + all optional/side missions |
| Cutscenes | placeholder primitives proven on `arrival` | placeholder cutscene per mission intro/outro; multi-line dialogue scenes |
| Clock/calendar | none | day/night clock, class schedule, curfew, Halloween, sleep |
| Mission starts | boot mission + cmdline only | overworld star triggers with time windows + unlock graph |
| Chapters | none | chapter state machine, Endless Summer |
| Classes | none | 10 minigames × 5 levels + rewards |
| Factions | one respect number | per-clique respect driving ambient AI |
| Authority | none | prefects/police/trouble/detention |
| Mission archetypes | reach/interact/fight/timer | escort, tail, defend, race vs AI, stealth, photo, delivery, boss phases |
| Economy | wallet only | shops, tickets, prizes, clothing inventory |
| Collectibles/stats | none | all §3 trackers + stats screen + trophies |
| Save | session wallet only | real save/load of story state |
| Story NPCs | 6 bully peds + prefect + 10 townsfolk | full named roster with personas |
| Errands | 1 original errand | 50 errands framework (blue X givers) |
| Jobs/races/carnival/sports | vehicles exist | all job/race/carnival/boxing/minigame content |

## 7. Suggested build order (each piece independently verifiable headless + rendered)

1. **Clock + calendar + mission-start stars + unlock graph + chapters** — the spine; without it
   every mission is cmdline-only. Extend `MissionSubsystem`; new `CampaignSubsystem`.
2. **Chapter 1 missions 3–14** (12 missions) using existing primitives + escort/protect and
   follow archetypes; placeholder cutscene open/close per mission via the arrival primitives.
3. **Boss framework** (Russell) — phased combat + arena scripting.
4. **Dialogue upgrade** (multi-beat conversations, skip) + story-NPC roster import.
5. Chapters 2–5 mission batches, each wave adding the archetypes it needs (stealth, tailing,
   races, defense, photo).
6. Classes, jobs, carnival, sports minigames (independent of mission waves).
7. Economy + collectibles + stats/trophies + save/load.
8. Errands ×50 (data-driven once giver framework exists).
9. Endless Summer end-state + 100% completion audit screen.

Every mission gets: `Config/Missions/<id>.json`, `run_mission_smoke.sh <id>` PASS, a rendered
`run_mission_shots.sh` pass for its cutscene beats, and an entry in the verification ledger.
