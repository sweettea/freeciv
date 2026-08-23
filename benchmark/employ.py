#!/usr/bin/env python3
"""Read ai/deep's employment ledger out of retained game logs.

The war-gap decomposition (STATUS 2026-08-22) localised the tech50war
deficit's largest leg as EMPLOYMENT -- deep converts a slightly larger
army into 0.54x the kills -- and could not say why, there being no
counters between "units built" and "units killed".  ai/deep now writes
four cumulative instruments into game.log (deepemploy.c) and the server
writes a fifth (kill-dyad, gated on $FREECIV_KILL_DYAD); this reads all
five and prints the tables.

    ./employ.py DIR [DIR ...] [--turns 40,60,80,100] [--section NAME]

DIR is a tree of run-game.sh output directories, either

    DIR/<arm>/seed-N/{game.log,commands.serv,scorelog.txt}

or a single game directory.  Mixed arms (mixed_dc / mixed_cd) are the
interesting case: seat -> module comes from commands.serv, so every
counter is attributable to the module that produced it and seat bias
cancels when both orderings are present.

Sections: census, funnel, sortie, ops, depart, poach, wait, dyad.
"""

import argparse
import os
import re
import sys

import scorelog
from wargap import mean, sem, tstat

def kv(line):
    """The `key=value` pairs of one metric line, quotes stripped.

    The cause lists are one whitespace token of comma-separated pairs
    (`nohome=3,notarget=15,...`), so every token is split on commas
    before the `=` is looked for.  No metric line quotes a value that
    contains a space, so a plain split() is the whole parser.
    """
    out = {}
    for tok in line.split():
        for part in tok.split(","):
            if "=" not in part:
                continue
            k, v = part.split("=", 1)
            if "=" in v:
                # A cause list introduced by its own key, as
                # `dep=dead=0/0,lost=0/0,...`: the first comma group
                # carries both the introducer and the first pair, and
                # the pair is the part worth keeping. No metric line
                # has a value that legitimately contains "=".
                k, v = v.split("=", 1)
            out[k] = v.strip('"')
    return out


def num(d, k, default=0):
    """Value of `k`, taking the count side of a `count/mean` pair."""
    v = d.get(k)
    if v is None:
        return default
    v = v.split("/")[0]
    try:
        return int(v)
    except ValueError:
        try:
            return float(v)
        except ValueError:
            return default


def seat_roles(cmdpath):
    """{player-name: module} from the commands.serv `create Pn <module>`."""
    roles = {}
    for line in open(cmdpath, errors="replace"):
        m = re.match(r"create\s+(\S+)\s+(\w+)", line.strip())
        if m:
            roles[m.group(1)] = m.group(2)
    return roles


class Game:
    """One game's employment ledger."""

    def __init__(self, gdir):
        self.dir = gdir
        self.roles = seat_roles(os.path.join(gdir, "commands.serv"))
        self.census = []          # dicts, one per player-turn
        self.funnel = {}          # player -> last cumulative dict
        self.sortie = {}          # player -> last cumulative dict
        self.ops = []             # dicts, one per resolved operation
        self.poach = []           # dicts, one per adjudicated poach
        self.dyad = []            # (turn, killerai, victimai)
        for line in open(os.path.join(gdir, "game.log"), errors="replace"):
            if "deep-employ " in line:
                self.census.append(kv(line))
            elif "deep-employform " in line:
                d = kv(line)
                self.funnel[d.get("player")] = d
            elif "deep-employsortie " in line:
                d = kv(line)
                self.sortie[d.get("player")] = d
            elif "deep-employop " in line:
                self.ops.append(kv(line))
            elif "deep-employpoach " in line:
                self.poach.append(kv(line))
            elif "kill-dyad " in line:
                d = kv(line)
                self.dyad.append((num(d, "turn"), d.get("killerai", "none"),
                                  d.get("victimai", "none")))

    def module(self, player):
        return self.roles.get(player, "?")


def find_games(roots):
    out = []
    for root in roots:
        for dirpath, dirnames, filenames in os.walk(root):
            if "game.log" in filenames and "commands.serv" in filenames:
                out.append(dirpath)
                dirnames[:] = []
    return sorted(out)


# --------------------------------------------------------------- census

