#!/usr/bin/env python3
"""Garrison-floor census from deep-floor log lines.

    floor.py --root DIR [--arm ARM] [--onset N] [--early N]

The floor is the sizing policy for most of deep's standing army (73-74%
of military city-turns sit at raw danger zero, E34.2/E34.2b) and it had
no counter until this census. One deep-floor line is emitted per
ELIGIBLE city-turn -- assessed danger zero after every fear term and
credit, garrison below the floor -- so:

  eligible    city-turns where the floor COULD fire
  applied     of those, the ones where it injected danger > 0

An arm with the floor switched off reports eligible > 0 and applied ==
0, which is what proves the off-switch executed rather than assuming it.

Fields are read as generic key=value pairs, so the class flags the
class-gated floor adds (barb=, sea=, air=, border=) are reported
automatically as soon as they appear on the line: any integer field is
summed and any field whose name is a known class is also reported as a
"live" share.
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

LINE = re.compile(r'deep-floor ((?:\w+=(?:"[^"]*"|\S+)\s*)+)')
KV = re.compile(r'(\w+)=("[^"]*"|\S+)')
CLASSES = ("barb", "sea", "air", "border")


def parse(gamedir):
    """-> list of dicts, one per eligible city-turn."""
    rows = []
    text = (gamedir / "game.log").read_text(errors="replace")
    for m in LINE.finditer(text):
        row = {}
        for k, v in KV.findall(m.group(1)):
            if v.startswith('"'):
                row[k] = v[1:-1]
            else:
                try:
                    row[k] = float(v) if "." in v else int(v)
                except ValueError:
                    row[k] = v
        rows.append(row)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm")
    ap.add_argument("--onset", type=int, default=60,
                    help="barbarian onset turn for the pre/post split")
    ap.add_argument("--early", type=int, default=40,
                    help="early-game window end turn")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    per_game = []
    classlive = defaultdict(int)
    classes_seen = []
    applied_hist = defaultdict(int)
    for d in gamedirs:
        rows = parse(d)
        elig = len(rows)
        app = sum(1 for r in rows if r.get("applied", 0) > 0)
        early = sum(1 for r in rows if r["turn"] <= args.early)
        early_app = sum(1 for r in rows
                        if r["turn"] <= args.early and r.get("applied", 0) > 0)
        pre = sum(1 for r in rows if r["turn"] < args.onset)
        pre_app = sum(1 for r in rows
                      if r["turn"] < args.onset and r.get("applied", 0) > 0)
        dsum = sum(r.get("applied", 0) for r in rows)
        for r in rows:
            applied_hist[r.get("applied", 0)] += 1
            for c in CLASSES:
                if c in r:
                    if c not in classes_seen:
                        classes_seen.append(c)
                    if r[c] > 0:
                        classlive[c] += 1
        per_game.append((d, elig, app, early, early_app, pre, pre_app, dsum))

    n = len(per_game)
    tot = [sum(g[i] for g in per_game) for i in range(1, 8)]
    print(f"games {n}   (root {args.root}, arm {args.arm or '*'})")
    print(f"  eligible city-turns/game       {tot[0]/n:10.1f}")
    print(f"  APPLIED  city-turns/game       {tot[1]/n:10.1f}"
          f"   ({100.0*tot[1]/max(tot[0],1):.1f}% of eligible)")
    print(f"  injected danger/game           {tot[6]/n:10.1f}")
    print(f"  eligible turn<={args.early:<4d}             {tot[2]/n:10.1f}"
          f"   applied {tot[3]/n:.1f}")
    print(f"  eligible turn<{args.onset:<4d} (pre-onset)  {tot[4]/n:10.1f}"
          f"   applied {tot[5]/n:.1f}")
    if classes_seen:
        print("  class live share of eligible city-turns:")
        for c in classes_seen:
            print(f"    {c:<8s} {classlive[c]/n:10.1f}/game"
                  f"   {100.0*classlive[c]/max(tot[0],1):5.1f}%")
    print("  applied-danger histogram (value: city-turns/game):")
    for v in sorted(applied_hist)[:12]:
        print(f"    {v:>5}: {applied_hist[v]/n:9.1f}")


if __name__ == "__main__":
    main()
