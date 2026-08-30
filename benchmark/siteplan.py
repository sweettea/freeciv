#!/usr/bin/env python3
"""Tables for the S-SITEPLAN 2x2 (founder site cap x founder run hold).

  siteplan.py DIR              every venue under DIR (war/, econ/)
  siteplan.py DIR/war          one venue

DIR/<venue>/<cell>/seed-N/ holds one finished game each; cells are c00
(today), c10 (this epic's two gates), c01 (founder_run_hold alone) and
c11 (both). Every contrast is PAIRED against c00 on the same seed, which
is the same map -- run-game.sh sets mapseed = gameseed = seed -- so the
map cancels out of every difference reported here.

The score column is the benchmark objective tune.py optimises: every
seat's score at its last logged turn, summed, dead seats frozen where
they died. All seats are deep in both venues, so it is the deep
population's total and not a margin over an opponent.

The census columns come from deep's own per-player-per-turn lines:

  found/alive       deep-expand (cities founded, founders standing).
                    The same line's born= is NOT read: a unit built in
                    end-turn processing has a birth_turn the next
                    census no longer matches, so it undercounts about
                    four to one. Settlers delivered come from
                    deep-prodswitch builtlast=1, the corrected census
                    S-PRODKEEP settled on.
  joins             deep-join ... done=1, the irreversible act
  guard             deep-joinguard held=1 -- emitted in BOTH arms, so
                    the c00 column is the counterfactual: how many joins
                    the guard WOULD have declined had it been on
  cap               deep-sitecap capped=1, the same for the bid
  sites             deep_remaining_sites(), mean over player-turns
"""
import re
import sys
from pathlib import Path
from statistics import mean, stdev

sys.path.insert(0, str(Path(__file__).resolve().parent))
from scorelog import Scorelog                          # noqa: E402

CELLS = ("c00", "c10", "c01", "c11")

R_EXPAND = re.compile(r'deep-expand turn=(\d+) player="[^"]+" .*'
                      r' born=(\d+) alive=(\d+) found=(\d+) sites=(\S+)')
R_JOIN = re.compile(r'deep-join turn=\d+ .* done=1')
R_BUILT = re.compile(r'deep-prodswitch turn=\d+ .*from="([^"]*)" to="([^"]*)"'
                     r'.* builtlast=(\d)')
R_GUARD = re.compile(r'deep-joinguard turn=\d+ .* held=(\d) gate=(\d)')
R_CAP = re.compile(r'deep-sitecap turn=\d+ .* capped=(\d) gate=(\d)')
R_LAD = re.compile(r'deep-siteladder turn=\d+ .* found=(\d) noterr=\d+'
                   r' nomin=\d+ nomap=\d+ nolegal=\d+ offbridge=\d+'
                   r' offpeace=(\d+) tbskip=(\d+) tbown=(\d+)'
                   r' tbunit=\d+ tbland=(\d+)')


def gamelog(d):
    p = d / "game.log"
    return p.read_text(errors="replace") if p.exists() else ""


def game_metrics(d, marks):
    """One game's row: scorelog cuts plus the deep census counters."""
    sl = Scorelog(d / "scorelog.txt")
    end = sl.last_turn
    row = {"score": sl.frozen_total("score", end) or 0}
    nseat = max(1, len(sl.series.get("cities", {})))
    for t in marks + [end]:
        key = "end" if t == end else str(t)
        for tag in ("cities", "pop"):
            v = sl.frozen_total(tag, min(t, end))
            if v is not None:
                row[f"{tag}{key}"] = v / nseat

    text = gamelog(d)
    found = 0
    sites_n = sites_s = 0.0
    alive_by_turn = {}
    for m in R_EXPAND.finditer(text):
        turn, al, fo, si = (int(m.group(1)), int(m.group(3)),
                            int(m.group(4)), float(m.group(5)))
        found += fo
        sites_n += 1
        sites_s += si
        alive_by_turn[turn] = alive_by_turn.get(turn, 0) + al
    row["found"] = found
    row["sites"] = sites_s / sites_n if sites_n else 0.0
    # Founders standing, per seat, at the same cuts the city curve uses.
    # `born` on the same line is NOT used: a unit built in end-turn
    # processing has a birth_turn the next census no longer matches, so
    # it undercounts by roughly four to one. Settlers actually delivered
    # are counted from deep-prodswitch builtlast=1 below, which is the
    # corrected census S-PRODKEEP settled on.
    if alive_by_turn:
        last = max(alive_by_turn)
        for t in marks + [last]:
            key = "end" if t == last else str(t)
            near = [x for x in alive_by_turn if x <= t]
            if near:
                row[f"alive{key}"] = alive_by_turn[max(near)] / nseat
    row["joins"] = len(R_JOIN.findall(text))

    built = starts = 0
    for m in R_BUILT.finditer(text):
        if m.group(1) == "Settlers":
            starts += 0
            built += 1 if m.group(3) == "1" else 0
        if m.group(2) == "Settlers":
            starts += 1
    row["settlers_built"] = built
    row["settler_starts"] = starts

    held = fired = 0
    for m in R_GUARD.finditer(text):
        held += int(m.group(1))
        fired += 1 if (m.group(1) == "1" and m.group(2) == "1") else 0
    row["guard_seen"] = held
    row["guard_fired"] = fired

    capped = capfired = 0
    for m in R_CAP.finditer(text):
        capped += int(m.group(1))
        capfired += 1 if (m.group(1) == "1" and m.group(2) == "1") else 0
    row["cap_seen"] = capped
    row["cap_fired"] = capfired

    empty = tbland = offpeace = 0
    for m in R_LAD.finditer(text):
        if m.group(1) == "0":
            empty += 1
            offpeace += int(m.group(2))
            tbland += int(m.group(5))
    row["empty_searches"] = empty
    row["empty_offpeace"] = offpeace
    row["empty_tbland"] = tbland
    return row


