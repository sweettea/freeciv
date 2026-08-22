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
    dsum = 0
    allylinks = 0
    # The SAME-LINK subset: both sides picked the same two cities, so a
    # tile disagreement is a routing disagreement and nothing else. The
    # rest of the pairs disagree about WHICH link to build, which is a
    # different failure and is reported by `endpoint distance`.
    ex_num = ex_den = ex_par = ex_sym = 0
    for key, entries in arm.links.items():
        ally = [e for e in entries if e[1] == 1]
        allylinks += len(ally)
        for i in range(len(ally)):
            for j in range(i + 1, len(ally)):
                p, q = ally[i], ally[j]
                if p[0] == q[0]:
                    continue
                # p aims at a city of q and q at a city of p: p's far
                # endpoint should BE q's near endpoint, and vice versa.
                # Slack 12 because the two need not have picked the same
                # city pair at all -- each side takes its own nearest --
                # and a pair that is not the same link is exactly what
                # the mean distance below reports.
                d = chev(p[3], q[2]) + chev(p[2], q[3])
                if d > 12:
                    continue
                seen_pairs += 1
                dsum += d
                if d == 0:
                    exact += 1
                else:
                    near += 1
                sp, sq = set(p[4]), set(q[4])
                if sp or sq:
                    ov_num += len(sp & sq)
                    ov_den += len(sp | sq)
                    if d == 0:
                        ex_num += len(sp & sq)
                        ex_den += len(sp | sq)
                        ex_par += sum(
                            1 for t in (sp ^ sq)
                            if any(chev(t, u) <= 1
                                   for u in (sq if t in sp else sp)))
                        ex_sym += len(sp ^ sq)
                for t in (sp ^ sq):
                    other = sq if t in sp else sp
                    if any(chev(t, u) <= 1 for u in other):
                        par += 1
                    else:
                        lonely += 1
    return dict(exact_overlap=(ex_num / ex_den if ex_den else 0.0),
                exact_parallel=ex_par, exact_sym=ex_sym,
                allylinks=allylinks, pairs=seen_pairs, exact=exact,
                near=near, meand=(dsum / seen_pairs if seen_pairs else 0.0),
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


def read_save(path):
    if path.endswith(".zst"):
        import subprocess
        return subprocess.run(["zstd", "-dc", path], check=True,
                              stdout=subprocess.PIPE, text=True,
                              errors="replace").stdout
    return open(path, errors="replace").read()


def savegame_roads(path):
    """(roads, owner, players) out of a final savegame, in MAP coords.

    A savegame row is in NATIVE coordinates -- savegame3.c calls
    index_to_native_pos() before writing every position -- while
    TILE_XY, and so every tile= this epic logs, is in MAP coordinates.
    On an isometric topology the two differ and comparing them directly
    puts a fifth of the tiles off the map (the trap the W12 funnel
    session recorded). NATIVE_TO_MAP_POS (common/map.h:170) is the
    conversion, copied here from pickfunnel.py's own copy of it.
    """
    save = read_save(path)
    m = re.search(r"^extras_vector=(.*)$", save, re.M)
    names = [x.strip('"') for x in m.group(1).split(",")]
    road_idx = [i for i, n in enumerate(names) if n in ("Road", "Railroad")]
    iso = re.search(r'^"topology","[^"]*ISO[^"]*"', save, re.M) is not None
    width = max(len(r.group(2))
                for r in re.finditer(r'^t(\d{4})="([^"]*)"$', save, re.M))

    def to_map(nx, ny):
        if not iso:
            return nx, ny
        mx = (ny + (ny & 1)) // 2 + nx
        return mx, ny - mx + width

    roads = set()
    for row in re.finditer(r'^e(\d{2})_(\d{4})="([^"]*)"$', save, re.M):
        j, y, bits = int(row.group(1)), int(row.group(2)), row.group(3)
        for idx in road_idx:
            if idx // 4 != j:
                continue
            bit = 1 << (idx % 4)
            for x, ch in enumerate(bits):
                if int(ch, 16) & bit:
                    roads.add(to_map(x, y))

    owner = {}
    for row in re.finditer(r'^owner(\d{4})="([^"]*)"$', save, re.M):
        y = int(row.group(1))
        for x, cell in enumerate(row.group(2).split(",")):
            owner[to_map(x, y)] = None if cell == "-" else int(cell)

    # Per player: name, module, and the diplstate table. Its rows are
    # indexed by player number and include the player's own row, so no
    # off-by-one correction is wanted.
    players = {}
    for sec in re.finditer(r"^\[player(\d+)\]$(.*?)(?=^\[|\Z)", save,
                           re.M | re.S):
        n = int(sec.group(1))
        body = sec.group(2)
        nm = re.search(r'^name="([^"]*)"', body, re.M)
        ai = re.search(r'^ai_type="?([a-z]*)"?', body, re.M)
        ds = re.search(r"^diplstate=\{[^\n]*\n(.*?)^\}", body, re.M | re.S)
        rows = []
        if ds:
            for line in ds.group(1).strip().split("\n"):
                parts = [p.strip().strip('"') for p in line.split(",")]
                rows.append((parts[0], parts[7] == "TRUE"))
        players[n] = dict(name=nm.group(1) if nm else str(n),
                          ai=ai.group(1) if ai else "?", dipl=rows)
    return roads, owner, players


PICK = re.compile(r'deep-trunkpick .*?player="([^"]*)".*?'
                  r"tile=\((\d+),(\d+)\)")


def roads_report(root):
    """What is ON THE GROUND at game end under every corridor pick.

    The counters say what deep planned and assigned; this says what the
    world looks like afterwards, and it is the only reading that can
    call a road on a peace partner's tile a fact rather than an
    intention. Treaty state is taken from the savegame's own diplstate
    table at game end, so a state that changed mid-game is reported as
    it finished -- stated because it is a real limitation, not hidden.
    """
    print("== roads on foreign soil, from the final savegame")
    tot = collections.Counter()
    for label, log in games(root):
        d = os.path.dirname(log)
        saves = [os.path.join(d, "saves", f)
                 for f in sorted(os.listdir(os.path.join(d, "saves")))
                 if "final" in f]
        if not saves:
            continue
        roads, owner, players = savegame_roads(saves[0])
        by_name = {p["name"]: n for n, p in players.items()}
        picks = collections.defaultdict(set)
        for line in open(log, errors="replace"):
            m = PICK.search(line)
            if m:
                picks[m.group(1)].add((int(m.group(2)), int(m.group(3))))
        for pname, tiles in sorted(picks.items()):
            pn = by_name.get(pname)
            if pn is None:
                continue
            dipl = players[pn]["dipl"]
            per = collections.Counter()
            examples = {}
            for t in tiles:
                o = owner.get(t)
                if o is None:
                    st = "free"
                elif o == pn:
                    st = "self"
                else:
                    st = dipl[o][0] if o < len(dipl) else "?"
                key = (st, t in roads)
                per[key] += 1
                if t in roads and st not in ("free", "self"):
                    examples.setdefault(st, (t, o))
                tot[key] += 1
            for st in sorted({k[0] for k in per}):
                if st in ("free", "self"):
                    continue
                ex = examples.get(st)
                print("  %-22s %-6s %-10s picks=%d roaded=%d%s"
                      % (label, pname, st, per[(st, False)] + per[(st, True)],
                         per[(st, True)],
                         "" if ex is None
                         else "  e.g. %d,%d owned by p%d" % (ex[0][0],
                                                             ex[0][1], ex[1])))
    print("  TOTAL by state:")
    for st in sorted({k[0] for k in tot}):
        print("    %-12s picks=%d roaded=%d"
              % (st, tot[(st, False)] + tot[(st, True)], tot[(st, True)]))


def vision_report(root):
    """Do deep allies actually exchange shared vision?

    Relevant to the meet-in-the-middle question only as a REMEDY check:
    deep inherits stock's CLAUSE_VISION offer (ai/default's
    dai_diplomacy_actions(), which deep calls at deepai.c:1270), gated by
    shared_vision_is_safe() (daidiplomacy.c:185). Whether it actually
    happens is a fact about games, not about code.
    """
    print("\n== shared vision between allied deep players (game end)")
    pairs = both = one = 0
    for label, log in games(root):
        d = os.path.dirname(log)
        sv = [os.path.join(d, "saves", f)
              for f in sorted(os.listdir(os.path.join(d, "saves")))
              if "final" in f]
        if not sv:
            continue
        _roads, _owner, players = savegame_roads(sv[0])
        for a, pa in players.items():
            for b, pb in players.items():
                if b <= a or pa["ai"] != "deep" or pb["ai"] != "deep":
                    continue
                if a >= len(pb["dipl"]) or b >= len(pa["dipl"]):
                    continue
                if pa["dipl"][b][0] not in ("Alliance", "Team"):
                    continue
                pairs += 1
                n = int(pa["dipl"][b][1]) + int(pb["dipl"][a][1])
                both += (n == 2)
                one += (n == 1)
    print("  allied deep pairs=%d  both give vision=%d  one only=%d"
          % (pairs, both, one))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    if argv[1] == "--roads":
        for root in argv[2:]:
            roads_report(root)
            vision_report(root)
        return 0
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
    for k in ("allylinks", "pairs", "exact", "near", "parallel",
              "disjoint", "exact_parallel", "exact_sym"):
        print("%-24s%s" % (k, "".join("%14d" % g[k] for g in aligns)))
    print("%-24s%s" % ("endpoint distance",
                       "".join("%14.2f" % g["meand"] for g in aligns)))
    print("%-24s%s" % ("tile overlap",
                       "".join("%14.3f" % g["overlap"] for g in aligns)))
    print("%-24s%s" % ("same-link overlap",
                       "".join("%14.3f" % g["exact_overlap"]
                               for g in aligns)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
