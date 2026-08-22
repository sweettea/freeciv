#!/usr/bin/env python3
"""How big a corridor DOSE does a venue deliver?

    corridorcensus.py RESULTDIR [--arm deep]

The three W12 score nulls on `lategame-80` were all read as verdicts on
the terms. This reads them as a question about the venue instead: a
corridor term can only be worth what the corridors are, and this prints
what they are, per seed and in aggregate, out of `deep-trunk*` census
lines already in every game log.

Columns, all per PLAYER-TURN unless the name says otherwise:

    links       corridors the plan spans that are not yet finished
                (deeptrunk.c only records a link with gap > 0)
    gap         corridor tiles still without a road, summed over links
    gap/link    THE DOSE: how many tiles of road one corridor is short.
                On lategame-80 this is 1.75.
    hits        candidate tiles the trunk want reached
    beyond      corridor tiles outside every own city radius -- the
                demand W12 slice 2's task source exists to serve
    ally-lnk    corridors whose far endpoint is an allied city
    ally-gap/l  the same dose for ally corridors (1.14 on lategame-80)

and per GAME:

    picks       beyond-ceiling tasks actually assigned (the treatment
                dose the A/B is really applying)
    ally-pick   how many of those were on an ally corridor
    div-pick    how many were on a corridor carrying a trade dividend
    div-flip    picks the dividend FLIPPED, against the source's own
                dividend-free counterfactual

The last block is the one a power calculation wants: `picks` mean and
sd across seeds. A treatment whose own dose has a coefficient of
variation near 1 is being delivered at wildly different strengths from
seed to seed, and no amount of n fixes a dose that is near zero in a
quarter of the games.
"""
import os
import re
import statistics as st
import sys

TRUNK = re.compile(r'deep-trunk turn=(\d+) player="([^"]+)" links=(\d+) '
                   r'gap=(\d+) hits=(\d+) maxwant=([\d.]+)')
ALLY = re.compile(r'deep-trunkally turn=(\d+) player="([^"]+)" links=(\d+) '
                  r'gap=(\d+) offered=(\d+) picked=(\d+)')
SRC = re.compile(r'deep-trunksrc turn=(\d+) player="([^"]+)" beyond=(\d+) '
                 r'offered=(\d+) picked=(\d+) hist=(\S+)')
DIV = re.compile(r'deep-trunkdiv turn=(\d+) player="([^"]+)" pairs=(\d+) '
                 r'links=(\d+) maxdiv=([\d.]+) picked=(\d+) flipped=(\d+)')


def game(logpath):
    links, gaps, hits = [], [], []
    ally_l, ally_g, ally_pick = [], [], 0
    beyond, picks, hist = [], 0, None
    div_pick = div_flip = 0
    with open(logpath, errors='replace') as fh:
        for line in fh:
            m = TRUNK.search(line)
            if m:
                links.append(int(m.group(3)))
                gaps.append(int(m.group(4)))
                hits.append(int(m.group(5)))
                continue
            m = ALLY.search(line)
            if m:
                ally_l.append(int(m.group(3)))
                ally_g.append(int(m.group(4)))
                ally_pick += int(m.group(6))
                continue
            m = SRC.search(line)
            if m:
                beyond.append(int(m.group(3)))
                picks += int(m.group(5))
                h = [int(x) for x in m.group(6).split(':')]
                if hist is None:
                    hist = [0] * len(h)
                for i, v in enumerate(h):
                    hist[i] += v
                continue
            m = DIV.search(line)
            if m:
                div_pick += int(m.group(6))
                div_flip += int(m.group(7))

    def avg(x):
        return st.mean(x) if x else 0.0

    def per(a, b):
        return sum(a) / sum(b) if sum(b) else 0.0

    return dict(turns=len(links), links=avg(links), gap=avg(gaps),
                gapl=per(gaps, links), hits=avg(hits), beyond=avg(beyond),
                allyl=avg(ally_l), allygapl=per(ally_g, ally_l),
                picks=picks, allypick=ally_pick, divpick=div_pick,
                divflip=div_flip, hist=hist)


KEYS = [('links', 'links'), ('gap', 'gap'), ('gapl', 'gap/link'),
        ('hits', 'hits'), ('beyond', 'beyond'), ('allyl', 'ally-lnk'),
        ('allygapl', 'ally-gap/l'), ('picks', 'picks'),
        ('allypick', 'ally-pick'), ('divpick', 'div-pick'),
        ('divflip', 'div-flip')]


def main(root, arm='deep'):
    seeds = {}
    base = os.path.join(root, arm)
    if not os.path.isdir(base):
        base = root
    for name in sorted(os.listdir(base)):
        if not name.startswith('seed-'):
            continue
        log = os.path.join(base, name, 'game.log')
        if os.path.isfile(log):
            seeds[int(name.split('-')[1])] = game(log)
    if not seeds:
        print('no game logs under %s' % base)
        return 1

    print('%s  (%d games, %s player-turns of plan each on average)'
          % (root, len(seeds),
             round(st.mean([s['turns'] for s in seeds.values()]))))
    print('seed ' + ' '.join('%10s' % lbl for _, lbl in KEYS))
    for n in sorted(seeds):
        print('%4d ' % n
              + ' '.join('%10.2f' % seeds[n][k] for k, _ in KEYS))
    print('MEAN ' + ' '.join(
        '%10.2f' % st.mean([seeds[n][k] for n in seeds]) for k, _ in KEYS))
    if len(seeds) > 1:
        print('SD   ' + ' '.join(
            '%10.2f' % st.stdev([seeds[n][k] for n in seeds])
            for k, _ in KEYS))
        pk = [seeds[n]['picks'] for n in seeds]
        m = st.mean(pk)
        print('picks: mean %.1f sd %.1f  cv %.2f  zero-or-near '
              '(<= 10%% of mean) in %d/%d seeds'
              % (m, st.stdev(pk), (st.stdev(pk) / m) if m else float('nan'),
                 sum(1 for v in pk if v <= 0.1 * m), len(pk)))

    tot = None
    for n in seeds:
        h = seeds[n]['hist']
        if not h:
            continue
        if tot is None:
            tot = [0] * len(h)
        for i, v in enumerate(h):
            tot[i] += v
    if tot:
        print('pick distance histogram (bucket = map tiles from the '
              'worker, last = overflow): %s' % tot)
    return 0


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    a = 'deep'
    if '--arm' in sys.argv:
        a = sys.argv[sys.argv.index('--arm') + 1]
        args = [x for x in args if x != a]
    if not args:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(args[0], a))
