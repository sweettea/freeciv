#!/usr/bin/env python3
"""How well does the paired deep-baseline score diff at turn T predict
the final-turn diff? Validates truncated games as a tuning objective.

Pairs every deep/baseline scorelog with the same (dir, seed). Per-turn
total = sum over players of their last-known score at or before T
(same semantics as tune.py's final_score at T=end).

--layout tune additionally walks tune.py's own campaign layout
(out/<stage>/<block>/gen*/cand*/cfg{c}/seed-{s} against the stage's
shared out/<stage>/baseline/cfg{c}/seed-{s}), and its --mixed form
(.../cfg{c}/mixed_db|mixed_bd/seed-{s}), where the gap is the
within-game deep-side minus baseline-side total that tune.py's
mixed_sides() computes.  That is the only way to read a truncated
objective off a campaign that has already been paid for.

--cost adds the truncation-cost half of the question: a T-truncated
venue is only a better objective if it buys more precision per second.
Per game it reads the wall clock off the save mtimes (T1 autosave to
final save) and estimates the share of that clock spent at or before
turn T from a WORK PROXY -- the running sum over turns of every
player's units and cities, which the scorelog already carries -- with
the proxy's own fit against measured duration reported beside it so
the estimate can be judged rather than believed.
"""
import argparse
import math
import re
from collections import defaultdict
from pathlib import Path

CHECKPOINTS = [50, 60, 75, 80, 100, 120, 125, 150, 175, 200, 250]
RESULTS = Path(__file__).resolve().parent / "results"


def read_scorelog(scorelog, want_work=False):
    """(names, {tag: {plr: {turn: val}}}, turns) -- one parse, reused.

    `names` maps player number to the scorelog name, which is what
    separates the two sides of a --mixed game (tune.py mixed_sides()).
    Work-proxy tags are collected only when asked for: they double the
    parse cost and most callers do not want them."""
    tags, names, data, turns = {}, {}, {}, set()
    keep = {"score"} | ({"munits", "cities"} if want_work else set())
    ids = {}
    for line in open(scorelog, errors="replace"):
        p = line.split()
        if not p:
            continue
        if p[0] == "tag":
            tags[p[2]] = p[1]
            if p[2] in keep:
                ids[p[1]] = p[2]
        elif p[0] == "addplayer":
            names[p[2]] = " ".join(p[3:]).strip()
        elif p[0] == "turn":
            turns.add(int(p[1]))
        elif p[0] == "data":
            nm = ids.get(p[2])
            if nm is not None:
                data.setdefault(nm, {}).setdefault(p[3], {})[int(p[1])] = int(p[4])
    return names, data, turns


def _carry(per_player, turns, keys=None):
    """{turn: sum of last-known value} over the selected players."""
    out, last = {}, {}
    for t in sorted(turns):
        for plr, hist in per_player.items():
            if keys is not None and plr not in keys:
                continue
            if t in hist:
                last[plr] = hist[t]
        out[t] = sum(last.values())
    return out


def turn_totals(scorelog):
    """{turn: total-score-with-last-known-carry} for all logged turns."""
    _names, data, turns = read_scorelog(scorelog)
    return _carry(data.get("score", {}), turns)


def mixed_turn_gap(scorelog, arm):
    """{turn: deep-side total - baseline-side total} of one mixed game.

    Seat parity decides the side, exactly as tune.py mixed_sides()
    does; civil-war splinters (non-P names) belong to neither."""
    names, data, turns = read_scorelog(scorelog)
    score = data.get("score", {})
    deep, base = set(), set()
    for num, nm in names.items():
        if not re.fullmatch(r"P\d+", nm):
            continue
        odd = int(nm[1:]) % 2 == 1
        (deep if odd == (arm == "mixed_db") else base).add(num)
    dt, bt = _carry(score, turns, deep), _carry(score, turns, base)
    return {t: dt[t] - bt[t] for t in dt}


def work_curve(scorelog):
    """{turn: cumulative units+cities over all players up to that turn}.

    The server's per-turn cost is dominated by per-unit and per-city
    work (AI phases, pathfinding, city arrangement), so the running sum
    is a proxy for the share of a game's clock spent by turn T.  It is
    a proxy and is reported with its fit, not asserted."""
    _names, data, turns = read_scorelog(scorelog, want_work=True)
    units = _carry(data.get("munits", {}), turns)
    cities = _carry(data.get("cities", {}), turns)
    out, run = {}, 0.0
    for t in sorted(turns):
        run += units.get(t, 0) + cities.get(t, 0)
        out[t] = run
    return out


