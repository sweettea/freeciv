#!/usr/bin/env python3
"""Plant a hostile corridor across a travelling settler's route.

    make-corridor.py SAVE_IN SAVE_OUT [--attacker N] [--victim N]

Takes a savegame in which the victim player has a settler underway
(go=TRUE with a goto target several tiles off), and edits it so that

  - victim and attacker are at War (both diplstate directions), and
  - a line of attacker Warriors sits across the settler's route a few
    tiles ahead of it, on land -- close enough to enter the settler's
    field of view on its next steps.

Exit status 0 and a one-line summary on success; 3 if this save has no
usable settler/corridor (caller should try another turn or seed).

The save is edited as text: unit rows are appended to the attacker's
`u` table (nunits and identity_number_used bumped accordingly), so the
result stays loadable by the unmodified server. Used by
make-positions.sh to generate the hostile-corridor position (W05).
"""

import argparse
import csv
import io
import re
import subprocess
import sys
from pathlib import Path

WATER = set(" :+i")   # civ2civ3 ocean/deep ocean/lake/inaccessible
CORRIDOR_AHEAD = 3    # tiles ahead of the settler along its route
CORRIDOR_HALF = 2     # half-width; 2 -> up to 5 warriors

# Columns of a unit row whose values are strings (must be re-quoted
# when the row is written back).
UNIT_STR_COLS = ("facing", "type_by_name", "carrying",
                 "orders_list", "dir_list", "activity_list")


def read_save(path):
    path = str(path)
    if path.endswith(".zst"):
        return subprocess.run(["zstdcat", path], capture_output=True,
                              text=True, check=True).stdout
    return Path(path).read_text(encoding="utf-8", errors="replace")


def parse_table(lines, start):
    """Table starting at lines[start] ('key={"col",...'); returns
    (header, rows, end) where rows are lists of raw strings and
    lines[end] is the closing brace."""
    first = lines[start]
    header = next(csv.reader(io.StringIO(first[first.index("{") + 1:])))
    rows = []
    i = start + 1
    while not lines[i].startswith("}"):
        rows.append(next(csv.reader(io.StringIO(lines[i]))))
        i += 1
    return header, rows, i


def section_range(lines, name):
    tag = f"[{name}]"
    try:
        start = lines.index(tag)
    except ValueError:
        raise SystemExit(f"no {tag} section")
    for j in range(start + 1, len(lines)):
        if lines[j].startswith("["):
            return start, j
    return start, len(lines)


def find_in(lines, lo, hi, pattern):
    rx = re.compile(pattern)
    for i in range(lo, hi):
        if rx.match(lines[i]):
            return i
    return None


def terrain_grid(text):
    return {int(m.group(1)): m.group(2)
            for m in re.finditer(r'^t(\d+)="(.*)"$', text, re.M)}


