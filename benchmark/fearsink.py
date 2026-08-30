#!/usr/bin/env python3
"""S-FEARSINK: the 2x2 of S-FEARSEAT x S-SITEPLAN, with its interaction.

    fearsink.py VENUE_DIR              one venue (war/ or econ/)
    fearsink.py RESULTS_DIR            every venue under it

VENUE_DIR holds one directory per cell, each holding deep/seed-N/ game
directories (benchmark/fearsink.sh writes exactly that).

  c00   neither mechanism                      the base
  c10   mil_opportunity_gate                   S-FEARSEAT's one lever
  c01   founder_site_cap + founder_join_guard  S-SITEPLAN's pair
  c11   all three                              the corner
  boat  boat_shadow_gate                       outside the factorial

WHY THE INTERACTION IS THE POINT. Both parents were adjudicated on a
base without the other's gates, and both act on the same T50-75 founder
queue. c11 - c00 is NOT the sum of the two single-factor contrasts
unless the mechanisms are independent, and whether they are is exactly
the open question: the fear verdict's S4 says the price frees shields
that reach the founder queue at T50 and are gone by T75, and the
siteplan verdict says the join is what the queue spends founders on.
So the quantity to read is

    interaction = (c11 - c10) - (c01 - c00)
                = c11 - c10 - c01 + c00

taken PER SEED, because every cell plays the same map on the same seed
(run-game.sh sets mapseed = gameseed = seed) and the map cancels
exactly. Its SE is the SE of that per-seed contrast, which is the only
honest one: the four cells are not independent samples.

Every scorelog column is per player (seats filtered to P1..Pn: a civil
war splinter joins mid-game and scores far below the seats it split
from). Census counters that are per game over all seats -- joins,
settlers built, gate firings -- say so in the table.
"""
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

CELLS = ("c00", "c10", "c01", "c11", "boat")

# The pre-registered scorelog cuts. cities@50/@75 is the fear verdict's
# own finding (the T50 gain that vanished by T75); unitslost@75 is the
# falsification that must not fire; munits@25 says the opening army
# actually moved.
MARKS = [("munits", 25), ("unitsbuilt", 25), ("cities", 25),
         ("cities", 50), ("cities", 75), ("settlers", 50),
         ("unitslost", 50), ("unitslost", 75)]
FINALS = ["score", "cities", "pop", "techs", "munits", "settlers",
          "unitslost", "landarea", "culture"]

R_EXPAND = re.compile(r'deep-expand turn=(\d+) player="[^"]+" .*'
                      r' born=(\d+) alive=(\d+) found=(\d+) sites=(\S+)')
R_JOIN = re.compile(r'deep-join turn=\d+ .* done=1')
R_GUARD = re.compile(r'deep-joinguard turn=\d+ .* held=(\d) gate=(\d)')
R_CAP = re.compile(r'deep-sitecap turn=\d+ .* capped=(\d) gate=(\d)')
R_MILOPP = re.compile(r'deep-milopp turn=(\d+) .* mil=(\S+) founder=(\S+)'
                      r' charge=(\S+) after=(\S+)')

# The two bands the census localized the deficit to, and the two spend
# classes it weighed against each other.
BANDS = ((1, 25), (51, 75))
SPEND = ("military", "expansion")


def mean(xs):
    return _mean(xs) if xs else float("nan")


def se(xs):
    return stdev(xs) / len(xs) ** 0.5 if len(xs) > 1 else 0.0


def game_row(d, rules):
    """One game -> one row. Scorelog columns per seat, census columns
    per game unless the name says otherwise."""
    sl, players = scorelog(d / "scorelog.txt")
    seats = {n for n, nm in players.items() if re.fullmatch(r"P\d+", nm)}
    if not seats:
        return None
    nseat = len(seats)
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

    band = defaultdict(Counter)
    joins = built = 0
    alive_by_turn = {}
    found = 0
    guard_seen = guard_fired = cap_seen = cap_fired = 0
    milopp = 0
    milopp_zeroed = 0
    for ln in loglines(d):
        if "deep-prodswitch " in ln:
            m = R_SWITCH.search(ln)
            if m and m.group(11) == "1" and m.group(4):
                b = (int(m.group(1)) - 1) // BUCKET * BUCKET + 1
                band[b][m.group(4)] += 1
                if m.group(4) == "Settlers":
                    built += 1
        elif "deep-join " in ln:
            if R_JOIN.search(ln):
                joins += 1
        elif "deep-expand " in ln:
            m = R_EXPAND.search(ln)
            if m:
                t = int(m.group(1))
                alive_by_turn[t] = alive_by_turn.get(t, 0) + int(m.group(3))
                found += int(m.group(4))
        elif "deep-joinguard " in ln:
            m = R_GUARD.search(ln)
            if m:
                guard_seen += int(m.group(1))
                guard_fired += 1 if m.group(1) == "1" == m.group(2) else 0
        elif "deep-sitecap " in ln:
            m = R_CAP.search(ln)
            if m:
                cap_seen += int(m.group(1))
                cap_fired += 1 if m.group(1) == "1" == m.group(2) else 0
        elif "deep-milopp " in ln:
            m = R_MILOPP.search(ln)
            if m:
                milopp += 1
                milopp_zeroed += 1 if float(m.group(5)) <= 0 else 0

    for lo, hi in BANDS:
        for cls in SPEND:
            tot = 0.0
            for b in range(lo, hi + 1, BUCKET):
                tot += sum(band[b][x] * rules.get(x, (0, 0))[0]
                           for x in CLASSES[cls])
            row[f"{cls[:3]}sh@{lo}-{hi}"] = tot / nseat
    row["settlers_built"] = built
    row["joins"] = joins
    row["found"] = found
    if alive_by_turn:
        last = max(alive_by_turn)
        for t in (50, 75, last):
            key = "end" if t == last else str(t)
            near = [x for x in alive_by_turn if x <= t]
            if near:
                row[f"alive@{key}"] = alive_by_turn[max(near)] / nseat
    row["guard_seen"] = guard_seen
    row["guard_fired"] = guard_fired
    row["cap_seen"] = cap_seen
    row["cap_fired"] = cap_fired
    row["milopp_fired"] = milopp
    row["milopp_zeroed"] = milopp_zeroed
    return row


