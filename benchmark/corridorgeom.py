#!/usr/bin/env python3
"""Corridor geometry as it stands on the ground, from final savegames.

    corridorgeom.py SAVE [SAVE...]

corridorcensus.py reads what deep PLANNED, out of its own log lines.
This reads the world the plan was made in, out of the savegame: how
much land the venue supplies, how far apart a deep empire's cities
actually sit, and how much of the ground between them already carries
a road. Those three numbers are what a corridor term's dose is made
of, and they are properties of the VENUE, not of the term.

It reimplements deep_trunk_build()'s Prim (map distance, edges only
between cities on one continent) over each deep player's cities, then
walks each edge with the same strictly-closing greedy step
deep_trunk_walk() uses and counts the path tiles without a road. The
walk does not reproduce deeptrunk's border clip for ally links, and
continents are flood-filled here rather than read from the save's own
component numbering, so treat the numbers as the venue's shape to
within a tile -- which is the resolution the question needs.

Columns: ncity/nedge per deep player, elen the mean MST edge in tiles,
gap/edge the tiles of it still unroaded, unfin how many edges are
unfinished, roaded% the share of all MST path tiles that carry a road,
homecont the mean size of the continent its cities sit on, land the
venue's total land tiles and biggest its largest continent.
"""
import re, subprocess, sys, statistics as st


def read_save(path):
    if path.endswith('.zst'):
        return subprocess.run(['zstd', '-dc', path], check=True,
                              stdout=subprocess.PIPE, text=True,
                              errors='replace').stdout
    return open(path, errors='replace').read()


def parse(save):
    m = re.search(r'^extras_vector=(.*)$', save, re.M)
    names = [x.strip('"') for x in m.group(1).split(',')]
    road_idx = [i for i, n in enumerate(names) if n in ('Road', 'Railroad')]

    trows = re.findall(r'^t(\d{4})="([^"]*)"$', save, re.M)
    ys = len(trows)
    xs = len(trows[0][1])

    plr_name, deep_plr, teams, ai_type = {}, set(), {}, {}
    for sec in re.finditer(r'^\[player(\d+)\]$(.*?)(?=^\[player\d+\]|\Z)',
                           save, re.M | re.S):
        n = int(sec.group(1)); body = sec.group(2)
        nm = re.search(r'^name="([^"]*)"', body, re.M)
        if nm:
            plr_name[n] = nm.group(1)
        at = re.search(r'^ai_type="?([a-z]+)"?', body, re.M)
        if at:
            ai_type[n] = at.group(1)
            if at.group(1) == 'deep':
                deep_plr.add(n)

    # terrain grid (map t rows) -> land/ocean via ruleset-ish char set
    terr = {}
    for row in re.finditer(r'^t(\d{4})="([^"]*)"$', save, re.M):
        y = int(row.group(1))
        for x, ch in enumerate(row.group(2)):
            terr[(x, y)] = ch

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

    owner = {}
    for row in re.finditer(r'^owner(\d{4})="([^"]*)"$', save, re.M):
        y = int(row.group(1))
        for x, cell in enumerate(row.group(2).split(',')):
            owner[(x, y)] = None if cell == '-' else int(cell)

    # cities: a c={"y","x",...} table per [player<N>] section
    cities = {}
    for sec in re.finditer(r'^\[player(\d+)\]$(.*?)(?=^\[|\Z)',
                           save, re.M | re.S):
        n = int(sec.group(1)); body = sec.group(2)
        m2 = re.search(r'^c=\{(.*?)^\}$', body, re.M | re.S)
        if not m2:
            continue
        lines = m2.group(1).split('\n')
        for ln in lines[1:]:
            ln = ln.strip()
            if not ln:
                continue
            f = ln.split(',')
            try:
                y, x = int(f[0]), int(f[1])
            except ValueError:
                continue
            cities.setdefault(n, []).append((x, y))
    return dict(xs=xs, ys=ys, terr=terr, roaded=roaded, owner=owner,
                cities=cities, deep=deep_plr, name=plr_name, ai=ai_type)


# classic terrain.ruleset identifiers: Ocean " ", Deep Ocean ":",
# Lake "+", Inaccessible "i". Everything else is land.
OCEAN = set(' :+i')


def is_land(terr, xy):
    ch = terr.get(xy)
    return ch is not None and ch not in OCEAN


def dist(a, b):
    return max(abs(a[0] - b[0]), abs(a[1] - b[1]))


