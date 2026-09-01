#!/usr/bin/env python3
"""Per-city birth/death ledger for a benchmark corpus (S-CITYLEDGER).

    cityledger.py --root DIR [--root DIR ...] [--band 25] [--pair I J]

The question this answers: when a city count gained at turn T is gone by
turn T', was the city never founded (a founding-RATE difference) or was it
founded and then lost (attrition)?

Two independent readers, deliberately:

1. **The scorelog reader**, which works for EVERY arm including
   ``ai/baseline`` and for every archived corpus whose logs were filtered,
   because ``scorelog.txt`` carries tag 3 (``cities``) per player per turn.
   A per-player increase is a city gained that turn; a decrease is a city
   lost.  Losses split by whether the ALL-PLAYER total moved: the total
   conserved across the turn means the city changed hands (conquest,
   incite, or a barbarian taking it), the total down means it left the
   game (``remove_city()`` -- razed, disbanded, starved, nuked).

2. **The ``deep-city`` reader**, which works only on unfiltered logs of
   ``ai/deep`` arms.  ``ai/deep``'s ``city_got``/``city_lost`` hooks
   (``deepai.c:843``, ``:894``) print the city id, its size and its
   ``turn_founded``, so a gain with ``founded == turn`` is a FOUNDING and
   a gain with ``founded < turn`` is a CONQUEST, and every loss carries
   the size and the age of the city that died.

Where both readers apply, both are printed side by side.  Where only the
scorelog reader applies, its per-turn classification is a heuristic: a
turn holding both a founding and a destruction cannot be told from a
transfer, so such turns are counted into ``ambig`` and reported rather
than hidden.

The server itself logs none of this at ``LOG_NORMAL``: ``remove_city()``
and ``transfer_city()`` announce through ``notify_player()``, which is a
client packet, so ``game.log`` carries no city event of its own.  That is
why the readers above are what exist.
"""

import argparse
import io
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

SEAT_RE = re.compile(r"^P\d+$")
CITY_RE = re.compile(
    r'deep-city turn=(\d+) player="([^"]*)" (got|lost)=(\d+)'
    r" size=(\d+) founded=(-?\d+) at=\((\d+),(\d+)\)"
)
# ai/deep's unit obituary (deepai.c:1119), used here only to ask whether
# anything died fighting on the tile a city vanished from.
LOSS_RE = re.compile(
    r'deep-loss turn=(\d+) player="P\d+" unit="[^"]*" founder=\d+'
    r' reason="([^"]*)" at=\((\d+),(\d+)\)'
)
CITIES_TAG = "3"

FLOWS = ("found", "taken", "destroyed", "transferred")


# --------------------------------------------------------------- scorelog

def read_scorelog(path):
    """-> names {idx: name}, series {idx: {turn: cities}}, sorted turns."""
    names = {}
    series = defaultdict(dict)
    turns = set()
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.startswith("data "):
                f = line.split()
                if len(f) == 5 and f[2] == CITIES_TAG:
                    series[int(f[3])][int(f[1])] = int(f[4])
                    turns.add(int(f[1]))
            elif line.startswith("addplayer "):
                f = line.split(None, 3)
                if len(f) >= 4:
                    names[int(f[2])] = f[3].strip()
    return names, series, sorted(turns)


def scorelog_events(series, turns):
    """Yield (turn, player, kind, count) for every city-count delta.

    kind in found / taken / destroyed / transferred, plus a bare
    ("ambig", turn) marker when one turn mixes a birth and a death and the
    transfer match is therefore not provable.

    The turn reported is the turn the event HAPPENED, which is one before
    the turn its delta appears in the scorelog: the score snapshot is
    taken at the start of a turn, so a city founded during turn t first
    shows in the sample for turn t+1.  Verified against the ``deep-city``
    lines, which log at the true turn -- with the shift the two readers
    agree band for band, without it every band edge is off by one turn.
    """
    prev = {}
    for t in turns:
        gains, losses = {}, {}
        for p, d in series.items():
            v = d.get(t)
            if v is None:
                continue
            was = prev.get(p)
            if was is not None and v != was:
                (gains if v > was else losses)[p] = abs(v - was)
            prev[p] = v
        if not gains and not losses:
            continue
        g, l = sum(gains.values()), sum(losses.values())
        at = t - 1
        matched = min(g, l)           # cities that changed hands this turn
        if matched and (g > matched or l > matched):
            yield (at, None, "ambig", 1)
        rem = matched
        for p, n in sorted(losses.items()):
            take = min(n, rem)
            rem -= take
            if take:
                yield (at, p, "transferred", take)
            if n - take:
                yield (at, p, "destroyed", n - take)
        rem = matched
        for p, n in sorted(gains.items()):
            take = min(n, rem)
            rem -= take
            if take:
                yield (at, p, "taken", take)
            if n - take:
                yield (at, p, "found", n - take)


