# Campus life (sm-campus): what the original does and how the remake maps it

Scope: Bullworth Academy during the Chapter 1 opening. Runtime: `UBullyCampusSubsystem`
(`Source/BullyRemake/BullyCampusSubsystem.*`) on top of `UAmbientSubsystem` / `AAmbientWalker`, the game clock and the
campaign's faction respect. Data: `Config/Campus.json` (our values + citations), install-derived data from
`python3 -m tools.importers campus` (git-ignored `imported/campus/`, `imported/triggers/`). Sources are the decompiled
Lua in `~/bully-remake-refs/Bully-SE-scripts/scripts` and the install's data files. Everything the engine decides
(Bully.exe is DRM-wrapped, not read) is marked APPROX.

## School day

| Original | Remake |
| --- | --- |
| `Config/timeCycl.dat` periods: MORNING 7, FIRST_CLASS 9, LUNCH 12, SECOND_CLASS 13, AFTERNOON 16, EVENING 21, BED_TIME 23 | `schedule.periods`; class ends moved to 11:30 / 15:30 (next row) |
| `AreaScripts/MainMap.lua` class dockers 8:45 and 12:45 for 30 min (80 % run); class spawners 11:30 and 15:00/15:30 for 30 min; `SchoolHallways.lua` school docker 8:45 / 12:45, school spawner 11:30 / 15:30, exit docker 15:30 | students head for the school doors 15 game minutes before the bell, the stragglers run after it; after class they come back out a few per minute through the doors (`tschool_doors`, `ischool_doors` points) |
| `STimeCycle.lua` F_StartClass / F_EndClass ring `F_RingSchoolBell` (`SGlFunc.lua`: "School Bell 2D" inside the school, "Half Bell 2D" outside) | bell at class start and end; waves `SB_Int.SEA` "Schoolbell Ext Loop" x10 + "School Bell Ext End", both cues' waves |
| 1_02C sets 9:00 and rings the bell (the first class) | the campus holds the day at MORNING until 1_02C runs/passes (our boot chain does not pause the clock); 1_02C's 9:00 set starts the first class |
| F_AttendedClass: punishment 0, skipped = false; F_MissedClass: skipped = true; GetSkippingPunishment 100/100/200 at 1/3/5 skipped over the last 5 classes (`SInitGl.lua`), added at F_StartClass | same (the window, thresholds and the meter reset) |
| truancy itself (prefects bust truants on sight, "late for class") | APPROX: two game minutes after the bell, on school grounds (`Population.dat` SchoolArea volume or the main building), the meter is held at the chase line (100) and a banner says so; only after 1_02C |
| attending a class = entering the class (C_* class missions) | a classroom door during class time (or the docking quarter hour): fade, clock to the end of the period, F_AttendedClass. The class minigames are sm-minigames' |
| curfew 23:00-07:00 (`F_CurfewDefaultRules`); hangout spawners stop ~22:30; the halls empty at 18:45 | students go to the dorm doors at 22:30, the hall cast leaves at 18:45; out on the grounds after curfew holds the meter at the chase line |
| `PedPop.dat`: MainMap has 0 students in class time; School_Hallways 12 out of class, 3 in class | during class every student is inside (no hall truants yet) |

## Trouble meter, prefects, busts

| Original | Remake |
| --- | --- |
| punishment points: 1_02B forces 200 with a 199 floor until the player hides in the bin; 3_01A calls cops at 300; 2_S06 / 4_01 add 200; busts and attended classes zero it | one meter (0-400) for everything; MissionSubsystem `heat_set` / `hide` now drive it (1_02B contract unchanged: held until hidden, 25/s while hidden, `heat:zero`) |
| per-offence points, chase threshold, drain | APPROX: hit a student +20, hit a prefect +100, trespass +10/s (`PriOffice` POfficeExclude), chase line 100, unseen drain 2.5/s after 3 s |
| prefect vision `pedstats.dat` STAT_PF_BASIC_A: 100 deg total, 12 m; run 6 m/s (`ped.dat`) | cone + range + line of sight (plus 3 m all round); runs at 5.6 m/s, so a sprinting Jimmy (8 m/s) can lose him |
| prefect patrols `PrefectsSchoolGround.DAT` / `PrefectsMainBuilding.DAT` (MainMap.lua / SchoolHallways.lua PatrolPathInit) | Max on SG_Prefect01 (front lawn), Seth on the ambient crossing loop, Karl 1F_Prefect03, Edward 2F_Prefect01 |
| warnings / chase / bust speech | WARNMI under the chase line, WARNTRU / WARNT / WARNC / WARNCTC when he starts a chase, CHASE every 5 s, CHASEESC / CHASEOOB when he gives up (no sight 6 s or 30 m), BUSTING on the bust |
| `main.lua` F_PlayerBusted on school grounds after 1_02C: a class available -> sent to class; else 7-19 h and (first bust or more than 2 since the last) -> PriOff; otherwise nothing; before 1_02C the bust does nothing special | same branches: sent to class (fade, the class plays out = clock to the period end, out at 1_02C_ClassExit), principal's office (`PrincipalCutscene` PRIOFF_SPAWN, the lecture, PRIOFF_OUTSIDE), or just put back by the front door |
| `PriOff.lua` F_NISCore: first visit PRINCIPAL_LECTURING 10, 51, 52; later 1, 2, a bad deed, 50, 52 | same ids; the id is the `Crabble_PLECT_<nnn>` file number (checked by ear: the lines match their slots). Bad deed = 9 (busted by prefects) only. Subtitles from a local transcript (`--transcribe`, git-ignored) |
| detention chores (`Punishment/LawnMowing.lua`, GraffitiCleanup, P_Snow) | not built |