def game_seconds(seeddir):
    """Wall clock of one game from its save mtimes, or None.

    T1 autosave to the final save.  Games ran 8-at-a-time under nice
    10, so the absolute number carries scheduler contention; only the
    RATIO between venues and truncations is used, and that contention
    is common to both."""
    saves = Path(seeddir) / "saves"
    if not saves.is_dir():
        return None
    ts = sorted(f.stat().st_mtime for f in saves.iterdir())
    return (ts[-1] - ts[0]) if len(ts) >= 2 else None


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


def gather_tune(root):
    """tune.py campaign layout -> [(venue, key, deep_gap_by_turn_fn)].

    Two shapes.  PAIRED: every cand dir's cfg{c}/seed-{s} against the
    stage's own baseline/cfg{c}/seed-{s}, which tune.py runs once and
    shares across the whole stage.  MIXED: cfg{c}/mixed_db|mixed_bd/
    seed-{s}, where both arms are inside the one game.
    """
    out = []
    root = Path(root)
    for stage in sorted(d for d in root.iterdir() if d.is_dir()):
        base_root = stage / "baseline"
        for sl in stage.rglob("cfg*/seed-*/scorelog.txt"):
            parts = sl.parts
            cfg, seed = parts[-3], parts[-2]
            if "baseline" in parts:
                continue
            arm = None
            bsl = base_root / cfg / seed / "scorelog.txt"
            if not bsl.is_file():
                continue
            out.append((stage.name, cfg, f"{sl.parent}", sl, bsl, None))
        for sl in stage.rglob("cfg*/mixed_*/seed-*/scorelog.txt"):
            parts = sl.parts
            cfg, arm = parts[-4], parts[-3]
            out.append((stage.name, cfg, f"{sl.parent}", sl, None, arm))
    return out


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


def _tune_row(job):
    """One campaign game -> (venue, {turn: gap}, final gap, seconds, work)."""
    stage, cfg, key, dsl, bsl, arm, want_cost = job
    try:
        if arm is not None:
            gaps = mixed_turn_gap(dsl, arm)
        else:
            dt, bt = turn_totals(dsl), turn_totals(bsl)
            if not dt or not bt:
                return None
            end = min(max(dt), max(bt))
            gaps = {t: at(dt, t) - at(bt, t)
                    for t in sorted(dt) if t <= end}
        if not gaps:
            return None
        secs = work = None
        if want_cost:
            secs = game_seconds(Path(dsl).parent)
            work = work_curve(dsl)
        return (f"{stage}/{cfg}", key, gaps, secs, work)
    except (OSError, ValueError, KeyError):
        return None


def summarise(venue, rows, checkpoints, cost_rows):
    """Print one venue's correlation and truncation-cost bracket.

    The cost columns are a BRACKET, not an estimate.  Two models of how
    a game's wall clock is spread over its turns bound the truth from
    either side: turn-linear (every turn costs the same, T/end) and
    work-weighted (cost tracks units+cities, W(T)/W(end)).  The truth is
    between them because per-turn cost is a fixed overhead plus a term
    that grows with the empire.  The work model is reported with its own
    fit against measured seconds so it is never mistaken for a
    measurement -- see --cost's header line."""
    finals = [max(g) for _k, g, _s, _w in rows]
    end = max(finals)
    ys = [g[max(g)] for _k, g, _s, _w in rows]
    n = len(ys)
    sd_final = math.sqrt(sum((y - sum(ys) / n) ** 2 for y in ys) / (n - 1)) \
        if n > 1 else float("nan")
    secs = [x for _k, _g, x, _w in rows]
    good = [x for x in secs if x]
    clock = f"  median {sorted(good)[len(good)//2]:.0f}s/game" if good else ""
    print(f"\n{venue}  n={n}  final turn={end}  "
          f"sigma(final gap)={sd_final:.1f}  SE@8={sd_final/math.sqrt(8):.1f}"
          f"  SE@20={sd_final/math.sqrt(20):.1f}{clock}")
    if cost_rows and any(cost_rows):
        r_fit, nfit = cost_fit(secs, [w for _k, _g, _s, w in rows])
        print(f"  work proxy fit within this venue: r={r_fit:+.3f} "
              f"(n={nfit}) -- the cost columns are a bracket, not a fit")
    print(f"  {'T':>5s} {'n':>5s} {'r':>7s} {'rho':>7s} {'sign':>6s} "
          f"{'sigma_T':>8s} {'SE@8':>7s} {'clock share':>16s} "
          f"{'eff vs full':>14s}")
    for t in checkpoints:
        sub = [(g[max(x for x in g if x <= t)], g[max(g)])
               for _k, g, _s, _w in rows if any(x <= t for x in g)]
        if len(sub) < 8 or t > end:
            continue
        xs, yy = zip(*sub)
        r = pearson(list(xs), list(yy))
        rho = spearman(list(xs), list(yy))
        sign = sum(1 for x, y in zip(xs, yy) if (x > 0) == (y > 0)) / len(xs)
        m = sum(xs) / len(xs)
        sdt = math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))
        sh_w = clock_share(cost_rows, t, end)
        sh_l = min(1.0, t / end)
        if sh_w:
            lo, hi = min(sh_w, sh_l), max(sh_w, sh_l)
            sh = f"{lo:.2f}-{hi:.2f}".rjust(16)
            ef = f"{r*r/hi:.2f}-{r*r/lo:.2f}".rjust(14)
        else:
            sh = f"{sh_l:16.2f}"
            ef = f"{r*r/sh_l:14.2f}"
        print(f"  {t:5d} {len(xs):5d} {r:+7.2f} {rho:+7.2f} {sign:6.0%} "
              f"{sdt:8.1f} {sdt/math.sqrt(8):7.1f} {sh} {ef}")


