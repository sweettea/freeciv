#!/usr/bin/env python3
"""Decompose a paired arm-vs-arm score deficit out of retained scorelogs.

Both arms play the same seeds on the same maps with every seat running the
same module, so the comparable quantity is the whole-game score total
(tune.py's `final_score`) and every question below is a paired one.

    ./wargap.py --ref baseline=DIR --arm pv3=DIR [--arm ...] \\
                [--seeds 101-116] [--turn-step 10] [--section all]

Sections: trajectory, components, seats, counters, derived, rates, all.
(`rates` is not in `all`: it is a per-tag drilldown, one table per tag.)

Each DIR holds seed-N/ subdirectories (optionally under cfg0/) with a
scorelog.txt.  Nothing about the venue is hard-coded.
"""

import argparse
import math
import sys

import scorelog


# ---- small paired statistics ---------------------------------------------

def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def sem(xs):
    n = len(xs)
    if n < 2:
        return float("nan")
    m = mean(xs)
    var = sum((x - m) ** 2 for x in xs) / (n - 1)
    return math.sqrt(var / n)


def tstat(xs):
    s = sem(xs)
    if not s or math.isnan(s):
        return float("nan")
    return mean(xs) / s


def median(xs):
    if not xs:
        return float("nan")
    ys = sorted(xs)
    n = len(ys)
    return ys[n // 2] if n % 2 else (ys[n // 2 - 1] + ys[n // 2]) / 2


def parse_seeds(spec):
    if not spec:
        return None
    out = set()
    for part in spec.split(","):
        if "-" in part:
            a, b = part.split("-")
            out.update(range(int(a), int(b) + 1))
        else:
            out.add(int(part))
    return out


# ---- per-game reductions --------------------------------------------------

FROZEN = False   # set from --metric; see scorelog.Scorelog.last_value
SCORE_MODE = "full"  # full | civil | combat -- see game_total()

# get_civ_score() terms the two seats of a fight both get paid for.  In a
# single-module venue every seat is the same AI, so these are a measure of
# how violent the *population* is, not of who won.
COMBAT_TERMS = ("unitskilled", "unitsbuilt")
CIVIL_TERMS = ("citizens", "techs", "wonders", "culture")


def game_total(game, turn):
    """Whole-game score at `turn`.

    metric=alive: only seats still logging a value (what the living
    empires are worth).  metric=frozen: dead seats keep the score they had
    when they died, which is exactly tune.py's `final_score` objective at
    the end turn.
    """
    if SCORE_MODE != "full":
        c = game_components(game, turn)
        terms = COMBAT_TERMS if SCORE_MODE == "combat" else CIVIL_TERMS
        return sum(c.get(k, 0) for k in terms)
    if FROZEN:
        return game.frozen_total("score", turn) or 0
    return game.total("score", turn) or 0


def game_components(game, turn):
    """Whole-game score split into get_civ_score() terms."""
    agg = {}
    for pid in game.players:
        c = scorelog.score_components(game, pid, turn, frozen=FROZEN)
        if c is None:
            continue
        for k, v in c.items():
            agg[k] = agg.get(k, 0) + v
    return agg


def seat_scores(game, turn):
    """Per-seat scores at `turn`, honouring the chosen metric."""
    if FROZEN:
        return [game.last_value("score", pid, turn) or 0
                for pid in game.series.get("score", {})]
    return list(game.seat_values("score", turn).values())


COMPONENT_ORDER = ["citizens", "techs", "wonders", "unitskilled",
                   "unitsbuilt", "culture"]


# ---- sections -------------------------------------------------------------

def section_trajectory(ref_name, ref, arms, turns, step):
    print("\n### 1. Per-turn deficit trajectory "
          "(whole-game score, arm minus %s, paired over seeds)\n" % ref_name)
    for name, arm in arms.items():
        seeds = sorted(set(arm) & set(ref))
        print("  %s vs %s, n=%d seeds" % (name, ref_name, len(seeds)))
        print("  %5s %9s %9s %9s %7s %7s %6s" %
              ("turn", ref_name[:9], name[:9], "diff", "sem", "t", "wins"))
        for t in turns:
            if t % step and t != turns[-1] and t != turns[0]:
                continue
            rvals, avals, diffs = [], [], []
            for s in seeds:
                r = game_total(ref[s], t)
                a = game_total(arm[s], t)
                if r == 0 and a == 0:
                    continue
                rvals.append(r)
                avals.append(a)
                diffs.append(a - r)
            if not diffs:
                continue
            tt = tstat(diffs)
            wins = sum(1 for d in diffs if d > 0)
            print("  %5d %9.1f %9.1f %+9.1f %7.1f %+7.2f %3d/%-2d" %
                  (t, mean(rvals), mean(avals), mean(diffs), sem(diffs), tt,
                   wins, len(diffs)))
        # onset: first turn from which the paired t stays at or below -2
        fine = []
        for t in turns:
            diffs = [game_total(arm[s], t) - game_total(ref[s], t)
                     for s in seeds
                     if not (game_total(arm[s], t) == 0
                             and game_total(ref[s], t) == 0)]
            if len(diffs) < 2:
                continue
            fine.append((t, mean(diffs), tstat(diffs)))
        onset2 = onset1 = None
        for i, (t, m, tv) in enumerate(fine):
            if tv <= -1 and all(x[2] <= -1 for x in fine[i:]):
                onset1 = t
                break
        for i, (t, m, tv) in enumerate(fine):
            if tv <= -2 and all(x[2] <= -2 for x in fine[i:]):
                onset2 = t
                break
        # first turn the mean diff turns and stays negative
        onsetm = None
        for i, (t, m, tv) in enumerate(fine):
            if m < 0 and all(x[1] < 0 for x in fine[i:]):
                onsetm = t
                break
        print("  divergence onset: mean<0 from T%s; t<=-1 from T%s; "
              "t<=-2 from T%s" % (onsetm, onset1, onset2))
        print()


def section_components(ref_name, ref, arms, points):
    print("\n### 2. Component split (exact get_civ_score() terms, "
          "whole-game, arm minus %s)\n" % ref_name)
    for name, arm in arms.items():
        seeds = sorted(set(arm) & set(ref))
        for t in points:
            rows = []
            for comp in COMPONENT_ORDER + ["score"]:
                diffs = []
                rmeans = []
                ameans = []
                for s in seeds:
                    rc = game_components(ref[s], t)
                    ac = game_components(arm[s], t)
                    if not rc and not ac:
                        continue
                    rv = rc.get(comp, 0)
                    av = ac.get(comp, 0)
                    rmeans.append(rv)
                    ameans.append(av)
                    diffs.append(av - rv)
                if not diffs:
                    continue
                rows.append((comp, mean(rmeans), mean(ameans), mean(diffs),
                             sem(diffs), tstat(diffs)))
            # subtotals: the mutual-violence half of get_civ_score() and
            # the rest.  Both sides of every fight are paid for kills, so a
            # less violent population scores less by construction.
            for label, terms in (("= combat", COMBAT_TERMS),
                                 ("= civil", CIVIL_TERMS)):
                diffs, rm, am = [], [], []
                for s2 in seeds:
                    rc = game_components(ref[s2], t)
                    ac = game_components(arm[s2], t)
                    rv = sum(rc.get(k, 0) for k in terms)
                    av = sum(ac.get(k, 0) for k in terms)
                    rm.append(rv)
                    am.append(av)
                    diffs.append(av - rv)
                rows.append((label, mean(rm), mean(am), mean(diffs),
                             sem(diffs), tstat(diffs)))
            tot = next((r[3] for r in rows if r[0] == "score"), 0.0)
            print("  %s vs %s at T%d (n=%d)" % (name, ref_name, t, len(seeds)))
            print("  %-12s %9s %9s %9s %7s %7s %8s" %
                  ("component", ref_name[:9], name[:9], "diff", "sem", "t",
                   "share"))
            for comp, rm, am, dm, ds, dt in rows:
                share = (dm / tot * 100) if tot else float("nan")
                print("  %-12s %9.1f %9.1f %+9.1f %7.1f %+7.2f %7.0f%%" %
                      (comp, rm, am, dm, ds, dt, share))
            print()


def section_seats(ref_name, ref, arms, turn):
    print("\n### 3. Per-seat distribution at T%d "
          "(seats sorted within each game, so rank r is compared to rank r)\n"
          % turn)
    for name, arm in arms.items():
        seeds = sorted(set(arm) & set(ref))
        nseats = max(len(ref[seeds[0]].players), len(arm[seeds[0]].players))
        print("  %s vs %s, n=%d seeds" % (name, ref_name, len(seeds)))
        print("  %5s %9s %9s %9s %7s %7s" %
              ("rank", ref_name[:9], name[:9], "diff", "sem", "t"))
        for rank in range(nseats):
            rvals, avals, diffs = [], [], []
            for s in seeds:
                rs = sorted(seat_scores(ref[s], turn), reverse=True)
                as_ = sorted(seat_scores(arm[s], turn), reverse=True)
                rs += [0] * (nseats - len(rs))
                as_ += [0] * (nseats - len(as_))
                rvals.append(rs[rank])
                avals.append(as_[rank])
                diffs.append(as_[rank] - rs[rank])
            print("  %5d %9.1f %9.1f %+9.1f %7.1f %+7.2f" %
                  (rank + 1, mean(rvals), mean(avals), mean(diffs),
                   sem(diffs), tstat(diffs)))
        # survival
        rsurv = [len(ref[s].survivors(turn)) for s in seeds]
        asurv = [len(arm[s].survivors(turn)) for s in seeds]
        sd = [a - r for a, r in zip(asurv, rsurv)]
        print("  survivors at T%d: %s %.2f, %s %.2f, diff %+.2f "
              "(sem %.2f, t %+.2f)" %
              (turn, ref_name, mean(rsurv), name, mean(asurv), mean(sd),
               sem(sd), tstat(sd)))
        rdead = sum(len(ref[s].added) - v for s, v in zip(seeds, rsurv))
        adead = sum(len(arm[s].added) - v for s, v in zip(seeds, asurv))
        print("  total seats lost across the run: %s %d, %s %d"
              % (ref_name, rdead, name, adead))
        # death-turn histogram
        for label, armd in ((ref_name, ref), (name, arm)):
            deaths = []
            for s in seeds:
                deaths += [t for t in armd[s].deleted.values()]
            deaths.sort()
            print("  %s death turns: %s" % (label, deaths))
        # concentration: how much of the deficit sits in the top seat
        top_d = []
        rest_d = []
        for s in seeds:
            rs = sorted(seat_scores(ref[s], turn), reverse=True)
            as_ = sorted(seat_scores(arm[s], turn), reverse=True)
            rs += [0] * (nseats - len(rs))
            as_ += [0] * (nseats - len(as_))
            top_d.append(as_[0] - rs[0])
            rest_d.append(sum(as_[1:]) - sum(rs[1:]))
        print("  deficit carried by rank 1: %+.1f/game; by ranks 2-%d: "
              "%+.1f/game" % (mean(top_d), nseats, mean(rest_d)))
        print()


COUNTERS = ["cities", "pop", "settledarea", "landarea", "techs", "techout",
            "bnp", "mfg", "gold", "munits", "settlers", "unitsbuilt",
            "unitslost", "unitskilled", "unitsused", "wonders", "culture",
            "corruption", "riots"]


def section_counters(ref_name, ref, arms, points, counters):
    """Raw scorelog tags, whole-game sums -- what the AIs are *doing*,
    as opposed to what the score formula pays them for."""
    print("\n### 4. Raw counters (whole-game sums, arm minus %s)\n" % ref_name)
    for name, arm in arms.items():
        seeds = sorted(set(arm) & set(ref))
        for t in points:
            print("  %s vs %s at T%d (n=%d)" % (name, ref_name, t, len(seeds)))
            print("  %-12s %10s %10s %10s %8s %7s %7s" %
                  ("counter", ref_name[:10], name[:10], "diff", "sem", "t",
                   "ratio"))
            for tag in counters:
                rv, av, dv = [], [], []
                for s in seeds:
                    r = _tag_total(ref[s], tag, t)
                    a = _tag_total(arm[s], tag, t)
                    if r is None and a is None:
                        continue
                    r, a = r or 0, a or 0
                    rv.append(r)
                    av.append(a)
                    dv.append(a - r)
                if not dv:
                    continue
                ratio = (mean(av) / mean(rv)) if mean(rv) else float("nan")
                print("  %-12s %10.1f %10.1f %+10.1f %8.1f %+7.2f %7.3f" %
                      (tag, mean(rv), mean(av), mean(dv), sem(dv), tstat(dv),
                       ratio))
            print()


def section_rates(ref_name, ref, arms, counters, window):
    """Per-window increments of the cumulative counters -- when a
    behaviour starts or stops, rather than what it has added up to."""
    print("\n### 5. Per-%d-turn increments (whole-game sums, arm minus %s)\n"
          % (window, ref_name))
    for name, arm in arms.items():
        seeds = sorted(set(arm) & set(ref))
        end = min(min(g.last_turn for g in ref.values()),
                  min(g.last_turn for g in arm.values()))
        edges = list(range(1, end + 1, window))
        if edges[-1] != end:
            edges.append(end)
        for tag in counters:
            print("  %s: %s per %d turns" % (name, tag, window))
            print("  %11s %10s %10s %10s %7s" %
                  ("window", ref_name[:10], name[:10], "diff", "t"))
            for a, b in zip(edges, edges[1:]):
                rv, av, dv = [], [], []
                for s in seeds:
                    r0 = _tag_total(ref[s], tag, a) or 0
                    r1 = _tag_total(ref[s], tag, b) or 0
                    a0 = _tag_total(arm[s], tag, a) or 0
                    a1 = _tag_total(arm[s], tag, b) or 0
                    rv.append(r1 - r0)
                    av.append(a1 - a0)
                    dv.append((a1 - a0) - (r1 - r0))
                print("  %5d-%-5d %10.1f %10.1f %+10.1f %+7.2f" %
                      (a, b, mean(rv), mean(av), mean(dv), tstat(dv)))
            print()


DERIVED = {
    # kills bought per unit produced: separates "deep builds less army"
    # from "deep's army does less with itself"
    "kills/built": ("unitskilled", "unitsbuilt"),
    # exchange rate: >1 means the side is winning its fights
    "kills/lost": ("unitskilled", "unitslost"),
    "pop/city": ("pop", "cities"),
    "bnp/city": ("bnp", "cities"),
    "built/mfg": ("unitsbuilt", "mfg"),
}


def section_derived(ref_name, ref, arms, points):
    """Ratios of counters, computed per game and then paired.

    A ratio answers the question a level cannot: deep building fewer
    units explains fewer kills only in proportion, so kills-per-unit-built
    is where an effectiveness gap would show up.
    """
    print("\n### 6. Derived per-game ratios (paired, arm minus %s)\n"
          % ref_name)
    for name, arm in arms.items():
        seeds = sorted(set(arm) & set(ref))
        for t in points:
            print("  %s vs %s at T%d (n=%d)" % (name, ref_name, t, len(seeds)))
            print("  %-12s %10s %10s %10s %8s %7s" %
                  ("ratio", ref_name[:10], name[:10], "diff", "sem", "t"))
            for label, (num, den) in DERIVED.items():
                rv, av, dv = [], [], []
                for s in seeds:
                    rd = _tag_total(ref[s], den, t) or 0
                    ad = _tag_total(arm[s], den, t) or 0
                    if not rd or not ad:
                        continue
                    r = (_tag_total(ref[s], num, t) or 0) / rd
                    a = (_tag_total(arm[s], num, t) or 0) / ad
                    rv.append(r)
                    av.append(a)
                    dv.append(a - r)
                if not dv:
                    continue
                print("  %-12s %10.3f %10.3f %+10.3f %8.3f %+7.2f" %
                      (label, mean(rv), mean(av), mean(dv), sem(dv),
                       tstat(dv)))
            print()


def _tag_total(game, tag, turn):
    if FROZEN:
        return game.frozen_total(tag, turn)
    return game.total(tag, turn)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ref", required=True, help="NAME=DIR reference arm")
    ap.add_argument("--arm", action="append", default=[],
                    help="NAME=DIR comparison arm (repeatable)")
    ap.add_argument("--seeds", default=None, help="e.g. 101-116 or 1,2,3")
    ap.add_argument("--turn-step", type=int, default=10)
    ap.add_argument("--points", default=None,
                    help="comma-separated turns for the component split; "
                         "default is the divergence turn guess and the end")
    ap.add_argument("--score", default="full",
                    choices=["full", "civil", "combat"],
                    help="which get_civ_score() terms the trajectory uses")
    ap.add_argument("--metric", default="frozen", choices=["frozen", "alive"],
                    help="frozen (default) reproduces tune.py's objective; "
                         "alive counts only seats still in the game")
    ap.add_argument("--counters", default="unitskilled,unitsbuilt,cities",
                    help="comma-separated tags for --section rates")
    ap.add_argument("--section", default="all",
                    choices=["all", "trajectory", "components", "seats",
                             "counters", "rates", "derived"])
    args = ap.parse_args()

    global FROZEN, SCORE_MODE
    FROZEN = (args.metric == "frozen")
    SCORE_MODE = args.score

    seeds = parse_seeds(args.seeds)
    rname, rdir = args.ref.split("=", 1)
    ref = scorelog.load_arm(rdir, seeds)
    if not ref:
        sys.exit("no games under %s" % rdir)
    arms = {}
    for spec in args.arm:
        n, d = spec.split("=", 1)
        arms[n] = scorelog.load_arm(d, seeds)
        if not arms[n]:
            sys.exit("no games under %s" % d)

    end = min([g.last_turn for g in ref.values()] +
              [g.last_turn for a in arms.values() for g in a.values()])
    turns = sorted(t for t in set().union(*[g.turns for g in ref.values()])
                   if t <= end)

    print("# wargap: %s vs %s" % (", ".join(arms), rname))
    print("# seeds: %s" % sorted(set(ref) & set().union(*map(set, arms.values()))))
    print("# turns 1..%d, metric=%s, score=%s"
          % (end, args.metric, args.score))

    if args.points:
        points = [int(x) for x in args.points.split(",")]
    else:
        points = [end]

    if args.section in ("all", "trajectory"):
        section_trajectory(rname, ref, arms, turns, args.turn_step)
    if args.section in ("all", "components"):
        section_components(rname, ref, arms, points)
    if args.section in ("all", "seats"):
        section_seats(rname, ref, arms, end)
    if args.section in ("all", "counters"):
        section_counters(rname, ref, arms, points, COUNTERS)
    if args.section in ("all", "derived"):
        section_derived(rname, ref, arms, points)
    if args.section == "rates":
        section_rates(rname, ref, arms, args.counters.split(","), 20)


if __name__ == "__main__":
    main()
