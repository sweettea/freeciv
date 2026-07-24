#!/usr/bin/env python3
"""Settler (cityfounder) loss rates from deep-loss log lines.

    losses.py --root DIR [--arm ARM]

Walks DIR for */seed-*/DONE game directories (see run.sh) and counts the
`deep-loss ... founder=1` lines that ai/deep writes to game.log whenever
one of its units dies (any cause; the hook does not know the cause).
Only games of arms whose players run ai/deep produce these lines.

Per game: founder losses, total unit losses, final turn (from
scorelog.txt), and founder losses per 100 turns. Then the aggregate:
total founder losses, total turns, and the pooled per-100-turn rate.
"""

import argparse
import re
import sys
from pathlib import Path

LOSS_RE = re.compile(
    r'deep-loss turn=(\d+) player="([^"]*)" unit="([^"]*)" founder=(\d)'
)


def final_turn(scorelog):
    turn = 0
    with open(scorelog, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("turn "):
                turn = max(turn, int(line.split(" ")[1]))
    return turn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm", help="restrict to this arm subdirectory")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    tot_founder = tot_units = tot_turns = 0
    print("arm,seed,turns,founder_lost,units_lost,founder_per_100t")
    for d in gamedirs:
        losses = LOSS_RE.findall((d / "game.log").read_text(errors="replace"))
        founder = sum(1 for m in losses if m[3] == "1")
        turns = final_turn(d / "scorelog.txt")
        rate = 100.0 * founder / turns if turns else 0.0
        print(f"{d.parent.name},{d.name.removeprefix('seed-')},"
              f"{turns},{founder},{len(losses)},{rate:.2f}")
        tot_founder += founder
        tot_units += len(losses)
        tot_turns += turns

    pooled = 100.0 * tot_founder / tot_turns if tot_turns else 0.0
    print(f"TOTAL,,{tot_turns},{tot_founder},{tot_units},{pooled:.2f}")


if __name__ == "__main__":
    main()
