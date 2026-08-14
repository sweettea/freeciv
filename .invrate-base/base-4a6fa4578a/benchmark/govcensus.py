#!/usr/bin/env python3
"""Government census for a benchmark cell.

Reads what a run already writes -- the scorelog's government tag, the
savegame's own `changed_to_times` counter, and (when the deep valuation
is on and metric logging is up) the `deep-gov` term lines -- and answers
the three questions a government A/B actually turns on:

  WHERE it settles   turns spent under each government, per arm
  HOW OFTEN it moves government changes per game, and the turns of
                     Anarchy the server charges for them
  WHY it moved       the term decomposition at each evaluation burst,
                     and how much of the spread each term owns

Usage:
    benchmark/govcensus.py RESULTSDIR [RESULTSDIR ...]

Each RESULTSDIR is an arm: a directory containing <cfg>/seed-<n>/ with
scorelog.txt, saves/ and game.log. Arms are reported side by side and,
when there are exactly two with matching seeds, paired.

Written for the GOV diagnosis (findings/gov-anarchy-2026-08-12.md); the
G3 re-run is its first consumer.
"""
import collections
import glob
import os
import re
import statistics
import subprocess
import sys

# Ruleset government order; the scorelog's gov tag is an index into it.
# Classic and civ2civ3 share this prefix, which is all we index.
GOVNAME = ["Anarchy", "Tribal", "Despotism", "Monarchy", "Communism",
           "Fundamentalism", "Federation", "Republic", "Democracy"]

GOVLINE = re.compile(
    r'deep-gov turn=(\d+) player="([^"]+)" gov="([^"]+)" now="([^"]+)" '
    r'base=([-\d.]+) out=([-\d.]+) upk=([-\d.]+) delta=([-\d.]+) '
    r'horizon=(\d+) anturns=([-\d.]+) ancost=([-\d.]+) ceil=([-\d.]+) '
    r'dist=(\d+) val=([-\d.]+)')


def seed_dirs(root):
    """-> {seed: path}, over whatever config subdirectory the arm used."""
    out = {}
    for p in glob.glob(os.path.join(root, "*", "seed-*")):
        m = re.match(r'seed-(\d+)$', os.path.basename(p))
        if m and os.path.isdir(p):
            out[int(m.group(1))] = p
    return out


def read_scorelog(path):
    """-> {tagname: {turn: {player: value}}}"""
    tagname = {}
    data = collections.defaultdict(lambda: collections.defaultdict(dict))
    try:
        fh = open(os.path.join(path, "scorelog.txt"), errors="replace")
    except OSError:
        return data
    with fh:
        for ln in fh:
            f = ln.split()
            if not f:
                continue
            if f[0] == "tag" and len(f) >= 3:
                tagname[int(f[1])] = f[2]
            elif f[0] == "data" and len(f) >= 5:
                data[tagname.get(int(f[2]), f[2])][int(f[1])][int(f[3])] \
                    = int(f[4])
    return data


