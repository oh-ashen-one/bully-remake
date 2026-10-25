# Chapter 1 opening cast: ped ids -> original models

Status: 2026-10-25, branch `sm-models`. Covers arrival, day_one (1_01 + 1_02), 1_02B, 1_02C, 1_03 and 1_04.
Ids and model names only. Machine-readable copy: `unreal/BullyRemake/Config/PedModels.json` (the runtime reads its
`stand_in` column).

## How the ids resolve

The ped id a mission script passes to `PedCreatePoint(id, ...)` / `PedSetUniqueModelStatus(id, ...)` is the first
column of the `peds` block in the install's `Objects/default.ide` (one tab-separated row per ped: id, model, txd,
female flag, size, faction type, stat block, anim groups, ...). `tools/importers/ide.py` already parses that block;
`python3 tools/ue/ped_cast.py --check` resolves every row of the table below against the install and prints
`PED_CAST_CHECK OK` (2026-10-25: 23 rows, 0 mismatches). `python3 tools/ue/ped_cast.py --resolve <id> ...` looks
up any other id. **VERIFIED**.

Which id plays which part comes from the decompiled scripts (`Bully-SE-scripts/scripts/chap1/`): the `F_TableInit`
ped table in `1_02B.lua`, `DavisCreate` / `WaveInit` / `CreateCharacters` in `1_03.lua`, the field-jock table and the
coach spawn in `1_04.lua`, the `LoadPedModels` lists of `1_01.lua` / `1_02.lua`, and the cutscene `.inf` actor lists in
`Cuts/Cuts.img` (`1-1-1` = player, car, `CS_TE_SECRETARY`; `1-1-2` = player, `CS_TE_PRINCIPAL`, `CS_TE_SECRETARY`).

## Table

| id | model | part | where | stand-in before (and fallback now) |
|---:|---|---|---|---|
| 59 | `TE_Secretary` | Mrs. McRae | arrival greeter (1_01 places her at the gates, `1_01_Secretary` / `1_01_SecretaryCam`; cutscene 1-1-1), day_one briefing (1_02 B2, office) | `PFH1_Seth` (arrival) / none (office) |
| 74 | `GN_Fatgirl` | Eunice | day_one office (1_02 `PrincipalRoom`), 1_02B | `GN_Asiangirl` |
| 130 | `Nemesis_Gary` | Gary | 1_02B, 1_04 | `GN_Boy01` |
| 75 | `DOlead_Russell` | Russell | 1_02B | `GN_Bully05` |
| 4 | `NDH1a_Algernon` | Algie | 1_02B | `GN_Boy02` |
| 70 | `GN_Greekboy` | Constantine | 1_02B | already original |
| 52 | `PFlead_Karl` | Karl (prefect) | 1_02B lockers | `PFH1_Seth` |
| 51 | `PFH2_Edward` | Edward (prefect) | 1_02B outro | `PFH1_Seth` |
| 99 | `GN_Bully02` | Davis | 1_03 (chase + barrel boss), day_one crowd | already original |
| 134 | `Peter` | Peter | 1_03 | `GN_Boy01` |
| 146 | `GN_Bully05` | bully | 1_03 `_1_03_BULLY1` and `_1_03_GUARD1`; day_one sparring partner | `GN_Bully01` / `GN_Bully04` in 1_03 |
| 85 | `GN_Bully03` | bully | 1_03 `_1_03_BULLY3` | `GN_Bully04` |
| 102, 145 | `GN_Bully01`, `GN_Bully04` | bullies | day_one gauntlet + crowd | already original |
| 73 | `GN_Boy02` | student | day_one fight crowd | already original |
| 69 | `GN_Hispanicboy` | student | day_one fight crowd (`crowd_6`; 1_02 loads 69 and 139, not `GN_Asiangirl`) | `GN_Asiangirl` (fallback `GN_Boy02`) |
| 55 | `TE_GymTeacher` | Coach Burton | 1_04 field patrol | `TO_Cop` |
| 109, 111, 112, 231, 232, 110 | `JKKirby_FB`, `JKDan_FB`, `JKDamon_FB`, `JK_Bo_FB`, `JK_Casey_FB`, `JKTed_FB` | jocks in football kit | 1_04 field squad, `_1_04_FIELDSPAWN` elements 1-6 = our `jock1`..`jock6` | `GN_Bully03` |

Not spawned in-engine by these missions (so not imported): `TE_Principal` (65, Dr. Crabblesnitch; only the cutscene
actor `CS_TE_PRINCIPAL` in 1-1-2), `TE_MathTeacher` (61, the 1_02 B8 prefect scene, which day_one does not stage),
`GN_Bully06` (147, 1_03 `_1_03_GUARD2`, a point our 1_03 does not use), the mother and stepfather of cutscene 1-01
(cutscene actors only).

## Heads, hair, outfits

Every ped above is one self-contained skinned NIF in `Stream/World.img`: body, head and hair in one or two shapes, 36
joints in the same order and with the same names as `PLAYER.nif` (checked on all 16 newly imported glTFs). Only the
player builds his head from a clothing `HAIR` model (`ped-heads-hair.md`); NPCs need no `--head`. **VERIFIED** (shape
list + skinned heights 1.5-2.0 m in the contact sheets).

## Importer fix found on the way

`NiSkinData`'s overall transform O is the inverse of the skinned shape's node transform (W_shape * O = I on every ped
checked). `ped.py` used to apply it as `S_b * O` with the shape placed by the entity rule, which zeroes depth-1 nodes.
For most peds O is a few cm, but `PFH2_Edward` / `PFlead_Karl` keep their shape under a depth-1 node at +1.03 m (they
sank 1 m into the ground) and `DOlead_Russell` / `GN_Hispanicboy` keep a depth-2 shape offset (Russell was thrown
8 m off his skeleton). The vertices are now placed at O^-1 v with IBM = S_b * O, which is exact for all of them.
Peds whose O is identity (PLAYER, B_Buzz) are unchanged.
