#!/usr/bin/env python3
"""Census of deep's two unpriced shield lines: the DIPLOMAT and MIL-INFRA.

    diploprice.py DEEPDIR [BASEDIR] [--section diplomat|infra|all]

`findings/content-military-gold-2026-08-30.md` closed on two lines it
kept tripping over and never priced:

  * the Diplomat -- 138 shields + 43.5 gold per player-game, 12.4% of
    build picks at T26-50 and 11.9% at T51-75, all of it flowing from
    `dai_choose_diplomat_defensive()` whose want is a self-described
    "totally arbitrary 16000" (daidiplomat.c:130);
  * mil-infra (Barracks + City Walls + Coastal Defense) -- 290
    shields/player-game, larger than the whole content family.

This census asks what those shields BOUGHT. It reads finished games
only, so it can answer "how many, where are they standing at the end,
what did the other arm do instead" and it CANNOT answer "did the
defensive diplomat ever stop an incite" or "was a walled city ever
attacked". Section [D6] measures the one thing the end state does
answer about the trigger, and the gap is the instrumentation list.

THE TRIGGER, because every number here is downstream of it.
`dai_choose_diplomat_defensive()` fires on `city_data->diplomat_threat`,
whose comment reads "Enemy diplomat or spy is near the city"
(daicity.h:58). It is set at daimilitary.c:905 by

    if (utype_acts_hostile(unit_type_get(punit)) && 2 >= move_time)

and `utype_acts_hostile()` is `utype_can_do_action(putype,
ACTION_HOSTILE)`, which common/actres.c marks TRUE for ACTRES_ATTACK,
ACTRES_PILLAGE, ACTRES_BOMBARD and ACTRES_CONQUER_CITY as well as for
the four spy families. So an approaching Warriors sets the flag that
buys a Diplomat. [D6] measures how often the flag is true with no
enemy diplomat anywhere near, which is the false-positive rate of the
whole 138-shield line.

[D6]'s geometry, so it can be checked: unit and city coordinates are
saved NATIVE (savegame3.c:5180, :6175), the map is ISO|HEX, and
`real_map_distance()` runs on MAP coordinates, so the reader converts
with NATIVE_TO_MAP_POS (map.h:170), wraps in native space the way
`base_map_distance_vector()` does (map.c:1174), and applies the
iso-hex branch of `map_vector_to_real_distance()` (map.c:623). It is
a DISTANCE proxy for a `move_time` test, reported at several radii
rather than at one, because a Warriors at distance 2 and a Chariot at
distance 4 are both `move_time == 2`.

Read with findings/diploprice-verdict-2026-08-30.md.
"""
import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from contentcensus import (BUCKET, R_BUY, R_SWITCH, R_WANT, finalsave, games,
                           loglines, mean, player_games, ruleset_unit_cost,
                           ruleset_upkeep, sav_lines, scorelog, se, split_row)

DIPL = ("Diplomat", "Spy")
INFRA = ("Barracks", "Barracks II", "Barracks III", "City Walls",
         "Coastal Defense")

R_SPY = re.compile(r'deep-spy turn=(\d+) player="([^"]+)" on=(\d+) '
                   r'seen=(\d+) at_seam=(\d+) abstained=(\d+) refused=(\d+) '
                   r'embassy=(\d+) steal=(\d+) incite=(\d+) sabotage=(\d+)')

# --- topology --------------------------------------------------------


