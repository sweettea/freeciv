#!/usr/bin/env python3
"""Query facts from a Freeciv savegame (plain or zstd).

    savequery.py SAVE turn
    savequery.py SAVE extra-count NAME      # e.g. Hut
    savequery.py SAVE cities                # owner,x,y,size,original per line
    savequery.py SAVE captured              # cities whose founder != owner
    savequery.py SAVE wars                  # player pairs currently at War
    savequery.py SAVE min-cross-distance    # min city distance across owners

Used by the L1 corpus assertions (see corpus-run.sh). Parsing is
deliberately tolerant: it reads only the fields it needs.
"""

import csv
import io
import re
import subprocess
import sys
from pathlib import Path


def read_save(path):
    path = str(path)
    if path.endswith(".zst"):
        return subprocess.run(
            ["zstdcat", path], capture_output=True, text=True, check=True
        ).stdout
    return Path(path).read_text(encoding="utf-8", errors="replace")


def game_turn(text):
    m = re.search(r"^\[game\]$.*?^turn=(\d+)$", text, re.M | re.S)
    if not m:
        raise SystemExit("no [game] turn in save")
    return int(m.group(1))


def extra_count(text, name):
    """Count tiles carrying the named extra (e.g. Hut)."""
    m = re.search(r"^extras_vector=(.*)$", text, re.M)
    if not m:
        raise SystemExit("no extras_vector in save")
    names = next(csv.reader(io.StringIO(m.group(1))))
    idx = names.index(name)
    layer, bit = divmod(idx, 4)
    mask = 1 << bit
    count = 0
    for row in re.finditer(r"^e%02d_\d+=\"([0-9a-fA-F]+)\"$" % layer, text, re.M):
        count += sum(1 for c in row.group(1) if int(c, 16) & mask)
    return count


def _player_sections(text):
    """Yield (player_number, section_text)."""
    parts = re.split(r"^\[player(\d+)\]$", text, flags=re.M)
    for i in range(1, len(parts) - 1, 2):
        body = re.split(r"^\[", parts[i + 1], maxsplit=1, flags=re.M)[0]
        yield int(parts[i]), body


def _table(section, key):
    """Parse a `key={"col",... \n rows... \n }` table into dict rows."""
    m = re.search(r"^%s=\{(.*?)^\}" % re.escape(key), section, re.M | re.S)
    if not m:
        return []
    lines = m.group(1).splitlines()
    header = next(csv.reader(io.StringIO(lines[0])))
    rows = []
    for line in lines[1:]:
        if not line:
            continue
        vals = next(csv.reader(io.StringIO(line)))
        rows.append(dict(zip(header, vals)))
    return rows


def cities(text):
    """[(owner, x, y, size, original)] for every live city."""
    out = []
    for plr, section in _player_sections(text):
        for row in _table(section, "c"):
            out.append(
                (
                    plr,
                    int(row["x"]),
                    int(row["y"]),
                    int(row["size"]),
                    int(row["original"]),
                )
            )
    return out


def wars(text):
    """[(a, b)] pairs with diplstate current == War (a < b)."""
    out = set()
    for plr, section in _player_sections(text):
        for other, row in enumerate(_table(section, "diplstate")):
            if row.get("current") == "War":
                out.add((min(plr, other), max(plr, other)))
    return sorted(out)


def min_cross_distance(text):
    """Smallest euclidean city distance between different owners."""
    cs = cities(text)
    best = None
    for i, (o1, x1, y1, _, _) in enumerate(cs):
        for o2, x2, y2, _, _ in cs[i + 1:]:
            if o1 == o2:
                continue
            d = ((x1 - x2) ** 2 + (y1 - y2) ** 2) ** 0.5
            if best is None or d < best:
                best = d
    return best


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    text = read_save(sys.argv[1])
    q = sys.argv[2]
    if q == "turn":
        print(game_turn(text))
    elif q == "extra-count":
        print(extra_count(text, sys.argv[3]))
    elif q == "cities":
        for owner, x, y, size, orig in cities(text):
            print(f"{owner},{x},{y},{size},{orig}")
    elif q == "captured":
        for owner, x, y, size, orig in cities(text):
            if orig != owner:
                print(f"{owner},{x},{y},{size},{orig}")
    elif q == "wars":
        for a, b in wars(text):
            print(f"{a},{b}")
    elif q == "min-cross-distance":
        d = min_cross_distance(text)
        print("inf" if d is None else round(d, 2))
    else:
        raise SystemExit(f"unknown query: {q}")


if __name__ == "__main__":
    main()
