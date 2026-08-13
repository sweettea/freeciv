#!/usr/bin/env python3
"""Read one mixed head-to-head A/B, paired seed by seed.

    fencecmp.py OFF_TREE ON_TREE [--cfg cfg0]

Each tree is a `tune.py --mixed` output directory:

    TREE/cfg0/mixed_db/seed-N/{scorelog.txt,game.log,saves/*-final.sav*}
    TREE/cfg0/mixed_bd/seed-N/...

A seed is the PAIR of seat mirrors, summed, which is how tune.py's own
mixed objective is defined (run-game.sh: mixed_db seats deep on odd
players and ai/baseline on even, mixed_bd swaps them, so summing the
mirrors cancels seat quality). Every column below is therefore a
per-seed total over two games.

WHY THIS EXISTS RATHER THAN paircmp.py. paircmp reads metrics.csv,
which run.sh writes and tune.py does not, and it averages ONE arm's
seats -- in a mixed game both modules sit in the same result dir and
the quantity of interest is the difference BETWEEN the two sides of one
table. The score column here is deep-side minus stock-side.

VALIDATION. Run against the MUSTER A/B trees still on disk
(results/muster-ab/{a-control,b-muster}/evaluate) this reproduces
STATUS's published verdict exactly: margin -181.88 -> -278.75, delta
-96.88, t -1.98, 5/16 -- and -181.88/8 = -22.7, -278.75/8 = -34.8,
which are that table's "score/seed" column (it divides by PLAYERS).

Columns
    margin        deep total - stock total (the mixed objective).
                  Divide by the config's PLAYERS to get the per-seat
                  "score/seed" earlier tables report.
    deep / stock  the two halves of it, so a margin move can be read as
                  deep gaining or the counterparty losing
    cityratio     deep cities / stock cities
    wars          deep-side pairs at War in the final save
    citieslost    cities founded by a deep seat and held by somebody
                  else at the end -- the fence's advertised side effect
                  (longer wars) has to be visible next to the score
    fence*        deep-fence counters off the game log
"""
import argparse
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from statistics import mean, stdev

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import savequery                                          # noqa: E402

ARMS = ("mixed_db", "mixed_bd")
COLS = ["margin", "deep", "stock", "cityratio", "wars", "citieslost",
        "fencerefused", "fencewould", "fencelive"]


def scorelog_sides(path, arm):
    """(deep, stock) totals of `score` and `cities` at the last turn."""
    tags, names, data = {}, {}, {}
    for line in open(path, errors="replace"):
        parts = line.split(" ")
        if parts[0] == "tag":
            tags[parts[2].strip()] = int(parts[1])
        elif parts[0] == "addplayer":
            names[int(parts[2])] = " ".join(parts[3:]).strip()
        elif parts[0] == "data":
            t, tag, plr = int(parts[1]), int(parts[2]), int(parts[3])
            data.setdefault(plr, {}).setdefault(tag, {})[t] = parts[4]
    out = {}
    for key in ("score", "cities"):
        deep = stock = 0
        for plr, tagdata in data.items():
            name = names.get(plr, "")
            # Civil-war splinters join mid-game under a ruleset leader
            # name and belong to neither side.
            if not re.fullmatch(r"P\d+", name):
                continue
            turns = tagdata.get(tags[key], {})
            if not turns:
                continue
            v = int(turns[max(turns)])
            if (int(name[1:]) % 2 == 1) == (arm == "mixed_db"):
                deep += v
            else:
                stock += v
        out[key] = (deep, stock)
    return out


def save_facts(gamedir, arm):
    """(deep_wars, deep_cities_lost) from the final savegame."""
    saves = sorted(Path(gamedir, "saves").glob("*-final.sav*"))
    if not saves:
        return None
    text = savequery.read_save(saves[-1])
    deep_slots = set()
    for num, body in savequery._player_sections(text):
        m = re.search(r'^name="([^"]*)"', body, re.M)
        name = m.group(1) if m else ""
        if re.fullmatch(r"P\d+", name) \
           and ((int(name[1:]) % 2 == 1) == (arm == "mixed_db")):
            deep_slots.add(num)
    wars = sum(1 for a, b in savequery.wars(text)
               if (a in deep_slots) != (b in deep_slots))
    lost = sum(1 for owner, _x, _y, _s, orig in savequery.cities(text)
               if orig in deep_slots and owner != orig)
    return wars, lost


def fence_counts(gamedir):
    log = Path(gamedir, "game.log")
    if not log.exists():
        return 0, 0, 0
    text = log.read_text(errors="replace")
    lines = re.findall(r"deep-fence .*", text)
    return (len(lines),
            sum(1 for ln in lines if " would=1 " in ln),
            sum(1 for ln in lines if " live=1 " in ln))


def read_tree(root, cfg):
    """{seed: {col: value}} summed over the two seat mirrors."""
    acc = defaultdict(lambda: defaultdict(float))
    seen = defaultdict(int)
    for arm in ARMS:
        for d in sorted(Path(root, cfg, arm).glob("seed-*")):
            if not (d / "DONE").exists():
                continue
            seed = int(d.name.split("-")[1])
            s = scorelog_sides(d / "scorelog.txt", arm)
            acc[seed]["deep"] += s["score"][0]
            acc[seed]["stock"] += s["score"][1]
            acc[seed]["_dc"] += s["cities"][0]
            acc[seed]["_sc"] += s["cities"][1]
            facts = save_facts(d, arm)
            if facts:
                acc[seed]["wars"] += facts[0]
                acc[seed]["citieslost"] += facts[1]
            r, w, lv = fence_counts(d)
            acc[seed]["fencerefused"] += r
            acc[seed]["fencewould"] += w
            acc[seed]["fencelive"] += lv
            seen[seed] += 1
    out = {}
    for seed, v in acc.items():
        if seen[seed] != len(ARMS):
            print(f"  seed {seed}: only {seen[seed]}/{len(ARMS)} mirrors"
                  f" -- dropped", file=sys.stderr)
            continue
        v["margin"] = v["deep"] - v["stock"]
        v["cityratio"] = v["_dc"] / v["_sc"] if v["_sc"] else float("nan")
        out[seed] = v
    return out


def report(name, pairs):
    diffs = [b - a for a, b in pairs]
    n = len(diffs)
    if n < 2:
        return
    m = mean(diffs)
    se = stdev(diffs) / (n ** 0.5) if n > 1 else 0.0
    t = m / se if se else float("nan")
    wins = sum(1 for d in diffs if d > 0)
    print(f"    {name:<14} {mean(a for a, _ in pairs):>9.2f} "
          f"{mean(b for _, b in pairs):>9.2f} {m:>+9.2f} {se:>7.2f} "
          f"{t:>+6.2f}   {wins}/{n}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("off")
    ap.add_argument("on")
    ap.add_argument("--cfg", default="cfg0")
    a = ap.parse_args()
    mo, mn = read_tree(a.off, a.cfg), read_tree(a.on, a.cfg)
    seeds = sorted(set(mo) & set(mn))
    print(f"paired seeds: {len(seeds)}  (off {len(mo)}, on {len(mn)})"
          f"   [each seed = 2 seat mirrors]")
    print(f"    {'metric':<14} {'OFF':>9} {'ON':>9} {'delta':>9} {'se':>7} "
          f"{'t':>6}   wins")
    for c in COLS:
        report(c, [(mo[s][c], mn[s][c]) for s in seeds])


main()
