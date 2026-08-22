#!/usr/bin/env python3
"""When each surviving city was founded, and by which module.

    ./foundcurve.py DIR [DIR ...] [--bin 10]

Reads the final savegame of every mixed game under DIR and pairs each
city's `turn_founded` with the module of the seat that ORIGINALLY
founded it (`original`, not the current owner), so conquest does not
contaminate the founding curve the way a scorelog's city count does.

That distinction is the whole point: on a war venue `cities` moves for
two reasons and only one of them is expansion. The savegame separates
them; the scorelog cannot.

Cities razed or never captured are of course absent -- this is the
curve of cities that SURVIVED to the final save, which is the
conservative direction for "who kept founding longer".
"""

import argparse
import collections
import csv
import io
import os
import re
import subprocess
import sys


def read_save(path):
    if path.endswith(".zst"):
        return subprocess.run(["zstdcat", path], capture_output=True,
                              text=True, check=True).stdout
    return open(path, errors="replace").read()


def seat_modules(cmdpath):
    """{player-number: module} from commands.serv."""
    out = {}
    for line in open(cmdpath, errors="replace"):
        m = re.match(r"create\s+P(\d+)\s+(\w+)", line.strip())
        if m:
            out[int(m.group(1)) - 1] = m.group(2)
    return out


def cities(text):
    """[(owner_player_number, turn_founded, original, size)]."""
    out = []
    plr = None
    hdr = None
    for line in text.splitlines():
        m = re.match(r"\[player(\d+)\]", line)
        if m:
            plr = int(m.group(1))
            hdr = None
            continue
        if line.startswith('c={"y","x","id","original"'):
            hdr = next(csv.reader(io.StringIO(line[2:])))
            continue
        if hdr is not None and line.startswith('"'):
            hdr = None
            continue
        if hdr is not None and line and line[0].isdigit():
            row = next(csv.reader(io.StringIO(line)))
            if len(row) < len(hdr):
                continue
            d = dict(zip(hdr, row))
            try:
                out.append((plr, int(d["turn_founded"]), int(d["original"]),
                            int(d["size"])))
            except (KeyError, ValueError):
                pass
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--bin", type=int, default=10)
    args = ap.parse_args()

    per = collections.defaultdict(lambda: collections.Counter())
    size = collections.defaultdict(list)
    games = 0
    for root in args.dirs:
        for dirpath, _, names in os.walk(root):
            fin = [n for n in names if "-final.sav" in n]
            cmd = os.path.join(os.path.dirname(dirpath), "commands.serv")
            if not fin or not os.path.exists(cmd):
                continue
            mods = seat_modules(cmd)
            text = read_save(os.path.join(dirpath, fin[0]))
            games += 1
            for _owner, tf, orig, sz in cities(text):
                mod = mods.get(orig)
                if mod is None:
                    continue
                per[mod][max(1, tf) // args.bin * args.bin] += 1
                size[mod].append((tf, sz))
    if not games:
        sys.exit("no final saves found")

    mods = sorted(per)
    print("# foundcurve: %d games, cities by FOUNDING module and turn"
          % games)
    print("  %8s %s" % ("turn", " ".join("%9s" % m for m in mods)))
    edges = sorted({b for m in mods for b in per[m]})
    cum = {m: 0 for m in mods}
    for b in edges:
        row = []
        for m in mods:
            cum[m] += per[m][b]
            row.append("%4d/%-4d" % (per[m][b], cum[m]))
        print("  %8s %s" % ("%d-%d" % (b, b + args.bin - 1),
                            " ".join("%9s" % c for c in row)))
    print("\n# mean final size of a city by when it was founded")
    print("  %8s %s" % ("founded", " ".join("%9s" % m for m in mods)))
    for lo in (1, 21, 41, 61, 81, 121):
        hi = {1: 20, 21: 40, 41: 60, 61: 80, 81: 120}.get(lo, 999)
        row = []
        for m in mods:
            v = [s for tf, s in size[m] if lo <= tf <= hi]
            row.append("%5.2f/%-3d" % (sum(v) / len(v), len(v)) if v
                       else "     -/0 ")
        print("  %8s %s" % ("%d-%d" % (lo, hi),
                            " ".join("%9s" % c for c in row)))


if __name__ == "__main__":
    main()