def cell(root, name, marks):
    out = {}
    for d in sorted((root / name).glob("seed-*")):
        if (d / "DONE").exists() and (d / "scorelog.txt").exists():
            out[int(d.name.split("-")[1])] = game_metrics(d, marks)
    return out


def paired(a, b, key, seeds):
    xs = [(a[s][key], b[s][key]) for s in seeds
          if key in a[s] and key in b[s]]
    if len(xs) < 2:
        return None
    diffs = [y - x for x, y in xs]
    m = mean(diffs)
    se = stdev(diffs) / len(diffs) ** 0.5 if len(diffs) > 1 else 0.0
    return (mean(x for x, _ in xs), mean(y for _, y in xs), m, se,
            m / se if se else float("nan"),
            sum(1 for d in diffs if d > 0), len(diffs))


def venue(root):
    marks = [45, 100] if "war" in root.name else [40, 60]
    cells = {c: cell(root, c, marks) for c in CELLS if (root / c).is_dir()}
    cells = {c: v for c, v in cells.items() if v}
    if "c00" not in cells:
        print(f"{root}: no c00 arm")
        return
    seeds = sorted(set.intersection(*(set(v) for v in cells.values())))
    print(f"\n=== {root.name}: {len(seeds)} paired seeds, "
          f"cells {' '.join(f'{c}={len(v)}' for c, v in cells.items())}")

    keys = [k for k in ("score", "cities45", "cities100", "cities40",
                        "cities60", "citiesend", "popend", "found",
                        "settlers_built", "settler_starts", "alive45",
                        "alive100", "alive40", "alive60", "aliveend",
                        "joins", "guard_seen",
                        "guard_fired", "cap_seen", "cap_fired", "sites",
                        "empty_searches", "empty_offpeace", "empty_tbland")
            if any(k in r for v in cells.values() for r in v.values())]

    print("\n  absolute means (per seat where the metric is per seat)")
    print(f"    {'metric':<16}" + "".join(f"{c:>12}" for c in cells))
    for k in keys:
        vals = []
        for c in cells:
            xs = [cells[c][s][k] for s in seeds if k in cells[c][s]]
            vals.append(mean(xs) if xs else float("nan"))
        print(f"    {k:<16}" + "".join(f"{v:>12.2f}" for v in vals))

    for c in [x for x in cells if x != "c00"]:
        print(f"\n  {c} - c00, paired on {len(seeds)} seeds")
        print(f"    {'metric':<16}{'c00':>10}{c:>10}{'delta':>10}"
              f"{'se':>8}{'t':>7}   wins")
        for k in keys:
            r = paired(cells["c00"], cells[c], k, seeds)
            if r:
                print(f"    {k:<16}{r[0]:>10.2f}{r[1]:>10.2f}{r[2]:>+10.2f}"
                      f"{r[3]:>8.2f}{r[4]:>+7.2f}   {r[5]}/{r[6]}")


def main():
    root = Path(sys.argv[1] if len(sys.argv) > 1
                else Path(__file__).parent / "results/siteplan")
    subs = [root / v for v in ("war", "econ") if (root / v).is_dir()]
    for r in (subs or [root]):
        venue(r)


main()
