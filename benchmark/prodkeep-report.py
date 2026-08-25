#!/usr/bin/env python3
"""Tables for the production completion bias cell (prod_keep_gate).

  prodkeep-report.py DIR            paired score tables for DIR/{baseline,off,on}
  prodkeep-report.py --census DIR.. settler starts vs abandonments per
                                    25-turn bucket, from deep-prodswitch

The score metric is the one tune.py optimises and the holdout reports
publish: the sum of every player's score at their last logged turn, deep
arm minus baseline arm on the SAME seed. Both deep arms are paired
against the same baseline games, so the off->on contrast is itself
paired and the baseline cancels out of it exactly.

The census is the pipeline the defaults-deficit finding
(findings/defaults-deficit-census-2026-08-25.md) was taken with: switches
INTO "Settlers" against switches OUT of "Settlers", bucketed by turn. In
a healthy pipeline starts exceed abandonments; that census measured
487/529, 287/288, 333/375, 99/153.
"""
import re
import sys
from pathlib import Path

SWITCH = re.compile(rb'deep-prodswitch turn=(\d+) .*?from="([^"]*)" to="([^"]*)"'
                    rb'.*? builtlast=(\d+).*?keep=([-\d.]+)')


def final_score(scorelog):
    """Sum of every player's score at their last logged turn."""
    tags, data = {}, {}
    for line in open(scorelog, errors="replace"):
        parts = line.split(" ")
        if parts[0] == "tag":
            tags[parts[2].strip()] = int(parts[1])
        elif parts[0] == "data":
            t, tag, plr = int(parts[1]), int(parts[2]), int(parts[3])
            data.setdefault(plr, {}).setdefault(tag, {})[t] = parts[4]
    sid = tags["score"]
    total = 0
    for plr, tagdata in data.items():
        turns = tagdata.get(sid, {})
        if turns:
            total += int(turns[max(turns)])
    return total


def arm_scores(root):
    out = {}
    for d in sorted(Path(root).glob("seed-*")):
        if (d / "DONE").exists() and (d / "scorelog.txt").exists():
            out[int(d.name.split("-")[1])] = final_score(d / "scorelog.txt")
    return out


def stats(xs):
    n = len(xs)
    if n == 0:
        return 0, 0.0, 0.0, 0.0
    mean = sum(xs) / n
    if n > 1:
        var = sum((x - mean) ** 2 for x in xs) / (n - 1)
        se = (var / n) ** 0.5
    else:
        se = 0.0
    med = sorted(xs)[n // 2] if n % 2 else sum(sorted(xs)[n // 2 - 1:
                                                        n // 2 + 1]) / 2
    return n, mean, se, med


def score_report(root):
    root = Path(root)
    base = arm_scores(root / "baseline")
    names = [a for a in ("off", "on", "hold") if (root / a).is_dir()]
    arms = {a: arm_scores(root / a) for a in names}
    seeds = sorted(set(base).intersection(*(set(arms[a]) for a in names)))
    print("paired seeds: %d  (baseline %d, %s)\n"
          % (len(seeds), len(base),
             ", ".join(f"{a} {len(arms[a])}" for a in names)))

    print(f"{'arm':6s} {'n':>3s} {'mean diff vs base':>18s} {'se':>7s} "
          f"{'median':>8s} {'wins':>6s}")
    for a in names:
        d = [arms[a][s] - base[s] for s in seeds]
        n, mean, se, med = stats(d)
        wins = sum(1 for x in d if x > 0)
        print(f"{a:6s} {n:3d} {mean:+18.1f} {se:7.1f} {med:+8.1f} "
              f"{wins:3d}/{n}")

    for a in names:
        if a == "off":
            continue
        d = [arms[a][s] - arms["off"][s] for s in seeds]
        n, mean, se, med = stats(d)
        wins = sum(1 for x in d if x > 0)
        print(f"\n{a.upper()} - OFF (paired, same seeds and maps):")
        print(f"  mean {mean:+.1f}  se {se:.1f}"
              f"  t {(mean / se if se else 0):+.2f}"
              f"  median {med:+.1f}  {wins}/{n} seeds better")
        print("  per seed: "
              + " ".join(f"{s}:{arms[a][s] - arms['off'][s]:+d}"
                         for s in seeds))


def census(dirs):
    """Settler runs per 25-turn bucket, split by how each one ENDED.

    A switch away from Settlers is not by itself an abandonment: a
    delivered settler also ends its run and the city then picks
    something else, so the raw out-count conflates the two. The line's
    own `builtlast=` (city_built_last_turn()) separates them, which the
    original starts-vs-abandonments census did not do -- read `built%`
    as the share of runs that reached a unit.

    `kept` counts abandonments the completion bias had credited and
    lost anyway; abandonments with kept=0 are ones the bias never
    reached, because the current production was not a candidate in that
    city-turn's auction at all.
    """
    print(f"{'cell':12s} {'bucket':>7s} {'starts':>7s} {'built':>7s} "
          f"{'abandon':>8s} {'built%':>7s} {'kept':>6s} {'allsw':>7s}")
    for root in dirs:
        cells = {}
        n_games = 0
        tot = [0, 0, 0, 0, 0]
        for log in sorted(Path(root).rglob("game.log")):
            n_games += 1
            for m in SWITCH.finditer(log.read_bytes()):
                turn = int(m.group(1))
                frm, to = m.group(2).decode(), m.group(3).decode()
                built, keep = int(m.group(4)), float(m.group(5))
                b = (turn // 25) * 25
                c = cells.setdefault(b, [0, 0, 0, 0, 0])
                if to == "Settlers":
                    c[0] += 1
                if frm == "Settlers":
                    c[1 if built else 2] += 1
                    if not built and keep > 0:
                        c[3] += 1
                c[4] += 1
        name = Path(root).name
        for b in sorted(cells):
            c = cells[b]
            for k in range(5):
                tot[k] += c[k]
            pct = 100.0 * c[1] / (c[1] + c[2]) if c[1] + c[2] else 0.0
            print(f"{name:12s} {('T%d+' % b):>7s} {c[0]:7d} {c[1]:7d} "
                  f"{c[2]:8d} {pct:6.1f}% {c[3]:6d} {c[4]:7d}")
        pct = 100.0 * tot[1] / (tot[1] + tot[2]) if tot[1] + tot[2] else 0.0
        print(f"{name:12s} {'TOTAL':>7s} {tot[0]:7d} {tot[1]:7d} "
              f"{tot[2]:8d} {pct:6.1f}% {tot[3]:6d} {tot[4]:7d}"
              f"   ({n_games} games)\n")


def main():
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    if args[0] == "--census":
        census(args[1:])
    else:
        score_report(args[0])


if __name__ == "__main__":
    main()
