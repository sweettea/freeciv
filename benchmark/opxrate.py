#!/usr/bin/env python3
"""Operation exchange-rate census: what the launch model PREDICTED for
each assault operation against what actually HAPPENED to it.

    opxrate.py funnel  ARMDIR [ARMDIR...]     # per-op ledger, one arm per dir
    opxrate.py score   ARMDIR [ARMDIR...]     # scorelog kill/loss curves

An "arm dir" is any directory with `*/seed-*/game.log[.zst]` under it.

WHY A LEDGER AND NOT A COUNTER. S-WEAKPREY established that deep aims
at weaker rivals, forms 63% more operations, and still trades ~2 losses
per extra kill. A per-turn counter cannot say where in an operation's
life that trade is lost, because an operation is a five-stage object:

    form -> (muster) -> march -> strike -> consolidate

and each stage has its own prediction. Formation prices the smallest
prefix of the pool that clears op_take_target against the garrison AT
ARRIVAL. The launch gate re-prices the force actually standing at the
staging tile. The march re-gates every turn against op_abort_floor.
Only the strike is a fight. So the ledger keys on (arm, seed, player,
slot) and closes an episode at its dissolve line, carrying every
prediction the operation was given and every realized fact the log
retained.

WHAT IS PREDICTED (and where it comes from):
  odds0/bar0/required/pool/drafted/eta0/overseas/def_n/def_best
                                    deep-opodds verdict="formed"
  odds_m (P(take) at launch), surv_m (E[survivors]), present, eta_m
                                    deep-op ... march

WHAT IS REALIZED:
  reached march / strike / consolidate; terminal verb; turns alive;
  member count at each stage; kills/losses when deep-employop survived
  the log filter (it did not in benchmark/results/weakprey).

WHAT IS NOT LOGGED AT ALL, and is the reason this census has a blind
spot the fix epic has to close: the launch gate computes `loss_now`
(E[attacker shields destroyed]) and prices value - cost with it, but
only writes it to the log on the REFUSE branch (deep-netval). Every
operation that actually launched carries no recorded loss forecast.
"""
import bisect
import gzip
import re
import statistics
import subprocess
import sys
from pathlib import Path

# ---------------------------------------------------------------- lines

RE_FORM = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) (form|muster) '
    r'target=(\d+) staging=(\d+) required=(\d+) pool=(\d+) drafted=(\d+)')
RE_ODDS = re.compile(
    r'deep-opodds turn=(\d+) player="([^"]+)" op=(\d+) target=(\d+) '
    r'verdict="([a-z-]+)" odds=([\d.]+) bar=([\d.]+) take_target=([\d.]+) '
    r'required=(\d+) pool=(\d+) drafted=(\d+) eta=(-?\d+) overseas=(\d+) '
    r'def_n=(\d+) def_best=(\d+).*?worth=(-?\d+)'
    # prey=/preyunits= arrived with the weak-prey epic; logs older than
    # it end the line at mabandon= and must still parse.
    r'(?:.*?prey=([\d.]+) preyunits=(\d+))?')
RE_MARCH = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) march target=(\d+) '
    r'present=(\d+) required=(\d+) waited=(\d+) eta=(\d+) odds=([\d.]+) '
    r'surv=([-\d.]+) bar=(\d+)')
RE_STRIKE = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) strike target=(\d+) '
    r'present=(\d+) required=(\d+) waited=(\d+)')
RE_CONSOL = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) consolidate '
    r'target=(\d+) members=(\d+)')
RE_REGATE = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) regate target=(\d+) '
    r'odds=([\d.]+) eta=(\d+) members=(\d+)')
RE_RETARG = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) retarget from=(\d+) '
    r'to=(\d+) staging=(\d+) members=(\d+) eta=(\d+) p=([\d.]+)')
RE_POACH = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) poach ')
RE_END = re.compile(
    r'deep-op turn=(\d+) player="([^"]+)" op=(\d+) '
    r'(held|abort|abort-odds|abort-wait|abort-march|abort-strike|'
    r'target-gone) target=(\d+) members=(\d+)')
RE_EMPLOYOP = re.compile(
    r'deep-employop turn=(\d+) player="([^"]+)" op=(\d+) target=(\d+) '
    r'muster=(\d+) formed=(\d+) launched=(\d+) waited=(\d+) held=(\d+) '
    r'members0=(\d+) peak=(\d+) final=(\d+) kills=(\d+) losses=(\d+) '
    r'why=(\S+) dep=dead=(\d+)/\d+,lost=(\d+)/\d+,defence=(\d+)/\d+,'
    r'poach=(\d+)/\d+,garrison=(\d+)/\d+')
