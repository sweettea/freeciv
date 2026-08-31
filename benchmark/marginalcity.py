#!/usr/bin/env python3
"""What a marginal city is worth, by founding band (S-MARGINAL).

    marginalcity.py value  --root LABEL=DIR [...] [--at 150] [--at 80]
    marginalcity.py yield  --deep LABEL=DIR --base LABEL=DIR [...]
    marginalcity.py arms   --arm NAME=TREAT:CTRL [...] [--at 150]
    marginalcity.py power  --arm NAME=TREAT:CTRL [...]

Four expansion epics (S-FEARSEAT, S-SITEPLAN, S-FEARSINK, S-FRONTIER)
each produced a clean mechanism, +0.09..+0.19 cities per seat at T50
decaying to +0.03..+0.15 at T75, and a score delta inside +/-2 with an
SE of 1.5-2.2.  All four verdicts read that as a *conserved founder
queue*: foundings moved in time, none added, marginal city worth ~0.

This tool tests the competing reading -- that the effects are real,
small, and below the power of a 20-seed cell -- by measuring the thing
neither reading ever put a number on: **how much end score a city
founded in a given 25-turn band is actually worth.**  With that number
a city delta becomes a *predicted* score delta which can be compared
against the observed one and its SE, and the power question ("what n
would have seen it?") becomes arithmetic.

No games are run.  Everything here is read from ``scorelog.txt``, which
every retained corpus has, for ``ai/baseline`` arms as well as
``ai/deep`` ones.

---------------------------------------------------------------- method

``value`` -- the observational estimator.  One observation per SEAT per
game.  Regress an end-of-game outcome on the number of cities that seat
founded in each 25-turn band, with a fixed effect for (arm, seed) so
that only *within-game, between-seat* variation identifies the
coefficient -- the map, the ruleset, the turn count and the opponent
mix all cancel exactly.  Conquests and losses enter as their own
regressors so that a seat which took its cities is not credited to
foundings.  SEs are cluster-robust on (arm, seed).

**This is a correlation and it is an upper bound.**  A seat that founds
more cities in T51-75 is also a seat with better land, and the land is
not in the regression.  The number to trust as *causal* is the ``arms``
meta-regression below; ``value`` is what says whether the causal
estimate is even in the plausible range.

``arms`` -- the experimental estimator.  Every A/B contrast this
project has run is a randomised (well, seed-paired) shock to the city
count.  Regressing each contrast's paired score delta on its paired
city delta, across contrasts, estimates the same coefficient with the
treatment as the instrument.  Small n (one point per arm x venue), but
identified.

``power`` -- per-seed paired sigma for score and for cities, and the n
each would need to detect a given effect at z = 2.  The interesting
output is the *ratio*: if a city is worth V points and the paired sigma
of the city count is much less than sigma(score)/V, then the city count
is a strictly better-powered instrument for an expansion lever than the
score is, and every one of the four epics was reading the wrong meter.
"""

import argparse
import hashlib
import math
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from cityledger import read_scorelog, scorelog_events  # noqa: E402

TAGS = {"pop": 0, "cities": 3, "techs": 4, "settlers": 6,
        "score": 25, "culture": 29, "unitslost": 28}
# tag 0 "pop" is get_pop() == total_player_citizens() (server/report.c:691),
# i.e. the first term of get_civ_score() -- citizens, not thousands.

BAND_W = 25


def bands_for(last_turn, width=BAND_W):
    out, lo = [], 1
    while lo <= last_turn:
        out.append((lo, min(lo + width - 1, last_turn)))
        lo += width
    return out


def band_name(b):
    return f"T{b[0]}-{b[1]}"


# ----------------------------------------------------------------- reader

def read_tag(path, tag):
    """-> {turn: {player_idx: value}} for one scorelog tag."""
    out = defaultdict(dict)
    want = str(tag)
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.startswith("data "):
                f = line.split()
                if len(f) == 5 and f[2] == want:
                    out[int(f[1])][int(f[3])] = int(f[4])
    return out


def at_turn(series, turn):
    """Value at the last sample <= turn, per player."""
    ts = [t for t in series if t <= turn]
    if not ts:
        return {}
    return series[max(ts)]


