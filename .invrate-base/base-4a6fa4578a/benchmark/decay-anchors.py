"""Measure the two honest anchors for an exponential want discount from
the existing scorelog corpus:
  g      = empire shield-output growth rate per turn (tag 2, mfg)
  hazard = city-loss rate per city-turn (downticks of tag 3, cities)
Both split by game phase and by AI side where names allow it."""
import glob, math, os, sys
from collections import defaultdict

CORPUS = []
CORPUS += glob.glob('/home/sweettea/freeciv/benchmark/results/muster-ab/*/evaluate/cfg0/mixed_*/seed-*/scorelog.txt')
CORPUS += ['/home/sweettea/freeciv/derby4/saves/scorelog.txt',
           '/home/sweettea/freeciv/derby5/saves-0813/scorelog.txt',
           '/home/sweettea/freeciv/derby6/saves-0813/scorelog.txt']
CORPUS = [p for p in CORPUS if os.path.exists(p)]

PHASES = [(1, 50), (51, 100), (101, 150), (151, 200), (201, 300)]

def side_of(name):
    if name.startswith('Deep'): return 'deep'
    if name.startswith('Def') or name.startswith('P'):  # P-odd handled below
        return 'stock'
    return None

# growth: per player, per phase, mean dlog(mfg) over consecutive turns
glogs = defaultdict(list)     # (side, phase) -> list of dlog samples
loss = defaultdict(int)       # (side, phase) -> city loss events (downtick sum)
expo = defaultdict(int)       # (side, phase) -> city-turns of exposure

def phase_of(t):
    for i, (a, b) in enumerate(PHASES):
        if a <= t <= b: return i
    return None

for path in CORPUS:
    names, mixed = {}, 'mixed_' in path
    deepseats = set()
    if mixed:
        cs = os.path.join(os.path.dirname(path), 'commands.serv')
        try:
            for line in open(cs):
                if line.startswith('create ') and line.split()[2] == 'deep':
                    deepseats.add(line.split()[1])
        except FileNotFoundError:
            continue
    mfg = defaultdict(dict)   # pl -> {turn: v}
    cty = defaultdict(dict)
    for line in open(path, errors='replace'):
        p = line.split(' ')
        if p[0] == 'addplayer':
            names[int(p[2])] = ' '.join(p[3:]).strip().strip('"')
        elif p[0] == 'data' and len(p) >= 5:
            try: t, tag, pl, v = int(p[1]), int(p[2]), int(p[3]), int(p[4])
            except ValueError: continue
            if tag == 2: mfg[pl][t] = v
            elif tag == 3: cty[pl][t] = v
    for pl, series in mfg.items():
        nm = names.get(pl, '')
        side = ('deep' if nm in deepseats else 'stock') if mixed else side_of(nm)
        if side is None: continue
        turns = sorted(series)
        for a, b in zip(turns, turns[1:]):
            if b - a != 1: continue
            ph = phase_of(b)
            if ph is None: continue
            va, vb = series[a], series[b]
            if va >= 3 and vb >= 3:   # skip dead/newborn noise
                glogs[(side, ph)].append(math.log(vb / va))
            ca, cb = cty[pl].get(a), cty[pl].get(b)
            if ca is not None and cb is not None and ca > 0:
                expo[(side, ph)] += ca
                if cb < ca:
                    loss[(side, ph)] += ca - cb

print(f'corpus: {len(CORPUS)} scorelogs')
print(f"{'phase':>10} | {'g deep':>8} {'g stock':>8} | {'hz deep':>8} {'hz stock':>8} | {'rate deep':>9} {'rate stock':>10}")
for i, (a, b) in enumerate(PHASES):
    row = []
    for side in ('deep', 'stock'):
        gs = glogs[(side, i)]
        g = sum(gs) / len(gs) if gs else float('nan')
        hz = loss[(side, i)] / expo[(side, i)] if expo[(side, i)] else float('nan')
        row.append((g, hz))
    print(f"T{a:>3}-{b:<4}  | {row[0][0]:>8.4f} {row[1][0]:>8.4f} | {row[0][1]:>8.4f} {row[1][1]:>8.4f} | {row[0][0]+row[0][1]:>9.4f} {row[1][0]+row[1][1]:>10.4f}")
print()
print(f"reference: 1/MORT = {1/24:.4f}/turn (stock amortize, half-life {math.log(2)/ (1/24):.0f} turns)")
for side in ('deep', 'stock'):
    gs = [x for i in range(len(PHASES)) for x in glogs[(side, i)]]
    L = sum(loss[(side, i)] for i in range(len(PHASES)))
    E = sum(expo[(side, i)] for i in range(len(PHASES)))
    g = sum(gs) / len(gs); hz = L / E
    r = g + hz
    print(f"{side}: overall g={g:.4f} hazard={hz:.4f} -> rate={r:.4f}/turn, half-life {math.log(2)/r:.1f} turns, samples g:{len(gs)} exposure:{E}")
