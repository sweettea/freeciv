#!/usr/bin/env python3
"""Building-ladder census of final saves, split by ai_type.

    ladder.py --root RESULTDIR [--arm ARM] [--save-glob GLOB]

Walks RESULTDIR for */seed-*/DONE game dirs (see run.sh), reads the
newest matching save in each, and counts improvements per player,
pooling players by their `ai_type=` field. Prints counts per 100
cities so arms with different city counts stay comparable.

This exists because `buildmix.py` matches players by benchmark naming
(P1, P2, ...) and therefore outputs nothing on the real-nation mixed
games that every tuning cell uses. The ai_type field is written by the
server for every player, so it works on both.

The ladder columns (see findings/happiness-ladder.md) are reported
separately: CAPACITY (the hard size cap), CONTENT (the soft cap), and
MULTIPLIER (the percentage buildings, which `base_want()` does price
but only against today's tile arrangement).
"""

import argparse
import csv
import io
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

# RULESET-DEPENDENT. These are civ2civ3 / classic names, matched
# literally. A building absent from the ruleset simply never appears in
# improvement_vector and contributes nothing -- but a ruleset that
# renames a building, or gives it a different role (civ2civ3's
# Courthouse makes citizens content where classic's only cuts
# corruption), is silently mis-filed rather than flagged. Check this
# table against the ruleset before comparing columns across rulesets.
COLUMNS = {
    "CAPACITY": ["Aqueduct", "Aqueduct, Lake", "Aqueduct, River",
                 "Sewer System", "Harbour", "Supermarket", "Granary"],
    "CONTENT": ["Temple", "Colosseum", "Cathedral", "Amphitheater",
                "Courthouse", "Police Station"],
    "MULTIPLIER": ["Marketplace", "Library", "Bank", "University",
                   "Stock Exchange", "Factory", "Power Plant",
                   "Hydro Plant", "Nuclear Plant", "Mfg. Plant",
                   "Offshore Platform", "Research Lab"],
}
# Courthouse sits in CONTENT above because civ2civ3 gives it a content
# effect, but its corruption cut is a multiplier; report it on its own
# line too rather than argue about the column.
SOLO = ["Marketplace", "Library", "Bank", "University", "Courthouse",
        "Temple", "Aqueduct", "Granary", "Harbour", "Colosseum",
        "Factory"]


def read_save(path):
    if str(path).endswith(".zst"):
        return subprocess.run(["zstdcat", str(path)], capture_output=True,
                              text=True, check=True).stdout
    return Path(path).read_text(encoding="utf-8", errors="replace")


def parse_table(section, prefix):
    """Parse an INI table like c={"col",...<rows>...}. Returns row dicts."""
    m = re.search(r"^%s=\{(.*?)^\}" % prefix, section, re.M | re.S)
    if not m:
        return []
    lines = m.group(1).strip().splitlines()
    cols = next(csv.reader(io.StringIO(lines[0])))
    return [dict(zip(cols, next(csv.reader(io.StringIO(ln)))))
            for ln in lines[1:]]


# Barbarians, pirates and the animal kingdom are servergenerated
# players. They carry an ai_type like anyone else (`classic`, whatever
# the server default is), so pooling by ai_type alone silently mixes
# them into an arm.
NONPLAYERS = {"Barbarian", "Pirate", "Animal Kingdom"}


def census(path):
    """[(ai_type, ncities, improvement Counter, total citizens)]"""
    text = read_save(path)
    # improvement_vector holds ruleset names, which contain commas in
    # civ2civ3 ("Aqueduct, Lake"), so it must go through csv.reader.
    impr_names = next(csv.reader(io.StringIO(
        re.search(r"^improvement_vector=(.*)$", text, re.M).group(1))))
    out = []
    parts = re.split(r"^\[player\d+\]$", text, flags=re.M)
    for body in parts[1:]:
        ai = re.search(r'^ai_type="(.*?)"', body, re.M)
        if not ai:
            continue
        nation = re.search(r'^nation="(.*?)"', body, re.M)
        if nation and nation.group(1) in NONPLAYERS:
            continue
        cities = parse_table(body, "c")
        imps = Counter()
        citizens = 0
        for c in cities:
            citizens += int(c.get("size", 0) or 0)
            for i, b in enumerate(c.get("improvements", "")):
                # The bitstring is one char per improvement; anything
                # other than '0' means present (redundant/normal).
                if b != "0" and i < len(impr_names):
                    imps[impr_names[i]] += 1
        out.append((ai.group(1), len(cities), imps, citizens))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm", help="restrict to this arm subdirectory")
    ap.add_argument("--save-glob", default="*final.sav*",
                    help="which save to census (default the final one)")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    cities = Counter()
    citizens = Counter()
    games = Counter()
    imps = defaultdict(Counter)
    for d in gamedirs:
        saves = sorted(d.glob(f"saves/{args.save_glob}"))
        if not saves:
            print(f"no save matching {args.save_glob} in {d}", file=sys.stderr)
            continue
        for ai, n, counter, cits in census(saves[-1]):
            cities[ai] += n
            citizens[ai] += cits
            games[ai] += 1
            imps[ai] += counter

    types = sorted(cities)
    if not types:
        sys.exit("no players with an ai_type in any save")

    w = max(22, *(len(t) for t in types))
    print(f"{len(gamedirs)} games under {args.root}")
    print()
    print(f"{'':<{w}}" + "".join(f"{t:>14}" for t in types))
    print(f"{'players':<{w}}" + "".join(f"{games[t]:>14}" for t in types))
    print(f"{'cities':<{w}}" + "".join(f"{cities[t]:>14}" for t in types))
    print(f"{'mean city size':<{w}}" + "".join(
        f"{citizens[t] / cities[t]:>14.2f}" if cities[t] else f"{'-':>14}"
        for t in types))
    print()
    print("per 100 cities")
    for col, names in COLUMNS.items():
        print(f"{col:<{w}}" + "".join(
            f"{100 * sum(imps[t][n] for n in names) / cities[t]:>14.1f}"
            if cities[t] else f"{'-':>14}" for t in types))
    print()
    print("per 100 cities, individually (raw count in parens)")
    for name in SOLO:
        if not any(imps[t][name] for t in types):
            continue
        print(f"{name:<{w}}" + "".join(
            f"{100 * imps[t][name] / cities[t]:>8.1f}({imps[t][name]:>4})"
            if cities[t] else f"{'-':>14}" for t in types))


if __name__ == "__main__":
    main()