class Topo:
    """Native<->map conversion and real distance for one saved map."""

    def __init__(self, width, height, iso, hexa, wrapx, wrapy):
        self.w, self.h = width, height
        self.iso, self.hex = iso, hexa
        self.wrapx, self.wrapy = wrapx, wrapy

    def to_map(self, nx, ny):
        if not self.iso:
            return nx, ny
        mx = (ny + (ny & 1)) // 2 + nx
        return mx, ny - mx + self.w

    def dist(self, a, b):
        """a, b are (nat_x, nat_y)."""
        dx, dy = b[0] - a[0], b[1] - a[1]
        if self.wrapx:
            dx = (dx + self.w // 2) % self.w - self.w // 2
        if self.wrapy:
            dy = (dy + self.h // 2) % self.h - self.h // 2
        x0, y0 = self.to_map(a[0], a[1])
        x1, y1 = self.to_map(a[0] + dx, a[1] + dy)
        dx, dy = x1 - x0, y1 - y0
        adx, ady = abs(dx), abs(dy)
        if not self.hex:
            return max(adx, ady)
        if self.iso:
            return adx + ady if (dx < 0 < dy) or (dy < 0 < dx) \
                else max(adx, ady)
        return adx + ady if (dx > 0 and dy > 0) or (dx < 0 and dy < 0) \
            else max(adx, ady)


# --- savegame --------------------------------------------------------

def parse_rich(path):
    """-> (Topo, [player dicts]) with per-unit and per-city detail.

    contentcensus.parse_save() keeps only a unit-type Counter and a
    per-city improvement set; this census needs the coordinates, the
    veteran level, the birth turn and the city's `original` owner, so
    it re-walks the file rather than widening the shared reader.
    """
    players, impr, rows = [], [], []
    cur, mode, hdr = None, None, None
    iso = hexa = wrapx = wrapy = False
    endturn = 0
    for ln in sav_lines(path):
        if ln.startswith("turn=") and cur is None and endturn == 0:
            endturn = int(ln.split("=", 1)[1])
            continue
        if ln.startswith("improvement_vector="):
            impr = split_row(ln.split("=", 1)[1])
            continue
        if ln.startswith('"topology"'):
            v = split_row(ln)[1]
            iso, hexa = "ISO" in v, "HEX" in v
            continue
        if ln.startswith('"wrap"'):
            v = split_row(ln)[1]
            wrapx, wrapy = "WRAPX" in v, "WRAPY" in v
            continue
        if re.match(r"^t\d{4}=", ln):
            rows.append(ln.split("=", 1)[1].strip('"'))
            continue
        if re.match(r"^\[player\d+\]$", ln):
            cur = {"name": "", "ai_type": "", "cities": [], "units": [],
                   "built": 0, "killed": 0, "lost": 0}
            players.append(cur)
            mode = None
            continue
        if cur is None:
            continue
        if ln.startswith("c={"):
            hdr, mode = split_row(ln[3:]), "c"
            continue
        if ln.startswith("u={"):
            hdr, mode = split_row(ln[3:]), "u"
            continue
        if mode in ("c", "u"):
            if ln.startswith("}"):
                mode = None
                continue
            d = dict(zip(hdr, split_row(ln)))
            if mode == "c":
                bits = d.get("improvements", "")
                cur["cities"].append({
                    "xy": (int(d["x"]), int(d["y"])),
                    "size": int(d.get("size", 0) or 0),
                    "original": int(d.get("original", -1) or -1),
                    "founded": int(d.get("turn_founded", 0) or 0),
                    "impr": {impr[i] for i, b in enumerate(bits)
                             if b == "1" and i < len(impr)},
                })
            else:
                cur["units"].append({
                    "xy": (int(d["x"]), int(d["y"])),
                    "type": d.get("type_by_name", "?"),
                    "vet": int(d.get("veteran", 0) or 0),
                    "born": int(d.get("born", 0) or 0),
                    "act": int(d.get("activity", -1) or -1),
                    "home": int(d.get("homecity", 0) or 0),
                })
            continue
        if ln.startswith("["):
            mode = None
        elif ln.startswith("name="):
            cur["name"] = ln.split("=", 1)[1].strip('"')
        elif ln.startswith("ai_type="):
            cur["ai_type"] = ln.split("=", 1)[1].strip('"')
        elif ln.startswith("units_built="):
            cur["built"] = int(ln.split("=", 1)[1])
        elif ln.startswith("units_killed="):
            cur["killed"] = int(ln.split("=", 1)[1])
        elif ln.startswith("units_lost="):
            cur["lost"] = int(ln.split("=", 1)[1])
    width = len(rows[0]) if rows else 0
    topo = Topo(width, len(rows), iso, hexa, wrapx, wrapy)
    return topo, endturn, [p for p in players if p["name"].startswith("P")]


# --- ruleset ---------------------------------------------------------

def unit_traits(rulesdir):
    """-> {rule_name: (attack, class, frozenset(flags))}."""
    out, classflags = {}, {}
    name = cls = None
    attack = 0
    flags = []
    section = None
    for raw in open(Path(rulesdir) / "units.ruleset",
                    encoding="utf8", errors="replace"):
        ln = raw.split(";")[0].strip() if raw.lstrip().startswith(";") else raw.rstrip()
        if ln.startswith("["):
            if section == "unit" and name:
                out[name] = (attack, cls, frozenset(flags))
            elif section == "class" and name:
                classflags[name] = frozenset(flags)
            section = ("class" if ln.startswith("[unitclass_")
                       else "unit" if ln.startswith("[unit_") else None)
            name, cls, attack, flags = None, None, 0, []
            continue
        if ln.startswith("name") and "=" in ln and name is None:
            v = ln.split("=", 1)[1].strip()
            m = re.search(r'"(?:\?[^:]*:)?([^"]*)"', v)
            if m:
                name = m.group(1)
        elif ln.startswith("class"):
            m = re.search(r'"([^"]*)"', ln)
            if m:
                cls = m.group(1)
        elif ln.startswith("attack"):
            m = re.search(r"=\s*(\d+)", ln)
            if m:
                attack = int(m.group(1))
        elif ln.startswith("flags") or (flags and ln.startswith(" ")
                                        and '"' in ln):
            flags += re.findall(r'"([^"]*)"', ln)
    if section == "unit" and name:
        out[name] = (attack, cls, frozenset(flags))
    elif section == "class" and name:
        classflags[name] = frozenset(flags)
    return out, classflags


def hostile_types(rulesdir):
    """The set `utype_acts_hostile()` answers TRUE for, approximated.

    Exact would need the action-enabler evaluator. This takes the
    three families that actually populate an ancient-era map: anything
    with an attack strength (ACTRES_ATTACK), anything whose class can
    pillage (ACTRES_PILLAGE), and the Diplomat flag (the four spy
    families). It therefore UNDERCOUNTS -- every type it misses would
    make [D6]'s false-positive rate larger, not smaller.
    """
    traits, classflags = unit_traits(rulesdir)
    host, dipl = set(), set()
    for name, (atk, cls, flags) in traits.items():
        if "Diplomat" in flags:
            dipl.add(name)
            host.add(name)
        elif atk > 0 or "CanPillage" in classflags.get(cls, ()):
            host.add(name)
    return host, dipl


# --- log side --------------------------------------------------------

def log_ledger(root):
    """-> (picks by band, starts, built, aband, burnt, bought, spy)."""
    picks = defaultdict(Counter)
    starts, built, aband, burnt = Counter(), Counter(), Counter(), Counter()
    bandbuilt = defaultdict(Counter)
    bought, boughtgold = Counter(), Counter()
    spy = defaultdict(dict)          # game -> player -> tuple
    n = 0
    for d in games(root):
        n += 1
        for ln in loglines(d):
            if "deep-want " in ln:
                m = R_WANT.search(ln)
                if m:
                    b = (int(m.group(1)) - 1) // BUCKET * BUCKET + 1
                    picks[b][m.group(4)] += 1
            elif "deep-prodswitch " in ln:
                m = R_SWITCH.search(ln)
                if m:
                    if m.group(5):
                        starts[m.group(5)] += 1
                    if m.group(4):
                        bb = (int(m.group(1)) - 1) // BUCKET * BUCKET + 1
                        if m.group(11) == "1":
                            built[m.group(4)] += 1
                            bandbuilt[bb][m.group(4)] += 1
                        else:
                            aband[m.group(4)] += 1
                            burnt[m.group(4)] += int(m.group(10))
            elif "deep-buy " in ln:
                m = R_BUY.search(ln)
                if m:
                    bought[m.group(4)] += 1
                    boughtgold[m.group(4)] += int(m.group(5))
            elif "deep-spy " in ln:
                m = R_SPY.search(ln)
                if m:
                    spy[d.name][m.group(2)] = tuple(
                        int(m.group(i)) for i in range(4, 12))
    return dict(picks=picks, starts=starts, built=built, aband=aband,
                burnt=burnt, bandbuilt=bandbuilt, bought=bought,
                boughtgold=boughtgold, spy=spy, ngames=n)


# --- save side -------------------------------------------------------

def save_census(root, rulesdir):
    host, dipl = hostile_types(rulesdir)
    acc = Counter()
    vets = Counter()
    radii = (1, 2, 3, 4)
    trig = {r: Counter() for r in radii}
    npg = 0
    for d in games(root):
        f = finalsave(d)
        if f is None:
            continue
        topo, endturn, players = parse_rich(f)
        npg += len(players)
        allunits = [(pi, u) for pi, p in enumerate(players) for u in p["units"]]
        for pi, p in enumerate(players):
            acc["cities"] += len(p["cities"])
            acc["built"] += p["built"]
            acc["killed"] += p["killed"]
            acc["lost"] += p["lost"]
            acc["units"] += len(p["units"])
            citytiles = {c["xy"] for c in p["cities"]}
            for u in p["units"]:
                if u["type"] in dipl:
                    acc["dipl"] += 1
                    acc["dipl_age"] += max(0, endturn - u["born"])
                    if u["xy"] in citytiles:
                        acc["dipl_home"] += 1
                    # ACTIVITY_IDLE 0 / FORTIFIED 4 / SENTRY 5 /
                    # GOTO 7 (fc_types.h:139).
                    acc["dipl_act%d" % u["act"]] += 1
                elif u["type"] in host:
                    acc["mil"] += 1
                    vets[u["vet"]] += 1
            for c in p["cities"]:
                for k in INFRA:
                    if k in c["impr"]:
                        acc[k] += 1
                if c["original"] != pi:
                    acc["taken"] += 1
                # [D6]: what is standing near this city at the end. One
                # pass over the foreign roster, keeping the nearest
                # hostile-capable unit and the nearest diplomat, then
                # every radius is a comparison rather than a rescan.
                nh = nd = 10 ** 6
                for qi, u in allunits:
                    if qi == pi:
                        continue
                    if u["type"] not in host:
                        continue
                    dd = topo.dist(c["xy"], u["xy"])
                    if dd < nh:
                        nh = dd
                    if u["type"] in dipl and dd < nd:
                        nd = dd
                for r in radii:
                    trig[r]["n"] += 1
                    if nh <= r:
                        trig[r]["host"] += 1
                        if nd <= r:
                            trig[r]["both"] += 1
                    if nd <= r:
                        trig[r]["dipl"] += 1
    return acc, vets, trig, npg, radii


# --- scorelog side ---------------------------------------------------

def score_series(root, tags=("score", "cities", "settlers", "unitsbuilt",
                             "unitskilled", "unitslost", "munits"),
                 turns=(50, 75, 150)):
    """-> {(tag, turn): [per-player values across games]}."""
    out = defaultdict(list)
    for d in games(root):
        s, players = scorelog(d / "scorelog.txt")
        for tag in tags:
            for t in turns:
                row = s.get(tag, {}).get(t)
                if row:
                    out[(tag, t)] += list(row.values())
    return out


# --- report ----------------------------------------------------------

def section_diplomat(deep, base, rules, rulesdir):
    print("=" * 72)
    print("DIPLOMAT")
    print("=" * 72)
    L = log_ledger(deep)
    npg = max(player_games(deep), 1)

    print(f"\n[D1] Diplomat share of deep-want picks by band "
          f"({L['ngames']} games, city-turns)")
    for b in sorted(L["picks"]):
        tot = sum(L["picks"][b].values())
        d = sum(L["picks"][b][k] for k in DIPL)
        print(f"  T{b:>3}-{b+BUCKET-1:<3} n={tot:<6} diplomat {d:>5} "
              f"({100*d/max(tot,1):>4.1f}%)")

    print("\n[D2] the Diplomat build ledger, per player-game")
    print(f"  {'item':<14} {'starts':>7} {'built':>6} {'aband':>6} "
          f"{'deliv%':>7} {'delivered':>10} {'burnt':>7} {'buys':>5} "
          f"{'buygold':>8}")
    for k in DIPL:
        cost = rules.get(k, (0, 0))[0]
        st, bl, ab = L["starts"][k], L["built"][k], L["aband"][k]
        if st == 0 and bl == 0:
            continue
        print(f"  {k:<14} {st:>7} {bl:>6} {ab:>6} "
              f"{100*bl/max(st,1):>6.1f}% {bl*cost/npg:>10.1f} "
              f"{L['burnt'][k]/npg:>7.1f} {L['bought'][k]:>5} "
              f"{L['boughtgold'][k]/npg:>8.1f}")
    heads = sorted(L["bandbuilt"])
    print("  delivered shields per player-game by band: " + "  ".join(
        f"T{b}-{b+BUCKET-1}: "
        f"{sum(L['bandbuilt'][b][k]*rules.get(k,(0,0))[0] for k in DIPL)/npg:.1f}"
        for b in heads))

    print("\n[D3] deep-spy: what a diplomat-turn was, per player-game")
    print("     seen = diplomat-turns with movement left; at_seam = the")
    print("     unit stood beside a foreign city with a LEGAL action;")
    print("     abstained = at the seam and deep priced nothing above 0.")
    tot = Counter()
    ngp = 0
    for g, byp in L["spy"].items():
        for p, v in byp.items():
            ngp += 1
            for i, k in enumerate(("seen", "at_seam", "abstained", "refused",
                                   "embassy", "steal", "incite", "sabotage")):
                tot[k] += v[i]
    if ngp:
        print(f"  player-games with a deep-spy line: {ngp}")
        print(f"  {'seen':>8} {'at_seam':>9} {'seam%':>7} {'abstain':>8} "
              f"{'acted':>6}")
        acted = sum(tot[k] for k in ("embassy", "steal", "incite", "sabotage"))
        print(f"  {tot['seen']/ngp:>8.1f} {tot['at_seam']/ngp:>9.1f} "
              f"{100*tot['at_seam']/max(tot['seen'],1):>6.1f}% "
              f"{tot['abstained']/ngp:>8.1f} {acted:>6}")

    for label, root in (("deep", deep), ("base", base)):
        if root is None:
            continue
        acc, vets, trig, npg2, radii = save_census(root, rulesdir)
        n = max(npg2, 1)
        print(f"\n[D4/{label}] final-save diplomat stock, per player-game "
              f"({npg2} player-games)")
        print(f"  diplomats alive {acc['dipl']/n:>6.2f}   "
              f"in one of our own cities {100*acc['dipl_home']/max(acc['dipl'],1):>5.1f}%   "
              f"idle {100*acc['dipl_act0']/max(acc['dipl'],1):>5.1f}%  "
              f"sentry {100*acc['dipl_act5']/max(acc['dipl'],1):>5.1f}%  "
              f"goto {100*acc['dipl_act7']/max(acc['dipl'],1):>5.1f}%   "
              f"mean age {acc['dipl_age']/max(acc['dipl'],1):>5.1f} turns")
        print(f"  units alive {acc['units']/n:>6.2f}  built {acc['built']/n:>6.2f}  "
              f"killed {acc['killed']/n:>5.2f}  lost {acc['lost']/n:>5.2f}  "
              f"cities {acc['cities']/n:>5.2f}  taken-from-someone "
              f"{acc['taken']/n:>5.2f}")
        print(f"\n[D6/{label}] the trigger, measured on the end state: "
              f"share of cities with an enemy")
        print("  HOSTILE-capable unit within r, and with an enemy DIPLOMAT "
              "within r.")
        print(f"  {'r':>3} {'cities':>7} {'hostile':>8} {'diplomat':>9} "
              f"{'P(dipl|hostile)':>16}")
        for r in radii:
            t = trig[r]
            print(f"  {r:>3} {t['n']:>7} {100*t['host']/max(t['n'],1):>7.1f}% "
                  f"{100*t['dipl']/max(t['n'],1):>8.1f}% "
                  f"{100*t['both']/max(t['host'],1):>15.1f}%")


def section_infra(deep, base, rules, rulesdir):
    print("\n" + "=" * 72)
    print("MIL-INFRA")
    print("=" * 72)
    L = log_ledger(deep)
    npg = max(player_games(deep), 1)
    print("\n[M1] the mil-infra build ledger, per player-game (deep arm)")
    print(f"  {'item':<18} {'starts':>7} {'built':>6} {'aband':>6} "
          f"{'deliv%':>7} {'delivered':>10} {'burnt':>7} {'buys':>5} "
          f"{'buygold':>8}")
    for k in INFRA:
        cost = rules.get(k, (0, 0))[0]
        st, bl, ab = L["starts"][k], L["built"][k], L["aband"][k]
        if st == 0 and bl == 0:
            continue
        print(f"  {k:<18} {st:>7} {bl:>6} {ab:>6} "
              f"{100*bl/max(st,1):>6.1f}% {bl*cost/npg:>10.1f} "
              f"{L['burnt'][k]/npg:>7.1f} {L['bought'][k]:>5} "
              f"{L['boughtgold'][k]/npg:>8.1f}")
    heads = sorted(L["bandbuilt"])
    print("  delivered shields per player-game by band: " + "  ".join(
        f"T{b}-{b+BUCKET-1}: "
        f"{sum(L['bandbuilt'][b][k]*rules.get(k,(0,0))[0] for k in INFRA)/npg:.1f}"
        for b in heads))

    for label, root in (("deep", deep), ("base", base)):
        if root is None:
            continue
        acc, vets, trig, npg2, radii = save_census(root, rulesdir)
        n = max(npg2, 1)
        print(f"\n[M2/{label}] mil-infra standing at the end, per "
              f"player-game, and coverage per city")
        for k in INFRA:
            if acc[k] == 0:
                continue
            print(f"  {k:<18} {acc[k]/n:>6.2f}/player  "
                  f"{100*acc[k]/max(acc['cities'],1):>5.1f}% of cities")
        tv = sum(vets.values())
        print(f"  surviving military units {acc['mil']/n:>6.2f}/player; "
              f"veteran level mix: " + "  ".join(
                  f"v{k}={100*v/max(tv,1):.1f}%" for k, v in sorted(vets.items())))

    for label, root in (("deep", deep), ("base", base)):
        if root is None:
            continue
        s = score_series(root)
        print(f"\n[M3/{label}] scorelog means per player at T50 / T75 / end")
        for tag in ("cities", "settlers", "munits", "unitsbuilt",
                    "unitskilled", "unitslost", "score"):
            cells = []
            for t in (50, 75, 150):
                v = s.get((tag, t), [])
                cells.append(f"{mean(v):>8.2f}+-{se(v):<5.2f}" if v
                             else f"{'-':>14}")
            print(f"  {tag:<12} " + "  ".join(cells))


def section_cells(dirs, rules):
    """One row per factorial cell: the two shield lines and the gates.

    The A/B cells keep no savegames (disk), so this reads the log side
    only -- which is the side the gates act on. `refuse%` is the gate
    firing; `deliv%` and `delivered` are what the epic promised to
    move, and for mil-infra the pre-registered direction is STARTS
    down with DELIVERED flat.
    """
    print(f"  {'cell':<28} {'games':>5} "
          f"{'dip.start':>9} {'dip.blt':>8} {'dip.dlv%':>9} {'dip.sh':>7} "
          f"{'dipRef%':>8} "
          f"{'inf.start':>9} {'inf.blt':>8} {'inf.dlv%':>9} {'inf.sh':>7} "
          f"{'infRef%':>8}")
    for d in dirs:
        d = Path(d)
        root = d / "deep" if (d / "deep").is_dir() else d
        L = log_ledger(root)
        npg = max(player_games(root), 1)
        ref = {"dip": [0, 0], "inf": [0, 0]}
        for g in games(root):
            for ln in loglines(g):
                if "deep-dipthreat " in ln and " verdict=" in ln:
                    ref["dip"][1] += 1
                    if "verdict=refuse" in ln:
                        ref["dip"][0] += 1
                elif "deep-milinfra " in ln:
                    ref["inf"][1] += 1
                    if "verdict=refuse" in ln:
                        ref["inf"][0] += 1
        cells = []
        for fam, key in ((DIPL, "dip"), (INFRA, "inf")):
            st = sum(L["starts"][k] for k in fam)
            bl = sum(L["built"][k] for k in fam)
            sh = sum(L["built"][k] * rules.get(k, (0, 0))[0] for k in fam)
            r, n = ref[key]
            cells += [f"{st:>9}", f"{bl:>8}", f"{100*bl/max(st,1):>8.1f}%",
                      f"{sh/npg:>7.1f}", f"{100*r/max(n,1):>7.1f}%"]
        print(f"  {d.parent.name + '/' + d.name:<28} {L['ngames']:>5} "
              + " ".join(cells))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("deep")
    ap.add_argument("base", nargs="?")
    ap.add_argument("more", nargs="*")
    ap.add_argument("--section", default="all",
                    choices=("all", "diplomat", "infra", "cells"))
    ap.add_argument("--rules", default=None)
    a = ap.parse_args()
    rulesdir = a.rules or (Path(__file__).resolve().parent.parent
                           / "data" / "civ2civ3")
    rules = ruleset_upkeep(rulesdir)
    rules.update({k: v for k, v in ruleset_unit_cost(rulesdir).items()
                  if k not in rules})
    if a.section == "cells":
        section_cells([a.deep] + ([a.base] if a.base else []) + a.more,
                      rules)
        return
    deep = Path(a.deep)
    base = Path(a.base) if a.base else None
    if a.section in ("all", "diplomat"):
        section_diplomat(deep, base, rules, rulesdir)
    if a.section in ("all", "infra"):
        section_infra(deep, base, rules, rulesdir)


if __name__ == "__main__":
    sys.exit(main())
