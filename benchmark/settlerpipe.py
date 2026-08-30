#!/usr/bin/env python3
"""Settler-pipeline census: what happens to a delivered settler.

  settlerpipe.py DIR ...        one table per directory of seed-*/ games

DIR is a directory of finished games (each `seed-N/` with `DONE`,
`scorelog.txt` and `game.log` or `game.log.zst`). It answers the
question the prod-keep verdict left open: the expansion stall is
DOWNSTREAM of settler delivery, so which link eats the settler?

Every founder unit that reaches deep's settler run is followed to one
of three ends:

  founded  a `deep-city ... founded=<this turn>` appears for that
           player on a tile the unit was targeting;
  joined   a `deep-join ... done=1` dissolved it into a city;
  other    it died, was captured, or was still alive at the end.

and the turns it spent are attributed to that end. The other blocks
are the refusal ladder (`deep-siteladder`, split by whether the search
came home with a site), the arrival gate (an `arrived` stall with
moves=x/0 -- MinMoveFrags 1 means the city cannot be founded that
turn), and the mid-flight cancels (`deep-settler ... cancel`).

Read with findings/settler-pipeline-2026-08-29.md, which is where
these numbers were first taken.
"""
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

R_PICK = re.compile(r'deep-settler turn=(\d+) player="([^"]+)" unit=(\d+) '
                    r'at=\(\d+,\d+\) pick target=\((\d+),(\d+)\)')
R_MIS = re.compile(r'deep-settler turn=(\d+) player="([^"]+)" unit=(\d+) '
                   r'at=\(\d+,\d+\) mission target=\((\d+),(\d+)\)')
R_ST = re.compile(r'deep-settler turn=(\d+) player="([^"]+)" unit=(\d+) '
                  r'at=\(\d+,\d+\) stall target=\(\d+,\d+\) reason=(\S+) '
                  r'.* moves=(\d+)/(\d+) got=(\d)')
R_CANCEL = re.compile(r'deep-settler turn=\d+ player="[^"]+" unit=\d+ '
                      r'at=\(\d+,\d+\) cancel target=\(\d+,\d+\) '
                      r'reason=(\S+) towner=(\d+) dist=(\d+)')
R_JOIN = re.compile(r'deep-join turn=(\d+) player="([^"]+)" unit=(\d+) '
                    r'city=\d+ size=\d+ pop=\d+ done=1')
R_CITY = re.compile(r'deep-city turn=(\d+) player="([^"]+)" got=\d+ '
                    r'size=\d+ founded=(\d+) at=\((\d+),(\d+)\)')
R_LAD = re.compile(r'deep-siteladder turn=(\d+) player="[^"]+" unit=(\d+) '
                   r'boat=(\d+) seen=(\d+) off=(\d+) nofound=(\d+) '
                   r'mindist=(\d+) danger=\d+ taken=\d+ starve=(\d+) '
                   r'scored=(\d+) neg=\d+ best_raw=\S+ best_net=\S+ '
                   r'found=(\d) noterr=(\d+) nomin=(\d+)')

LAD_FIELDS = (("seen", 4), ("off", 5), ("nofound", 6), ("mindist", 7),
              ("starve", 8), ("scored", 9), ("noterr", 11), ("nomin", 12))


def loglines(gamedir):
    zst = gamedir / "game.log.zst"
    if zst.exists():
        out = subprocess.run(["zstdcat", str(zst)], capture_output=True).stdout
        return out.decode("utf8", "replace").splitlines()
    return (gamedir / "game.log").read_text(errors="replace").splitlines()


def new_unit():
    return {"mis": 0, "stall": Counter(), "first": None, "joined": None,
            "targets": set(), "picks": 0, "waitarr": 0}


