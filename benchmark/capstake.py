#!/usr/bin/env python3
"""Capital-stake liveness reader (S-CAPSTAKE).

    capstake.py RESULT_TREE

RESULT_TREE holds one directory per arm (`off`, `on`), each with
seed-<n> game directories written by run-game.sh. Two questions, both
read off lines ai/deep already logs:

  1. Did the mechanism execute? `deep-capstake` fires once per turn per
     capital whose loss could start a civil war, and carries the whole
     derivation (P, the defector pool, the multiplier). Zero lines on
     the off arm is what proves the gate is a gate.

  2. Did it move anything? `deep-def` carries `capital=` and `ndef=`,
     so the capital's garrison is separable from every other city's on
     BOTH arms. The non-capital column is the control: a stake that
     merely made deep build more defenders everywhere would move both.
"""
import re
import sys
from pathlib import Path

DEF = re.compile(
    r'deep-def turn=(\d+) player="([^"]*)" city=(\d+) danger=(\d+)'
    r' urgency=(\d+) defense=(-?\d+) ndef=(\d+).*capital=(\d+)')
STAKE = re.compile(
    r'deep-capstake turn=(\d+) player="([^"]*)" city=(\d+)'
    r' site="(?P<site>[^"]*)" prob=(-?\d+)'
    r' odds=([\d.]+) cands=(\d+) flips=(\d+) pool=([\d.]+)'
    r' exposure=([\d.]+) worth=([\d.]+) mult=([\d.]+)'
    r' danger=(\d+) staked=(\d+) clipped=(\d+)')


def arm_logs(d):
    """Both layouts: a batch of seed dirs, or one replayed position."""
    logs = sorted(d.glob("seed-*/game.log"))
    if not logs and (d / "game.log").is_file():
        logs = [d / "game.log"]
    return logs


def read_arm(d):
    stakes, cap, other, capdanger, otherdanger = [], [], [], [], []
    games = 0
    for log in arm_logs(d):
        games += 1
        with open(log, errors="replace") as fh:
            for line in fh:
                m = STAKE.search(line)
                if m:
                    stakes.append((int(m.group(7)), float(m.group(6)),
                                   float(m.group(12)), int(m.group(13)),
                                   int(m.group(14)), int(m.group(15)),
                                   m.group("site")))
                    continue
                m = DEF.search(line)
                if m:
                    ndef, is_cap = int(m.group(7)), int(m.group(8))
                    danger = int(m.group(4))
                    (cap if is_cap else other).append(ndef)
                    (capdanger if is_cap else otherdanger).append(danger)
    return games, stakes, cap, other, capdanger, otherdanger


def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def main():
    root = Path(sys.argv[1])
    print("%-5s %6s %8s %8s %8s %8s %9s %9s"
          % ("arm", "games", "stakes", "meanmult", "cap-ndef", "oth-ndef",
             "cap-dang", "oth-dang"))
    for arm in ("off", "on"):
        d = root / arm
        if not d.is_dir():
            continue
        games, stakes, cap, other, capd, othd = read_arm(d)
        print("%-5s %6d %8d %8.3f %8.3f %8.3f %9.1f %9.1f"
              % (arm, games, len(stakes), mean([s[2] for s in stakes]),
                 mean(cap), mean(other), mean(capd), mean(othd)))
    # The stake's own factors, split by SITE: the ledger's view runs
    # after the incoming-defence credit and often has nothing left to
    # scale, while the build choice re-assesses and is what the
    # defender is bought from.
    for arm in ("off", "on"):
        d = root / arm
        if not d.is_dir():
            continue
        _, stakes, _, _, _, _ = read_arm(d)
        for site in ("ledger", "build"):
            here = [s for s in stakes if s[6] == site]
            if not here:
                continue
            bit = [s for s in here if s[3] > 0]
            print("\n%s arm, %s site: %d priced city-turns, mean"
                  " candidates %.1f, mean P(civil war) %.3f, mean"
                  " multiplier %.3f"
                  % (arm, site, len(here), mean([s[0] for s in here]),
                     mean([s[1] for s in here]),
                     mean([s[2] for s in here])))
            print("%s arm, %s site: %d of them had danger to scale"
                  " (mean %.1f -> %.1f), %d hit the quadratic clip"
                  % (arm, site, len(bit), mean([s[3] for s in bit]),
                     mean([s[4] for s in bit]),
                     sum(s[5] for s in here)))


if __name__ == "__main__":
    main()
