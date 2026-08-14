#!/usr/bin/env python3
"""Read the reinforcement model off deep-kill's robs= / rrate= fields.

    reinforce.py RESULT_DIR [RESULT_DIR ...]

robs= is what the target city can measurably build per turn --
surplus[O_SHIELD] / cost(best buildable defender). rrate= is what
deep_reinforce_rate() actually charged per turn of eta. At
invasion_reinforce_slope 0 the second is the flat knob on every
civilised line and the two are unrelated; the point of the pair is
that the FIRST is written in every arm, so the distribution a slope
would ride on is readable from an archived corpus without spending a
single sweep game on it.

Reports, per result dir and split by sea= (invasion wants vs land
wants), the observed-rate distribution and how it compares with the
rate the model charged. "over" is the fraction of observations where
the target can out-build what the model assumed -- if that is ~0, the
shipped flat rate is an overestimate everywhere and adding a slope on
top of it can only make deep more afraid, which is the reading that
chose this epic's A/B arms.
"""
import re
import sys
from pathlib import Path

LINE = re.compile(r"deep-kill turn=(\d+) .*? sea=(\d) eta=(\d+) "
                  r"robs=([0-9.]+) rrate=([0-9.]+)")


def quantile(v, q):
    return v[min(len(v) - 1, int(len(v) * q))] if v else float("nan")


def summarise(tag, rows):
    if not rows:
        print(f"  {tag:<12} (no observations)")
        return
    obs = sorted(r[3] for r in rows)
    n = len(obs)
    over = sum(1 for r in rows if r[3] > r[4]) / n
    charged = sorted(r[4] for r in rows)
    print(f"  {tag:<12} n={n:<7} robs mean={sum(obs)/n:.3f} "
          f"med={quantile(obs, .5):.3f} p90={quantile(obs, .9):.3f} "
          f"max={obs[-1]:.3f} zero={sum(1 for x in obs if x == 0)/n:.3f}  "
          f"charged med={quantile(charged, .5):.3f}  robs>charged={over:.3f}")


def main():
    for root in sys.argv[1:]:
        rows = []
        for log in sorted(Path(root).rglob("game.log")):
            for m in LINE.finditer(log.read_text(errors="replace")):
                rows.append((int(m.group(1)), int(m.group(2)),
                             int(m.group(3)), float(m.group(4)),
                             float(m.group(5))))
        print(f"{root}  ({len(rows)} deep-kill lines with the fields)")
        summarise("all", rows)
        summarise("sea=1", [r for r in rows if r[1] == 1])
        summarise("sea=0", [r for r in rows if r[1] == 0])
        # The rate is a per-turn charge multiplied by eta, so the
        # thing that actually reaches the odds is eta x rrate.
        for lo, hi, tag in ((1, 3, "eta 1-3"), (4, 6, "eta 4-6"),
                            (7, 99, "eta 7+")):
            summarise(tag, [r for r in rows if lo <= r[2] <= hi])
        print()


main()
