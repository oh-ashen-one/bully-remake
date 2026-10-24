# Baked cutscenes: `Cuts/Cuts.img`, `PLAYLIST/Cuts.bin`, subtitle string tables

Status: 2026-10-24, branch `sm-cutscenes`. Code: `tools/importers/cutscene.py` (`python3 -m tools.importers cutscene
list|build|actors`), tests `tools/tests/test_cutscene.py` (synthetic fixtures, plus an opt-in check against the install).
Player: `unreal/BullyRemake/Source/BullyRemake/BullyCutscene.{h,cpp}`. Confidence tags as in `format-survey.md`:
**VERIFIED** (checked against the data, round-trips or every file parses), **INFERRED** (fits all data seen, not proven),
**OPEN**. This page describes layouts in our own words; no dialogue, subtitle text, animation or audio is reproduced, and
none is committed. Everything derived from the install goes to `imported/` (git-ignored).

## 0. Result

| What | Result | Confidence |
|---|---|---|
| Per-cutscene file set in `Cuts.img` | `<id>.dat` camera track, `<id>.inf` actor list, `<id>_CS*.HXD` clip index, `<group>.agr` clips, `<id>.SUB` subtitle timing; ids are the ones mission scripts pass to `PlayCutsceneWithLoad` (`1-1-1`, `1-02B`, `1-03`, ...) | VERIFIED |
| Camera track (`.dat`) | text, four keyed channels (FOV, roll, position, look-at target) with per-key in/out tangents, Hermite segments, game metres | VERIFIED (parse of every Chapter 1 opening file; sampled paths are smooth, cuts are one-frame jumps) |
| Actors and clips | `.HXD` (same container as `Anim/*.HXD`) + `.agr` (same clip codec as pedestrian animation); one body clip and one facial clip (`F_` prefix) per speaking character | VERIFIED |
| Actor world placement | per-clip `vector` field of the HXD record = the actor's world origin in game metres; the scene's yaw is in the clip's `Offset_Node` rotation | INFERRED (consistent across all 7 opening cutscenes; the rendered check is the arbiter) |
| Subtitle timing (`.SUB`) | MicroDVD-style `{start}{end}key` lines, frame numbers at the rate given in the first line (30) | VERIFIED |
| Subtitle text | `Config/Text/<Language>.img`, one string table per cutscene (`<id>_SUB.bin`), keys hashed | VERIFIED (hash), see 4 |
| Cutscene audio | `audio/PLAYLIST/Cuts.bin` entry *i* = line *i* of `Cuts.lst` (`<id>.rsm`); stereo MS-ADPCM, decoded with the existing `audio.py` | VERIFIED (decoded lengths agree with the clip/camera lengths within 0.4 s on all 7 opening cutscenes) |
| `CS_*.mxd` / `MXDs.MGR` | model descriptors pointing at `CUTS\MODELS\...`; the same models are in `Stream/World.img` as NIFs, so the descriptors are not needed for playback | INFERRED (not decoded) |

Chapter 1 opening set and where each plays (mission-script evidence: `opening-slice-recon.md`, `ch1/1_02B-this-is-your-school.md`, `ch1/1_03-the-setup.md`, `ch1/1_04-slingshot.md`):

