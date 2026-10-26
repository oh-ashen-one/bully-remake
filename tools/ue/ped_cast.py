# OWNER: sm-models (story cast)
"""Story-cast ped table (unreal/BullyRemake/Config/PedModels.json): numeric ped id -> original model name.

  python3 tools/ue/ped_cast.py                 -> "PEDS Nemesis_Gary DOlead_Russell ..." (rows with import=true)
  python3 tools/ue/ped_cast.py --check         -> resolves every id in the install's Objects/*.ide `peds` blocks
                                                  and fails if a row's model disagrees (needs BULLY_INSTALL or
                                                  tools/importers/local_config.json)
  python3 tools/ue/ped_cast.py --resolve 130 75 -> id -> model straight from the install

The table holds ids and model names only; nothing from the install is written anywhere.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
TABLE = os.path.join(REPO, "unreal", "BullyRemake", "Config", "PedModels.json")
sys.path.insert(0, REPO)


def load_table(path=TABLE):
    with open(path) as f:
        return json.load(f)["peds"]


def install_peds():
    """{id: model} from the `peds` sections of the install's .ide text sources."""
    from tools.importers.cli import resolve_install
    from tools.importers.ide import ModelDefs
    defs = ModelDefs.load_install(resolve_install(None))
    return {e.id: e.model for e in defs.by_name.values() if e.section == "peds"}


def main(argv):
    if "--resolve" in argv:
        peds = install_peds()
        for a in argv[argv.index("--resolve") + 1:]:
            print(a, peds.get(int(a), "?"))
        return 0
    rows = load_table()
    if "--check" in argv:
        peds = install_peds()
        bad = 0
        for r in rows:
            got = peds.get(r["id"])
            ok = got is not None and got.lower() == r["model"].lower()
            bad += 0 if ok else 1
            print("%-4s %3d %-16s install=%s" % ("OK" if ok else "BAD", r["id"], r["model"], got))
        print("PED_CAST_CHECK %s (%d rows, %d mismatches)" % ("OK" if bad == 0 else "FAIL", len(rows), bad))
        return 1 if bad else 0
    print("PEDS " + " ".join(r["model"] for r in rows if r.get("import")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
