#!/usr/bin/env python3
"""W12H census: soft borders, corridor risk, exposure and ally alignment.

Reads the four lines this epic added to deep's metric stream --
deep-trunksoft, deep-trunkrisk, deep-trunkduty and deep-trunklink --
out of one or more run.sh result trees and prints four tables, one per
question the brief asks.

    softcensus.py DIR [DIR ...]        one column per DIR

Every DIR is an arm: pass them in the order you want the columns, e.g.
results/w12h-live/family results/w12h-live/soft results/w12h-live/risk.

Nothing here computes a score. This epic's bar is mechanism truth and
liveness; the corridor family's benchmark verdict is elsewhere and is
null-to-negative.
"""
import collections
import os
import re
import sys

OWN = ["free", "self", "ally", "peace", "cease", "war", "other"]


def games(root):
    """Yield (label, path-to-game.log) for every completed game under root."""
    for dirpath, dirnames, filenames in os.walk(root):
        if "game.log" not in filenames:
            continue
        if "DONE" not in filenames:
            continue
        # Label by arm/seed only. run.sh puts results under
        # <config>-<key12>/, and the key is a hash of config + binary +
        # TUNING -- so two arms one knob apart have DIFFERENT key
        # directories, and a label carrying the key would never join
        # across them.
        rel = os.path.relpath(dirpath, root).split(os.sep)
        yield os.path.join(*rel[-2:]), os.path.join(dirpath, "game.log")


def fields(rest):
    out = {}
    for m in re.finditer(r'(\w+)=("[^"]*"|\S+)', rest):
        out[m.group(1)] = m.group(2).strip('"')
    return out


def tup(s):
    v = [int(x) for x in s.split(":")]
    return v + [0] * (len(OWN) - len(v))


