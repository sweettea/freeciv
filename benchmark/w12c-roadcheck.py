#!/usr/bin/env python3
"""Did an ally corridor tile outside deep's borders actually get a road?

The counters in deep-trunkally say what deep PLANNED and what it
assigned. This says what is on the ground when the game ends, read out
of the final savegame rather than out of deep's own log of its
intentions:

  * every tile a `deep-trunkpick ... ally=1` line named (an assigned
    task on a corridor whose far endpoint is an allied city),
  * intersected with the tiles that carry a road extra at game end,
  * split by whether the tile's owner at game end is the deep player
    who worked it, someone else, or nobody.

The interesting number is roads on tiles NOT owned by that player:
those are tiles no city radius of deep's covers and no pre-slice-3
corridor could have reached.

Usage: w12c-roadcheck.py RESULTDIR    (a dir with game.log and saves/)
"""
import os
import re
import subprocess
import sys


def read_save(path):
    if path.endswith('.zst'):
        return subprocess.run(['zstd', '-dc', path], check=True,
                              stdout=subprocess.PIPE,
                              text=True, errors='replace').stdout
    with open(path, errors='replace') as f:
        return f.read()


def main(resdir):
    logp = os.path.join(resdir, 'game.log')
    saves = os.path.join(resdir, 'saves')
    if not os.path.isfile(logp) or not os.path.isdir(saves):
        print('%s: no game.log/saves' % resdir)
        return 1
    finals = [f for f in os.listdir(saves) if '-final.sav' in f]
    if not finals:
        print('%s: no final save' % resdir)
        return 1
    save = read_save(os.path.join(saves, finals[0]))

    # Which extras are roads that speed movement? Name-matched against
    # the classic ruleset's two, which is all this check needs to be
    # right about -- a false negative would only weaken the claim.
    m = re.search(r'^extras_vector=(.*)$', save, re.M)
    names = [x.strip('"') for x in m.group(1).split(',')]
    road_idx = [i for i, n in enumerate(names) if n in ('Road', 'Railroad')]

    # Player number by name, and which of them run ai/deep.
    plr_num, deep_plr = {}, set()
    for sec in re.finditer(r'^\[player(\d+)\]$(.*?)(?=^\[|\Z)', save,
                           re.M | re.S):
        n = int(sec.group(1))
        body = sec.group(2)
        nm = re.search(r'^name="([^"]*)"', body, re.M)
        if nm:
            plr_num[nm.group(1)] = n
        if re.search(r'^ai_type="?deep"?', body, re.M):
            deep_plr.add(n)

    # owner grid
    owner = {}
    for row in re.finditer(r'^owner(\d{4})="([^"]*)"$', save, re.M):
        y = int(row.group(1))
        for x, cell in enumerate(row.group(2).split(',')):
            owner[(x, y)] = None if cell == '-' else int(cell)

    # extras grid: group j holds extras 4j..4j+3, bit i = extra 4j+i
    roaded = set()
    for row in re.finditer(r'^e(\d{2})_(\d{4})="([^"]*)"$', save, re.M):
        j, y, bits = int(row.group(1)), int(row.group(2)), row.group(3)
        for idx in road_idx:
            if idx // 4 != j:
                continue
            bit = 1 << (idx % 4)
            for x, ch in enumerate(bits):
                if int(ch, 16) & bit:
                    roaded.add((x, y))

    # The ally corridor tasks deep actually assigned.
    picks = {}
    for line in open(logp, errors='replace'):
        m = re.search(r'deep-trunkpick .*player="([^"]*)".*'
                      r'tile=\((\d+),(\d+)\) dist=\d+ ally=1', line)
        if m:
            picks.setdefault(m.group(1), set()).add((int(m.group(2)),
                                                     int(m.group(3))))

    tag = os.path.basename(resdir.rstrip('/'))
    if not picks:
        print('%s: no ally-corridor picks in the log' % tag)
        return 1

    total_out = 0
    for pname, tiles in sorted(picks.items()):
        pn = plr_num.get(pname)
        mine = [t for t in tiles if owner.get(t) == pn]
        other = [t for t in tiles if owner.get(t) != pn]
        road_out = [t for t in other if t in roaded]
        total_out += len(road_out)
        print('%s %s(p%s): %d ally-corridor task tiles, %d inside own '
              'border, %d outside; roads at game end: %d inside, '
              '%d OUTSIDE %s'
              % (tag, pname, pn, len(tiles), len(mine), len(other),
                 len([t for t in mine if t in roaded]), len(road_out),
                 sorted(road_out)[:8]))
        for t in sorted(road_out)[:3]:
            o = owner.get(t)
            print('   tile %s road=yes owner=%s (%s)'
                  % (t, 'none' if o is None else 'p%d' % o,
                     'ALLY' if o is not None and o in deep_plr and o != pn
                     else 'unowned' if o is None else 'other'))
    return 0 if total_out > 0 else 1


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
