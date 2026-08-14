#!/usr/bin/env python3
"""Census of the trade-route portfolio (TRADE) over benchmark results.

    tradecensus.py RESULTS_DIR [RESULTS_DIR...] [--arm deep]

Four things per result dir, all per game and averaged over games:

  seam      the deep-trade-merit line -- how often the build seam is
            asked, what it answers relative to stock's bid, and the
            decomposition of its own answer (windfall, per-turn flow,
            priced life, chosen rank).
  annuity   deep's NPV beside what caravan.c's own assembly would have
            said about the SAME destination at the SAME arrival time
            (`cnpv`). caravan.c's annuity() is inverted, so on any
            destination where an ongoing route is actually available
            the two differ in SIGN. This is the measurement that turns
            upstream-bugs.md's arithmetic claim into evidence, and it
            is free: both numbers are computed in the same call.
  portfolio the slot economics -- free slots, caravans already
            committed, the rank the marginal caravan was sold, how
            many candidates were enumerated and how many survived
            pricing, and how many were truncated by a cease-fire.
  routes    ROUTE ENDPOINTS from the final save (`traderoute<N>` keys
            in each player's city table), which is a route that EXISTS
            rather than one that was intended; plus caravan dispatches
            by errand from the deep-trade line, and the two stance
            counters (spared=, loved=) off deep-leaders.

Percentiles are p10/p50/p90 wherever a distribution is more honest
than a mean.
"""

import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean

FIELD = re.compile(r"(\w+)=(-?[\d.]+)")