def census(root):
    root = Path(root)
    out = Counter()
    stalls = Counter()
    lad = defaultdict(Counter)
    cancels = Counter()
    travel = []
    games = 0
    for d in sorted(root.glob("seed-*")):
        if not (d / "DONE").exists():
            continue
        games += 1
        units = defaultdict(new_unit)
        founds = defaultdict(list)
        seen_mis, seen_stall = set(), set()
        for ln in loglines(d):
            if "deep-settler " in ln:
                m = R_PICK.search(ln)
                if m:
                    u = units[(m.group(2), m.group(3))]
                    u["picks"] += 1
                    u["targets"].add((m.group(4), m.group(5)))
                    if u["first"] is None:
                        u["first"] = int(m.group(1))
                    continue
                m = R_MIS.search(ln)
                if m:
                    u = units[(m.group(2), m.group(3))]
                    u["mis"] += 1
                    u["targets"].add((m.group(4), m.group(5)))
                    if u["first"] is None:
                        u["first"] = int(m.group(1))
                    seen_mis.add((m.group(1), m.group(2), m.group(3)))
                    continue
                m = R_ST.search(ln)
                if m:
                    u = units[(m.group(2), m.group(3))]
                    u["stall"][m.group(4)] += 1
                    stalls[m.group(4)] += 1
                    if m.group(4) == "arrived":
                        out["arrivals"] += 1
                        if m.group(6) == "0":
                            u["waitarr"] += 1
                            out["arrivals_nomoves"] += 1
                    seen_stall.add((m.group(1), m.group(2), m.group(3)))
                    continue
                m = R_CANCEL.search(ln)
                if m:
                    cancels["%s/towner%s" % (m.group(1), m.group(2))] += 1
                    out["cancels"] += 1
                    continue
            elif "deep-join " in ln:
                m = R_JOIN.search(ln)
                if m:
                    units[(m.group(2), m.group(3))]["joined"] = int(m.group(1))
            elif "deep-city " in ln:
                m = R_CITY.search(ln)
                if m and m.group(1) == m.group(3):
                    founds[(m.group(2), (m.group(4), m.group(5)))].append(
                        int(m.group(1)))
            elif "deep-siteladder " in ln:
                m = R_LAD.search(ln)
                if m:
                    key = ("boat" if int(m.group(3)) else "land",
                           "virt" if m.group(2) == "0" else "real",
                           "site" if m.group(10) == "1" else "nosite")
                    c = lad[key]
                    c["n"] += 1
                    for name, grp in LAD_FIELDS:
                        c[name] += int(m.group(grp))
        out["mission_turns"] += len(seen_mis)
        out["silent"] += len(seen_mis - seen_stall)
        for (plr, _uid), u in units.items():
            got = None
            for tgt in u["targets"]:
                for t in founds.get((plr, tgt), []):
                    if (u["first"] is not None and t >= u["first"]
                            and (got is None or t < got)):
                        got = t
            end = ("founded" if got is not None
                   else "joined" if u["joined"] is not None else "other")
            out[end] += 1
            out[end + "_missionturns"] += u["mis"]
            out[end + "_waitarr"] += u["waitarr"]
            out["units"] += 1
            if got is not None and u["first"] is not None:
                travel.append(got - u["first"])
    return games, out, stalls, lad, cancels, travel


def report(root):
    games, out, stalls, lad, cancels, travel = census(root)
    if games == 0:
        print("%s: no finished games" % root)
        return
    print("=" * 72)
    print("%s   %d games" % (root, games))
    n = max(out["units"], 1)
    print("\n founder units that entered the settler run: %d (%.1f/game)"
          % (out["units"], out["units"] / games))
    for end in ("founded", "joined", "other"):
        print("   %-8s %5d  %5.1f%%  %6.2f/game   %7d mission-turns"
              " (%6.2f/game)"
              % (end, out[end], 100.0 * out[end] / n, out[end] / games,
                 out[end + "_missionturns"],
                 out[end + "_missionturns"] / games))
    if travel:
        travel.sort()
        print("\n pick -> founding: n=%d mean=%.1f median=%d p90=%d turns"
              % (len(travel), sum(travel) / len(travel),
                 travel[len(travel) // 2], travel[int(0.9 * len(travel))]))
    print("\n stall reasons:", dict(stalls.most_common()))
    print(" arrival gate: %d arrivals, %d with 0 move fragments left"
          " (%.0f%%) = %.2f wasted turns/game, %.2f per founding"
          % (out["arrivals"], out["arrivals_nomoves"],
             100.0 * out["arrivals_nomoves"] / max(out["arrivals"], 1),
             out["arrivals_nomoves"] / games,
             out["arrivals_nomoves"] / max(out["founded"], 1)))
    print(" mid-flight cancels: %d (%.2f/game); %d of %d mission-turns"
          " wrote no stall line (%.1f%%)"
          % (out["cancels"], out["cancels"] / games, out["silent"],
             out["mission_turns"],
             100.0 * out["silent"] / max(out["mission_turns"], 1)))
    if cancels:
        print("   ", dict(cancels.most_common(6)))
    print("\n refusal ladder, % of tiles the search saw:")
    print("   %-16s %6s %7s %6s %6s %7s %7s %8s"
          % ("population", "n", "seen/s", "off", "nomin", "noterr",
             "mindist", "scored/s"))
    for key in sorted(lad):
        c = lad[key]
        seen = max(c["seen"], 1)
        print("   %-16s %6d %7.1f %5.0f%% %5.0f%% %6.0f%% %6.0f%% %8.1f"
              % ("-".join(key), c["n"], c["seen"] / c["n"],
                 100.0 * c["off"] / seen, 100.0 * c["nomin"] / seen,
                 100.0 * c["noterr"] / seen, 100.0 * c["mindist"] / seen,
                 c["scored"] / c["n"]))


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    for arg in sys.argv[1:]:
        report(arg)
