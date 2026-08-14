#!/usr/bin/env python3
"""How well does the paired deep-baseline score diff at turn T predict
the final-turn diff? Validates truncated games as a tuning objective.

Pairs every deep/baseline scorelog with the same (dir, seed). Per-turn
total = sum over players of their last-known score at or before T
(same semantics as tune.py's final_score at T=end).
"""
import argparse
import math
from collections import defaultdict
from pathlib import Path

CHECKPOINTS = [50, 75, 100, 125, 150, 175, 200, 250]
RESULTS = Path(__file__).resolve().parent / "results"


def turn_totals(scorelog):
    """{turn: total-score-with-last-known-carry} for all logged turns."""
    sid = None
    per_player = defaultdict(dict)  # player -> {turn: score}
    turns = set()
    for line in open(scorelog, errors="replace"):
        p = line.split()
        if not p:
            continue
        if p[0] == "tag" and p[2] == "score":
            sid = p[1]
        elif p[0] == "turn":
            turns.add(int(p[1]))
        elif p[0] == "data" and p[2] == sid:
            per_player[p[3]][int(p[1])] = int(p[4])
    out = {}
    last = {}
    for t in sorted(turns):
        for plr, hist in per_player.items():
            if t in hist:
                last[plr] = hist[t]
        out[t] = sum(last.values())
    return out


def at(totals, t):
    ts = [x for x in totals if x <= t]
    return totals[max(ts)] if ts else 0


def pearson(xs, ys):
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    vx = sum((x - mx) ** 2 for x in xs)
    vy = sum((y - my) ** 2 for y in ys)
    return cov / math.sqrt(vx * vy) if vx and vy else float("nan")


def spearman(xs, ys):
    def ranks(v):
        order = sorted(range(len(v)), key=lambda i: v[i])
        r = [0.0] * len(v)
        for rank, i in enumerate(order):
            r[i] = rank
        return r
    return pearson(ranks(xs), ranks(ys))


def gather_pairs(root):
    """[(group, seedkey, deep_totals, base_totals)]"""
    pairs = []
    # ordinary run.sh dirs: <config>-<hash>/<arm>/seed-N
    for d in root.iterdir():
        if not d.is_dir() or "-" not in d.name:
            continue
        cfg = d.name.rsplit("-", 1)[0]
        for dsl in d.glob("deep/seed-*/scorelog.txt"):
            bsl = d / "baseline" / dsl.parts[-2] / "scorelog.txt"
            if bsl.is_file():
                pairs.append((cfg, f"{d.name}/{dsl.parts[-2]}", dsl, bsl))
    # archived campaign holdouts: cfg0=contested cfg1=primary cfg2=garden
    names = {"cfg0": "holdout-contested", "cfg1": "holdout-primary",
             "cfg2": "holdout-garden"}
    for hold in root.glob("campaign-*/results/campaign"):
        for hd in ("holdout-A", "holdout-B"):
            for dsl in (hold / hd / "holdout-report").glob(
                    "cfg*/seed-*/scorelog.txt"):
                cfg, seed = dsl.parts[-3], dsl.parts[-2]
                bsl = hold / hd / "baseline" / cfg / seed / "scorelog.txt"
                if bsl.is_file():
                    pairs.append(
                        (names[cfg], f"{hd}/{cfg}/{seed}", dsl, bsl))
    return pairs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=RESULTS, type=Path)
    args = ap.parse_args()

    groups = defaultdict(list)  # group -> [(dfinal, {t: dt})]
    for cfg, key, dsl, bsl in gather_pairs(args.root):
        dt = turn_totals(dsl)
        bt = turn_totals(bsl)
        if not dt or not bt:
            continue
        end = min(max(dt), max(bt))
        if end < 250:  # game ended early; final diff still defined
            pass
        final = at(dt, end) - at(bt, end)
        diffs = {t: at(dt, t) - at(bt, t) for t in CHECKPOINTS if t < end}
        groups[cfg].append((final, diffs))

    hdr = f"{'group':18s} {'n':>4s}  " + "  ".join(
        f"T{t:<3d} r/rho/sign" for t in (100, 150, 200))
    print(hdr)
    for cfg in sorted(groups, key=lambda c: -len(groups[c])):
        rows = groups[cfg]
        if len(rows) < 8:
            continue
        cells = []
        for t in (100, 150, 200):
            sub = [(d[t], f) for f, d in rows if t in d]
            if len(sub) < 8:
                cells.append("      -      ")
                continue
            xs, ys = zip(*sub)
            r = pearson(xs, ys)
            rho = spearman(xs, ys)
            sign = sum(1 for x, y in zip(xs, ys)
                       if (x > 0) == (y > 0)) / len(xs)
            cells.append(f"{r:+.2f}/{rho:+.2f}/{sign:.0%}")
        print(f"{cfg:18s} {len(rows):>4d}  " + "  ".join(cells))


if __name__ == "__main__":
    main()
