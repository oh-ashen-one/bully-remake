<!-- OWNER: builder-A (importers) -->
# tools/importers

Converts content from your own installed copy of *Bully: Scholarship Edition* into open
intermediates (glTF 2.0, PNG, JSON). Python 3 standard library, plus numpy (fast DXT
decoding, preview renders) and Pillow (preview only) when present. The game install is
opened read-only. Output goes to `--out` (default `imported/`, git-ignored). Format
reference: `docs/re-notes/format-survey.md`.

## Install path

Set one of these. The path is never hard-coded.

1. `--install "<path>"`
2. `export BULLY_INSTALL="<path>"`
3. `tools/importers/local_config.json` (git-ignored): `{"install": "<path>"}`

## Commands

    python3 -m tools.importers world                      # academy: tschool, ischool, isc_*, zone_school1-6
    python3 -m tools.importers world --preset core        # tschool + ischool only
    python3 -m tools.importers world --ipb tschool tGlobal
    python3 -m tools.importers exits [--previews DIR]      # ExitLocs.dat + areabbox.dat -> imported/door_links_game.json (+ exits_report.json); see tools/ue/README.md
    python3 -m tools.importers validate                   # structural check of every imported glTF
    python3 -m tools.importers preview tschool --view oblique   # CPU render to imported/previews/
    python3 -m tools.importers img list Stream/World.img --ext ipb
    python3 -m tools.importers img extract Stream/World.img tschool.ipb
    python3 -m tools.importers nif SC1b_bldg_main LOBBY01
    python3 -m tools.importers nft G_scBuilding           # or a path to a loose .nft/.txd
    python3 -m tools.importers ipb tschool ischool
    python3 -m tools.importers trigger                    # DAT/Trigger.img -> imported/triggers/day_one.json (missions 1_01 + 1_02 slice)
    python3 -m tools.importers trigger --list             # every .dat with point/path/trigger/POI counts
    python3 -m tools.importers trigger --files 1_01 --all # also per-file JSON and all.json (all 410 files, ~14 MB)
    python3 -m tools.importers ped PLAYER                  # Jimmy: skinned glTF + locomotion clips
    python3 -m tools.importers ped PLAYER --anims '*' --sheet ~/bully-remake-evidence/m3/sheet.png
    python3 -m tools.importers anim                       # list MAINPED.HXD animation groups
    python3 -m tools.importers anim C_Player              # clips: play duration, travel, speed
    python3 -m tools.importers ped GN_Boy01 --anims IDLE PLAYERWALK PLAYERRUN RUNFAST ...   # any student ped (same 36-joint skeleton)
    python3 -m tools.importers townlife --evidence ~/evidence/townlife   # town pedestrian loops + parked-car spots -> imported/townlife.json (needs the scene manifest from `world --preset town`)
    python3 -m tools.importers audio survey               # .SEA sound banks + PLAYLIST streams: counts, codecs
    python3 -m tools.importers audio list FeetAll         # wave names, durations, rates
    python3 -m tools.importers audio extract FeetAll Wind # MS-ADPCM -> 16-bit WAV in imported/audio/<bank>/ + manifest.json
    python3 -m tools.importers audio extract Ambs --names 'NewSchoolOutdoor_F|Birds01'   # streamed banks (audio/PLAYLIST), on request
    python3 -m tools.importers cutscene list              # every baked cutscene in Cuts.img (audio / subtitle / HXD presence)
    python3 -m tools.importers cutscene build             # Chapter 1 opening set -> imported/cutscenes/<id>/cutscene.json + <id>.wav
    python3 -m tools.importers cutscene build 1-05 1-06   # any other ids (as Cuts.img names them); --no-audio, --language
    python3 -m tools.importers cutscene actors            # every character actor of those cutscenes -> imported/models/CS_*.gltf + cutscenes/actors.json
    python3 -m tools.importers ped CS_PLAY_BODY --head CS_PLAY --cutscene 1-1-1,1-03 --root-motion raw   # one actor by hand

Tests: `python3 -m pytest tools/tests` (synthetic fixtures). Set `BULLY_INSTALL` to also
run the integration tests against the real install.

