#!/usr/bin/env python3
"""Show that the "unfixed" AI module plays exactly like the stock one.

For each seed, play the same all-AI game twice on one server binary:
once with every player of the stock type ("classic", built from
ai/default) and once with every player "unfixed" (the vendored copy in
ai/unfixed). Every turn is autosaved, and each pair of same-turn saves
must be identical once the fields that name the module are normalised.

Normalisation (and nothing else):
  * player%d.ai_type, and the "aitype" row of the settings table,
    become OWN.
  * Each module saves its private data for every player under its own
    savefile prefix: classic under "ai" (player relations
    ai%d.<REL_AI_FIELDS>, city columns ai.<CITY_AI_FIELDS>) and ""
    (unit columns <UNIT_AI_FIELDS>); unfixed under "unfixed" and
    "unfixed_". The prefix of the module that controls the players
    becomes OWN, the other one OBS, so the data of both modules is
    compared, not dropped.
  * Three wall-clock fields are dropped: game.last_turn_change_time
    (how long the last turn change took), game.phase_seconds (time
    spent in the current phase) and the timestamps of the event
    cache. They differ between two runs of the same module too
    (see --control).
Tables are expanded into flat keys first, so a column's position in
its table does not matter, only its name and value.

--mixed instead plays one game with half the players of each type to
the turn limit and checks that it ends, writes a final save, and logs
no errors (sanity check failures are logged as errors).

Every game runs under `nice -n 19 ionice -c3`.
"""

import argparse
import os
import re
import subprocess
import sys
import time

STOCK = "classic"
UNFIXED = "unfixed"
# The fields the AI's savegame hooks write (dai_player_save_relations,
# dai_city_save, dai_unit_save). Only these are renamed: the core saves
# fields of its own under "ai" too, e.g. player%d.ai%d.love.
REL_AI_FIELDS = ("spam", "countdown", "war_reason", "patience",
                 "warn_space", "ask_peace", "ask_alliance", "ask_ceasefire")
CITY_AI_FIELDS = ("urgency", "building_turn", "building_wait",
                  "founder_turn", "founder_want", "founder_boat")
UNIT_AI_FIELDS = ("passenger", "ferryboat", "charge", "bodyguard")
# Wall-clock fields. They differ between two runs of the same module.
DROPPED = (r"game\.last_turn_change_time$",
           r"game\.phase_seconds$",
           r"event_cache\.events\d+\.timestamp$")


def game_commands(args, seed, aitypes):
    """Server commands for one game; aitypes is a list (mixed) or a name."""
    # Always named: the server's default ruleset need not be classic.
    cmds = ["rulesetdir %s" % args.ruleset]
    if isinstance(aitypes, str):
        # aitype also decides the type of barbarians and animals.
        cmds += ["set aitype %s" % aitypes,
                 "set aifill 0",
                 "set aifill %d" % args.players]
    else:
        cmds += ["set aitype %s" % STOCK, "set aifill 0"]
        for i, t in enumerate(aitypes):
            cmds.append("create P%d %s" % (i, t))
    cmds += ["set minplayers 0",
             "set timeout -1",
             "set endturn %d" % args.turns,
             "set saveturns 1",
             "set compresstype PLAIN",
             "set gameseed %d" % seed,
             "set mapseed %d" % seed,
             "start"]
    return cmds


def play(args, outdir, seed, aitypes):
    """Play one game into outdir; return the server's log path."""
    os.makedirs(outdir, exist_ok=True)
    for f in os.listdir(outdir):
        if f.endswith(".sav"):
            os.unlink(os.path.join(outdir, f))
    logpath = os.path.join(outdir, "server.log")
    env = dict(os.environ, FREECIV_DATA_PATH=args.data)
    with open(logpath, "w") as log:
        srv = subprocess.Popen(
            ["nice", "-n", "19", "ionice", "-c3", args.server,
             "-s", outdir, "--Announce", "none", "-p", str(args.port)],
            stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT,
            env=env, text=True, cwd=outdir)
        srv.stdin.write("\n".join(game_commands(args, seed, aitypes)) + "\n")
        srv.stdin.flush()
        deadline = time.time() + args.game_timeout
        while not final_save(outdir):
            if srv.poll() is not None:
                sys.exit("server exited early; see %s" % logpath)
            if time.time() > deadline:
                srv.kill()
                sys.exit("game timed out; see %s" % logpath)
            time.sleep(1)
        time.sleep(1)
        try:
            srv.stdin.write("quit\n")
            srv.stdin.close()
        except BrokenPipeError:
            pass  # The server may already have quit after the game.
        srv.wait(timeout=60)
    return logpath


def final_save(outdir):
    for f in os.listdir(outdir):
        if f.endswith("-final.sav"):
            return f
    return None


