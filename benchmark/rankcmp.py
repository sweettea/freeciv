#!/usr/bin/env python3
"""The scoreboard reader: competitive axis from mixed games, societal
axis from self-play, and the T50/T100 panel (scoreboard.sh writes the
layout; doc/deep-ai/findings/scoreboard-design-2026-10-28.md is the
design).

    rankcmp.py SCOREBOARD_DIR [--md FILE]
    rankcmp.py --selftest

SCOREBOARD_DIR holds <label>/<venue>/selfplay/{deep,baseline}/seed-N/
and <label>/<venue>/mixed/{mixed_db,mixed_bd}/seed-N/, each game dir
with scorelog.txt. <label> is `defaults` or `vector-NAME`; baseline
self-play lives only under defaults and serves every label.

COMPETITIVE AXIS (mixed games). Seat convention is run-game.sh's and
tune.py mixed_sides()'s: in mixed_db the odd seats P1, P3, ... are deep
and the even seats baseline; mixed_bd is the mirror. Seats whose name
is not P<n> (civil-war splinters) are no one's and are not ranked.
Every P seat is ranked within its game: a seat eliminated before the
end (scorelog `delplayer`) ranks below every survivor, survivors by
final score, ties share the average rank. Then, per game:

  AUC       P(random deep seat outranks random baseline seat in the
            same game), ties 1/2 -- the Mann-Whitney U over the
            game's deep x baseline seat pairs, divided by their count.
            0.5 is parity.
  normrank  mean over an arm's seats of (rank-1)/(N-1): 0 best, 1
            worst, 0.5 the expectation under no difference.
  win       1 if the rank-1 seat is deep (1/2 each on a shared top).
  score     per-seat mean final score of each arm (last logged score,
            the tune.py mixed objective's convention, dead seats
            included at their last score).

Each seed's two mirrored games are averaged into ONE datum (seat bias
cancels in the pair, run-game.sh), and means and SEs are over seeds --
the seed is the independent unit; seats in one game are not.

With equal arm sizes and no eliminations, normrank is an affine image
of AUC: deep mean rank - baseline mean rank = n(1 - 2 AUC) for n seats
a side. They part only through eliminations and ties; both are printed
so that the day they part is visible.

SOCIETAL AXIS (self-play). Per seed, each arm's P seats are averaged
(paircmp.py's convention, splinters excluded) and deep - baseline is
paired by seed over paircmp.py's columns; the score row adds the
ratio of means. paircmp.py itself cannot be imported (it runs at
import) and pairs one arm name across two roots, while this pairs
two arms, so the arithmetic is restated here with its columns.

PANEL. benchmark/panel.py BASELINE_ROOT DEEP_ROOT, so its delta column
is deep - baseline: T50/T100 on contested and contested-classic,
T50/T80 (the end) on lategame-80.
"""
import argparse
import math
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from statistics import mean, stdev

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from extract import game_rows  # noqa: E402

# paircmp.py's columns, same order (see its COLS).
COLS = ["score", "cities", "unitsbuilt", "techs", "pop", "mean_city_size",
        "settlers", "landarea", "munits", "wonders",
        "unitslost", "unitskilled"]
# Panel turns per venue: the brief's T50/T100; lategame-80 ends at T80
# (lategame-80.conf ENDTURN=80), so its second cut is the end.
PANEL_AT = {"lategame-80": [50, 80],
            "europe-late": [200, 250]}   # resumed at T150, ends T250
PANEL_DEFAULT = [50, 100]
SEAT = re.compile(r"P(\d+)")