# -------------------------------------------------------------- deep-city

EXPAND_RE = re.compile(r'deep-expand turn=(\d+) player="(P\d+)" (.*)$')
LADDER_RE = re.compile(r'deep-siteladder turn=(\d+) player="(P\d+)" (.*)$')


def log_lines(gdir):
    """Iterate an unfiltered game log, or yield nothing when absent."""
    for name in ("game.log.zst", "game.log"):
        p = gdir / name
        if not p.exists():
            continue
        if p.suffix == ".zst":
            try:
                raw = subprocess.run(["zstdcat", str(p)], check=True,
                                     stdout=subprocess.PIPE).stdout
            except (OSError, subprocess.CalledProcessError):
                return
            handle = io.StringIO(raw.decode("utf-8", "replace"))
        else:
            handle = open(p, encoding="utf-8", errors="replace")
        with handle:
            for line in handle:
                yield line
        return


def kv(text):
    out = {}
    for tok in text.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            try:
                out[k] = float(v)
            except ValueError:
                pass
    return out


def scan_log(gdir, want_funnel):
    """-> (deep-city rows, deep-expand rows, deep-siteladder rows, combat)."""
    city, expand, ladder = [], [], []
    combat = set()                 # (turn, x, y) where a unit died fighting
    for line in log_lines(gdir):
        m = CITY_RE.search(line)
        if m:
            city.append((int(m.group(1)), m.group(2), m.group(3),
                         int(m.group(4)), int(m.group(5)), int(m.group(6)),
                         (int(m.group(7)), int(m.group(8)))))
            continue
        m = LOSS_RE.search(line)
        if m:
            if m.group(2) in ("killed", "eliminated", "captured", "bribed",
                              "city_lost"):
                combat.add((int(m.group(1)), int(m.group(3)),
                            int(m.group(4))))
            continue
        if not want_funnel:
            continue
        m = EXPAND_RE.search(line)
        if m:
            expand.append((int(m.group(1)), kv(m.group(3))))
            continue
        m = LADDER_RE.search(line)
        if m:
            ladder.append((int(m.group(1)), kv(m.group(3))))
    return city, expand, ladder, combat


# ------------------------------------------------------------------ bands

def make_bands(band, last):
    out, a = [], 1
    while a <= last:
        out.append((a, min(a + band - 1, last)))
        a += band
    return out


def band_of(edges, t):
    for i, (a, b) in enumerate(edges):
        if a <= t <= b:
            return i
    return None


