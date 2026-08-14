#!/usr/bin/env python3
"""Aggregate the deep-lift census: how much lift demand could SHARE a boat.

The 1:1 ferry cap counts settlers, not trips. A unit boarding a boat that
is already crossing costs nothing (civ2civ3's Transport has capacity 8 and
carries one passenger), so the question the lift operation has to answer
before it is worth building is: when the cap refuses a passenger, is there
a trip under way that it could have joined?

Reads game.log files and reports the ladder emitted by deep_lift_census():

    wait    units flagged waiting for a lift
    slot    ... that could ride some boat of ours with a free slot
    d0/d3/d5 ... with that boat 0 / within 3 / within 5 tiles away
    loaded  ... within 5 tiles of a boat that ALREADY has cargo aboard

Only player-turns where the 1:1 cap actually binds (psng >= boats) are
counted in the "capped" section -- that is the population the change
would act on.

Usage: liftcensus.py RESULTS_DIR [RESULTS_DIR ...]
"""

import re
import sys
from pathlib import Path

LINE = re.compile(
    r"deep-lift turn=(\d+) player=\"([^\"]+)\" boats=(\d+) avail=(\d+) "
    r"psng=(\d+) slots=(\d+) loadedboats=(\d+) wait=(\d+) slot=(\d+) "
    r"d0=(\d+) d3=(\d+) d5=(\d+) loaded=(\d+)"
)

FIELDS = ["turn", "player", "boats", "avail", "psng", "slots",
          "loadedboats", "wait", "slot", "d0", "d3", "d5", "loaded"]


def rows(paths):
    for root in paths:
        for log in Path(root).rglob("game.log"):
            for line in log.open(errors="replace"):
                m = LINE.search(line)
                if m:
                    r = dict(zip(FIELDS, m.groups()))
                    r["player"] = m.group(2)
                    for k in FIELDS:
                        if k != "player":
                            r[k] = int(r[k])
                    r["game"] = str(log.parent)
                    yield r


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = list(rows(sys.argv[1:]))
    if not data:
        sys.exit("no deep-lift lines found")

    games = len({r["game"] for r in data})
    print(f"{len(data)} player-turns over {games} games\n")

    def block(title, sel):
        sub = [r for r in sel]
        if not sub:
            print(f"{title}: none\n")
            return
        tot = {k: sum(r[k] for r in sub) for k in
               ("wait", "slot", "d0", "d3", "d5", "loaded", "boats", "slots",
                "loadedboats")}
        w = tot["wait"]
        print(f"{title}: {len(sub)} player-turns, {w} waiting unit-turns")
        if w:
            for k, label in (("slot", "a rideable boat with a free slot"),
                             ("d5", "... within 5 tiles"),
                             ("d3", "... within 3 tiles"),
                             ("d0", "... on the same tile"),
                             ("loaded", "... already carrying cargo (d<=5)")):
                print(f"  {k:8s} {tot[k]:7d}  {100.0*tot[k]/w:5.1f}%"
                      f"   {label}")
        pt = len(sub)
        print(f"  boats/turn {tot['boats']/pt:5.2f}   "
              f"free slots/turn {tot['slots']/pt:5.2f}   "
              f"loaded boats/turn {tot['loadedboats']/pt:5.2f}")
        print()

    block("ALL player-turns", data)
    block("CAP BINDING (psng >= boats, boats > 0)",
          [r for r in data if r["boats"] > 0 and r["psng"] >= r["boats"]])
    block("CAP BINDING with zero boats",
          [r for r in data if r["boats"] == 0])

    # The decisive number, stated on its own.
    capped = [r for r in data if r["boats"] > 0 and r["psng"] >= r["boats"]]
    cw = sum(r["wait"] for r in capped)
    cl = sum(r["loaded"] for r in capped)
    print("VERDICT")
    print(f"  waiting unit-turns under a binding cap : {cw}")
    print(f"  of those, a trip under way to join     : {cl}"
          f"  ({100.0*cl/cw:.1f}%)" if cw else "  (no capped turns)")


if __name__ == "__main__":
    main()