CENSUS_COLS = ["mil", "att", "attidle", "muster", "march", "strike", "raid",
               "lift", "escort", "ward", "garrison", "attack", "hunt",
               "tasked", "moved", "fort", "idle", "idlecity", "ops", "wars"]


def section_census(games, turns, module):
    print("# 1. DISPOSITION CENSUS -- mean per %s seat per turn (n=%d games)"
          % (module, len(games)))
    print("  %5s %6s" % ("turn", "seats")
          + "".join("%9s" % c for c in CENSUS_COLS))
    for t in turns:
        rows = []
        for g in games:
            for c in g.census:
                if num(c, "turn") == t and g.module(c.get("player")) == module:
                    rows.append(c)
        if not rows:
            continue
        print("  %5d %6d" % (t, len(rows))
              + "".join("%9.2f" % mean([num(r, c) for r in rows])
                        for c in CENSUS_COLS))
    print()


# --------------------------------------------------------------- funnel

VERDICTS = ["nowar", "noslot", "nohome", "notarget", "nostaging", "poolempty",
            "musteroff", "worth", "muster", "formed"]


def section_funnel_print(games, module):
    rows = []
    for g in games:
        for player, d in g.funnel.items():
            if g.module(player) != module:
                continue
            causes = {}
            for k in VERDICTS:
                causes[k] = num(d, k)
            rows.append((d, causes))
    if not rows:
        print("# 2. ATTACK-OPPORTUNITY FUNNEL: no data\n")
        return
    n = len(rows)
    print("# 2. ATTACK-OPPORTUNITY FUNNEL -- per %s seat-game (n=%d)"
          % (module, n))
    for label, key in (("formation attempts", "attempts"),
                       ("  reached pricing", "priced"),
                       ("    odds exactly 0", "oddszero"),
                       ("    odds short of bar", "oddsshort"),
                       ("    odds cleared bar", "oddsclear"),
                       ("  free blades priced", "pool"),
                       ("  draft fired", "draftfired"),
                       ("  blades drafted", "drafted")):
        v = [num(d, key) for d, _ in rows]
        print("  %-24s %8.1f  sem %6.2f" % (label, mean(v), sem(v)))
    tot = mean([num(d, "attempts") for d, _ in rows])
    print("  mean best-prefix odds    %8.3f   (bar %.3f)"
          % (mean([num(d, "moddsx1000") for d, _ in rows]) / 1000.0,
             mean([num(d, "mbarx1000") for d, _ in rows]) / 1000.0))
    print("  verdicts, ranked:")
    ranked = sorted(VERDICTS, key=lambda k: -mean([c[k] for _, c in rows]))
    for k in ranked:
        v = [c[k] for _, c in rows]
        if mean(v) == 0:
            continue
        print("    %-12s %8.1f  %5.1f%%  sem %5.2f"
              % (k, mean(v), 100.0 * mean(v) / tot if tot else 0, sem(v)))
    print()


# --------------------------------------------------------------- sortie

SORTIE = ["noblade", "novictim", "floor", "price", "home", "go"]
BLADE = ["bmoves", "bdone", "bclaim"]


def section_sortie(games, module):
    rows = []
    for g in games:
        for player, d in g.sortie.items():
            if g.module(player) != module:
                continue
            rows.append(d)
    if not rows:
        print("# 3. SORTIE FUNNEL: no data\n")
        return
    print("# 3. SORTIE FUNNEL -- per %s seat-game (n=%d)" % (module, len(rows)))
    tot = mean([num(d, "passes") for d in rows])
    print("  city-passes              %8.1f" % tot)
    for k in SORTIE:
        v = [num(d, k) for d in rows]
        if mean(v) == 0 and k != "go":
            continue
        print("    %-12s %8.1f  %5.1f%%  sem %5.2f"
              % (k, mean(v), 100.0 * mean(v) / tot if tot else 0, sem(v)))
    print("  blade rejections (units, not passes):")
    for k in BLADE:
        v = [num(d, k) for d in rows]
        print("    %-12s %8.1f  sem %5.2f" % (k[1:], mean(v), sem(v)))
    print()


# ------------------------------------------------------------------ ops