RE_DYAD = re.compile(
    r'kill-dyad turn=(\d+) killer="([^"]+)" killerai="([^"]+)" '
    r'victim="([^"]+)" victimai="([^"]+)" utype="([^"]+)"')
RE_NETVAL = re.compile(
    r'deep-netval turn=(\d+) player="([^"]+)" op=(\d+) refuse '
    r'target=(\d+) force=(\d+) odds=([\d.]+) worth=([-\d.]+) '
    r'eworth=([-\d.]+) loss=([\d.]+) value=([-\d.]+) cost=([\d.]+)')


def open_log(path):
    if path.suffix == ".zst":
        p = subprocess.Popen(["zstdcat", str(path)], stdout=subprocess.PIPE)
        for raw in p.stdout:
            yield raw.decode("utf-8", "replace")
        p.wait()
    elif path.suffix == ".gz":
        with gzip.open(path, "rt", errors="replace") as fh:
            yield from fh
    else:
        with open(path, encoding="utf-8", errors="replace") as fh:
            yield from fh


def logs(root):
    for pat in ("*/seed-*/game.log", "*/seed-*/game.log.zst",
                "seed-*/game.log", "seed-*/game.log.zst"):
        found = sorted(Path(root).glob(pat))
        if found:
            for f in found:
                yield f
            return


# ------------------------------------------------------------- episodes

def scan_game(path):
    """One game -> (episodes, dyads, netvals)."""
    live = {}          # (player, slot) -> episode
    done = []
    dyads = []
    netvals = []
    seed = path.parent.name

    def close(key, turn, why, members):
        ep = live.pop(key, None)
        if ep is None:
            return
        ep["end_turn"] = turn
        ep["why"] = why
        ep["end_members"] = members
        done.append(ep)

    for line in open_log(path):
        if ("deep-op" not in line and "kill-dyad" not in line
                and "deep-employop" not in line
                and "deep-netval" not in line):
            continue
        m = RE_FORM.search(line)
        if m:
            turn, plr, slot = int(m.group(1)), m.group(2), int(m.group(3))
            key = (plr, slot)
            close(key, turn, "reformed", 0)   # defensive; should not fire
            live[key] = {
                "seed": seed, "player": plr, "slot": slot,
                "form_turn": turn, "kind": m.group(4),
                "target": int(m.group(5)), "staging": int(m.group(6)),
                "required": int(m.group(7)), "pool": int(m.group(8)),
                "drafted": int(m.group(9)),
                "odds0": None, "bar0": None, "eta0": None,
                "overseas": None, "def_n": None, "def_best": None,
                "worth": None, "prey": None, "preyunits": None,
                "march_turn": None, "present": None, "odds_m": None,
                "surv_m": None, "eta_m": None, "waited": None,
                "strike_turn": None, "strike_present": None,
                "consol_turn": None, "consol_members": None,
                "regates": 0, "retargets": 0, "poached": 0,
                "min_regate_odds": None, "regate_eta": None,
                "regate_members": None,
                "end_turn": None, "why": None, "end_members": None,
                "kills": None, "losses": None, "dead": None,
                "lost": None, "defence": None, "garrison": None,
                "poach_dep": None,
                "members0": None, "peak": None, "final": None,
            }
            continue
        m = RE_ODDS.search(line)
        if m and m.group(5) in ("formed", "muster", "muster-launch"):
            key = (m.group(2), int(m.group(3)))
            ep = live.get(key)
            if ep is not None and ep["odds0"] is None:
                ep["odds0"] = float(m.group(6))
                ep["bar0"] = float(m.group(7))
                ep["eta0"] = int(m.group(12))
                ep["overseas"] = int(m.group(13))
                ep["def_n"] = int(m.group(14))
                ep["def_best"] = int(m.group(15))
                ep["worth"] = int(m.group(16))
                ep["prey"] = float(m.group(17)) if m.group(17) else None
                ep["preyunits"] = int(m.group(18)) if m.group(18) else None
            continue
        m = RE_MARCH.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None and ep["march_turn"] is None:
                ep["march_turn"] = int(m.group(1))
                ep["present"] = int(m.group(5))
                ep["waited"] = int(m.group(7))
                ep["eta_m"] = int(m.group(8))
                ep["odds_m"] = float(m.group(9))
                ep["surv_m"] = float(m.group(10))
            continue
        m = RE_STRIKE.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None and ep["strike_turn"] is None:
                ep["strike_turn"] = int(m.group(1))
                ep["strike_present"] = int(m.group(5))
            continue
        m = RE_CONSOL.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None:
                ep["consol_turn"] = int(m.group(1))
                ep["consol_members"] = int(m.group(5))
            continue
        m = RE_REGATE.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None:
                ep["regates"] += 1
                o = float(m.group(5))
                if ep["min_regate_odds"] is None or o < ep["min_regate_odds"]:
                    ep["min_regate_odds"] = o
                    ep["regate_eta"] = int(m.group(6))
                    ep["regate_members"] = int(m.group(7))
            continue
        m = RE_RETARG.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None:
                ep["retargets"] += 1
            continue
        m = RE_POACH.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None:
                ep["poached"] += 1
            continue
        m = RE_EMPLOYOP.search(line)
        if m:
            ep = live.get((m.group(2), int(m.group(3))))
            if ep is not None:
                ep["members0"] = int(m.group(10))
                ep["peak"] = int(m.group(11))
                ep["final"] = int(m.group(12))
                ep["kills"] = int(m.group(13))
                ep["losses"] = int(m.group(14))
                ep["dead"] = int(m.group(16))
                ep["lost"] = int(m.group(17))
                ep["defence"] = int(m.group(18))
                ep["poach_dep"] = int(m.group(19))
                ep["garrison"] = int(m.group(20))
            continue
        m = RE_END.search(line)
        if m:
            close((m.group(2), int(m.group(3))), int(m.group(1)),
                  m.group(4), int(m.group(6)))
            continue
        m = RE_NETVAL.search(line)
        if m:
            netvals.append({"turn": int(m.group(1)), "player": m.group(2),
                            "odds": float(m.group(6)),
                            "eworth": float(m.group(8)),
                            "loss": float(m.group(9)),
                            "value": float(m.group(10)),
                            "cost": float(m.group(11))})
            continue
        m = RE_DYAD.search(line)
        if m:
            dyads.append({"turn": int(m.group(1)), "killer": m.group(2),
                          "victim": m.group(4), "utype": m.group(6)})

    for key in list(live):
        close(key, -1, "open-at-end", 0)
    return done, dyads, netvals


