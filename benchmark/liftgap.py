#!/usr/bin/env python3
"""Read deep-liftgap: settlement demand against lift supply, per water.

    liftgap.py RESULTS_DIR [RESULTS_DIR ...]

One line per water per player-turn, written by deep_lift_gap_census()
(ai/deep/deepops.c). The question it answers is the one EXPANSION
slice 2 is aimed at: after t20 every founder want deep forms is an
overseas want, so is the lift queue actually short of hulls, and is
the wait the site scorer charges backed by a real shortage?

    founders  founders waiting to cross this water, unclaimed
    waiting   every unit waiting (deep_ferry_demand_on's population)
    idle      idle hulls; berths their capacity
    building  berths our cities on this water have in production
    gap       founders - berths - building
    wait      deep_wait_expected() for the same water
    colony    dp->colony_want when fresh, else 0
"""
import re
import sys
from pathlib import Path

LINE = re.compile(r'deep-liftgap (?P<kv>.*)$')
COMM = re.compile(r'deep-commission (?P<kv>.*)$')
INVALID = re.compile(r'deep-lift-invalid .* phase=(\w+)')


def read(root):
    rows, comm, inval = [], [], []
    for log in sorted(Path(root).rglob("game.log")):
        for line in open(log, errors="ignore"):
            m = LINE.search(line)
            if m:
                d = dict(re.findall(r'(\w+)=(-?\d+)', m.group('kv')))
                d['seed'] = log.parent.name
                rows.append(d)
                continue
            m = COMM.search(line)
            if m:
                d = dict(re.findall(r'(\w+)=(-?[\d.]+)', m.group('kv')))
                d['seed'] = log.parent.name
                comm.append(d)
                continue
            m = INVALID.search(line)
            if m:
                inval.append((log.parent.name, m.group(1)))
    return rows, comm, inval


def main(argv):
    rows, comm, inval = [], [], []
    for root in argv[1:] or ["benchmark/results"]:
        a, b, c = read(root)
        rows += a
        comm += b
        inval += c
    if not rows:
        print("no deep-liftgap lines")
        return 1

    print("water-turns: %d over %d seeds, turns %d-%d"
          % (len(rows), len({r['seed'] for r in rows}),
             min(int(r['turn']) for r in rows),
             max(int(r['turn']) for r in rows)))
    print("%-10s %8s %8s %8s %8s" % ("field", ">0", "max", "mean", "sum"))
    for k in ("founders", "waiting", "idle", "berths", "building", "wait",
              "colony"):
        v = [int(r[k]) for r in rows]
        print("%-10s %8d %8d %8.2f %8d"
              % (k, sum(1 for x in v if x > 0), max(v), sum(v) / len(v),
                 sum(v)))

    gaps = [int(r['gap']) for r in rows]
    pos = [r for r in rows if int(r['gap']) > 0]
    print("\ngap > 0 on %d/%d water-turns (max %+d, min %+d)"
          % (len(pos), len(rows), max(gaps), min(gaps)))
    print("gap > 0 AND the water bridges: %d"
          % sum(1 for r in pos if r['bridge'] == '1'))
    print("gap > 0 AND wait > 0:          %d"
          % sum(1 for r in pos if int(r['wait']) > 0))
    print("founders > 0 anywhere:         %d"
          % sum(1 for r in rows if int(r['founders']) > 0))
    print("wait > 0 with founders == 0:   %d"
          % sum(1 for r in rows if int(r['wait']) > 0
                and int(r['founders']) == 0))
    print("\ndeep-lift-invalid: %d march, %d muster"
          % (sum(1 for _, p in inval if p == "march"),
             sum(1 for _, p in inval if p == "muster")))
    if comm:
        beat = sum(1 for c in comm if c.get('beat') == '1')
        print("deep-commission: %d firings, %d beat the incumbent choice"
              % (len(comm), beat))
        print("   mean gap %.2f, wait %.2f, n %.2f, want %.1f"
              % tuple(sum(float(c[k]) for c in comm) / len(comm)
                      for k in ("gap", "wait", "n", "want")))
    else:
        print("deep-commission: no firings (knob off, or no gap)")

    for r in rows:
        if int(r['founders']) > 0:
            print("   " + " ".join("%s=%s" % (k, r[k]) for k in
                                   ("seed", "turn", "ocean", "bridge",
                                    "founders", "waiting", "idle", "berths",
                                    "building", "gap", "wait", "colony")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