def section_ops(games, module):
    ops = [o for g in games for o in g.ops if g.module(o.get("player")) == module]
    if not ops:
        print("# 4. OP LIFECYCLE: no operations\n")
        return
    ngames = len(games)
    launched = [o for o in ops if num(o, "launched") > 0]
    print("# 4. OP LIFECYCLE -- %d assault operations over %d games"
          % (len(ops), ngames))
    print("  operations per game       %8.2f" % (len(ops) / ngames))
    print("  launched                  %8d  (%.1f%%)"
          % (len(launched), 100.0 * len(launched) / len(ops)))
    print("  members at formation      %8.2f"
          % mean([num(o, "members0") for o in ops]))
    print("  peak members              %8.2f"
          % mean([num(o, "peak") for o in ops]))
    print("  members at dissolve       %8.2f"
          % mean([num(o, "final") for o in ops]))
    print("  turns gathering           %8.2f"
          % mean([num(o, "waited") for o in ops]))
    if launched:
        print("  turns from march to end   %8.2f"
              % mean([num(o, "held") for o in launched]))
    print("  kills booked at the walls %8.2f /op  (%.1f total)"
          % (mean([num(o, "kills") for o in ops]),
             sum(num(o, "kills") for o in ops) / ngames))
    print("  losses booked at the walls%8.2f /op  (%.1f total)"
          % (mean([num(o, "losses") for o in ops]),
             sum(num(o, "losses") for o in ops) / ngames))
    print("  dissolved with final=0    %8d  (%.1f%%)"
          % (sum(1 for o in ops if num(o, "final") == 0),
             100.0 * sum(1 for o in ops if num(o, "final") == 0) / len(ops)))
    why = {}
    for o in ops:
        why[o.get("why", "?")] = why.get(o.get("why", "?"), 0) + 1
    print("  end causes, ranked:")
    for k in sorted(why, key=lambda k: -why[k]):
        print("    %-16s %6d  %5.1f%%" % (k, why[k], 100.0 * why[k] / len(ops)))
    print()


# --------------------------------------------------------------- departs

DEPART = ["dead", "lost", "defence", "poach", "garrison"]


def section_depart(games, module):
    """Where a wave's members go, by the path they left through.

    The `dep=` field of deep-employop carries `cause=life/firstturn`
    per cause, so one pass over the op lines gives both columns.
    """
    ops = [o for g in games for o in g.ops
           if g.module(o.get("player")) == module]
    if not ops or "dead" not in ops[0]:
        print("# 7. MEMBER DEPARTURES: no dep= field (older binary)\n")
        return
    life = {k: 0 for k in DEPART}
    first = {k: 0 for k in DEPART}
    for o in ops:
        for k in DEPART:
            v = o.get(k, "0/0").split("/")
            life[k] += int(v[0])
            first[k] += int(v[1]) if len(v) > 1 else 0
    tot = sum(life[k] for k in DEPART if k != "garrison")
    ng = len(games)
    print("# 7. MEMBER DEPARTURES -- %d assault operations over %d games"
          % (len(ops), ng))
    print("  %-10s %10s %8s %12s %10s"
          % ("cause", "lifetime", "share", "formation+1", "per game"))
    for k in sorted(DEPART, key=lambda k: -life[k]):
        share = (100.0 * life[k] / tot) if tot and k != "garrison" else \
            float("nan")
        print("  %-10s %10d %7.1f%% %12d %10.2f"
              % (k, life[k], share, first[k], life[k] / ng))
    print("  %-10s %10d %8s %12d %10.2f"
          % ("(total)", sum(life.values()), "", sum(first.values()),
             sum(life.values()) / ng))
    print("  losses excluding the garrison conversion: %d, of which %d"
          " (%.1f%%) on the turn after formation"
          % (tot, sum(first[k] for k in DEPART if k != "garrison"),
             100.0 * sum(first[k] for k in DEPART if k != "garrison") / tot
             if tot else 0))
    print()


# ----------------------------------------------------------------- poach


def fnum(d, k, default=0.0):
    """Float value of `k`."""
    try:
        return float(d.get(k, default))
    except ValueError:
        return default


def pct(n, d):
    return "%6.1f%%" % (100.0 * n / d) if d else "     --"


def poach_rows(games, module):
    return [p for g in games for p in g.poach
            if g.module(p.get("player")) == module]


