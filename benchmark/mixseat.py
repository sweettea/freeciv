#!/usr/bin/env python3
"""Within-game deep-vs-classic seat comparison for mixed venues.

A single-module venue (all eight seats deep, or all eight baseline)
cannot tell "deep loses its fights" apart from "a population of deeps
fights less", because kills are a property of the whole game.  A mixed
game -- four deep seats, four classic seats, same map, same war -- can:
both sides face the same opponents, so a per-seat counter difference is
attributable.

Reads run-game.sh's mixed_dc/ and mixed_cd/ trees (P1,P3,P5,P7 deep in
mixed_dc; P2,P4,P6,P8 deep in mixed_cd), so seat position cancels when
both orderings are present.

    ./mixseat.py DIR [DIR ...] [--turn 201]
"""

import argparse
import os
import re
import sys

import scorelog
from wargap import mean, sem, tstat

TAGS = ["score", "cities", "pop", "techs", "munits", "settlers", "gold",
        "bnp", "mfg", "techout", "unitsbuilt", "unitskilled", "unitslost",
        "unitsused", "wonders", "culture", "landarea", "settledarea"]


def seat_roles(cmdpath):
    """{player-id: module} from the commands.serv `create Pn <module>`."""
    roles = {}
    for line in open(cmdpath, errors="replace"):
        m = re.match(r"create\s+P(\d+)\s+(\w+)", line.strip())
        if m:
            roles[int(m.group(1)) - 1] = m.group(2)
    return roles


def collect(dirs, turn):
    """[(seed, arm, {module: {tag: sum over that module's seats}})]"""
    out = []
    for root in dirs:
        for arm in ("mixed_dc", "mixed_cd"):
            adir = os.path.join(root, arm)
            if not os.path.isdir(adir):
                continue
            for name in sorted(os.listdir(adir)):
                m = re.fullmatch(r"seed-(\d+)", name)
                if not m:
                    continue
                gdir = os.path.join(adir, name)
                sl = os.path.join(gdir, "scorelog.txt")
                cs = os.path.join(gdir, "commands.serv")
                if not (os.path.exists(sl) and os.path.exists(cs)):
                    continue
                roles = seat_roles(cs)
                g = scorelog.Scorelog(sl)
                sides = {}
                for pid, mod in roles.items():
                    side = sides.setdefault(mod, {t: 0 for t in TAGS})
                    for tag in TAGS:
                        # last value <= turn: a dead seat keeps its final
                        # counters, matching tune.py's objective convention
                        v = g.last_value(tag, pid, turn)
                        side[tag] += v or 0
                    side["alive"] = side.get("alive", 0) + (
                        1 if g.alive_at(pid, turn)
                        and g.value("score", pid, turn) is not None else 0)
                out.append((int(m.group(1)), (root, arm), sides))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--turn", type=int, default=201)
    ap.add_argument("--deep", default="deep")
    ap.add_argument("--other", default="classic")
    args = ap.parse_args()

    rows = collect(args.dirs, args.turn)
    if not rows:
        sys.exit("no mixed games found")
    rows = [r for r in rows if args.deep in r[2] and args.other in r[2]]
    print("# mixseat: %d mixed games at T%d (%s vs %s, 4 seats each)"
          % (len(rows), args.turn, args.deep, args.other))
    print("  %-12s %10s %10s %10s %8s %7s %7s" %
          ("counter", args.deep[:10], args.other[:10], "diff", "sem", "t",
           "ratio"))
    for tag in TAGS + ["alive"]:
        d = [r[2][args.deep][tag] for r in rows]
        o = [r[2][args.other][tag] for r in rows]
        diff = [a - b for a, b in zip(d, o)]
        ratio = mean(d) / mean(o) if mean(o) else float("nan")
        print("  %-12s %10.1f %10.1f %+10.1f %8.1f %+7.2f %7.3f" %
              (tag, mean(d), mean(o), mean(diff), sem(diff), tstat(diff),
               ratio))


if __name__ == "__main__":
    main()
