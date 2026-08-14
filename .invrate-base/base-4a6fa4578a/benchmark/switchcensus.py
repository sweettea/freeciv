#!/usr/bin/env python3
"""Census of production-switch liquidity (session HEDGE, step 0).

    switchcensus.py RESULTS_DIR [RESULTS_DIR...] [--arm deep]

Reads the three seam lines added by `ai/deep: census the production
switch and the shields it would forfeit`:

  deep-prodswitch   one production CHANGE, with the shields
                    city_change_production_penalty() would have taken.
                    Deep pays none of them today: change_build_target()
                    (server/citytools.c:3221) applies the penalty only
                    to humans and to AI seats carrying H_PRODCHGPEN,
                    and no difficulty level above `easy` sets it. So
                    `loss` is a COUNTERFACTUAL, and the sum over a game
                    is an upper bound on the exempted mass -- under the
                    penalty the stock the next event acts on would
                    already be smaller.
  deep-convertible  city-turns where disbanded_shields or
                    caravan_shields survive to the build seam: a
                    disband-into-build recovery, or a Help Wonder
                    arrival.
  deep-fallback     city-turns where every producer bid zero -- the
                    only population a surplus-shield SINK could be
                    compared on.

The denominator for the penalty mass is the empire's own shield output,
summed from the per-city-turn `shield=` surplus on the deep-def line,
so the answer is a SHARE and not a raw count that only means something
next to a cell of the same size.

Percentiles are p10/p50/p90 over the event population, never over the
zeros: a switch inside one class is not a penalty priced at nothing,
it is not a penalty event at all, and averaging its zero in reads as
the penalty being small when it is merely rare.
"""

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean

NUM = re.compile(r"(\w+)=(-?[\d.]+)")
STR = re.compile(r"(\w+)=\"([^\"]*)\"")
CLS = re.compile(r"(fromk|tok|prodk)=(\w)")


