#!/usr/bin/env python3
"""The terrain-chain funnel, rolled out of the deep-terrachain census.

    chaincensus.py RESULTDIR [RESULTDIR ...]

deepchain.c writes one cumulative line per deep player per turn, so the
LAST line of a game per player is that player's whole-game funnel. This
reads those, sums them per game and prints the stages.

Read it as two questions, in this order.

  1. IS THE VENUE ASKING? `cand` counts terrain-change candidates the
     advisor priced at all and `multi` those that had a chain beyond
     the immediate result. Both are counted with the gate SHUT as well
     as open, so a run with cand=0 says the venue never offered a
     cultivate or a plant -- a statement about the map and the ruleset,
     not about the mechanism -- and a run with multi=0 says the
     ruleset's terrain graph has no chains to find.

  2. DID THE PRICE DO ANYTHING? `lift` is zero by construction with the
     gate shut, so it and nothing else is the knob. Beyond it the
     tile-level stages say whether a raised price ever turned into
     work: `tiles` distinct tiles lifted, `work` those whose terrain
     later changed at all, `done` those that reached the terrain the
     price was paid for. lift > 0 with work = 0 is the readable null --
     priced, and never won the auction.

Reads artefacts only; it runs no games and needs no savegames.
"""
import re
import sys
import glob
import os
import collections

LINE = re.compile(r'deep-terrachain turn=(\d+) player="([^"]*)" '
                  r'cand=(\d+) multi=(\d+) lift=(\d+) gain=(-?\d+) '
                  r'tiles=(\d+) work=(\d+) done=(\d+) stray=(\d+) '
                  r'lost=(\d+) live=(\d+)')

FIELDS = ['cand', 'multi', 'lift', 'gain', 'tiles', 'work', 'done',
          'stray', 'lost', 'live']


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
    tot['players'] = len(last)
    return tot


def pct(a, b):
    return '     --' if not b else '%6.1f%%' % (100.0 * a / b)


def main():
    logs = []
    for root in sys.argv[1:]:
        logs += sorted(glob.glob(os.path.join(root, '**', 'game.log'),
                                 recursive=True))
    logs = [lg for lg in logs if os.path.exists(
        os.path.join(os.path.dirname(lg), 'DONE'))]
    if not logs:
        sys.exit('no completed games under %s' % ' '.join(sys.argv[1:]))

    games = []
    for lg in logs:
        g = read_game(lg)
        if g['players'] > 0:
            g['seed'] = os.path.basename(os.path.dirname(lg))
            g['dir'] = os.path.dirname(os.path.dirname(os.path.dirname(lg)))
            games.append(g)
    if not games:
        sys.exit('games found, but not one carries a deep-terrachain line')

    n = len(games)
    tot = collections.Counter()
    for g in games:
        tot.update({k: v for k, v in g.items()
                    if k not in ('seed', 'dir')})

    print('%d games, %d deep player-games' % (n, tot['players']))
    print()
    print('%-6s %8s %8s %8s %8s %6s %6s %6s %6s %6s' %
          ('seed', 'cand', 'multi', 'lift', 'gain', 'tiles', 'work',
           'done', 'stray', 'live'))
    for g in games:
        print('%-6s %8d %8d %8d %8d %6d %6d %6d %6d %6d' %
              (g['seed'].replace('seed-', ''), g['cand'], g['multi'],
               g['lift'], g['gain'], g['tiles'], g['work'], g['done'],
               g['stray'], g['live']))
    print('%-6s %8.1f %8.1f %8.1f %8.1f %6.1f %6.1f %6.1f %6.1f %6.1f' %
          ('MEAN', tot['cand'] / n, tot['multi'] / n, tot['lift'] / n,
           tot['gain'] / n, tot['tiles'] / n, tot['work'] / n,
           tot['done'] / n, tot['stray'] / n, tot['live'] / n))

    print()
    print('=== the funnel, per game (mean over %d games) ===' % n)
    rows = [
        ('1 cand    terrain-change candidates priced', tot['cand'], None),
        ('2 multi   of those, ones with a chain', tot['multi'], tot['cand']),
        ('3 lift    prices raised above one step', tot['lift'], tot['multi']),
        ('. gain    want points added', tot['gain'], None),
        ('4 tiles   DISTINCT tiles lifted', tot['tiles'], None),
        ('5 work    lifted tiles whose terrain changed', tot['work'],
         tot['tiles']),
        ('6 done    lifted tiles that reached the target', tot['done'],
         tot['tiles']),
        ('. stray   changed off the chain instead', tot['stray'],
         tot['tiles']),
        ('. lost    stopped being ours', tot['lost'], tot['tiles']),
        ('. live    still pending at the end', tot['live'], tot['tiles']),
    ]
    for label, val, base in rows:
        print('%-45s %10.1f %s' % (label, val / n, pct(val, base)))

    if tot['lift'] == 0:
        print()
        print('lift=0: the gate was SHUT in every game read (or no chain'
              ' ever beat its own first step).')
    elif tot['work'] == 0:
        print()
        print('lift>0 with work=0: prices were raised and not one lifted'
              ' tile was ever worked -- the raise never won the auction.')


if __name__ == '__main__':
    main()