# ------------------------------------------------------------- parsing
def seat_finals(scorelog):
    """[(name, last_score, eliminated)] for the P seats of one game,
    plus the count of non-P (splinter) players. A player id can be
    reused after delplayer, so data is attributed to the name the id
    carried when the line was written."""
    score_tag = None
    cur = {}                    # player id -> name
    last = {}                   # name -> last score
    dead = set()
    others = set()
    with open(scorelog, encoding="utf-8", errors="replace") as f:
        for line in f:
            p = line.rstrip("\n").split(" ")
            if p[0] == "tag" and len(p) > 2 and p[2] == "score":
                score_tag = int(p[1])
            elif p[0] == "addplayer" and len(p) > 3:
                cur[int(p[2])] = " ".join(p[3:])
            elif p[0] == "delplayer" and len(p) > 2:
                name = cur.pop(int(p[2]), None)
                if name is not None:
                    dead.add(name)
            elif p[0] == "data" and score_tag is not None:
                try:
                    turn, tag, plr = int(p[1]), int(p[2]), int(p[3])
                    val = float(p[4])
                except (ValueError, IndexError):
                    continue        # a torn line, as extract.py allows
                if tag != score_tag or plr not in cur:
                    continue
                last[cur[plr]] = val
    if score_tag is None:
        raise ValueError(f"{scorelog}: no score tag")
    seats = []
    for name, sc in last.items():
        if SEAT.fullmatch(name):
            seats.append((name, sc, name in dead))
        else:
            others.add(name)
    return sorted(seats, key=lambda s: int(s[0][1:])), len(others)


def side_of(name, arm):
    """'deep' or 'base' for seat `name` in mixed arm `arm` (run-game.sh:
    mixed_XY puts X on odd seats, Y on even)."""
    odd = int(name[1:]) % 2 == 1
    letters = arm[len("mixed_"):]
    letter = letters[0] if odd else letters[1]
    return {"d": "deep", "b": "base"}[letter]


def ranks(seats):
    """{name: rank} with 1 best; eliminated below survivors; ties get
    the average rank of the positions they span."""
    key = {n: (0 if dead else 1, sc) for n, sc, dead in seats}
    order = sorted(key, key=lambda n: key[n], reverse=True)
    out, i = {}, 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and key[order[j + 1]] == key[order[i]]:
            j += 1
        for k in range(i, j + 1):
            out[order[k]] = (i + j) / 2 + 1
        i = j + 1
    return out


def game_stats(scorelog, arm):
    """Per-game competitive numbers, or None if a side has no seat."""
    seats, splinters = seat_finals(scorelog)
    rk = ranks(seats)
    n = len(seats)
    side = {name: side_of(name, arm) for name, _, _ in seats}
    deep = [s for s in seats if side[s[0]] == "deep"]
    base = [s for s in seats if side[s[0]] == "base"]
    if not deep or not base:
        return None
    u = 0.0
    for d, _, _ in deep:
        for b, _, _ in base:
            u += 1.0 if rk[d] < rk[b] else 0.5 if rk[d] == rk[b] else 0.0
    norm = (lambda xs: mean((rk[x[0]] - 1) / (n - 1) for x in xs))
    top = [name for name in rk if rk[name] == min(rk.values())]
    return dict(auc=u / (len(deep) * len(base)),
                nr_deep=norm(deep), nr_base=norm(base),
                win=sum(side[t] == "deep" for t in top) / len(top),
                sc_deep=mean(s[1] for s in deep),
                sc_base=mean(s[1] for s in base),
                elim_deep=sum(s[2] for s in deep),
                elim_base=sum(s[2] for s in base),
                splinters=splinters)


# --------------------------------------------------------------- stats
def mse(xs):
    xs = list(xs)
    if not xs:
        return float("nan"), float("nan"), 0
    m = mean(xs)
    se = stdev(xs) / math.sqrt(len(xs)) if len(xs) > 1 else float("nan")
    return m, se, len(xs)


def fmt(m, se, prec=3):
    if m != m:
        return "n/a"
    s = f"{m:.{prec}f}"
    return s + (f" ± {se:.{prec}f}" if se == se else " ± n/a")


def games(root):
    """{seed: game dir} for root/seed-N carrying DONE and a scorelog."""
    out = {}
    if not root.is_dir():
        return out
    for d in root.glob("seed-*"):
        if (d / "DONE").exists() and (d / "scorelog.txt").exists():
            out[int(d.name[5:])] = d
    return out


