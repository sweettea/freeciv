#!/usr/bin/env python3
"""Paired OFF/ON compare of the within-game deep-vs-other SEAT GAP.

    ./seatgap.py OFF_ROOT ON_ROOT [--arm mixed_dc] [--turn 201]

mixseat.py reports the gap for one corpus; paircmp.py pairs two corpora
on a single arm's absolute level. Neither answers the question an
expansion A/B asks, which is whether a CHANGE moved the gap:

    gap(seed)   = sum over deep seats - sum over other seats
    delta(seed) = gap_ON(seed) - gap_OFF(seed)

and delta is what gets the paired t. Pairing on the gap rather than on
deep's level cancels the map: both arms play the identical map from the
identical seed, and the other module is unchanged between them, so the
opponent side is a within-seed control as well as a between-arm one.

Four-seat SUMS per side, which is mixseat.py's convention and the one
the funnel's published gaps are in (mixed_dc -1.75, mixed_cd -4.56
cities at T201). Divide by four for a per-seat reading.

ROOT is a run.sh output directory (the one holding <config>-<key12>/),
or the config directory itself.
"""

import argparse
import os
import re
import sys

import scorelog
from wargap import mean, sem, tstat

TAGS = ["score", "cities", "pop", "settlers", "techs", "munits",
        "unitsbuilt", "unitskilled", "unitslost", "gold", "landarea",
        "wonders"]


def seat_roles(cmdpath):
    """{player-index: module} from commands.serv `create Pn <module>`."""
    roles = {}
    for line in open(cmdpath, errors="replace"):
        m = re.match(r"create\s+P(\d+)\s+(\w+)", line.strip())
        if m:
            roles[int(m.group(1)) - 1] = m.group(2)
    return roles


def armdirs(root, arm):
    """Every <root>/**/<arm> directory, so a run.sh out dir or a config
    dir both work."""
    found = []
    for dirpath, dirnames, _ in os.walk(root):
        if os.path.basename(dirpath) == arm:
            found.append(dirpath)
        dirnames[:] = [d for d in dirnames if not d.startswith("seed-")]
    return found


def gaps(root, arm, turn, deep, other):
    """{seed: {tag: four-seat gap}}"""
    out = {}
    for adir in armdirs(root, arm):
        for name in sorted(os.listdir(adir)):
            m = re.fullmatch(r"seed-(\d+)", name)
            if not m:
                continue
            g = os.path.join(adir, name)
            sl, cs = (os.path.join(g, "scorelog.txt"),
                      os.path.join(g, "commands.serv"))
            if not (os.path.exists(sl) and os.path.exists(cs)):
                continue
            roles = seat_roles(cs)
            log = scorelog.Scorelog(sl)
            side = {deep: {}, other: {}}
            for pid, mod in roles.items():
                if mod not in side:
                    continue
                for tag in TAGS:
                    side[mod].setdefault(tag, []).append(
                        log.last_value(tag, pid, turn) or 0)
            if not side[deep] or not side[other]:
                continue
            out[int(m.group(1))] = {
                t: sum(side[deep][t]) - sum(side[other][t]) for t in TAGS}
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("off")
    ap.add_argument("on")
    ap.add_argument("--arm", default="mixed_dc")
    ap.add_argument("--turn", type=int, default=201)
    ap.add_argument("--deep", default="deep")
    ap.add_argument("--other", default="classic")
    args = ap.parse_args()

    o = gaps(args.off, args.arm, args.turn, args.deep, args.other)
    n = gaps(args.on, args.arm, args.turn, args.deep, args.other)
    seeds = sorted(set(o) & set(n))
    if not seeds:
        sys.exit("no paired seeds (off %d, on %d)" % (len(o), len(n)))

    print("# seatgap %s at T%d: %d paired seeds (off %d, on %d)"
          % (args.arm, args.turn, len(seeds), len(o), len(n)))
    print("# gap = sum over %s seats - sum over %s seats, within game"
          % (args.deep, args.other))
    print("  %-12s %10s %10s %10s %8s %7s %8s"
          % ("counter", "gapOFF", "gapON", "delta", "sem", "t", "wins"))
    for tag in TAGS:
        d = [n[s][tag] - o[s][tag] for s in seeds]
        print("  %-12s %10.2f %10.2f %+10.2f %8.2f %+7.2f %8s"
              % (tag, mean([o[s][tag] for s in seeds]),
                 mean([n[s][tag] for s in seeds]), mean(d), sem(d),
                 tstat(d), "%d/%d" % (sum(1 for x in d if x > 0), len(d))))


if __name__ == "__main__":
    main()
