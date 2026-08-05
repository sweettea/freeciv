#!/usr/bin/env python3
"""Size W36 step 0 from the order and wait censuses.

Three questions, and each one can retire a piece of the commissioning
design before any of it is written:

  1. THE AWARD. When a hull was ordered, was there a city on the SAME
     water that would have finished it materially sooner? If most
     orders would land at the same city anyway, ranking candidates by
     ETA is ceremony and only cancellation and the ETA handed to the
     passenger are worth building.

  2. THE FALLBACK. How often does a demand find no producer at all? A
     commission with no candidate city cannot fall through to a
     next-best one, so this population sizes the failure mode the
     award loop has to handle rather than the one it optimises.

  3. THE WAIT. Turns from "first wanted a boat" to enrolled as a
     lift's core. Hitching is currently decided on feasibility
     because the comparison -- "what would I get by waiting for my own
     boat" -- has no number in it. This is that number, and it is also
     the honest price of the fleet's remaining idleness, which the
     idle count is not.

Reads game.log files under the given results directories.

Usage: commission.py RESULTS_DIR [RESULTS_DIR ...]
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

ORDER = re.compile(
    r"deep-hull-order turn=(\d+) player=\"([^\"]+)\" city=(\d+) "
    r"type=\"([^\"]+)\" when=(\w+) stock=(-?\d+) surplus=(-?\d+) "
    r"cost=(\d+) eta=(-?\d+) waters=(\d+) bridge=(\d+) ncand=(\d+) "
    r"best=(\d+) besteta=(-?\d+) bestfresh=(-?\d+)"
)
ORDER_FIELDS = ["turn", "player", "city", "type", "when", "stock",
                "surplus", "cost", "eta", "waters", "bridge", "ncand",
                "best", "besteta", "bestfresh"]

WAIT = re.compile(
    r"deep-wait turn=(\d+) player=\"([^\"]+)\" unit=(\d+) type=\"([^\"]+)\" "
    r"born=(-?\d+) at=\((\d+),(\d+)\) goto=\((\d+),(\d+)\) claimed=(-?\d+) "
    r"boats=(\d+) near=(-?\d+) wcity=(\d+) pcity=(\d+)"
)
WAIT_FIELDS = ["turn", "player", "unit", "type", "born", "x", "y",
               "gx", "gy", "claimed", "boats", "near", "wcity", "pcity"]

FORM = re.compile(
    r"deep-lift-op turn=(\d+) player=\"([^\"]+)\" op=\d+ form boat=\d+ "
    r"psngr=(\d+)"
)

STRS = {"player", "type", "when"}


def parse(paths):
    orders, waits, forms = [], [], {}
    games, last_turn = set(), defaultdict(int)
    for root in paths:
        for log in Path(root).rglob("game.log"):
            game = str(log.parent)
            games.add(game)
            for line in log.open(errors="replace"):
                m = ORDER.search(line)
                if m:
                    r = dict(zip(ORDER_FIELDS, m.groups()))
                    for k in ORDER_FIELDS:
                        if k not in STRS:
                            r[k] = int(r[k])
                    r["game"] = game
                    orders.append(r)
                    last_turn[game] = max(last_turn[game], r["turn"])
                    continue
                m = WAIT.search(line)
                if m:
                    r = dict(zip(WAIT_FIELDS, m.groups()))
                    for k in WAIT_FIELDS:
                        if k not in STRS:
                            r[k] = int(r[k])
                    r["game"] = game
                    waits.append(r)
                    last_turn[game] = max(last_turn[game], r["turn"])
                    continue
                m = FORM.search(line)
                if m:
                    turn, player, psngr = int(m.group(1)), m.group(2), \
                        int(m.group(3))
                    key = (game, player, psngr)
                    # A unit id can be enrolled, released and enrolled
                    # again; every enrolment turn is kept so a wait can
                    # be closed by the FIRST one after it began.
                    forms.setdefault(key, []).append(turn)
    return orders, waits, forms, games, last_turn


def runs(orders):
    """Group order lines into consecutive-turn runs.

    One run is one ORDER: the first line is the turn production was
    placed on that hull, the last is the turn it left (built, switched
    or the game ended). A gap of one turn ends the run, because a city
    that switches away and back has made a second decision.
    """
    by_key = defaultdict(list)
    for r in orders:
        by_key[(r["game"], r["player"], r["city"], r["type"])].append(r)

    out = []
    for key, obs in by_key.items():
        obs.sort(key=lambda r: r["turn"])
        cur = []
        for r in obs:
            if cur and r["turn"] != cur[-1]["turn"] + 1:
                out.append(cur)
                cur = []
            cur.append(r)
        if cur:
            out.append(cur)
    return out


def pct(n, d):
    return f"{100.0 * n / d:5.1f}%" if d else "    - "


def quantiles(xs):
    if not xs:
        return None
    s = sorted(xs)

    def q(p):
        return s[min(len(s) - 1, int(p * len(s)))]

    return {"n": len(s), "mean": sum(s) / len(s), "p25": q(0.25),
            "med": q(0.50), "p75": q(0.75), "p90": q(0.90), "max": s[-1]}


def show_q(label, q):
    if q is None:
        print(f"  {label:<28} (none)")
        return
    print(f"  {label:<28} n={q['n']:5d}  mean={q['mean']:6.2f}  "
          f"p25={q['p25']:3d}  med={q['med']:3d}  p75={q['p75']:3d}  "
          f"p90={q['p90']:3d}  max={q['max']:3d}")


def main(paths):
    orders, waits, forms, games, last_turn = parse(paths)
    ngames = len(games) or 1
    print(f"games: {len(games)}   order lines: {len(orders)}   "
          f"wait lines: {len(waits)}")

    # ---------------- 1. the award ----------------
    # The COMMITTED choice is the order event, and the only vantage
    # that sees a hull the city buys outright. The production vantage
    # is kept for one thing the choice cannot give: how long the order
    # stood, and therefore how long the demand behind it had to last.
    chosen = runs([r for r in orders if r["when"] == "choice"])
    standing = runs([r for r in orders if r["when"] == "prod"])
    print(f"\n=== 1. THE AWARD: {len(chosen)} hull orders "
          f"({len(chosen)/ngames:.2f}/game) ===")
    if standing:
        held = [len(r) for r in standing]
        print(f"  order stood in production    "
              f"mean={sum(held)/len(held):.2f} turns  max={max(held)}  "
              f"({len(standing)} standing spells)")
    if chosen:
        placed = [r[0] for r in chosen]
        nocand = sum(1 for r in placed if r["besteta"] < 0)
        withcand = [r for r in placed if r["besteta"] >= 0]
        gains = [r["eta"] - r["besteta"] for r in withcand
                 if r["eta"] >= 0]
        # The same comparison against a rival credited with NO stock:
        # the floor of what an award could buy, since it borrows
        # nothing from what the rival was already building.
        fresh = [r["eta"] - r["bestfresh"] for r in placed
                 if r["bestfresh"] >= 0 and r["eta"] >= 0]
        never = [r for r in placed if r["eta"] < 0]
        print(f"  no other producer on water   {nocand:5d}  "
              f"{pct(nocand, len(placed))}")
        print(f"  ordering city never finishes {len(never):5d}  "
              f"{pct(len(never), len(placed))}  (no shield surplus)")
        for label, xs in (("after switch penalty", gains),
                          ("from a standing start", fresh)):
            if not xs:
                continue
            print(f"  -- rival ETA advantage, {label} --")
            for k in (1, 2, 3, 5, 10):
                n = sum(1 for g in xs if g >= k)
                print(f"     rival faster by >= {k:2d}    {n:5d}  "
                      f"{pct(n, len(xs))}")
            show_q("     distribution", quantiles(xs))
        show_q("ordering city's own ETA",
               quantiles([r["eta"] for r in placed if r["eta"] >= 0]))
        byc = defaultdict(int)
        for r in placed:
            byc[(r["game"], r["player"], r["city"])] += 1
        print(f"  distinct ordering cities     {len(byc):5d}")
        closed = sum(1 for r in placed if r["bridge"] == 0)
        print(f"  ordered into closed water    {closed:5d}  "
              f"{pct(closed, len(placed))}  (gate backstop)")

    # ---------------- 2. the fallback ----------------
    print("\n=== 2. THE FALLBACK: demand with no vehicle, and with no "
          "producer ===")
    if waits:
        noboat = sum(1 for r in waits if r["boats"] == 0)
        nopcity = sum(1 for r in waits if r["pcity"] == 0)
        nowcity = sum(1 for r in waits if r["wcity"] == 0)
        inland = sum(1 for r in waits if r["wcity"] == 0
                     and r["pcity"] > 0)
        print(f"  waiting unit-turns           {len(waits):5d}  "
              f"({len(waits)/ngames:.1f}/game)")
        print(f"  ... no loadable hull exists  {noboat:5d}  "
              f"{pct(noboat, len(waits))}")
        print(f"  ... no producer city at all  {nopcity:5d}  "
              f"{pct(nopcity, len(waits))}")
        print(f"  ... none on its OWN water    {nowcity:5d}  "
              f"{pct(nowcity, len(waits))}  "
              f"(of which {inland} have a producer elsewhere)")
        show_q("distance to nearest hull",
               quantiles([r["near"] for r in waits if r["near"] >= 0]))

    # ---------------- 3. the wait ----------------
    by_unit = defaultdict(list)
    for r in waits:
        by_unit[(r["game"], r["player"], r["unit"])].append(r)

    spells, enrolled, censored = [], [], []
    for key, obs in by_unit.items():
        obs.sort(key=lambda r: r["turn"])
        # One spell per consecutive run of waiting turns: a unit that
        # stops wanting a boat and wants one again later has waited
        # twice.
        cur = []
        unit_spells = []
        for r in obs:
            if cur and r["turn"] != cur[-1]["turn"] + 1:
                unit_spells.append(cur)
                cur = []
            cur.append(r)
        if cur:
            unit_spells.append(cur)
        ends = sorted(forms.get(key, []))
        for sp in unit_spells:
            start, stop = sp[0]["turn"], sp[-1]["turn"]
            hit = [t for t in ends if start <= t <= stop + 1]
            rec = {"start": start, "stop": stop, "len": len(sp),
                   "game": key[0], "type": sp[0]["type"],
                   "boatless": all(r["boats"] == 0 for r in sp)}
            if hit:
                rec["wait"] = hit[0] - start
                enrolled.append(rec)
            else:
                # Censored: still waiting when the game ended, or the
                # unit died / retasked / walked. Reported, never
                # dropped -- dropping them would price the wait on the
                # passengers lucky enough to be served.
                rec["wait"] = stop - start
                rec["gameend"] = stop >= last_turn[key[0]]
                censored.append(rec)

    spells = enrolled + censored
    print(f"\n=== 3. THE WAIT: {len(spells)} waiting spells "
          f"({len(spells)/ngames:.2f}/game) ===")
    print(f"  enrolled as a lift core      {len(enrolled):5d}  "
          f"{pct(len(enrolled), len(spells))}")
    print(f"  censored (never enrolled)    {len(censored):5d}  "
          f"{pct(len(censored), len(spells))}  "
          f"of which {sum(1 for r in censored if r['gameend'])} "
          f"still waiting at game end")
    show_q("wait to enrolment", quantiles([r["wait"] for r in enrolled]))
    show_q("censored spell length", quantiles([r["wait"] for r in censored]))
    show_q("all spells (lower bound)", quantiles([r["wait"] for r in spells]))
    boatless = [r for r in spells if r["boatless"]]
    print(f"  spells with NO hull at all   {len(boatless):5d}  "
          f"{pct(len(boatless), len(spells))}  "
          f"({len(boatless)/ngames:.2f}/game)")
    bytype = defaultdict(list)
    for r in spells:
        bytype[r["type"]].append(r["wait"])
    for t, xs in sorted(bytype.items(), key=lambda kv: -len(kv[1]))[:6]:
        show_q(f"spells: {t}", quantiles(xs))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    main(sys.argv[1:])