class Game:
    """One finished game, reduced to per-seat flows per band."""

    def __init__(self, gdir, band, want_funnel=False):
        names, series, turns = read_scorelog(gdir / "scorelog.txt")
        self.ok = bool(turns)
        if not self.ok:
            return
        self.edges = make_bands(band, turns[-1])
        nb = len(self.edges)
        seats = {p for p, nm in names.items() if SEAT_RE.match(nm)}
        if not seats:
            seats = set(series)
        self.seats = len(seats)
        self.flow = {k: [0] * nb for k in FLOWS}
        self.ambig = 0
        for t, p, kind, n in scorelog_events(series, turns):
            i = band_of(self.edges, t)
            if i is None:
                continue
            if kind == "ambig":
                self.ambig += n
            elif p in seats:
                self.flow[kind][i] += n
        # Cities HELD AT THE END of the band's last turn, which is the
        # sample taken at the start of the turn after it (see
        # scorelog_events on the one-turn lag).
        last = turns[-1]
        self.cities = [sum(series[p].get(min(b + 1, last), 0) for p in seats)
                       for _, b in self.edges]

        rows, expand, ladder, combat = scan_log(gdir, want_funnel)
        self.have_deep = bool(rows)
        self.funnel = self._funnel(expand, ladder) if want_funnel else None
        self.dflow = {k: [0] * nb for k in ("found", "taken", "lost")}
        self.lost = [[] for _ in range(nb)]
        idx = {nm: p for p, nm in names.items()}

        # Removals per turn, EXACTLY, when the deep-city reader covers
        # every seat: the all-player city total moves by
        # (foundings - removals) and the foundings are counted exactly, so
        # removals = foundings - delta.  Everything else a seat lost that
        # turn changed hands.  This is stronger than the scorelog reader's
        # match heuristic, which cannot tell a founding beside a removal
        # from a hand-over; the econ venue has 21 such turns.
        total = {t: sum(series[p].get(t, 0) for p in series) for t in turns}
        founds_at = defaultdict(int)
        lost_at = defaultdict(list)
        for turn, who, what, _cid, size, founded, at in rows:
            if idx.get(who) not in seats:
                continue
            if what == "got":
                if founded == turn:
                    founds_at[turn] += 1
            else:
                fought = ((turn, at[0], at[1]) in combat
                          or (turn - 1, at[0], at[1]) in combat)
                lost_at[turn].append((size, turn - founded if founded > 0
                                      else -1, fought))
        removals_at = {}
        prev_t = None
        for t in turns:
            if prev_t is not None:
                removals_at[prev_t] = founds_at[prev_t] - (total[t]
                                                           - total[prev_t])
            prev_t = t

        for turn, who, what, _cid, _size, founded, _at in rows:
            i = band_of(self.edges, turn)
            if i is None or idx.get(who) not in seats:
                continue
            if what == "got":
                self.dflow["found" if founded == turn else "taken"][i] += 1
            else:
                self.dflow["lost"][i] += 1
        for turn, dead in sorted(lost_at.items()):
            i = band_of(self.edges, turn)
            if i is None:
                continue
            # `remove_city()` is what runs when a size-1 city is conquered
            # (citytools.c:2040), when famine, upkeep failure or a disaster
            # takes the last citizen, or when the city is disbanded;
            # conquest at size >= 2 goes through `transfer_city()` and the
            # total is conserved. A removal is therefore almost always the
            # smallest city that died that turn, so spend the turn's
            # removal budget smallest-first.
            budget = max(removals_at.get(turn, 0), 0)
            for k, (size, age, fought) in enumerate(sorted(dead)):
                cause = "razed" if k < budget else "taken"
                self.lost[i].append((size, age, cause, fought))

    # The founder funnel (deep-expand, one line per player-turn, fields
    # documented in ai/deep/deepexpand.h) and the site-search refusal
    # ladder (deep-siteladder, one line per search, deepsite.c:2360).
    # This is the WHY beside the ledger's WHAT: when the ledger says a
    # band founded fewer cities, these say at which stage it stopped.
    EXPAND_KEYS = ("cities", "fresh", "stale", "w0", "wpos", "held", "vfood",
                   "skipwar", "auc", "win", "lostdom", "lostmil", "prod",
                   "alive", "found", "sites", "freeshare")
    LADDER_KEYS = ("seen", "off", "offpeace", "offbridge", "nofound",
                   "noterr", "nomin", "mindist", "danger", "taken", "starve",
                   "scored", "best_raw", "best_net")

    def _funnel(self, expand, ladder):
        nb = len(self.edges)
        f = {"e_n": [0] * nb, "l_n": [0] * nb, "l_hit": [0] * nb,
             "l_empty": [0] * nb}
        for k in self.EXPAND_KEYS:
            f["e_" + k] = [0.0] * nb
        for k in self.LADDER_KEYS:
            f["E_" + k] = [0.0] * nb        # empty searches only
        for turn, d in expand:
            i = band_of(self.edges, turn)
            if i is None:
                continue
            f["e_n"][i] += 1
            for k in self.EXPAND_KEYS:
                f["e_" + k][i] += d.get(k, 0.0)
        for turn, d in ladder:
            i = band_of(self.edges, turn)
            if i is None:
                continue
            f["l_n"][i] += 1
            if d.get("found", 0):
                f["l_hit"][i] += 1
                continue
            f["l_empty"][i] += 1
            for k in self.LADDER_KEYS:
                f["E_" + k][i] += d.get(k, 0.0)
        return f

    def check(self):
        """Reconstructed count vs the scorelog's own, at every band edge."""
        run, bad = 0, []
        for i in range(len(self.edges)):
            run += (self.flow["found"][i] + self.flow["taken"][i]
                    - self.flow["destroyed"][i] - self.flow["transferred"][i])
            if run != self.cities[i]:
                bad.append((self.edges[i][1], run, self.cities[i]))
        return bad


