#!/usr/bin/env python3
"""Paired A/B compare of two benchmark result dirs on the same seeds.

    paircmp.py OFF_DIR ON_DIR [--arm deep]

Per seed, the arm's seats are averaged (four deep seats per game, no
opponent variance -- 00-context section 6), then the ON-OFF difference
is taken seed by seed. Reports mean, paired se, t and the win count.
Log counters (per game, all seats) come from game.log.

Seats are filtered to the benchmark players P1..Pn. A civil-war
splinter joins mid-game under a ruleset leader name and scores far
below the seats it split from (seed 13 of one 20-game batch: 97
against 158/182/103/177), so averaging it in moves a cell's mean by
tenths of a point in whichever arm happened to fracture. With the
filter this reproduces STATUS's published hull-pricing table exactly
(196.25 -> 198.68, +2.42, se 3.14, t +0.77, 12/20).
"""
import csv
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

COLS = ["score", "cities", "unitsbuilt", "techs", "pop", "mean_city_size",
        "settlers", "landarea", "munits", "wonders"]


def metrics(root, arm):
    """{seed: {col: mean over that arm's seats}}"""
    out = defaultdict(lambda: defaultdict(list))
    with open(Path(root) / "metrics.csv") as fh:
        for row in csv.DictReader(fh):
            # Civil-war splinters join mid-game with a low score and are
            # NOT one of the arm's seats; benchmark players are P1..Pn.
            if row["arm"] != arm or not re.fullmatch(r"P\d+", row["name"]):
                continue
            for c in COLS:
                if row.get(c) not in (None, ""):
                    out[int(row["seed"])][c].append(float(row[c]))
    return {s: {c: mean(v) for c, v in d.items() if v} for s, d in out.items()}


def logcounts(root, arm, pats):
    """{seed: {name: count}} from each game.log."""
    out = {}
    for d in sorted((Path(root) / arm).glob("seed-*")):
        log = d / "game.log"
        if not log.exists():
            continue
        text = log.read_text(errors="replace")
        out[int(d.name.split("-")[1])] = {
            name: len(re.findall(pat, text)) for name, pat in pats.items()}
    return out


def report(name, pairs):
    """pairs: list of (off, on)"""
    diffs = [b - a for a, b in pairs]
    n = len(diffs)
    if n < 2:
        return
    m, se = mean(diffs), stdev(diffs) / (n ** 0.5)
    t = m / se if se else float("nan")
    wins = sum(1 for d in diffs if d > 0)
    print(f"    {name:<22} {mean(a for a, _ in pairs):>9.2f} "
          f"{mean(b for _, b in pairs):>9.2f} {m:>+9.2f} {se:>7.2f} "
          f"{t:>+6.2f}   {wins}/{n}")


def main():
    argv = list(sys.argv[1:])
    # Extra counters, so an epic can pair its OWN execution counters
    # without editing this file: --pat NAME=REGEX, repeatable. The
    # standing lift/water-gate set below is kept because four
    # workstreams' tables were produced with it.
    extra = {}
    while "--pat" in argv:
        i = argv.index("--pat")
        name, _, rx = argv[i + 1].partition("=")
        extra[name] = rx
        del argv[i:i + 2]
    off, on = argv[0], argv[1]
    arm = argv[3] if len(argv) > 3 else "deep"
    mo, mn = metrics(off, arm), metrics(on, arm)
    seeds = sorted(set(mo) & set(mn))
    print(f"paired seeds: {len(seeds)}  (off {len(mo)}, on {len(mn)})")
    print(f"    {'metric':<22} {'OFF':>9} {'ON':>9} {'delta':>9} {'se':>7} "
          f"{'t':>6}   wins")
    for c in COLS:
        pairs = [(mo[s][c], mn[s][c]) for s in seeds
                 if c in mo[s] and c in mn[s]]
        report(c, pairs)

    pats = {"gate-refusals": r"deep-watergate turn=",
            "gate-second": r"deep-watergate-second turn=",
            "lift-arrived": r"deep-lift-op turn=\S+ player=\"[^\"]+\" op=\d+ arrive",
            "lift-formed": r"deep-lift-op turn=\S+ player=\"[^\"]+\" op=\d+ form"}
    pats.update(extra)
    lo, ln = logcounts(off, arm, pats), logcounts(on, arm, pats)
    print()
    for k in pats:
        pairs = [(lo[s][k], ln[s][k]) for s in seeds if s in lo and s in ln]
        report(k, pairs)


main()
