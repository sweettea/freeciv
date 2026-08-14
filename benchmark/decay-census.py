#!/usr/bin/env python3
"""DECAY slice 1: offline flip-vs-rate curves from the shape census.

Input: game.log files (or extracted deep-decay* line files) from a
corpus run with the census logging built in.  Per build-pass decision
(game, turn, player, city) the census gives every horizon-credited
improvement's stream shapes plus the best uncredited competitor, so the
domestic argmax can be recomputed under two pricings:

  rectangle (the code's own): credit = b * steps * (1-1/MORT)^delay
  exponential at rate r:      credit = b * sum_i q^(delay + i*spacing),
                              q = 1 - r, i = 0..N-1
      N = infinity where the census marked the instalment count as
      horizon-derived (cap=-1: the MORT window was doing duty as a
      discount), N = cap where the cap is structural (effect amount,
      the endgame bell); lumps (steps=1, cap=1) are invariant.

The upkeep bill is a per-turn stream too and is re-priced the same way
(rect: upk*H; exp: upk/(1-q)).  gmult/threatened and the expiry keep
factor are replayed exactly as deep_adjust_building_wants() applies
them.  Stock's base want (pre) and uncredited competitors carry no
time shape and are invariant.

A decision "flips" at rate r when the argmax over that candidate set
differs from the rectangle argmax.  Output: flip share by rate x game
phase (T1-50 / 51-100 / 101+) and by family (the dominant credit
family of whichever winner is deep-credited), plus the validation
error of the rectangle reconstruction against the logged credits.

Decisions inside the endgame window (default: last 8 turns) are
dropped: deep_endgame_bias_buildings() rescales the array after the
census lines are emitted, so their reconstruction is not exact there.

Usage:
  decay-census.py [--rates r1,r2,...] [--endturn N] [--window N]
                  [--csv FILE] LOG...
"""

import argparse
import math
import re
import sys
from collections import defaultdict

MORT = 24
QM = 1.0 - 1.0 / MORT

LINE_RE = re.compile(r'^\d+: (deep-decay(?:-sum|-city)?) turn=(\d+) '
                     r'player="([^"]*)" city=(\d+) (.*)$')
KV_RE = re.compile(r'(\w+)=(?:"([^"]*)"|(\S+))')


def kvparse(rest):
    out = {}
    for m in KV_RE.finditer(rest):
        out[m.group(1)] = m.group(2) if m.group(2) is not None else m.group(3)
    return out


def rect_value(b, delay, steps):
    return b * steps * (QM ** delay)


def exp_value(b, delay, steps, spacing, cap, rate, left):
    """Exponential price of the same instalment stream.

    `left` truncates every stream at the scored end of the game (the
    brief's unified form: delay = leading zeros, the endgame clamp a
    truncated geometric tail).  Pass None for the pure-perpetuity
    variant (--no-bell).
    """
    q = 1.0 - rate
    if steps <= 1.0 and cap == 1:
        # Lump: invariant by construction.
        return b * steps
    if spacing <= 0:
        spacing = 1
    ratio = q ** spacing
    lead = q ** delay
    # Structural payment cap (effect amount, the bell the culture term
    # already counted); -1 = horizon-derived, no structural cap.
    n = cap if cap > 0 else None
    if left is not None:
        if delay > left:
            return 0.0
        nbell = (left - delay) // spacing + 1
        n = nbell if n is None else min(n, nbell)
    if n is None:
        return b * lead / (1.0 - ratio)
    return b * lead * (1.0 - ratio ** n) / (1.0 - ratio)


class Cred:
    __slots__ = ('fam', 'b', 'delay', 'steps', 'spacing', 'cap', 'credit')

    def __init__(self, kv):
        self.fam = kv['fam']
        self.b = float(kv['b'])
        self.delay = int(kv['delay'])
        self.steps = float(kv['steps'])
        self.spacing = int(kv['spacing'])
        self.cap = int(kv['cap'])
        self.credit = float(kv['credit'])


