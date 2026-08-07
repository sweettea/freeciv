#!/usr/bin/env python3
"""How far up the tech tree does each benchmark cell actually get?

Gates W32 (air doctrine) and the rest of W35: a mechanism that needs
Flight, or Espionage, or a Spy rather than a Diplomat, is worth nothing
in a cell that ends three eras earlier. Nothing in the harness recorded
this -- metrics.csv counts techs but never says WHICH -- so this reads
the archived final savegames, whose research rows carry a per-advance
bitstring, and maps the bits back through the ruleset's own advance
order.

Usage:  benchmark/era-check.py [--results DIR] [--cells a,b,c]

Reads only archived results; runs nothing.
"""
import argparse
import collections
import csv
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# The canonical portfolio (configs/README.md) plus the tools cell, in
# the order a report should read them.
CELLS = ["smoke", "lategame-80", "primary-120", "contested",
         "isolated-150", "isolated-classic", "garden2b"]

# Landmarks worth naming, because a workstream hangs off each: the
# gunpowder line is when defence stops being spears, Sanitation and
# Railroad are the classic industrial gate, Flight is W32's floor, and
# Espionage is when a Diplomat becomes a Spy (W35's later slices).
LANDMARKS = ["Bronze Working", "Currency", "Monarchy", "Philosophy",
             "Feudalism", "Gunpowder", "Sanitation", "Railroad",
             "Industrialization", "Electricity", "The Corporation",
             "Espionage", "Automobile", "Flight", "Advanced Flight",
             "Rocketry", "Computers", "Space Flight"]


def advance_order(ruleset):
    """Advance names in ruleset order, which is what the savegame's
    per-research bitstring is indexed by. Index 0 is None (always
    known), so the list is offset by one."""
    path = os.path.join(ROOT, "data", ruleset, "techs.ruleset")
    names = ["None"]
    with open(path, encoding="utf-8") as f:
        section = None
        for line in f:
            line = line.strip()
            m = re.match(r"^\[(advance_[a-z0-9_]+)\]", line)
            if m:
                section = m.group(1)
                continue
            if section is None:
                continue
            m = re.match(r'^name\s*=\s*_\("([^"]+)"\)', line)
            if m:
                # Some names carry a translation qualifier the ruleset
                # writes into the string itself: _("?tech:Railroad").
                names.append(re.sub(r"^\?[^:]*:", "", m.group(1)))
                section = None
    return names


def read_save_techs(path, names):
    """Every advance any player in this game knows, from the research
    rows of a final savegame."""
    try:
        raw = subprocess.run(["zstd", "-dc", path], capture_output=True,
                             timeout=120).stdout.decode("utf-8", "replace")
    except Exception:
        return None
    known = set()
    best = 0
    in_r = False
    for line in raw.split("\n"):
        if line.startswith('r={"number"'):
            in_r = True
            continue
        if in_r:
            if line.startswith("}"):
                break
            m = re.search(r'"([01]+)"\s*$', line)
            if not m:
                continue
            bits = m.group(1)
            n = 0
            for i, ch in enumerate(bits):
                if ch == "1" and i < len(names):
                    if i > 0:
                        n += 1
                    known.add(names[i])
            best = max(best, n)
    return known, best


def newest_dir_for(results, cell):
    """The most recently written result dir for this cell that has a
    metrics.csv. Cells are content-addressed, so a cell has many."""
    cands = []
    for d in os.listdir(results):
        if not re.match(r"^%s-[0-9a-f]{12}$" % re.escape(cell), d):
            continue
        full = os.path.join(results, d)
        mcsv = os.path.join(full, "metrics.csv")
        if os.path.exists(mcsv):
            cands.append((os.path.getmtime(mcsv), full))
    if not cands:
        return None
    return sorted(cands)[-1][1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=os.path.join(ROOT, "benchmark",
                                                      "results"))
    ap.add_argument("--cells", default=",".join(CELLS))
    ap.add_argument("--ruleset", default="civ2civ3")
    args = ap.parse_args()

    names = advance_order(args.ruleset)
    print("ruleset %s: %d advances in order (index 0 = %s)\n"
          % (args.ruleset, len(names) - 1, names[0]))

    hdr = "%-18s %-14s %5s %6s %6s %6s   %s" % (
        "cell", "dir", "games", "T", "techs", "max", "highest landmark any"
        " player reached")
    print(hdr)
    print("-" * len(hdr))

    reached_any = collections.Counter()
    for cell in args.cells.split(","):
        d = newest_dir_for(args.results, cell)
        if d is None:
            print("%-18s (no archived results)" % cell)
            continue

        turns, techs = [], []
        with open(os.path.join(d, "metrics.csv")) as f:
            for row in csv.DictReader(f):
                turns.append(int(row["last_turn"]))
                techs.append(int(row["techs"]))
        if not techs:
            print("%-18s (empty metrics.csv)" % cell)
            continue

        known = set()
        saves = 0
        per_game = collections.Counter()
        for arm in sorted(os.listdir(d)):
            armdir = os.path.join(d, arm)
            if not os.path.isdir(armdir):
                continue
            for seed in sorted(os.listdir(armdir)):
                sd = os.path.join(armdir, seed, "saves")
                if not os.path.isdir(sd):
                    continue
                finals = [x for x in os.listdir(sd) if "final" in x]
                if not finals:
                    continue
                got = read_save_techs(os.path.join(sd, finals[0]), names)
                if got:
                    known |= got[0]
                    saves += 1
                    for lm in LANDMARKS:
                        if lm in got[0]:
                            per_game[lm] += 1

        top = "-"
        for lm in LANDMARKS:
            if lm in known:
                top = lm
                reached_any[lm] += 1
        print("%-18s %-14s %5d %6d %6.1f %6d   %s"
              % (cell, os.path.basename(d)[-12:], len(techs),
                 max(turns), sum(techs) / len(techs), max(techs),
                 top if saves else "(no savegames)"))
        got = ["%s %d/%d" % (lm, per_game[lm], saves)
               for lm in LANDMARKS if lm in known]
        print("%-18s %-14s %5s %6s %6s %6s   games reaching: %s"
              % ("", "", "", "", "", "", ", ".join(got) if got else "none"))

    print("\ncells reaching each landmark (of those with savegames):")
    for lm in LANDMARKS:
        print("  %-20s %d" % (lm, reached_any[lm]))


if __name__ == "__main__":
    sys.exit(main())