## Factions

`SFaction.lua` F_MissionFactionChanges sets each faction's default attitude toward the player from the first
completed mission in its list (0 Abhor .. 4 Adore; Ch1: bullies 1 (0 after 1_07), nerds 2 -> 3 after 1_04, students
2 -> 3 after 1_04, preps / greasers / jocks 2, dropouts 1, prefects 1). Campaign respect (the `SetFactionRespect`
values, 1_01 seeds bullies 25 and the others 50) moves it one step at >= 80 / < 20 (APPROX). Behaviour by attitude:
Dig+ greet, Averse- taunt / jeer, Abhor fighters (pedstats aggression >= 50) pick a fight within 3 m (one a minute,
never in a mission). Hit: aggression < 15 runs (SCARED / DONTHIT), the rest fight back (INDIG) and one faction mate
with aggression >= 50 within 10 m joins; bystanders cheer (FIGHW / SEESCRAP); KO'd: FIGHB. Faction, voice and
aggression per model come from `Objects/default.ide` (`peds`: type, stat, `N_<name>`) and `pedstats.dat`.

## Ambient speech context codes

Speech.lst holds ~38 000 non-mission lines `<Speaker>_<CODE>_v<k>` (speaker = the ide `N_<name>`, cut to 8 letters).
The codes are abbreviations of the event names the scripts pass to `SoundPlayAmbientSpeechEvent` (and that Scripts.img
lists); a sample of each was checked by ear. The importer takes 3 takes per (speaker, code) for the campus cast.

| Code | Event | Used for |
| --- | --- | --- |
| GREE / RESPGF / RESPGD | GREET / RESPONSE_GREET_FRIENDLY / dismissive | greeting Jimmy |
| BUMPF / BUMPR | bump (apologetic) / BUMP_RUDE | Jimmy runs into him |
| TAUN, JEER, LAFFC | TAUNT, JEER, LAUGH_CRUEL | Averse / Abhor students |
| CHATTER, COMPLAIN | CHATTER, COMPLAIN | Dispassionate students, idle |
| CONVQ -> CONVQR, CONVG -> CONVGR, CONVPP -> CONVPR, CONVNP -> CONVNR, CONVP | CONVERSATION_QUESTION / _REPLY, _GOSSIP, positive / negative personal + reply, parting | two idle students (hangout cliques) talking |
| INDIG, FIGHI, FIGH, FIGHB, FIGHW, SEESCRAP, SCARED, DONTHIT | INDIGNANT, FIGHT_INITIATE, FIGHTING, FIGHT_BEATEN, FIGHT_WATCH, SEE_SOMETHING_CRAP, SCARED, DONT_HIT | fights |
| WARNMI, WARNTRU, WARNGTC, WARNT, WARNC, WARNCTC, CHASE, CHASEESC, CHASEOOB, BUSTING | WARNING_MINOR_INFRACTION, truancy, go to class, WARNING_TRESPASSING, curfew, WARNING_COMING_TO_CATCH, CHASE, chase escaped / out of bounds, BUSTING_JIMMY | prefects |

## Cast and missions

Front lawn: the 9 ambient-loop students (duplicates replaced so each named student appears once), five hangout
cliques at `tschool_hangouts` points (bullies Troy + Trent, jocks Kirby + Dan, preps Gord + Tad, greasers Lefty +
Vance, girls Pinky + Mandy) and two prefects. Halls: six students on `Patrol_School` hall paths and two prefects.
Walkers more than 110 m from Jimmy freeze and hide. While a mission runs: no truancy, no new trouble, no chases or
picked fights, every walker is a bystander (pass-through, out of the combat registry), and walkers whose model the
mission's JSON names are hidden (the original's `F_SetCharacterModelsUnique`).
