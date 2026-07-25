#!/usr/bin/env python3
"""Settler (cityfounder) loss rates from deep-loss log lines.

    losses.py --root DIR [--arm ARM]

Walks DIR for */seed-*/DONE game directories (see run.sh) and parses the
`deep-loss` lines that ai/deep writes to game.log whenever one of its
units is removed, tagged with the server's loss reason. Only games of
arms whose players run ai/deep produce these lines.

Founder losses are split into
    hostile   killed/captured/bribed/executed/eliminated/stack_conflict/
              missile/sdi/nuke/transfer -- enemy action; W05's target
    founded   "used" -- the settler successfully became a city
    other     everything else (starved, disbanded, city_lost, ...)

Per game and pooled: hostile founder losses per 100 turns (the W05
acceptance metric) and cities founded per 100 turns (the over-timidity
guard).
"""

import argparse
import re
import sys
from pathlib import Path

LOSS_RE = re.compile(
    r'deep-loss turn=(\d+) player="([^"]*)" unit="([^"]*)" founder=(\d)'
    r' reason="([^"]*)"'
)

HOSTILE = {"killed", "captured", "bribed", "executed", "eliminated",
           "stack_conflict", "missile", "sdi", "nuke", "transfer"}


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

    tot = {"hostile": 0, "founded": 0, "other": 0, "turns": 0}
    print("arm,seed,turns,founder_hostile,founded,founder_other,"
          "hostile_per_100t,founded_per_100t")
    for d in gamedirs:
        losses = LOSS_RE.findall((d / "game.log").read_text(errors="replace"))
        hostile = founded = other = 0
        for m in losses:
            if m[3] != "1":
                continue
            if m[4] == "used":
                founded += 1
            elif m[4] in HOSTILE:
                hostile += 1
            else:
                other += 1
        turns = final_turn(d / "scorelog.txt")
        hr = 100.0 * hostile / turns if turns else 0.0
        fr = 100.0 * founded / turns if turns else 0.0
        print(f"{d.parent.name},{d.name.removeprefix('seed-')},{turns},"
              f"{hostile},{founded},{other},{hr:.2f},{fr:.2f}")
        tot["hostile"] += hostile
        tot["founded"] += founded
        tot["other"] += other
        tot["turns"] += turns

    hr = 100.0 * tot["hostile"] / tot["turns"] if tot["turns"] else 0.0
    fr = 100.0 * tot["founded"] / tot["turns"] if tot["turns"] else 0.0
    print(f"TOTAL,,{tot['turns']},{tot['hostile']},{tot['founded']},"
          f"{tot['other']},{hr:.2f},{fr:.2f}")


if __name__ == "__main__":
    main()
