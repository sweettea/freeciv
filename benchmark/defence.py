#!/usr/bin/env python3
"""Defence bookkeeping metrics from deep-def log lines (W11).

    defence.py --root DIR [--arm ARM]

Walks DIR for */seed-*/DONE games and parses the per-city deep-def
lines ai/deep logs every turn (raw assessed danger, on-tile defence
power, military headcount, current build target).

Reported per game and pooled:

  excess      mean surplus defenders per city-turn. A city-turn's
              surplus is ndef * (P - D) / P clamped to [0, ndef - 1]
              (one garrison unit is never counted as surplus; martial
              law and ZoC make a single defender normal), where P is
              on-tile defence power and D is max(danger, 1). This is
              the operationalisation of "defenders produced beyond
              assessed need": standing surplus is what sustained
              over-requisitioning leaves behind.
  osc         build-target changes per city per 10 logged turns (the
              production-oscillation guard).
"""

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

DEF_RE = re.compile(
    r'deep-def turn=(\d+) player="([^"]*)" city=(\d+) danger=(\d+)'
    r' urgency=(\d+) defense=(-?\d+) ndef=(\d+) prod="([^"]*)"')


def game_metrics(gamedir):
    surplus = 0.0
    cityturns = 0
    prods = defaultdict(list)   # (player, city) -> [prod per logged turn]
    for m in DEF_RE.finditer((gamedir / "game.log")
                             .read_text(errors="replace")):
        turn, player, city, danger, urg, P, ndef, prod = m.groups()
        danger, P, ndef = int(danger), int(P), int(ndef)
        cityturns += 1
        if ndef > 0 and P > 0:
            d = max(danger, 1)
            if P > d:
                surplus += min(ndef - 1, ndef * (P - d) / P)
        prods[(player, city)].append(prod)
    changes = sum(sum(1 for a, b in zip(seq, seq[1:]) if a != b)
                  for seq in prods.values())
    citylogturns = sum(len(seq) for seq in prods.values())
    return surplus, cityturns, changes, citylogturns


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm", help="restrict to this arm subdirectory")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    T = [0.0, 0, 0, 0]
    print("arm,seed,cityturns,excess_per_cityturn,osc_per_city_10t")
    for d in gamedirs:
        s, ct, ch, clt = game_metrics(d)
        for i, v in enumerate((s, ct, ch, clt)):
            T[i] += v
        print(f"{d.parent.name},{d.name.removeprefix('seed-')},{ct},"
              f"{s / ct if ct else 0:.4f},"
              f"{10 * ch / clt if clt else 0:.3f}")
    print(f"TOTAL,,{T[1]},{T[0] / T[1] if T[1] else 0:.4f},"
          f"{10 * T[2] / T[3] if T[3] else 0:.3f}")


if __name__ == "__main__":
    main()
