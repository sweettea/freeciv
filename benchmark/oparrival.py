#!/usr/bin/env python3
"""The op-arrival funnel: what each factorial cell did to the journey.

    oparrival.py funnel CELLDIR [CELLDIR...]
    oparrival.py march  CELLDIR [CELLDIR...]

A CELLDIR is any directory with `*/seed-*/game.log[.zst]` under it.

WHY A SEPARATE CENSUS FROM opxrate.py. opxrate.py builds a per-operation
ledger keyed on (arm, seed, player, slot) and answers "what did the
launch model predict for this operation and what happened to it". This
one answers a narrower question the factorial needs and opxrate.py
cannot: how each CELL moved the funnel, and what the per-turn
deep-march line says about convergence. The two agree on the funnel
columns by construction -- they count the same verbs -- and this file
exists so the A/B report does not depend on the ledger's episode
reconstruction surviving a log filter.

WHAT IS COUNTED

  formed    deep-op ... form / muster           an operation exists
  launched  deep-op ... march                   a wave left the staging
  patience  ... patience=1 on that line         released by the clock,
                                                not by the model
  etacap    deep-netval ... etacap              refused by the eta cap
  strike    deep-op ... strike                  the wave is at the walls
  take      deep-op ... consolidate             the target became ours
                                                while the op was live
  terminal  deep-op ... <verb>                  how the operation ended

  kills/losses/deaths come from the deep-employop ledger when it
  survived the filter. `kills` is booked ONLY at the ordered-attack
  seat, so it counts the kills the operations layer DECIDED, not every
  kill its blades made -- see findings/op-exchange-rate-2026-08-29.md
  section 5.

The per-cell numbers are printed with their per-seed spread, because
operation counts are wildly overdispersed across seeds: the archived
20-seed holdout ran from 0 formations on one seed to 38 on another.
A cell difference smaller than that spread is not a cell difference.
"""
import re
import statistics
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

RE_MARCH_LINE = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) march target=(\d+)'
    r' present=(\d+) required=(\d+) waited=(\d+) eta=(\d+) odds=([\d.]+)'
    r' surv=([-\d.]+) bar=(\d+) loss=([-\d.]+) cost=([-\d.]+)'
    r' eworth=([-\d.]+) patience=(\d+)(?: arrive=([\d.]+))?')
RE_DEEPMARCH = re.compile(
    r'deep-march turn=(\d+) player="([^"]+)" op=(\d+) target=(\d+)'
    r' marched=(\d+) eta=(\d+) members=(\d+) required=(\d+)'
    r' hp=(\d+) hpmax=(\d+) dist=(-?\d+) near=(-?\d+)')
RE_VERB = re.compile(
    r'deep-op turn=\d+ player="[^"]+" op=\d+ ([a-z-]+) ')
RE_DISSOLVE = re.compile(
    r'deep-op turn=\d+ player="[^"]+" op=\d+ dissolve [^ ]* ?why="([a-z-]+)"')
RE_ETACAP = re.compile(r'deep-netval turn=\d+ [^\n]* etacap ')
RE_NETREF = re.compile(r'deep-netval turn=\d+ [^\n]* refuse ')
RE_EMPLOY = re.compile(
    r'deep-employop [^\n]*?\bkills=(\d+)\b[^\n]*?\blosses=(\d+)\b')


def lines(path):
    if path.suffix == ".zst":
        out = subprocess.run(["zstdcat", str(path)], capture_output=True)
        text = out.stdout.decode("utf-8", "replace")
    else:
        text = path.read_text("utf-8", "replace")
    return text.splitlines()


def games(root):
    """Both layouts: CELL/ARM/seed-N (what the driver writes) and
    CELL/seed-N (what an ad-hoc probe writes)."""
    root = Path(root)
    seen = set()
    for pattern in ("*/seed-*", "seed-*"):
        for d in sorted(root.glob(pattern)):
            for name in ("game.log", "game.log.zst"):
                p = d / name
                if p.exists() and d.name not in seen:
                    seen.add(d.name)
                    yield d.name, p
                    break


def census(root):
    """Per seed: the funnel counters and the march trajectory."""
    per_seed = {}
    marchlines = []
    launchlines = []
    terminals = Counter()
    for seed, path in games(root):
        c = Counter()
        for ln in lines(path):
            m = RE_VERB.search(ln)
            if m:
                c[m.group(1)] += 1
                terminals[m.group(1)] += 1
            m = RE_MARCH_LINE.search(ln)
            if m:
                launchlines.append(m.groups())
                if m.group(15) == "1":
                    c["patience"] += 1
            m = RE_DEEPMARCH.search(ln)
            if m:
                marchlines.append(tuple(int(x) for x in m.groups()[4:]))
            if RE_ETACAP.search(ln):
                c["etacap"] += 1
            if RE_NETREF.search(ln):
                c["netrefuse"] += 1
        per_seed[seed] = c
    return per_seed, launchlines, marchlines, terminals