def attribute_dyads(eps, dyads):
    """Losses and kills booked inside each launched operation's window.

    A crude attribution and named as one: kill-dyad is empire-wide, so
    a unit this player lost to an unrelated defence in the same turns
    is charged to the operation. It is an UPPER bound on what the
    operation cost and an upper bound on what it bought, which is the
    honest direction for a census asking whether the trade is bad --
    if it looks bad on the upper bound of the take, it is bad.
    """
    by_victim = {}
    by_killer = {}
    for d in dyads:
        by_victim.setdefault(d["victim"], []).append(d["turn"])
        by_killer.setdefault(d["killer"], []).append(d["turn"])
    for k in by_victim:
        by_victim[k].sort()
    for k in by_killer:
        by_killer[k].sort()

    for ep in eps:
        if ep["march_turn"] is None:
            ep["win_lost"] = ep["win_killed"] = None
            continue
        lo, hi = ep["march_turn"], (ep["end_turn"] if ep["end_turn"] > 0
                                    else ep["march_turn"])
        v = by_victim.get(ep["player"], [])
        k = by_killer.get(ep["player"], [])
        ep["win_lost"] = bisect.bisect_right(v, hi) - bisect.bisect_left(v, lo)
        ep["win_killed"] = (bisect.bisect_right(k, hi)
                            - bisect.bisect_left(k, lo))


def scan_arm(root):
    eps, netvals, games = [], [], 0
    for path in logs(root):
        games += 1
        e, d, nv = scan_game(path)
        attribute_dyads(e, d)
        eps.extend(e)
        netvals.extend(nv)
    return games, eps, netvals


# ------------------------------------------------------------- reporting

def mean(xs):
    xs = [x for x in xs if x is not None]
    return statistics.fmean(xs) if xs else float("nan")


def pct(a, b):
    return 100.0 * a / b if b else float("nan")


