#!/usr/bin/env python3
"""S-FRONTIER: the 2x2 of the border wall against the founder floor.

    frontier.py VENUE_DIR       one venue (war/ or econ/)
    frontier.py RESULTS_DIR     every venue under it

VENUE_DIR holds one directory per cell, each holding deep/seed-N/ game
directories (benchmark/frontier.sh writes exactly that).

  c00   neither                        the base
  c10   frontier_transit_gate          the site search may cross a
                                       peace partner's border
  c01   founder_floor_gate             an undoable empty search does
                                       not zero the founder want
  c11   both                           the corner

WHY THE INTERACTION IS THE POINT. Both act on the same T1-75 founder
queue -- the transit gate by giving an empty search somewhere to find,
the floor gate by keeping the founder in the build queue when it finds
nothing -- so c11 - c00 is NOT the sum of the two single-factor
contrasts unless they are independent, and whether they are is exactly
the question. The quantity to read is

    interaction = (c11 - c10) - (c01 - c00)
                = c11 - c10 - c01 + c00

taken PER SEED, because every cell plays the same map on the same seed
(run-game.sh sets mapseed = gameseed = seed) and the map cancels
exactly. Its SE is the SE of that per-seed contrast, which is the only
honest one: the four cells are not independent samples.

Scorelog columns are per player (seats filtered to P1..Pn: a civil war
splinter joins mid-game and scores far below the seats it split from).
Census counters are per game over all seats unless the name says
otherwise.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean as _mean, stdev

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from contentcensus import scorelog, games, loglines  # noqa: E402

CELLS = ("c00", "c10", "c01", "c11")

# The pre-registered scorelog cuts. cities@50/@75 is where the ledger
# put the deep-vs-baseline gap; settlers@ is the founder stock the same
# ledger found at a fifth of baseline's; unitslost is the falsification
# that must not fire when settlers start walking through other people's
# land.
MARKS = [("cities", 25), ("cities", 50), ("cities", 75), ("cities", 100),
         ("settlers", 50), ("settlers", 75),
         ("unitslost", 50), ("unitslost", 75)]
FINALS = ["score", "cities", "pop", "techs", "munits", "settlers",
          "unitslost", "landarea"]

R_EXPAND = re.compile(r'deep-expand turn=(\d+) player="[^"]+" .*'
                      r' w0=(\d+) wpos=(\d+) .* born=(\d+) alive=(\d+)'
                      r' found=(\d+) sites=(\S+)')
R_LADDER = re.compile(r'deep-siteladder turn=(\d+) .* found=(\d)')
R_TRANSIT = re.compile(r'deep-transit turn=(\d+) .* n=(\d+) land=(\d+)'
                       r' seen=(\d+) found=(\d)')
R_FLOOR = re.compile(r'deep-founderfloor turn=(\d+) .* undoable=(\d)'
                     r' sites=(\S+) reach=(\S+) kept=(-?\d+) gate=(\d)')

BANDS = ((1, 25), (26, 50), (51, 75))


def mean(xs):
    return _mean(xs) if xs else float("nan")


def se(xs):
    return stdev(xs) / len(xs) ** 0.5 if len(xs) > 1 else 0.0


def game_row(d):
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
            row["%s@%d" % (tag, t)] = sum(vals) / len(vals)
    turns = sorted(sl.get("score", {}))
    if turns:
        last = turns[-1]
        for tag in FINALS:
            vals = [v for p, v in sl.get(tag, {}).get(last, {}).items()
                    if p in seats]
            if vals:
                row["%s@end" % tag] = sum(vals) / len(vals)

    found = 0
    alive_by_turn = {}
    w0 = wpos = 0
    searches = defaultdict(int)
    empties = defaultdict(int)
    transit_n = transit_lines = transit_found = 0
    floor_seen = floor_kept = 0
    for ln in loglines(d):
        if "deep-expand " in ln:
            m = R_EXPAND.search(ln)
            if m:
                t = int(m.group(1))
                w0 += int(m.group(2))
                wpos += int(m.group(3))
                alive_by_turn[t] = alive_by_turn.get(t, 0) + int(m.group(5))
                found += int(m.group(6))
        elif "deep-siteladder " in ln:
            m = R_LADDER.search(ln)
            if m:
                t = int(m.group(1))
                for lo, hi in BANDS:
                    if lo <= t <= hi:
                        searches[(lo, hi)] += 1
                        if m.group(2) == "0":
                            empties[(lo, hi)] += 1
        elif "deep-transit " in ln:
            m = R_TRANSIT.search(ln)
            if m:
                transit_lines += 1
                transit_n += int(m.group(2))
                transit_found += int(m.group(5))
        elif "deep-founderfloor " in ln:
            m = R_FLOOR.search(ln)
            if m:
                floor_seen += 1
                if int(m.group(5)) != 0:
                    floor_kept += 1

    row["found"] = found
    row["w0"] = w0
    row["wpos"] = wpos
    if alive_by_turn:
        last = max(alive_by_turn)
        for t in (50, 75, last):
            key = "end" if t == last else str(t)
            near = [x for x in alive_by_turn if x <= t]
            if near:
                row["alive@%s" % key] = alive_by_turn[max(near)] / nseat
    for b in BANDS:
        if searches[b]:
            row["empty%%@%d-%d" % b] = 100.0 * empties[b] / searches[b]
            row["search@%d-%d" % b] = searches[b]
    row["transit_lines"] = transit_lines
    row["transit_tiles"] = transit_n
    row["transit_found"] = transit_found
    row["floor_seen"] = floor_seen
    row["floor_kept"] = floor_kept
    return row


def cell(root, name):
    out = {}
    for d in games(root / name / "deep"):
        r = game_row(d)
        if r is not None:
            out[int(d.name.split("-")[-1])] = r
    return out


KEYS = (["%s@%d" % (t, n) for t, n in MARKS]
        + ["%s@end" % t for t in FINALS]
        + ["found", "w0", "wpos", "alive@50", "alive@75", "alive@end"]
        + ["empty%%@%d-%d" % b for b in BANDS]
        + ["search@%d-%d" % b for b in BANDS]
        + ["transit_lines", "transit_tiles", "transit_found",
           "floor_seen", "floor_kept"])


def contrast(a, b, key, seeds):
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
    d = []
    for s in seeds:
        try:
            v = [cells[c][s][key] for c in CELLS]
        except KeyError:
            continue
        d.append(v[3] - v[1] - v[2] + v[0])
    if len(d) < 2:
        return None
    m, s = mean(d), se(d)
    return m, s, (m / s if s else float("nan")), len(d)


def venue(root):
    cells = {c: cell(root, c) for c in CELLS if (root / c).is_dir()}
    cells = {c: v for c, v in cells.items() if v}
    if "c00" not in cells:
        print("%s: no c00 cell" % root)
        return
    print("\n=== %s: cells %s" % (root.name,
                                 " ".join("%s=%d" % (c, len(v))
                                          for c, v in cells.items())))

    print("\n[1] absolute means")
    print("  %-18s" % "metric" + "".join("%10s" % c for c in cells))
    for k in KEYS:
        vals = [mean([r[k] for r in cells[c].values() if k in r])
                for c in cells]
        if all(v != v for v in vals):
            continue
        print("  %-18s" % k + "".join("%10.2f" % v for v in vals))

    for c in [x for x in cells if x != "c00"]:
        seeds = sorted(set(cells["c00"]) & set(cells[c]))
        print("\n[2] %s - c00, paired on %d seeds" % (c, len(seeds)))
        print("  %-18s%10s%10s%10s%9s%7s   wins"
              % ("metric", "c00", c, "delta", "se", "t"))
        for k in KEYS:
            r = contrast(cells["c00"], cells[c], k, seeds)
            if r:
                print("  %-18s%10.2f%10.2f%+10.3f%9.3f%+7.2f   %d/%d"
                      % (k, r[0], r[1], r[2], r[3], r[4], r[5], r[6]))

    if all(c in cells for c in CELLS):
        seeds = sorted(set.intersection(*(set(cells[c]) for c in CELLS)))
        print("\n[3] INTERACTION  (c11 - c10) - (c01 - c00), "
              "paired on %d seeds" % len(seeds))
        print("  %-18s%10s%9s%7s%5s" % ("metric", "value", "se", "t", "n"))
        for k in KEYS:
            r = interaction(cells, k, seeds)
            if r:
                print("  %-18s%+10.3f%9.3f%+7.2f%5d"
                      % (k, r[0], r[1], r[2], r[3]))


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1
                else HERE / "results/frontier")
    subs = [root / v for v in ("war", "econ") if (root / v).is_dir()]
    for r in (subs or [root]):
        venue(r)


main()