## Output layout

    imported/
      models/<Model>.gltf + .bin   one node, one mesh per model; primitives per NIF shape
      textures/<TXD>/<Texture>.png mip 0 of each referenced texture (RGB, or RGBA if it has alpha)
      ipb/<stem>.json              raw placements, all fields, as stored
      scenes/academy.json          scene manifest (below)
      report.json                  counts, failures by category, size, wall time
      previews/*.png               only if `preview` was run

## Scene manifest (`scenes/academy.json`, format `bully-remake.scene-manifest/1`)

- `coordinate_systems`, `units`: the conventions below, written into the file itself.
- `scenes[]`: one per IPB: `name`, `kind` (`exterior`/`interior`), section counts, bounds.
- `placements[]`: `scene`, `index`, `model`, `model_id`, `gltf` (null if not converted),
  `visible` (false for `nogo_`/`walkable_`/`_nd` marker volumes), and:
  - `game`: `position`, `scale` (per axis), `rotation_xyzw_stored` (raw),
    `rotation_quat_xyzw` (conjugated, ready to use), `rotation_rows`.
  - `ue`: `location_cm`, `rotation_quat_xyzw` (pass to `FQuat(X, Y, Z, W)`),
    `rotation_rows`, `scale`.
  - `unknown`: `unk68` (equals a per-file area code: 0 outdoors, 2 ischool, 15 isc_clas, ...;
    INFERRED), `unk112`, `unk116`.
- `models{}`: per model `gltf`, `txd`, `ide_id`, counts, `bounds_game`, warnings, missing
  textures, `sanitized_nonfinite`.
- `textures[]`, `texture_fallbacks[]`, `failures[]` (category, item, error).

## Coordinates

| Space | Handedness | Up | Unit | From game (x, y, z) |
|---|---|---|---|---|
| game (NIF, IPB) | right | +Z | metre | |
| glTF files | right | +Y | metre | (x, z, -y) |
| UE | left | +Z | cm | (100x, -100y, 100z) |

UE's glTF importer maps glTF (x, y, z) to UE (x, z, y) and metres to centimetres, so an
imported model and a manifest `ue` placement share one frame. Models are in entity space:
the NIF root and model-node transforms are identity, because the game replaces the
model-node transform with the placement. Placement rule:
`world = R * (scale * v) + position`, R = transpose of the stored quaternion's matrix.

## Materials

Base colour, normal (tangent-space RGB) and glow textures map to glTF slots; the `_s`
specular map and all raw Gamebryo values are kept in `material.extras`. `extras.blend_mode`
is `opaque`, `masked`, `translucent` or `additive`. glTF has no additive mode, so additive
materials are written as `BLEND` and the UE step must read `blend_mode`. Vertex colours are
`COLOR_0`. They hold baked lighting (`vertex_color_mode` 2 = ambient+diffuse).

## Known limits / OPEN

- OPEN-8: `BBonusB`, `Barr01_Switch`, `BeerKeg`, `BirdBath` `.nft` were LF->CRLF converted
  (header line ends in CRLF, zero bare LF bytes); the inverse is ambiguous. They are skipped.
- Normal-map green channel convention (D3D vs GL) is not verified.
- TexDesc clamp/filter bit layout is inferred from Gamebryo docs (school data: all 0x3200).
- `nif`/`world` still export skinned shapes as static meshes in their stored pose; use `ped` for a
  skinned character (skeleton, JOINTS_0/WEIGHTS_0, inverse bind matrices, glTF animations).
- Texture transforms on slots are recorded but not applied.

## Skinned characters and animation (`ped`, `anim`)

Formats and confidence: `docs/re-notes/anim-formats.md`. `ped <MODEL>` writes `models/<MODEL>.gltf`:

- one glTF node for the model, under it the skeleton joints (every NiNode of the skeleton subtree, NIF
  file order, which is the `.agr` track order: 36 joints for a pedestrian, `Dummy` first, `ARROW` last);
- one mesh node per skinned shape, all sharing one skin (inverse bind matrices from NiSkinData), up to 4
  weights per vertex (Jimmy uses at most 3);
- one glTF animation per clip, named by the HXD (`IDLE`, `PLAYERRUN`, ...), times in seconds of the HXD
  play duration, `extras` with group, hash, key type, loop check and `root_motion`
  (`travel_game_m`, `travel_gltf_m`, `speed_m_s`, `mode`).

`--root-motion`: `inplace` (default; pelvis minus the ground-arrow travel, arrow held, the travel left to
the movement code), `raw` (as stored), `dummy` (travel on the root joint, for root-motion extraction).
Forward in every clip is game +y = glTF -Z; the bind pose itself faces the other way (see the notes).
`--sheet` renders a CPU contact sheet (bind pose + frames of every clip) as evidence.

## Baked cutscenes (`cutscene`)

Formats and confidence: `docs/re-notes/cutscenes-format.md`. Nothing derived from the game is committed; everything
lands in `imported/` (git-ignored) and the game falls back to the missions' own text beats without it.

- `cutscene build [ids]` writes `cutscenes/<id>/cutscene.json` (format `bully-remake.cutscene/1`): duration, the
  audio stream (`<id>.wav`, decoded from `audio/PLAYLIST/Cuts.bin`), the subtitle cues (timing from `<id>.SUB`, text
  from `Config/Text/<Language>.img`, matched by key hash or, for tables keyed under another prefix, by sequence), the
  camera track baked to 30 Hz in UE cm (`pos_cm`, `target_cm`, `fov_deg`, `roll_deg`, `cuts`), and the actors (HXD
  clip, world origin, facial clip, World.img model, the UE asset paths the player looks for).
- `cutscene actors [ids]` runs `ped` for every character actor (props are skipped): body + head merged for the
  player (`CS_PLAY_BODY` + `CS_PLAY`), one `CUT_<id>` clip per cutscene the actor is in, the `F_<actor>` facial clip
  merged onto the face bones, raw root motion, Offset_Node keys made relative to the actor origin. It also writes
  `cutscenes/actors.json`, the list `tools/ue/run_import_cutscene_actors.sh` imports into
  `/Game/Imported/Characters/<ACTOR>/` (SK_<ACTOR>, Anims/CUT_<id>).
- In game: mission action `{"type": "cutscene", "id": "1-03", "event": "...", "else": [...]}` (see
  `MissionSubsystem.h`); console `Cutscene.Play <id> [audio]`, `Cutscene.Skip`, `Cutscene.Status`; offscreen check
  `tools/ue/run_cutscene_shots.sh <id> --times 3,10,25`.

## Town life (`townlife`)

`townlife` writes `imported/townlife.json` (`bully-remake.townlife/1`, UE cm): per outdoor district (`business_strip`
= tbusines, `old_bullworth_vale` = trich, `new_coventry` = tindust, `carnival` = tcarni, `junkyard` = tjyard,
`bmx_park` = tBMX, `go_kart_track` = tgokart, `tglobal`) a list of `loops` (`id`, `kind`, `source`, `waypoints_cm`,
`length_m`; closed, 4-10 waypoints, 25-120 m) and `parking` (`location_cm`, `yaw_deg` = UE yaw, `source`). It needs
the scene manifest + models (`world --preset town`), the install (`Config/GRIDS.DAT`), and numpy + Pillow.

**The `WALKABLE_*OP` markers carry no geometry.** Every one is a 2 KB NIF holding one 2 m triangle (and so is every
`NOGO_*OP` except `NOGO_SHOREA_B`, a 22-triangle vertical wall), so a walkable surface cannot be built from them.
The game's own pedestrian/road navigation lives in `Config/GRIDS.DAT` (1234 sidewalk nodes / 1365 links, 476 road
nodes / 507 links with lane counts, world metres), and its node heights match the converted terrain (median 0.0-0.1 m).
`townlife` uses:

- sidewalk graph -> walkable network (a segment is on the surface when every 0.5 m sample is within 1.75 m of a
  sidewalk link). Loops are graph cycles (`kind` `cycle`) or out-and-back loops offset 0.9 m each way
  (`out_and_back`); `source` `sidewalk_graph`.
- districts with no sidewalk data (junkyard, BMX park, go-kart track, tGlobal): rings / thin racetracks on flat,
  non-road, above-water terrain without tall blockers (`source` `terrain_ring`, `kind` `ring`/`racetrack`). Synthetic:
  they are valid walkable ground but not authored paths.
- parking: road-graph links, kerb = edge of road-textured terrain (street/asphalt/cobroad), spot centre 1.6 m out from
  the kerb, heading parallel to it with the kerb on the car's right, 5 x 2.4 m footprint wholly on road and clear of the
  sidewalk corridor, >= 6 m apart, >= 7 m from junctions (`source` `road_graph`). The game has road links only in
  business strip, Old Bullworth Vale and New Coventry (plus the BMX race line); other districts get none unless
  `--raster-parking` is given (road raster alone, low confidence).
- `nogo_`/`walkable_` marker models are converted to `imported/markers/` (separate from `imported/models`); stubs are
  ignored and counted in `source.markers`; real NOGO geometry becomes keep-out walls/polygons.

`--evidence DIR` writes `<district>.png` top-down renders (terrain, cyan = walkable corridor, dark lines = road graph,
colour = loops, orange = parked cars).
