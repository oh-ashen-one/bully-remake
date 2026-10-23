# 3_01C — Nutcrackin': recon notes

Status: 2026-10-23, read-only recon. Confidence tags: **VERIFIED** (read in the script/data), **INFERRED**, **OPEN**.

Source: decompiled Lua `Bully-SE-scripts/scripts/chap3/3_01C.lua` (435 lines, complete). `SMissPass.lua` has **no** 3_01C case (**VERIFIED** by absence). Structure described in our own words; no dialogue, cutscene or objective text reproduced. Script/native/string-key names are citations only.

## 1. Identification

**3_01C = "Nutcrackin'"** (canonical Chapter 3 list, #30, 12PM–6PM; SE-exclusive Christmas mission) — **VERIFIED** by content: the player is put in the **"Nutcracker" outfit** (`ClothingSetPlayerOutfit("Nutcracker")`, granted permanently on pass), the music-class minigame engine (`MinigameCreate("MUSIC")`, `ClassMusic*`) runs a carols track (`ms_musicclass_Carols01.rsm`), Ms. Peters (249, the music teacher — **INFERRED** from role + `MGMusic` anim set) conducts, three ballet dancers perform, and the reward is the Nutcracker outfit. The [StrategyWiki walkthrough](https://strategywiki.org/wiki/Bully/Chapter_3:_Love_Makes_The_World_Go_Round) matches: starts outside the Auditorium (area 19 below), Ms. Peters' percussionist is sick, the player fills in at the Christmas pageant, reward $20 + Nutcracker outfit — and its warning that "leaving with a passed score will still fail the mission" corroborates the `ClassMusicGetWinner() == 0` check (N3): passing the chart isn't enough, the player must out-score the AI partner. Shares only the id prefix with 3_01.

Setting: area **19** (the auditorium stage — **INFERRED**, corroborated by the walkthrough's "outside the Auditorium" start), entered via area transition from area 2 (school interior, **INFERRED** from cleanup returning to `_3_01C_PLAYEREND` in area 2). No external Lua references to "3_01C" (**VERIFIED** by grep) — offering mechanism **OPEN** (world mission table, outside the scripts).

## 2. Mission flow

Driver: `MissionSetup()` → `main()`; the whole mission is one minigame session with setup and results beats.

| # | Beat | Logic (functional) |
|---|---|---|
| N0 | **Setup + baked cutscene "3-01CA"** | `DATLoad("3_01C.DAT", 2)` **and** `DATLoad("ClassMusic.DAT", 2)` (shared music-class data — **VERIFIED** this mission reuses the class minigame's note charts). HUD fully cleared/saved, PA stopped, speech tree disabled, interactive music off. Special entity set `nutcracker` loaded; six **geometry instances** placed at hardcoded coords (~(-763.5, 307.5-308.6, 74.9-77.2)): xylophone, timpani, two xylophone sticks, cowbell, timpani sticks — the stage props. Punishment points zeroed. Outfit swapped to `Nutcracker` (backup first). `MinigameCreate("MUSIC", false)` and wait for ready. Player transitioned into area 19 at `_3_01C_PLAYERSTART`. |
| N1 | **Baked cutscene "3-01CB" + stage dressing** (`main`) | Player unequipped, weapon HUD locked. Ms. Peters (249) at `_3_01C_MSPETERS`; dancers 257/256/258 at `_3_01C_DANCER01..03`; the **second band member** (255) at `_3_01C_BAND02` — its anim root is `MGMusic/Animations/Gary/` (a costumed Gary variant playing opposite the player, **INFERRED**; Gary's normal model is 130). `ClassMusicSetPlayers(3)` (three scoring participants? player + partner + …, **OPEN**). Instruments: player index 0 → **xylophone** (InstrumentIndex 5; three sound banks), partner index 1 → **timpani** (index 3). Both peds posed with sticks then custom idles. |
| N2 | **The performance** | Get-ready text, begin text, then `ClassMusicStartSeq(3)` runs the chart. Per-note callback `F_ActionsCallback(player, action, note, passed, state)`: on pass → the matching hit anim (right/left/both-hands per instrument) + the sampled instrument note (named sample, or hardcoded per-instrument left/right/both fallbacks); on miss → a per-instrument mistake sound. Meanwhile the **camera auto-cycles** between `PATH._3_01C_CAMPATH01` and `_3_01C_CAMPATH02` every 25 s at speed (0.2, 0.2, 0.2). Dancers run their pageant nodes (`Flower`/`Fairy`) and Ms. Peters conducts — pure dressing. Loop until `MinigameIsActive()` ends. |
| N3 | **Results** | If `MinigameIsSuccess()` **and** `ClassMusicGetWinner() == 0` (the player out-scored the AI partner — **VERIFIED** competitive scoring, not just survival): sticks removed, clap SFX, `M_PASS` banner (2000 = $20, **INFERRED**), pass music, `missionSuccess = true`, `MissionSucceed`. Otherwise: failure anim + fail music + `MissionFail(false, true, "3_01C_FAIL_01")` (retry allowed). A plain non-success (`MinigameIsSuccess` false) fails with no key. |
| N4 | **Cleanup/grant** (`MissionCleanup`) | Everything restored (HUD, PA, speech, interactive music, outfit backup, health topped to max, area back to 2/`_3_01C_PLAYEREND`). On success only: **`ClothingGivePlayerOutfit("Nutcracker")`** (permanent unlock, **VERIFIED**) + blank banner with tutorial tag `TUT_301C`, success anim + bark at the school. |

### Fail conditions and retry

| Cause | Mechanism | Fail key |
|---|---|---|
| Minigame ends without success | `MinigameIsSuccess()` false | (none — bare `MissionFail(false, true)`) |
| AI partner out-scores the player | `ClassMusicGetWinner() ~= 0` | `3_01C_FAIL_01` |

Retry: full replay from N0; no checkpoints (it's one song). There is **no fail during the song** — only the end score matters (**VERIFIED**: no mid-loop fail path).

### Pass effects

In-script only: Nutcracker outfit grant, $20 banner, `TUT_301C` tag, health refill. No respect/chapter/weapon effects.

## 3. Cutscene points

| Marker | Where | Type | Suggested placeholder (our primitives) |
|---|---|---|---|
| CS-1 | N0 intro | baked (`3-01CA`) | `fade` + `letterbox` + `say` beat in area 2 |
| CS-2 | N1 stage intro | baked (`3-01CB`) | `fade` into area 19 stage |
| CS-3 | N2 performance | minigame UI + in-engine dressing | the rhythm minigame itself is the content; dancers/conductor/camera cycle are ambient beats |
| CS-4 | N3 results | in-engine anims + engine UI | `completion_banner`; win/lose `pair_anim`-style pose beats |

## 4. NPCs and assets needed

### Peds (numbers as scripted, unchecked against `default.ide`)

| Model id | Role | Notes |
|---|---|---|
| 249 | Ms. Peters | conductor dressing (`MGMusic/Animations/Teacher`) |
| 257, 256, 258 | dancers 1-3 | pageant anims `Flower`/`Fairy` from `Act/Conv/3_01C.act` |
| 255 | band partner | plays the AI timpani part; anim root `Gary/` — costumed Gary variant, **INFERRED** |

### Animation groups / action trees

`LoadAnimationGroup`: `Drumming`, `Pageant`, `UBO`, `NPC_Spectator`, `MINI_REACT`. Action trees: `Act/Conv/MGMusic.act` (shared music-class anim set — hit/miss/idle/sticks per instrument, plus `Success`/`Failure`), `Act/Conv/3_01C.act` (dancer nodes).

### Props / pickups / weapons

- Six `GeometryInstance` stage props (xylophone, timpani, sticks, cowbell) at hardcoded positions.
- Special entity set `nutcracker` (stage set; loaded/unloaded via `AreaLoadSpecialEntities`).
- Weapon models 439/443/444 requested — likely the instrument stick props-as-weapons (**OPEN**, unused by name in the Lua).
- Sound banks: `MINIGAME\COWBELL_01`, `MARACAS_01`, `TIMPANI_01`, `SNARE_01`, `XYLO_01a/b/c`, `Clapping\Claps`; instrument scene files `CAROLS_*.scn`.

### Speech / music

No scripted speech events at all (**VERIFIED** — the mission is music-only). Track: `ms_musicclass_Carols01.rsm` (single-song `SongTable`; a `NutDebugSet` debug flag would substitute `Dummy.rsm`). Pass/fail stingers + sampled notes + claps.

### Named world data to extract from `3_01C.dat` (+ `ClassMusic.dat` for the note chart)

- POINTLISTs: `_3_01C_PLAYEREND` (area 2), `_3_01C_PLAYERSTART` (area 19), `_3_01C_MSPETERS`, `_3_01C_DANCER01..03`, `_3_01C_BAND02`, `_3_01C_CAMPOINT01`.
- PATHs: `_3_01C_CAMPATH01`, `_3_01C_CAMPATH02`.
- TRIGGERs: none referenced in the Lua.
- Hardcoded: stage-prop coords (×6), two fixed cameras ((-761.265, 304.796, 78.6007)→(-761.93, 305.54, 78.54) and (-763.4773, 306.53128, 78.49783)→(-763.74, 307.50, 78.55)), player heading 135.
- Area codes: **2** (return) and **19** (stage). DATs `3_01C.DAT` + `ClassMusic.DAT` referenced; not in the Lua-only clone.

## 5. Framework gaps (beat → primitives)

Reuses: `temp_outfit_swap` (ch1/ch2 — ClothingBackup/Set/Restore) + `give_clothing` for the permanent grant, `completion_banner`, `frontend_ui_lock` (HUD save/clear/restore), `fade`, `camera` path, `world_state_swap`-adjacent (`AreaLoadSpecialEntities`), `cutscene_skip_on_retry`. The schema's `MinigameLaunch` is currently "timed hold-interact progress bar" per the header comment — this mission needs the real thing.

| Beat | Covered by | New primitives needed |
|---|---|---|
| N2 rhythm game | schema `MinigameLaunch` (v1 stub) | **minigame_rhythm**: note chart from data (`ClassMusic.DAT` analog), per-note hit/miss callback → paired anim + sample, competitive scoring vs an AI participant (`ClassMusicGetWinner`), per-player instrument binding |
| N2 camera cycle | `camera` path | **camera_path_cycle**: swap between N camera paths on a timer while a mode runs (small; could fold into minigame_rhythm config) |
| N0 stage props | `prop_set_state`-adjacent | **geometry_prop_instance**: place/remove named geometry instances at coordinates (they are not interactive props — set dressing only) |
| N0/N4 special entities | `world_state_swap` | **special_entity_set_load** (`AreaLoadSpecialEntities(name, on)`) — area-set dressing toggle; likely folds into world_state_swap |
| N2 note→sample | — | part of minigame_rhythm: per-instrument sound-bank mapping (5 instruments × left/right/both/mistake) is data |
| N1 AI partner | `spawn_npc` | **minigame_partner_bot**: a posed NPC whose hit anims are driven by its own simulated note track (its `AnimList` is fed by the same callback with player index 1) |

Genuinely NEW names introduced here: **minigame_rhythm**, **camera_path_cycle**, **geometry_prop_instance**, **special_entity_set_load**, **minigame_partner_bot**. The big one is minigame_rhythm; the rest are dressing.

## 6. Open questions

- Note chart format in `ClassMusic.DAT` (shared with the Music class minigame) — extraction pending; the rhythm minigame cannot be specced deeper without it.
- `ClassMusicSetPlayers(3)` vs two visible players — what the third scoring slot is (**OPEN**; possibly conductor/difficulty slot).
- `ClassMusicSetSong(..., 1)` second arg (difficulty/speed?) — **OPEN**.
- Weapon models 439/443/444 role (instrument props?) — **OPEN**.
- `ActionState < 6` miss gating (only early action states make mistake sounds) — enum decode **OPEN**.
- Whether the partner bot's inputs are simulated or chart-played — engine-side, **OPEN** (for the remake we can play the chart on rails).
- Instrument indexes for the mission are hardcoded (player = xylophone, partner = timpani) although five instruments exist — the other three (cowbell/maracas/snare) are presumably the Music class's rotation (**INFERRED**).