def read_changes(path):
    """Government change counter out of the final savegame."""
    fin = [f for f in glob.glob(os.path.join(path, "saves", "*"))
           if "final" in f]
    if not fin:
        return None
    try:
        raw = subprocess.run(["zstd", "-dc", fin[0]], capture_output=True,
                             check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return None
    txt = raw.decode("utf-8", "replace")
    m = re.search(r'government=\{"name","changes"\n(.*?)\n\}', txt, re.S)
    if not m:
        return None
    out = collections.Counter()
    for ln in m.group(1).splitlines():
        g = re.match(r'"([^"]+)",(\d+)', ln.strip())
        if g:
            out[g.group(1)] = int(g.group(2))
    return out


def read_govlines(path, seed):
    recs = []
    try:
        fh = open(os.path.join(path, "game.log"), errors="replace")
    except OSError:
        return recs
    with fh:
        for ln in fh:
            if "deep-gov " not in ln:
                continue
            m = GOVLINE.search(ln)
            if not m:
                continue
            g = m.groups()
            recs.append(dict(
                seed=seed,
                turn=int(g[0]), plr=g[1], gov=g[2], now=g[3],
                base=float(g[4]), out=float(g[5]), upk=float(g[6]),
                delta=float(g[7]), horizon=int(g[8]), anturns=float(g[9]),
                ancost=float(g[10]), ceil=float(g[11]), dist=int(g[12]),
                val=float(g[13])))
    return recs


def amortize(value, dist):
    """Freeciv's amortize() for MORT = 24, enough for ranking."""
    if value <= 0:
        return value
    for _ in range(dist):
        value = value * 23.0 / 24.0
    return value


def census(root):
    """-> dict of per-arm aggregates."""
    sd = seed_dirs(root)
    occ = collections.Counter()
    changes_per_seed, anarchy_per_seed, seeds_used = [], [], []
    dest = collections.Counter()
    recs_all = []
    for seed in sorted(sd):
        path = sd[seed]
        d = read_scorelog(path)
        if "gov" not in d:
            continue
        an = 0
        for turn in d["gov"]:
            for _plr, g in d["gov"][turn].items():
                name = GOVNAME[g] if g < len(GOVNAME) else str(g)
                occ[name] += 1
                if name == "Anarchy":
                    an += 1
        ch = read_changes(path)
        if ch is not None:
            changes_per_seed.append(sum(ch.values()))
            anarchy_per_seed.append(an)
            dest += ch
            seeds_used.append(seed)
        recs_all.extend(read_govlines(path, seed))
    return dict(occ=occ, changes=changes_per_seed, anarchy=anarchy_per_seed,
                seeds=seeds_used, dest=dest, recs=recs_all, n=len(sd))


def burstify(recs):
    b = collections.defaultdict(list)
    for r in recs:
        b[(r["seed"], r["plr"], r["turn"])].append(r)
    return [c for c in b.values() if len(c) > 1 and c[0]["horizon"] > 0]


def report_terms(recs, label):
    bursts = burstify(recs)
    if not bursts:
        return
    print(f"\n--- {label}: term decomposition over {len(bursts)} bursts ---")
    ceil_live = sum(1 for c in bursts for r in c if r["ceil"] != 0.0)
    anc_live = sum(1 for c in bursts for r in c if r["ancost"] != 0.0)
    lines = sum(len(c) for c in bursts)
    print(f"  candidate lines {lines};  ceiling term nonzero on {ceil_live};"
          f"  anarchy charge nonzero on {anc_live}")

    upk = collections.defaultdict(list)
    for c in bursts:
        for r in c:
            upk[r["gov"]].append(r["upk"])
    print(f"  {'gov':<16}{'mean upkeep charged':>21}")
    for g in sorted(upk, key=lambda x: -statistics.mean(upk[x])):
        print(f"  {g:<16}{statistics.mean(upk[g]):>21.1f}")

    # self-consistency: re-scoring the incumbent must give delta 0
    self_d = [r["delta"] for c in bursts for r in c if r["gov"] == c[0]["now"]]
    if self_d:
        print(f"  SELF-DELTA (re-scoring the government already held; a "
              f"correct valuation gives 0):")
        print(f"    n {len(self_d)}  mean {statistics.mean(self_d):+.1f}"
              f"  median {statistics.median(self_d):+.1f}"
              f"  sd {statistics.pstdev(self_d):.1f}"
              f"  negative {sum(1 for x in self_d if x < 0)/len(self_d):.1%}")

    # counterfactual: who would win with the upkeep bill added back
    net = collections.Counter()
    gross = collections.Counter()
    flips = 0
    for c in bursts:
        w = max(c, key=lambda r: r["val"])
        wg = max(c, key=lambda r: amortize(
            (r["out"] + r["upk"] - r["base"]) * r["horizon"]
            - r["ancost"] + r["ceil"], r["dist"]))
        net[w["gov"]] += 1
        gross[wg["gov"]] += 1
        if w["gov"] != wg["gov"]:
            flips += 1
    print(f"  burst winner {'as shipped':>14}{'upkeep removed':>16}")
    for g in sorted(set(net) | set(gross)):
        print(f"    {g:<14}{net[g]:>12}{gross[g]:>16}")
    print(f"  upkeep term changes the winner in {flips}/{len(bursts)} "
          f"= {flips/len(bursts):.1%} of bursts")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    arms = {}
    for root in argv[1:]:
        arms[root] = census(root)

    print("=== TURNS SPENT UNDER EACH GOVERNMENT (player-turns) ===")
    govs = sorted({g for a in arms.values() for g in a["occ"]})
    print(f"  {'gov':<16}" + "".join(f"{os.path.basename(r.rstrip('/')):>22}"
                                     for r in arms))
    for g in govs:
        row = f"  {g:<16}"
        for r in arms:
            tot = sum(arms[r]["occ"].values()) or 1
            row += f"{arms[r]['occ'][g]:>14} ({100*arms[r]['occ'][g]/tot:4.1f}%)"
        print(row)

    print("\n=== REVOLUTIONS AND WHAT THEY COST ===")
    for r, a in arms.items():
        if not a["changes"]:
            continue
        ch, an = a["changes"], a["anarchy"]
        ratio = [x / y for x, y in zip(an, ch) if y]
        print(f"  {os.path.basename(r.rstrip('/'))}: "
              f"{statistics.mean(ch):.1f} government changes per game, "
              f"{statistics.mean(an):.1f} player-turns of Anarchy")
        print(f"    anarchy turns per change: {statistics.mean(ratio):.2f}"
              f" +- {statistics.pstdev(ratio):.2f}"
              f"  (over {len(ratio)} seeds)")
        print("    destinations: " + ", ".join(
            f"{k}:{v}" for k, v in a["dest"].most_common() if v))

    names = list(arms)
    if len(names) == 2:
        a, b = arms[names[0]], arms[names[1]]
        common = sorted(set(a["seeds"]) & set(b["seeds"]))
        if common:
            ia = {s: a["changes"][a["seeds"].index(s)] for s in common}
            ib = {s: b["changes"][b["seeds"].index(s)] for s in common}
            d = [ia[s] - ib[s] for s in common]
            m = statistics.mean(d)
            sd = statistics.stdev(d) if len(d) > 1 else 0.0
            t = m / (sd / len(d) ** 0.5) if sd else float("nan")
            print(f"\n  paired change-count delta "
                  f"({os.path.basename(names[0].rstrip('/'))} minus "
                  f"{os.path.basename(names[1].rstrip('/'))}): "
                  f"{m:+.1f} per game, t {t:+.2f}, "
                  f"{sum(1 for x in d if x > 0)}/{len(d)} seeds higher")

    for r, a in arms.items():
        if a["recs"]:
            report_terms(a["recs"], os.path.basename(r.rstrip("/")))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
