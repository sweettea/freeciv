#!/usr/bin/env python3
"""Legibility meter for the Europe late venue: what did each nation build?

    europemeter.py RUN_DIR [--venue europe-late] [--label defaults ...]
    europemeter.py --save SAVE [SAVE ...]       # per-nation rows of one save

The scoreboard's europe-late venue resumes a per-seed baseline T150
start and plays to T250 (scoreboard.sh --europe). Its score lines go
through rankcmp.py like any venue; this reads what the score does not
show, out of each game's kept final save (final.sav.zst):

  land      tiles the nation owns that are not ocean or lake
  road      owned land tiles carrying the Road extra, city centres
            excluded (every centre gets a free road and railroad);
            rail likewise
  dens      road / land
  cities    live cities
  routes    trade routes that EXIST (traderouteN slots of the city
            table, 0 = empty), counted once per route endpoint the
            nation owns, so a domestic route counts twice for one nation
            and a foreign route once for each side
  far       routes whose two cities are >= FAR tiles apart (Chebyshev
            distance; the Europe map is flat and unwrapped)
  dist      mean and max length of the nation's routes
  foreign   routes whose partner city belongs to another nation
  caravans  Caravan/Freight units alive at the final turn

Per-nation rows are grouped by (label, kind, arm, ai_type) and averaged
over nations. When the run dir holds europe-starts/seed-N.sav.zst, the
same table is also printed as the CHANGE from the T150 start, per
nation (player numbers are stable across the resume: run-game.sh only
rewrites ai_type), which is the fairer reading because both arms
inherit the same T150 map. The paired reading is the mixed games: deep seats minus
baseline seats inside the SAME game, mean +- SE over games, since the
seats share one map and one T150 start. Self-play rows are reported
beside each other but they are different games on the same seeds.

Everything here reports; nothing adjudicates (CLAUDE.md rule 6: the
training seeds decide, this venue is for legibility).
"""

import argparse
import csv
import io
import math
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

FAR = 12            # tiles; about a third of the Europe map's height
WATER = set(" :+")  # ocean, deep ocean, lake identifiers (classic, civ2civ3)
CARAVANS = {"Caravan", "Freight"}


def read_save(path):
    path = str(path)
    if path.endswith(".zst"):
        return subprocess.run(["zstdcat", path], capture_output=True,
                              text=True, check=True).stdout
    return Path(path).read_text(encoding="utf-8", errors="replace")


def _csv(line):
    return next(csv.reader(io.StringIO(line)))


def _table(section, key):
    m = re.search(r"^%s=\{(.*?)^\}" % re.escape(key), section, re.M | re.S)
    if not m:
        return []
    lines = m.group(1).splitlines()
    header = _csv(lines[0])
    return [dict(zip(header, _csv(l))) for l in lines[1:] if l]


def _player_sections(text):
    parts = re.split(r"^\[player(\d+)\]$", text, flags=re.M)
    for i in range(1, len(parts) - 1, 2):
        body = re.split(r"^\[", parts[i + 1], maxsplit=1, flags=re.M)[0]
        yield int(parts[i]), body


def _scalar(section, key, default=None):
    m = re.search(r'^%s=(?:"([^"]*)"|(.*))$' % re.escape(key), section, re.M)
    if not m:
        return default
    return m.group(1) if m.group(1) is not None else m.group(2)


def _rows(text, prefix):
    """{y: string} for map rows named <prefix>NNNN."""
    out = {}
    for m in re.finditer(r'^%s(\d{4})="([^"]*)"$' % prefix, text, re.M):
        out[int(m.group(1))] = m.group(2)
    return out


def extra_tiles(text, name):
    """set of (x, y) carrying the named extra."""
    m = re.search(r"^extras_vector=(.*)$", text, re.M)
    if not m:
        raise SystemExit("no extras_vector in save")
    idx = _csv(m.group(1)).index(name)
    layer, bit = divmod(idx, 4)
    mask = 1 << bit
    tiles = set()
    for y, row in _rows(text, "e%02d_" % layer).items():
        for x, c in enumerate(row):
            if int(c, 16) & mask:
                tiles.add((x, y))
    return tiles


def owned_land(text):
    """{player: set of (x, y)} land tiles owned."""
    terrain = _rows(text, "t")
    owned = defaultdict(set)
    for y, row in _rows(text, "owner").items():
        trow = terrain.get(y, "")
        for x, o in enumerate(row.split(",")):
            if o == "-" or x >= len(trow) or trow[x] in WATER:
                continue
            owned[int(o)].add((x, y))
    return owned


