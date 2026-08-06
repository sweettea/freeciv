#!/usr/bin/env python3
"""Combat E2 census: what an assault is priced at, and what it costs.

    combat.py RESULTS_DIR [RESULTS_DIR...] [--arm ARM]

Reads four log streams, all per game and all read-only:

  deep-kill    the build side's assault want. gross= is the want before
               the cost-of-victory subtraction and loss= is the expected
               attacker loss in SHIELDS, so a DECLINE -- an attack
               priced positive and refused because it costs more than
               it wins -- is (gross > 0 and want == 0) and needs no
               counter of its own. far= is the free attackers the
               per-target pool dropped for being unable to arrive.
  deep-op      form / march / dissolve. The march line carries odds=
               and surv= (the expected force still standing after the
               take) when the launch bar is odds-based, and bar=0 when
               it is the old head count.
  deep-tired   the tired-attack hold, with seat= naming which of the
               two seats fired. The first_activities seat has never
               fired in this project's history (D1); seat=strike is
               this epic's re-seat.
  deep-air     enemy air seen by the survival wave, logged only when
               the term has a population, so zero lines is the proof a
               cell never reached flight.

Per-game means with a paired-t against the first directory, the same
shape benchmark/paircmp.py reports for score metrics.
"""

import argparse
import math
import re
import sys
from collections import defaultdict
from pathlib import Path

KV = re.compile(r'(\w+)=("[^"]*"|\S+)')


def kvs(rest):
    row = {}
    for k, v in KV.findall(rest):
        if v.startswith('"'):
            row[k] = v[1:-1]
        else:
            try:
                row[k] = float(v) if "." in v else int(v)
            except ValueError:
                row[k] = v
    return row


def game_stats(gamedir):
    """-> dict of per-game counters for one seed directory."""
    s = defaultdict(float)
    log = gamedir / "game.log"
    if not log.exists():
        return None
    for line in log.read_text(errors="replace").splitlines():
        if "deep-kill" in line:
            r = kvs(line.split("deep-kill", 1)[1])
            s["kill_evals"] += 1
            s["kill_gross"] += r.get("gross", 0.0)
            s["kill_loss"] += r.get("loss", 0.0)
            s["kill_want"] += r.get("want", 0.0)
            s["kill_pool"] += r.get("pool", 0)
            s["kill_far"] += r.get("far", 0)
            s["kill_elsewhere"] += r.get("elsewhere", 0)
            if r.get("far", 0) > 0:
                s["kill_far_evals"] += 1
            if r.get("gross", 0.0) > 0 and r.get("want", 0.0) <= 0:
                s["kill_declined"] += 1
            if r.get("gross", 0.0) > 0:
                s["kill_positive"] += 1
        elif "deep-op " in line:
            rest = line.split("deep-op ", 1)[1]
            r = kvs(rest)
            for word in ("form", "march", "strike", "regate", "garrison"):
                if " %s " % word in " " + rest:
                    s["op_" + word] += 1
            if " march " in " " + rest:
                s["march_odds"] += r.get("odds", 0.0)
                s["march_surv"] += r.get("surv", 0.0)
                s["march_present"] += r.get("present", 0)
                s["march_required"] += r.get("required", 0)
                s["march_waited"] += r.get("waited", 0)
        elif "deep-op-end" in line or "dissolve" in line:
            pass
        elif "deep-tired" in line:
            r = kvs(line.split("deep-tired", 1)[1])
            s["tired_" + str(r.get("seat", "first"))] += 1
        elif "deep-air" in line:
            r = kvs(line.split("deep-air", 1)[1])
            s["air_lines"] += 1
            s["air_units"] += r.get("air", 0)
    return s


def collect(root, arm):
    """-> {seed: stats}"""
    out = {}
    for d in sorted(Path(root).glob("%s/seed-*" % (arm or "*"))):
        if not (d / "DONE").exists():
            continue
        st = game_stats(d)
        if st is not None:
            out[int(d.name.split("-")[1])] = st
    return out


def paired(a, b, key):
    seeds = sorted(set(a) & set(b))
    d = [b[s][key] - a[s][key] for s in seeds]
    n = len(d)
    if n == 0:
        return 0.0, 0.0, 0.0, 0
    mean = sum(d) / n
    if n > 1:
        var = sum((x - mean) ** 2 for x in d) / (n - 1)
        se = math.sqrt(var / n)
    else:
        se = 0.0
    t = mean / se if se > 0 else 0.0
    wins = sum(1 for x in d if x > 0)
    return mean, se, t, wins


KEYS = [
    ("kill_evals", "deep-kill evaluations"),
    ("kill_positive", "... priced positive gross"),
    ("kill_declined", "... DECLINED on cost"),
    ("kill_gross", "gross want, summed"),
    ("kill_loss", "expected loss, shields"),
    ("kill_want", "net want, summed"),
    ("kill_pool", "pool members, summed"),
    ("kill_far", "pool members OUT of reach"),
    ("kill_far_evals", "evals where reach bound"),
    ("kill_elsewhere", "pool members claimed elsewhere"),
    ("op_form", "operations formed"),
    ("op_march", "operations launched"),
    ("op_strike", "operations at the walls"),
    ("op_regate", "march re-gate aborts"),
    ("march_odds", "launch odds, summed"),
    ("march_surv", "expected survivors, summed"),
    ("march_present", "present at launch, summed"),
    ("march_required", "required at launch, summed"),
    ("march_waited", "turns waited, summed"),
    ("tired_strike", "tired holds, strike seat"),
    ("tired_first", "tired holds, first_activities seat"),
    ("air_lines", "deep-air lines"),
    ("air_units", "enemy air in wave, summed"),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("roots", nargs="+")
    ap.add_argument("--arm", default="deep")
    args = ap.parse_args()

    arms = [collect(r, args.arm) for r in args.roots]
    names = [Path(r).name for r in args.roots]
    if not arms[0]:
        sys.exit("no completed games under %s" % args.roots[0])

    width = max(len(k[1]) for k in KEYS) + 2
    head = "%-*s" % (width, "per game")
    for n in names:
        head += "%12s" % n[:12]
    if len(arms) > 1:
        head += "%12s%9s%7s" % ("delta", "se", "t")
    print(head)
    for key, label in KEYS:
        row = "%-*s" % (width, label)
        present = False
        for a in arms:
            v = sum(g[key] for g in a.values()) / max(len(a), 1)
            present = present or v != 0
            row += "%12.2f" % v
        if len(arms) > 1:
            mean, se, t, wins = paired(arms[0], arms[-1], key)
            row += "%12.2f%9.2f%7.2f" % (mean, se, t)
        if present:
            print(row)
    print()
    for i, a in enumerate(arms):
        n = len(a)
        dec = sum(g["kill_declined"] for g in a.values())
        pos = sum(g["kill_positive"] for g in a.values())
        print("%-12s games=%d  decline rate=%.1f%% of positively priced"
              % (names[i][:12], n, 100.0 * dec / pos if pos else 0.0))


if __name__ == "__main__":
    main()
