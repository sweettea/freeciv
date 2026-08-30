#!/usr/bin/env python3
"""S-FEARSEAT cell report: one venue, several rungs, paired on seeds.

    fearcell.py VENUE_DIR [--rungs "off ramp evid price all boat"]

VENUE_DIR holds one directory per rung, each holding deep/seed-N/ game
directories (benchmark/fearseat.sh writes exactly that).

Three tables, in the order the epic's pre-registrations are read:

  [1] THE SEAT. Share of BUILD-seat evaluations carrying each injected
      fear term, by 25-turn band, straight off `deep-fear site=build`.
      The census's second pre-registration lives here: a ramp that
      does not move the T1-50 density share never reached the seat,
      and the arm's score means nothing.

  [2] THE SPEND. Delivered shields per player-game by class and band,
      from `deep-prodswitch ... builtlast=1` priced at the ruleset's
      build cost -- contentcensus.py's [4b], per rung.

  [3] THE OUTCOME, paired. Per seed the rung's seats are averaged,
      then rung - off is taken seed by seed, so the map is held fixed.
      Reports mean, paired SE and t for the pre-registered quantities:
      munits at T25/T50, cities at T50/T75/end, unitslost at T75 (the
      falsification: it must NOT rise), and score.

Every number is per player. Seats are filtered to P1..Pn: a civil-war
splinter joins mid-game and scores far below the seats it split from
(paircmp.py's own note).
"""
import argparse
import re
import sys
from collections import defaultdict, Counter
from pathlib import Path
from statistics import mean as _mean, stdev

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from contentcensus import (  # noqa: E402
    scorelog, games, loglines, ruleset_upkeep, ruleset_unit_cost,
    CLASSES, R_SWITCH, BUCKET,
)

R_FEAR = re.compile(
    r'deep-fear turn=(\d+) player="([^"]+)" city=(\d+) site=(\w+) '
    r'pre=(\d+) over=(\d+) shadow=(\d+) density=(\d+) prior=(\d+) '
    r'barb=(\d+) surv=(\d+) credit=(\d+) updisc=(\S+) sat=(\S+) '
    r'fmult=(\S+)(?: evid=(-?\d+))? prefloor=(\d+) post=(\d+)')

MARKS = [("munits", 25), ("munits", 50), ("unitsbuilt", 25),
         ("unitsbuilt", 50), ("settlers", 50), ("cities", 25),
         ("cities", 50), ("cities", 75), ("unitslost", 50),
         ("unitslost", 75), ("unitskilled", 75)]
FINALS = ["cities", "score", "munits", "unitslost", "techs", "pop"]


def mean(xs):
    return _mean(xs) if xs else 0.0


def se(xs):
    return stdev(xs) / len(xs) ** 0.5 if len(xs) > 1 else 0.0


def per_seed(rung_dir):
    """{seed: {key: per-player mean}} from the scorelogs."""
    out = {}
    for d in sorted(games(Path(rung_dir) / "deep")):
        seed = int(d.name.split("-")[-1])
        sl, players = scorelog(d / "scorelog.txt")
        seats = {n for n, nm in players.items() if re.fullmatch(r"P\d+", nm)}
        if not seats:
            continue
        row = {}
        for tag, t in MARKS:
            vals = [v for p, v in sl.get(tag, {}).get(t, {}).items()
                    if p in seats]
            if vals:
                row[f"{tag}@{t}"] = sum(vals) / len(vals)
        turns = sorted(sl.get("score", {}))
        if turns:
            last = turns[-1]
            for tag in FINALS:
                vals = [v for p, v in sl.get(tag, {}).get(last, {}).items()
                        if p in seats]
                if vals:
                    row[f"{tag}@end"] = sum(vals) / len(vals)
        out[seed] = row
    return out


def seat_shares(rung_dir):
    """band -> counters over deep-fear site=build lines."""
    acc = defaultdict(Counter)
    for d in games(Path(rung_dir) / "deep"):
        for ln in loglines(d):
            if "deep-fear " not in ln:
                continue
            m = R_FEAR.search(ln)
            if m is None or m.group(4) != "build":
                continue
            t = int(m.group(1))
            b = (t - 1) // BUCKET * BUCKET + 1
            a = acc[b]
            a["n"] += 1
            for name, g in (("pre", 5), ("shadow", 7), ("density", 8),
                            ("prior", 9), ("barb", 10), ("surv", 11)):
                v = int(m.group(g))
                a[name] += 1 if v > 0 else 0
                a[name + "_sum"] += v
            a["post"] += 1 if int(m.group(18)) > 0 else 0
            a["post_sum"] += int(m.group(18))
            if m.group(16) is not None:
                a["evid_n"] += 1
                a["evid_sum"] += int(m.group(16))
                a["evid_pos"] += 1 if int(m.group(16)) > 0 else 0
    return acc


