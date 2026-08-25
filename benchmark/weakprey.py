#!/usr/bin/env python3
"""Weak-prey census over one arm's retained game.logs.

    weakprey.py ARMDIR [ARMDIR...]

Two populations, and they answer different questions.

`deep-weakprey` is the per-player-per-turn execution counter: how many
conquest valuations that turn carried a factor above 1.0, what they
averaged, and what the largest was. It describes the MENU -- everything
the player priced -- and it is emitted only when the gate is on, since
nothing increments it otherwise.

`prey=` on `deep-opodds` is the factor of the target a formation
attempt actually CHOSE. It is a pure read taken at log time, so it is
present in BOTH arms: the off arm reports the counterfactual factor of
the target it picked without the bias, which is exactly the baseline
the on arm has to beat. A gate that works moves the chosen-target
factor up relative to the off arm -- deep aiming at weaker players --
and a gate that only inflates prices without re-aiming does not.

`preyunits` is the chosen victim's own score.units, so "weaker" can be
read in units rather than only as a ratio.
"""
import re
import sys
from pathlib import Path
from statistics import mean, median

CENSUS = re.compile(r"deep-weakprey .*?priced=(\d+) mean=([\d.]+) "
                    r"max=([\d.]+) ours=(\d+)")
ODDS = re.compile(r"deep-opodds .*?verdict=\"([a-z]+)\".*?"
                  r"prey=([\d.]+) preyunits=(\d+)")


def scan(root):
    games = 0
    priced = 0
    census_means = []
    census_max = 0.0
    picks = []          # (verdict, prey, preyunits)
    for log in sorted(Path(root).glob("*/seed-*/game.log")):
        games += 1
        with open(log, encoding="utf-8", errors="replace") as fh:
            for line in fh:
                m = CENSUS.search(line)
                if m:
                    n = int(m.group(1))
                    priced += n
                    census_means.append(float(m.group(2)))
                    census_max = max(census_max, float(m.group(3)))
                    continue
                m = ODDS.search(line)
                if m:
                    picks.append((m.group(1), float(m.group(2)),
                                  int(m.group(3))))
    return games, priced, census_means, census_max, picks


def report(root):
    games, priced, cmeans, cmax, picks = scan(root)
    print(f"# {root}")
    if games == 0:
        print("  no game.logs found")
        return
    print(f"  games                    {games}")
    print(f"  deep-weakprey valuations {priced}"
          f" ({priced / games:.1f}/game)")
    if cmeans:
        print(f"    mean factor            {mean(cmeans):.3f}")
        print(f"    largest factor seen    {cmax:.3f}")
    else:
        print("    (no census lines -- gate off, or nothing priced)")
    if not picks:
        print("  no deep-opodds lines")
        return
    print(f"  formation attempts       {len(picks)}"
          f" ({len(picks) / games:.1f}/game)")
    for label, rows in (("all picks", picks),
                        ("formed only",
                         [p for p in picks if p[0] == "formed"])):
        if not rows:
            continue
        prey = [p[1] for p in rows]
        units = [p[2] for p in rows]
        above = sum(1 for v in prey if v > 1.0)
        print(f"    {label:<20} n={len(rows)}"
              f" chosen prey mean={mean(prey):.3f}"
              f" median={median(prey):.3f}"
              f" >1.0={above} ({100.0 * above / len(rows):.1f}%)"
              f" victim units mean={mean(units):.1f}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    for root in sys.argv[1:]:
        report(root)
        print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