def row(label, sel, eps):
    n = len(sel)
    if n == 0:
        print(f"  {label:<26} n=0")
        return
    marched = [e for e in sel if e["march_turn"] is not None]
    struck = [e for e in sel if e["strike_turn"] is not None]
    took = [e for e in sel if e["consol_turn"] is not None]
    turns = [e["end_turn"] - e["form_turn"] for e in sel
             if e["end_turn"] and e["end_turn"] > 0]
    print(f"  {label:<26} n={n:<5} march={pct(len(marched), n):5.1f}%"
          f" strike={pct(len(struck), n):5.1f}%"
          f" take={pct(len(took), n):5.1f}%"
          f" odds0={mean([e['odds0'] for e in sel]):.3f}"
          f" oddsM={mean([e['odds_m'] for e in marched]):.3f}"
          f" survM={mean([e['surv_m'] for e in marched]):5.2f}"
          f" pres={mean([e['present'] for e in marched]):4.1f}"
          f" req={mean([e['required'] for e in sel]):4.1f}"
          f" turns={mean(turns):5.1f}"
          f" lost={mean([e['win_lost'] for e in marched]):5.2f}"
          f" kill={mean([e['win_killed'] for e in marched]):5.2f}")


def calibration(eps, key, edges, label):
    print(f"\n  {label}")
    print(f"    {'bucket':<14} {'n':>5} {'take%':>7} {'E[surv]':>8}"
          f" {'realized final':>15} {'lost':>6} {'killed':>7}")
    for i in range(len(edges) - 1):
        lo, hi = edges[i], edges[i + 1]
        sel = [e for e in eps
               if e[key] is not None and lo <= e[key] < hi]
        if not sel:
            continue
        took = [e for e in sel if e["consol_turn"] is not None]
        print(f"    [{lo:.2f},{hi:.2f}){'':<3} {len(sel):>5}"
              f" {pct(len(took), len(sel)):>6.1f}%"
              f" {mean([e['surv_m'] for e in sel]):>8.2f}"
              f" {mean([e['end_members'] for e in sel]):>15.2f}"
              f" {mean([e['win_lost'] for e in sel]):>6.2f}"
              f" {mean([e['win_killed'] for e in sel]):>7.2f}")


def histogram(eps, key, label, buckets):
    vals = [e[key] for e in eps if e[key] is not None]
    if not vals:
        print(f"  {label}: none")
        return
    print(f"  {label} (n={len(vals)} mean={mean(vals):.2f}): " + " ".join(
        f"{lo}-{hi}:{sum(1 for v in vals if lo <= v <= hi)}"
        for lo, hi in buckets))


def attrition(eps):
    """What the march loses BEFORE the walls.

    The re-gate is the operations layer's own hysteresis: a marching
    wave is re-priced every turn against op_abort_floor, and it dies
    there far more often than at a city. Two causes are separable in
    the log -- the wave shrank (members at abort < present at march)
    or the garrison grew (members held, odds fell) -- and they call
    for different fixes, so the census splits them.
    """
    marched = [e for e in eps if e["march_turn"] is not None
               and e["end_members"] is not None and e["end_turn"] > 0]
    aborted = [e for e in marched if e["why"] and e["why"].startswith("abort")
               and e["strike_turn"] is None]
    if not aborted:
        print("  no aborted marches")
        return
    shrank = [e for e in aborted if e["end_members"] < e["present"]]
    held = [e for e in aborted if e["end_members"] >= e["present"]]
    print(f"  MARCHES THAT DIED BEFORE THE WALLS  n={len(aborted)}"
          f" of {len(marched)} marches ({pct(len(aborted), len(marched)):.1f}%)")
    print(f"    wave shrank  n={len(shrank):<4}"
          f" present={mean([e['present'] for e in shrank]):.2f}"
          f" -> end={mean([e['end_members'] for e in shrank]):.2f}"
          f" oddsM={mean([e['odds_m'] for e in shrank]):.3f}"
          f" -> regate={mean([e['min_regate_odds'] for e in shrank]):.3f}"
          f" turns_marching="
          f"{mean([e['end_turn'] - e['march_turn'] for e in shrank]):.1f}"
          f" eta {mean([e['eta_m'] for e in shrank]):.2f}"
          f"->{mean([e['regate_eta'] for e in shrank]):.2f}")
    print(f"    wave intact  n={len(held):<4}"
          f" present={mean([e['present'] for e in held]):.2f}"
          f" -> end={mean([e['end_members'] for e in held]):.2f}"
          f" oddsM={mean([e['odds_m'] for e in held]):.3f}"
          f" -> regate={mean([e['min_regate_odds'] for e in held]):.3f}"
          f" turns_marching="
          f"{mean([e['end_turn'] - e['march_turn'] for e in held]):.1f}"
          f" eta {mean([e['eta_m'] for e in held]):.2f}"
          f"->{mean([e['regate_eta'] for e in held]):.2f}")