def q(vals):
    v = sorted(vals)
    if not v:
        return "      --      --      --"
    n = len(v)
    return "%8.2f%8.2f%8.2f" % (v[n // 10], v[n // 2], v[(9 * n) // 10])


def read(path):
    return Path(path).read_text(encoding="utf-8", errors="replace")


def read_save(path):
    if str(path).endswith(".zst"):
        return subprocess.run(["zstdcat", str(path)], capture_output=True,
                              text=True, check=True).stdout
    return read(path)


def merit_rows(text):
    rows = []
    for line in text.split("\n"):
        if "deep-trade-merit" not in line:
            continue
        d = {k: float(v) for k, v in FIELD.findall(line)}
        m = re.search(r"mode=(\w+)", line)
        d["mode"] = m.group(1) if m else "?"
        rows.append(d)
    return rows


def dispatch_counts(text):
    """Caravan errands actually dispatched, by kind."""
    c = Counter()
    for line in text.split("\n"):
        if "deep-trade " not in line or "kind=" not in line:
            continue
        m = re.search(r"kind=(\w+)", line)
        if m:
            c[m.group(1)] += 1
    return c


def stance_counters(text):
    """Last value of the two cumulative counters on deep-leaders."""
    spared = loved = 0
    for line in text.split("\n"):
        if "deep-leaders" not in line:
            continue
        m = re.search(r"spared=(\d+) loved=(\d+)", line)
        if m:
            spared = max(spared, int(m.group(1)))
            loved = max(loved, int(m.group(2)))
    return spared, loved


def route_endpoints(save_text):
    """Routes that EXIST at the end, counted as endpoints.

    Each player's city table is a `c={"col",...}` header followed by one
    row per city; the partner city ids live in the `traderouteN`
    columns and a 0 means the slot is empty. A domestic route therefore
    contributes two endpoints (one per city's row) and a foreign one
    contributes one per side that belongs to a benchmark seat. Counting
    endpoints rather than routes is what R2 published and is kept for
    comparability.

    Read from the FINAL SAVE rather than from an intention log: an
    endpoint is a route that exists.
    """
    import csv as _csv

    total = 0
    lines = save_text.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        i += 1
        if not line.startswith("c={"):
            continue
        cols = re.findall(r'"([^"]*)"', line)
        idx = [n for n, c in enumerate(cols)
               if re.fullmatch(r"traderoute\d+", c)]
        if not idx:
            continue
        while i < len(lines):
            row = lines[i]
            if not row or row.startswith("[") or row.startswith("c={") \
               or re.match(r"^[a-zA-Z_][\w.]*=", row):
                break
            i += 1
            try:
                vals = next(_csv.reader([row]))
            except StopIteration:
                continue
            for n in idx:
                if n < len(vals) and vals[n].strip() not in ("", "0"):
                    total += 1
    return total


def final_save(seed_dir):
    saves = sorted(Path(seed_dir, "saves").glob("*.sav*"))
    if not saves:
        return None

    def turn(p):
        m = re.search(r"T(\d+)", p.name)
        return int(m.group(1)) if m else -1

    return max(saves, key=turn)


def census(results_dir, arm):
    games = sorted(Path(results_dir).glob("*/%s/seed-*" % arm))
    games = [g for g in games if (g / "DONE").exists()]
    if not games:
        return None

    per = defaultdict(list)
    npv, cnpv, ratio, stockbid, merit = [], [], [], [], []
    wind, flow, life, rank, seen, priced = [], [], [], [], [], []
    modes = Counter()

    for g in games:
        text = read(g / "game.log") if (g / "game.log").exists() else ""
        rows = merit_rows(text)
        per["seam lines"].append(len(rows))
        eligible = [r for r in rows if r.get("dest", -1) >= 0]
        per["with a destination"].append(len(eligible))
        per["capped enumerations"].append(
            sum(1 for r in rows if r.get("capped", 0) > 0))
        per["cease-fire truncations"].append(
            sum(r.get("trunc", 0) for r in rows))
        gm = Counter(r["mode"] for r in rows)
        for k in ("replace", "promote", "keep"):
            per["mode=" + k].append(gm.get(k, 0))
        modes.update(gm)

        for r in eligible:
            npv.append(r["npv"])
            cnpv.append(r["cnpv"])
            merit.append(r["merit"])
            stockbid.append(r["stock"])
            if r["stock"] > 0:
                ratio.append(r["merit"] / r["stock"])
            wind.append(r["wind"])
            flow.append(r["flow"])
            life.append(r["life"])
            rank.append(r["rank"])
            seen.append(r["seen"])
            priced.append(r["priced"])

        d = dispatch_counts(text)
        per["dispatch route"].append(d.get("route", 0))
        per["dispatch wonder"].append(d.get("wonder", 0))
        sp, lv = stance_counters(text)
        per["targets spared"].append(sp)
        per["love added"].append(lv)

        save = final_save(g)
        per["ROUTE ENDPOINTS"].append(
            route_endpoints(read_save(save)) if save else 0)

    return games, per, dict(npv=npv, cnpv=cnpv, ratio=ratio, merit=merit,
                            stock=stockbid, wind=wind, flow=flow, life=life,
                            rank=rank, seen=seen, priced=priced)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    arm = "deep"
    if "--arm" in sys.argv:
        arm = sys.argv[sys.argv.index("--arm") + 1]

    for d in args:
        got = census(d, arm)
        print("== %s (arm %s)" % (d, arm))
        if got is None:
            print("   no completed games\n")
            continue
        games, per, dist = got
        print("   %d games" % len(games))
        for k in ("seam lines", "with a destination", "mode=replace",
                  "mode=promote", "mode=keep", "cease-fire truncations",
                  "capped enumerations", "dispatch route", "dispatch wonder",
                  "ROUTE ENDPOINTS", "targets spared", "love added"):
            if per[k]:
                print("   %-24s %10.2f" % (k, mean(per[k])))
        if dist["npv"]:
            print("   %-24s %s" % ("", "     p10     p50     p90"))
            for k in ("npv", "cnpv", "merit", "stock", "ratio", "wind",
                      "flow", "life", "rank", "seen", "priced"):
                print("   %-24s %s" % (k, q(dist[k])))
            neg = sum(1 for v in dist["cnpv"] if v < 0)
            flowpos = sum(1 for v in dist["flow"] if v > 0)
            print("   caravan.c NPV negative   %10.1f%% of %d"
                  % (100.0 * neg / len(dist["cnpv"]), len(dist["cnpv"])))
            print("   ... an ongoing route is available on %.1f%%"
                  % (100.0 * flowpos / len(dist["flow"])))
        print()


if __name__ == "__main__":
    main()
