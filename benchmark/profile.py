#!/usr/bin/env python3
"""Turn a profiling game's server log into a per-turn AI cost table.

    profile.py GAMELOG [GAMELOG ...] [--csv OUT.csv]

Reads the AISCALE and "AI timing results" lines that a build configured
with -Daitiming=true emits at the end of every turn, and reports:

  * the per-bucket share of total AI time over the run,
  * how AI time per turn grows with the number of cities, as a log-log
    slope -- the number the scaling argument actually turns on.

A slope near 1 means AI cost is linear in empire size and there is no
quadratic term to collapse. A slope near 2 means there is.

Pass several logs to pool their samples. One game is a poor slope
estimate on its own -- the empires stop growing once the map fills, so
the city count barely varies -- which is why the scale-20p family runs
the same map and turn count at different starting settler counts.

The timers are CPU-time timers and are GLOBAL, not per player: bucket
totals cover every AI in the game. AIT_ALL nests the others, so shares
are computed against AIT_ALL rather than against their sum.
"""

import argparse
import re
import sys

# "  Total AI time: 1.5 sec turn, 45.6 sec game" -- the label may be
# indented further and prefixed with "- " for nested buckets.
TIMING_RE = re.compile(
    r"^\s*(?:\d+:\s*)?\s*(-?\s*[A-Za-z][A-Za-z ]*?):\s*"
    r"([0-9.eE+-]+) sec turn,\s*([0-9.eE+-]+) sec game\s*$")
SCALE_RE = re.compile(r"AISCALE T(\d+) cities:(\d+) units:(\d+)")


def parse(path):
    """Return a list of per-turn dicts: turn, cities, units, buckets."""
    turns = []
    pending = None

    with open(path, errors="replace") as fh:
        for line in fh:
            m = SCALE_RE.search(line)
            if m:
                # AISCALE is emitted immediately before the bucket dump,
                # so it opens a new record.
                if pending is not None:
                    turns.append(pending)
                pending = {"turn": int(m.group(1)),
                           "cities": int(m.group(2)),
                           "units": int(m.group(3)),
                           "buckets": {}}
                continue

            if pending is None:
                continue

            m = TIMING_RE.match(line)
            if m:
                label = m.group(1).lstrip("- ").strip()
                pending["buckets"][label] = float(m.group(2))

    if pending is not None:
        turns.append(pending)
    return turns


def loglog_slope(xs, ys):
    """Least-squares slope of log(y) against log(x). None if degenerate."""
    import math

    pts = [(math.log(x), math.log(y))
           for x, y in zip(xs, ys) if x > 0 and y > 0]
    if len(pts) < 3:
        return None
    n = len(pts)
    mx = sum(p[0] for p in pts) / n
    my = sum(p[1] for p in pts) / n
    num = sum((p[0] - mx) * (p[1] - my) for p in pts)
    den = sum((p[0] - mx) ** 2 for p in pts)
    if den == 0:
        return None
    return num / den


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gamelog", nargs="+")
    ap.add_argument("--csv")
    args = ap.parse_args()

    turns = []
    for path in args.gamelog:
        got = parse(path)
        if not got:
            sys.exit("no AISCALE/timing records in %s -- was the server "
                     "built with -Daitiming=true?" % path)
        for t in got:
            t["log"] = path
        turns.extend(got)

    labels = []
    for t in turns:
        for k in t["buckets"]:
            if k not in labels:
                labels.append(k)

    if args.csv:
        with open(args.csv, "w") as fh:
            fh.write("log,turn,cities,units," + ",".join(labels) + "\n")
            for t in turns:
                fh.write("%s,%d,%d,%d," % (t.get("log", ""), t["turn"],
                                           t["cities"], t["units"]))
                fh.write(",".join("%g" % t["buckets"].get(k, 0.0)
                                  for k in labels) + "\n")

    total = {k: sum(t["buckets"].get(k, 0.0) for t in turns) for k in labels}
    all_time = total.get("Total AI time", 0.0)

    cities = [t["cities"] for t in turns]
    print("%d turn samples from %d log(s)   cities %d-%d   units %d-%d"
          % (len(turns), len(args.gamelog), min(cities), max(cities),
             min(t["units"] for t in turns), max(t["units"] for t in turns)))
    print("total AI CPU over the samples: %.1f sec" % all_time)
    print()
    print("%-22s %10s %8s %10s" % ("bucket", "sec", "%of AI", "slope"))

    for k in sorted(labels, key=lambda k: -total[k]):
        if total[k] <= 0:
            continue
        share = 100.0 * total[k] / all_time if all_time else float("nan")
        xs = [t["cities"] for t in turns]
        ys = [t["buckets"].get(k, 0.0) for t in turns]
        slope = loglog_slope(xs, ys)
        print("%-22s %10.1f %7.1f%% %10s"
              % (k, total[k], share,
                 "%.2f" % slope if slope is not None else "-"))

    print()
    print("slope is d log(bucket sec per turn) / d log(cities), fitted over")
    print("the run. ~1 linear in empire size, ~2 quadratic.")


if __name__ == "__main__":
    main()
