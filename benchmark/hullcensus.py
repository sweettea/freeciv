#!/usr/bin/env python3
"""Reconstruct per-HULL lifetimes from the deep-hull-turn census.

Why per hull and not per turn: five interventions moved the fleet between
4.57 and 6.24 hulls per player-turn and left the idle share pinned at
75-80% every time, while arrivals fell when hulls were cut. A per-turn
snapshot cannot tell "idle" from "idle now, sailing next turn", and no
fleet aggregate can say WHY a particular hull is idle. The three causes a
count conflates have three different fixes:

    too many boats     demand existed on its water and other hulls served
                       it -- fixable by building fewer
    wrong harbour      demand NEVER appeared on its water -- fixable by
                       where they are built, not how many
    demand evaporated  demand was there while it waited and then stopped
                       -- the order outlived its reason

This groups deep-hull-turn lines by (game, player, unit) and reports, per
hull: turns from birth to first cargo, total idle turns, what ENDED each
idle spell, and whether any crossing demand ever stood on its water while
it waited.

A hull whose last census line is well before the game's last turn died;
one still logged at the end survived. Both are reported, because "died
still empty" and "sat empty to the horizon" are the same waste.

Usage: hullcensus.py RESULTS_DIR [RESULTS_DIR ...]
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

LINE = re.compile(
    r"deep-hull-turn turn=(\d+) player=\"([^\"]+)\" unit=(\d+) type=\"([^\"]+)\" "
    r"born=(-?\d+) at=\((\d+),(\d+)\) incity=(\d+) ocean=(-?\d+) waters=(\d+) "
    r"cargo=(\d+) psngr=(-?\d+) task=(\d+) ssa=(\d+) op=(-?\d+) "
    r"demand=(\d+) neard=(-?\d+) idle=(\d+)(?: bridge=(-?\d+))?"
)

FIELDS = ["turn", "player", "unit", "type", "born", "x", "y", "incity",
          "ocean", "waters", "cargo", "psngr", "task", "ssa", "op",
          "demand", "neard", "idle", "bridge"]
STRS = {"player", "type"}
# `bridge` postdates the first census batches; -1 means "not logged",
# which is kept distinct from 0 ("logged, and this water is closed").
MISSING = {"bridge": -1}


def rows(paths):
    for root in paths:
        for log in Path(root).rglob("game.log"):
            game = str(log.parent)
            for line in log.open(errors="replace"):
                m = LINE.search(line)
                if m:
                    r = dict(zip(FIELDS, m.groups()))
                    for k in FIELDS:
                        if k not in STRS:
                            r[k] = (MISSING[k] if r[k] is None
                                    else int(r[k]))
                    r["game"] = game
                    yield r


def lifetimes(data):
    """One record per hull, in census order."""
    by_hull = defaultdict(list)
    last_turn = defaultdict(int)
    for r in data:
        by_hull[(r["game"], r["player"], r["unit"])].append(r)
        last_turn[r["game"]] = max(last_turn[r["game"]], r["turn"])

    out = []
    for (game, player, unit), obs in by_hull.items():
        obs.sort(key=lambda r: r["turn"])
        h = {
            "game": game, "player": player, "unit": unit,
            "type": obs[0]["type"], "born": obs[0]["born"],
            "seen": len(obs), "first": obs[0]["turn"], "last": obs[-1]["turn"],
            "idle_turns": sum(r["idle"] for r in obs),
            "carry_turns": sum(1 for r in obs if r["cargo"] > 0),
            "demand_idle_turns": sum(1 for r in obs
                                     if r["idle"] and r["demand"] > 0),
            "max_demand": max(r["demand"] for r in obs),
            # Idle turns with a waiting unit within reach on the same
            # water. One ocean can be the whole world sea, so "demand
            # on its water" is only an upper bound on opportunity.
            "near5_idle_turns": sum(1 for r in obs if r["idle"]
                                    and 0 <= r["neard"] <= 5),
            "near10_idle_turns": sum(1 for r in obs if r["idle"]
                                     and 0 <= r["neard"] <= 10),
            "survived": obs[-1]["turn"] >= last_turn[game],
            "died_idle": bool(obs[-1]["idle"]),
            "oceans": {r["ocean"] for r in obs},
            # Geometry, not demand: was any water it touched ever one
            # that reaches a second land continent. -1 where the census
            # predates the field.
            "bridge": max(r["bridge"] for r in obs),
            "closed_turns": sum(1 for r in obs if r["bridge"] == 0),
            # civ2civ3 Triremes navigate RIVERS, and a hull on a river
            # tile touches no ocean at all -- waters=0, which the bridge
            # flag scores 0 exactly like a lake. Per hull the two are
            # separable: a river-borne hull from a coastal city has
            # bridging turns too, so only a hull that never touched any
            # water in its life is ambiguous.
            "wet": max(r["waters"] for r in obs),
        }
        carried = [r["turn"] for r in obs if r["cargo"] > 0]
        h["first_carry"] = carried[0] if carried else None
        h["to_first_carry"] = (carried[0] - obs[0]["born"]) if carried else None

        # Idle spells and what ends them. A spell ends when the hull is
        # next seen non-idle: carrying (a crossing), or claimed/tasked
        # (a re-task), or never (it died or the game ended).
        spells, cur, ends = 0, None, defaultdict(int)
        for r in obs:
            if r["idle"]:
                if cur is None:
                    cur, spells = r["turn"], spells + 1
            elif cur is not None:
                # Which non-idle state it moved into. "assigned" and
                # "claimed" are work about to happen; only "crossing"
                # is work happening. Explore is a hull the ferry code
                # never gets to use at all.
                if r["cargo"] > 0:
                    ends["crossing"] += 1
                elif r["psngr"] >= 0:
                    ends["assigned"] += 1
                elif r["op"] >= 0:
                    ends["claimed"] += 1
                else:
                    ends["explore"] += 1
                cur = None
        if cur is not None:
            ends["gameend" if h["survived"] else "death"] += 1
        h["spells"] = spells
        h["ends"] = dict(ends)
        out.append(h)
    return out


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    data = list(rows(sys.argv[1:]))
    if not data:
        sys.exit("no deep-hull-turn lines found")
    hulls = lifetimes(data)
    games = len({h["game"] for h in hulls})
    n = len(hulls)
    ht = sum(h["seen"] for h in hulls)

    print(f"{n} hulls over {games} games, {ht} hull-turns "
          f"({ht/n:.1f} turns of life each)\n")

    never = [h for h in hulls if h["first_carry"] is None]
    ever = [h for h in hulls if h["first_carry"] is not None]
    print("DID IT EVER CARRY ANYTHING?")
    print(f"  never carried   {len(never):5d}  {100.0*len(never)/n:5.1f}%")
    print(f"  carried at some point {len(ever):d}  {100.0*len(ever)/n:5.1f}%")
    if ever:
        waits = sorted(h["to_first_carry"] for h in ever)
        print(f"  turns from birth to first cargo: median "
              f"{waits[len(waits)//2]}, mean {sum(waits)/len(waits):.1f}, "
              f"max {waits[-1]}")
    print()

    idle = sum(h["idle_turns"] for h in hulls)
    print("WHERE THE IDLE TURNS ARE")
    print(f"  idle hull-turns {idle} of {ht}  ({100.0*idle/ht:.1f}%)")
    if idle:
        dem = sum(h["demand_idle_turns"] for h in hulls)
        n10 = sum(h["near10_idle_turns"] for h in hulls)
        n5 = sum(h["near5_idle_turns"] for h in hulls)
        print(f"  ... with a unit waiting to cross ITS water: {dem} "
              f"({100.0*dem/idle:.1f}%)")
        print(f"  ... that unit within 10 tiles: {n10} "
              f"({100.0*n10/idle:.1f}%)")
        print(f"  ... within 5 tiles: {n5} ({100.0*n5/idle:.1f}%)")
    nc = sum(h["idle_turns"] for h in never)
    print(f"  idle turns owned by hulls that NEVER carried: {nc} "
          f"({100.0*nc/idle:.1f}% of idle)" if idle else "")
    print()

    # civ2civ3's Transport carries the Workers flag, and stock's
    # dispatch tests worker before ferry -- such a hull is parked on
    # SSA_AUTOWORKER and never reaches the ferry code (upstream-bugs
    # #33). The idle predicate does not exclude it, so it is worth
    # knowing how much of the idleness is that.
    aw = sum(1 for r in data if r["idle"] and r["ssa"] == 1)
    print(f"  ... of which parked on auto-worker (ssa=1): {aw} "
          f"({100.0*aw/idle:.1f}%)" if idle else "")
    print()

    # The whole life of the fleet, in mutually exclusive states. The
    # idle predicate calls a hull with an assigned passenger BUSY, so
    # "assigned but empty" -- waiting for a passenger that has not
    # boarded -- hides inside the 20-25% the fleet spends not idle.
    print("EVERY HULL-TURN, BY STATE")
    st = defaultdict(int)
    for r in data:
        if r["cargo"] > 0:
            st["carrying"] += 1
        elif r["psngr"] >= 0:
            st["assigned, empty"] += 1
        elif r["op"] >= 0:
            st["op-claimed, empty"] += 1
        elif r["ssa"] == 2 or r["task"] == 5:
            st["exploring"] += 1
        else:
            st["idle"] += 1
    for k in ("carrying", "assigned, empty", "op-claimed, empty",
              "exploring", "idle"):
        print(f"  {k:20s} {st[k]:7d}  {100.0*st[k]/ht:5.1f}%")
    print()

    print("WHAT ENDS AN IDLE SPELL")
    ends = defaultdict(int)
    for h in hulls:
        for k, v in h["ends"].items():
            ends[k] += v
    tot = sum(ends.values()) or 1
    for k in ("crossing", "assigned", "claimed", "explore", "death",
              "gameend"):
        print(f"  {k:9s} {ends[k]:6d}  {100.0*ends[k]/tot:5.1f}%")
    print()

    print("DID DEMAND EVER STAND ON ITS WATER?")
    nodem = [h for h in hulls if h["max_demand"] == 0]
    print(f"  hulls that never once saw a waiting passenger on their "
          f"water: {len(nodem)}  ({100.0*len(nodem)/n:.1f}%)")
    print(f"  ... their share of all idle hull-turns: "
          f"{sum(h['idle_turns'] for h in nodem)} "
          f"({100.0*sum(h['idle_turns'] for h in nodem)/idle:.1f}%)"
          if idle else "")
    dead_empty = [h for h in hulls if h["first_carry"] is None
                  and not h["survived"]]
    print(f"  hulls that DIED having never carried anything: "
          f"{len(dead_empty)}  ({100.0*len(dead_empty)/n:.1f}%)")
    print()

    # What a BUILD-TIME geometry gate could refuse. "Closed" water
    # touches one land continent or none, so no hull on it can put a
    # passenger ashore anywhere but where it embarked -- at any demand,
    # in any turn. This is the part of the wrong-harbour class that is
    # decidable before the hull is paid for.
    known = [h for h in hulls if h["bridge"] >= 0]
    if known:
        closed = [h for h in known if h["bridge"] == 0 and h["wet"] > 0]
        dry = [h for h in known if h["bridge"] == 0 and h["wet"] == 0]
        ci = sum(h["idle_turns"] for h in closed)
        print("GEOMETRY: COULD IT EVER HAVE CROSSED TO ANOTHER CONTINENT?")
        print(f"  hulls whose water NEVER reached a second continent: "
              f"{len(closed)}  ({100.0*len(closed)/len(known):.1f}% of "
              f"{len(known)} censused)")
        print(f"  ... their share of all idle hull-turns: {ci} "
              f"({100.0*ci/idle:.1f}%)" if idle else "")
        blind = [h for h in closed if h["max_demand"] == 0]
        print(f"  ... of which also never saw demand (wrong harbour): "
              f"{len(blind)}")
        carried = [h for h in closed if h["first_carry"] is not None]
        print(f"  ... but carried something anyway: {len(carried)} "
              f"(same-continent hops a gate would also refuse)")
        print(f"  hulls that never touched any water (rivers, inland): "
              f"{len(dry)}  -- NOT closed seas, reported apart")
        print()

    print("VERDICT LADDER (share of hulls)")
    wrong = [h for h in nodem]
    # A hull that never carried, split by whether demand ever stood on
    # its water WHILE it was idle -- i.e. whether it could have taken
    # the crossing at the moment the crossing was wanted. "Missed" is
    # an assignment failure; "mistimed" says the hull was busy or held
    # whenever demand appeared and idle whenever it did not.
    missed = [h for h in hulls if h["first_carry"] is None
              and h["demand_idle_turns"] > 0]
    mistimed = [h for h in hulls if h["first_carry"] is None
                and h["max_demand"] > 0 and h["demand_idle_turns"] == 0]
    for label, sel, why in (
            ("wrong harbour", wrong, "demand never appeared on its water"),
            ("missed it", missed, "idle WHILE demand stood on its water"),
            ("mistimed", mistimed, "demand only when it was not idle"),
            ("worked", ever, "carried at least once")):
        print(f"  {label:14s}: {len(sel):5d}  {100.0*len(sel)/n:5.1f}%"
              f"   {why}")
    near = [h for h in missed if h["near10_idle_turns"] > 0]
    print(f"  {'  ... near':14s}: {len(near):5d}  {100.0*len(near)/n:5.1f}%"
          "   that unit within 10 tiles while it sat")
    print("\n  (idle hull-turns owned by each class)")
    for label, sel in (("wrong harbour", wrong), ("missed it", missed),
                       ("mistimed", mistimed), ("worked", ever)):
        t = sum(h["idle_turns"] for h in sel)
        print(f"  {label:14s}: {t:6d}  {100.0*t/idle:5.1f}%" if idle else "")


if __name__ == "__main__":
    main()
