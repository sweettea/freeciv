#!/usr/bin/env python3
"""The corridor task funnel, rolled out of the deep-trunkfun census.

    funnelreport.py RESULTDIR [RESULTDIR ...]

deepfunnel.c writes one cumulative line per deep player per turn, so the
LAST line of a game per player is that player's whole-game funnel. This
reads those, sums them per game, and prints the stages with the
attrition between them plus the drop causes ranked.

Reads artefacts only; it runs no games and needs no savegames.
"""
import re
import sys
import glob
import os
import collections

LINE = re.compile(r'deep-trunkfun turn=(\d+) player="([^"]*)" '
                  r'picks=(\d+) tiles=(\d+) start=(\d+) wtiles=(\d+) '
                  r'wturns=(\d+) done=(\d+) dwork=(\d+) dsalv=(\d+) '
                  r'live=(\d+) over=(\d+) drops=(\d+) (\S+)')

FIELDS = ['picks', 'tiles', 'start', 'wtiles', 'wturns', 'done', 'dwork',
          'dsalv', 'live', 'over', 'drops']

# Ordered as deepfunnel.h declares them, so the printed rank keeps the
# stage meaning of each name visible.
CAUSES = ['roaded', 'replan', 'unsafe', 'taken', 'nopath', 'noact',
          'price', 'switch', 'outbid', 'request', 'norun', 'gone',
          'flee', 'stop']


def read_game(path):
    """Last cumulative line per player, summed over the game's players."""
    last = {}
    with open(path, errors='replace') as fh:
        for line in fh:
            m = LINE.search(line)
            if m:
                last[m.group(2)] = m
    tot = collections.Counter()
    for m in last.values():
        for i, f in enumerate(FIELDS):
            tot[f] += int(m.group(3 + i))
        for part in m.group(3 + len(FIELDS)).split(','):
            k, v = part.split('=')
            tot['c_' + k] += int(v)
    tot['players'] = len(last)
    return tot


def pct(a, b):
    return '' if not b else '%6.1f%%' % (100.0 * a / b)


def main():
    logs = []
    for root in sys.argv[1:]:
        logs += sorted(glob.glob(os.path.join(root, '**', 'game.log'),
                                 recursive=True))
    logs = [lg for lg in logs if os.path.exists(
        os.path.join(os.path.dirname(lg), 'DONE'))]
    if not logs:
        sys.exit('no completed games with a deep-trunkfun census under %s'
                 % ' '.join(sys.argv[1:]))

    games = []
    for lg in logs:
        g = read_game(lg)
        if g['picks'] > 0:
            g['seed'] = os.path.basename(os.path.dirname(lg))
            games.append(g)
    if not games:
        sys.exit('games found, but not one carries a deep-trunkfun line')

    n = len(games)
    tot = collections.Counter()
    for g in games:
        tot.update({k: v for k, v in g.items() if k != 'seed'})

    print('%d games, %d deep player-games' % (n, tot['players']))
    print()
    print('%-6s %8s %8s %8s %8s %8s %8s %8s' %
          ('seed', 'picks', 'tiles', 'start', 'wtiles', 'wturns', 'done',
           'dsalv'))
    for g in games:
        print('%-6s %8d %8d %8d %8d %8d %8d %8d' %
              (g['seed'].replace('seed-', ''), g['picks'], g['tiles'],
               g['start'], g['wtiles'], g['wturns'], g['done'], g['dsalv']))
    print('%-6s %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f %8.1f' %
          ('MEAN', tot['picks'] / n, tot['tiles'] / n, tot['start'] / n,
           tot['wtiles'] / n, tot['wturns'] / n, tot['done'] / n,
           tot['dsalv'] / n))

    print()
    print('=== the funnel, per game (mean over %d games) ===' % n)
    rows = [
        ('1 picks       assignments made', tot['picks'], None),
        ('2 tiles       DISTINCT tiles ever assigned', tot['tiles'],
         tot['picks']),
        ('3 walk        picks that did not begin work',
         tot['picks'] - tot['start'], tot['picks']),
        ('4 start       times work actually began', tot['start'],
         tot['picks']),
        ('. wtiles      distinct tiles worked', tot['wtiles'], tot['tiles']),
        ('5 wturns      turns of work accumulated', tot['wturns'], None),
        ('6 done        corridor tiles that got a road', tot['done'],
         tot['tiles']),
        ('. dwork       of those, deep began work there', tot['dwork'],
         tot['done']),
        ('7 dsalv       of those, deep never worked them', tot['dsalv'],
         tot['done']),
        ('. live        still assigned at game end', tot['live'], None),
        ('. over        ledger slots exhausted', tot['over'], None),
    ]
    for label, v, base in rows:
        print('%-44s %9.1f  %s' % (label, v / n, pct(v, base)))
    print()
    print('picks per distinct tile          %6.2f'
          % (tot['picks'] / max(tot['tiles'], 1)))
    print('picks per work start             %6.2f'
          % (tot['picks'] / max(tot['start'], 1)))
    print('work starts per worked tile      %6.2f'
          % (tot['start'] / max(tot['wtiles'], 1)))

    print()
    print('=== where an assignment ended (%.1f drops per game) ==='
          % (tot['drops'] / n))
    ranked = sorted(CAUSES, key=lambda c: -tot['c_' + c])
    for c in ranked:
        v = tot['c_' + c]
        if v == 0:
            continue
        print('%-10s %9.1f  %s' % (c, v / n, pct(v, tot['drops'])))


main()
