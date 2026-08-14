#!/usr/bin/env python3
"""First-build turns for growth/capacity buildings (W03).

    firstbuild.py --root DIR [--arm ARM] [--picks A,B,...]

Parses deep-want lines (the final build decision per city per pass)
and reports, per game and pooled: the first turn each watched pick was
chosen and how many times it was chosen overall. Defaults to the W03
acceptance set; Supermarket count > 0 at all closes the README bug.
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

DEFAULT_PICKS = "Aqueduct,Granary,Harbour,Sewer System,Supermarket"
RE = re.compile(r'deep-want turn=(\d+) city=\d+ producer="[^"]*"'
                r' pick="([^"]*)"')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm")
    ap.add_argument("--picks", default=DEFAULT_PICKS)
    args = ap.parse_args()

    picks = args.picks.split(",")
    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    first = defaultdict(list)   # pick -> [first turn per game]
    count = defaultdict(int)
    games = 0
    for d in gamedirs:
        games += 1
        seen = {}
        for m in RE.finditer((d / "game.log").read_text(errors="replace")):
            turn, pick = int(m.group(1)), m.group(2)
            if pick in picks:
                count[pick] += 1
                if pick not in seen:
                    seen[pick] = turn
        for pick, turn in seen.items():
            first[pick].append(turn)

    print(f"{'pick':16} {'games_built':>11} {'median_first_turn':>17}"
          f" {'total_picks':>11}")
    for pick in picks:
        f = sorted(first[pick])
        med = f[len(f) // 2] if f else "-"
        print(f"{pick:16} {len(f):>3}/{games:<7} {str(med):>17}"
              f" {count[pick]:>11}")


if __name__ == "__main__":
    main()