def cell(root, name, rules):
    out = {}
    for d in games(root / name / "deep"):
        r = game_row(d, rules)
        if r is not None:
            out[int(d.name.split("-")[-1])] = r
    return out


KEYS = ([f"{t}@{n}" for t, n in MARKS]
        + [f"{t}@end" for t in FINALS]
        + [f"{c[:3]}sh@{lo}-{hi}" for lo, hi in BANDS for c in SPEND]
        + ["settlers_built", "joins", "found",
           "alive@50", "alive@75", "alive@end",
           "guard_seen", "guard_fired", "cap_seen", "cap_fired",
           "milopp_fired", "milopp_zeroed"])


def contrast(a, b, key, seeds):
    """b - a, paired."""
    xs = [(a[s][key], b[s][key]) for s in seeds
          if key in a[s] and key in b[s]]
    if len(xs) < 2:
        return None
    d = [y - x for x, y in xs]
    m, s = mean(d), se(d)
    return (mean([x for x, _ in xs]), mean([y for _, y in xs]), m, s,
            m / s if s else float("nan"),
            sum(1 for v in d if v > 0), len(d))


def interaction(cells, key, seeds):
    """(c11 - c10) - (c01 - c00), paired on the seed."""
    d = []
    for s in seeds:
        try:
            v = [cells[c][s][key] for c in ("c00", "c10", "c01", "c11")]
        except KeyError:
            continue
        d.append(v[3] - v[1] - v[2] + v[0])
    if len(d) < 2:
        return None
    m, s = mean(d), se(d)
    return m, s, (m / s if s else float("nan")), len(d)


def venue(root, rules):
    cells = {c: cell(root, c, rules) for c in CELLS if (root / c).is_dir()}
    cells = {c: v for c, v in cells.items() if v}
    if "c00" not in cells:
        print(f"{root}: no c00 cell")
        return
    print(f"\n=== {root.name}: cells "
          + " ".join(f"{c}={len(v)}" for c, v in cells.items()))

    print("\n[1] absolute means")
    print(f"  {'metric':<18}" + "".join(f"{c:>10}" for c in cells))
    for k in KEYS:
        vals = [mean([r[k] for r in cells[c].values() if k in r])
                for c in cells]
        if all(v != v for v in vals):
            continue
        print(f"  {k:<18}" + "".join(f"{v:>10.2f}" for v in vals))

    for c in [x for x in cells if x != "c00"]:
        seeds = sorted(set(cells["c00"]) & set(cells[c]))
        print(f"\n[2] {c} - c00, paired on {len(seeds)} seeds")
        print(f"  {'metric':<18}{'c00':>10}{c:>10}{'delta':>10}"
              f"{'se':>9}{'t':>7}   wins")
        for k in KEYS:
            r = contrast(cells["c00"], cells[c], k, seeds)
            if r:
                print(f"  {k:<18}{r[0]:>10.2f}{r[1]:>10.2f}{r[2]:>+10.3f}"
                      f"{r[3]:>9.3f}{r[4]:>+7.2f}   {r[5]}/{r[6]}")

    if all(c in cells for c in ("c00", "c10", "c01", "c11")):
        seeds = sorted(set.intersection(*(set(cells[c])
                                          for c in ("c00", "c10",
                                                    "c01", "c11"))))
        print(f"\n[3] INTERACTION  (c11 - c10) - (c01 - c00), "
              f"paired on {len(seeds)} seeds")
        print(f"  {'metric':<18}{'value':>10}{'se':>9}{'t':>7}{'n':>5}")
        for k in KEYS:
            r = interaction(cells, k, seeds)
            if r:
                print(f"  {k:<18}{r[0]:>+10.3f}{r[1]:>9.3f}"
                      f"{r[2]:>+7.2f}{r[3]:>5}")


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1
                else HERE / "results/fearsink")
    rulesdir = HERE.parent / "data" / "civ2civ3"
    rules = ruleset_upkeep(rulesdir)
    rules.update({k: v for k, v in ruleset_unit_cost(rulesdir).items()
                  if k not in rules})
    subs = [root / v for v in ("war", "econ") if (root / v).is_dir()]
    for r in (subs or [root]):
        venue(r, rules)


main()
