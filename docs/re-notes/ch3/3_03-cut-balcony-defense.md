# 3_03 — (cut) New Coventry balcony defense: recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_03.lua` (798 lines, complete). `SMissPass.lua` has **no** 3_03 case (**VERIFIED** by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_03 = a dummied-out mission with no shipped name** — this is the doc's **identification surprise**. Content: the player meets Lola (25) in an alley in the poor district, sprints to a balcony vantage under a timer, and — staying hidden — snipes waves of greasers who are beating on **Tad** (31, preppy faction 5, with a boss-style health bar) while Lola watches; fail if any greaser *or Tad* sees the player down in the back alley. TV Tropes' [Bully trivia page](https://tvtropes.org/pmwiki/pmwiki.php/Trivia/VideoGame/Bully2006) describes exactly this as "a dummied out mission in Chapter 3 that involved Lola bringing Tad to New Coventry and Jimmy having to defend Tad by sniping Greasers with his slingshot."

Corroborating cut status, all **VERIFIED** in-tree:

- **Nothing references `"3_03"`** anywhere else in the Lua (grep across the whole scripts tree: no mission table entry, no prereq, no `ForceMissionAvailable`, no SMissPass case) — contrast 3_01/3_02, which are wired in `main.lua`/`SFaction.lua`/`MainMap.lua`.
- **No speech events at all**: every story beat is `TextPrint` subtitles (`3_03_LOLA_*`, `3_03_TAD_*`, `3_03_GREASER_*`); not one `SoundPlayScriptedSpeechEvent`. No interactive music either — only pass/fail stingers. VO/music were never produced (or were stripped).
- `MissionSucceed()` is called **bare** — no banner, no money, no respect, no rewards anywhere.
- The file is full of unpolished leftovers (see dead-code list in §2).

The canonical Chapter 3 list has no slot for it; "Wrong Part of Town" (the wiki's Algie/Chad escort) is a different mission — **VERIFIED** as `3_04.lua` (Cornelius/Algie variables, `PlayerFixToBackOfVehicle` onto Chad's bike). Title used here is ours.

Setting: New Coventry alleys, area 0 only.

## 2. Mission flow

Driver: `MissionSetup()` → `main()` → `F_Intro()` → `F_Stage1()` → `F_Stage2()` — linear function calls, with threads `T_MonitorTad`, `T_MonitorWaves`, `T_MonitorGreasers` doing the real work in stage 2.