def eta_report(eps):
    """The travel estimate is the defender's budget, so its error is
    the reinforcement model's error.

    eta at both seats is raw map distance over move rate: no terrain,
    no zones of control, no ferry queueing, no stock leash. Whatever
    the wave actually spends getting there is what the target actually
    had to build in -- and the model charged it eta x reinforce_rate
    virtual defenders, not the truth.
    """
    reached = [e for e in eps if e["strike_turn"] is not None
               and e["march_turn"] is not None]
    ended = [e for e in eps if e["march_turn"] is not None
             and e["end_turn"] and e["end_turn"] > 0
             and e["strike_turn"] is None]
    print("\n  ETA HONESTY (predicted travel vs realized)")
    if reached:
        print(f"    reached the walls   n={len(reached):<4}"
              f" eta_at_launch={mean([e['eta_m'] for e in reached]):.2f}"
              f" actual_turns="
              f"{mean([e['strike_turn'] - e['march_turn'] for e in reached]):.2f}"
              f" eta_at_formation={mean([e['eta0'] for e in reached]):.2f}"
              f" form->strike="
              f"{mean([e['strike_turn'] - e['form_turn'] for e in reached]):.2f}")
    if ended:
        print(f"    never arrived       n={len(ended):<4}"
              f" eta_at_launch={mean([e['eta_m'] for e in ended]):.2f}"
              f" turns_marching_before_giving_up="
              f"{mean([e['end_turn'] - e['march_turn'] for e in ended]):.2f}")


def pooled(all_eps):
    """One table over every arm given, counted rather than averaged.

    Percentages from separate arms cannot be averaged -- the arms have
    different denominators -- and hand-pooling them off the per-arm
    tables is how a report acquires an off-by-one it cannot explain.
    """
    eps = all_eps
    marched = [e for e in eps if e["march_turn"] is not None]
    print("\n" + "=" * 68)
    print(f"POOLED over {len(eps)} formed operations, {len(marched)} launched")
    struck = [e for e in eps if e["strike_turn"] is not None]
    took = [e for e in eps if e["consol_turn"] is not None]
    both = [e for e in took if e["strike_turn"] is not None]
    print(f"  formed {len(eps)} -> march {len(marched)}"
          f" ({pct(len(marched), len(eps)):.1f}%)"
          f" -> strike {len(struck)} ({pct(len(struck), len(eps)):.1f}%)"
          f" -> take {len(took)} ({pct(len(took), len(eps)):.1f}%)"
          f"; took after a fight {len(both)}"
          f" ({pct(len(both), len(eps)):.1f}%)")
    whys = {}
    for e in eps:
        whys[e["why"]] = whys.get(e["why"], 0) + 1
    print("  terminal: " + ", ".join(
        f"{k}={v}" for k, v in sorted(whys.items(), key=lambda kv: -kv[1])))

    def line(label, sel):
        st = [e for e in sel if e["strike_turn"] is not None]
        tk = [e for e in sel if e["consol_turn"] is not None]
        print(f"    {label:<24} n={len(sel):<5}"
              f" strike={len(st):>3} ({pct(len(st), len(sel)):5.1f}%)"
              f" take={len(tk):>3} ({pct(len(tk), len(sel)):5.1f}%)")

    print("  by launch eta:")
    for lo, hi in ((0, 1), (2, 2), (3, 4), (5, 99)):
        line(f"eta {lo}-{hi}",
             [e for e in marched if lo <= e["eta_m"] <= hi])
    print("  by quorum (`required`):")
    for lo, hi in ((1, 2), (3, 4), (5, 6), (7, 8), (9, 999)):
        line(f"required {lo}-{hi}",
             [e for e in marched if lo <= e["required"] <= hi])
    print("  by release:")
    line("cleared the bar", [e for e in marched if e["bar0"] is not None
                             and e["odds_m"] >= e["bar0"]])
    line("patience release", [e for e in marched if e["bar0"] is not None
                              and e["odds_m"] < e["bar0"]])
    print("  by geography and force composition (all formations):")
    line("land", [e for e in eps if e["overseas"] == 0])
    line("overseas", [e for e in eps if e["overseas"] == 1])
    line("drafted>0", [e for e in eps if e["drafted"] > 0])
    line("drafted=0", [e for e in eps if e["drafted"] == 0])
    print("  target garrison at formation:")
    for lo, hi in ((0, 0), (1, 5), (6, 10), (11, 20), (21, 999)):
        sel = [e for e in eps if e["def_best"] is not None
               and lo <= e["def_best"] <= hi]
        print(f"    def_best {lo}-{hi:<4} n={len(sel)}")
    emp = [e for e in eps if e["kills"] is not None]
    if emp:
        print(f"  per-operation ledger rows {len(emp)}:"
              f" member deaths={sum(e['dead'] for e in emp)}"
              f" kills at the walls={sum(e['kills'] for e in emp)}")
    print("=" * 68)


