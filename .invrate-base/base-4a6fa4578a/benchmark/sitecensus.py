#!/usr/bin/env python3
"""Site-truth censuses from a result dir's game logs (session D3).

    sitecensus.py RESULTS_DIR [RESULTS_DIR...] [--arm deep]

Reports, per result dir, means per game over the arm's games:

  naval    the closed-water premium gate (deep-sitenaval): candidates
           paid the maritime premium, candidates refused it because
           their water closes on its own continent, and the site score
           those refusals withheld.
  lift     mid-crossing mission invalidation (deep-lift-invalid): the
           frequency D-E was never able to quote, plus how many of them
           killed the beachhead itself, and the re-aims and aborts that
           followed.

Both lines are metric-level, so they are present in every benchmark
game log without a debug build. The per-candidate defence line
(deep-sitedef) is LOG_VERBOSE and deliberately NOT read here: it only
exists in a probe run.
"""
import re
import sys
from pathlib import Path
from statistics import mean

NAVAL = re.compile(r'deep-sitenaval turn=(\d+) player="([^"]+)" unit=\d+'
                   r' paid=(\d+) refused=(\d+) withheld=([\d.]+)')
INVALID = re.compile(r'deep-lift-invalid turn=(\d+) player="([^"]+)"'
                     r' op=\d+ unit=\d+ target=\(\d+,\d+\) beach=\(\d+,\d+\)'
                     r' wasbeach=(\d+) boatd=(\d+) turns=(\d+)')
REAIM = re.compile(r'deep-lift-op turn=\d+ player="[^"]+" op=\d+ reaim')
ABORT = re.compile(r'deep-op turn=\d+ player="[^"]+" op=\d+ lift-invalid')
ARRIVE = re.compile(r'deep-lift-op turn=\S+ player="[^"]+" op=\d+ arrive')


def games(root, arm):
    for d in sorted((Path(root) / arm).glob("seed-*")):
        log = d / "game.log"
        if log.exists():
            yield d.name, log.read_text(errors="replace")


def main():
    argv = list(sys.argv[1:])
    arm = "deep"
    if "--arm" in argv:
        i = argv.index("--arm")
        arm = argv[i + 1]
        del argv[i:i + 2]
    for root in argv:
        rows = list(games(root, arm))
        if not rows:
            print(f"{root}: no {arm} games")
            continue
        paid, refused, withheld = [], [], []
        inval, wasbeach, reaim, arrive, abort = [], [], [], [], []
        dist, age = [], []
        for _, text in rows:
            n = [(int(m.group(3)), int(m.group(4)), float(m.group(5)))
                 for m in NAVAL.finditer(text)]
            paid.append(sum(x[0] for x in n))
            refused.append(sum(x[1] for x in n))
            withheld.append(sum(x[2] for x in n))
            iv = list(INVALID.finditer(text))
            inval.append(len(iv))
            wasbeach.append(sum(1 for m in iv if m.group(3) == "1"))
            dist += [int(m.group(4)) for m in iv]
            age += [int(m.group(5)) for m in iv]
            reaim.append(len(REAIM.findall(text)))
            abort.append(len(ABORT.findall(text)))
            arrive.append(len(ARRIVE.findall(text)))
        print(f"== {root}  ({len(rows)} {arm} games)")
        print(f"   naval premium paid      {mean(paid):>10.1f} /game")
        print(f"   naval premium REFUSED   {mean(refused):>10.1f} /game"
              f"   ({sum(1 for x in refused if x)} games > 0)")
        print(f"   site score withheld     {mean(withheld):>10.1f} /game")
        print(f"   lift-invalid            {mean(inval):>10.2f} /game"
              f"   ({sum(1 for x in inval if x)} games > 0)")
        print(f"     of which killed beach {mean(wasbeach):>10.2f} /game")
        print(f"   lift re-aims            {mean(reaim):>10.2f} /game")
        print(f"   lift aborts (invalid)   {mean(abort):>10.2f} /game")
        print(f"   lift-arrived            {mean(arrive):>10.2f} /game")
        if dist:
            print(f"   invalidation: boat {mean(dist):.1f} tiles from the"
                  f" beachhead, {mean(age):.1f} turns after forming"
                  f" (n={len(dist)})")


main()