| # | Beat | Logic (functional) |
|---|---|---|
| W0 | **Baked cutscene "3-03"** | `PlayCutsceneWithLoad("3-03", true)`. `DATLoad("3_03.DAT", 2)`, slingshot model 303 requested, radar clamped (`RadarSetMinMax(30, 75, 45)`), POI system off, ambient Lola (25) and Tad (31) suppressed. No attempt-count check — **OPEN** (moot; never shipped). |
| W1 | **Intro: Lola rides off** | Player placed at `_SPAWNPLAYER`. Lola spawns at `_SPAWNLOLA`, mounts her bike (279) at `_SPAWNLOLABIKE`, rides `PATH._INTROLOLARIDEOFF`; at path node 4 she is re-created at `_SPAWNLOLAALLEY` (invulnerable) and her bike is recreated at `_SPAWNLOLABIKEALLEY`. Control to player. |
| W2 | **Stage 1: timed approach** | Objective `3_03_OMEET`, blip on the alley; **60 s timer**. Reaching `TRIGGER._TRIGGERLOLA`: Lola and player face each other, blip moves to `_BLIPBALCONY`, objective `3_03_OVANT`, **15 s timer** to stand at the balcony point (2 m radius check). If either timer expires (before the alley / before the balcony) → `bMissionFailed`. |
| W3 | **Stage 2 setup: the ambush** | Objective `3_03_OPROT` (protect). Tad spawns at `_SPAWNTAD` with 350 health, **invulnerable to the player only**, walks `PATH._ROUTETAD`, then tethered to `TRIGGER._TRIGGERTADTETHER`. Scripted text beats: Lola waves, Tad lines, then wave-1 greasers A01/A02/A03 (24/29/21) spawn at `_SPAWNGREASERSNORTH` and walk `_ROUTEGREASER01/02/03`; A03 then runs `_ROUTEGREASER03b` and despawns at node 2 (`bDeletePeanut` — a scripted survivor who flees, **INFERRED** story role). A01/A02 attack Tad; Tad gets a **health bar** (`PedShowHealthBar(pedTad.id, true, "3_03_HEALTHBAR", false)`), made untargetable, factions set (Lola 1, Tad 5), stat overrides (Tad: 6→0, 3→50, 2→360, 14→90 — undecoded). All greasers are created via `F_CreateGreaser`: blip, anti-prep attitude, stat 3→35, **combat zone mask** (ground levels only, **INFERRED** from `PedSetCombatZoneMask(id, true, true, false)`), stat 14→90, and **ignore-list entries for the player and Lola** — they never target the player. |
| W4 | **Waves** (`T_MonitorWaves`) | Wave machine on `gWave` + `F_AllPedsInTableDead(tblGreasersAlive)`: wave 1 = A01+A02 (A03 flees); both dead → **wave 2**: B01/B02/B03 (28/27/22) walk `_ROUTEGREASERSSOUTH` (note: created with the *North* callback `F_routeGreasersNorth` — copy-paste, **VERIFIED**), plus the jumpers thread arms; all dead → **wave 3**: C01/C02 (22/24) spawn on bikes (273) at `_SPAWNBIKES`, ride `_ROUTEGREASERSNORTH` (dismount at path end → attack Tad, via `idGetOffBike` handoff in `T_MonitorGreasers`; when two are off bikes they're ordered to attack), plus **C03 (26): a slingshot sniper** (weapon 303 ×200) posted at `_SPAWNENEMYWINDOW`, put in `PedCoverSet` cover against Tad at `_POINTENEMYWINDOW` — the one enemy who can hurt Tad from range; all dead → **wave 4**: D01/D02 (24/29) from north+south paths; all dead → **wave 5**: E01/E02 (21/28) from north; all dead → pass. During one randomly chosen wave among 2-4 (`F_JumpWavePicker`), **two extra greasers (28/27) jump the east wall** (`PedJump` at `_EASTWALL`) after a 5 s delay and attack Tad. Path-end callback: every path greaser attacks Tad on arrival. |
| W5 | **The hidden-sniper constraint** (`T_MonitorGreasers`) | The player is never attacked (ignore lists), but: if **any living greaser sees the player** (`PedCanSeeObject`) **while the player is inside `TRIGGER._TRIGGERBACKALLEY`** → nag text + 4 s → fail; same if **Tad** sees the player there. So the player must do the whole defense from the balcony with the slingshot (303 was requested in setup; no ammo is granted — the player's own ammo, **INFERRED**). This is the mission's only fail pressure besides Tad's health — and it means the balcony is mandatory, not suggested. |
| W6 | **Ending** (`F_EndingCinematic`) | In-engine, partially skippable (`WaitSkippable`): fade, Tad/Lola/player teleported to `_ENDTADLOCATION`/`_ENDLOLALOCATION`/`_ENDPLAYERLOCATION`, fixed camera (540.877, -109.256, 12.798 → 545.193, -104.917, 10.1), vehicles cleared, widescreen. Lola line, Lola runs `PATH._ENDLOLARUN`; look-at cut to (568.883, -112.568, 6); Tad line, Tad runs `PATH._ENDTADRUN`; fade; both deleted; control restored. |
| W7 | **Pass** | `main()` prints `MPASS`, 3 s, pass stinger, **`MissionSucceed()` bare** — no banner/money/respect (**VERIFIED**; cut-mission state). |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Stage-1 timers expire (60 s alley / 15 s balcony) | W2 | (none — `MFAIL` text + bare `MissionFail()`) |
| Tad dies | `T_MonitorTad` | (none — `3_03_MFAIL` text + bare `MissionFail()`) |
| Seen in the back alley by a greaser or by Tad | `T_MonitorGreasers` | `3_03_MOBJ_05` nag, then bare fail |

Retry: bare `MissionFail()` — engine default (retry prompt presumed, **OPEN**). No checkpoints.

### Dead / unused code in the file (cut-mission archaeology, all VERIFIED uncalled or unwired)

- `T_PlayerOnBalcony` (a written-but-never-created thread: pauses/stops the mission timer while the player is on `_TRIGGERBALCONY2`, restarts a 5 s timer off it — implies a once-planned "get back to the balcony" mechanic).
- `CbStealthCallback` / `bSpottedOnBalcony`, `blipGreaserSpotter` + `CB_SpotPlayer` (a spotter-ped path callback that would fail on sight — never registered), `F_routeGreasersSouth` (the correct south-path callback — waves use the North one instead), `F_routeGreasersEast` (`PedClimbWall` variant of the wall entry — live code uses `PedJump`), `F_routeUpstairs` (a greaser reaching a balcony and playing a talk node — suggests a planned counter-sniper beat), `F_routeCrate`/`bTestingCrates`/`_TESTCRATES` (crate mechanic stub), `F_routeLolaIntro`, `F_AbsNumber`.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | W0 intro | baked (`3-03`) | `fade` + `letterbox` + `say` beat at `_SPAWNPLAYER` |
| CS-2 | W3 ambush reveal | in-engine text beats + `pair_anim`-adjacent wave | timed `say` (subtitle) beats + `npc_follow_path` |
| CS-3 | W6 ending | in-engine, skippable | `camera` + `say` + `npc_follow_path` |
| CS-4 | W7 pass | none (bare succeed) | if rebuilt: `completion_banner` + respect/reward decision needed |

## 4. NPCs and assets needed

### Peds / vehicles (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 25 | Lola | intro ride-off; alley meet; invulnerable spectator; faction 1 |
| 31 | Tad | the VIP: 350 hp, health bar, invulnerable-to-player, tethered, faction 5; death = fail |
| 24, 29, 21 | wave 1 (A01-03) | A03 (the "Peanut" var) flees and despawns |
| 28, 27, 22 | wave 2 (B01-03) | south path |
| 22, 24 | wave 3 bikers (C01-02) | bikes 273, dismount-and-attack |
| 26 | wave 3 window sniper (C03) | slingshot 303 ×200, `PedCoverSet` vs Tad |
| 24, 29 | wave 4 (D01-02) | north + south paths |
| 21, 28 | wave 5 (E01-02) | north path |
| 28, 27 | wall jumpers ×2 | random wave 2-4, `PedJump` at `_EASTWALL` |
| 279 | Lola's bike | ambient'd then recreated at the alley |

Greaser models used: 21, 22, 24, 26, 27, 28, 29 — the whole clique roster (**INFERRED**).

### Animation groups / action trees

No `LoadAnimationGroup` at all (**VERIFIED** — another cut marker; the wave node `/Global/3_03/Wave` and talk node `/Global/3_03/Animations/Talking` come from `Act/Conv/3_03.act`, which is also never explicitly loaded — presumed baked-cutscene side effect or a bug).

### Props / pickups / weapons

- Weapon **303 = slingshot** (VERIFIED from ch1 docs) — the player's tool; also the window sniper's weapon.
- No pickups, no props referenced.

### Speech

**None** — zero `SoundPlayScriptedSpeechEvent` calls; 20+ subtitle-only text keys (`3_03_LOLA_01..08`, `3_03_TAD_01..05`, `3_03_GREASER_01..07`, objectives/nags). Music: pass/fail stingers only.

### Named world data to extract from `3_03.dat`

- POINTLISTs: `_SPAWNPLAYER`, `_SPAWNLOLA`, `_SPAWNLOLABIKE`, `_SPAWNPLAYERBIKE`, `_SPAWNLOLAALLEY`, `_SPAWNLOLABIKEALLEY`, `_BLIPBALCONY`, `_SPAWNTAD`, `_SPAWNGREASERSNORTH` (3), `_SPAWNGREASERSSOUTH` (3), `_SPAWNBIKES` (≥2), `_SPAWNBIKESNORTH` (2), `_SPAWNENEMYWINDOW`, `_POINTENEMYWINDOW`, `_EASTWALL` (2), `_SPAWNGREASERSEAST` (2), `_ENDTADLOCATION`, `_ENDLOLALOCATION`, `_ENDPLAYERLOCATION`, `_TESTCRATES` (dead). Note the **generic, un-prefixed names** — unusual vs the `_3_0x_` convention elsewhere; suggests an early/placeholder DAT.
- PATHs: `_INTROLOLARIDEOFF`, `_ROUTETAD`, `_ROUTEGREASER01/02/03/03b`, `_ROUTELOLARUN`, `_ROUTEGREASERSSOUTH`, `_ROUTEGREASERSNORTH`, `_ENDLOLARUN`, `_ENDTADRUN`.
- TRIGGERs: `_TRIGGERLOLA`, `_TRIGGERTADTETHER`, `_TRIGGERBACKALLEY`, `_TRIGGERBALCONY2` (dead thread only).
- Hardcoded cameras: ending (540.877, -109.256, 12.798)→(545.193, -104.917, 10.1); look-at (568.883, -112.568, 6).
- Area codes seen: **0 only**. DAT `3_03.DAT` referenced; not in the Lua-only clone.

## 5. Framework gaps (beat → primitives)

Reuses: `fade`, `letterbox`, `say` (subtitle-only here), `camera`, `spawn_npc` flag bundles, `npc_follow_path` + `ped_reached_path_node(last)` (arrive-then-attack is data on the path callback), `wave_group` + `count_threshold` (wave-clear gating via `F_AllPedsInTableDead`), `countdown_fail_timer` (two-leg approach), `mission_critical_npc` (Tad death), `stealth_spotter` family (see below), `npc_cover_behavior` + `cover_point_register` (ch2 2_03 — the window sniper is exactly that), `npc_ride_vehicle_path` + `dismount_vehicle` (biker greasers), `unique_ped_suppress` (25/31), `radar_set_range`, `tether` — see 3_02's **npc_tether_to_zone**, `spawn_ally` n/a, `poi_enable`. Schema conditions `Timer`/`Reach`/`TriggerEnter` cover stage 1.

| Beat | Covered by | New primitives needed |
|---|---|---|
| W3 Tad VIP | `mission_critical_npc` | **npc_health_bar** (`PedShowHealthBar` — a boss-bar bound to a non-hostile VIP) + **npc_invulnerable_to_player** flag variant (player can't hurt the VIP; enemies can — spawn_npc needs this split) |
| W3 greaser setup | `spawn_npc` | **npc_ignore_list** (`PedAddPedToIgnoreList`: these NPCs never target the player/another NPC — the whole hidden-helper premise) + **combat_zone_mask** (floor filter on where combat may happen, **INFERRED** semantics) |
| W5 hidden constraint | `stealth_spotter`, `spotted_by` | **spotted_in_zone_fail**: fail when any of a ped set has LOS to the player while the player is inside a trigger (LOS ∧ zone, with nag-text grace) — the balcony-sniper rule |
| W4 waves | `wave_group` | nothing new (sequential all-dead gating + per-wave spawn tables is the existing pattern; the random-wave **wall-jump ambush** is a small add: **npc_jump_entry** — `PedJump` at a named point as a spawn entrance, foldable into spawn_npc flags) |
| W4 bikers | `npc_ride_vehicle_path` | **ride_dismount_attack**: ride path → dismount at last node → attack target (composition; name it if reused) |
| W2 two-leg approach | `countdown_fail_timer` | nothing new (chain two Reach+Timer stages) |
| W6 ending | `camera`, `say`, `npc_follow_path` | nothing new |

Genuinely NEW names introduced here: **npc_health_bar**, **npc_invulnerable_to_player**, **npc_ignore_list**, **combat_zone_mask**, **spotted_in_zone_fail**, **npc_jump_entry** / **ride_dismount_attack** (small, foldable).

## 6. Open questions

- Whether 3_03 was ever completable in any shipped build (SE or original): the script is functionally complete end-to-end (pass path exists), but with no wiring, no VO, no music and no rewards it reads as cut late. TCRF may have more — **OPEN**.
- Original intended name and slot — **OPEN**; the wiki/mission lists have no entry. The generic `_SPAWN*`/`_ROUTE*` DAT names suggest it predates the naming convention.
- Who was meant to see the player legitimately — as written, *Tad* spotting the player also fails, i.e. the help is meant to stay secret even from the rescuee (**VERIFIED** in code; story intent **INFERRED**).
- How the player was supposed to have enough slingshot ammo (no grant in-file) — **OPEN**; possibly why it was cut/never tuned.
- `PedSetCombatZoneMask` argument semantics and Tad's stat overrides (2/3/6/14) — undecoded, **OPEN**.
- The dead `T_PlayerOnBalcony` timer-pause thread and `F_routeUpstairs` balcony greaser imply planned mechanics (balcony occupancy pressure / counter-snipers) that were abandoned mid-script — worth mining if we ever build a homage mission.
- Whether `3_03.DAT` exists in shipped game files (the Lua references it, and trigger-format extraction would confirm) — flag for the DAT pass.