def clock_share(cost_rows, t, end):
    """Mean share of a game's wall clock spent at or before turn T.

    Work-proxy weighted: W(T)/W(end) averaged over the games that
    carried a work curve.  None when no cost data was collected."""
    vals = []
    for w in cost_rows:
        if not w:
            continue
        ts = [x for x in w if x <= t]
        if not ts or not w:
            continue
        tot = w[max(w)]
        if tot > 0:
            vals.append(w[max(ts)] / tot)
    return sum(vals) / len(vals) if vals else None


def cost_fit(secs, works):
    """Pearson r of measured seconds against total work, as the proxy's
    own credential."""
    pairs = [(w[max(w)], s) for w, s in zip(works, secs)
             if w and s is not None and s > 0]
    if len(pairs) < 8:
        return float("nan"), 0
    xs, ys = zip(*pairs)
    return pearson(list(xs), list(ys)), len(pairs)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=RESULTS, type=Path)
    ap.add_argument("--layout", choices=("classic", "tune"), default="classic")
    ap.add_argument("--checkpoints", default="60,80,100,120")
    ap.add_argument("--cost", action="store_true",
                    help="also estimate the wall-clock share of each "
                         "truncation from save mtimes + the work proxy")
    ap.add_argument("--jobs", type=int, default=1)
    args = ap.parse_args()

    if args.layout == "tune":
        checkpoints = [int(x) for x in args.checkpoints.split(",")]
        jobs = [(st, cfg, k, d, b, a, args.cost)
                for st, cfg, k, d, b, a in gather_tune(args.root)]
        if args.jobs > 1:
            import multiprocessing as mp
            with mp.get_context("fork").Pool(args.jobs) as pool:
                res = pool.map(_tune_row, jobs, chunksize=8)
        else:
            res = [_tune_row(j) for j in jobs]
        groups = defaultdict(list)
        for r in res:
            if r is None:
                continue
            venue, key, gaps, secs, work = r
            groups[venue].append((key, gaps, secs, work))
        allsecs = [s for g in groups.values() for _k, _g, s, _w in g]
        allwork = [w for g in groups.values() for _k, _g, _s, w in g]
        if args.cost:
            r, nfit = cost_fit(allsecs, allwork)
            good = [x for x in allsecs if x]
            if good:
                print(f"work proxy vs measured seconds, POOLED: r={r:+.3f} "
                      f"over {nfit} games; median game "
                      f"{sorted(good)[len(good)//2]:.0f}s.")
                print("Wall clock here is 8-way-contended nice-10 time, so "
                      "it carries scheduler noise; the work model is used "
                      "only as one side of a bracket.")
            else:
                print("no timing data")
        for venue in sorted(groups, key=lambda v: -len(groups[v])):
            rows = groups[venue]
            if len(rows) < 8:
                continue
            summarise(venue, rows, checkpoints,
                      [w for _k, _g, _s, w in rows])
        print(f"\ntotal games paired: {sum(len(g) for g in groups.values())}")
        return

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
