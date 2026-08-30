#!/usr/bin/env python3
"""Read deep-sitewall: what deep's own border wall hides from its settlers.

    ./benchmark/wallcensus.py DIR [DIR ...]

DIR is any tree containing seed-*/game.log[.zst] (an arm directory, a
cell, a whole venue). One table per DIR, banded by 25 turns.

WHAT THE LINE IS. deep's site search pathfinds with
deep_settler_tile_behavior(), which returns TB_IGNORE for every tile
whose owner is not pplayers_allied() -- and path_finding.c:288 drops a
TB_IGNORE node before it can be reached, entered, or offered as a
destination.  Free land BEHIND a peace partner's border is therefore
not refused by the search, it is invisible to it, and every bucket on
deep-siteladder is already post-truncation.  Stock has no such wall:
find_best_city_placement()'s phase 1 sets no get_TB at all
(ai/baseline/default/daisettler.c:913).

deep-sitewall is the counterfactual, taken once per player-turn on a
search that came home EMPTY: the same pf map with the ownership test
replaced by deep_frontier_reachable() (peace partners open, war and
cease-fire still shut), and then

    wseen    tiles the open map offers
    wextra   ... that the walled map did not
    wland    ... on our own continent and not TER_NO_CITIES
    wfree    ... that city_can_be_built_here() actually permits, so
             ownership, citymindist and terrain are all honoured
    wbest    the best RAW site value among a bounded nearest-first
             sample of wfree, and wturns its travel time

`wbest` is raw on purpose.  The net is the reader's to compute -- it is
the same amortize(total, d*turns) the search applies, and duplicating
that arithmetic inside the census would make it a second implementation
of the price rather than a measurement of the opportunity.  The `net`
column below applies the shipped default d = 3 (site_travel_discount)
so the two are comparable at a glance; pass --discount to change it.
"""

import argparse
import collections
import glob
import os
import re
import subprocess
import sys

BANDS = [(1, 25), (26, 50), (51, 75), (76, 100), (101, 125), (126, 150)]
MORT = 24


def band(turn):
    for lo, hi in BANDS:
        if lo <= turn <= hi:
            return (lo, hi)
    return None


def lines(path):
    if path.endswith(".zst"):
        proc = subprocess.Popen(["zstdcat", path], stdout=subprocess.PIPE,
                                text=True, errors="replace")
        with proc.stdout as fh:
            for line in fh:
                yield line
        proc.wait()
    else:
        with open(path, errors="replace") as fh:
            for line in fh:
                yield line


def read(root):
    per = collections.defaultdict(lambda: collections.defaultdict(float))
    n = collections.defaultdict(int)
    pos = collections.defaultdict(int)
    games = 0
    for logdir in sorted(glob.glob(os.path.join(root, "**", "seed-*"),
                                   recursive=True)):
        cand = [p for p in (os.path.join(logdir, "game.log"),
                            os.path.join(logdir, "game.log.zst"))
                if os.path.exists(p)]
        if not cand:
            continue
        games += 1
        for line in lines(cand[0]):
            if "deep-sitewall " not in line:
                continue
            d = dict(re.findall(r"(\w+)=(-?[\d.]+)", line))
            b = band(int(d["turn"]))
            if b is None:
                continue
            n[b] += 1
            if float(d["wfree"]) > 0:
                pos[b] += 1
            for k in ("seen", "wseen", "wextra", "wland", "wfree",
                      "wscored", "wbest"):
                if k in d:
                    per[b][k] += float(d[k])
            if float(d["wbest"]) > 0:
                # wturns is -1 when nothing scored; averaging that in
                # would make the travel column report a discount on
                # searches that found nothing to discount.
                per[b]["wturns"] += float(d["wturns"])
                per[b]["nturns"] += 1
            if float(d["wbest"]) > per[b]["maxbest"]:
                per[b]["maxbest"] = float(d["wbest"])
                per[b]["maxturns"] = float(d["wturns"])
    return games, n, pos, per


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--discount", type=float, default=3.0,
                    help="site_travel_discount to net wbest with (default 3)")
    args = ap.parse_args()

    for root in args.dirs:
        games, n, pos, per = read(root)
        total = sum(n.values())
        print("### %s  (%d games, %d wall censuses)" % (root, games, total))
        if not total:
            print("    no deep-sitewall lines")
            continue
        print("band      n   empty-with-land%   seen  wseen wextra  wland"
              "  wfree   wbest  wturns     net")
        for b in BANDS:
            if not n[b]:
                continue
            c = float(n[b])
            wb = per[b]["wbest"] / c
            wt = (per[b]["wturns"] / per[b]["nturns"]
                  if per[b]["nturns"] else 0.0)
            net = wb * ((MORT - 1.0) / MORT) ** (args.discount * max(wt, 0.0))
            print("%3d-%3d %4d %13.1f%% %8.1f %6.1f %6.1f %6.1f %6.2f"
                  " %7.1f %7.1f %7.1f"
                  % (b[0], b[1], n[b], 100.0 * pos[b] / c,
                     per[b]["seen"] / c, per[b]["wseen"] / c,
                     per[b]["wextra"] / c, per[b]["wland"] / c,
                     per[b]["wfree"] / c, wb, wt, net))
        print()


if __name__ == "__main__":
    sys.exit(main())