def poach_table(rows, label):
    """One slice of the poach ledger, both sides of the price and the
    window's verdict on each."""
    n = len(rows)
    if not n:
        print("  %-22s        no poaches" % label)
        return
    # The claim, adjudicated. `fell` is the city changing hands, adjmax
    # the high-water enemy count ON an adjacent tile at any sample in
    # the window, adj0 the same at claim time. "Never came" is the
    # slice where none of the three ever happened: the claimed wave was
    # a forecast that the window did not confirm.
    fell = sum(1 for p in rows if num(p, "fell"))
    walls = sum(1 for p in rows if num(p, "adjmax") > 0 or num(p, "adj0") > 0)
    never = sum(1 for p in rows
                if not num(p, "fell") and num(p, "adjmax") == 0
                and num(p, "adj0") == 0)
    # The blade.
    kept0 = sum(1 for p in rows if num(p, "kept", -1) == 0)
    arrived = sum(1 for p in rows if num(p, "home") > 0)
    hurt = sum(1 for p in rows if num(p, "dmg") > 0)
    gone = sum(1 for p in rows if num(p, "gone"))
    reclaim = sum(1 for p in rows if num(p, "reclaim"))
    idle = sum(1 for p in rows
               if num(p, "home") > 0 and num(p, "adjmax") == 0
               and num(p, "dmg") == 0 and not num(p, "gone"))
    print("  %-22s %8d" % (label, n))
    print("    city fell in window   %8d %s" % (fell, pct(fell, n)))
    print("    enemy at the walls    %8d %s" % (walls, pct(walls, n)))
    print("    danger never came     %8d %s" % (never, pct(never, n)))
    print("    force never deployed  %8d %s" % (kept0, pct(kept0, n)))
    print("    blade reached city    %8d %s" % (arrived, pct(arrived, n)))
    print("    blade took damage     %8d %s" % (hurt, pct(hurt, n)))
    print("    blade gone in window  %8d %s" % (gone, pct(gone, n)))
    print("    re-claimed by an op   %8d %s" % (reclaim, pct(reclaim, n)))
    print("    IDLED UN-ATTACKED     %8d %s" % (idle, pct(idle, n)))
    print("    mean p / relief       %8.3f %7.3f"
          % (mean([fnum(p, "p") for p in rows]),
             mean([fnum(p, "relief") for p in rows])))
    print("    mean wave / pool      %8.2f %7.2f"
          % (mean([num(p, "wave") for p in rows]),
             mean([num(p, "pool") for p in rows])))
    print("    mean op age / members %8.2f %7.2f"
          % (mean([num(p, "age") for p in rows]),
             mean([num(p, "members") for p in rows])))


RATIO_BUCKETS = [1.0, 2.0, 5.0, 10.0, 100.0, 1000.0]


