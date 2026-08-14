#!/usr/bin/env python3
"""Tech-want writer census for a mixed deep-vs-stock cell.

Reads the two streams ai/deep/deeptechcensus.c emits into game.log and
answers the three questions the TECHMIL brief asks:

  (a) WHICH WRITERS ARE DEAD for deep seats -- per-site totals and the
      share of turns each site fired on. A site that never fires is a
      military tech appetite deep does not have.
  (b) THE DEEP:STOCK RATIO of military tech_want -- computed on the
      end-of-turn array, which is the one quantity observable on BOTH
      sides (per-site attribution for a stock seat is impossible
      without a log line in ai/default, which rule 2 forbids).
  (c) WHETHER CHOSEN GOALS DIFFER in military share -- the class of the
      advance each side is researching and heading for, per turn.

Usage:
    benchmark/techcensus.py DIR [DIR ...]

Each DIR is a result directory containing <arm>/seed-<n>/game.log. Arms
are pooled, because a mirrored pair (mixed_dc and mixed_cd) is one
corpus by construction: pooling is what cancels the seat bias.
"""
import collections
import glob
import os
import re
import statistics
import sys

WANT = re.compile(
    r'deep-techwant turn=(\d+) player="([^"]*)" side="(\w+)" '
    r'total=([-\d.]+) mil=([-\d.]+) econ=([-\d.]+) neg=([-\d.]+) '
    r'advs=(\d+) milsteps=([-\d.]+) econsteps=([-\d.]+) '
    r'milnow=([-\d.]+) econnow=([-\d.]+) '
    r'res="([^"]*)" resmil=(-?\d+) '
    r'goal="([^"]*)" goalmil=(-?\d+)')

SITE = re.compile(
    r'deep-techsite turn=(\d+) player="([^"]*)" site="(\w+)" '
    r'writes=(\d+) total=([-\d.]+) mil=([-\d.]+)')

# Site -> whether the want it writes is military BY CONSTRUCTION (the
# writer is pricing a unit), independent of what advance it lands on.
# This is the site-level classification; the advance-level one lives in
# the C and is what the cross-side ratio uses.
MILITARY_SITE = {
    "stock_assess": True,
    "stock_defender": True,
    "stock_attacker": True,
    "stock_diplomat": True,
    "stock_paradrop": True,
    "deep_techwall": True,
    "stock_domestic": False,   # role units: worker/founder/ferry/trade
    "stock_building": False,
    "stock_gov": False,
    "stock_effects": False,
    "deep_horizon": False,
    "deep_settler": False,
    "other": False,
}

SITE_ORDER = [
    "stock_assess", "stock_defender", "stock_attacker", "stock_diplomat",
    "stock_paradrop", "deep_techwall",
    "stock_domestic", "stock_building", "stock_gov", "stock_effects",
    "deep_horizon", "deep_settler", "other",
]


def game_logs(root):
    """-> [(seed, arm, path)] over every game.log under root."""
    out = []
    for p in glob.glob(os.path.join(root, "*", "*", "seed-*", "game.log")):
        m = re.search(r'seed-(\d+)/game\.log$', p)
        arm = os.path.basename(os.path.dirname(os.path.dirname(p)))
        if m:
            out.append((int(m.group(1)), arm, p))
    if not out:
        for p in glob.glob(os.path.join(root, "*", "seed-*", "game.log")):
            m = re.search(r'seed-(\d+)/game\.log$', p)
            arm = os.path.basename(os.path.dirname(os.path.dirname(p)))
            if m:
                out.append((int(m.group(1)), arm, p))
    return sorted(out)


class Acc:
    def __init__(self):
        # side -> list of per-(game,player) aggregates
        self.side_total = collections.defaultdict(float)
        self.side_mil = collections.defaultdict(float)
        self.side_rows = collections.defaultdict(int)
        # side -> per-game military share of the end-of-turn array
        self.side_share = collections.defaultdict(list)
        # side -> counts of research/goal class
        self.res = collections.defaultdict(lambda: collections.Counter())
        self.goal = collections.defaultdict(lambda: collections.Counter())
        # site -> [total, mil, writes, turnsfired, turnsseen]
        self.site = collections.defaultdict(lambda: [0.0, 0.0, 0, 0, 0])
        # side -> want-weighted distance of the want to its target
        self.steps = collections.defaultdict(list)
        self.now = collections.defaultdict(list)
        self.esteps = collections.defaultdict(list)
        self.enow = collections.defaultdict(list)
        self.games = 0
        self.seeds = set()


