#!/usr/bin/env python3
"""NETTHREAT item 2 read: does a garrisoning enemy unit ever come at us?

Reads `deep-pin` lines only. Each is one observer's running funnel over
the game, so the LAST line per (seed, player) is that seat's total; the
corpus number is the sum over seats.

The funnel, weakest claim first:

    tagged  enemy units that were standing in one of their OWNER's
            cities at the moment we sized a garrison against them
    res     tags whose horizon window has closed (the denominator)
    left    ... that were ever seen off their home tile
    terr    ... that were ever seen inside our borders
    near    ... that ever reached a tile from which they could hit one
            of our cities -- the pin rate's numerator, and a superset
            of "attacked us"

`best` / `alone` are the sub-populations item 2's claim is really
about: the tagged unit was its city's best defender, or its only
military unit at all.

    pincensus.py RESULTS_DIR
"""

import glob
import re
import sys

FIELD = re.compile(r'(\w+)=("?[\w.\-]+"?)')
KEYS = ("tagged", "res", "left", "terr", "near", "home", "dead",
        "best", "best_near", "alone", "alone_near")


def main(root):
    last = {}

    for path in sorted(glob.glob(root + "/**/game.log", recursive=True)):
        seed = path.split("seed-")[1].split("/")[0] if "seed-" in path else "?"
        for line in open(path, errors="ignore"):
            if "deep-pin " not in line:
                continue
            d = dict(FIELD.findall(line))
            player = d.get("player", "").strip('"')
            last[(seed, player)] = {k: int(d.get(k, 0)) for k in KEYS}

    if not last:
        print("no deep-pin lines under", root)
        return 1

    agg = {k: 0 for k in KEYS}
    print("per seat (seed, player): tagged res left terr near home dead")
    for key in sorted(last):
        row = last[key]
        print("  %-14s %6d %6d %6d %6d %6d %6d %6d"
              % ("%s/%s" % key, row["tagged"], row["res"], row["left"],
                 row["terr"], row["near"], row["home"], row["dead"]))
        for k in KEYS:
            agg[k] += row[k]

    res = max(agg["res"], 1)
    print("\nseats=%d  tagged=%d  resolved=%d" %
          (len(last), agg["tagged"], agg["res"]))
    print("  left home      %6d  %6.2f%%" % (agg["left"], 100.0 * agg["left"] / res))
    print("  entered ours   %6d  %6.2f%%" % (agg["terr"], 100.0 * agg["terr"] / res))
    print("  REACHED A CITY %6d  %6.2f%%   <- the pin rate" %
          (agg["near"], 100.0 * agg["near"] / res))
    print("  never left     %6d  %6.2f%%" % (agg["home"], 100.0 * agg["home"] / res))
    print("  died in window %6d  %6.2f%%" % (agg["dead"], 100.0 * agg["dead"] / res))
    alive = max(agg["res"] - agg["dead"], 1)
    print("  pin rate excluding deaths from the denominator: %.2f%%"
          % (100.0 * agg["near"] / alive))
    print("\nsub-populations item 2's claim is about:")
    print("  best defender  res=%6d  near=%6d  %6.2f%%"
          % (agg["best"], agg["best_near"],
             100.0 * agg["best_near"] / max(agg["best"], 1)))
    print("  sole defender  res=%6d  near=%6d  %6.2f%%"
          % (agg["alone"], agg["alone_near"],
             100.0 * agg["alone_near"] / max(agg["alone"], 1)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "benchmark/results"))