def poach_ratio(rows):
    """Defence bid over offence bid, at claim time.

    Both sides are read off the line as the poach itself computed them:
    `risk` = city worth * defence_worth_mult * (1 - p_survival), and
    `take` = conquest worth * odds at the wave's own eta. The poach
    fires only when risk > take, so the ratio is bounded below by 1 by
    construction and the question the distribution answers is HOW
    lopsided, not which way.
    """
    zero, fin = 0, []
    for p in rows:
        take = fnum(p, "take")
        if take <= 0.0:
            zero += 1
        else:
            fin.append(fnum(p, "risk") / take)
    n = len(rows)
    print("  defence-worth / offence-worth at claim time (n=%d)" % n)
    print("    offence bid exactly 0 %8d %s" % (zero, pct(zero, n)))
    if not fin:
        return
    fin.sort()
    lo = 0.0
    for hi in RATIO_BUCKETS + [float("inf")]:
        k = sum(1 for r in fin if lo <= r < hi)
        print("    ratio %6s - %-8s%8d %s"
              % ("%.0f" % lo if lo else "0", "%.0f" % hi if hi < 1e9
                 else "inf", k, pct(k, n)))
        lo = hi
    print("    median ratio          %8.1f   mean %.1f"
          % (fin[len(fin) // 2], mean(fin)))
    print("    median risk / take    %8.1f / %.1f"
          % (sorted(fnum(p, "risk") for p in rows)[n // 2],
             sorted(fnum(p, "take") for p in rows)[n // 2]))


def section_poach(games, module):
    rows = poach_rows(games, module)
    ng = len(games)
    if not rows:
        print("# 8. POACH PRICE: no deep-employpoach lines (older binary)\n")
        return
    print("# 8. POACH PRICE -- %d adjudicated poaches over %d games (%.1f/game)"
          % (len(rows), ng, len(rows) / ng))
    print("  window = survival_horizon turns after the claim; poaches"
          " still open when\n  the game ends never adjudicate and are not"
          " here.")
    poach_table(rows, "all poaches")
    print()
    poach_table([p for p in rows if num(p, "muster")], "from a muster")
    print()
    poach_table([p for p in rows if not num(p, "muster")], "from a formed wave")
    print()
    poach_ratio(rows)
    print()
    # The adjudication crossed with what the claim bought.
    #
    # `kept` is sharper than it looks. The poach is called only after
    # the whole free pool has failed to lift the city to
    # survival_target, and the blade is appended LAST -- so a force
    # that clears the bar clears it BECAUSE of the poached blade
    # (kept=1: the blade was decisive), and a force that still falls
    # short is written off in the same pass without deploying anybody
    # (kept=0: the wave lost its blade and the city got nothing).
    print("  THE POACH-PRICE TABLE")
    print("    %-34s %8s %8s %10s" % ("", "n", "share", "take=0"))
    cats = [
        ("paid and needed (kept, danger came)",
         lambda p: num(p, "kept", -1) == 1 and danger(p)),
        ("paid and unneeded (kept, no danger)",
         lambda p: num(p, "kept", -1) == 1 and not danger(p)),
        ("paid for nothing (city written off)",
         lambda p: num(p, "kept", -1) == 0),
        ("unsettled (neither exit ran)",
         lambda p: num(p, "kept", -1) < 0),
    ]
    for lab, f in cats:
        sel = [p for p in rows if f(p)]
        z = sum(1 for p in sel if fnum(p, "take") <= 0.0)
        print("    %-34s %8d %s %s"
              % (lab, len(sel), pct(len(sel), len(rows)), pct(z, len(sel))))
    print()


def danger(p):
    """Did the claimed danger materialise inside the window?

    The union of the three things a siege leaves behind: the enemy
    standing on a tile adjacent to the city at the claim or at any
    sample, and the city changing hands. Adjacency is sampled once a
    turn at deep's own phase, so an attacker that arrives, strikes and
    dies inside another player's phase leaves no adjacency trace -- the
    blade's own damage and the city falling are the two event-true
    signals, and both are in the union.
    """
    return (num(p, "adjmax") > 0 or num(p, "adj0") > 0 or num(p, "fell")
            or num(p, "dmg") > 0)


# ------------------------------------------------------------------ wait

WAIT_BUCKET = {
    "notarget": "no target worth hitting",
    "nostaging": "no target worth hitting",
    "nohome": "no target worth hitting",
    "poolempty": "target priced, no force",
    "worth": "target priced but vetoed",
    "musteroff": "target priced but vetoed",
    "noslot": "all op slots occupied",
    "muster": "waiting for a wave",
    "formed": "a wave formed",
    "nowar": "no formation attempt",
}


def section_wait(games, module, tmin=0, tmax=10**9):
    rows = [c for g in games for c in g.census
            if g.module(c.get("player")) == module
            and tmin <= num(c, "turn") <= tmax]
    if not rows:
        print("# 5. WAIT LEDGER: no data\n")
        return
    agg = {}
    for c in rows:
        b = WAIT_BUCKET.get(c.get("why", "nowar"), c.get("why"))
        a = agg.setdefault(b, {"turns": 0, "idle": 0, "attidle": 0,
                               "mil": 0, "musters": 0})
        a["turns"] += 1
        a["idle"] += num(c, "idle") + num(c, "fort")
        a["attidle"] += num(c, "attidle")
        a["mil"] += num(c, "mil")
        a["musters"] += num(c, "musters")
    tot = sum(a["turns"] for a in agg.values())
    print("# 5. WAIT LEDGER -- %s seat-turns T%d-T%d (n=%d)"
          % (module, tmin, tmax if tmax < 10**9 else
             max(num(c, "turn") for c in rows), tot))
    print("  %-28s %8s %7s %10s %10s %8s"
          % ("gate that held the army", "turns", "share", "idle/turn",
             "attidle/t", "mil/turn"))
    for b in sorted(agg, key=lambda b: -agg[b]["attidle"]):
        a = agg[b]
        print("  %-28s %8d %6.1f%% %10.2f %10.2f %8.2f"
              % (b, a["turns"], 100.0 * a["turns"] / tot,
                 a["idle"] / a["turns"], a["attidle"] / a["turns"],
                 a["mil"] / a["turns"]))
    print()


# ------------------------------------------------------------------ dyad


def section_dyad(games, turns):
    pairs = {}
    for g in games:
        for turn, ka, va in g.dyad:
            pairs.setdefault((ka, va), []).append(turn)
    if not pairs:
        print("# 6. KILL DYADS: no kill-dyad lines "
              "(was FREECIV_KILL_DYAD set?)\n")
        return
    ngames = len(games)
    print("# 6. KILL DYADS -- kills per game by (killer module, "
          "victim module), n=%d games" % ngames)
    print("  %-22s" % "killer -> victim"
          + "".join("%10s" % ("T%d" % t) for t in turns) + "%10s" % "total")
    order = sorted(pairs, key=lambda p: -len(pairs[p]))
    for p in order:
        ts = pairs[p]
        print("  %-22s" % ("%s -> %s" % p)
              + "".join("%10.2f" % (sum(1 for x in ts if x <= t) / ngames)
                        for t in turns)
              + "%10.2f" % (len(ts) / ngames))
    print()
    mods = sorted(({k for k, _ in pairs} | {v for _, v in pairs})
                  - {"none"})
    print("  per-module kill:loss from the dyad table (excludes 'none'"
          " killers on the kill side, includes them on the loss side)")
    print("  %-10s" % "module"
          + "".join("%10s" % ("T%d" % t) for t in turns))
    for m in mods:
        if m == "none":
            continue
        row = []
        for t in turns:
            k = sum(1 for (ka, va), ts in pairs.items() if ka == m
                    for x in ts if x <= t)
            l = sum(1 for (ka, va), ts in pairs.items() if va == m
                    for x in ts if x <= t)
            row.append(k / l if l else float("nan"))
        print("  %-10s" % m + "".join("%10.3f" % v for v in row))
    print()
    print("  and the same kills split by whether the victim was the SAME"
          " module")
    print("  %-10s %10s %10s %10s" % ("killer", "vs same", "vs other",
                                      "same share"))
    for m in mods:
        if m == "none":
            continue
        same = sum(len(ts) for (ka, va), ts in pairs.items()
                   if ka == m and va == m)
        other = sum(len(ts) for (ka, va), ts in pairs.items()
                    if ka == m and va != m)
        print("  %-10s %10.2f %10.2f %10.1f%%"
              % (m, same / ngames, other / ngames,
                 100.0 * same / (same + other) if same + other else 0))
    print()


# ------------------------------------------------------------------ main


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+")
    ap.add_argument("--turns", default="40,60,80,100,200")
    ap.add_argument("--module", default="deep")
    ap.add_argument("--section", default="all")
    ap.add_argument("--windows", default="",
                    help="comma-separated turn bounds, e.g. 60,100 splits "
                         "the wait ledger into T0-60, T61-100, T101-end")
    args = ap.parse_args()

    turns = [int(t) for t in args.turns.split(",")]
    gdirs = find_games(args.dirs)
    if not gdirs:
        sys.exit("no game directories found")
    games = [Game(d) for d in gdirs]
    print("# employ.py: %d games under %s" % (len(games),
                                              ", ".join(args.dirs)))
    print()
    want = args.section
    if want in ("all", "census"):
        section_census(games, turns, args.module)
    if want in ("all", "funnel"):
        section_funnel_print(games, args.module)
    if want in ("all", "sortie"):
        section_sortie(games, args.module)
    if want in ("all", "ops"):
        section_ops(games, args.module)
    if want in ("all", "depart"):
        section_depart(games, args.module)
    if want in ("all", "poach"):
        section_poach(games, args.module)
    if want in ("all", "wait"):
        section_wait(games, args.module)
        if args.windows:
            bounds = [int(b) for b in args.windows.split(",")]
            lo = 0
            for hi in bounds + [10 ** 9]:
                section_wait(games, args.module, lo, hi)
                lo = hi + 1
    if want in ("all", "dyad"):
        section_dyad(games, turns)


if __name__ == "__main__":
    main()
