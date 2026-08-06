#!/usr/bin/env python3
"""Census of the wonder-caravan producer (R2b) over benchmark results.

    wondercensus.py RESULTS_DIR [RESULTS_DIR...] [--arm deep]

Three things per result dir, all per game and averaged over games:

  seam      the deep-wonder-merit line -- how often the producer is
            asked, how often it has a wonder city to answer about, and
            what it answers relative to stock's bid at the same seam.
            The ELIGIBLE subset (wwant > 0) is the only population the
            two prices can be compared on; the rest are city-turns with
            no wonder under construction anywhere.
  build     caravan build picks (deep-want) and caravan dispatches by
            errand (deep-trade kind=), which is as close to "deliveries"
            as this tree instruments: the Help Wonder action itself is
            executed inside ai/default's dai_manage_caravan(), which
            deep must not modify, so nothing logs the arrival.
  wonders   completed wonders and the turns they completed on, read from
            the scorelog's own `wonders` tag rather than from any
            intention log -- a wonder that exists is a wonder that got
            built. Completion turns are the turns on which the count
            rises, averaged over every rise by every benchmark seat.

Percentiles are p10/p50/p90, printed for the quantities R3's deflator
table needs: deep's merit, stock's bid, and their ratio.
"""

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean

FIELD = re.compile(r"(\w+)=(-?[\d.]+)")
INF = 1e8


def q(vals):
    v = sorted(vals)
    if not v:
        return "     --      --      --"
    n = len(v)
    return "%8.3f%8.3f%8.3f" % (v[n // 10], v[n // 2], v[(9 * n) // 10])


def seam_rows(text):
    rows = []
    for line in text.split("\n"):
        if "deep-wonder-merit" not in line:
            continue
        d = {k: float(v) for k, v in FIELD.findall(line)}
        m = re.search(r"mode=(\w+)", line)
        r = re.search(r"recover=(\S+)", line)
        d["mode"] = m.group(1) if m else "?"
        d["recover"] = r.group(1) if r else "-"
        rows.append(d)
    return rows


def wonder_series(scorelog):
    """{player: {turn: wonders}} for the benchmark seats P1..Pn."""
    tags, players, data = {}, {}, defaultdict(dict)
    for line in scorelog.split("\n"):
        p = line.split(" ")
        if p[0] == "tag":
            tags[int(p[1])] = p[2]
        elif p[0] == "addplayer":
            players[int(p[2])] = " ".join(p[3:])
        elif p[0] == "data":
            turn, tag, plr = int(p[1]), int(p[2]), int(p[3])
            data[plr].setdefault(turn, {})[tag] = p[4]
    wid = next((t for t, n in tags.items() if n == "wonders"), None)
    if wid is None:
        return {}
    out = {}
    for plr, turns in data.items():
        if not re.fullmatch(r"P\d+", players.get(plr, "")):
            continue
        out[plr] = {t: int(v[wid]) for t, v in sorted(turns.items())
                    if wid in v}
    return out


def game(gamedir):
    text = (gamedir / "game.log").read_text(errors="replace")
    rows = seam_rows(text)
    # Three nested populations, and the distinction matters: a line with
    # a wonder want is not a line that got PRICED. The want function
    # returns 0 early -- with walk and build still at their -1 sentinel
    # -- when the wonder is already fully funded by caravans in the
    # field, which is stock's count gate expressed as a zero. Mixing
    # those zeros into the percentiles reads as "deep prices half of
    # them at nothing", when what happened is that deep refused half of
    # them for a stated reason.
    el = [r for r in rows if r.get("wwant", 0) > 0]
    priced = [r for r in el if r.get("walk", -1) >= 0]
    refused = [r for r in el if r.get("walk", -1) < 0]
    fin = [r for r in priced if r.get("build", INF) < INF]
    noshields = [r for r in priced if r.get("build", INF) >= INF]
    modes = Counter(r["mode"] for r in rows)
    rec = Counter(r["recover"] for r in rows)

    picks = len(re.findall(r'deep-want .*pick="(?:Caravan|Freight)"', text))
    disp_w = len(re.findall(r"deep-trade turn=\S+ .*kind=wonder", text))
    disp_r = len(re.findall(r"deep-trade turn=\S+ .*kind=route", text))

    ws = wonder_series((gamedir / "scorelog.txt").read_text(errors="replace"))
    built, turns = 0, []
    for series in ws.values():
        prev = None
        for t, v in sorted(series.items()):
            if prev is not None and v > prev:
                built += v - prev
                turns.extend([t] * (v - prev))
            prev = v
    final = sum(s[max(s)] for s in ws.values() if s)

    return {
        "lines": len(rows), "eligible": len(el), "priced": len(priced),
        "refused_funded": len(refused), "no_shields": len(noshields),
        "finite": len(fin),
        "replace": modes["replace"], "promote": modes["promote"],
        "keep": modes["keep"],
        "recover_yes": rec["yes"], "recover_partial": rec["partial"],
        "picks": picks, "disp_wonder": disp_w, "disp_route": disp_r,
        "wonders_final": final, "wonders_built": built,
        "wonder_turns": turns,
        "merit": [r["merit"] for r in fin],
        "stock": [r["stock"] for r in fin],
        "ratio": [r["merit"] / r["stock"] for r in fin if r["stock"] > 0],
        "share": [r["usable"] / r["cost"] for r in fin if r["cost"] > 0],
        "walk": [r["walk"] for r in fin],
        "buildt": [r["build"] for r in fin],
        "pledged_pos": sum(1 for r in fin if r["pledged"] > 0),
    }


def main():
    argv = list(sys.argv[1:])
    arm = "deep"
    if "--arm" in argv:
        i = argv.index("--arm")
        arm = argv[i + 1]
        del argv[i:i + 2]

    for root in argv:
        dirs = sorted((Path(root) / arm).glob("seed-*"))
        dirs = [d for d in dirs if (d / "DONE").exists()]
        if not dirs:
            print("%s: no completed games" % root)
            continue
        games = [game(d) for d in dirs]
        print("== %s   (%d games, arm %s)" % (root, len(games), arm))
        for k in ("lines", "eligible", "priced", "refused_funded",
                  "no_shields", "finite", "replace", "promote",
                  "keep", "recover_yes", "recover_partial", "picks",
                  "disp_wonder", "disp_route", "wonders_final",
                  "wonders_built", "pledged_pos"):
            print("   %-16s %9.2f per game" % (k, mean(g[k] for g in games)))
        wt = [t for g in games for t in g["wonder_turns"]]
        print("   %-16s %9.2f  (n=%d)"
              % ("completion turn", mean(wt) if wt else float("nan"), len(wt)))
        print("   %-16s %8s%8s%8s" % ("quantity", "p10", "p50", "p90"))
        for k in ("merit", "stock", "ratio", "share", "walk", "buildt"):
            print("   %-16s %s" % (k, q([v for g in games for v in g[k]])))
        print()


main()