def nations(text):
    """Per-nation dict rows for every non-barbarian AI player."""
    road = extra_tiles(text, "Road")
    rail = extra_tiles(text, "Railroad")
    land = owned_land(text)
    players = {}
    cities = {}   # city id -> (owner, x, y)
    for plr, sec in _player_sections(text):
        flags = _scalar(sec, "flags", "") or ""
        if (_scalar(sec, "ai.barb_type", "None") or "None") != "None":
            continue
        if '"ai"' not in flags and "ai" not in flags.split(","):
            continue
        players[plr] = sec
        for row in _table(sec, "c"):
            cities[int(row["id"])] = (plr, int(row["x"]), int(row["y"]))
    out = []
    for plr, sec in players.items():
        crow = _table(sec, "c")
        dists, foreign = [], 0
        for row in crow:
            x, y = int(row["x"]), int(row["y"])
            for col, val in row.items():
                if not re.fullmatch(r"traderoute\d+", col) or val.strip() in ("", "0"):
                    continue
                p = cities.get(int(val))
                if p is None:
                    continue    # partner city gone or not a nation's
                dists.append(max(abs(p[1] - x), abs(p[2] - y)))
                foreign += p[0] != plr
        mine = land.get(plr, set())
        centres = {(int(r["x"]), int(r["y"])) for r in crow}
        caravans = sum(1 for u in _table(sec, "u") if u["type_by_name"] in CARAVANS)
        out.append({
            "player": plr,
            "name": _scalar(sec, "name", "?"),
            "nation": _scalar(sec, "nation", "?"),
            "ai_type": _scalar(sec, "ai_type", "?"),
            "alive": _scalar(sec, "is_alive", "TRUE") == "TRUE",
            "land": len(mine),
            "road": len((mine & road) - centres),
            "rail": len((mine & rail) - centres),
            "cities": len(crow),
            "routes": len(dists),
            "far": sum(d >= FAR for d in dists),
            "foreign": foreign,
            "dists": dists,
            "caravans": caravans,
            "dens": len((mine & road) - centres) / len(mine) if mine else 0.0,
        })
    return out


METRICS = ["land", "road", "rail", "cities", "routes", "far", "foreign", "caravans"]


def summarise(rows):
    """Mean per nation of each metric, plus route-length pooled stats."""
    if not rows:
        return None
    s = {k: mean(r[k] for r in rows) for k in METRICS + ["dens"]}
    pooled = [d for r in rows for d in r["dists"]]
    s["dist"] = mean(pooled) if pooled else 0.0
    s["dmax"] = max(pooled) if pooled else 0
    s["n"] = len(rows)
    s["dead"] = sum(not r["alive"] for r in rows)
    return s


def fmt_summary(key, s):
    return ("%-44s n=%3d dead=%2d land %6.1f road %6.1f rail %5.1f dens %.3f "
            "cities %5.1f routes %5.2f far %4.2f foreign %4.2f dist %5.1f/%-3d caravans %4.2f"
            % (key, s["n"], s["dead"], s["land"], s["road"], s["rail"], s["dens"],
               s["cities"], s["routes"], s["far"], s["foreign"], s["dist"], s["dmax"],
               s["caravans"]))


def print_save(path):
    text = read_save(path)
    turn = re.search(r"^turn=(\d+)$", text, re.M)
    print("%s  turn=%s" % (path, turn.group(1) if turn else "?"))
    rows = nations(text)
    print("  %-4s %-9s %-14s %5s %5s %5s %6s %7s %4s %7s %6s %8s" % (
        "plr", "ai", "nation", "land", "road", "rail", "cities", "routes",
        "far", "foreign", "dist", "caravans"))
    for r in sorted(rows, key=lambda r: r["player"]):
        d = mean(r["dists"]) if r["dists"] else 0
        print("  %-4d %-9s %-14s %5d %5d %5d %6d %7d %4d %7d %6.1f %8d%s" % (
            r["player"], r["ai_type"], r["nation"][:14], r["land"], r["road"],
            r["rail"], r["cities"], r["routes"], r["far"], r["foreign"], d,
            r["caravans"], "" if r["alive"] else "  (dead)"))
    by = defaultdict(list)
    for r in rows:
        by[r["ai_type"]].append(r)
    for k in sorted(by):
        print(" ", fmt_summary(k, summarise(by[k])))