def col(per_seed, key):
    v = [c[key] for c in per_seed.values()]
    if not v:
        return 0.0, 0.0
    m = statistics.mean(v)
    sd = statistics.stdev(v) if len(v) > 1 else 0.0
    return m, sd


def do_funnel(roots):
    hdr = ("cell", "games", "formed", "launch", "patience%", "etacap",
           "strike", "take", "abort-odds", "abort-wait", "abort-march")
    print("%-8s %5s %8s %8s %9s %7s %7s %7s %10s %10s %11s" % hdr)
    for root in roots:
        per_seed, launches, marches, terms = census(root)
        n = len(per_seed)
        if n == 0:
            continue
        formed = sum(c["form"] + c["muster"] for c in per_seed.values())
        launch = sum(c["march"] for c in per_seed.values())
        pat = sum(c["patience"] for c in per_seed.values())
        print("%-8s %5d %8d %8d %8.1f%% %7d %7d %7d %10d %10d %11d" % (
            Path(root).name, n, formed, launch,
            100.0 * pat / launch if launch else 0.0,
            sum(c["etacap"] for c in per_seed.values()),
            sum(c["strike"] for c in per_seed.values()),
            sum(c["consolidate"] for c in per_seed.values()),
            sum(c["abort-odds"] for c in per_seed.values()),
            sum(c["abort-wait"] for c in per_seed.values()),
            sum(c["abort-march"] for c in per_seed.values())))
    print()
    print("per-seed mean +- sd (the spread is the point: operation counts"
          " are overdispersed)")
    print("%-8s %16s %16s %16s %16s" % ("cell", "formed/game",
                                        "launch/game", "strike/game",
                                        "take/game"))
    for root in roots:
        per_seed, _, _, _ = census(root)
        if not per_seed:
            continue
        f = [c["form"] + c["muster"] for c in per_seed.values()]
        row = [(statistics.mean(f),
                statistics.stdev(f) if len(f) > 1 else 0.0)]
        for k in ("march", "strike", "consolidate"):
            row.append(col(per_seed, k))
        print("%-8s %s" % (Path(root).name,
                           " ".join("%8.2f+-%-6.2f" % t for t in row)))
    print()
    print("launch eta histogram and what it converted (pooled)")
    print("%-8s %s" % ("cell", "  eta0-1   eta2   eta3-4    eta5+"
                       "   mean-eta  mean-odds  arrive"))
    for root in roots:
        _, launches, _, _ = census(root)
        if not launches:
            continue
        buckets = Counter()
        etas, odds, arr = [], [], []
        for g in launches:
            e = int(g[7])
            etas.append(e)
            odds.append(float(g[8]))
            if g[15] is not None:
                arr.append(float(g[15]))
            buckets["0-1" if e <= 1 else "2" if e == 2
                    else "3-4" if e <= 4 else "5+"] += 1
        print("%-8s %8d %6d %8d %8d %10.2f %10.3f %7.3f" % (
            Path(root).name, buckets["0-1"], buckets["2"], buckets["3-4"],
            buckets["5+"], statistics.mean(etas), statistics.mean(odds),
            statistics.mean(arr) if arr else 1.0))


def do_march(roots):
    """The convergence curve, from the deep-march per-turn line."""
    print("the march, by turns since launch (pooled over the cell)")
    print("%-8s %8s %7s %8s %8s %8s %8s" % (
        "cell", "marched", "n", "eta", "members", "hp/hpmax", "dist"))
    for root in roots:
        _, _, marches, _ = census(root)
        if not marches:
            continue
        by = defaultdict(list)
        for (marched, eta, members, required,
             hp, hpmax, dist, near) in marches:
            by[min(marched, 6)].append((eta, members, hp, hpmax, dist))
        for k in sorted(by):
            v = by[k]
            print("%-8s %8s %7d %8.2f %8.2f %8.3f %8.2f" % (
                Path(root).name, k if k < 6 else "6+", len(v),
                statistics.mean(x[0] for x in v),
                statistics.mean(x[1] for x in v),
                (sum(x[2] for x in v) / sum(x[3] for x in v)
                 if sum(x[3] for x in v) else 0.0),
                statistics.mean(x[4] for x in v)))
        print()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    mode, roots = sys.argv[1], sys.argv[2:]
    if mode == "funnel":
        do_funnel(roots)
    elif mode == "march":
        do_march(roots)
    else:
        sys.exit(__doc__)
