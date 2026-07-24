#!/usr/bin/env python3
"""Extract per-player metrics from benchmark game directories to CSV.

    extract.py GAMEDIR [GAMEDIR...]        rows for the given games
    extract.py --root DIR                  walk DIR for */seed-*/DONE

Each completed game directory (see run-game.sh) contains scorelog.txt in
SCORELOG2 format. One CSV row is emitted per player per game: the value
of every scorelog tag at that player's last logged turn, plus:

    arm, seed        parsed from the .../ARM/seed-N/ path
    last_turn        last turn with data for this player
    alive            1 if the player survived to the game's final turn
    mean_city_size   citizens per city, derived as
                     (happypop + contentpop + unhappypop + specialists)
                     / cities

The CSV goes to stdout, or --csv FILE.
"""

import argparse
import csv
import sys
from pathlib import Path


def parse_scorelog(path):
    """Return (tags, players, data, final_turn).

    tags: {tag_id: name}; players: {player_id: name};
    data: {player_id: {turn: {tag_id: value}}}.
    """
    tags = {}
    players = {}
    data = {}
    final_turn = 0
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            parts = line.split(" ")
            cmd = parts[0]
            if cmd == "tag":
                tags[int(parts[1])] = parts[2]
            elif cmd == "turn":
                final_turn = max(final_turn, int(parts[1]))
            elif cmd == "addplayer":
                players[int(parts[2])] = " ".join(parts[3:])
            elif cmd == "data":
                turn, tag, plr = int(parts[1]), int(parts[2]), int(parts[3])
                data.setdefault(plr, {}).setdefault(turn, {})[tag] = parts[4]
    return tags, players, data, final_turn


def game_rows(gamedir):
    gamedir = Path(gamedir)
    scorelog = gamedir / "scorelog.txt"
    if not scorelog.is_file():
        raise FileNotFoundError(f"no scorelog.txt in {gamedir}")

    seed = gamedir.name.removeprefix("seed-")
    arm = gamedir.parent.name

    tags, players, data, final_turn = parse_scorelog(scorelog)
    by_name = {name: tid for tid, name in tags.items()}
    tag_names = [tags[tid] for tid in sorted(tags)]

    rows = []
    for plr in sorted(players):
        turns = data.get(plr, {})
        if not turns:
            continue
        last_turn = max(turns)
        vals = turns[last_turn]
        row = {
            "arm": arm,
            "seed": seed,
            "player_id": plr,
            "name": players[plr],
            "last_turn": last_turn,
            "alive": int(last_turn == final_turn),
        }
        for tid in sorted(tags):
            row[tags[tid]] = vals.get(tid, "")

        try:
            citizens = sum(
                int(vals[by_name[t]])
                for t in ("happypop", "contentpop", "unhappypop", "specialists")
            )
            cities = int(vals[by_name["cities"]])
            row["mean_city_size"] = (
                round(citizens / cities, 3) if cities else 0
            )
        except (KeyError, ValueError):
            row["mean_city_size"] = ""
        rows.append(row)

    header = (
        ["arm", "seed", "player_id", "name", "last_turn", "alive"]
        + tag_names
        + ["mean_city_size"]
    )
    return header, rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gamedirs", nargs="*")
    ap.add_argument("--root", help="walk this dir for */seed-*/DONE")
    ap.add_argument("--csv", help="write here instead of stdout")
    args = ap.parse_args()

    dirs = [Path(d) for d in args.gamedirs]
    if args.root:
        dirs += sorted(
            p.parent for p in Path(args.root).glob("*/seed-*/DONE")
        )
    if not dirs:
        ap.error("no game directories given")

    header = None
    all_rows = []
    for d in dirs:
        h, rows = game_rows(d)
        if header is None:
            header = h
        elif h != header:
            sys.exit(f"inconsistent scorelog tags in {d}")
        all_rows.append((d, rows))

    out = open(args.csv, "w", newline="") if args.csv else sys.stdout
    w = csv.DictWriter(out, fieldnames=header)
    w.writeheader()
    for _, rows in all_rows:
        w.writerows(rows)
    if args.csv:
        out.close()
        print(f"{sum(len(r) for _, r in all_rows)} rows -> {args.csv}")


if __name__ == "__main__":
    main()
