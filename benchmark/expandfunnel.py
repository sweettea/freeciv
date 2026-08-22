#!/usr/bin/env python3
"""Read ai/deep's founder funnel (`deep-expand` lines) out of game logs.

    ./expandfunnel.py DIR [DIR ...] [--windows 1,20,40,60,80,120,201]

DIR is anything containing seed-*/game.log (or game.log.gz) -- a
run.sh arm directory, or the whole config directory.

One `deep-expand` line per deep player per turn carries the whole
build-side founder chain (see ai/deep/deepexpand.h). This script sums
the stages over players and games inside turn windows, which is the
only aggregation that keeps the arithmetic self-checks readable:

    cities = gate + norole + fresh + stale
    auc    = win + lostdom + lostmil

and prints the veto ranking -- every way a founder want can fail to
become a settler, ordered by how many city-turns it took.
"""

import argparse
import collections
import gzip
import os
import re
import sys

FIELDS = ["cities", "gate", "norole", "fresh", "stale", "w0", "wpos",
          "wneg", "held", "vfood", "vwonder", "vcap", "skipwar",
          "skipemerg", "auc", "win", "lostdom", "lostmil", "lostmilt",
          "wantsum", "bidsum", "lostsum", "prod", "born", "alive",
          "found", "sites", "freetiles", "freeshare"]

LINE = re.compile(r"deep-expand turn=(\d+) player=\"([^\"]*)\" (.*)$")


def logs(dirs):
    for root in dirs:
        for dirpath, _, names in os.walk(root):
            for n in names:
                if n in ("game.log", "game.log.gz"):
                    yield os.path.join(dirpath, n)


def read(path):
    """[(turn, player, {field: float})] for one game."""
    op = gzip.open if path.endswith(".gz") else open
    rows = []
    with op(path, "rt", errors="replace") as fh:
        for line in fh:
            i = line.find("deep-expand turn=")
            if i < 0:
                continue
            m = LINE.match(line[i:])
            if not m:
                continue
            d = {k: float(v)
                 for k, v in re.findall(r"(\w+)=([-\d.]+)", m.group(3))}
            rows.append((int(m.group(1)), m.group(2), d))
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--windows", default="1,20,40,60,80,120,201")
    args = ap.parse_args()

    edges = [int(x) for x in args.windows.split(",")]
    paths = sorted(logs(args.dirs))
    if not paths:
        sys.exit("no game logs found")

    # window index -> field -> sum;  and the per-player-turn count
    acc = [collections.Counter() for _ in edges]
    n = [0] * len(edges)
    games = 0
    for p in paths:
        rows = read(p)
        if not rows:
            continue
        games += 1
        for turn, _player, d in rows:
            w = 0
            for i, e in enumerate(edges):
                if turn >= e:
                    w = i
            acc[w].update({k: d.get(k, 0.0) for k in FIELDS})
            n[w] += 1

    print("# expandfunnel: %d games, %d player-turns"
          % (games, sum(n)))
    print("# counts are per PLAYER-TURN means inside the window")
    hdr = ["window", "n"] + FIELDS
    print("  " + " ".join("%9s" % h for h in hdr))
    for i, e in enumerate(edges):
        hi = edges[i + 1] - 1 if i + 1 < len(edges) else 999
        if n[i] == 0:
            continue
        row = ["%d-%d" % (e, hi), "%d" % n[i]]
        for f in FIELDS:
            row.append("%.2f" % (acc[i][f] / n[i]))
        print("  " + " ".join("%9s" % c for c in row))

    # Whole-run veto ranking: every stage that consumed a city-turn
    # which could otherwise have carried a founder bid to an auction.
    tot = collections.Counter()
    for a in acc:
        tot.update(a)
    print("\n# veto ranking (city-turns over the whole run, "
          "%d city-turns total)" % int(tot["cities"]))
    order = [
        ("gate      expansion_war_survival / war footing", tot["gate"]),
        ("skipwar   domestic advisor skipped: war footing", tot["skipwar"]),
        ("skipemerg domestic advisor skipped: emergency tier",
         tot["skipemerg"]),
        ("w0        site search found nothing", tot["w0"]),
        ("norole    no ACTION_FOUND_CITY unit", tot["norole"]),
        ("vfood     food surplus below founder upkeep", tot["vfood"]),
        ("vcap      city-count divisor (/100)", tot["vcap"]),
        ("vwonder   wonder-city divisor", tot["vwonder"]),
        ("lostdom   domestic advisor preferred another bid",
         tot["lostdom"]),
        ("lostmil   military choice beat the founder", tot["lostmil"]),
        ("lostmilt  ... of those, lost on TIER not value",
         tot["lostmilt"]),
    ]
    for name, v in sorted(order, key=lambda kv: -kv[1]):
        share = 100.0 * v / tot["cities"] if tot["cities"] else 0.0
        print("  %-46s %9d  %5.1f%%" % (name, int(v), share))
    print("  %-46s %9d" % ("win       founder won the auction", tot["win"]))
    print("  %-46s %9d" % ("found     cities founded", tot["found"]))


if __name__ == "__main__":
    main()
