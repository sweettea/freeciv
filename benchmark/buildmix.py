#!/usr/bin/env python3
"""Per-player unit and improvement census from Freeciv savegames.

    buildmix.py SAVE                  # census of one save
    buildmix.py --series DIR [N]      # census every save in DIR/saves
                                      # (or DIR if it holds saves),
                                      # sampled every N turns (default 40)
    buildmix.py --csv DIR             # full series as CSV:
                                      # turn,player,cities,units,gold,
                                      # kind,name,count

Reads plain or zstd saves. Players are matched by benchmark naming
(P1, P2, ...); other players (barbarians, civil-war splinters) are
skipped. Used for the mechanism-level behaviour audits (STATUS
2026-07-31: distribution checks, not sightings).
"""
import io
import csv
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path


def read_save(path):
    path = str(path)
    if path.endswith(".zst"):
        return subprocess.run(["zstdcat", path], capture_output=True,
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


def census(path):
    """[(player, ncities, nunits, gold, unit Counter, impr Counter)]"""
    text = read_save(path)
    impr_names = next(csv.reader(io.StringIO(
        re.search(r"^improvement_vector=(.*)$", text, re.M).group(1))))
    out = []
    parts = re.split(r"^\[player(\d+)\]$", text, flags=re.M)
    for pid, body in zip(parts[1::2], parts[2::2]):
        name = re.search(r'^name="(.*?)"', body, re.M)
        if not name or not re.fullmatch(r"P\d+", name.group(1)):
            continue
        cities = parse_table(body, "c")
        units = parse_table(body, "u")
        imps = Counter()
        for c in cities:
            for i, b in enumerate(c.get("improvements", "")):
                if b == "1" and i < len(impr_names):
                    imps[impr_names[i]] += 1
        utypes = Counter(u.get("type_by_name", u.get("type", "?"))
                         for u in units)
        gold = re.search(r"^gold=(\d+)", body, re.M)
        out.append((name.group(1), len(cities), len(units),
                    int(gold.group(1)) if gold else 0, utypes, imps))
    return out


def save_turn(path):
    m = re.search(r"freeciv-T(\d+)", Path(path).name)
    return int(m.group(1)) if m else -1


def series_paths(d):
    d = Path(d)
    if (d / "saves").is_dir():
        d = d / "saves"
    return sorted(d.glob("freeciv-T*.sav*"), key=save_turn)


def print_census(path):
    for p, nc, nu, gold, utypes, imps in census(path):
        print(f"--- {p}: {nc} cities, {nu} units, gold={gold}")
        print("  units: " + ", ".join(f"{t}x{n}" for t, n in
                                      utypes.most_common()))
        print(f"  improvements: "
              + ", ".join(f"{k}={v}" for k, v in imps.most_common(14)))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "--series":
        step = int(sys.argv[3]) if len(sys.argv) > 3 else 40
        for f in series_paths(sys.argv[2]):
            t = save_turn(f)
            if t != 1 and t % step != 0 and "final" not in f.name:
                continue
            print(f"======== T{t} ({f.name})")
            print_census(f)
    elif len(sys.argv) >= 3 and sys.argv[1] == "--csv":
        w = csv.writer(sys.stdout)
        w.writerow(["turn", "player", "cities", "units", "gold",
                    "kind", "name", "count"])
        for f in series_paths(sys.argv[2]):
            t = save_turn(f)
            for p, nc, nu, gold, utypes, imps in census(f):
                for kind, ctr in (("unit", utypes), ("impr", imps)):
                    for nm, n in ctr.most_common():
                        w.writerow([t, p, nc, nu, gold, kind, nm, n])
    elif len(sys.argv) == 2:
        print_census(sys.argv[1])
    else:
        sys.exit(__doc__)