def read_game(gdir, width=BAND_W):
    """-> dict for one game: per-seat band foundings and outcome series.

    Returns None when the game has no usable scorelog.
    """
    sl = gdir / "scorelog.txt"
    if not sl.exists():
        return None
    names, cities, turns = read_scorelog(sl)
    if not turns:
        return None
    last = max(turns)
    seats = sorted(cities)
    bands = bands_for(last, width)

    found = {p: [0] * len(bands) for p in seats}
    taken = {p: 0 for p in seats}
    lost = {p: 0 for p in seats}
    ambig = 0
    for turn, p, kind, n in scorelog_events(cities, turns):
        if kind == "ambig":
            ambig += n
            continue
        if p not in found:
            continue
        if kind == "found":
            for i, (lo, hi) in enumerate(bands):
                if lo <= turn <= hi:
                    found[p][i] += n
                    break
        elif kind == "taken":
            taken[p] += n
        else:                                   # destroyed / transferred
            lost[p] += n

    series = {k: read_tag(sl, t) for k, t in TAGS.items()}
    return {"dir": gdir, "seats": seats, "bands": bands, "last": last,
            "found": found, "taken": taken, "lost": lost, "ambig": ambig,
            "series": series, "cities": cities}


def scan_root(root, width=BAND_W, arm_of=None):
    """-> list of games under root (any depth; a game dir has scorelog.txt).

    ``root`` may be a comma-separated list of directories; each becomes its
    own arm label so that the fixed effect still absorbs it.
    """
    games = []
    for one in str(root).split(","):
        one = one.strip()
        if not one:
            continue
        tag = arm_of if arm_of else "/".join(Path(one).resolve().parts[-4:])
        for sl in sorted(Path(one).rglob("scorelog.txt")):
            g = read_game(sl.parent, width)
            if g is None:
                continue
            g["seed"] = seed_of(sl.parent)
            g["arm"] = tag
            games.append(g)
    return games


SEED_RE = re.compile(r"seed-(\d+)")


def seed_of(gdir):
    m = SEED_RE.search(gdir.name)
    return int(m.group(1)) if m else gdir.name


def digest(gdir):
    """Content hash of a scorelog with the per-run random ``id`` removed."""
    h = hashlib.sha256()
    with open(gdir / "scorelog.txt", "rb") as fh:
        for line in fh:
            if line.startswith(b"id "):
                continue
            h.update(line)
    return h.hexdigest()[:16]


# -------------------------------------------------------------- linear alg

def matinv(a):
    n = len(a)
    m = [row[:] + [1.0 if i == j else 0.0 for j in range(n)]
         for i, row in enumerate(a)]
    for c in range(n):
        piv = max(range(c, n), key=lambda r: abs(m[r][c]))
        if abs(m[piv][c]) < 1e-12:
            raise ValueError("singular design matrix")
        m[c], m[piv] = m[piv], m[c]
        d = m[c][c]
        m[c] = [v / d for v in m[c]]
        for r in range(n):
            if r == c:
                continue
            f = m[r][c]
            if f:
                m[r] = [vr - f * vc for vr, vc in zip(m[r], m[c])]
    return [row[n:] for row in m]


def ols_cluster(y, X, groups):
    """OLS with cluster-robust SEs.  X has no intercept (data are demeaned).

    Columns with no remaining variance (a band in which nobody founded
    anything, e.g. the one-turn tail band of a T151 game) are dropped and
    reported back as an exact zero rather than making the design singular.
    """
    n, kk = len(y), len(X[0])
    keep = [a for a in range(kk)
            if sum(X[i][a] * X[i][a] for i in range(n)) > 1e-9]
    if len(keep) < kk:
        Xs = [[row[a] for a in keep] for row in X]
        b, se, n_, ng_ = ols_cluster(y, Xs, groups)
        bo, so = [0.0] * kk, [0.0] * kk
        for j, a in enumerate(keep):
            bo[a], so[a] = b[j], se[j]
        return bo, so, n_, ng_
    k = kk
    xtx = [[sum(X[i][a] * X[i][b] for i in range(n)) for b in range(k)]
           for a in range(k)]
    xty = [sum(X[i][a] * y[i] for i in range(n)) for a in range(k)]
    inv = matinv(xtx)
    beta = [sum(inv[a][b] * xty[b] for b in range(k)) for a in range(k)]
    res = [y[i] - sum(X[i][a] * beta[a] for a in range(k)) for i in range(n)]
    by = defaultdict(list)
    for i, g in enumerate(groups):
        by[g].append(i)
    meat = [[0.0] * k for _ in range(k)]
    for idx in by.values():
        s = [sum(X[i][a] * res[i] for i in idx) for a in range(k)]
        for a in range(k):
            for b in range(k):
                meat[a][b] += s[a] * s[b]
    ng = len(by)
    corr = (ng / max(ng - 1, 1)) * ((n - 1) / max(n - k, 1))
    v = [[corr * sum(inv[a][c] * meat[c][d] * inv[d][b]
                     for c in range(k) for d in range(k))
          for b in range(k)] for a in range(k)]
    se = [math.sqrt(max(v[a][a], 0.0)) for a in range(k)]
    return beta, se, n, ng


