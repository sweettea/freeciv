#!/usr/bin/env python3
"""What n would this paired venue need to resolve an effect of size D?

    powern.py OFF_DIR ON_DIR [--metric score] [--arm deep] [--effect 3]

A pilot's job is not to find an effect; it is to price one. This takes
the per-seed paired differences the pilot actually produced, reports
their sd, and inverts the paired t statistic:

    se(n) = sd / sqrt(n),   |t| = D / se(n) >= 2   <=>   n >= (sd*2/D)^2

so the answer is n >= (2*sd/D)^2 rounded up. The same line is printed
for a few effect sizes, absolute and as a percentage of the OFF arm's
own mean, because "+3 score" means something different in a venue that
scores 166 and one that scores 100.

Two honest limits on any number this prints:

  * sd is estimated from the pilot's own n, and the sampling error on
    an sd at n=8 is about 25%, so the implied n is good to roughly a
    factor of 1.5 either way. It sizes a decision; it is not a
    guarantee.
  * sd depends on how hard the treatment shakes the game. A
    family-on-vs-all-off contrast perturbs far more than one marginal
    knob does, so this sd is an UPPER bound for the marginal contrasts
    a factorial would read, and a fair estimate for the family's own
    main effect.

The paired differences themselves are computed exactly as paircmp.py
computes them, including the P\\d+ seat filter that keeps civil-war
splinters out of an arm's mean.
"""
import csv
import math
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev


def metrics(root, arm, col):
    out = defaultdict(list)
    with open(Path(root) / "metrics.csv") as fh:
        for row in csv.DictReader(fh):
            if row["arm"] != arm or not re.fullmatch(r"P\d+", row["name"]):
                continue
            if row.get(col) not in (None, ""):
                out[int(row["seed"])].append(float(row[col]))
    return {s: mean(v) for s, v in out.items() if v}


def main():
    argv = sys.argv[1:]

    def opt(name, default):
        if name in argv:
            i = argv.index(name)
            v = argv[i + 1]
            del argv[i:i + 2]
            return v
        return default

    col = opt("--metric", "score")
    arm = opt("--arm", "deep")
    effects = [float(x) for x in opt("--effect", "3").split(",")]
    off, on = argv[0], argv[1]

    mo, mn = metrics(off, arm, col), metrics(on, arm, col)
    seeds = sorted(set(mo) & set(mn))
    if len(seeds) < 2:
        print("need at least 2 paired seeds, have %d" % len(seeds))
        return 1
    diffs = [mn[s] - mo[s] for s in seeds]
    n = len(diffs)
    m, sd = mean(diffs), stdev(diffs)
    se = sd / math.sqrt(n)
    base = mean(mo[s] for s in seeds)
    print("metric=%s  paired seeds n=%d" % (col, n))
    print("  OFF mean %.2f   ON mean %.2f   delta %+.2f"
          % (base, mean(mn[s] for s in seeds), m))
    print("  paired sd %.2f   se %.2f   t %+.2f   wins %d/%d"
          % (sd, se, (m / se) if se else float("nan"),
             sum(1 for d in diffs if d > 0), n))
    print("  per-seed diffs: %s"
          % " ".join("%+.1f" % d for d in diffs))
    print()
    print("  implied paired n for |t| >= 2, i.e. n >= (2*sd/D)^2:")
    for d in effects:
        need = math.ceil((2 * sd / d) ** 2)
        print("    D = %+5.1f  (%.1f%% of the OFF mean)   n >= %d "
              "  -> %d games both arms" % (d, 100 * d / base, need, 2 * need))
    return 0


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    sys.exit(main())
