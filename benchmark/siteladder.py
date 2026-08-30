#!/usr/bin/env python3
"""What the settler search was never allowed to look at (tbskip census).

  siteladder.py DIR ...     one table per directory of seed-*/ games

The refusal ladder's buckets all count tiles the pf map OFFERED. The
tbskip/tbown/tbunit/tbland counters added by S-SITEPLAN count the tiles
it never queued, because deep_settler_tile_behavior() returned TB_IGNORE
and pf_normal_node_init() dropped the node (path_finding.c:288). The
settler-pipeline census could say the horizon was truncated and not by
how much; this is by how much, per search, split by whether the search
came home with a site.
"""
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

R = re.compile(r'deep-siteladder turn=(\d+) player="[^"]+" unit=\d+ '
               r'boat=(\d+) seen=(\d+) off=(\d+) nofound=(\d+) '
               r'mindist=(\d+) danger=\d+ taken=\d+ starve=\d+ '
               r'scored=(\d+) neg=\d+ best_raw=\S+ best_net=\S+ '
               r'found=(\d) noterr=\d+ nomin=(\d+) nomap=\d+ '
               r'nolegal=\d+ offbridge=(\d+) offpeace=(\d+) '
               r'tbskip=(\d+) tbown=(\d+) tbunit=(\d+) tbland=(\d+)')

COLS = ("seen", "off", "offpeace", "mindist", "nomin", "scored",
        "tbskip", "tbown", "tbunit", "tbland")


def logtext(d):
    # Finished game logs are zstd-compressed as a matter of course
    # on this box, so both forms have to be readable.
    zst = d / "game.log.zst"
    if zst.exists():
        out = subprocess.run(["zstdcat", str(zst)],
                             capture_output=True).stdout
        return out.decode("utf8", "replace")
    p = d / "game.log"
    return p.read_text(errors="replace") if p.exists() else ""


def table(root):
    root = Path(root)
    pops = {"all": Counter(), "found": Counter(), "empty": Counter()}
    n = {"all": 0, "found": 0, "empty": 0}
    bucket = defaultdict(Counter)
    bn = Counter()
    for d in sorted(root.glob("seed-*")):
        if not (d / "DONE").exists():
            continue
        for m in R.finditer(logtext(d)):
            turn = int(m.group(1))
            vals = {"seen": int(m.group(3)), "off": int(m.group(4)),
                    "mindist": int(m.group(6)), "scored": int(m.group(7)),
                    "nomin": int(m.group(9)), "offpeace": int(m.group(11)),
                    "tbskip": int(m.group(12)), "tbown": int(m.group(13)),
                    "tbunit": int(m.group(14)), "tbland": int(m.group(15))}
            pop = "found" if m.group(8) == "1" else "empty"
            for k in ("all", pop):
                pops[k].update(vals)
                n[k] += 1
            b = (turn // 25) * 25
            bucket[b].update(vals)
            bn[b] += 1

    print(f"\n=== {root}  ({n['all']} searches: {n['found']} found a site, "
          f"{n['empty']} came home empty)")
    print(f"    {'population':<10}" + "".join(f"{c:>10}" for c in COLS))
    for k in ("all", "found", "empty"):
        if n[k]:
            print(f"    {k:<10}"
                  + "".join(f"{pops[k][c]/n[k]:>10.1f}" for c in COLS))
    print(f"\n    per search, by 25-turn bucket")
    print(f"    {'turn':<10}{'n':>10}" + "".join(f"{c:>10}" for c in COLS))
    for b in sorted(bucket):
        k = bn[b]
        print(f"    {b:<10}{k:>10}"
              + "".join(f"{bucket[b][c]/k:>10.1f}" for c in COLS))


for arg in sys.argv[1:] or ["benchmark/results/siteplan/identity"]:
    table(arg)
