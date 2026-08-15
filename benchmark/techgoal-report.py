#!/usr/bin/env python3
"""Read the deep-techgoal counter and the deep-side research shares.

    techgoal-report.py TREE [TREE ...]

Each TREE is a result directory whose games live at
<arm>/seed-<n>/game.log (run.sh cells and tune.py --mixed trees alike;
arms are pooled). Two streams are read:

  deep-techgoal   one line per GATED deep player-turn (deeptech.c).
                  Reports the goals-differ rate (deep's reselected goal
                  vs stock's committed one -- the honesty pair logged on
                  the same line), the researching-moved rate, and the
                  military share of both sides' goals.
  deep-techwant   the slice-1 census line (deeptechcensus.c), for the
                  realised military-research share (resmil per readable
                  player-turn). On a gate-OFF tree only this stream
                  exists, which is what makes an off-vs-on share
                  comparison possible from the same reader.

A tree with no deep-techgoal lines is reported as gate-off rather than
an error.
"""
import glob
import os
import re
import sys

GOAL = re.compile(
    r'deep-techgoal turn=(\d+) player="([^"]*)" power=([-\d.]+)'
    r' goal="([^"]*)" goalmil=(-?\d+) goalsteps=(-?\d+) goalwant=([-\d.]+)'
    r' stockgoal="([^"]*)" stockgoalmil=(-?\d+) stockgoalsteps=(-?\d+)'
    r' stockgoalwant=([-\d.]+) differ=(\d)'
    r' res="([^"]*)" stockres="([^"]*)" resdiffer=(\d) switched=(\d)')

WANT = re.compile(
    r'deep-techwant turn=\d+ player="[^"]*" side="(\w+)" .* '
    r'res="[^"]*" resmil=(-?\d+) goal="[^"]*" goalmil=(-?\d+)')


def read_tree(tree):
    # run.sh cells: TREE/<arm>/seed-N; tune.py trees: TREE/cfg0/<arm>/seed-N.
    logs = sorted(glob.glob(os.path.join(tree, "*", "seed-*", "game.log"))
                  + glob.glob(os.path.join(tree, "*", "*", "seed-*",
                                           "game.log")))
    g = dict(lines=0, differ=0, resdiffer=0, switched=0,
             goalmil=0, goalmil_n=0, stockgoalmil=0, stockgoalmil_n=0,
             goalsteps=0.0, goalsteps_n=0,
             stockgoalsteps=0.0, stockgoalsteps_n=0)
    w = {}  # side -> [resmil_hits, resmil_n, goalmil_hits, goalmil_n]
    for path in logs:
        with open(path, errors="replace") as f:
            for line in f:
                m = GOAL.search(line)
                if m:
                    g["lines"] += 1
                    g["differ"] += int(m.group(12))
                    g["resdiffer"] += int(m.group(15))
                    g["switched"] += int(m.group(16))
                    for key, milgrp, stepgrp in (("goal", 5, 6),
                                                 ("stockgoal", 9, 10)):
                        mil = int(m.group(milgrp))
                        if mil >= 0:
                            g[key + "mil"] += mil
                            g[key + "mil_n"] += 1
                        steps = int(m.group(stepgrp))
                        if steps >= 0:
                            g[key + "steps"] += steps
                            g[key + "steps_n"] += 1
                    continue
                m = WANT.search(line)
                if m:
                    side = m.group(1)
                    s = w.setdefault(side, [0, 0, 0, 0])
                    if m.group(2) != "-1":
                        s[0] += int(m.group(2))
                        s[1] += 1
                    if m.group(3) != "-1":
                        s[2] += int(m.group(3))
                        s[3] += 1
    return len(logs), g, w


def pct(hits, n):
    return "%5.1f%% (n=%d)" % (100.0 * hits / n, n) if n else "    --"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for tree in sys.argv[1:]:
        nlogs, g, w = read_tree(tree)
        print("== %s  (%d game logs)" % (tree, nlogs))
        for side in sorted(w):
            s = w[side]
            print("   %-5s research mil share %s   goal mil share %s"
                  % (side, pct(s[0], s[1]), pct(s[2], s[3])))
        if g["lines"] == 0:
            print("   deep-techgoal: no lines (gate off)")
            continue
        n = g["lines"]
        print("   deep-techgoal: %d gated player-turns" % n)
        print("   goals differ   %s   researching moved %s   switched %s"
              % (pct(g["differ"], n), pct(g["resdiffer"], n),
                 pct(g["switched"], n)))
        print("   deep goal: mil %s  steps %.2f | stock goal: mil %s"
              " steps %.2f"
              % (pct(g["goalmil"], g["goalmil_n"]),
                 g["goalsteps"] / max(1, g["goalsteps_n"]),
                 pct(g["stockgoalmil"], g["stockgoalmil_n"]),
                 g["stockgoalsteps"] / max(1, g["stockgoalsteps_n"])))


if __name__ == "__main__":
    main()
