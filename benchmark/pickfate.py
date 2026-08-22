#!/usr/bin/env python3
"""What became of a corridor pick, at two resolutions the ledger cannot
reach.

    pickfate.py RESULTDIR/<arm>

deepfunnel.c charges every dropped corridor assignment to a cause, but
two of its buckets are opaque by construction. `norun` means the task
source never saw the worker again -- held, mid-activity elsewhere, out
of moves, or on a city mission -- and none of those paths reaches a site
that could record which. And nothing at all is charged when a pick is
overridden LATER IN THE SAME CALL, because the corridor auction has
already returned by then.

Both are answerable from the log alone, on games already run.

SAME TURN. deep_auto_settler_run() runs the corridor task source inside
the UTYF_WORKERS block and then, for a unit that is also a cityfounder,
runs deep_find_best_city_placement() and takes the site if it outscores
the worker champion -- which is the corridor pick. In the classic
ruleset Settlers carry UTYF_WORKERS, so the corridor auction bids for
them and can win a unit that is about to be sent off to found a city
instead. `deep-settler ... pick target=` is that branch firing, so a
pick and a founder pick for the same unit on the same turn is the
override, counted exactly.

NEXT TURN. A pick that no pick by the same unit follows is split by what
the unit was doing: on a city mission (the BUILD_CITY branch returns
before FIND_WORK), silent but alive (mid-activity elsewhere or held), or
never named again (dead, disbanded, joined a city).
"""
import re
import sys
import glob
import os
import collections

PICK = re.compile(r'deep-trunkpick turn=(\d+) player="([^"]*)" unit=(\d+) ')
UNIT = re.compile(r'deep-\w+ turn=(\d+) player="([^"]*)" unit=(\d+) ')
FOUND = re.compile(r'deep-settler turn=(\d+) player="([^"]*)" unit=(\d+) '
                   r'at=\(\d+,\d+\) pick target=')
MISSION = re.compile(r'deep-settler turn=(\d+) player="([^"]*)" unit=(\d+)'
                     r'.*? mission target=')


def one(path):
    picks = collections.defaultdict(set)
    seen = collections.defaultdict(set)
    founder, mission = set(), set()
    for line in open(path, errors='replace'):
        m = PICK.search(line)
        if m:
            picks[(m.group(2), int(m.group(3)))].add(int(m.group(1)))
        m = UNIT.search(line)
        if m:
            seen[(m.group(2), int(m.group(3)))].add(int(m.group(1)))
        m = FOUND.search(line)
        if m:
            founder.add((m.group(2), int(m.group(3)), int(m.group(1))))
        m = MISSION.search(line)
        if m:
            mission.add((m.group(2), int(m.group(3)), int(m.group(1))))

    out = collections.Counter()
    for (plr, uid), turns in picks.items():
        last = max(seen[(plr, uid)])
        for t in turns:
            out['picks'] += 1
            if (plr, uid, t) in founder:
                out['founder override'] += 1
            if t + 1 in turns:
                out['renewed'] += 1
            elif (plr, uid, t + 1) in mission:
                out['city mission'] += 1
            elif t + 1 <= last:
                out['silent, alive'] += 1
            else:
                out['silent, gone'] += 1
    return out


def main():
    dirs = [d for d in sorted(glob.glob(os.path.join(sys.argv[1], 'seed-*')))
            if os.path.exists(os.path.join(d, 'DONE'))]
    if not dirs:
        sys.exit('no completed games under %s' % sys.argv[1])

    keys = ['picks', 'founder override', 'renewed', 'city mission',
            'silent, alive', 'silent, gone']
    print('%-5s' % 'seed' + ''.join('%17s' % k for k in keys))
    tot = collections.Counter()
    for d in dirs:
        r = one(os.path.join(d, 'game.log'))
        tot.update(r)
        print('%-5s' % os.path.basename(d).replace('seed-', '')
              + ''.join('%17d' % r[k] for k in keys))
    n = len(dirs)
    print('%-5s' % 'MEAN' + ''.join('%17.1f' % (tot[k] / n) for k in keys))

    print()
    print('SAME TURN: %d of %d picks (%.1f%%) were overridden by the'
          ' city-placement search in the same call'
          % (tot['founder override'], tot['picks'],
             100.0 * tot['founder override'] / max(tot['picks'], 1)))
    print()
    unre = tot['city mission'] + tot['silent, alive'] + tot['silent, gone']
    print('NEXT TURN: %d picks were not followed by a pick by the same'
          ' unit' % unre)
    for k in keys[3:]:
        print('  %-14s %6d  %5.1f%%'
              % (k, tot[k], 100.0 * tot[k] / max(unre, 1)))


main()