def games(run, venue, labels):
    """Yield (label, kind, arm, seed, save_path) for finished games."""
    run = Path(run)
    for label in labels:
        base = run / label / venue
        for kind in ("selfplay", "mixed"):
            for d in sorted((base / kind).glob("*/seed-*")) if (base / kind).is_dir() else []:
                if (d / "DONE").exists() and (d / "final.sav.zst").exists():
                    yield label, kind, d.parent.name, int(d.name.split("-")[1]), d / "final.sav.zst"


def se(xs):
    """Standard error of the mean (sample SD / sqrt n)."""
    if len(xs) < 2:
        return float("nan")
    return stdev(xs) / math.sqrt(len(xs))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("run", nargs="?", help="scoreboard run dir (run-<pin12>)")
    ap.add_argument("--venue", default="europe-late")
    ap.add_argument("--label", action="append",
                    help="label dirs to read (default: every dir with the venue)")
    ap.add_argument("--save", nargs="+", help="print per-nation rows of these saves")
    a = ap.parse_args()
    if a.save:
        for p in a.save:
            print_save(p)
        return
    if not a.run:
        ap.error("RUN_DIR or --save")
    run = Path(a.run)
    labels = a.label or sorted(p.name for p in run.iterdir()
                               if (p / a.venue).is_dir())
    groups = defaultdict(list)      # (label, kind, arm, ai_type) -> nation rows
    deltas = defaultdict(list)      # same key -> rows minus the T150 start
    starts = {}                     # seed -> {player: row} of the start save
    paired = defaultdict(dict)      # (label, metric) -> {(arm, seed): deep - baseline}
    n_games = 0
    for label, kind, arm, seed, path in games(run, a.venue, labels):
        rows = nations(read_save(path))
        n_games += 1
        start = run / "europe-starts" / ("seed-%d.sav.zst" % seed)
        drows = []                  # this game's rows minus the T150 start
        if start.exists():
            if seed not in starts:
                starts[seed] = {r["player"]: r for r in nations(read_save(start))}
            for r in rows:
                s0 = starts[seed].get(r["player"])
                if s0 is None:
                    continue
                d = dict(r)
                for m in METRICS + ["dens"]:
                    d[m] = r[m] - s0[m]
                d["dists"] = r["dists"]     # lengths are not differenced
                drows.append(d)
                deltas[(label, kind, arm, r["ai_type"])].append(d)
        if kind == "mixed":
            # Pair on the change from T150 when the start is known: the
            # deep and baseline seats are different nations, and the
            # start sets their size.
            use = drows or rows
            sd = summarise([r for r in use if r["ai_type"] == "deep"])
            sb = summarise([r for r in use if r["ai_type"] == "baseline"])
            if sd and sb:
                for m in METRICS + ["dens"]:
                    paired[(label, m)][(arm, seed)] = sd[m] - sb[m]
                if sd["routes"] > 0 and sb["routes"] > 0:
                    paired[(label, "dist")][(arm, seed)] = sd["dist"] - sb["dist"]
    if not n_games:
        sys.exit("no finished %s games with final.sav.zst under %s" % (a.venue, run))
    print("europemeter: %s venue %s, %d games, FAR=%d tiles" % (run, a.venue, n_games, FAR))
    print("\nPER NATION (mean over nations; dist = mean/max route length in tiles)")
    for key in sorted(groups):
        print(" ", fmt_summary("/".join(key), summarise(groups[key])))
    if deltas:
        print("\nCHANGE FROM THE T150 START (same columns; dist is the T250 value)")
        for key in sorted(deltas):
            print(" ", fmt_summary("/".join(key), summarise(deltas[key])))
    if paired:
        print("\nMIXED GAMES, deep seats minus baseline seats in the same game (mean +- SE over games;"
              "\n  change from T150 when the start save is present; dist only where both sides have routes)")
        for label in labels:
            cells = [(m, paired[(label, m)]) for m in METRICS + ["dens", "dist"]
                     if paired.get((label, m))]
            if not cells:
                continue
            print("  %s  (%d games)" % (label, len(cells[0][1])))
            for m, vals in cells:
                xs = list(vals.values())
                print("    %-9s %+8.3f +- %6.3f" % (m, mean(xs), se(xs)))


if __name__ == "__main__":
    main()
