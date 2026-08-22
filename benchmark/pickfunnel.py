#!/usr/bin/env python3
"""The corridor funnel as far as artefacts that PREDATE deepfunnel.c
can carry it.

    pickfunnel.py RESULTDIR/<arm>

The `deep-trunkpick` line has carried (turn, player, unit, tile, dist)
since W12 slice 2, and run-game.sh has been keeping final savegames for
longer than that. Between them they answer three of the funnel's stages
on games already on disk, at no cost in games:

  * how many DISTINCT tiles the picks are spread over, and how many
    separate times each is taken up (an EPISODE is a maximal run of
    consecutive turns in which one worker holds one tile),
  * how often a worker abandons a tile it is still walking to, which is
    visible as a next-turn pick of a different tile at dist > 0,
  * and, from the final savegame, how many of the tiles the game ever
    wanted a road on actually carry one at the end.

The last of those is an INDEPENDENT read of deepfunnel's `done`: it
comes from the world rather than from the ledger, so the two disagreeing
would mean the ledger is wrong.

What it cannot say is WHY an assignment ended -- that needs the counters,
because the reason is only visible at the site that dropped it.
"""
import re
import sys
import glob
import os
import subprocess
import collections

PICK = re.compile(r'deep-trunkpick turn=(\d+) player="([^"]*)" unit=(\d+) '
                  r'at=\((\d+),(\d+)\) tile=\((\d+),(\d+)\) dist=(\d+)')


def roaded_set(path):
    """Tiles carrying a movement road in a final savegame, in the MAP
    coordinates a log line prints.

    Two decodes, both from the server's own definitions. The extras
    bitfield is the one corridorgeom.py uses: extras_vector names the
    extras in bit order and each e<nibble>_<y> row spells four of them
    per hex digit. The coordinates then have to be converted, because a
    savegame row is in NATIVE coordinates (savegame3.c calls
    index_to_native_pos() before writing every position) while TILE_XY,
    and so every tile= in a log line, is in MAP coordinates. On an
    isometric topology -- which corridor-150 has, `"topology","ISO|HEX"`
    in its own save -- the two differ, and comparing them directly puts
    a fifth of the picked tiles off the map and another eighth in the
    ocean. NATIVE_TO_MAP_POS (common/map.h:170) is the conversion.
    """
    if path.endswith('.zst'):
        save = subprocess.run(['zstd', '-dc', path], check=True,
                              stdout=subprocess.PIPE, text=True,
                              errors='replace').stdout
    else:
        save = open(path, errors='replace').read()
    m = re.search(r'^extras_vector=(.*)$', save, re.M)
    if m is None:
        return None
    names = [x.strip('"') for x in m.group(1).split(',')]
    road_idx = [i for i, n in enumerate(names) if n in ('Road', 'Railroad')]
    iso = re.search(r'^"topology","[^"]*ISO[^"]*"', save, re.M) is not None
    width = max(len(r.group(2))
                for r in re.finditer(r'^t(\d{4})="([^"]*)"$', save, re.M))

    def to_map(nat_x, nat_y):
        if not iso:
            return nat_x, nat_y
        map_x = (nat_y + (nat_y & 1)) // 2 + nat_x
        return map_x, nat_y - map_x + width

    roaded = set()
    for row in re.finditer(r'^e(\d{2})_(\d{4})="([^"]*)"$', save, re.M):
        j, y, bits = int(row.group(1)), int(row.group(2)), row.group(3)
        for idx in road_idx:
            if idx // 4 != j:
                continue
            bit = 1 << (idx % 4)
            for x, ch in enumerate(bits):
                if int(ch, 16) & bit:
                    roaded.add(to_map(x, y))
    return roaded


def read_picks(path):
    per = collections.defaultdict(list)
    n = 0
    for line in open(path, errors='replace'):
        m = PICK.search(line)
        if m:
            t, p, u, _ax, _ay, tx, ty, d = m.groups()
            per[(p, int(u))].append((int(t), (int(tx), int(ty)), int(d)))
            n += 1
    return n, per


def churn(per):
    epis = renew = walkoff = resume = ontile = 0
    for seq in per.values():
        prev = None
        for t, tile, d in seq:
            if d == 0:
                ontile += 1
            if prev is None or prev[0] != t - 1 or prev[1] != tile:
                epis += 1
            else:
                renew += 1
            if prev is not None and prev[0] == t - 1 and prev[1] != tile \
                    and prev[2] > 0:
                walkoff += 1
            if prev is not None and prev[0] < t - 1:
                resume += 1
            prev = (t, tile, d)
    return epis, renew, walkoff, resume, ontile


def main():
    dirs = sorted(glob.glob(os.path.join(sys.argv[1], 'seed-*')))
    if not dirs:
        sys.exit('no seed-* dirs under %s' % sys.argv[1])

    cols = ('picks', 'tiles', 'epis', 'renew', 'walkoff', 'resume',
            'ontile', 'workers', 'roaded')
    print('%-5s' % 'seed' + ''.join('%9s' % c for c in cols))
    tot = collections.Counter()
    ngame = nsave = 0
    for d in dirs:
        lg = os.path.join(d, 'game.log')
        if not os.path.exists(lg):
            continue
        n, per = read_picks(lg)
        if n == 0:
            continue
        tiles = {(p, tile) for (p, _u), seq in per.items()
                 for _t, tile, _d in seq}
        epis, renew, walkoff, resume, ontile = churn(per)
        sv = glob.glob(os.path.join(d, 'saves', '*final*'))
        rs = roaded_set(sv[0]) if sv else None
        hit = (sum(1 for _p, xy in tiles if xy in rs)
               if rs is not None else -1)
        if hit >= 0:
            nsave += 1
            tot['roaded'] += hit
            tot['savetiles'] += len(tiles)
        ngame += 1
        row = dict(picks=n, tiles=len(tiles), epis=epis, renew=renew,
                   walkoff=walkoff, resume=resume, ontile=ontile,
                   workers=len(per), roaded=hit)
        for k in cols:
            if k != 'roaded':
                tot[k] += row[k]
        print('%-5s' % os.path.basename(d).replace('seed-', '')
              + ''.join('%9d' % row[c] for c in cols))
    if ngame == 0:
        sys.exit('no game carried a deep-trunkpick line')
    print('%-5s' % 'MEAN'
          + ''.join('%9.1f' % (tot[c] / ngame) for c in cols[:-1])
          + '%9.1f' % (tot['roaded'] / max(nsave, 1)))
    print()
    print('picks per distinct tile        %6.2f'
          % (tot['picks'] / max(tot['tiles'], 1)))
    print('episodes per distinct tile     %6.2f'
          % (tot['epis'] / max(tot['tiles'], 1)))
    print('picks that renew yesterday     %5.1f%%'
          % (100.0 * tot['renew'] / max(tot['picks'], 1)))
    print('picks abandoned while walking  %5.1f%%'
          % (100.0 * tot['walkoff'] / max(tot['picks'], 1)))
    if nsave:
        print('wanted tiles roaded at the end %5.1f%%  (%d of %d, %d saves)'
              % (100.0 * tot['roaded'] / max(tot['savetiles'], 1),
                 tot['roaded'], tot['savetiles'], nsave))


main()