# --------------------------------------------------------- competitive
def mixed_by_seed(vdir):
    """{seed: averaged datum over the two mirrors} for complete pairs,
    plus {seed: per-mirror stats} and the unpaired seed list."""
    db, bd = games(vdir / "mixed" / "mixed_db"), games(vdir / "mixed" / "mixed_bd")
    per, mirrors, unpaired = {}, {}, sorted(set(db) ^ set(bd))
    for s in sorted(set(db) & set(bd)):
        a = game_stats(db[s] / "scorelog.txt", "mixed_db")
        b = game_stats(bd[s] / "scorelog.txt", "mixed_bd")
        if a is None or b is None:
            unpaired.append(s)
            continue
        mirrors[s] = (a, b)
        per[s] = {k: (a[k] + b[k]) / 2 for k in a}
    return per, mirrors, unpaired


def competitive(vdir, out):
    per, mirrors, unpaired = mixed_by_seed(vdir)
    if not per:
        out.append("_no complete mirrored pair on disk_\n")
        return per
    seeds = sorted(per)
    col = lambda k: [per[s][k] for s in seeds]  # noqa: E731
    auc = mse(col("auc"))
    out.append(f"n = {len(seeds)} seeds × 2 mirrored games"
               + (f"; unpaired/unusable seeds skipped: {unpaired}" if unpaired else ""))
    out.append("")
    out.append("| quantity | deep | baseline | deep − baseline |")
    out.append("|---|---:|---:|---:|")
    z = (auc[0] - 0.5) / auc[1] if auc[1] and auc[1] == auc[1] else float("nan")
    out.append(f"| **P(deep seat outranks baseline seat)** (AUC) | {fmt(*auc[:2])} | | "
               f"{auc[0] - 0.5:+.3f} vs 0.5, z {z:+.2f} |")
    nd, nb = mse(col("nr_deep")), mse(col("nr_base"))
    dn = mse(per[s]["nr_deep"] - per[s]["nr_base"] for s in seeds)
    out.append(f"| mean normalized rank (0 best, 1 worst) | {fmt(*nd[:2])} | {fmt(*nb[:2])} | {fmt(*dn[:2])} |")
    w = mse(col("win"))
    out.append(f"| win share (rank 1) | {fmt(*w[:2])} | {fmt(1 - w[0], w[1])} | {w[0] - (1 - w[0]):+.3f} |")
    sd, sb = mse(col("sc_deep")), mse(col("sc_base"))
    ds = mse(per[s]["sc_deep"] - per[s]["sc_base"] for s in seeds)
    ratio = sd[0] / sb[0] if sb[0] else float("nan")
    out.append(f"| per-seat final score | {fmt(*sd[:2], 1)} | {fmt(*sb[:2], 1)} | {fmt(*ds[:2], 2)} (ratio {ratio:.3f}) |")
    ed, eb = mse(col("elim_deep")), mse(col("elim_base"))
    out.append(f"| seats eliminated per game | {ed[0]:.2f} | {eb[0]:.2f} | |")
    out.append("")
    # The mirror split: if the two seatings disagree wildly the seat
    # map, not the module, is carrying the number.
    a_db = mse(mirrors[s][0]["auc"] for s in seeds)
    a_bd = mse(mirrors[s][1]["auc"] for s in seeds)
    wins = sum(per[s]["auc"] > 0.5 for s in seeds)
    ties = sum(per[s]["auc"] == 0.5 for s in seeds)
    out.append(f"AUC by seating: deep on odd seats {fmt(*a_db[:2])}, deep on even "
               f"seats {fmt(*a_bd[:2])}. Seeds with AUC > 0.5: {wins}/{len(seeds)}"
               f" ({ties} at exactly 0.5). Splinter players per game "
               f"{mean(col('splinters')):.2f} (not ranked).")
    out.append("")
    return per


# ------------------------------------------------------------ societal
def seat_means(gamedir):
    """{col: mean over P seats} from extract.game_rows."""
    _h, rows = game_rows(gamedir)
    acc = {}
    for r in rows:
        if not SEAT.fullmatch(r["name"]):
            continue
        for c in COLS:
            v = r.get(c)
            if v not in (None, ""):
                acc.setdefault(c, []).append(float(v))
    return {c: mean(v) for c, v in acc.items()}


