#!/usr/bin/env python3
"""Final-savegame army census, paired across two arms.

The scorelogs say deep buys fewer kills per unit it builds.  They cannot
say why.  A retained end-of-game savegame can distinguish the two
candidates:

  * an *idle* army -- units alive, modern, sitting in Fortified/Sentry
  * an *obsolete* army -- units alive and busy, but of older types than
    the opponent's, so they lose the fights they do pick

so this reports, per game and averaged over seeds, the unit-type mix, the
activity mix, and the veteran-level mix at the last saved turn.

    ./armycensus.py --ref baseline=DIR --arm pv3=DIR [--seeds 101-116]

DIR is an arm directory holding seed-N/saves/*final*.sav[.zst]
(optionally under cfg0/).
"""

import argparse
import os
import re
import sys
from collections import Counter

import savequery
from wargap import mean, sem, tstat, parse_seeds


def find_final_saves(root, seeds=None):
    """{seed: path} for the last save under each seed directory."""
    out = {}
    if not os.path.isdir(root):
        return out
    entries = sorted(os.listdir(root))
    if "cfg0" in entries and not any(e.startswith("seed-") for e in entries):
        return find_final_saves(os.path.join(root, "cfg0"), seeds)
    for name in entries:
        m = re.fullmatch(r"seed-(\d+)", name)
        if not m:
            continue
        seed = int(m.group(1))
        if seeds is not None and seed not in seeds:
            continue
        sdir = os.path.join(root, name, "saves")
        if not os.path.isdir(sdir):
            continue
        saves = sorted(f for f in os.listdir(sdir) if ".sav" in f)
        finals = [f for f in saves if "final" in f] or saves
        if finals:
            out[seed] = os.path.join(sdir, finals[-1])
    return out


# enum unit_activity, common/fc_types.h -- the savegame stores the number
ACTIVITY_NAMES = {
    "0": "Idle", "1": "Cultivate", "2": "Mine", "3": "Irrigate",
    "4": "Fortified", "5": "Sentry", "6": "Pillage", "7": "Goto",
    "8": "Explore", "9": "Transform", "10": "Fortifying", "11": "Clean",
    "12": "Base", "13": "Road", "14": "Convert", "15": "Plant",
}


def census(path):
    """Counters for one game: unit types, activities, veteran levels."""
    text = savequery.read_save(path)
    types, acts, vets, hp = Counter(), Counter(), Counter(), []
    total = 0
    for _plr, section in savequery._player_sections(text):
        for row in savequery._table(section, "u"):
            total += 1
            types[row.get("type_by_name", "?")] += 1
            act = row.get("activity", "?")
            acts["%s(%s)" % (ACTIVITY_NAMES.get(act, "?"), act)] += 1
            vets[row.get("veteran", "?")] += 1
            try:
                hp.append(int(row.get("hp", 0)))
            except ValueError:
                pass
    return {"total": total, "types": types, "activities": acts,
            "veteran": vets, "hp": hp}


def report(label, ref_name, ref, arm_name, arm, key, top=12):
    seeds = sorted(set(ref) & set(arm))
    rkeys = Counter()
    for s in seeds:
        rkeys.update(ref[s][key])
        rkeys.update(arm[s][key])
    print("  %s (%s vs %s, n=%d, per game)" %
          (label, arm_name, ref_name, len(seeds)))
    print("  %-16s %9s %9s %9s %7s %7s" %
          (key[:-1] if key.endswith("s") else key, ref_name[:9], arm_name[:9],
           "diff", "sem", "t"))
    for name, _ in rkeys.most_common(top):
        rv = [ref[s][key].get(name, 0) for s in seeds]
        av = [arm[s][key].get(name, 0) for s in seeds]
        dv = [a - b for a, b in zip(av, rv)]
        print("  %-16s %9.1f %9.1f %+9.1f %7.1f %+7.2f" %
              (name, mean(rv), mean(av), mean(dv), sem(dv), tstat(dv)))
    print()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True)
    ap.add_argument("--arm", required=True)
    ap.add_argument("--seeds", default=None)
    ap.add_argument("--top", type=int, default=12)
    args = ap.parse_args()

    seeds = parse_seeds(args.seeds)
    rname, rdir = args.ref.split("=", 1)
    aname, adir = args.arm.split("=", 1)
    rsaves = find_final_saves(rdir, seeds)
    asaves = find_final_saves(adir, seeds)
    common = sorted(set(rsaves) & set(asaves))
    if not common:
        sys.exit("no paired final saves under %s and %s" % (rdir, adir))
    ref = {s: census(rsaves[s]) for s in common}
    arm = {s: census(asaves[s]) for s in common}

    print("# armycensus: %s vs %s, %d paired final saves"
          % (aname, rname, len(common)))
    rt = [ref[s]["total"] for s in common]
    at = [arm[s]["total"] for s in common]
    dt = [a - b for a, b in zip(at, rt)]
    print("  units alive: %s %.1f, %s %.1f, diff %+.1f (t %+.2f)\n"
          % (rname, mean(rt), aname, mean(at), mean(dt), tstat(dt)))
    report("unit types", rname, ref, aname, arm, "types", args.top)
    report("activities", rname, ref, aname, arm, "activities", args.top)
    report("veteran levels", rname, ref, aname, arm, "veteran", args.top)


if __name__ == "__main__":
    main()
