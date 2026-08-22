#!/usr/bin/env python3
"""Reader for freeciv SCORELOG2 files (see doc/README.scorelog).

The format is line-oriented; the only lines we care about are

    tag <tag-id> <name>
    addplayer <turn> <player-id> <name>
    delplayer <turn> <player-id>
    data <turn> <tag-id> <player-id> <value>

`delplayer` is what tells us a seat died: the player was active *through*
the given turn, so its last data point is at that turn.

Nothing here is specific to a venue or an arm -- wargap.py builds on it.
"""

import os
import re


class Scorelog:
    """One game's score stream, indexed by (tag name, player id, turn)."""

    def __init__(self, path):
        self.path = path
        self.tags = {}            # tag id -> name
        self.tag_id = {}          # name -> tag id
        self.players = {}         # pid -> name
        self.added = {}           # pid -> turn added
        self.deleted = {}         # pid -> turn deleted (last active turn)
        self.turns = set()
        # series[tagname][pid][turn] = value
        self.series = {}

        with open(path, "r", errors="replace") as fh:
            for line in fh:
                if not line or line[0] == "#":
                    continue
                parts = line.split()
                if not parts:
                    continue
                cmd = parts[0]
                if cmd == "tag" and len(parts) >= 3:
                    tid = int(parts[1])
                    self.tags[tid] = parts[2]
                    self.tag_id[parts[2]] = tid
                elif cmd == "addplayer" and len(parts) >= 4:
                    turn, pid = int(parts[1]), int(parts[2])
                    self.players[pid] = " ".join(parts[3:])
                    self.added[pid] = turn
                elif cmd == "delplayer" and len(parts) >= 3:
                    turn, pid = int(parts[1]), int(parts[2])
                    self.deleted[pid] = turn
                elif cmd == "data" and len(parts) >= 5:
                    turn, tid, pid = int(parts[1]), int(parts[2]), int(parts[3])
                    try:
                        val = int(parts[4])
                    except ValueError:
                        val = int(float(parts[4]))
                    name = self.tags.get(tid)
                    if name is None:
                        continue
                    self.turns.add(turn)
                    self.series.setdefault(name, {}).setdefault(pid, {})[turn] = val

    # ---- accessors -------------------------------------------------------

    @property
    def last_turn(self):
        return max(self.turns) if self.turns else 0

    def value(self, tag, pid, turn):
        """Value of `tag` for `pid` at `turn`, or None if not logged."""
        return self.series.get(tag, {}).get(pid, {}).get(turn)

    def alive_at(self, pid, turn):
        if self.added.get(pid, 0) > turn:
            return False
        if pid in self.deleted and self.deleted[pid] < turn:
            return False
        return True

    def total(self, tag, turn):
        """Sum of `tag` over every seat that logged a value at `turn`."""
        per = self.series.get(tag, {})
        vals = [d[turn] for d in per.values() if turn in d]
        return sum(vals) if vals else None

    def last_value(self, tag, pid, turn):
        """Value of `tag` for `pid` at `turn`, or at its last logged turn
        before `turn` if the seat is already dead.

        This is what tune.py's `final_score` sums at the end of a game: a
        dead seat keeps contributing the score it had when it died.  Use it
        when reproducing the benchmark objective; use `value` when asking
        what the living empires are worth.
        """
        d = self.series.get(tag, {}).get(pid, {})
        if not d:
            return None
        if turn in d:
            return d[turn]
        past = [t for t in d if t <= turn]
        return d[max(past)] if past else None

    def frozen_total(self, tag, turn):
        """tune.py's objective at `turn`: dead seats frozen at death."""
        out = 0
        seen = False
        for pid in self.series.get(tag, {}):
            v = self.last_value(tag, pid, turn)
            if v is not None:
                out += v
                seen = True
        return out if seen else None

    def seat_values(self, tag, turn):
        """{pid: value} for every seat with a value at `turn`."""
        per = self.series.get(tag, {})
        return {pid: d[turn] for pid, d in per.items() if turn in d}

    def survivors(self, turn):
        """Seats still logging data at `turn`."""
        return sorted(self.seat_values("score", turn).keys())


# Score identity, from server/score.c get_civ_score():
#   citizens + 2*techs + 5*wonders + spaceship + built/10 + killed/3
#     + culture/50
# `citizens` has no tag of its own (happypop+contentpop+unhappypop+
# specialists misses *angry* citizens), so it is recovered as the residual
# and reported as one component.  The named parts are exact.
SCORE_PARTS = (
    ("techs", lambda r: 2 * (r("techs") or 0)),
    ("wonders", lambda r: 5 * (r("wonders") or 0)),
    ("unitsbuilt", lambda r: (r("unitsbuilt") or 0) // 10),
    ("unitskilled", lambda r: (r("unitskilled") or 0) // 3),
    ("culture", lambda r: (r("culture") or 0) // 50),
)


def score_components(game, pid, turn, frozen=False):
    """Exact per-seat split of the `score` tag into get_civ_score() terms.

    Returns None when the seat has no score at that turn (dead or not yet
    added), unless `frozen`, which reads each seat at its last logged turn
    <= `turn` -- the convention tune.py's objective uses.  The `citizens`
    entry is the residual, which also absorbs the spaceship term (always 0
    before a ship arrives).
    """
    read = ((lambda tag: game.last_value(tag, pid, turn)) if frozen
            else (lambda tag: game.value(tag, pid, turn)))
    total = read("score")
    if total is None:
        return None
    out = {}
    named = 0
    for name, fn in SCORE_PARTS:
        v = fn(read)
        out[name] = v
        named += v
    out["citizens"] = total - named
    out["score"] = total
    return out


def find_games(root, seeds=None):
    """{seed: dir} for every seed-N directory under `root` holding a scorelog."""
    out = {}
    if not os.path.isdir(root):
        return out
    for name in sorted(os.listdir(root)):
        m = re.fullmatch(r"seed-(\d+)", name)
        if not m:
            continue
        seed = int(m.group(1))
        if seeds is not None and seed not in seeds:
            continue
        d = os.path.join(root, name)
        if os.path.exists(os.path.join(d, "scorelog.txt")):
            out[seed] = d
    return out


def load_arm(root, seeds=None):
    """{seed: Scorelog} for an arm directory (accepts a cfg0/ level)."""
    games = find_games(root, seeds)
    if not games:
        cfg = os.path.join(root, "cfg0")
        games = find_games(cfg, seeds)
    return {s: Scorelog(os.path.join(d, "scorelog.txt")) for s, d in games.items()}