| Cutscene | Original call site | Our mission hook |
|---|---|---|
| 1-1-1 | game start (arrival at the gate) | `arrival` stage `original_cutscene`; the six text beats carry `skip_if` on its flag |
| 1-1-2 | 1_02 B0 (principal's office) | `1_02` stage `office_cutscene` (also in generated `day_one`) |
| 1-02B, 1-02E | 1_02A C0 / C3 (boys' dorm arrival / end) | `1_02` stages `dorm_arrival_cutscene`, `dorm_room_cutscene` before the end card (we have no separate 1_02A) |
| 1-02D | 1_02B S12 (cafeteria) | `1_02B` stage `cafe_cutscene` before the prefect outro |
| 1-03, 1-04 | mission setup | `1_03` / `1_04` intro: `cutscene` action whose `else` is the existing speech sequence |

There is no baked cutscene for 1_02C.

## 1. Camera track `<id>.dat`

Plain text (CRLF), four blocks separated by `;`, in this order: **FOV**, **roll**, **position**, **target**. Each block is
`n,` followed by `n` key rows of comma-separated numbers (trailing comma):

    scalar channel (FOV, roll):   t, v,           in,           out,           e0, e1        (6 columns)
    vector channel (pos, target): t, x, y, z,     inx, iny, inz, outx, outy, outz, e0, e1   (12 columns)

- `t` is seconds from the start. Keys come one frame (1/30 s) apart where the camera cuts: the value jumps between two
  adjacent keys. **VERIFIED** in the data (key pairs 1/30 s apart with a large jump); that these are shot cuts is **INFERRED**.
- Values are in game world metres (+Z up). Position and target are absolute world points. **VERIFIED** (the paths sit
  inside the imported academy scenes at the locations the mission scripts use).
- Segment *i -> i+1* is a cubic Hermite curve using key *i*'s **out** tangent and key *i+1*'s **in** tangent, expressed
  as a per-segment delta (not per second). With tangents equal to the value delta the curve is linear. **INFERRED**
  (smooth, no overshoot at the sampled rate; a per-second interpretation overshoots visibly).
- FOV is degrees. We read it as the horizontal FOV of a 4:3 frame and widen it Hor+ for 16:9. **INFERRED**.
- Roll is degrees; the player negates it for UE's left-handed basis. **INFERRED** (sign only).
- The two trailing columns `e0, e1`: **OPEN** (mostly zero; possibly ease in/out).
- Coordinates: UE cm = (100 x, -100 y, 100 z), the same mapping as the level import.

The importer bakes the curves at 30 Hz (`camera.pos_cm`, `target_cm`, `fov_deg`, `roll_deg`) and marks a frame pair as
a cut when the position jumps more than 0.75 m between frames (`camera.cuts`); the player never blends across a cut.

## 2. Actors: `<id>.inf`, `<id>_CS*.HXD`, `<group>.agr`

- `.inf`: text; a count, then that many model names (actors and their `F_` facial tracks). **VERIFIED**.
- HXD files per cutscene: `<id>_CS.HXD` (characters, group `<id>_CHAR`), and for props `<id>_CS_CAR.HXD`,
  `<id>_CSPROP2.hxd`, `<id>_CS_SLNG.HXD` and similar, each with its own group. They parse with the pedestrian HXD
  reader (`hxd.py`): group table, per-clip records with name `<group>\<ACTOR>`, play duration, `.agr` byte size and a
  3-float vector. **VERIFIED**.
- The record vector is the actor's **world origin** (game metres). When a clip also keys `Offset_Node` positions,
  their first key equals that same vector, so the importer makes them relative to it before export. **INFERRED**.
- Clips decode with the pedestrian `.agr` codec (`agr.py`). The record sizes split the group exactly. Facial clips
  can be long (tens of thousands of keys); the clip-header sanity cap used when scanning without sizes is relaxed when
  the HXD sizes are given. **VERIFIED**.
- Character skeleton: 64 bones, identical in every cutscene HXD (Dummy, Offset_Node, Root, pelvis/spine/neck/head,
  23 facial bones at tracks 9..31, arms with fingers, legs). It is index-aligned with the joint order of every `CS_*`
  NIF in `World.img` (bone names differ, order and parenting match). **VERIFIED** (name-by-name comparison).
- `Offset_Node` carries a constant rotation = the scene's yaw, so the player spawns actors at their origin with zero
  yaw. **INFERRED**.
- Body and facial clips animate disjoint bone sets (the body clip holds constant keys on the facial bones); the
  importer copies the facial bones' tracks from the `F_` clip into the body clip. **VERIFIED** (key counts).
- Models: `<ACTOR>.nif` in `World.img`; the player character is a body + head pair (`CS_PLAY_BODY` + `CS_PLAY`),
  merged with `ped --head`. Every character model exports with 64 joints. **VERIFIED**.

## 3. Subtitle timing `<id>.SUB`

    {1}{1}30.000
    {start}{end}<id>.SUB0001
    ...

First line: frame rate. Then one cue per line, `start`/`end` in frames at that rate, the third field a string key.
**VERIFIED** (parse of every opening file; all cues fall inside the audio length).

## 4. String tables `Config/Text/<Language>.img`

An IMG archive of 256 tables (`<id>_SUB.bin` per cutscene, plus per-mission tables).

    0x00  u32 magic 0xABCD1234, u32 version 1, u32 0, u32 string-block length, u32 table length (= 8 * count),
          u32 count, u32 2, u32 0, 8 bytes (two ASCII letters + padding)
    0x28  string block: each string starts with an FF FF marker, UTF-8 text, NUL; 4-byte aligned
          then `count` (hash, offset) pairs sorted by hash; the offset points at the second marker byte

- Key hash: 32-bit, `h = h * 131 + byte` over the key text, starting at 0 (a BKDR-style hash). **VERIFIED** (keys from
  the `.SUB` files hash to entries of their table).
- Text contains `~n~` for a line break and `~CODE~` button/glyph markers; the importer turns the first into a newline and
  drops the others. **INFERRED** (glyph semantics not needed).
- Three opening tables (1-02B, 1-02D, 1-02E) are keyed under a prefix we have not recovered. Their sorted hashes keep the
  decimal spacing of `...0001, ...0002, ...` suffixes, so the importer matches cues to strings by sequence when the hash
  lookup misses and the counts and spacing agree; it refuses otherwise. **INFERRED** (every opening cue gets text; the
  whole install: 936 cues by hash, 1339 by sequence, 0 unmatched). The prefix itself is **OPEN**.

## 5. Audio `audio/PLAYLIST/Cuts.bin`

Same container as the other streamed banks (`audio-formats.md`): entry *i* is line *i* of `Cuts.lst`, named
`<id>.rsm`; stereo MS-ADPCM at 22.05 kHz. Decoded to 16-bit WAV next to the manifest. **VERIFIED**.

## 6. Playback in the remake

`UBullyCutsceneSubsystem` (world subsystem) loads `imported/cutscenes/<id>/cutscene.json` (or `Content/Imported/Cutscenes/<id>/`):

- **Full tier** when every character actor's mesh and `CUT_<id>` animation exist under `/Game/Imported/Characters/<ACTOR>/`:
  an owned camera follows the baked track, actors are spawned at their origins and scrubbed to the cutscene clock,
  every pawn is hidden, input is off, letterbox + subtitles + original audio.
- **Audio tier** (manifest + WAV only): original audio + subtitles + letterbox over the mission's own staging.
- **None** (clean clone, or `-BRNoCutscenes`): the mission runs its fallback beats exactly as before.
- Enter / gamepad right face button skips after a short grace period. Mission smoke runs skip each cutscene after 1.2 s.

Open items: the `e0/e1` camera columns; the unknown subtitle key prefix; prop actors (car, tea set, slingshot, stone,
board game) are not imported yet; whether the in-game cutscene camera should also apply the Offset_Node yaw to props.
