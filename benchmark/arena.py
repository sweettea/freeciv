#!/usr/bin/env python3
"""Run ONE game in which different deep seats run different tuning files,
and report each seat's score keyed by the file it played.

    arena.py --config smoke --seed 201 --out DIR \
             --candidate a=tuning/a.ini --candidate b=tuning/b.ini
    arena.py --report DIR

WHY THIS EXISTS. Until PERPLAYER, FREECIV_DEEP_TUNING loaded one ini per
server process, so judging two candidate value sets cost two games. The
arena tier (findings/tuning-venues-2026-08-06.md section 6a) seats a
POPULATION of candidates at one table: the game itself punishes a weak
candidate at N times the throughput of one-candidate-per-world, and
punishes it sharply -- a weak defence gets eaten by a table-mate instead
of surfacing as a diffuse self-play score dip.

WHAT IT IS NOT. Arena fitness is population-dependent and can cycle. It
TRIAGES -- it never claims. Claims stay with the paired protocol in
run.sh, on held-out seeds, with the z-floor.

SEATING. Candidates are dealt round-robin over the deep seats, rotated
by (seed % seats) so a candidate does not always draw the same seat.
Seat quality is not uniform (one seat is one island at PLAYERS=2), so
one table ranks nothing on its own; run the same population over many
seeds and read the mean rank.

OUTPUT. One row per seat: seat, candidate, final score, rank. With
several games under --report DIR, the per-candidate mean rank too.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from extract import parse_scorelog                       # noqa: E402


def params_registry():
    """{key: type} from tuning-params.ini, the registry run.sh checks."""
    reg = {}
    for line in (HERE / "tuning-params.ini").read_text().split("\n"):
        m = re.match(r"\s*([a-z_0-9]+)\s*=.*,\s*(\w+)\s*$", line)
        if m:
            reg[m.group(1)] = m.group(2)
    return reg


def validate_tuning(path):
    """run.sh's three tuning-file checks, applied to a candidate.

    An arena runs several files at once, so every one of these failures
    would otherwise happen once PER CANDIDATE and still produce a game
    that looks fine: a missing [tuning] header applies nothing, an
    unknown key is a silently inert line, and a float tunable written
    without a decimal point is stored as an int and falls back to the
    compiled default. Each of the three has already cost this project a
    40-game A/B (run.sh's own comments name the dates).
    """
    text = Path(path).read_text()
    if not re.search(r"^\[tuning\]", text, re.M):
        raise SystemExit("%s has no [tuning] section, so NOTHING in it"
                         " would be applied" % path)
    reg = params_registry()
    seen = set()
    for line in text.split("\n"):
        m = re.match(r"\s*([a-z_0-9]+)\s*=\s*(\S+)", line)
        if not m:
            continue
        key, val = m.group(1), m.group(2).strip('"')
        if key not in reg:
            raise SystemExit("%s sets a key the server does not know: %s"
                             % (path, key))
        if reg[key] == "float" and "." not in val and "e" not in val.lower():
            raise SystemExit("%s writes float tunable %s with no decimal"
                             " point, which the loader SILENTLY ignores"
                             % (path, key))
        # A fourth check, found live 2026-08-14: secfile_load() REFUSES
        # a file with a duplicate key ("Tried to insert same value
        # twice"), the loader logs one error line and the seat plays
        # the COMPILED DEFAULTS. derby-lean.ini and pv3-lean.ini both
        # shipped with floor_interior_weight twice and were silently
        # unloadable the whole time.
        if key in seen:
            raise SystemExit("%s sets %s twice; secfile_load() refuses"
                             " the WHOLE file over a duplicate key and"
                             " the server falls back to the compiled"
                             " defaults" % (path, key))
        seen.add(key)


def config_path(name):
    p = Path(name)
    if p.is_file():
        return p.resolve()
    p = HERE / "configs" / (name if name.endswith(".conf") else name + ".conf")
    if not p.is_file():
        raise SystemExit("no such config: %s" % name)
    return p.resolve()


def read_players(config):
    """PLAYERS from the config file, without sourcing the whole thing."""
    for line in config.read_text().split("\n"):
        line = line.strip()
        if line.startswith("PLAYERS="):
            return int(line.split("=", 1)[1])
    raise SystemExit("config %s sets no PLAYERS" % config)


def seating(candidates, players, seed):
    """{seat name -> (candidate label, ini path)}, dealt round-robin."""
    off = seed % players
    seats = {}
    for i in range(players):
        label, path = candidates[i % len(candidates)]
        seats["P%d" % (((i + off) % players) + 1)] = (label, path)
    return seats


def write_manifest(path, seats):
    with open(path, "w") as f:
        f.write("; arena manifest written by benchmark/arena.py --"
                " see ai/deep/deeptuning.h for the loading scheme.\n")
        f.write("[arena]\n")
        for seat in sorted(seats, key=lambda s: int(s[1:])):
            f.write('%s = "%s"\n' % (seat, seats[seat][1]))


def run_game(args, seats):
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    manifest = out / "arena.ini"
    write_manifest(manifest, seats)
    with open(out / "arena-seats.txt", "w") as f:
        for seat in sorted(seats, key=lambda s: int(s[1:])):
            f.write("%s %s %s\n" % (seat, seats[seat][0], seats[seat][1]))

    env = dict(os.environ, ARENA=str(manifest))
    if args.tuning:
        validate_tuning(args.tuning)
        env["TUNING"] = str(Path(args.tuning).resolve())
    cmd = [str(HERE / "run-game.sh"), "--config", str(config_path(args.config)),
           "--seed", str(args.seed), "--arm", "deep", "--out", str(out),
           "--port", str(args.port)]
    if args.server:
        cmd += ["--server", str(Path(args.server).resolve())]
    rc = subprocess.call(cmd, env=env)
    if rc != 0:
        raise SystemExit("game failed; see %s/game.log" % out)
    return out


def seat_scores(gamedir):
    """[(seat name, final score)], from the game's own scorelog."""
    tags, players, data, final_turn = parse_scorelog(
        Path(gamedir) / "scorelog.txt")
    score_tag = next((t for t, n in tags.items() if n == "score"), None)
    if score_tag is None:
        raise SystemExit("scorelog has no 'score' tag: %s" % gamedir)
    rows = []
    for pid, name in sorted(players.items()):
        turns = [t for t in data.get(pid, {}) if score_tag in data[pid][t]]
        if not turns:
            continue
        rows.append((name, int(data[pid][max(turns)][score_tag])))
    return rows


def read_seats(gamedir):
    """{seat -> candidate label} as this game was actually dealt."""
    f = Path(gamedir) / "arena-seats.txt"
    if not f.is_file():
        return {}
    out = {}
    for line in f.read_text().split("\n"):
        parts = line.split()
        if len(parts) >= 2:
            out[parts[0]] = parts[1]
    return out


def report(gamedirs):
    """Per-seat attribution, and the mean rank per candidate over games."""
    ranks = {}
    for gd in gamedirs:
        seats = read_seats(gd)
        rows = seat_scores(gd)
        order = sorted(rows, key=lambda r: -r[1])
        rank_of = {name: i + 1 for i, (name, _) in enumerate(order)}
        print("== %s" % gd)
        print("   %-6s %-14s %8s %5s" % ("seat", "candidate", "score", "rank"))
        for name, score in sorted(rows, key=lambda r: r[0]):
            cand = seats.get(name, "?")
            print("   %-6s %-14s %8d %5d"
                  % (name, cand, score, rank_of[name]))
            ranks.setdefault(cand, []).append(rank_of[name])
    if len(gamedirs) > 1 or len(ranks) > 1:
        print("\n== candidates (%d game(s))" % len(gamedirs))
        print("   %-14s %6s %10s" % ("candidate", "seats", "mean rank"))
        for cand in sorted(ranks, key=lambda c: sum(ranks[c]) / len(ranks[c])):
            r = ranks[cand]
            print("   %-14s %6d %10.2f" % (cand, len(r), sum(r) / len(r)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--config", default="smoke")
    ap.add_argument("--seed", type=int, default=201)
    ap.add_argument("--out")
    ap.add_argument("--server")
    ap.add_argument("--port", type=int, default=5555)
    ap.add_argument("--tuning", help="base ini for seats no candidate names")
    ap.add_argument("--candidate", action="append", default=[],
                    metavar="LABEL=FILE",
                    help="a candidate tuning file; repeatable")
    ap.add_argument("--report", nargs="+", metavar="GAMEDIR",
                    help="report finished arena games instead of running one")
    args = ap.parse_args()

    if args.report:
        report(args.report)
        return

    if not args.candidate or not args.out:
        raise SystemExit("need --out and at least one --candidate LABEL=FILE")
    candidates = []
    for spec in args.candidate:
        if "=" not in spec:
            raise SystemExit("--candidate wants LABEL=FILE, got %r" % spec)
        label, path = spec.split("=", 1)
        p = Path(path)
        if not p.is_file():
            raise SystemExit("no such candidate file: %s" % path)
        validate_tuning(p)
        candidates.append((label, str(p.resolve())))

    players = read_players(config_path(args.config))
    if len(candidates) > players:
        raise SystemExit("%d candidates but only %d seats"
                         % (len(candidates), players))
    seats = seating(candidates, players, args.seed)
    out = run_game(args, seats)
    report([out])


if __name__ == "__main__":
    main()
