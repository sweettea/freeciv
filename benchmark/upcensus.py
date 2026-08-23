#!/usr/bin/env python3
"""deep-trunkup, summed per game.

One line per seed of a result directory, so the rail-upgrade epic's
liveness can be pasted rather than described. The fields are the census
line's own (deeptrunk.c):

  whole   links whose every tile already carries a road. Counted in BOTH
          settings of trunk_upgrade_gate, so it is the denominator, and
          on its own it is the venue statement: whole=0 says this venue
          never finishes a trunk and no upgrade could be claimed here.
  links   the whole links that entered the UPGRADE phase. Zero by
          construction with the gate shut.
  gap     their tiles below the best road the player could lay.
  hits    upgrade candidates the advisor's task-value hook priced.
  off     upgrade candidates deep's own task source priced, beyond the
          advisor's enumeration ceiling.
  pick    assignments actually made ON A TILE THAT ALREADY CARRIES A
          ROAD -- the job that cannot exist below the gate.

Every field is a per-player-turn count, so the sums are player-turns of
demand and not distinct tiles; `lines` is the denominator for them.
"""

import os
import re
import sys

PAT = re.compile(r'deep-trunkup .*whole=(\d+) links=(\d+) gap=(\d+) '
                 r'hits=(\d+) offered=(\d+) picked=(\d+) maxwant=([0-9.]+)')

FIELDS = ("whole", "links", "gap", "hits", "off", "pick")


def game_logs(root):
    """Every (seed, path) under a result dir, whatever arm layout it has."""
    for arm in sorted(os.listdir(root)):
        adir = os.path.join(root, arm)
        if not os.path.isdir(adir):
            continue
        for seed in sorted(os.listdir(adir),
                           key=lambda s: (len(s), s)):
            log = os.path.join(adir, seed, "game.log")
            if os.path.exists(log):
                yield "%s/%s" % (arm, seed), log


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    grand = [0] * 6
    grand_lines = 0
    games = 0
    print("%-18s %8s %8s %8s %8s %8s %8s %8s %9s"
          % ("game", "lines", *FIELDS, "maxwant"))
    for name, log in game_logs(sys.argv[1]):
        tot = [0] * 6
        lines = 0
        peak = 0.0
        with open(log, errors="replace") as fh:
            for line in fh:
                m = PAT.search(line)
                if m is None:
                    continue
                lines += 1
                for i in range(6):
                    tot[i] += int(m.group(i + 1))
                peak = max(peak, float(m.group(7)))
        print("%-18s %8d %8d %8d %8d %8d %8d %8d %9.2f"
              % (name, lines, *tot, peak))
        games += 1
        grand_lines += lines
        for i in range(6):
            grand[i] += tot[i]
    if games:
        print("%-18s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f"
              % ("MEAN/game", grand_lines / games,
                 *[t / games for t in grand]))
    else:
        print("no game.log with a deep-trunkup line under", sys.argv[1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
