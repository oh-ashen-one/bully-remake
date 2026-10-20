# HANDOFF — bully-remake (phase2: game models for rides + weapons, riding/throw anims, FX + SFX, town life, door links)

Date: 2026-10-20 · Branch: `phase2` (pushed; **NOT merged to main** — owner playtest first) · Host: Studio (`ssh studio`; checkout
`~/bully-remake` on `phase2`) · Laptop checkout `~/bully-remake` also on `phase2` (edit/commit/push there, pull on the Studio).
Status: **built + headless-verified + rendered stills reviewed; owner feel-playtest pending.**

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

- **Not merged to main** — owner plays first (merge = fast-forward `phase2` → `main` once OK'd, per the standing merge rule).
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
work out of ~/bully-remake on branch phase2, read HANDOFF.md, owner playtest feedback first, then "Not done / open"
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