def load(root, band, want_funnel=False):
    out = {}
    for sl in sorted(Path(root).rglob("scorelog.txt")):
        if not (sl.parent / "DONE").exists():
            continue
        g = Game(sl.parent, band, want_funnel)
        if g.ok:
            out[sl.parent.name] = g
    return out


def mean_se(v):
    if not v:
        return float("nan"), float("nan")
    m = sum(v) / len(v)
    if len(v) < 2:
        return m, float("nan")
    var = sum((x - m) ** 2 for x in v) / (len(v) - 1)
    return m, (var / len(v)) ** 0.5


def report(name, games):
    if not games:
        print("%s: no finished games" % name)
        return
    n = len(games)
    edges = max((g.edges for g in games.values()), key=len)
    nb = len(edges)
    deep_n = sum(1 for g in games.values() if g.have_deep)
    acc = {k: [0.0] * nb for k in FLOWS}
    dacc = {k: [0.0] * nb for k in ("found", "taken", "lost")}
    cit = [0.0] * nb
    cit_n = [0] * nb
    lost = [[] for _ in range(nb)]
    bad = late = 0
    for g in games.values():
        miss = g.check()
        if miss:
            bad += 1
            if min(t for t, _, _ in miss) >= g.edges[-1][0] - 2:
                late += 1
        for i in range(len(g.edges)):
            for k in FLOWS:
                acc[k][i] += g.flow[k][i] / g.seats
            for k in dacc:
                dacc[k][i] += g.dflow[k][i] / g.seats
            cit[i] += g.cities[i] / g.seats
            cit_n[i] += 1
            lost[i].extend(g.lost[i])
    seats = sum(g.seats for g in games.values()) / n
    head = "%s   n=%d games, %.1f seats/game" % (name, n, seats)
    if deep_n:
        head += "   deep-city lines in %d" % deep_n
    print(head)
    print("  band       found  taken   died  (razed  taken-away)  cities@edge"
          + ("   [deep: found taken lost]" if deep_n else ""))
    for i, (a, b) in enumerate(edges):
        died = acc["destroyed"][i] + acc["transferred"][i]
        row = ("  T%-3d-%-3d %6.3f %6.3f %6.3f  (%6.3f %11.3f) %10.3f"
               % (a, b, acc["found"][i] / n, acc["taken"][i] / n, died / n,
                  acc["destroyed"][i] / n, acc["transferred"][i] / n,
                  cit[i] / max(cit_n[i], 1)))
        if deep_n:
            row += ("   [%6.3f %5.3f %5.3f]"
                    % (dacc["found"][i] * n / deep_n / n,
                       dacc["taken"][i] * n / deep_n / n,
                       dacc["lost"][i] * n / deep_n / n))
        print(row)
    amb = sum(g.ambig for g in games.values())
    if amb:
        print("  ambiguous turns (a birth and a death in the same turn): %d"
              % amb)
    if bad:
        print("  ledger closes in %d/%d games; %d disagree with the scorelog"
              " count (%d of them only in the final turns, where the last"
              " score sample precedes the last event)"
              % (n - bad, n, bad, late))
    else:
        print("  ledger closes: reconstruction == scorelog count at every"
              " band edge, all %d games" % n)
    rows = [r for band_rows in lost for r in band_rows]
    if rows:
        print("  every city lost, by cause (deep-city reader joined to the"
              " scorelog total):")
        print("    (`fought` = a unit died on that tile that turn or the one"
              " before, i.e. the city was taken by force)")
        for cause in ("taken", "razed"):
            sel = [r for r in rows if r[2] == cause]
            if not sel:
                continue
            ages = sorted(a for _, a, _, _ in sel if a >= 0)
            print("    %-6s n=%-4d %6.3f/seat/game  mean size %.2f"
                  "  size1 %3d%%  mean age %5.1f  median age %3d"
                  "  fought %3d%%"
                  % (cause, len(sel), len(sel) / n / seats,
                     sum(s for s, _, _, _ in sel) / len(sel),
                     round(100.0 * sum(1 for s, _, _, _ in sel if s <= 1)
                           / len(sel)),
                     sum(ages) / len(ages) if ages else float("nan"),
                     ages[len(ages) // 2] if ages else -1,
                     round(100.0 * sum(1 for _, _, _, f in sel if f)
                           / len(sel))))


def report_funnel(games):
    have = [g for g in games.values() if g.funnel and any(g.funnel["e_n"])]
    if not have:
        return
    edges = max((g.edges for g in have), key=len)
    nb = len(edges)
    acc = defaultdict(lambda: [0.0] * nb)
    for g in have:
        for i in range(len(g.edges)):
            for k, v in g.funnel.items():
                acc[k][i] += v[i]
    print("\n  the founder funnel (deep-expand), mean per player-turn")
    keys = ("cities", "fresh", "w0", "held", "vfood", "skipwar", "auc",
            "win", "lostdom", "lostmil", "prod", "alive", "sites",
            "freeshare")
    print("  band    " + "".join("%9s" % k for k in keys) + "  empty-search%")
    for i, (a, b) in enumerate(edges):
        n = acc["e_n"][i]
        if not n:
            continue
        fresh = acc["e_fresh"][i]
        print("  T%-3d-%-3d" % (a, b)
              + "".join("%9.3f" % (acc["e_" + k][i] / n) for k in keys)
              + "%14.1f" % (100.0 * acc["e_w0"][i] / fresh if fresh else 0.0))
    if not any(acc["l_n"]):
        return
    print("\n  the site-search ladder (deep-siteladder): how often a search"
          " comes home with NO site,")
    print("  and what the tiles it saw were refused for when it does."
          " Buckets partition `seen`.")
    keys = ("seen", "off", "offpeace", "offbridge", "noterr", "nomin",
            "mindist", "starve", "scored")
    print("  band      searches   found%%   %s"
          % "".join("%9s" % k for k in keys))
    for i, (a, b) in enumerate(edges):
        n, e = acc["l_n"][i], acc["l_empty"][i]
        if not n:
            continue
        print("  T%-3d-%-3d %9d  %6.1f%%   %s"
              % (a, b, n, 100.0 * acc["l_hit"][i] / n,
                 "".join("%9.2f" % (acc["E_" + k][i] / e if e else 0.0)
                         for k in keys)))
    print("  (the bucket columns are means over the EMPTY searches only)")


def paired(na, ga, nb_, gb):
    keys = sorted(set(ga) & set(gb))
    if not keys:
        print("\nno shared seeds between %s and %s" % (na, nb_))
        return
    nb = min(len(ga[k].edges) for k in keys)
    nb = min(nb, min(len(gb[k].edges) for k in keys))
    edges = ga[keys[0]].edges[:nb]
    print("\npaired  %s  -  %s   (n=%d seeds, per seat)" % (na, nb_, len(keys)))
    print("  band       d(found)         d(died)         d(cities@edge)")
    for i, (a, b) in enumerate(edges):
        df, dd, dc = [], [], []
        for k in keys:
            x, y = ga[k], gb[k]
            df.append(x.flow["found"][i] / x.seats - y.flow["found"][i] / y.seats)
            dd.append((x.flow["destroyed"][i] + x.flow["transferred"][i]) / x.seats
                      - (y.flow["destroyed"][i] + y.flow["transferred"][i]) / y.seats)
            dc.append(x.cities[i] / x.seats - y.cities[i] / y.seats)
        print("  T%-3d-%-3d %+7.3f+-%.3f  %+7.3f+-%.3f  %+7.3f+-%.3f"
              % ((a, b) + mean_se(df) + mean_se(dd) + mean_se(dc)))
    # cumulative foundings, which is what the cities@edge curve is made of
    print("  cumulative d(found) through each edge:")
    for i, (_a, b) in enumerate(edges):
        cf = []
        for k in keys:
            x, y = ga[k], gb[k]
            cf.append(sum(x.flow["found"][:i + 1]) / x.seats
                      - sum(y.flow["found"][:i + 1]) / y.seats)
        m, se = mean_se(cf)
        print("    through T%-3d  %+7.3f+-%.3f" % (b, m, se))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", action="append", required=True)
    ap.add_argument("--band", type=int, default=25)
    ap.add_argument("--pair", nargs=2, action="append", default=[],
                    metavar=("I", "J"),
                    help="0-based --root indices to contrast per seed")
    ap.add_argument("--why", action="store_true",
                    help="also read deep-expand and deep-siteladder and"
                         " print where the founder funnel stops")
    args = ap.parse_args()
    loaded = [(r, load(r, args.band, args.why)) for r in args.root]
    for r, g in loaded:
        report(r, g)
        if args.why:
            report_funnel(g)
        print()
    for i, j in args.pair:
        a, b = loaded[int(i)], loaded[int(j)]
        paired(a[0], a[1], b[0], b[1])


if __name__ == "__main__":
    sys.exit(main())