def continents(terr, xs, ys):
    """Flood-fill land continents; returns (id_by_tile, size_by_id)."""
    cid, sizes, nxt = {}, {}, 1
    for y in range(ys):
        for x in range(xs):
            if not is_land(terr, (x, y)) or (x, y) in cid:
                continue
            stack, n = [(x, y)], 0
            cid[(x, y)] = nxt
            while stack:
                cx, cy = stack.pop(); n += 1
                for dx in (-1, 0, 1):
                    for dy in (-1, 0, 1):
                        p = ((cx + dx) % xs, cy + dy)
                        if p[1] < 0 or p[1] >= ys or p in cid:
                            continue
                        if is_land(terr, p):
                            cid[p] = nxt
                            stack.append(p)
            sizes[nxt] = n
            nxt += 1
    return cid, sizes


def walk(a, b):
    """Greedy strictly-closing step walk, like deep_trunk_walk()."""
    path, cur = [], a
    guard = 0
    while cur != b and guard < 200:
        guard += 1
        dx = b[0] - cur[0]
        dy = b[1] - cur[1]
        sx = (dx > 0) - (dx < 0)
        sy = (dy > 0) - (dy < 0)
        cur = (cur[0] + sx, cur[1] + sy)
        if cur != b:
            path.append(cur)
    return path


def mst(nodes, cid):
    """Prim on map distance, same continent only. Returns list of edges."""
    n = len(nodes)
    if n < 2:
        return []
    seen = [False] * n
    near_to = [-1] * n
    near_d = [10 ** 9] * n
    seen[0] = True
    for j in range(1, n):
        if cid.get(nodes[j]) == cid.get(nodes[0]):
            near_to[j] = 0
            near_d[j] = dist(nodes[0], nodes[j])
    edges = []
    for _ in range(1, n):
        pick, best = -1, 10 ** 9
        for j in range(n):
            if not seen[j] and near_d[j] < best:
                best, pick = near_d[j], j
        if pick < 0:
            for j in range(n):
                if not seen[j]:
                    pick = j
                    break
            if pick < 0:
                break
            near_to[pick] = -1
        seen[pick] = True
        if near_to[pick] >= 0:
            edges.append((nodes[near_to[pick]], nodes[pick]))
        for j in range(n):
            if not seen[j] and cid.get(nodes[j]) == cid.get(nodes[pick]):
                d = dist(nodes[pick], nodes[j])
                if d < near_d[j]:
                    near_d[j], near_to[j] = d, pick
    return edges


def main(paths):
    rows = []
    for p in paths:
        s = parse(read_save(p))
        cid, sizes = continents(s['terr'], s['xs'], s['ys'])
        land = sum(sizes.values())
        for pn in sorted(s['deep']):
            nodes = s['cities'].get(pn, [])
            if len(nodes) < 2:
                continue
            edges = mst(nodes, cid)
            lens, gaps, unroaded_beyond = [], [], []
            for a, b in edges:
                pa = walk(a, b)
                if not pa:
                    continue
                lens.append(len(pa))
                g = sum(1 for t in pa if t not in s['roaded'])
                gaps.append(g)
            csz = [sizes.get(cid.get(c), 0) for c in nodes]
            rows.append(dict(
                save=p, plr=pn, ncity=len(nodes), nedge=len(edges),
                edgelen=st.mean(lens) if lens else 0,
                gap=st.mean(gaps) if gaps else 0,
                unfin=sum(1 for g in gaps if g > 0),
                roadfrac=(1 - sum(gaps) / sum(lens)) if sum(lens) else 0,
                homecont=st.mean(csz) if csz else 0,
                land=land, biggest=max(sizes.values()) if sizes else 0))
    if not rows:
        print('no rows')
        return
    print('%-4s %-6s %6s %6s %8s %7s %7s %9s %9s %7s' %
          ('plr', 'ncity', 'nedge', 'elen', 'gap/edge', 'unfin',
           'roaded%', 'homecont', 'land', 'biggest'))
    for r in rows:
        print('p%-3d %6d %6d %8.2f %8.2f %7d %7.1f %9.0f %9d %7d' %
              (r['plr'], r['ncity'], r['nedge'], r['edgelen'], r['gap'],
               r['unfin'], 100 * r['roadfrac'], r['homecont'], r['land'],
               r['biggest']))

    def agg(k):
        return st.mean([r[k] for r in rows])
    print('MEAN over %d player-games: ncity %.2f  MST edges %.2f  '
          'edge len %.2f tiles  gap %.2f tiles/edge  unfinished %.2f  '
          'roaded %.1f%%  home continent %.0f tiles  land %.0f  '
          'biggest continent %.0f' %
          (len(rows), agg('ncity'), agg('nedge'), agg('edgelen'),
           agg('gap'), agg('unfin'), 100 * agg('roadfrac'),
           agg('homecont'), agg('land'), agg('biggest')))


if __name__ == '__main__':
    main(sys.argv[1:])
