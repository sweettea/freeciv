#!/usr/bin/env python3
"""What worker stacking did, per arm: the gate's stages and the stacks.

    stackcensus.py RESULTDIR [RESULTDIR ...]

Two instruments, written by two files, and they answer different halves
of the question.

`deep-trunkstack` (deeptrunk.c) is the GATE's own ledger, one line per
deep player per turn with the counters reset after each write, so a
game's totals are the SUM of its lines:

    seen      occupied corridor tiles the task source met -- counted in
              BOTH settings of trunk_stack_gate, so it is the shared
              denominator
    offered   those it went on to price as a join (0 by construction
              with the gate shut)
    declined  joins refused, split idle:late:partial -- nobody was
              actually working the tile (so there was nothing to join
              and today's skip was right), the job would finish before
              this worker arrived, or the join bought no whole turn.
              Only the last is the marginal test turning down a real
              opportunity
    picked    join assignments actually made
    saved     whole turns those assignments were priced to shave

`deep-stack` (deepstack.c) is the OUTCOME ledger and is cumulative over
the game, so a game's totals are read from its LAST line per player:

    turns     worksite-turns observed (the denominator of the shape)
    n1..n5    those turns by simultaneous worker count
    stk       the n>=2 part, so stk/turns is the stacking RATE
    done      worksites whose improvement appeared, split dsolo/dstk
    tsolo     calendar turns the solo completions took
    tstk      calendar turns the stacked completions took

The two means tstk/dstk and tsolo/dsolo are observational WITHIN an arm
-- a stacked site is stacked because it is slow -- so the number to read
is how each moves BETWEEN two arms of the same seeds.

Artefacts only; runs no games and reads no savegames.
"""
import re
import sys
import glob
import os
import collections

GATE = re.compile(r'deep-trunkstack turn=(\d+) player="([^"]*)" '
                  r'seen=(\d+) offered=(\d+) declined=(\d+) '
                  r'dec=(\d+):(\d+):(\d+) picked=(\d+) saved=(\d+)')
STACK = re.compile(r'deep-stack turn=(\d+) player="([^"]*)" sites=(\d+) '
                   r'turns=(\d+) wturns=(\d+) stk=(\d+) n=(\S+) '
                   r'done=(\d+) dsolo=(\d+) dstk=(\d+) tsolo=(\d+) '
                   r'tstk=(\d+) lost=(\d+) over=(\d+)')

WORK = re.compile(r'deep-workturn turn=(\d+) player="([^"]*)" workers=(\d+) '
                  r'base=(\d+) road=(\d+) irrig=(\d+) mine=(\d+) '
                  r'transform=(\d+) clean=(\d+) travel=(\d+) idle=(\d+) '
                  r'other=(\d+)')

GATE_KEYS = ('seen', 'offered', 'declined', 'dec_idle', 'dec_late',
             'dec_part', 'picked', 'saved')
WORK_KEYS = ('base', 'road', 'irrig', 'mine', 'transform', 'clean',
             'travel', 'idle', 'other')
STACK_KEYS = ('sites', 'turns', 'wturns', 'stk', 'done', 'dsolo', 'dstk',
              'tsolo', 'tstk', 'lost', 'over')


def read_game(path):
    tot = collections.Counter()
    last = {}
    lastwork = {}
    with open(path, errors='replace') as fh:
        for line in fh:
            m = GATE.search(line)
            if m:
                for i, k in enumerate(GATE_KEYS):
                    tot[k] += int(m.group(3 + i))
                tot['gatelines'] += 1
                continue
            m = STACK.search(line)
            if m:
                last[m.group(2)] = m
                continue
            m = WORK.search(line)
            if m:
                lastwork[m.group(2)] = m
    for m in lastwork.values():
        for i, k in enumerate(WORK_KEYS):
            tot['w_' + k] += int(m.group(4 + i))
    hist = [0] * 5
    for m in last.values():
        # Group 7 is the n= histogram string, so every key after `stk`
        # sits one group further along than its index.
        for i, k in enumerate(STACK_KEYS):
            tot[k] += int(m.group(3 + i if i < 4 else 4 + i))
        for i, v in enumerate(m.group(7).split(':')):
            hist[i] += int(v)
    tot['players'] = len(last)
    for i, v in enumerate(hist):
        tot['n%d' % (i + 1)] = v
    return tot


def collect(root):
    logs = sorted(glob.glob(os.path.join(root, '**', 'game.log'),
                            recursive=True))
    logs = [lg for lg in logs
            if os.path.exists(os.path.join(os.path.dirname(lg), 'DONE'))]
    games = []
    for lg in logs:
        g = read_game(lg)
        g['seed'] = os.path.basename(os.path.dirname(lg))
        games.append(g)
    return games


def report(name, games):
    n = len(games)
    if n == 0:
        print('  %s: no completed games' % name)
        return
    tot = collections.Counter()
    for g in games:
        for k, v in g.items():
            if k != 'seed':
                tot[k] += v
    print('  %s (%d games, %d deep player-games)'
          % (name, n, tot['players']))
    print('    gate     seen=%.1f offered=%.1f declined=%.1f'
          ' (idle %.1f, late %.1f, partial %.1f) picked=%.1f saved=%.1f'
          '  (per game)'
          % tuple(tot[k] / n for k in GATE_KEYS))
    if tot['seen']:
        print('    gate     offered/seen %.1f%%  picked/offered %s'
              % (100.0 * tot['offered'] / tot['seen'],
                 ('%.1f%%' % (100.0 * tot['picked'] / tot['offered']))
                 if tot['offered'] else 'n/a'))
    print('    sites    opened=%.1f turns=%.1f wturns=%.1f'
          ' n=%s' % (tot['sites'] / n, tot['turns'] / n, tot['wturns'] / n,
                     ':'.join('%.1f' % (tot['n%d' % i] / n)
                              for i in range(1, 6))))
    if tot['turns']:
        print('    stacked  %.1f worksite-turns a game, %.2f%% of all'
              % (tot['stk'] / n, 100.0 * tot['stk'] / tot['turns']))
    print('    done     %.1f (solo %.1f, stacked %.1f) lost=%.1f'
          % (tot['done'] / n, tot['dsolo'] / n, tot['dstk'] / n,
             tot['lost'] / n))
    print('    turns/completion  solo %s  stacked %s'
          % (('%.2f' % (tot['tsolo'] / tot['dsolo'])) if tot['dsolo']
             else 'n/a',
             ('%.2f' % (tot['tstk'] / tot['dstk'])) if tot['dstk']
             else 'n/a'))
    if tot['over']:
        print('    OVERFLOW %d worksites went unrecorded' % tot['over'])
    wtot = sum(tot['w_' + k] for k in WORK_KEYS)
    if wtot:
        # deep-workturn, the worker-turn split by class. Cumulative like
        # deep-stack, so the last line per player is the game's total.
        # It is the outside check on the census above: `road` and
        # `transform` here are worker-turns spent on the very jobs a
        # stack is supposed to shorten.
        print('    workturns %s'
              % '  '.join('%s=%.1f (%.1f%%)'
                          % (k, tot['w_' + k] / n,
                             100.0 * tot['w_' + k] / wtot)
                          for k in WORK_KEYS))


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for root in sys.argv[1:]:
        report(root, collect(root))


if __name__ == '__main__':
    main()