def selfplay_table(base_root, deep_root, out, title="baseline", other="deep"):
    b, d = games(base_root), games(deep_root)
    seeds = sorted(set(b) & set(d))
    if len(seeds) < 2:
        out.append(f"_fewer than two paired seeds ({len(b)} {title}, {len(d)} {other})_\n")
        return {}
    mb = {s: seat_means(b[s]) for s in seeds}
    md = {s: seat_means(d[s]) for s in seeds}
    out.append(f"n = {len(seeds)} paired seeds (per-seat means, {other} − {title})")
    out.append("")
    out.append(f"| metric | {title} | {other} | Δ | se | t | {other} higher |")
    out.append("|---|---:|---:|---:|---:|---:|---:|")
    for c in COLS:
        pairs = [(mb[s][c], md[s][c]) for s in seeds if c in mb[s] and c in md[s]]
        if len(pairs) < 2:
            continue
        diffs = [y - x for x, y in pairs]
        m, se, n = mse(diffs)
        t = m / se if se else float("nan")
        extra = ""
        if c == "score":
            extra = f" (ratio {mean(y for _, y in pairs) / mean(x for x, _ in pairs):.3f})"
        out.append(f"| {c}{extra} | {mean(x for x, _ in pairs):.2f} | {mean(y for _, y in pairs):.2f} "
                   f"| {m:+.2f} | {se:.2f} | {t:+.2f} | {sum(x > 0 for x in diffs)}/{n} |")
    out.append("")
    return {s: md[s]["score"] - mb[s]["score"] for s in seeds
            if "score" in md[s] and "score" in mb[s]}


def panel(base_root, deep_root, venue, out):
    at = PANEL_AT.get(venue, PANEL_DEFAULT)
    r = subprocess.run([sys.executable, str(HERE / "panel.py"), str(base_root),
                        str(deep_root), "--at", *map(str, at)],
                       capture_output=True, text=True)
    out.append(f"panel.py root0 = baseline, root1 = deep (delta = deep − baseline), at T{'/T'.join(map(str, at))}")
    out.append("")
    out.append("```")
    out.append((r.stdout or "").rstrip() or f"(panel.py failed: {r.stderr.strip()[-300:]})")
    out.append("```")
    out.append("")


# --------------------------------------------------------------- report
def map_identity(root, out):
    """Every cell of one (venue, seed) must have played the same map."""
    seen = {}
    for mh in root.glob("*/*/*/*/seed-*/maphash.txt"):
        rel = mh.relative_to(root).parts
        seen.setdefault((rel[1], rel[4]), set()).add(mh.read_text().strip())
    bad = sorted(k for k, v in seen.items() if len(v) > 1)
    out.append(f"Map identity: {len(seen)} (venue, seed) groups, "
               + ("all cells on one map." if not bad else f"**{len(bad)} DISAGREE**: {bad[:10]}"))


def guard_summary(root, out):
    ok = fail = missing = 0
    for done in root.glob("*/*/*/*/seed-*/DONE"):
        g = done.parent / "guard.txt"
        if not g.exists():
            missing += 1
        elif "verdict=OK" in g.read_text():
            ok += 1
        else:
            fail += 1
    cached = len(list(root.glob("*/*/*/*/seed-*/cached.txt")))
    out.append(f"Guards: {ok} OK, **{fail} FAIL**, {missing} without guard.txt; "
               f"{cached} baseline games hardlinked from the certified cache.")


