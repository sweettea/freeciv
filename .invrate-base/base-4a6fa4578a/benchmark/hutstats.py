#!/usr/bin/env python3
"""Exploration metrics from hut-config games (W06).

    hutstats.py --root DIR [--arm ARM]

Per completed game: mean techs per player at turn 50 (scorelog), huts
remaining on the map at game end, mean explored fraction at game end
(the [map] k-layers pack 4 players' known-bits per hex digit), and the
maximum deep-explore want seen in the log (0 everywhere is the hut-free
requirement; any positive want in a hut-free game is a violation).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

WANT_RE = re.compile(r'deep-explore turn=\d+ player="[^"]*" want=(\d+)')


def zcat(path):
    return subprocess.run(["zstdcat", str(path)], capture_output=True,
                          text=True, check=True).stdout


def techs_at(scorelog, turn):
    tags, data = {}, {}
    for line in open(scorelog, errors="replace"):
        parts = line.split(" ")
        if parts[0] == "tag":
            tags[parts[2].strip()] = int(parts[1])
        elif parts[0] == "data":
            t, tag, plr = int(parts[1]), int(parts[2]), int(parts[3])
            if t <= turn:
                data.setdefault(plr, {}).setdefault(tag, {})[t] = parts[4]
    tid = tags.get("techs")
    vals = []
    for plr, tagdata in data.items():
        turns = tagdata.get(tid, {})
        if turns:
            vals.append(int(turns[max(turns)]))
    return sum(vals) / len(vals) if vals else 0


def known_fraction(savetext, nplayers):
    main = savetext.split("[player0]")[0]
    per = []
    for plr in range(nplayers):
        layer, bit = divmod(plr, 4)
        mask = 1 << bit
        known = total = 0
        for m in re.finditer(r'^k%02d_\d+="([0-9a-fA-F]+)"$' % layer,
                             main, re.M):
            for c in m.group(1):
                total += 1
                if int(c, 16) & mask:
                    known += 1
        if total:
            per.append(known / total)
    return sum(per) / len(per) if per else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    T = [0.0, 0, 0.0, 0]
    print("arm,seed,techs_t50,huts_left,explored_frac,max_want")
    for d in gamedirs:
        techs = techs_at(d / "scorelog.txt", 50)
        final = sorted(d.glob("saves/*-final.sav*"))[0]
        text = zcat(final)
        huts = subprocess.run(
            ["python3", str(Path(__file__).parent / "savequery.py"),
             str(final), "extra-count", "Hut"],
            capture_output=True, text=True).stdout.strip() or "0"
        nplayers = len(re.findall(r"^\[player\d+\]$", text, re.M))
        frac = known_fraction(text, nplayers)
        wants = [int(w) for w in
                 WANT_RE.findall((d / "game.log").read_text(errors="replace"))]
        print(f"{d.parent.name},{d.name.removeprefix('seed-')},"
              f"{techs:.2f},{huts},{frac:.3f},{max(wants, default=0)}")
        T[0] += techs
        T[1] += int(huts)
        T[2] += frac
        T[3] += 1

    n = T[3]
    print(f"TOTAL,,{T[0]/n:.2f},{T[1]/n:.1f},{T[2]/n:.3f},")


if __name__ == "__main__":
    main()