def is_number(v):
    return re.fullmatch(r"-?\d+|TRUE|FALSE", v) is not None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("save_in")
    ap.add_argument("save_out")
    ap.add_argument("--attacker", type=int, default=1)
    ap.add_argument("--victim", type=int, default=0)
    args = ap.parse_args()

    text = read_save(args.save_in)
    lines = text.splitlines()
    terrain = terrain_grid(text)

    # --- find a travelling settler of the victim -------------------------
    vlo, vhi = section_range(lines, f"player{args.victim}")
    u_at = find_in(lines, vlo, vhi, r"u=\{")
    if u_at is None:
        sys.exit(3)
    header, rows, _ = parse_table(lines, u_at)
    col = {name: k for k, name in enumerate(header)}
    settler = None
    for r in rows:
        if r[col["type_by_name"]] == "Settlers" and r[col["go"]] == "TRUE":
            sx, sy = int(r[col["x"]]), int(r[col["y"]])
            gx, gy = int(r[col["goto_x"]]), int(r[col["goto_y"]])
            if max(abs(gx - sx), abs(gy - sy)) >= 4:
                settler = (sx, sy, gx, gy)
                break
    if settler is None:
        sys.exit(3)
    sx, sy, gx, gy = settler

    # --- corridor tiles: a line across the route, a bit ahead ------------
    dx, dy = gx - sx, gy - sy
    dist = max(abs(dx), abs(dy))
    step = min(CORRIDOR_AHEAD, dist - 1) / dist
    cx, cy = round(sx + dx * step), round(sy + dy * step)
    # Perpendicular to the dominant direction of travel.
    px, py = (0, 1) if abs(dx) >= abs(dy) else (1, 0)
    tiles = []
    for k in range(-CORRIDOR_HALF, CORRIDOR_HALF + 1):
        x, y = cx + k * px, cy + k * py
        if y not in terrain or not (0 <= x < len(terrain[y])):
            continue
        if terrain[y][x] in WATER:
            continue
        if (x, y) in ((sx, sy), (gx, gy)):
            continue
        tiles.append((x, y))
    if len(tiles) < 3:
        sys.exit(3)

    # --- declare war both ways -------------------------------------------
    for me, other in ((args.victim, args.attacker),
                      (args.attacker, args.victim)):
        lo, hi = section_range(lines, f"player{me}")
        d_at = find_in(lines, lo, hi, r"diplstate=\{")
        dh, drows, dend = parse_table(lines, d_at)
        dcol = {name: k for k, name in enumerate(dh)}
        row = drows[other]
        row[dcol["current"]] = "War"
        row[dcol["closest"]] = "War"
        row[dcol["first_contact_turn"]] = "1"
        lines[d_at + 1:dend] = [
            ",".join(v if is_number(v) else '"%s"' % v for v in r)
            for r in drows]

    # --- append warriors to the attacker ---------------------------------
    alo, ahi = section_range(lines, f"player{args.attacker}")
    au_at = find_in(lines, alo, ahi, r"u=\{")
    if au_at is None:
        sys.exit(3)
    aheader, arows, aend = parse_table(lines, au_at)
    acol = {name: k for k, name in enumerate(aheader)}
    if not arows:
        sys.exit(3)
    template = arows[0]
    strcols = {acol[c] for c in UNIT_STR_COLS}

    # Unit and city ids share one identity space; take max over every
    # id-leading table row in the save.
    next_id = 1 + max(int(m.group(1))
                      for m in re.finditer(r'^(\d+),-?\d+,', text, re.M))

    new_rows = []
    for x, y in tiles:
        r = list(template)
        r[acol["id"]] = str(next_id)
        next_id += 1
        r[acol["x"]], r[acol["y"]] = str(x), str(y)
        r[acol["type_by_name"]] = "Warriors"
        r[acol["veteran"]] = "0"
        r[acol["hp"]] = "10"
        r[acol["homecity"]] = "0"
        r[acol["activity"]] = "0"
        r[acol["activity_count"]] = "0"
        r[acol["done_moving"]] = "TRUE"
        r[acol["moves"]] = "0"
        r[acol["go"]] = "FALSE"
        r[acol["goto_x"]] = "0"
        r[acol["goto_y"]] = "0"
        r[acol["transported_by"]] = "-1"
        r[acol["orders_length"]] = "0"
        new_rows.append(",".join(
            v if k not in strcols else '"%s"' % v for k, v in enumerate(r)))

    lines[aend:aend] = new_rows

    # --- bump counters ----------------------------------------------------
    n_at = find_in(lines, alo, ahi, r"nunits=")
    lines[n_at] = "nunits=%d" % (int(lines[n_at].split("=")[1])
                                 + len(new_rows))
    plo, phi = section_range(lines, "players")
    i_at = find_in(lines, plo, phi, r"identity_number_used=")
    lines[i_at] = "identity_number_used=%d" % (
        int(lines[i_at].split("=")[1]) + len(new_rows))

    Path(args.save_out).write_text("\n".join(lines) + "\n",
                                   encoding="utf-8")
    print(f"corridor: settler ({sx},{sy})->({gx},{gy}); "
          f"{len(new_rows)} warriors at {tiles}")


if __name__ == "__main__":
    main()