def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def sd(xs):
    if len(xs) < 2:
        return float("nan")
    m = mean(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def sem(xs):
    return sd(xs) / math.sqrt(len(xs)) if len(xs) > 1 else float("nan")



# ------------------------------------------------------- shared estimator

def value_betas(games, hz, outcome="score", width=BAND_W):
    """Within-(arm,seed) OLS of `outcome` at T`hz` on band foundings.

    -> (band list used, beta, se, n seats, n groups).  Regressors are the
    foundings in every band that has CLOSED by `hz`, plus conquests and
    losses, so a seat that took its cities is not credited to founding.
    """
    last = max(g["last"] for g in games)
    bands = bands_for(last, width)
    use = [i for i, b in enumerate(bands) if b[1] <= hz]
    rows, groups, y = [], [], []
    for g in games:
        out = at_turn(g["series"][outcome], hz)
        for p in g["seats"]:
            if p not in out:
                continue
            rows.append([float(g["found"][p][i]) for i in use]
                        + [float(g["taken"][p]), float(g["lost"][p])])
            groups.append((g["arm"], g["seed"]))
            y.append(float(out[p]))
    if not rows:
        return [], [], [], 0, 0
    gx = defaultdict(list)
    for i, gid in enumerate(groups):
        gx[gid].append(i)
    k = len(rows[0])
    for idx in gx.values():
        for a in range(k):
            m = mean([rows[i][a] for i in idx])
            for i in idx:
                rows[i][a] -= m
        m = mean([y[i] for i in idx])
        for i in idx:
            y[i] -= m
    beta, se, n, ng = ols_cluster(y, rows, groups)
    return [bands[i] for i in use], beta, se, n, ng


# ------------------------------------------------------------------ value

OUTCOMES = ("score", "pop", "techs", "culture", "cities")


def cmd_value(args):
    width = args.band
    for spec in args.root:
        label, root = spec.split("=", 1)
        games = scan_root(root, width)
        if not games:
            print(f"\n{label}: no games under {root}")
            continue
        last = max(g["last"] for g in games)
        horizons = args.at or [last]
        horizons = [h for h in horizons if h <= last] or [last]
        bands = bands_for(last, width)
        nb = len(bands)

        print(f"\n=== {label}   {len(games)} games, "
              f"{sum(len(g['seats']) for g in games)} seats, "
              f"last turn {last}, bands of {width}")
        arms = sorted({g["arm"] for g in games})
        print(f"    arms: {', '.join(arms)}")

        # cohort sizes
        print(f"    foundings per seat by band: " + "  ".join(
            f"{band_name(b)} {mean([g['found'][p][i] for g in games for p in g['seats']]):.3f}"
            for i, b in enumerate(bands)))
        amb = sum(g["ambig"] for g in games)
        print(f"    ambiguous birth+death turns: {amb} "
              f"({amb / len(games):.2f} a game)")

        for hz in horizons:
            rows, groups = [], []
            ys = {k: [] for k in OUTCOMES}
            drop = 0
            for g in games:
                vals = {k: at_turn(g["series"][k], hz) for k in OUTCOMES}
                # only bands that have closed by the horizon are regressors
                use = [i for i, b in enumerate(bands) if b[1] <= hz]
                for p in g["seats"]:
                    if p not in vals["score"]:
                        drop += 1
                        continue
                    rows.append([float(g["found"][p][i]) for i in use]
                                + [float(g["taken"][p]), float(g["lost"][p])])
                    groups.append((g["arm"], g["seed"]))
                    for k in OUTCOMES:
                        ys[k].append(float(vals[k].get(p, 0)))
            if not rows:
                continue
            # within-group demeaning: only between-seat variation identifies
            k = len(rows[0])
            gx = defaultdict(list)
            for i, gid in enumerate(groups):
                gx[gid].append(i)
            Xd = [r[:] for r in rows]
            yd = {kk: list(vv) for kk, vv in ys.items()}
            for idx in gx.values():
                for a in range(k):
                    m = mean([rows[i][a] for i in idx])
                    for i in idx:
                        Xd[i][a] -= m
                for kk in OUTCOMES:
                    m = mean([ys[kk][i] for i in idx])
                    for i in idx:
                        yd[kk][i] -= m

            names = [band_name(bands[i]) for i in
                     [j for j, b in enumerate(bands) if b[1] <= hz]] \
                + ["taken", "lost"]
            print(f"\n  -- value of one city at T{hz} "
                  f"(within-(arm,seed) OLS, cluster-robust SE, "
                  f"n={len(rows)} seats in {len(gx)} games, {drop} dropped)")
            head = "    " + f"{'outcome':>9} " + " ".join(
                f"{nm:>16}" for nm in names)
            print(head)
            for kk in OUTCOMES:
                try:
                    beta, se, _, _ = ols_cluster(yd[kk], Xd, groups)
                except ValueError as exc:
                    print(f"    {kk:>9} {exc}")
                    continue
                print(f"    {kk:>9} " + " ".join(
                    f"{b:>9.2f} +/-{s:5.2f}" for b, s in zip(beta, se)))


# ------------------------------------------------------------------ yield

def cmd_yield(args):
    """Same regression run separately on deep arms and on baseline arms."""
    width = args.band
    for spec_d, spec_b in zip(args.deep, args.base):
        ld, rd = spec_d.split("=", 1)
        lb, rb = spec_b.split("=", 1)
        gd = scan_root(rd, width, arm_of="deep")
        gb = scan_root(rb, width, arm_of="base")
        if not gd or not gb:
            print(f"\n{ld} vs {lb}: missing games")
            continue
        last = min(max(g["last"] for g in gd), max(g["last"] for g in gb))
        bands = bands_for(last, width)
        hz = args.at[0] if args.at else last
        print(f"\n=== {ld} (n={len(gd)}) vs {lb} (n={len(gb)})  at T{hz}")

        betas = {}
        for tag, games in (("deep", gd), ("base", gb)):
            rows, groups, y = [], [], []
            use = [i for i, b in enumerate(bands) if b[1] <= hz]
            for g in games:
                sc = at_turn(g["series"]["score"], hz)
                for p in g["seats"]:
                    if p not in sc:
                        continue
                    rows.append([float(g["found"][p][i]) for i in use]
                                + [float(g["taken"][p]), float(g["lost"][p])])
                    groups.append((g["arm"], g["seed"]))
                    y.append(float(sc[p]))
            gx = defaultdict(list)
            for i, gid in enumerate(groups):
                gx[gid].append(i)
            k = len(rows[0])
            for idx in gx.values():
                for a in range(k):
                    m = mean([rows[i][a] for i in idx])
                    for i in idx:
                        rows[i][a] -= m
                m = mean([y[i] for i in idx])
                for i in idx:
                    y[i] -= m
            betas[tag] = ols_cluster(y, rows, groups)

        names = [band_name(bands[i]) for i in
                 [j for j, b in enumerate(bands) if b[1] <= hz]] \
            + ["taken", "lost"]
        print("    " + f"{'arm':>6} " + " ".join(f"{nm:>16}" for nm in names))
        for tag in ("deep", "base"):
            b, s, _, _ = betas[tag]
            print(f"    {tag:>6} " + " ".join(
                f"{bb:>9.2f} +/-{ss:5.2f}" for bb, ss in zip(b, s)))
        bd, sd_, _, _ = betas["deep"]
        bb, sb, _, _ = betas["base"]
        print(f"    {'d-b':>6} " + " ".join(
            f"{x - yv:>9.2f} +/-{math.sqrt(a * a + c * c):5.2f}"
            for x, yv, a, c in zip(bd, bb, sd_, sb)))

        # The aggregate exchange rate, which needs no regression at all.
        # Pairing is on (deep corpus dir, seed) against the base game of
        # the same seed, so pooling several deep corpora does not silently
        # collapse them onto one another.
        def summarise(g):
            sc = at_turn(g["series"]["score"], hz)
            ci = at_turn(g["series"]["cities"], hz)
            seats = [p for p in g["seats"] if p in sc]
            if not seats:
                return None
            return (mean([sc[p] for p in seats]),
                    mean([ci.get(p, 0) for p in seats]))
        basemap = {}
        for g in gb:
            v = summarise(g)
            if v:
                basemap[g["seed"]] = v
        ds, dc = [], []
        for g in gd:
            v = summarise(g)
            b = basemap.get(g["seed"])
            if v and b:
                ds.append(v[0] - b[0])
                dc.append(v[1] - b[1])
        if ds:
            print(f"    aggregate deep-base, per seat, n={len(ds)} paired "
                  f"games: score {mean(ds):+.2f} +/-{sem(ds):.2f}, "
                  f"cities {mean(dc):+.3f} +/-{sem(dc):.3f}, "
                  f"exchange rate {mean(ds) / mean(dc):.2f} score/city")


# ------------------------------------------------------------------- arms

def arm_stats(treat, ctrl, width, hz):
    """Paired per-seed deltas between two cell dirs."""
    gt = {g["seed"]: g for g in scan_root(treat, width, arm_of="t")}
    gc = {g["seed"]: g for g in scan_root(ctrl, width, arm_of="c")}
    seeds = sorted(set(gt) & set(gc), key=str)
    if not seeds:
        return None
    last = min(min(g["last"] for g in gt.values()),
               min(g["last"] for g in gc.values()))
    hz = min(hz, last)
    bands = bands_for(last, width)
    d_found = {i: [] for i in range(len(bands))}
    d_cum = {i: [] for i in range(len(bands))}
    d_score, d_cities, d_pop = [], [], []
    for s in seeds:
        a, b = gt[s], gc[s]

        def per(g, f):
            return mean([f(g, p) for p in g["seats"]])
        for i in range(len(bands)):
            d_found[i].append(per(a, lambda g, p, i=i: g["found"][p][i])
                              - per(b, lambda g, p, i=i: g["found"][p][i]))
            d_cum[i].append(sum(d_found[j][-1] for j in range(i + 1)))
        sa, sb = at_turn(a["series"]["score"], hz), at_turn(b["series"]["score"], hz)
        ca, cb = at_turn(a["series"]["cities"], hz), at_turn(b["series"]["cities"], hz)
        pa, pb = at_turn(a["series"]["pop"], hz), at_turn(b["series"]["pop"], hz)
        d_score.append(mean([sa[p] for p in a["seats"] if p in sa])
                       - mean([sb[p] for p in b["seats"] if p in sb]))
        d_cities.append(mean([ca.get(p, 0) for p in a["seats"]])
                        - mean([cb.get(p, 0) for p in b["seats"]]))
        d_pop.append(mean([pa.get(p, 0) for p in a["seats"]])
                     - mean([pb.get(p, 0) for p in b["seats"]]))
    return {"seeds": seeds, "bands": bands, "hz": hz,
            "d_found": d_found, "d_cum": d_cum,
            "d_score": d_score, "d_cities": d_cities, "d_pop": d_pop}


def cmd_arms(args):
    width = args.band
    hz = args.at[0] if args.at else 10 ** 9
    pts = []
    for spec in args.arm:
        name, rest = spec.split("=", 1)
        treat, ctrl = rest.split(":", 1)
        st = arm_stats(treat, ctrl, width, hz)
        if st is None:
            print(f"\n{name}: no paired seeds")
            continue
        print(f"\n=== {name}   n={len(st['seeds'])} paired seeds, "
              f"horizon T{st['hz']}")
        print("    cumulative d(found) per seat:  " + "  ".join(
            f"{band_name(b)} {mean(st['d_cum'][i]):+.3f}+/-{sem(st['d_cum'][i]):.3f}"
            for i, b in enumerate(st["bands"])))
        print(f"    d(cities@T{st['hz']}) {mean(st['d_cities']):+.3f} "
              f"+/-{sem(st['d_cities']):.3f}    "
              f"d(pop) {mean(st['d_pop']):+.3f} +/-{sem(st['d_pop']):.3f}    "
              f"d(score) {mean(st['d_score']):+.3f} "
              f"+/-{sem(st['d_score']):.3f}")
        pts.append((name, st))

    if args.valroot and pts:
        vg = scan_root(args.valroot, width)
        vhz = min(min(st["hz"] for _, st in pts),
                  max(g["last"] for g in vg))
        vb, beta, se, nn, ngg = value_betas(vg, vhz)
        print(f"\n=== predicted vs observed score, using the value table from "
              f"{args.valroot}")
        print(f"    value of one city at T{vhz} ({nn} seats, {ngg} games): "
              + "  ".join(f"{band_name(b)} {beta[i]:+.2f}+/-{se[i]:.2f}"
                          for i, b in enumerate(vb)))
        print(f"\n    {'arm':>22} {'predicted':>18} {'observed':>18} "
              f"{'obs-pred':>18} {'z(pred)':>8}")
        for name, st in pts:
            nb = min(len(vb), len(st["bands"]))
            pred, pvar = 0.0, 0.0
            for i in range(nb):
                d = mean(st["d_found"][i])
                sd_ = sem(st["d_found"][i])
                pred += beta[i] * d
                pvar += (beta[i] * sd_) ** 2 + (se[i] * d) ** 2
            ps = math.sqrt(pvar)
            obs, osd = mean(st["d_score"]), sem(st["d_score"])
            diff = obs - pred
            dsd = math.sqrt(osd ** 2 + ps ** 2)
            print(f"    {name:>22} {pred:>+10.2f} +/-{ps:5.2f} "
                  f"{obs:>+10.2f} +/-{osd:5.2f} "
                  f"{diff:>+10.2f} +/-{dsd:5.2f} "
                  f"{(pred / ps if ps else 0):>8.2f}")

    if len(pts) >= 3:
        print("\n=== meta-regression: d(score) on d(cities@end) across arms")
        print("    (the experimental estimate of a city's value: the "
              "treatment is the instrument)")
        xs = [mean(st["d_cities"]) for _, st in pts]
        ys = [mean(st["d_score"]) for _, st in pts]
        ws = [1.0 / max(sem(st["d_score"]) ** 2, 1e-9) for _, st in pts]
        sxx = sum(w * x * x for w, x in zip(ws, xs))
        sxy = sum(w * x * y for w, x, y in zip(ws, xs, ys))
        if sxx > 0:
            b = sxy / sxx
            resid = [y - b * x for x, y in zip(xs, ys)]
            chi = sum(w * r * r for w, r in zip(ws, resid))
            dof = max(len(xs) - 1, 1)
            se = math.sqrt(max(chi / dof, 1.0) / sxx)
            print(f"    slope (score per city) = {b:+.2f} +/- {se:.2f}   "
                  f"[{len(xs)} arm x venue points, through the origin, "
                  f"inverse-variance weighted]")


# ------------------------------------------------------------------ power

def cmd_power(args):
    width = args.band
    hz = args.at[0] if args.at else 10 ** 9
    print(f"{'arm':>26} {'n':>3} {'sd(score)':>10} {'sd(cities)':>11} "
          f"{'n for +1':>9} {'+2':>6} {'+4':>6} {'n cities +0.1':>14}")
    agg = defaultdict(list)
    for spec in args.arm:
        name, rest = spec.split("=", 1)
        treat, ctrl = rest.split(":", 1)
        st = arm_stats(treat, ctrl, width, hz)
        if st is None:
            continue
        ss, sc = sd(st["d_score"]), sd(st["d_cities"])
        agg["score"].append(ss)
        agg["cities"].append(sc)

        def nfor(sigma, delta):
            return math.ceil((2.0 * sigma / delta) ** 2)
        print(f"{name:>26} {len(st['seeds']):>3} {ss:>10.2f} {sc:>11.3f} "
              f"{nfor(ss, 1):>9} {nfor(ss, 2):>6} {nfor(ss, 4):>6} "
              f"{nfor(sc, 0.1):>14}")
    if agg["score"]:
        ms, mc = mean(agg["score"]), mean(agg["cities"])
        print(f"\n  pooled paired sd: score {ms:.2f}, cities {mc:.3f} "
              f"(per seat, per seed)")
        for d in (1, 2, 4):
            print(f"  n to detect {d:+d} score at z=2: "
                  f"{math.ceil((2 * ms / d) ** 2)}")
        for d in (0.05, 0.1, 0.2):
            print(f"  n to detect {d:+.2f} cities at z=2: "
                  f"{math.ceil((2 * mc / d) ** 2)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    for nm in ("value", "yield", "arms", "power"):
        p = sub.add_parser(nm)
        p.add_argument("--band", type=int, default=BAND_W)
        p.add_argument("--at", type=int, action="append")
        if nm == "value":
            p.add_argument("--root", action="append", required=True,
                           metavar="LABEL=DIR")
        elif nm == "yield":
            p.add_argument("--deep", action="append", required=True)
            p.add_argument("--base", action="append", required=True)
        else:
            p.add_argument("--arm", action="append", required=True,
                           metavar="NAME=TREATDIR:CTRLDIR")
            p.add_argument("--valroot", default=None,
                           help="corpus to estimate the per-band value of a "
                                "city from, for the predicted-vs-observed "
                                "table (comma-separated dirs allowed)")
    args = ap.parse_args()
    {"value": cmd_value, "yield": cmd_yield,
     "arms": cmd_arms, "power": cmd_power}[args.cmd](args)


if __name__ == "__main__":
    main()
