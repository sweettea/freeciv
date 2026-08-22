#!/usr/bin/env python3
"""Summarise the deep-wrace* instrument from one or more game.log files.

    wonder-report.py LABEL GLOB [LABEL GLOB ...]

Reads deep-wrace-city (per player-turn census), deep-wrace (per
candidate), deep-wrace-sum, deep-wrace-abandon and deep-wrace-end, and
reports the venue cell the wonder race-pass session needs: how often
there was a wonder city at all, how big it was, what it was actually
producing, how many wonders it finished, and how much of that is the
race pass's doing.
"""
import glob
import gzip
import re
import statistics
import sys

FIELD = re.compile(r'(\w+)=("([^"]*)"|-?[\d.]+)')


def parse(line, tag):
    i = line.find(tag + ' ')
    if i < 0:
        return None
    d = {}
    for m in FIELD.finditer(line[i + len(tag):]):
        d[m.group(1)] = m.group(3) if m.group(3) is not None else m.group(2)
    return d


def num(d, k, default=0.0):
    try:
        return float(d[k])
    except (KeyError, ValueError, TypeError):
        return default


def mean(xs):
    return statistics.mean(xs) if xs else float('nan')


def med(xs):
    return statistics.median(xs) if xs else float('nan')


def openlog(path):
    """game.log files are ~15 MB each and are kept gzipped after scoring."""
    if path.endswith('.gz'):
        return gzip.open(path, 'rt', errors='replace')
    return open(path, errors='replace')


def cell(label, pattern):
    logs = sorted(glob.glob(pattern) + glob.glob(pattern + '.gz'))
    if not logs:
        print(f'--- {label}: NO LOGS ({pattern})')
        return
    games = 0
    fin = []                    # final census per seat
    turns_nocity = turns_city = turns_building = 0
    wsize, wsurplus, cands, bestwant, factors = [], [], [], [], []
    priced = declined = abandoned = 0
    own_eta = []
    contested = uncontested = 0
    ends_won = ends_lost = lost_hi_p = 0
    starts = completions = interruptions = 0
    stock_dropped = []

    for path in logs:
        games += 1
        prev = {}
        for line in openlog(path):
            if 'deep-wrace' not in line:
                continue
            d = parse(line, 'deep-wrace-city')
            if d is not None:
                plr = d['player']
                p0 = prev.get(plr)
                if d['city'] == '-1':
                    turns_nocity += 1
                else:
                    turns_city += 1
                    if d['building'] == '1':
                        turns_building += 1
                    wsize.append(num(d, 'size'))
                    wsurplus.append(num(d, 'surplus'))
                    cands.append(num(d, 'cands'))
                    bestwant.append(num(d, 'bestwant'))
                if p0 is not None:
                    was = p0['building'] == '1'
                    now = d['building'] == '1'
                    got = num(d, 'own') - num(p0, 'own')
                    completions += max(0, int(got))
                    if now and not was:
                        starts += 1
                    if was and not now and got <= 0:
                        interruptions += 1
                        stock_dropped.append(num(p0, 'stock'))
                prev[plr] = d
                continue
            d = parse(line, 'deep-wrace-sum')
            if d is not None:
                priced += int(num(d, 'priced'))
                declined += int(num(d, 'declined'))
                abandoned += int(num(d, 'abandoned'))
                continue
            d = parse(line, 'deep-wrace-end')
            if d is not None:
                if d['won'] == '1':
                    ends_won += 1
                else:
                    ends_lost += 1
                    if num(d, 'p') >= 0.75:
                        lost_hi_p += 1
                continue
            if parse(line, 'deep-wrace-abandon') is not None:
                continue
            d = parse(line, 'deep-wrace')
            if d is not None:
                if num(d, 'ours', -1) >= 0:
                    own_eta.append(num(d, 'ours'))
                if num(d, 'theirs', -1) >= 0:
                    contested += 1
                else:
                    uncontested += 1
                factors.append(num(d, 'factor', 1.0))
        fin.extend(prev.values())

    seats = len(fin)
    turns = turns_city + turns_nocity
    print(f'--- {label}   ({games} games, {seats} deep seats)')
    print(f'  final: cities {mean([num(d,"cities") for d in fin]):.2f}   '
          f'wonders OURS {mean([num(d,"own") for d in fin]):.2f}   '
          f'rivals {mean([num(d,"rival") for d in fin]):.2f}   '
          f'unbuilt {mean([num(d,"open") for d in fin]):.2f}')
    print(f'  wonder city:  present on {turns_city}/{turns} player-turns '
          f'({100.0*turns_nocity/max(1,turns):.1f}% with NO wonder city)')
    print(f'                building a great wonder on {turns_building} '
          f'({100.0*turns_building/max(1,turns_city):.1f}% of present turns)')
    print(f'                size mean {mean(wsize):.2f} median {med(wsize):.0f}'
          f'   shield surplus mean {mean(wsurplus):.2f} '
          f'median {med(wsurplus):.0f}')
    print(f'  admission:    eligible {mean([num(d,"eligible") for d in fin]):.2f}'
          f' of {mean([num(d,"cities") for d in fin]):.2f} cities   '
          f'(grave_danger rejects {mean([num(d,"danger") for d in fin]):.2f})')
    print(f'  candidates:   {mean(cands):.2f}/turn   best pre-damp want '
          f'mean {mean(bestwant):.1f}')
    print(f'  race pass:    priced {priced}  declined {declined} '
          f'({100.0*declined/max(1,priced+declined):.1f}%)  abandoned '
          f'{abandoned}')
    print(f'                factor mean {mean(factors):.3f}  min '
          f'{min(factors) if factors else float("nan"):.3f}   '
          f'(1.000 = the pass changed nothing)')
    print(f'  rivals seen:  {contested} contested / '
          f'{contested+uncontested} evaluations '
          f'({100.0*contested/max(1,contested+uncontested):.1f}%)')
    print(f'  own ETA:      mean {mean(own_eta):.1f} median {med(own_eta):.0f} '
          f'turns')
    print(f'  build course: starts {starts}   completions {completions}   '
          f'interruptions {interruptions} '
          f'(mean stock abandoned {mean(stock_dropped):.1f})')
    print(f'  races closed: won {ends_won}  lost {ends_lost}  '
          f'(lost at p>=0.75: {lost_hi_p})')


if __name__ == '__main__':
    a = sys.argv[1:]
    for i in range(0, len(a), 2):
        cell(a[i], a[i + 1])