def scan(paths, acc):
    for seed, arm, path in paths:
        acc.games += 1
        acc.seeds.add(seed)
        # Per (game, player) accumulation so a share is a per-seat
        # figure rather than a line count.
        pertot = collections.defaultdict(float)
        permil = collections.defaultdict(float)
        with open(path, errors="replace") as fh:
            for ln in fh:
                if "deep-techwant" in ln:
                    m = WANT.search(ln)
                    if not m:
                        continue
                    (_t, plr, side, total, mil, _econ, _neg, _advs,
                     milsteps, econsteps, milnow, econnow,
                     _res, resmil, _goal, goalmil) = m.groups()
                    if float(mil) > 0:
                        acc.steps[side].append(float(milsteps))
                        acc.now[side].append(float(milnow))
                    if float(_econ) > 0:
                        acc.esteps[side].append(float(econsteps))
                        acc.enow[side].append(float(econnow))
                    acc.side_total[side] += float(total)
                    acc.side_mil[side] += float(mil)
                    acc.side_rows[side] += 1
                    pertot[(side, plr)] += float(total)
                    permil[(side, plr)] += float(mil)
                    acc.res[side][int(resmil)] += 1
                    acc.goal[side][int(goalmil)] += 1
                elif "deep-techsite" in ln:
                    m = SITE.search(ln)
                    if not m:
                        continue
                    _t, _plr, site, writes, total, mil = m.groups()
                    s = acc.site[site]
                    s[0] += float(total)
                    s[1] += float(mil)
                    s[2] += int(writes)
                    s[4] += 1
                    if int(writes) > 0:
                        s[3] += 1
        for key, tot in pertot.items():
            side = key[0]
            if tot > 0:
                acc.side_share[side].append(permil[key] / tot)


def pct(n, d):
    return 0.0 if d == 0 else 100.0 * n / d


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    acc = Acc()
    paths = []
    for root in sys.argv[1:]:
        paths.extend(game_logs(root))
    if not paths:
        print("no game.log found under: %s" % " ".join(sys.argv[1:]))
        return 1
    scan(paths, acc)

    print("games=%d seeds=%d" % (acc.games, len(acc.seeds)))
    print()
    print("(a) WRITER SITES -- deep seats only (stock seats are")
    print("    unattributable: rule 2 forbids a log line in ai/default)")
    print()
    print("  %-16s %10s %10s %10s %8s %8s" %
          ("site", "want", "mil-want", "writes", "fired%", "class"))
    dead = []
    site_total = sum(v[0] for v in acc.site.values())
    for name in SITE_ORDER:
        v = acc.site.get(name)
        if v is None:
            continue
        fired = pct(v[3], v[4])
        print("  %-16s %10.0f %10.0f %10d %7.1f%% %8s" %
              (name, v[0], v[1], v[2], fired,
               "MIL" if MILITARY_SITE.get(name) else "econ"))
        if v[2] == 0:
            dead.append(name)
    print()
    print("  total attributed want: %.0f" % site_total)
    print("  DEAD (never wrote once): %s" %
          (", ".join(dead) if dead else "none"))
    milsite = sum(v[0] for k, v in acc.site.items() if MILITARY_SITE.get(k))
    print("  military-site share of deep's own writes: %.1f%%" %
          pct(milsite, site_total))
    print()

    print("(b) END-OF-TURN ARRAY -- both sides, classified by the")
    print("    ADVANCE written to (military = gates a non-civilian unit)")
    print()
    print("  %-8s %12s %12s %9s %9s" %
          ("side", "want", "mil-want", "mil%", "seat-mil%"))
    for side in ("deep", "stock"):
        t = acc.side_total[side]
        m = acc.side_mil[side]
        sh = acc.side_share[side]
        print("  %-8s %12.0f %12.0f %8.1f%% %8.1f%%" %
              (side, t, m, pct(m, t),
               100.0 * statistics.mean(sh) if sh else 0.0))
    dt, dm = acc.side_total["deep"], acc.side_mil["deep"]
    st, sm = acc.side_total["stock"], acc.side_mil["stock"]
    print()
    if sm:
        print("  deep:stock military tech_want ratio  = %.3f" % (dm / sm))
    if st:
        print("  deep:stock TOTAL tech_want ratio     = %.3f" % (dt / st))
    if sm and st and dt:
        print("  ratio of military SHARES (deep/stock) = %.3f" %
              ((dm / dt) / (sm / st)))
    print()

    print("(b2) WHERE THE WANT SITS -- dai_select_tech() spreads each")
    print("     want over the path to it DIVIDED BY STEPS, then takes")
    print("     the argmax among gettable techs. Distance, not size,")
    print("     is what converts appetite into direction.")
    print()
    print("  %-8s %14s %14s %14s %14s" %
          ("side", "mil steps", "mil on-deck%", "econ steps", "econ on-deck%"))
    for side in ("deep", "stock"):
        if not acc.steps[side]:
            continue
        print("  %-8s %14.2f %13.1f%% %14.2f %13.1f%%" %
              (side, statistics.mean(acc.steps[side]),
               statistics.mean(acc.now[side]),
               statistics.mean(acc.esteps[side]) if acc.esteps[side] else 0.0,
               statistics.mean(acc.enow[side]) if acc.enow[side] else 0.0))
    print()

    print("(c) CHOSEN RESEARCH -- military share of what each side is")
    print("    actually researching, and of its goal")
    print()
    print("  %-8s %14s %14s" % ("side", "researching", "goal"))
    for side in ("deep", "stock"):
        r = acc.res[side]
        g = acc.goal[side]
        rn = r[0] + r[1]
        gn = g[0] + g[1]
        print("  %-8s %13.1f%% %13.1f%%" %
              (side, pct(r[1], rn), pct(g[1], gn)))
    print()
    print("  turn-lines: deep=%d stock=%d" %
          (acc.side_rows["deep"], acc.side_rows["stock"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
