#!/usr/bin/env python3
"""The build-want calibration corpus: what each producer class bids, and
what it bids when it WINS.

    wantcensus.py RESULTS_DIR... [--arm deep]

Reads two log lines per city-turn from `game.log`:

  deep-want    one per FINAL build decision -- the winner, with its
               producer tag (ADV_CHOICE_TRACK), the native want, and
               the (tier, value) the merge seam derived from it.
  deep-bid     one per city-turn -- the two rigged domestic wants
               (founder, worker) as `ai/default` computed them BEFORE
               the advisor scaled them, the military want deep formed
               itself, and what actually won.

The first is winner-conditioned and is the like-for-like re-take of the
July 2026 W01 corpus. The second is not: it reports what a class bid
whether or not it won, which is the only way to see a producer that
loses every auction (W06's explorer) or one that is pegged at a clamp.

Producer tags are grouped into the CLASSES a deflator would act on;
`--by-producer` reports the raw tags instead.
"""
import re
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean

# Producer tag -> deflator class. Tags come from adv_choice_set_use()
# in ai/default, ai/deep and server/advisors.
CLASS_OF = {
    "founder": "settler",
    "founder's boat": "settler-boat",
    "worker": "worker",
    "improvement": "building",
    "defense building": "building",
    "veterancy building": "building",
    "wonder": "caravan-wonder",
    "deep wonder merit": "caravan-wonder",
    "trade route": "caravan-trade",
    "deep trade merit": "caravan-trade",
    "fallback trade route": "caravan-trade",
    "first defender": "defender",
    "defender": "defender",
    "police": "defender",
    "fallback defender": "defender",
    "fallback worker": "worker",
    "bodyguard": "defender",
    "attacker": "attacker",
    "deep assault": "attacker",
    "hunter": "attacker",
    "missile": "attacker",
    "offensive air": "attacker",
    "paratrooper": "attacker",
    "attacker ferry": "ferry",
    "colony lift": "ferry",
    "defensive diplomat": "diplomat",
    "offensive diplomat": "diplomat",
    "explorer": "explorer",
    "barbarian": "barbarian",
}

WANT = re.compile(
    r'deep-want turn=(\d+) city=(\d+) producer="([^"]*)" pick="([^"]*)"'
    r' native=(-?[\d.]+) tier=(\d+) value=(-?[\d.]+)')
BID = re.compile(
    r'deep-bid turn=(\d+) player="[^"]*" city=(\d+)'
    r' founder=(-?[\d.]+) worker=(-?[\d.]+) expand=(\d+)'
    r' mil=(-?[\d.]+) miltier=(\d+) build=(-?[\d.]+)')


def pct(xs, p):
    if not xs:
        return float("nan")
    xs = sorted(xs)
    i = min(len(xs) - 1, max(0, int(round(p / 100.0 * (len(xs) - 1)))))
    return xs[i]


def describe(name, xs, games, width=22):
    print(f"    {name:<{width}} {len(xs) / games:>8.1f} {pct(xs, 10):>9.1f} "
          f"{pct(xs, 50):>9.1f} {pct(xs, 90):>9.1f} {max(xs) if xs else 0:>10.1f} "
          f"{mean(xs) if xs else 0:>9.1f}")


def main():
    argv = list(sys.argv[1:])
    arm = "deep"
    by_producer = "--by-producer" in argv
    if by_producer:
        argv.remove("--by-producer")
    if "--arm" in argv:
        i = argv.index("--arm")
        arm = argv[i + 1]
        del argv[i:i + 2]

    games = 0
    win = defaultdict(list)          # class -> winning values (NORMAL tier)
    win_emerg = defaultdict(int)     # class -> emergency-tier win count
    clamped = defaultdict(int)
    bids = defaultdict(list)         # class -> bid values, win or lose
    bid_rows = 0
    beat = defaultdict(lambda: [0, 0])   # class -> [bids > 0, bids that won]

    for root in argv:
        for d in sorted((Path(root) / arm).glob("seed-*")):
            log = d / "game.log"
            if not log.exists():
                continue
            games += 1
            for line in log.read_text(errors="replace").splitlines():
                m = WANT.search(line)
                if m:
                    prod, native, tier, value = (m.group(3), float(m.group(5)),
                                                 int(m.group(6)),
                                                 float(m.group(7)))
                    cls = prod if by_producer else CLASS_OF.get(prod, prod)
                    if tier:
                        win_emerg[cls] += 1
                    else:
                        win[cls].append(value)
                        if abs(value - 750.075) < 0.05:
                            clamped[cls] += 1
                    continue
                m = BID.search(line)
                if m:
                    bid_rows += 1
                    founder = float(m.group(3))
                    worker = float(m.group(4))
                    expand = int(m.group(5))
                    mil = float(m.group(6))
                    miltier = int(m.group(7))
                    build = float(m.group(8))
                    bids["settler"].append(abs(founder))
                    bids["worker"].append(worker * expand / 100.0)
                    bids["building"].append(build)
                    if not miltier:
                        bids["military"].append(mil)

    print(f"games: {games}   deep-bid city-turns: {bid_rows}")
    print()
    print("WINNING want by class, NORMAL tier only "
          "(the July 2026 corpus, re-taken)")
    print(f"    {'class':<22} {'per game':>8} {'p10':>9} {'p50':>9} "
          f"{'p90':>9} {'max':>10} {'mean':>9}")
    order = sorted(win, key=lambda c: -len(win[c]))
    for c in order:
        describe(c, win[c], max(games, 1))
    print()
    print("    emergency-tier wins per game, and domestic-clamp hits")
    for c in sorted(set(win) | set(win_emerg)):
        e = win_emerg.get(c, 0)
        k = clamped.get(c, 0)
        if e or k:
            print(f"    {c:<22} emergency {e / max(games, 1):>8.1f}"
                  f"   at clamp 750.1 {k / max(games, 1):>8.1f}"
                  f"  ({100.0 * k / max(len(win[c]), 1):.1f}% of its wins)")

    if bid_rows:
        print()
        print("BID by class, win or lose (deep-bid; founder as |want|,")
        print("worker after the expand factor, military NORMAL tier only)")
        print(f"    {'class':<22} {'per game':>8} {'p10':>9} {'p50':>9} "
              f"{'p90':>9} {'max':>10} {'mean':>9}")
        for c in ("settler", "worker", "building", "military"):
            if bids[c]:
                describe(c, [x for x in bids[c] if x > 0], max(games, 1))
        print()
        print("    share of city-turns where the class bids at all")
        for c in ("settler", "worker", "building", "military"):
            if bids[c]:
                n = sum(1 for x in bids[c] if x > 0)
                print(f"    {c:<22} {100.0 * n / bid_rows:>6.1f}%")


main()