def funnel(roots):
    all_eps = []
    for root in roots:
        games, eps, netvals = scan_arm(root)
        all_eps.extend(eps)
        print(f"# {root}")
        print(f"  games {games}   assault episodes {len(eps)}"
              f"   ({len(eps) / games:.1f}/game)" if games else "  no games")
        if not eps:
            print()
            continue
        marched = [e for e in eps if e["march_turn"] is not None]
        struck = [e for e in eps if e["strike_turn"] is not None]
        took = [e for e in eps if e["consol_turn"] is not None]
        print(f"  FUNNEL  formed {len(eps)}"
              f" -> march {len(marched)} ({pct(len(marched), len(eps)):.1f}%)"
              f" -> strike {len(struck)} ({pct(len(struck), len(eps)):.1f}%)"
              f" -> take {len(took)} ({pct(len(took), len(eps)):.1f}%)")
        whys = {}
        for e in eps:
            whys[e["why"]] = whys.get(e["why"], 0) + 1
        print("  terminal: " + ", ".join(
            f"{k}={v}" for k, v in sorted(whys.items(),
                                          key=lambda kv: -kv[1])))
        print()
        row("all", eps, eps)
        row("land", [e for e in eps if e["overseas"] == 0], eps)
        row("overseas", [e for e in eps if e["overseas"] == 1], eps)
        row("undefended (def_n=0)",
            [e for e in eps if e["def_n"] == 0], eps)
        row("defended (def_n>=1)",
            [e for e in eps if e["def_n"] and e["def_n"] >= 1], eps)
        row("def_best>=15",
            [e for e in eps if e["def_best"] and e["def_best"] >= 15], eps)
        row("drafted>0", [e for e in eps if e["drafted"] > 0], eps)
        row("drafted=0", [e for e in eps if e["drafted"] == 0], eps)
        row("draft>=half of required",
            [e for e in eps if e["required"] > 0
             and e["drafted"] * 2 >= e["required"]], eps)
        row("under-quorum march",
            [e for e in marched if e["present"] < e["required"]], eps)
        row("full-quorum march",
            [e for e in marched if e["present"] >= e["required"]], eps)
        row("bar-clearing march",
            [e for e in marched if e["bar0"] is not None
             and e["odds_m"] >= e["bar0"]], eps)
        row("patience march (odds<bar)",
            [e for e in marched if e["bar0"] is not None
             and e["odds_m"] < e["bar0"]], eps)
        row("eta0<=2", [e for e in eps if e["eta0"] is not None
                        and e["eta0"] <= 2], eps)
        row("eta0>=6", [e for e in eps if e["eta0"] is not None
                        and e["eta0"] >= 6], eps)
        print()
        histogram(eps, "eta0", "formation eta",
                  [(0, 2), (3, 5), (6, 9), (10, 999)])
        histogram(eps, "def_n", "defenders at target",
                  [(0, 0), (1, 1), (2, 3), (4, 999)])
        histogram(eps, "def_best", "best defender rating",
                  [(0, 0), (1, 5), (6, 10), (11, 20), (21, 999)])
        histogram(eps, "required", "required force",
                  [(1, 2), (3, 5), (6, 10), (11, 999)])
        print()
        attrition(eps)
        print("  -- launches that CLEARED the bar --")
        attrition([e for e in eps if e["march_turn"] is not None
                   and e["bar0"] is not None and e["odds_m"] >= e["bar0"]])
        print("  -- patience releases (odds below the bar) --")
        attrition([e for e in eps if e["march_turn"] is not None
                   and e["bar0"] is not None and e["odds_m"] < e["bar0"]])
        eta_report(eps)
        print("\n  STRIKE RATE BY QUORUM SIZE (`required` is the k in"
              " deep_op_eta's k-th smallest arrival, so it gates contact)")
        for lo, hi in ((1, 2), (3, 4), (5, 6), (7, 8), (9, 99)):
            sel = [e for e in marched if lo <= e["required"] <= hi]
            if not sel:
                continue
            st = [e for e in sel if e["strike_turn"] is not None]
            tk = [e for e in sel if e["consol_turn"] is not None]
            print(f"    required {lo}-{hi:<3} n={len(sel):<4}"
                  f" strike={pct(len(st), len(sel)):5.1f}%"
                  f" take={pct(len(tk), len(sel)):5.1f}%"
                  f" present={mean([e['present'] for e in sel]):.2f}"
                  f" etaM={mean([e['eta_m'] for e in sel]):.2f}")
        print("\n  TAKE RATE BY LAUNCH ETA (raw-distance travel estimate)")
        for lo, hi in ((0, 1), (2, 2), (3, 4), (5, 99)):
            sel = [e for e in marched if lo <= e["eta_m"] <= hi]
            if not sel:
                continue
            st = [e for e in sel if e["strike_turn"] is not None]
            tk = [e for e in sel if e["consol_turn"] is not None]
            print(f"    eta {lo}-{hi:<3} n={len(sel):<4}"
                  f" strike={pct(len(st), len(sel)):5.1f}%"
                  f" take={pct(len(tk), len(sel)):5.1f}%"
                  f" oddsM={mean([e['odds_m'] for e in sel]):.3f}"
                  f" present={mean([e['present'] for e in sel]):.2f}")
        struck = [e for e in eps if e["strike_turn"] is not None]
        took = [e for e in eps if e["consol_turn"] is not None]
        both = [e for e in took if e["strike_turn"] is not None]
        print(f"\n  AT THE WALLS: struck={len(struck)}"
              f" of which took the city={len(both)}"
              f" ({pct(len(both), len(struck)):.1f}%);"
              f" takes without a strike (walk-in)={len(took) - len(both)}")
        calibration(marched, "odds_m",
                    [0.0, 0.05, 0.3, 0.6, 0.7, 0.9, 1.01],
                    "LAUNCH CALIBRATION: predicted P(take) vs realized")
        if netvals:
            print(f"\n  deep-netval refusals: {len(netvals)}"
                  f"  E[loss shields]={mean([n['loss'] for n in netvals]):.2f}"
                  f"  value={mean([n['value'] for n in netvals]):.1f}"
                  f"  cost={mean([n['cost'] for n in netvals]):.1f}")
        emp = [e for e in eps if e["kills"] is not None]
        if emp:
            print(f"\n  PER-OPERATION LEDGER (deep-employop, n={len(emp)})")
            print(f"    members0={mean([e['members0'] for e in emp]):.2f}"
                  f"  peak={mean([e['peak'] for e in emp]):.2f}"
                  f"  final={mean([e['final'] for e in emp]):.2f}")
            print(f"    ordered attacks at the walls:"
                  f" kills={mean([e['kills'] for e in emp]):.3f}"
                  f"  losses={mean([e['losses'] for e in emp]):.3f}")
            print(f"    departures: dead-in-transit"
                  f"={mean([e['dead'] for e in emp]):.2f}"
                  f"  captured/bribed={mean([e['lost'] for e in emp]):.2f}"
                  f"  recalled-to-defence"
                  f"={mean([e['defence'] for e in emp]):.2f}"
                  f"  poached={mean([e['poach_dep'] for e in emp]):.2f}"
                  f"  to-garrison={mean([e['garrison'] for e in emp]):.2f}")
            tot_dead = sum(e["dead"] for e in emp)
            tot_kill = sum(e["kills"] for e in emp)
            print(f"    TOTALS over {len(emp)} closed operations:"
                  f" member deaths={tot_dead}"
                  f"  kills at the walls={tot_kill}"
                  f"  exchange="
                  f"{tot_kill / tot_dead if tot_dead else 0:.3f} kills/death")
        print()
    if len(roots) > 1 and all_eps:
        pooled(all_eps)


