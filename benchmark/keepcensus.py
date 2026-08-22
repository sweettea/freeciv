#!/usr/bin/env python3
"""What the W12G keepable-bid gates did, per game.

    keepcensus.py RESULTDIR [RESULTDIR ...]

deeptrunk.c writes one `deep-trunkkeep` line per deep player per turn
and resets its counters after writing, so a game's totals are the SUM of
its lines -- unlike deep-trunkfun, which is cumulative and read from its
last line. Three fields:

    veto   corridor bids the founder floor refused: bids that beat the
           advisor's champion and would have been assigned, then
           discarded by the very next comparison. Zero by construction
           when trunk_keepable_gate is shut.
    held   workers the source re-priced a corridor tile for that they
           were already walking to. Zero when trunk_keep_margin is 0,
           because no hold is recorded at all.
    kept   of those, the ones the sunk-walk premium was decisive for --
           the raw bid had lost.

Artefacts only; runs no games and reads no savegames.
"""
import re
import sys
import glob
import os
import collections

LINE = re.compile(r'deep-trunkkeep turn=(\d+) player="([^"]*)" '
                  r'veto=(\d+) held=(\d+) kept=(\d+)')


def read_game(path):
    tot = collections.Counter()
    players = set()
    with open(path, errors='replace') as fh:
        for line in fh:
            m = LINE.search(line)
            if m:
                players.add(m.group(2))
                tot['veto'] += int(m.group(3))
                tot['held'] += int(m.group(4))
                tot['kept'] += int(m.group(5))
                tot['lines'] += 1
    tot['players'] = len(players)
    return tot


def main():
    logs = []
    for root in sys.argv[1:]:
        logs += sorted(glob.glob(os.path.join(root, '**', 'game.log'),
                                 recursive=True))
    logs = [lg for lg in logs
            if os.path.exists(os.path.join(os.path.dirname(lg), 'DONE'))]
    if not logs:
        sys.exit('no completed games under %s' % ' '.join(sys.argv[1:]))

    games = []
    for lg in logs:
        g = read_game(lg)
        g['seed'] = os.path.basename(os.path.dirname(lg))
        games.append(g)

    print('    %-10s %8s %8s %8s %8s' % ('seed', 'veto', 'held', 'kept',
                                         'lines'))
    tot = collections.Counter()
    for g in games:
        print('    %-10s %8d %8d %8d %8d'
              % (g['seed'], g['veto'], g['held'], g['kept'], g['lines']))
        for k in ('veto', 'held', 'kept', 'lines'):
            tot[k] += g[k]
    n = len(games)
    print('    %-10s %8.1f %8.1f %8.1f %8.1f'
          % ('MEAN', tot['veto'] / n, tot['held'] / n, tot['kept'] / n,
             tot['lines'] / n))
    print('    games with any keep line: %d of %d'
          % (sum(1 for g in games if g['lines'] > 0), n))
    if tot['held']:
        print('    kept / held: %.1f%%' % (100.0 * tot['kept'] / tot['held']))


if __name__ == '__main__':
    main()