def phase_of(turn):
    if turn <= 50:
        return 'T1-50'
    if turn <= 100:
        return 'T51-100'
    return 'T101+'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rates', default='0.002,0.005,0.011,0.02,0.037,'
                    '0.041667,0.06,0.08,0.12')
    ap.add_argument('--endturn', type=int, default=200)
    ap.add_argument('--window', type=int, default=8,
                    help='drop decisions in the last N turns (endgame bias)')
    ap.add_argument('--csv', default=None)
    ap.add_argument('--no-bell', action='store_true',
                    help='price cap=-1 streams as pure perpetuities '
                    'instead of truncating at the scored end')
    ap.add_argument('logs', nargs='+')
    args = ap.parse_args()
    rates = [float(r) for r in args.rates.split(',')]

    # decisions[(log, turn, player, city)] =
    #   {'caps': [Cred...] (city capacity shapes),
    #    'imprs': {name: {'creds': [Cred], 'sum': kv}},
    #    'bestun': (name, want)}
    decisions = defaultdict(lambda: {'caps': [], 'imprs': {},
                                     'bestun': None})
    nlines = 0
    for path in args.logs:
        with open(path, errors='replace') as f:
            for line in f:
                m = LINE_RE.match(line)
                if not m:
                    continue
                tag, turn, player, city, rest = m.groups()
                turn = int(turn)
                if turn > args.endturn - args.window:
                    continue
                kv = kvparse(rest)
                key = (path, turn, player, int(city))
                d = decisions[key]
                nlines += 1
                if tag == 'deep-decay':
                    c = Cred(kv)
                    if kv['impr'] == '*':
                        d['caps'].append(c)
                    else:
                        d['imprs'].setdefault(kv['impr'],
                                              {'creds': [], 'sum': None})
                        d['imprs'][kv['impr']]['creds'].append(c)
                elif tag == 'deep-decay-sum':
                    d['imprs'].setdefault(kv['impr'],
                                          {'creds': [], 'sum': None})
                    d['imprs'][kv['impr']]['sum'] = kv
                elif tag == 'deep-decay-city':
                    if kv['bestun'] != '-':
                        d['bestun'] = (kv['bestun'], float(kv['bwant']))

    def reprice(entry, caps, rate, turn):
        """Final add under pricing; rate=None -> rectangle."""
        s = entry['sum']
        capadd = float(s['capadd'])
        left = args.endturn - turn
        total = 0.0
        creds = list(entry['creds'])
        if capadd > 0:
            creds = creds + caps
        for c in creds:
            if rate is None:
                total += rect_value(c.b, c.delay, c.steps)
            else:
                total += exp_value(c.b, c.delay, c.steps, c.spacing,
                                   c.cap, rate,
                                   None if args.no_bell else left)
        gmult = float(s['gmult'])
        thr = s['thr'] == '1'
        if total > 0 and gmult != 1.0:
            total = max(total, total * gmult) if thr else total * gmult
        upk = float(s['upk'])
        if total > 0:
            if rate is None:
                # The code charges the bill over deep_horizon_turns(),
                # which pv3's endgame_horizon clamps to the turns left
                # inside the final MORT window.
                total -= upk * min(MORT, left)
            else:
                total -= exp_value(upk, 0, MORT, 1, -1, rate,
                                   None if args.no_bell else left)
            total = max(total, 0.0)
        keep = float(s['keep'])
        if total > 0 and int(s['ob']) >= 0:
            total *= keep
        return total

    # Validation: rectangle reconstruction vs logged add.
    val_n = val_bad = 0
    val_relerr = 0.0
    flips = {r: defaultdict(lambda: [0, 0]) for r in rates}  # phase -> [flip, n]
    famflips = {r: defaultdict(lambda: [0, 0]) for r in rates}
    ndec = 0
    for key, d in decisions.items():
        imprs = {n: e for n, e in d['imprs'].items() if e['sum'] is not None}
        if not imprs:
            continue
        phase = phase_of(key[1])
        # Rectangle wants (and validation).
        rect_want = {}
        domfam = {}
        for name, e in imprs.items():
            s = e['sum']
            pre = float(s['pre'])
            logged_add = float(s['add'])
            recon = reprice(e, d['caps'], None, key[1])
            val_n += 1
            denom = max(abs(logged_add), 1.0)
            err = abs(recon - logged_add) / denom
            val_relerr += err
            if err > 0.02:
                val_bad += 1
            rect_want[name] = pre + max(logged_add, 0.0)
            creds = e['creds'] + (d['caps'] if float(s['capadd']) > 0 else [])
            domfam[name] = (max(creds, key=lambda c: c.credit).fam
                            if creds else 'none')
        cands = dict(rect_want)
        if d['bestun'] is not None and d['bestun'][0] not in cands:
            cands[d['bestun'][0]] = d['bestun'][1]
        if len(cands) < 2:
            continue
        ndec += 1
        rwin = max(cands, key=lambda n: cands[n])
        for r in rates:
            ew = {}
            for name in cands:
                if name in imprs:
                    ew[name] = (float(imprs[name]['sum']['pre'])
                                + reprice(imprs[name], d['caps'], r,
                                          key[1]))
                else:
                    ew[name] = cands[name]
            ewin = max(ew, key=lambda n: ew[n])
            flip = ewin != rwin
            flips[r][phase][1] += 1
            # Decision family: dominant credit family of the rectangle
            # winner ('uncredited' when stock's own want won).
            famr = domfam.get(rwin, 'uncredited')
            famflips[r][famr][1] += 1
            if flip:
                flips[r][phase][0] += 1
                famflips[r][famr][0] += 1

    print(f'# census lines: {nlines}, decisions with >=2 candidates: {ndec}')
    print(f'# validation: {val_n} credited imprs, mean rel err '
          f'{val_relerr / max(val_n, 1):.4f}, >2% err: {val_bad} '
          f'({100.0 * val_bad / max(val_n, 1):.2f}%)')
    print()
    phases = ['T1-50', 'T51-100', 'T101+']
    hdr = 'rate      ' + ''.join(f'{p:>12}' for p in phases) + f'{"all":>12}'
    print(hdr)
    rows = []
    for r in rates:
        cells = []
        tot = [0, 0]
        for p in phases:
            f, n = flips[r][p]
            tot[0] += f
            tot[1] += n
            cells.append(f'{100.0 * f / n:.1f}% n={n}' if n else '-')
        cells.append(f'{100.0 * tot[0] / tot[1]:.1f}%' if tot[1] else '-')
        print(f'{r:<10.4g}' + ''.join(f'{c:>12}' for c in cells))
        rows.append((r, [flips[r][p] for p in phases], tot))
    print()
    fams = sorted({f for r in rates for f in famflips[r] if f != 'all'})
    print('flip share by dominant family of the rect winner '
          '(denominator: decisions whose rect winner has that family):')
    print('rate      ' + ''.join(f'{f:>14}' for f in fams))
    for r in rates:
        cells = []
        for f in fams:
            fl, n = famflips[r][f]
            cells.append(f'{100.0 * fl / n:.1f}%/{n}' if n else '-')
        print(f'{r:<10.4g}' + ''.join(f'{c:>14}' for c in cells))

    if args.csv:
        with open(args.csv, 'w') as f:
            f.write('rate,phase,flips,decisions\n')
            for r, per, tot in rows:
                for p, (fl, n) in zip(phases, per):
                    f.write(f'{r},{p},{fl},{n}\n')
                f.write(f'{r},all,{tot[0]},{tot[1]}\n')
        print(f'\nwrote {args.csv}')


if __name__ == '__main__':
    sys.exit(main())