# ---------------------------------------------------------------- scores

def scorelog(path):
    """turn -> {player -> {tag -> value}} for the three war counters."""
    tags = {}
    players = {}
    out = {}
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            f = line.split()
            if not f:
                continue
            if f[0] == "tag":
                tags[int(f[1])] = f[2]
            elif f[0] == "addplayer":
                players[int(f[2])] = f[3]
            elif f[0] == "data" and len(f) >= 5:
                turn, tag, pid, val = (int(f[1]), int(f[2]), int(f[3]),
                                       float(f[4]))
                name = tags.get(tag)
                if name in ("unitsbuilt", "unitskilled", "unitslost",
                            "munits", "score"):
                    out.setdefault(turn, {}).setdefault(
                        players.get(pid, str(pid)), {})[name] = val
    return out


def score_report(roots):
    for root in roots:
        rows = []
        slices = {}
        for path in sorted(Path(root).glob("*/seed-*/scorelog.txt")) or \
                sorted(Path(root).glob("seed-*/scorelog.txt")):
            sl = scorelog(path)
            if not sl:
                continue
            last = max(sl)
            for plr, vals in sl[last].items():
                if "unitskilled" in vals and "unitslost" in vals:
                    rows.append(vals)
            for cut in (50, 100, 150):
                near = [t for t in sl if t <= cut]
                if not near:
                    continue
                t = max(near)
                for vals in sl[t].values():
                    if "unitskilled" in vals:
                        slices.setdefault(cut, []).append(vals)
        if not rows:
            print(f"# {root}: no scorelogs")
            continue
        built = mean([r.get("unitsbuilt", 0) for r in rows])
        killed = mean([r.get("unitskilled", 0) for r in rows])
        lost = mean([r.get("unitslost", 0) for r in rows])
        print(f"# {root}")
        print(f"  player-games {len(rows)}  built={built:.2f}"
              f" killed={killed:.2f} lost={lost:.2f}"
              f" k/l={killed / lost if lost else float('nan'):.3f}"
              f" survive={(built - lost) / built if built else 0:.3f}"
              f" score={mean([r.get('score', 0) for r in rows]):.1f}")
        for cut in sorted(slices):
            rs = slices[cut]
            k = mean([r.get("unitskilled", 0) for r in rs])
            lo = mean([r.get("unitslost", 0) for r in rs])
            print(f"    T{cut:<4} built={mean([r.get('unitsbuilt', 0) for r in rs]):6.2f}"
                  f" killed={k:6.2f} lost={lo:6.2f}"
                  f" k/l={k / lo if lo else float('nan'):.3f}"
                  f" munits={mean([r.get('munits', 0) for r in rs]):5.2f}")


