#!/usr/bin/env python3
"""Does a withdrawn founder bid still get charged as displacement?

    sinkseam.py GAMEDIR...

The S-FEARSINK brief's one code question, answered from the c11 games
rather than only from the source. `deep_founder_site_capped()` sets
`founder_want = 0` BEFORE `domestic_advisor_choose_build()` runs
(deepdefence.c:8509), and `deep_mil_opportunity_charge()` charges only
when the choice the advisor RETURNED is a founder (deepdefence.c:8198),
so on a city-turn where the cap fired there must be no `deep-milopp`
line at all.

Prints the count of (turn, player, city) keys carrying a fired cap, a
charge, and both. BOTH MUST BE ZERO.
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contentcensus import loglines                     # noqa: E402

R_CAP = re.compile(r'deep-sitecap turn=(\d+) player="([^"]+)" city=(\d+)'
                   r'.* capped=1 gate=1')
R_OPP = re.compile(r'deep-milopp turn=(\d+) player="([^"]+)" city=(\d+)')

caps = set()
opps = set()
for d in sys.argv[1:]:
    for ln in loglines(Path(d)):
        m = R_CAP.search(ln)
        if m:
            caps.add((d,) + m.groups())
            continue
        m = R_OPP.search(ln)
        if m:
            opps.add((d,) + m.groups())

both = caps & opps
print(f"cap fired on   {len(caps)} city-turns")
print(f"charge made on {len(opps)} city-turns")
print(f"BOTH on        {len(both)} city-turns   "
      f"{'OK -- a withdrawn bid is never charged' if not both else 'BUG'}")
sys.exit(1 if both else 0)