def saves_by_turn(outdir):
    saves = {}
    for f in os.listdir(outdir):
        m = re.match(r"freeciv-T(\d+)-.*\.sav$", f)
        if m:
            saves[int(m.group(1))] = os.path.join(outdir, f)
    return saves


def split_csv(line):
    """Split one secfile table row or vector into its fields."""
    out, cur, inq, i = [], "", False, 0
    while i < len(line):
        c = line[i]
        if inq:
            cur += c
            if c == "\\" and i + 1 < len(line):
                cur += line[i + 1]
                i += 1
            elif c == '"':
                inq = False
        elif c == '"':
            inq = True
            cur += c
        elif c == ",":
            out.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    out.append(cur)
    return out


def open_quote(line):
    """Whether line ends inside a quoted string."""
    inq, i = False, 0
    while i < len(line):
        if inq and line[i] == "\\":
            i += 1
        elif line[i] == '"':
            inq = not inq
        i += 1
    return inq


def parse(path):
    """Flatten a plain-text savefile into {section.key: value}."""
    flat = {}
    section = ""
    table = None

    def put(key, value):
        key = section + "." + key
        if key in flat:
            sys.exit("%s: duplicate key %s" % (path, key))
        flat[key] = value

    with open(path, encoding="utf-8", errors="surrogateescape") as f:
        for line in f:
            line = line.rstrip("\n")
            # A quoted string may run over several lines.
            while open_quote(line):
                more = f.readline()
                if not more:
                    sys.exit("%s: unterminated string" % path)
                line += "\n" + more.rstrip("\n")
            if table is not None:
                if line == "}":
                    table = None
                    continue
                name, cols, rows = table
                vals = split_csv(line)
                if len(vals) != len(cols):
                    sys.exit("%s: bad row in table %s" % (path, name))
                for c, v in zip(cols, vals):
                    put("%s%d.%s" % (name, rows, c), v)
                table[2] += 1
                continue
            if not line or line.startswith(("#", ";")):
                continue
            if line.startswith("[") and line.endswith("]"):
                section = line[1:-1]
                continue
            m = re.match(r'([^=]+)=\{(.*)$', line)
            if m:
                cols = [c.strip('"') for c in split_csv(m.group(2))]
                table = [m.group(1), cols, 0]
                continue
            key, sep, value = line.partition("=")
            if not sep:
                sys.exit("%s: cannot parse line: %s" % (path, line[:80]))
            put(key, value)
    if table is not None:
        sys.exit("%s: unterminated table %s" % (path, table[0]))
    return flat


def canonical(flat, owner, dropped):
    """Rename the fields that name the module, relative to the owner."""
    other = UNFIXED if owner == STOCK else STOCK
    rel_prefix = {STOCK: "ai", UNFIXED: "unfixed"}
    unit_prefix = {STOCK: "", UNFIXED: "unfixed_"}
    role = {owner: "OWN", other: "OBS"}

    # The settings table row whose name is "aitype".
    aitype_rows = {k[:-len(".name")] for k, v in flat.items()
                   if re.match(r"settings\.set\d+\.name$", k)
                   and v == '"aitype"'}
    out = {}
    for key, value in flat.items():
        if any(re.match(d, key) for d in dropped):
            continue
        sect, _, rest = key.partition(".")
        newkey = key
        if re.match(r"player\d+$", sect):
            if rest == "ai_type":
                # Every player, barbarians included, must be the owner's
                # type, or the game was not the one it claims to be.
                if value.strip('"') != owner:
                    sys.exit("%s is %s, expected %s" % (key, value, owner))
                value = '"OWN"'
            for t in (STOCK, UNFIXED):
                # Relations: player%d.<prefix><n>.<field>
                m = re.match(r"%s(\d+)\.(%s)$"
                             % (rel_prefix[t], "|".join(REL_AI_FIELDS)), rest)
                if m:
                    newkey = "%s.%s%s.%s" % (sect, role[t], m.group(1),
                                             m.group(2))
                # City columns: c<n>.<prefix>.<field>
                m = re.match(r"(c\d+)\.%s\.(%s)$"
                             % (rel_prefix[t], "|".join(CITY_AI_FIELDS)),
                             rest)
                if m:
                    newkey = "%s.%s.%s.%s" % (sect, m.group(1), role[t],
                                              m.group(2))
                # Unit columns: u<n>.<prefix><field>
                m = re.match(r"(u\d+)\.%s(%s)$"
                             % (unit_prefix[t], "|".join(UNIT_AI_FIELDS)),
                             rest)
                if m:
                    newkey = "%s.%s.%s_%s" % (sect, m.group(1), role[t],
                                              m.group(2))
        elif key.rsplit(".", 1)[0] in aitype_rows \
                and key.endswith((".value", ".gamestart")):
            if value.strip('"') != owner:
                sys.exit("aitype setting is %s, expected %s" % (value, owner))
            value = '"OWN"'
        if newkey in out:
            sys.exit("normalisation collision at %s" % newkey)
        out[newkey] = value
    return out


