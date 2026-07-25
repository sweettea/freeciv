#!/usr/bin/env python3
"""Terrain changed between game start and end (W28).

    terraform.py --root DIR [--arm ARM]

Counts, per completed game (see run.sh), the map tiles whose terrain
letter differs between the turn-1 autosave and the final save. Worker
transforms are the dominant cause; global warming/nuclear fallout can
contribute, so treat the number as directional and compare arms on the
same seeds. This operationalises W28's "long transforms actually get
built at all".
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path


def terrain(path):
    text = subprocess.run(["zstdcat", str(path)], capture_output=True,
                          text=True, check=True).stdout
    # The main map's terrain rows are the t-rows before the first
    # player section (player sections carry private known-terrain maps).
    main = text.split("[player0]")[0]
    return {int(m.group(1)): m.group(2)
            for m in re.finditer(r'^t(\d+)="(.*)"$', main, re.M)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    total = games = 0
    from collections import Counter
    origins = Counter()
    print("arm,seed,tiles_changed,by_origin")
    for d in gamedirs:
        first = sorted(d.glob("saves/freeciv-T0001-*-auto.sav*"))
        final = sorted(d.glob("saves/*-final.sav*"))
        if not first or not final:
            continue
        t0, t1 = terrain(first[0]), terrain(final[0])
        game_orig = Counter(f"{a}>{b}" for y in t0 if y in t1
                            for a, b in zip(t0[y], t1[y]) if a != b)
        changed = sum(game_orig.values())
        print(f"{d.parent.name},{d.name.removeprefix('seed-')},{changed},"
              + "+".join(f"{k}:{v}" for k, v in sorted(game_orig.items())))
        origins.update(game_orig)
        total += changed
        games += 1

    # A pair is chain-evidence when the destination is not the origin's
    # immediate transform_result (one-step pairs are stock-plausible).
    ONE_STEP = {"m>h", "h>p", "f>h", "j>f", "s>g", "d>p", "t>p",
                "g>+", "p>+", "a>+", " >g", "+>g"}
    chained = sum(v for k, v in origins.items() if k not in ONE_STEP)
    print(f"TOTAL,,{total} over {games} games "
          f"({total / games if games else 0:.1f}/game) pairs="
          + "+".join(f"{k}:{v}" for k, v in sorted(origins.items()))
          + f"  chain_evidence={chained} (pairs beyond one step)")


if __name__ == "__main__":
    main()