def dyad_report(roots):
    """What deep actually loses, by unit type and by whether the loss
    fell inside one of its own operations' march windows.

    kill-dyad books at the LOSS, so it counts every way a unit dies --
    an assault, a defence, a bribe, a lost transport. Splitting the
    same ledger by unit type is what separates "the wave died at the
    walls" from "the walls fell while the wave was away".
    """
    for root in roots:
        per_game_lost = []
        per_game_killed = []
        types = {}
        in_win = out_win = 0
        games = 0
        for path in logs(root):
            games += 1
            eps, dyads, _ = scan_game(path)
            wins = {}
            for e in eps:
                if e["march_turn"] is None:
                    continue
                hi = e["end_turn"] if e["end_turn"] > 0 else 999
                wins.setdefault(e["player"], []).append((e["march_turn"], hi))
            lost = killed = 0
            for d in dyads:
                if not d["victim"].startswith("P"):
                    continue
                lost += 1
                types[d["utype"]] = types.get(d["utype"], 0) + 1
                w = wins.get(d["victim"], [])
                if any(lo <= d["turn"] <= hi for lo, hi in w):
                    in_win += 1
                else:
                    out_win += 1
            for d in dyads:
                if d["killer"] != "none":
                    killed += 1
            per_game_lost.append(lost)
            per_game_killed.append(killed)
        if not games:
            continue
        print(f"# {root}")
        print(f"  games {games}  unit deaths logged/game"
              f" {mean(per_game_lost):.1f}")
        print(f"  deaths inside one of the victim's own march windows:"
              f" {in_win} ({pct(in_win, in_win + out_win):.1f}%),"
              f" outside: {out_win}")
        tot = sum(types.values())
        top = sorted(types.items(), key=lambda kv: -kv[1])[:14]
        print("  by unit type (share of all deaths):")
        for name, n in top:
            print(f"    {name:<16} {n:>6}  {pct(n, tot):5.1f}%"
                  f"  {n / games:6.2f}/game")
        print()


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    mode, roots = sys.argv[1], sys.argv[2:]
    if mode == "funnel":
        funnel(roots)
    elif mode == "score":
        score_report(roots)
    elif mode == "dyad":
        dyad_report(roots)
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
