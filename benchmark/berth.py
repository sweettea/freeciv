#!/usr/bin/env python3
"""Read deep-berth: where a founder that needs a hull is standing.

    berth.py RESULTS_DIR [RESULTS_DIR ...]

One line per founder-turn that needs a lift, written by
deep_settler_berth_census() (ai/deep/deepsettler.c), plus the walks
deep_settler_berth_route() ordered when founder_berth_route is on.

    coast     the founder touches water at all -- the test the stock
              ferry path stops at (is_terrain_class_near_tile)
    serves    that water reaches the TARGET: one body of water touching
              both ends, which is what a hull actually needs
    bridge    that water leads to some other continent (slice 2's
              build-side gate)
    destsea   the target can be landed at by hull at all
    berth     nearest tile that does serve the crossing, turns to walk

The population slice 3 exists for is `coast=1 serves=0`: a founder that
passed the ferry path's test beside a sea that cannot carry it. Of
those, `bridge=1` is the case slice 2 could not see from the build side
-- water that goes somewhere, just not where this founder is going.
"""
import re
import sys
from collections import Counter
from pathlib import Path

BERTH = re.compile(r'deep-berth (?P<kv>.*)$')
ROUTE = re.compile(r'deep-berthroute (?P<kv>.*)$')


def kv(text):
    return dict(re.findall(r'(\w+)=(-?\d+)(?![\d,)])', text))


def read(root):
    rows, routes = [], []
    for log in sorted(Path(root).rglob("game.log")):
        for line in open(log, errors="ignore"):
            m = BERTH.search(line)
            if m:
                d = kv(m.group('kv'))
                d['seed'] = log.parent.name
                rows.append(d)
                continue
            m = ROUTE.search(line)
            if m:
                d = kv(m.group('kv'))
                d['seed'] = log.parent.name
                routes.append(d)
    return rows, routes


def main(argv):
    rows, routes = [], []
    for root in argv[1:] or ["benchmark/results"]:
        a, b = read(root)
        rows += a
        routes += b
    if not rows:
        print("no deep-berth lines")
        return 1

    seeds = {r['seed'] for r in rows}
    print("founder-turns needing a hull: %d over %d seeds, turns %d-%d"
          % (len(rows), len(seeds),
             min(int(r['turn']) for r in rows),
             max(int(r['turn']) for r in rows)))

    def frac(pred):
        n = sum(1 for r in rows if pred(r))
        return "%6d  %5.1f%%" % (n, 100.0 * n / len(rows))

    print("  coast=1 (beside water)          %s"
          % frac(lambda r: r['coast'] == '1'))
    print("  serves=1 (water reaches target) %s"
          % frac(lambda r: r['serves'] == '1'))
    print("  STRANDED coast=1 serves=0       %s"
          % frac(lambda r: r['coast'] == '1' and r['serves'] == '0'))
    print("    ... of which bridge=1         %s"
          % frac(lambda r: r['coast'] == '1' and r['serves'] == '0'
                 and r['bridge'] == '1'))
    print("    ... with a berth reachable    %s"
          % frac(lambda r: r['coast'] == '1' and r['serves'] == '0'
                 and int(r['turns']) >= 0))
    print("  inland coast=0                  %s"
          % frac(lambda r: r['coast'] == '0'))
    print("  destsea=0 (target unlandable)   %s"
          % frac(lambda r: r['destsea'] == '0'))
    print("  found=1 (a founder, not a worker) %s"
          % frac(lambda r: r.get('found') == '1'))

    stranded = [r for r in rows
                if r['coast'] == '1' and r['serves'] == '0'
                and int(r['turns']) >= 0]
    if stranded:
        t = [int(r['turns']) for r in stranded]
        print("  berth walk for the stranded: mean %.2f turns, max %d"
              % (sum(t) / len(t), max(t)))
    units = Counter((r['seed'], r['unit']) for r in rows
                    if r['coast'] == '1' and r['serves'] == '0')
    if units:
        print("  distinct stranded founders: %d, worst stands %d turns"
              % (len(units), max(units.values())))

    print("deep-berthroute: %d walks ordered over %d seeds"
          % (len(routes), len({r['seed'] for r in routes})))
    if routes:
        d = [int(r['dist']) for r in routes]
        t = [int(r['turns']) for r in routes]
        print("  mean dist %.2f (max %d), mean turns %.2f (max %d),"
              " %d distinct founders"
              % (sum(d) / len(d), max(d), sum(t) / len(t), max(t),
                 len({(r['seed'], r['unit']) for r in routes})))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