def report(root):
    root = Path(root)
    out = ["# Scoreboard", ""]
    b = root / "BINARY.txt"
    if b.exists():
        out += ["```", b.read_text().rstrip(), "```", ""]
    guard_summary(root, out)
    map_identity(root, out)
    out.append("")
    labels = sorted((p.name for p in root.iterdir() if p.is_dir()
                     and not p.name.startswith(".")),
                    key=lambda n: (n != "defaults", n))
    per_label = {}
    for label in labels:
        ldir = root / label
        vf = ldir / "VECTOR.txt"
        out.append(f"## {label}" + (f" — `{vf.read_text().split()[0]}`" if vf.exists() else
                                    " — compiled defaults"))
        out.append("")
        for vdir in sorted(p for p in ldir.iterdir() if p.is_dir()):
            venue = vdir.name
            base = root / "defaults" / venue / "selfplay" / "baseline"
            deep = vdir / "selfplay" / "deep"
            out.append(f"### {label} / {venue}")
            out.append("")
            rec = {}
            if (vdir / "mixed").is_dir():
                out.append("**Competitive axis — mixed games, rank (rankcmp)**")
                out.append("")
                rec["mixed"] = competitive(vdir, out)
            out.append("**Societal axis — self-play, paired by seed**")
            out.append("")
            rec["self"] = selfplay_table(base, deep, out)
            if games(base) and games(deep):
                out.append("**Doctrine panel — self-play**")
                out.append("")
                panel(base, deep, venue, out)
            per_label[(label, venue)] = rec
    # A vector against the defaults it would replace, paired by seed.
    for (label, venue), rec in sorted(per_label.items()):
        if label == "defaults" or (("defaults", venue) not in per_label):
            continue
        ref = per_label[("defaults", venue)]
        out.append(f"## {label} − defaults / {venue}, paired by seed")
        out.append("")
        for key, name, prec in (("mixed", "AUC", 3), ("self", "self-play per-seat score Δ(deep−base)", 2)):
            if key not in rec or key not in ref:
                continue        # e.g. the classic cell has no mixed games
            a, r = rec[key] or {}, ref[key] or {}
            if key == "mixed":
                a = {s: v["auc"] for s, v in a.items()}
                r = {s: v["auc"] for s, v in r.items()}
            common = sorted(set(a) & set(r))
            m, se, n = mse(a[s] - r[s] for s in common)
            out.append(f"- {name}: {fmt(m, se, prec)} over n = {n}")
        out.append("")
    return "\n".join(out) + "\n"


# ------------------------------------------------------------- selftest
def _write_scorelog(path, finals, dead=(), splinter=None):
    """A minimal SCORELOG2: one score tag, the given final scores."""
    lines = ["#FREECIV SCORELOG2 test", "id xyz", "tag 0 pop", "tag 1 score",
             "turn 1 -4000 4000 BCE"]
    for i, (name, _) in enumerate(finals):
        lines.append(f"addplayer 1 {i} {name}")
    for i, (name, sc) in enumerate(finals):
        lines.append(f"data 1 1 {i} {sc // 2}")
        lines.append(f"data 2 1 {i} {sc}")
        lines.append(f"data 2 0 {i} 3")
    for i, (name, _) in enumerate(finals):
        if name in dead:
            lines.append(f"delplayer 2 {i}")
    if splinter:
        lines.append(f"addplayer 2 99 {splinter}")
        lines.append("data 2 1 99 5")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n")