def q(vals, w=8, p=1):
    v = sorted(vals)
    if not v:
        return ("%*s" % (w, "--")) * 3
    n = len(v)
    return "".join("%*.*f" % (w, p, x)
                   for x in (v[n // 10], v[n // 2], v[(9 * n) // 10]))


def parse(line, tag):
    if tag not in line:
        return None
    d = {k: float(v) for k, v in NUM.findall(line)}
    d.update({k: v for k, v in STR.findall(line)})
    d.update({k: v for k, v in CLS.findall(line)})
    return d


def games(d, arm):
    return sorted(d.glob("%s/seed-*/game.log" % arm))


def census(d, arm):
    gl = games(d, arm)
    ng = max(len(gl), 1)
    sw, cv, fb, sc, fl, hd = [], [], [], [], [], []
    shields = 0.0
    coinpicks = 0
    cityturns = 0
    for g in gl:
        for line in g.read_text(errors="ignore").split("\n"):
            if "deep-prodswitch " in line:
                sw.append(parse(line, "deep-prodswitch"))
            elif "deep-convertible " in line:
                cv.append(parse(line, "deep-convertible"))
            elif "deep-fallback " in line:
                fb.append(parse(line, "deep-fallback"))
            elif "deep-switchcost " in line:
                sc.append(parse(line, "deep-switchcost"))
            elif "deep-switchflip " in line:
                fl.append(line)
            elif "deep-switchhold " in line:
                hd.append(line)
            elif "deep-def " in line:
                m = re.search(r" shield=(-?\d+)", line)
                if m:
                    shields += max(0, int(m.group(1)))
                cityturns += 1
            elif "deep-want " in line and 'pick="Coinage"' in line:
                coinpicks += 1

    print("=" * 74)
    print("%s   games=%d  arm=%s" % (d.name, len(gl), arm))
    print("=" * 74)
    if not sw:
        print("  no deep-prodswitch lines -- binary predates the census")
        return

    changed = [r for r in sw if r["fromk"] != r["tok"]]
    lossy = [r for r in sw if r["loss"] > 0]
    mass = sum(r["loss"] for r in sw)

    print("\n1. THE POPULATION (per game)")
    print("   city-turns (deep-def)        %9.1f" % (cityturns / ng))
    print("   production CHANGES           %9.1f" % (len(sw) / ng))
    print("   ... class-changing           %9.1f" % (len(changed) / ng))
    print("   ... with a penalty > 0       %9.1f" % (len(lossy) / ng))
    print("   shields produced (surplus)   %9.1f" % (shields / ng))
    print("   PENALTY MASS forfeited       %9.1f   = %.2f%% of output"
          % (mass / ng, 100.0 * mass / max(shields, 1)))
    # The brief's verdict yardstick: a settler is 40 shields on classic
    # and 30 on civ2civ3, so the cheaper one is the conservative
    # denominator -- it makes the mass look SMALLER, not bigger.
    print("   ... in settlers (40 shields) %9.1f" % (mass / ng / 40.0))
    # The honesty switch's own counter, read BEFORE any of the numbers
    # below: with pay_switch_penalty off every event reports pen=0 and
    # the mass is a counterfactual; with it on the mass is a bill.
    paid = [r for r in sw if r.get("pen", 0) > 0]
    print("   events that REALLY paid      %9.1f   (pen=1)" % (len(paid) / ng))
    print("   ... their mass               %9.1f"
          % (sum(r["loss"] for r in paid) / ng))

    print("\n2. THE CLASS PAIR MATRIX (per game; loss is the mean over the")
    print("   pair's own events, and p50/p90 over its non-zero ones)")
    pair = defaultdict(list)
    for r in sw:
        pair[(r["fromk"], r["tok"])].append(r["loss"])
    print("   pair      n/game   mass/game   mean     p50     p90 (loss>0)")
    for k, v in sorted(pair.items(), key=lambda x: -sum(x[1])):
        nz = [x for x in v if x > 0]
        print("   %s->%s   %8.1f  %10.1f %6.1f %s"
              % (k[0], k[1], len(v) / ng, sum(v) / ng, mean(v), q(nz, 8, 1)))

    print("\n3. WHY THE PENALTY IS OFTEN ZERO (share of class-changing")
    print("   events, and the mass each bucket holds)")
    buck = defaultdict(list)
    for r in changed:
        if r["bcs"] <= 0:
            b = "empty stock (bcs=0)"
        elif r["builtlast"] > 0:
            b = "built last turn (surplus exempt)"
        else:
            b = "standing stock, 50% charged"
        buck[b].append(r["loss"])
    for k, v in sorted(buck.items(), key=lambda x: -sum(x[1])):
        print("   %-34s n/game %7.1f   mass/game %8.1f"
              % (k, len(v) / ng, sum(v) / ng))

    print("\n4. THE HEDGE'S WINDOW: is the mass paid under elevated menace?")
    print("   (danger is the deep lattice's own city fear at switch time)")
    win = defaultdict(list)
    for r in changed:
        d0 = r["danger"]
        b = ("danger=0" if d0 == 0 else
             "danger 1-99" if d0 < 100 else
             "danger 100-499" if d0 < 500 else "danger>=500")
        win[b].append(r["loss"])
    for k in ("danger=0", "danger 1-99", "danger 100-499", "danger>=500"):
        v = win.get(k, [])
        print("   %-16s n/game %7.1f   mass/game %8.1f   %5.1f%% of mass"
              % (k, len(v) / ng, sum(v) / ng,
                 100.0 * sum(v) / max(mass, 1)))
    men = [r for r in changed if r["menace"] > 0]
    print("   menace>0 at switch: n/game %.1f, mass/game %.1f"
          % (len(men) / ng, sum(r["loss"] for r in men) / ng))

    print("\n5. THE ADDRESSABLE POPULATION: switching INTO a unit, with a")
    print("   HelpWonder-capable unit buildable in the same city")
    tomil = [r for r in changed if r["tok"] == "U"]
    addr = [r for r in tomil if r["carav"] > 0]
    addrd = [r for r in addr if r["danger"] > 0]
    for name, pop in (("switch into a unit", tomil),
                      ("... caravan-capable city", addr),
                      ("... and danger > 0", addrd)):
        print("   %-28s n/game %7.1f   mass/game %8.1f  %5.1f%% of mass"
              % (name, len(pop) / ng, sum(r["loss"] for r in pop) / ng,
                 100.0 * sum(r["loss"] for r in pop) / max(mass, 1)))
    if addr:
        print("   caravan cost at those cities  p10/p50/p90 %s"
              % q([r["carcost"] for r in addr], 7, 0))

    print("\n6. THE CONVERTIBLE CLASS (does anything convert today?)")
    print("   deep-convertible city-turns   %9.2f /game" % (len(cv) / ng))
    if cv:
        disb = [r for r in cv if r["disb"] > 0]
        cars = [r for r in cv if r["carsh"] > 0]
        print("   ... disband-into-build        %9.2f /game  shields %.1f"
              % (len(disb) / ng, sum(r["disb"] for r in disb) / ng))
        print("   ... Help Wonder ARRIVALS      %9.2f /game  shields %.1f"
              % (len(cars) / ng, sum(r["carsh"] for r in cars) / ng))

    print("\n7. THE SINK POPULATION (Coinage's only customers)")
    print("   deep-fallback city-turns      %9.2f /game" % (len(fb) / ng))
    if fb:
        print("   ... Coinage buildable there   %9.2f /game"
              % (sum(1 for r in fb if r["coinage"] > 0) / ng))
        print("   surplus there  p10/p50/p90 %s"
              % q([r["surplus"] for r in fb], 8, 1))
        print("   stock there    p10/p50/p90 %s"
              % q([r["stock"] for r in fb], 8, 1))
    print("   Coinage actually PICKED       %9.2f /game" % (coinpicks / ng))

    print("\n8. THE AUCTION'S OWN COUNTERS (switch_cost_price)")
    print("   deep-switchcost debits        %9.1f /game" % (len(sc) / ng))
    if sc:
        print("   ... forfeit charged           %9.1f /game shields"
              % (sum(r["forfeit"] for r in sc) / ng))
        print("   ... debit  p10/p50/p90 %s" % q([r["debit"] for r in sc]))
        print("   ... want   p10/p50/p90 %s" % q([r["want"] for r in sc]))
        print("   ... left   p10/p50/p90 %s" % q([r["left"] for r in sc]))
        zero = sum(1 for r in sc if r["left"] <= 0)
        print("   ... debited to ZERO           %9.1f /game (%.1f%%)"
              % (zero / ng, 100.0 * zero / len(sc)))
    print("   deep-switchflip (winner changed) %6.1f /game" % (len(fl) / ng))
    print("   deep-switchhold (switch refused) %6.1f /game" % (len(hd) / ng))


def main():
    args = [a for a in sys.argv[1:]]
    arm = "deep"
    if "--arm" in args:
        i = args.index("--arm")
        arm = args[i + 1]
        del args[i:i + 2]
    for a in args:
        census(Path(a), arm)


main()
