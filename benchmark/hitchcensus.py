#!/usr/bin/env python3
"""Aggregate the deep-hitch census: how much demand could JOIN a lift.

The lift makes crossings happen; hitch-a-ride would let a second unit
board a boat already servicing a first one bound for the same continent.
The trip-counting version of that idea was killed at 1.6% by a census
taken when no trips existed. The lift changed that denominator, so the
question is re-asked here -- BEFORE building the manifest -- against the
ladder deep_hitch_census() emits, per unit-turn where a unit wants a
boat, is unclaimed by any operation, and its target is overseas:

    want      the population hitch-a-ride could act on at all
    anyop     ... while a live lift's boat has a free slot it could load
    cont      ... and that lift's beachhead is on its OWN target's
                  continent -- the passage it wants is already running
    muster    ... and that lift has not sailed yet. The design gate is
                  "detour < one extra turn of waiting", and only a boat
                  still gathering can plausibly clear it
    d0/d3/d5  ... with that boat 0 / within 3 / within 5 tiles

`muster` is the decisive rung: it is the demand a cheap hitch-a-ride
would admit. `d0` is the tripwire -- if it equals d5 the load predicate
has collapsed to same-tile and no rung means anything.

`opfull` reports the turns where NO slot a lift may form into was free
(the army's four plus lift_op_slots of the lift-only range). If lifts
queue behind that bound, the ceiling is storage, not the manifest.

The hull rungs answer a different question: how often the AI carries
more than one unit at a time, and whether the boat could. Capacity is
not uniform -- civ2civ3's Trireme carries 2 against the Transport's 8 --
so `caple2` is what keeps "no room" from being read as "no demand" on
a cell that starts at low tech.

Usage: hitchcensus.py RESULTS_DIR [RESULTS_DIR ...]
"""

import re
import sys
from pathlib import Path

LINE = re.compile(
    r"deep-hitch turn=(\d+) player=\"([^\"]+)\" ops=(\d+) lifts=(\d+) "
    r"opfull=(\d+) liftslots=(\d+) want=(\d+) anyop=(\d+) cont=(\d+) "
    r"muster=(\d+) d0=(\d+) d3=(\d+) d5=(\d+)"
    # Hull rungs were added later; older logs stop at d5 and parse with
    # these reading zero rather than dropping out of the census.
    r"(?: boats=(\d+) caple2=(\d+) carry1=(\d+) carry2p=(\d+))?"
)

FIELDS = ["turn", "player", "ops", "lifts", "opfull", "liftslots",
          "want", "anyop", "cont", "muster", "d0", "d3", "d5",
          "boats", "caple2", "carry1", "carry2p"]


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
                            r[k] = int(r[k]) if r[k] is not None else 0
                    r["game"] = str(log.parent)
                    yield r


def block(title, sub):
    if not sub:
        print(f"{title}: none\n")
        return
    keys = ("want", "anyop", "cont", "muster", "d0", "d3", "d5",
            "ops", "lifts", "opfull", "liftslots",
            "boats", "caple2", "carry1", "carry2p")
    tot = {k: sum(r[k] for r in sub) for k in keys}
    w = tot["want"]
    print(f"{title}: {len(sub)} player-turns, {w} waiting unit-turns")
    if w:
        for k, label in (("anyop", "a live lift with a free slot"),
                         ("cont", "... bound for their own continent"),
                         ("muster", "... and still gathering (joinable)"),
                         ("d5", "... within 5 tiles"),
                         ("d3", "... within 3 tiles"),
                         ("d0", "... on the same tile [TRIPWIRE]")):
            print(f"  {k:8s} {tot[k]:7d}  {100.0*tot[k]/w:5.1f}%   {label}")
    pt = len(sub)
    b = tot["boats"]
    print(f"  boats/turn {b/pt:5.2f}   hulls capacity<=2 "
          f"{(100.0*tot['caple2']/b) if b else 0:5.1f}%   "
          f"carrying 1 {(100.0*tot['carry1']/b) if b else 0:5.1f}%   "
          f"carrying 2+ {(100.0*tot['carry2p']/b) if b else 0:5.1f}%")
    print(f"  live ops/turn {tot['ops']/pt:5.2f}   "
          f"lifts/turn {tot['lifts']/pt:5.2f}   "
          f"free lift slots/turn {tot['liftslots']/pt:5.2f}   "
          f"op array full {100.0*tot['opfull']/pt:5.1f}%")
    print()


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = list(rows(sys.argv[1:]))
    if not data:
        sys.exit("no deep-hitch lines found")

    games = len({r["game"] for r in data})
    print(f"{len(data)} player-turns over {games} games\n")

    block("ALL player-turns", data)
    block("A LIFT IS RUNNING (lifts > 0)", [r for r in data if r["lifts"]])

    live = [r for r in data if r["lifts"]]
    w = sum(r["want"] for r in live)
    m = sum(r["muster"] for r in live)
    print("VERDICT")
    print(f"  unclaimed overseas-bound waiters, lift running : {w}")
    if w:
        print(f"  of those, a gathering lift they could join    : {m}"
              f"  ({100.0*m/w:.1f}%)")
    else:
        print("  (no such unit-turns -- there is nothing to hitch onto)")


if __name__ == "__main__":
    main()