def chev(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


class Arm:
    def __init__(self, root):
        self.root = root
        self.name = os.path.basename(root.rstrip("/"))
        self.n_games = 0
        self.soft = {k: [0] * len(OWN)
                     for k in ("plan", "off", "pick", "work", "road")}
        self.soft_lines = 0
        self.risk = collections.Counter()
        self.risk_max = 0.0
        self.risk_lines = 0
        # deep-trunkduty is cumulative, so only the LAST line per
        # (game, player) is that game's total.
        self.duty_last = {}
        # (game, turn, player) -> list of (ally, from, to, tiles)
        self.links = collections.defaultdict(list)

    def read(self):
        for label, path in games(self.root):
            self.n_games += 1
            with open(path, "rb") as fh:
                for raw in fh:
                    if b"deep-trunk" not in raw:
                        continue
                    line = raw.decode("utf-8", "replace")
                    i = line.find("deep-trunk")
                    tag, _, rest = line[i:].partition(" ")
                    f = fields(rest)
                    if tag == "deep-trunksoft":
                        self.soft_lines += 1
                        for k in self.soft:
                            if k in f:
                                for j, v in enumerate(tup(f[k])):
                                    self.soft[k][j] += v
                    elif tag == "deep-trunkrisk":
                        self.risk_lines += 1
                        for k in ("steps", "dodge", "coast"):
                            self.risk[k] += int(f.get(k, 0))
                        self.risk["risk"] += float(f.get("risk", 0))
                        self.risk_max = max(self.risk_max,
                                            float(f.get("maxrisk", 0)))
                    elif tag == "deep-trunkduty":
                        self.duty_last[(label, f.get("player"))] = f
                    elif tag == "deep-trunklink":
                        tiles = []
                        if f.get("tiles"):
                            for xy in f["tiles"].split(";"):
                                x, y = xy.split(",")
                                tiles.append((int(x), int(y)))
                        self.links[(label, int(f["turn"]))].append(
                            (f.get("player"), int(f.get("ally", 0)),
                             tuple(int(v) for v in f["from"].split(",")),
                             tuple(int(v) for v in f["to"].split(",")),
                             tiles))


def table(arms, title, rows):
    w = max(len(r[0]) for r in rows) + 2
    print("\n== %s" % title)
    print(" " * w + "".join("%14s" % a.name for a in arms))
    for label, fn in rows:
        print(label.ljust(w) + "".join("%14s" % fn(a) for a in arms))


def alignment(arm):
    """Do two allied deep players plan the SAME tiles for a shared link?

    A shared link is two ally-flagged corridors, planned by different
    players in the same turn, that run between the same two cities --
    p's far endpoint is q's near endpoint and vice versa. That is the
    strictest reading and the only one that needs no knowledge the log
    does not carry; the near miss below is the looser one.

    Reported per (game, turn, pair): tile overlap, and the "parallel"
    count -- tiles one side plans that the other does not, but which sit
    adjacent to a tile the other does. Two roads one tile apart are the
    failure the meet-in-the-middle question is about; two roads that
    share no ground at all are a different (and worse) failure.
    """
    exact = near = 0
    ov_num = ov_den = 0
    par = 0
    lonely = 0
    seen_pairs = 0
    for key, entries in arm.links.items():
        ally = [e for e in entries if e[1] == 1]
        for i in range(len(ally)):
            for j in range(i + 1, len(ally)):
                p, q = ally[i], ally[j]
                if p[0] == q[0]:
                    continue
                d = chev(p[3], q[2]) + chev(p[2], q[3])
                if d > 4:
                    continue
                seen_pairs += 1
                if d == 0:
                    exact += 1
                else:
                    near += 1
                sp, sq = set(p[4]), set(q[4])
                if sp or sq:
                    ov_num += len(sp & sq)
                    ov_den += len(sp | sq)
                for t in (sp ^ sq):
                    other = sq if t in sp else sp
                    if any(chev(t, u) <= 1 for u in other):
                        par += 1
                    else:
                        lonely += 1
    return dict(pairs=seen_pairs, exact=exact, near=near,
                overlap=(ov_num / ov_den if ov_den else 0.0),
                parallel=par, disjoint=lonely)


def reroute(a, b):
    """First turn at which the two arms plan a DIFFERENT corridor.

    Two arms one key apart start from the same map and the same seed, so
    until something in the game state diverges they are the same game.
    The first turn whose plans differ is therefore a clean reroute: same
    cities, same terrain, same everything, one knob. After it the games
    are different games and a link diff means nothing, which is why only
    the first one is printed.
    """
    print("\n== reroute: first turn the two arms plan a different corridor")
    print("   %s  vs  %s" % (a.name, b.name))
    seeds = sorted({k[0] for k in a.links} & {k[0] for k in b.links})
    for seed in seeds:
        turns = sorted({k[1] for k in a.links if k[0] == seed}
                       | {k[1] for k in b.links if k[0] == seed})
        for t in turns:
            ka = {(e[0], e[2], e[3]): e[4] for e in a.links.get((seed, t), [])}
            kb = {(e[0], e[2], e[3]): e[4] for e in b.links.get((seed, t), [])}
            keys = sorted(set(ka) | set(kb))
            hit = [k for k in keys if ka.get(k) != kb.get(k)]
            if not hit:
                continue
            k = hit[0]
            print("  %s turn %d  player %s  %s -> %s" %
                  (seed, t, k[0], k[1], k[2]))
            for nm, side in ((a.name, ka), (b.name, kb)):
                tiles = side.get(k)
                print("    %-8s %s" % (nm, "MISSING" if tiles is None
                                       else ";".join("%d,%d" % xy
                                                     for xy in tiles)))
            if ka.get(k) and kb.get(k):
                only = [xy for xy in ka[k] if xy not in kb[k]]
                print("    differs on %d tile(s), first %s"
                      % (len(only), only[0] if only else "-"))
            break
        else:
            print("  %s: no divergence in %d turns of plans"
                  % (seed, len(turns)))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    if argv[1] == "--reroute":
        a, b = Arm(argv[2]), Arm(argv[3])
        a.read()
        b.read()
        reroute(a, b)
        return 0
    arms = []
    for root in argv[1:]:
        a = Arm(root)
        a.read()
        arms.append(a)

    print("W12H census over %d arm(s)" % len(arms))
    for a in arms:
        print("  %-10s %s  (%d games)" % (a.name, a.root, a.n_games))

    table(arms, "soft borders: corridor tiles by owner treaty state",
          [("games", lambda a: a.n_games),
           ("deep-trunksoft lines", lambda a: a.soft_lines)]
          + [("%s %s" % (k, OWN[j]), (lambda k, j: lambda a: a.soft[k][j])(k, j))
             for k in ("plan", "off", "pick", "work", "road")
             for j in range(len(OWN))])

    table(arms, "risk: corridor steps priced and rerouted",
          [("deep-trunkrisk lines", lambda a: a.risk_lines),
           ("steps priced", lambda a: a.risk["steps"]),
           ("dodge (rerouted)", lambda a: a.risk["dodge"]),
           ("dodge %", lambda a: "%.1f" % (100.0 * a.risk["dodge"]
                                           / a.risk["steps"])
            if a.risk["steps"] else "-"),
           ("risk walked", lambda a: "%.0f" % a.risk["risk"]),
           ("worst tile", lambda a: "%.1f" % a.risk_max),
           ("coastal steps", lambda a: a.risk["coast"])])

    def duty(a, what):
        tot = 0
        for f in a.duty_last.values():
            if what == "seen":
                tot += int(f.get("seen", 0))
            elif what == "coast":
                tot += int(f.get("coast", 0))
            else:
                tot += tup(f.get("lost", "0"))[OWN.index(what)]
        return tot

    table(arms, "exposure: workers lost while standing on a corridor tile",
          [("player-games with duty", lambda a: len(a.duty_last)),
           ("worker-turns on duty", lambda a: duty(a, "seen"))]
          + [("lost on %s" % o, (lambda o: lambda a: duty(a, o))(o))
             for o in OWN]
          + [("of those, coastal", lambda a: duty(a, "coast"))])

    aligns = [alignment(a) for a in arms]
    print("\n== meet in the middle: two allies planning one link")
    print("%-24s%s" % ("", "".join("%14s" % a.name for a in arms)))
    for k in ("pairs", "exact", "near", "parallel", "disjoint"):
        print("%-24s%s" % (k, "".join("%14d" % g[k] for g in aligns)))
    print("%-24s%s" % ("tile overlap",
                       "".join("%14.3f" % g["overlap"] for g in aligns)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