def compare(a_dir, a_owner, b_dir, b_owner, turns, dropped,
            exhaustive=False):
    """Return None if every turn matches, else a description: of the
    first difference, or with exhaustive of every key that differs."""
    a_saves, b_saves = saves_by_turn(a_dir), saves_by_turn(b_dir)
    if sorted(a_saves) != sorted(b_saves):
        return "different save turns: %s vs %s" % (sorted(a_saves),
                                                   sorted(b_saves))
    if max(a_saves) < turns:
        return "games ended at T%d, before T%d" % (max(a_saves), turns)
    patterns = set()
    for t in sorted(a_saves):
        a = canonical(parse(a_saves[t]), a_owner, dropped)
        b = canonical(parse(b_saves[t]), b_owner, dropped)
        if a != b:
            keys = sorted(k for k in set(a) | set(b) if a.get(k) != b.get(k))
            if exhaustive:
                patterns |= {re.sub(r"\d+", "N", k) for k in keys}
                continue
            lines = ["  %s: %s | %s" % (k, a.get(k), b.get(k))
                     for k in keys[:10]]
            return ("first difference at T%d (%s), %d keys:\n%s"
                    % (t, os.path.basename(a_saves[t]), len(keys),
                       "\n".join(lines)))
    if patterns:
        return "keys that differ on some turn: " + ", ".join(sorted(patterns))
    return None


def log_errors(logpath):
    with open(logpath, errors="replace") as f:
        return [l.rstrip() for l in f if re.match(r"[01]: ", l)]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--server", required=True)
    ap.add_argument("--data", required=True, help="FREECIV_DATA_PATH")
    ap.add_argument("--workdir", required=True)
    ap.add_argument("--ruleset", default="classic")
    ap.add_argument("--seeds", default="1,2,3")
    ap.add_argument("--turns", type=int, default=60)
    ap.add_argument("--players", type=int, default=6)
    ap.add_argument("--port", type=int, default=5801)
    ap.add_argument("--game-timeout", type=int, default=3600)
    ap.add_argument("--control", action="store_true",
                    help="compare two stock games without dropping the "
                    "wall-clock fields, and list every key that differs")
    ap.add_argument("--mixed", action="store_true")
    ap.add_argument("--keep", action="store_true",
                    help="keep the saves of matching pairs")
    args = ap.parse_args()
    args.server = os.path.abspath(args.server)
    args.data = os.path.abspath(args.data)

    failed = False
    for seed in [int(s) for s in args.seeds.split(",")]:
        base = os.path.join(args.workdir, "%s-s%d" % (args.ruleset, seed))
        if args.mixed:
            types = [STOCK if i % 2 == 0 else UNFIXED
                     for i in range(args.players)]
            d = base + "-mixed"
            errs = log_errors(play(args, d, seed, types))
            saves = saves_by_turn(d)
            fin = final_save(d)
            plrtypes = sorted(v for k, v in parse(os.path.join(d, fin)).items()
                              if re.match(r"player\d+\.ai_type$", k))
            print("%s seed %d mixed: last save T%d (%s), ai_types %s, "
                  "%d error lines" % (args.ruleset, seed, max(saves), fin,
                                      plrtypes, len(errs)))
            for e in errs[:20]:
                print("  " + e)
            failed |= bool(errs) or max(saves) < args.turns
            continue
        dropped = DROPPED
        if args.control:
            dropped = ()
            pair = ((base + "-" + STOCK, STOCK),
                    (base + "-" + STOCK + "2", STOCK))
        else:
            pair = ((base + "-" + STOCK, STOCK),
                    (base + "-" + UNFIXED, UNFIXED))
        for d, t in pair:
            errs = log_errors(play(args, d, seed, t))
            if errs:
                print("%s: %d error lines, first: %s" % (d, len(errs),
                                                         errs[0]))
        diff = compare(pair[0][0], pair[0][1], pair[1][0], pair[1][1],
                       args.turns, dropped, exhaustive=args.control)
        n = len(saves_by_turn(pair[0][0]))
        if diff is None:
            print("%s seed %d: identical, %d saves through T%d"
                  % (args.ruleset, seed, n, max(saves_by_turn(pair[0][0]))))
            if not args.keep:
                for d, _ in pair:
                    for f in os.listdir(d):
                        if f.endswith(".sav"):
                            os.unlink(os.path.join(d, f))
        else:
            print("%s seed %d: %s" % (args.ruleset, seed, diff))
            failed = True
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
