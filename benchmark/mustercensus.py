#!/usr/bin/env python3
"""ERA 3b liveness read: the muster counters off a corpus of game.logs.

Reads `deep-opodds` lines only. Prints the launch/abandon census the
brief asks for (a mechanism that musters forever and never launches is
a unit sink and this is what makes it visible), the abandon reasons,
the accumulation histogram, and the odds the held force reached.

    mustercensus.py RESULTS_DIR
"""

import collections
import glob
import re
import sys

FIELD = re.compile(r'(\w+)=("?[\w.\-]+"?)')


def main(root):
    totals = {}
    sizes = collections.Counter()
    reasons = collections.Counter()
    verdicts = collections.Counter()
    best_max = 0
    hold_odds = []

    for path in sorted(glob.glob(root + "/**/game.log", recursive=True)):
        seed = path.split("seed-")[1].split("/")[0] if "seed-" in path else "?"
        for line in open(path, errors="ignore"):
            if "deep-opodds" not in line:
                continue
            d = dict(FIELD.findall(line))
            player = d.get("player", "").strip('"')
            verdict = d.get("verdict", "").strip('"')
            verdicts[verdict] += 1
            if "mform" in d:
                totals[(seed, player)] = tuple(
                    int(d[k]) for k in ("mform", "mlaunch", "mabandon"))
            if verdict == "muster-abandon":
                reasons[d.get("why", "?")] += 1
            if verdict == "muster-hold":
                best_max = max(best_max, int(d.get("best", 0)))
                sizes[int(d.get("members", 0))] += 1
                hold_odds.append(float(d.get("odds", 0)))

    print("verdicts:", dict(sorted(verdicts.items())))
    print("\nper seat (seed, player): mform mlaunch mabandon")
    agg = [0, 0, 0]
    for key in sorted(totals):
        row = totals[key]
        print("  %-12s %4d %4d %4d" % (key, row[0], row[1], row[2]))
        agg = [a + b for a, b in zip(agg, row)]
    print("TOTAL mform=%d mlaunch=%d mabandon=%d"
          "  (launch %.1f%%, abandon %.1f%%)"
          % (agg[0], agg[1], agg[2],
             100.0 * agg[1] / max(agg[0], 1),
             100.0 * agg[2] / max(agg[0], 1)))
    print("\nabandon reasons:", dict(reasons))
    print("hold member-count histogram:", dict(sorted(sizes.items())))
    print("max high-water members:", best_max)
    nonzero = [o for o in hold_odds if o > 0]
    print("hold lines %d, odds>0 on %d (%.1f%%)"
          % (len(hold_odds), len(nonzero),
             100.0 * len(nonzero) / max(len(hold_odds), 1)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "benchmark/results/muster-live")
