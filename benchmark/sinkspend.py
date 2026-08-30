#!/usr/bin/env python3
"""Where do the shields the opportunity price frees actually land?

    sinkspend.py VENUE_DIR BAND_LO BAND_HI

Delivered shields per seat per game, by contentcensus class, for every
cell, over one 25-turn band. The fear verdict inferred that the freed
military shields "reach the founder queue and evaporate before T75";
that inference was made from the city curve, not from the ledger. This
prints the ledger.
"""
import re
import sys
from collections import defaultdict, Counter
from pathlib import Path
from statistics import mean, stdev

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from contentcensus import (  # noqa: E402
    scorelog, games, loglines, ruleset_upkeep, ruleset_unit_cost,
    CLASSES, R_SWITCH, BUCKET, SKIP,
)

CELLS = ("c00", "c10", "c01", "c11", "boat")


def per_seed(cdir, rules, lo, hi):
    out = {}
    for d in games(cdir / "deep"):
        sl, players = scorelog(d / "scorelog.txt")
        seats = [n for n in players.values() if re.fullmatch(r"P\d+", n)]
        n = max(1, len(seats))
        got = Counter()
        for ln in loglines(d):
            if "deep-prodswitch " not in ln:
                continue
            m = R_SWITCH.search(ln)
            if not m or m.group(11) != "1" or not m.group(4):
                continue
            t = int(m.group(1))
            if not (lo <= t <= hi):
                continue
            got[m.group(4)] += 1
        row = defaultdict(float)
        named = set()
        for cls, members in CLASSES.items():
            named.update(members)
            row[cls] = sum(got[x] * rules.get(x, (0, 0))[0]
                           for x in members) / n
        row["other"] = sum(v * rules.get(k, (0, 0))[0]
                           for k, v in got.items()
                           if k not in named and k not in SKIP) / n
        row["TOTAL"] = sum(row[c] for c in list(CLASSES) + ["other"])
        out[int(d.name.split("-")[-1])] = row
    return out


def main():
    venue = Path(sys.argv[1])
    lo, hi = int(sys.argv[2]), int(sys.argv[3])
    rules = ruleset_upkeep(HERE.parent / "data" / "civ2civ3")
    rules.update({k: v for k, v
                  in ruleset_unit_cost(HERE.parent / "data" / "civ2civ3").items()
                  if k not in rules})
    cells = {c: per_seed(venue / c, rules, lo, hi)
             for c in CELLS if (venue / c).is_dir()}
    cells = {c: v for c, v in cells.items() if v}
    keys = list(CLASSES) + ["other", "TOTAL"]
    print(f"\n{venue.name} T{lo}-{hi}: delivered shields per seat per game")
    print(f"  {'class':<12}" + "".join(f"{c:>10}" for c in cells)
          + f"{'c10-c00':>12}{'se':>8}")
    base = cells["c00"]
    for k in keys:
        row = "".join(f"{mean([r[k] for r in cells[c].values()]):>10.2f}"
                      for c in cells)
        seeds = sorted(set(base) & set(cells.get("c10", {})))
        if seeds:
            d = [cells["c10"][s][k] - base[s][k] for s in seeds]
            se = stdev(d) / len(d) ** 0.5 if len(d) > 1 else 0.0
            row += f"{mean(d):>+12.2f}{se:>8.2f}"
        print(f"  {k:<12}" + row)


main()
