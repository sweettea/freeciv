#!/usr/bin/env python3
"""The T50/T100 panel: the doctrine milestones every cell already logs.

findings/theory-of-play-2026-10-26.md §H: a handful of per-seat numbers
at fixed turns, readable from scorelog.txt tags both arms always write,
would have caught the 7x early army, the Amphitheater bill and the
unclaimed Map_Known achievement without a new game. This prints them,
paired by seed when two roots are given, levels only for one.

    panel.py ROOT [ROOT2] [--at 50 100 150] [--arm deep]

A root is any directory whose scorelogs live at .../seed-N/scorelog.txt
(a stacked-lever arm, a tune.py holdout-report/cfg0, a run.sh arm dir).
--arm filters by a path component (default: none, every seat counts).
Per seat at turn T (the last logged turn <= T):

    cities        tag 3              the 5.8x-cheaper meter
    pop           tag 0
    pop/city      0 / 3              Temple-gate compliance is >= 5 at T100
    techs         tag 4
    mil/city      5 / 3              munits = every non-special unit (server/report.c get_munits), so this is UNITS per city, founders and workers included; doctrine: <= 1.0 at T50
    gold          tag 14             doctrine band 150..500; both tails fail
    culture       tag 29             a +1000 step between samples is an achievement
    gov           tag 23             ruleset index; share Monarchy-or-better
    settlers      tag 6              live inventory

Government indices are the ruleset's [government_*] order; civ2civ3 is
Anarchy 0, Tribal 1, Despotism 2, Monarchy 3, Communism 4,
Fundamentalism 5, Federation 6, Republic 7, Democracy 8. "Monarchy+"
below is index >= 3 (every non-tribal, non-despotic government).
"""
import argparse, os, re, statistics as st, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract import parse_scorelog

TAG = dict(pop=0, cities=3, techs=4, munits=5, settlers=6, gold=14,
           gov=23, score=25, culture=29)


def find_games(root, arm):
    out = {}
    for d, _, files in os.walk(root):
        if "scorelog.txt" not in files:
            continue
        rel = os.path.relpath(d, root)
        if arm and arm not in rel.split(os.sep):
            continue
        m = re.search(r"seed-(\d+)", rel)
        if m:
            out[int(m.group(1))] = os.path.join(d, "scorelog.txt")
    return out


def at(series, t):
    """Value at the last logged turn <= t, or None."""
    turns = [u for u in series if u <= t]
    return series[max(turns)] if turns else None


def seat_rows(path, cuts):
    tags, players, data, final = parse_scorelog(path)
    name = {v: k for k, v in tags.items()}
    rows = []
    for plr, td in data.items():
        def ser(tagname):
            # extract.parse_scorelog: data[player][turn][tag_id] = value
            tid = [k for k, v in tags.items() if v == tagname]
            if not tid:
                return {}
            return {turn: float(vals[tid[0]]) for turn, vals in td.items()
                    if tid[0] in vals}
        s = {k: ser(k) for k in TAG}
        for t in cuts:
            cit = at(s["cities"], t)
            if cit is None:
                continue
            cul = s["culture"]
            jumps = 0
            prev = None
            for u in sorted(u for u in cul if u <= t):
                if prev is not None and cul[u] - prev >= 900:
                    jumps += 1
                prev = cul[u]
            gov = at(s["gov"], t)
            rows.append(dict(
                seat=plr, t=t, cities=cit, pop=at(s["pop"], t) or 0.0,
                techs=at(s["techs"], t) or 0.0, munits=at(s["munits"], t) or 0.0,
                settlers=at(s["settlers"], t) or 0.0, gold=at(s["gold"], t) or 0.0,
                culture=at(cul, t) or 0.0, achievements=jumps,
                monarchy_plus=1.0 if gov is not None and gov >= 3 else 0.0,
                tribal=1.0 if gov == 1 else 0.0, score=at(s["score"], t) or 0.0))
    return rows


def seed_means(games, cuts):
    """{seed: {t: {metric: mean over seats}}}"""
    out = {}
    for seed, path in games.items():
        rows = seat_rows(path, cuts)
        for t in cuts:
            rs = [r for r in rows if r["t"] == t]
            if not rs:
                continue
            m = {}
            for k in ("cities", "pop", "techs", "munits", "settlers", "gold",
                      "culture", "achievements", "monarchy_plus", "tribal", "score"):
                m[k] = st.mean(r[k] for r in rs)
            m["pop_per_city"] = st.mean(r["pop"] / r["cities"] for r in rs if r["cities"] > 0) if any(r["cities"] > 0 for r in rs) else 0.0
            m["units_per_city"] = st.mean(r["munits"] / r["cities"] for r in rs if r["cities"] > 0) if any(r["cities"] > 0 for r in rs) else 0.0
            m["gold_in_band"] = st.mean(1.0 if 150 <= r["gold"] <= 500 else 0.0 for r in rs)
            out.setdefault(seed, {})[t] = m
    return out


COLS = [("cities", "%.2f"), ("pop", "%.1f"), ("pop_per_city", "%.2f"),
        ("techs", "%.1f"), ("units_per_city", "%.2f"), ("settlers", "%.2f"),
        ("gold", "%.0f"), ("gold_in_band", "%.0%"), ("culture", "%.0f"),
        ("achievements", "%.2f"), ("monarchy_plus", "%.0%"), ("tribal", "%.0%"),
        ("score", "%.1f")]


def fmt(v, f):
    return ("%d%%" % round(100 * v)) if f == "%.0%" else (f % v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("roots", nargs="+")
    ap.add_argument("--at", type=int, nargs="+", default=[50, 100, 150])
    ap.add_argument("--arm", default=None)
    a = ap.parse_args()
    if len(a.roots) > 2:
        sys.exit("one or two roots")
    panels = [seed_means(find_games(r, a.arm), a.at) for r in a.roots]
    for i, (r, p) in enumerate(zip(a.roots, panels)):
        print(f"root{i}: {r}  ({len(p)} seeds)")
    for t in a.at:
        print(f"\n=== T{t}  (per-seat means; paired by seed when two roots)")
        print("    %-14s" % "metric" + "".join("%12s" % h for h in
              (["root0"] if len(panels) == 1 else ["root0", "root1", "delta", "se", "t"])))
        for k, f in COLS:
            s0 = {s: p[t][k] for s, p in panels[0].items() if t in p}
            if len(panels) == 1:
                print("    %-14s%12s" % (k, fmt(st.mean(s0.values()), f)))
                continue
            s1 = {s: p[t][k] for s, p in panels[1].items() if t in p}
            common = sorted(set(s0) & set(s1))
            if len(common) < 2:
                continue
            d = [s1[s] - s0[s] for s in common]
            m, se = st.mean(d), st.stdev(d) / len(d) ** 0.5
            tt = m / se if se > 0 else float("inf")
            pct = f == "%.0%"
            print("    %-14s%12s%12s%12s%12s%12s" % (
                k, fmt(st.mean(s0[s] for s in common), f),
                fmt(st.mean(s1[s] for s in common), f),
                "%+.0f%%" % (100 * m) if pct else "%+.2f" % m,
                "%.0f%%" % (100 * se) if pct else "%.2f" % se,
                "%+.2f" % tt))
        if len(panels) == 2:
            print(f"    n = {len(common)} paired seeds")


if __name__ == "__main__":
    main()