def selftest():
    with tempfile.TemporaryDirectory() as t:
        t = Path(t)
        # 4 seats. mixed_db: P1,P3 deep; P2,P4 base.
        # scores P1 100, P2 90, P3 50, P4 70 -> ranks 1,2,4,3.
        # deep x base pairs: (P1>P2) (P1>P4) (P3<P2) (P3<P4) -> U=2/4.
        g = t / "db" / "scorelog.txt"
        _write_scorelog(g, [("P1", 100), ("P2", 90), ("P3", 50), ("P4", 70)],
                        splinter="Spartacus")
        s = game_stats(g, "mixed_db")
        assert abs(s["auc"] - 0.5) < 1e-12, s
        assert s["win"] == 1.0 and s["splinters"] == 1, s
        # normrank: deep ranks 1,4 -> (0 + 1)/2 = 0.5; base 2,3 -> 0.5.
        assert abs(s["nr_deep"] - 0.5) < 1e-12 and abs(s["nr_base"] - 0.5) < 1e-12
        # The mirror, same scores: P1,P3 are now BASE -> AUC 0.5 again,
        # winner is base.
        s2 = game_stats(g, "mixed_bd")
        assert abs(s2["auc"] - 0.5) < 1e-12 and s2["win"] == 0.0, s2
        # Deep sweeps: deep 100, 90 vs base 10, 20 -> AUC 1, nr_deep 1/6.
        g = t / "sweep" / "scorelog.txt"
        _write_scorelog(g, [("P1", 100), ("P2", 10), ("P3", 90), ("P4", 20)])
        s = game_stats(g, "mixed_db")
        assert s["auc"] == 1.0 and abs(s["nr_deep"] - (0 + 1 / 3) / 2) < 1e-12, s
        # Affine identity, n = 2 a side: deep mean rank - base mean rank
        # = n(1 - 2 AUC) = -2 when AUC = 1: ranks 1.5 vs 3.5.
        rk = ranks(seat_finals(g)[0])
        assert (rk["P1"] + rk["P3"]) / 2 - (rk["P2"] + rk["P4"]) / 2 == -2
        # Ties: P1 = P2 = 50 count 1/2; P3 30 < P4 40.
        # pairs (P1,P2) .5, (P1,P4) 1, (P3,P2) 0, (P3,P4) 0 -> 1.5/4.
        g = t / "tie" / "scorelog.txt"
        _write_scorelog(g, [("P1", 50), ("P2", 50), ("P3", 30), ("P4", 40)])
        s = game_stats(g, "mixed_db")
        assert abs(s["auc"] - 0.375) < 1e-12 and s["win"] == 0.5, s
        # Elimination outranks score: P3 deep has 500 but was destroyed,
        # so it ranks last. Base P2 60, P4 40; deep P1 50.
        # pairs (P1,P2) 0, (P1,P4) 1, (P3,P2) 0, (P3,P4) 0 -> 1/4.
        g = t / "dead" / "scorelog.txt"
        _write_scorelog(g, [("P1", 50), ("P2", 60), ("P3", 500), ("P4", 40)],
                        dead=("P3",))
        s = game_stats(g, "mixed_db")
        assert abs(s["auc"] - 0.25) < 1e-12 and s["elim_deep"] == 1, s
        # 6 seats, 3 a side (lategame-80 shape): mixed_bd puts deep on
        # P2,P4,P6. Scores 60 10 50 20 40 30: deep 10,20,30 vs base
        # 60,50,40 -> deep loses every pair.
        g = t / "six" / "scorelog.txt"
        _write_scorelog(g, [("P1", 60), ("P2", 10), ("P3", 50), ("P4", 20),
                            ("P5", 40), ("P6", 30)])
        s = game_stats(g, "mixed_bd")
        assert s["auc"] == 0.0 and abs(s["nr_deep"] - (3 + 4 + 5) / 3 / 5) < 1e-12, s
        # Seed-level averaging over a mirrored pair, read through the
        # scoreboard layout.
        root = t / "sb" / "defaults" / "contested" / "mixed"
        _write_scorelog(root / "mixed_db" / "seed-121" / "scorelog.txt",
                        [("P1", 100), ("P2", 10), ("P3", 90), ("P4", 20)])
        _write_scorelog(root / "mixed_bd" / "seed-121" / "scorelog.txt",
                        [("P1", 50), ("P2", 50), ("P3", 30), ("P4", 40)])
        for a in ("mixed_db", "mixed_bd"):
            (root / a / "seed-121" / "DONE").touch()
        per, _m, unpaired = mixed_by_seed(t / "sb" / "defaults" / "contested")
        assert not unpaired and abs(per[121]["auc"] - (1.0 + 0.625) / 2) < 1e-12, per
        # (mirror bd: deep P2 50, P4 40 vs base P1 50, P3 30: pairs
        #  (P2,P1) .5 (P2,P3) 1 (P4,P1) 0 (P4,P3) 1 -> 2.5/4 = 0.625)
    print("rankcmp selftest: OK (AUC, ties, eliminations, mirrors, 3/3 seats,"
          " rank identity, seed averaging)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", nargs="?")
    ap.add_argument("--md", help="write the report here as well as stdout")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    if a.selftest:
        selftest()
        return
    if not a.root:
        ap.error("SCOREBOARD_DIR required")
    text = report(a.root)
    sys.stdout.write(text)
    if a.md:
        Path(a.md).write_text(text)


if __name__ == "__main__":
    main()