def band_spend(rung_dir, rules):
    """band -> class -> delivered shields, and the player-game count."""
    band = defaultdict(Counter)
    npg = 0
    for d in games(Path(rung_dir) / "deep"):
        sl, players = scorelog(d / "scorelog.txt")
        npg += len([n for n in players.values() if re.fullmatch(r"P\d+", n)])
        for ln in loglines(d):
            if "deep-prodswitch " not in ln:
                continue
            m = R_SWITCH.search(ln)
            if m and m.group(11) == "1" and m.group(4):
                b = (int(m.group(1)) - 1) // BUCKET * BUCKET + 1
                band[b][m.group(4)] += 1
    return band, max(npg, 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("venue")
    ap.add_argument("--rungs", default="off ramp evid price all boat")
    ap.add_argument("--rules", default=None)
    args = ap.parse_args()

    venue = Path(args.venue)
    rungs = [r for r in args.rungs.split() if (venue / r).is_dir()]
    if not rungs:
        print("no rung directories under", venue)
        return
    rulesdir = args.rules or (HERE.parent / "data" / "civ2civ3")
    rules = ruleset_upkeep(rulesdir)
    rules.update({k: v for k, v in ruleset_unit_cost(rulesdir).items()
                  if k not in rules})

    print("\n[1] the seat: share of BUILD-seat evaluations carrying each "
          "injected term")
    for rung in rungs:
        acc = seat_shares(venue / rung)
        if not acc:
            continue
        print(f"\n  rung {rung}")
        print(f"    {'band':<11} {'n':>7} {'pre':>7} {'density':>8} "
              f"{'shadow':>7} {'barb':>6} {'surv':>6} {'post>0':>7} "
              f"{'mean post':>10} {'mean evid':>10} {'evid=0':>7}")
        for b in sorted(acc):
            a = acc[b]
            n = max(a["n"], 1)
            ev = a["evid_sum"] / a["evid_n"] if a["evid_n"] else float("nan")
            print(f"    T{b:>3}-{b+BUCKET-1:<6} {a['n']:>7} "
                  f"{100*a['pre']/n:>6.1f}% {100*a['density']/n:>7.1f}% "
                  f"{100*a['shadow']/n:>6.1f}% {100*a['barb']/n:>5.1f}% "
                  f"{100*a['surv']/n:>5.1f}% {100*a['post']/n:>6.1f}% "
                  f"{a['post_sum']/n:>10.2f} {ev:>10.2f} "
                  f"{100*(a['evid_n']-a['evid_pos'])/max(a['evid_n'],1):>6.1f}%")

    print("\n[2] delivered shields per player-game, by class and band")
    for rung in rungs:
        band, npg = band_spend(venue / rung, rules)
        if not band:
            continue
        heads = sorted(band)
        print(f"\n  rung {rung} ({npg} player-games)")
        print(f"    {'class':<12} " + " ".join(f"T{b}-{b+BUCKET-1:<3}"
                                               for b in heads))
        for cls, members in CLASSES.items():
            cells = [sum(band[b][m] * rules.get(m, (0, 0))[0]
                         for m in members) / npg for b in heads]
            print(f"    {cls:<12} " + " ".join(f"{c:>8.1f}" for c in cells))

    print("\n[3] outcome, paired on seed against the off rung "
          "(rung - off, per-player)")
    base = per_seed(venue / "off")
    keys = [f"{t}@{n}" for t, n in MARKS] + [f"{t}@end" for t in FINALS]
    for rung in rungs:
        if rung == "off":
            continue
        cur = per_seed(venue / rung)
        common = sorted(set(base) & set(cur))
        print(f"\n  rung {rung} vs off, n={len(common)} paired seeds")
        print(f"    {'metric':<18} {'off':>9} {'rung':>9} {'diff':>9} "
              f"{'se':>8} {'t':>7}")
        for k in keys:
            d = [cur[s][k] - base[s][k] for s in common
                 if k in cur[s] and k in base[s]]
            if not d:
                continue
            o = [base[s][k] for s in common if k in base[s] and k in cur[s]]
            c = [cur[s][k] for s in common if k in base[s] and k in cur[s]]
            s_ = se(d)
            t = mean(d) / s_ if s_ else 0.0
            print(f"    {k:<18} {mean(o):>9.2f} {mean(c):>9.2f} "
                  f"{mean(d):>+9.3f} {s_:>8.3f} {t:>+7.2f}")


if __name__ == "__main__":
    main()
