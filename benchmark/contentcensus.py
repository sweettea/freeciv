#!/usr/bin/env python3
"""Content / military / gold census: three secondary signatures of the
defaults deficit, each localized to the seat that produces it.

  contentcensus.py [--section S] DEEPDIR [BASEDIR]

DEEPDIR and BASEDIR are directories of finished games (`seed-N/` with
`DONE`, `scorelog.txt`, `game.log[.zst]` and `saves/*final.sav.zst`),
the deep arm and its paired baseline arm on the same seeds and maps.
BASEDIR may be omitted; every deep-only block still prints.

`--section` selects `content`, `military`, `gold` or `all` (default).

CONTENT answers where the Colosseum spree comes from.  Stock cannot
price a Make_Content building at all (advdata.c's impr_calc switch has
the happiness family `#if 0`'d out), so a Colosseum's ENTIRE want is
deep's `deep_content_credit()`.  The `deep-decay fam=content` line
carries that credit's shape, and its `delay` field is the diagnostic:
`deep_content_credit()` passes `delay = (load > 0 ? 0 : ttg)`, so

    delay == 0   the city has citizens on pacification duty TODAY
    delay >  0   it has none, and the credit is for citizens the
                 growth clock is expected to deliver later

which splits the spree into responsive and speculative without a new
log line.  `deep-decay-sum` carries `pre` (stock's want before deep
touched it) and `add` (what deep added), so the two authors of the
final want are separable per improvement.

MILITARY follows the early army back to the seat that ordered it:
every `deep-want` pick of a military unit is joined to that city's
`deep-def` line for the same turn (danger, urgency, menace, ndef) and
to `deep-density` (the neighbour-count fear prior), so a unit ordered
against a measured threat is separated from one ordered against a
prior.  Whether the threat was real is read off the scorelog: cities
lost and units lost before T50, both arms.

GOLD is a ledger, not a level.  Per player-turn the scorelog gives
gross revenue (`bnp` x `taxrate`, bnp being the corruption-adjusted
trade surplus) and the treasury delta, so

    outflow = revenue - delta

is everything the treasury paid for that turn: upkeep, buys, incite,
less coinage and windfalls.  Deep's buys are itemized (`deep-buy`) so
they come out of the residual; the baseline arm logs nothing, so its
buys stay inside it and the standing upkeep from the final save is
what separates the two.  Both arms' final saves give the exact
end-state bill: buildings x the ruleset's own upkeep figure.

Read with findings/content-military-gold-2026-08-30.md.
"""
import argparse
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path

BUCKET = 25

# --- log line shapes -------------------------------------------------

R_WANT = re.compile(r'deep-want turn=(\d+) city=(\d+) producer="([^"]*)" '
                    r'pick="([^"]*)" native=(\S+) tier=(\d+) value=(\S+)')
R_DECAY = re.compile(r'deep-decay turn=(\d+) player="([^"]+)" city=(\d+) '
                     r'impr="([^"]+)" fam=(\w+) b=(\S+) delay=(\d+) '
                     r'steps=(\S+) spacing=(\S+) cap=(\S+) credit=(\S+) '
                     r'dur=(\S+)')
R_DSUM = re.compile(r'deep-decay-sum turn=(\d+) player="([^"]+)" city=(\d+) '
                    r'impr="([^"]+)" pre=(\S+) raw=(\S+) capadd=(\S+) '
                    r'gmult=(\S+) thr=(\d+) upk=(\S+) ob=(\S+) keep=(\S+) '
                    r'add=(\S+)')
R_SWITCH = re.compile(r'deep-prodswitch turn=(\d+) player="([^"]+)" '
                      r'city=(\d+) from="([^"]*)" to="([^"]*)" '
                      r'fromk=(\S) tok=(\S) stock=(\d+) after=(\d+) '
                      r'loss=(\d+).*builtlast=(\d)')
R_BUY = re.compile(r'deep-buy turn=(\d+) player="([^"]+)" city=(\d+) '
                   r'kind="([^"]+)" gold=(\d+) shields=(\d+) want=(\S+) '
                   r'urgency=(\d+) tier="([^"]+)"')
R_FIN = re.compile(r'deep-fin turn=(\d+) player="([^"]+)" gold=(-?\d+) '
                   r'projected=(-?\d+) sell="([^"]+)" city=(\d+)')
R_DEF = re.compile(r'deep-def turn=(\d+) player="([^"]+)" city=(\d+) '
                   r'danger=(\d+) urgency=(\d+) defense=(\d+) ndef=(\d+) '
                   r'prod="([^"]*)" shield=(-?\d+) food=(-?\d+) '
                   r'menace=(\S+)')
R_DENS = re.compile(r'deep-density turn=(\d+) player="([^"]+)" city=(\d+) '
                    r'near=(\d+) cross=(\d+) discounted=(\d+) raw=(\S+) '
                    r'gated=(\S+) ramp=(\S+) fear=(\d+)')

# Units in civ2civ3 that `munits` counts; the census only needs to tell
# a military pick from a civilian one, which the pick name does.
CIVILIAN = {"Settlers", "Workers", "Engineers", "Migrants", "Diplomat",
            "Spy", "Caravan", "Freight", "Explorer", "Barbarian Leader",
            "Leader"}

# Coarse spend classes for the band ledger. Deliberately crude: the
# question is where a band's shields go, not a taxonomy.
CLASSES = {
    "military": ("Warriors", "Phalanx", "Archers", "Pikemen", "Legion",
                 "Musketeers", "Riflemen", "Horsemen", "Chariot",
                 "Knights", "Cavalry", "Catapult", "Marines", "Trireme",
                 "Galleon", "Destroyer", "Ironclad", "Transport",
                 "Frigate", "Caravel", "Dragoons", "Alpine Troops",
                 "Cannon", "Elephants", "Crusaders", "Fanatics"),
    "mil-infra": ("Barracks", "Barracks II", "Barracks III", "City Walls",
                  "Coastal Defense", "Great Wall", "SAM Battery",
                  "SDI Defense", "Port Facility", "Sun Tzu's War Academy"),
    "content": ("Colosseum", "Temple", "Cathedral", "Police Station",
                "Hanging Gardens", "Mausoleum of Mausolos",
                "Temple of Artemis", "Michelangelo's Chapel",
                "J.S. Bach's Cathedral", "Shakespeare's Theatre"),
    "expansion": ("Settlers", "Migrants"),
    "civil": ("Workers", "Engineers"),
    "diplomat": ("Diplomat", "Spy"),
}
# Coinage is a shield SINK, not a 999-shield build: it never "completes",
# so it must not be priced at its nominal cost.
SKIP = {"Coinage"}


def loglines(gamedir):
    zst = gamedir / "game.log.zst"
    if zst.exists():
        out = subprocess.run(["zstdcat", str(zst)], capture_output=True).stdout
        return out.decode("utf8", "replace").splitlines()
    p = gamedir / "game.log"
    if not p.exists():
        return []
    return p.read_text(errors="replace").splitlines()


# --- scorelog --------------------------------------------------------

def scorelog(path):
    """-> (tagname -> {turn -> {player -> value}}, players)."""
    tags, players = {}, {}
    out = defaultdict(lambda: defaultdict(dict))
    for line in open(path):
        f = line.split()
        if not f:
            continue
        if f[0] == "tag":
            tags[int(f[1])] = f[2]
        elif f[0] == "addplayer":
            players[int(f[2])] = f[3]
        elif f[0] == "data" and len(f) >= 5:
            t, tg, pl, v = int(f[1]), int(f[2]), int(f[3]), float(f[4])
            if tg in tags:
                out[tags[tg]][t][pl] = v
    return out, players


# --- savegame --------------------------------------------------------

def sav_lines(path):
    out = subprocess.run(["zstdcat", str(path)], capture_output=True).stdout
    return out.decode("utf8", "replace").splitlines()


def split_row(row):
    """Split one savegame table row on commas outside double quotes."""
    fields, cur, inq = [], [], False
    for ch in row:
        if ch == '"':
            inq = not inq
            continue
        if ch == "," and not inq:
            fields.append("".join(cur))
            cur = []
            continue
        cur.append(ch)
    fields.append("".join(cur))
    return fields


def parse_save(path):
    """-> (improvement_vector, [player dicts]).

    Each player dict: name, ai_type, gov, gold, cities (list of dicts
    with size/improvements set/building), units (Counter of type name).
    """
    impr = []
    players = []
    cur = None
    mode = None
    hdr = None
    for ln in sav_lines(path):
        if ln.startswith("improvement_vector="):
            # Names contain commas ("Aqueduct, Lake"), so the row splitter
            # is the only correct reader here -- splitting on bare commas
            # shifts every index past the first such name.
            impr = split_row(ln.split("=", 1)[1])
            continue
        if re.match(r"^\[player\d+\]$", ln):
            cur = {"name": "", "ai_type": "", "gov": "", "gold": 0,
                   "cities": [], "units": Counter()}
            players.append(cur)
            mode = None
            continue
        if cur is None:
            continue
        if ln.startswith("["):
            mode = None
            continue
        if mode == "c" or mode == "u":
            if ln.startswith("}"):
                mode = None
                continue
            row = split_row(ln)
            d = dict(zip(hdr, row))
            if mode == "c":
                bits = d.get("improvements", "")
                built = {impr[i] for i, b in enumerate(bits)
                         if b == "1" and i < len(impr)}
                cur["cities"].append({
                    "size": int(d.get("size", 0) or 0),
                    "improvements": built,
                    "building": d.get("currently_building_name", ""),
                    "kind": d.get("currently_building_kind", ""),
                    "founded": int(d.get("turn_founded", 0) or 0),
                })
            else:
                cur["units"][d.get("type_by_name", "?")] += 1
            continue
        if ln.startswith("c={"):
            # "cma_minimal_surplus,1" is one quoted column name.
            hdr = split_row(ln[3:])
            mode = "c"
            continue
        if ln.startswith("u={"):
            hdr = split_row(ln[3:])
            mode = "u"
            continue
        if ln.startswith("name="):
            cur["name"] = ln.split("=", 1)[1].strip('"')
        elif ln.startswith("ai_type="):
            cur["ai_type"] = ln.split("=", 1)[1].strip('"')
        elif ln.startswith("government_name="):
            cur["gov"] = ln.split("=", 1)[1].strip('"')
        elif ln.startswith("gold="):
            cur["gold"] = int(ln.split("=", 1)[1])
    return impr, [p for p in players if p["name"].startswith("P")]


def ruleset_upkeep(rulesdir):
    """-> {rule_name: (build_cost, gold upkeep)} from buildings.ruleset."""
    p = Path(rulesdir) / "buildings.ruleset"
    out = {}
    name = rule = None
    cost = upk = 0
    for ln in p.read_text(errors="replace").splitlines():
        ln = ln.strip()
        if ln.startswith("[building_"):
            if rule or name:
                out[rule or name] = (cost, upk)
            name = rule = None
            cost = upk = 0
        elif ln.startswith("name") and "=" in ln and "_(" in ln:
            name = ln.split('_("', 1)[1].split('"')[0]
            if name.startswith("?") and ":" in name:
                name = name.split(":", 1)[1]
        elif ln.startswith("rule_name"):
            rule = ln.split('"')[1]
        elif ln.startswith("build_cost"):
            cost = int(ln.split("=", 1)[1].strip())
        elif ln.startswith("upkeep"):
            upk = int(ln.split("=", 1)[1].strip())
    if rule or name:
        out[rule or name] = (cost, upk)
    return out


def ruleset_unit_cost(rulesdir):
    """-> {rule_name: (build_cost, 0)} from units.ruleset, so the shield
    ledger can price a unit run the same way it prices a building."""
    p = Path(rulesdir) / "units.ruleset"
    out = {}
    name = rule = None
    cost = 0
    for ln in p.read_text(errors="replace").splitlines():
        ln = ln.strip()
        if ln.startswith("[unit_"):
            if rule or name:
                out[rule or name] = (cost, 0)
            name = rule = None
            cost = 0
        elif ln.startswith("name") and "_(" in ln:
            # "?unit:Workers" -- the translator qualifier is not the name.
            name = ln.split('_("', 1)[1].split('"')[0]
            if name.startswith("?") and ":" in name:
                name = name.split(":", 1)[1]
        elif ln.startswith("rule_name"):
            rule = ln.split('"')[1]
        elif ln.startswith("build_cost"):
            cost = int(ln.split("=", 1)[1].strip())
    if rule or name:
        out[rule or name] = (cost, 0)
    return out


def player_games(root):
    """Number of (player, game) pairs, read off the scorelogs."""
    n = 0
    for d in games(root):
        for line in open(d / "scorelog.txt"):
            if line.startswith("addplayer "):
                n += 1
    return n


def games(root):
    for d in sorted(Path(root).glob("seed-*")):
        if (d / "DONE").exists():
            yield d


def finalsave(d):
    got = sorted(d.glob("saves/*final.sav.zst"))
    return got[-1] if got else None


def mean(xs):
    return sum(xs) / len(xs) if xs else float("nan")


def se(xs):
    if len(xs) < 2:
        return float("nan")
    m = mean(xs)
    v = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return (v / len(xs)) ** 0.5


# --- section: content ------------------------------------------------

def section_content(deep, base, rules):
    print("=" * 72)
    print("CONTENT: where the Colosseum want comes from")
    print("=" * 72)

    picks = defaultdict(Counter)          # bucket -> pick -> city-turns
    credit = defaultdict(lambda: Counter())   # impr -> counters
    csum = defaultdict(lambda: [0.0, 0.0])    # impr -> [pre, add]
    starts = Counter()
    builtlast = Counter()
    abandoned = Counter()
    bandbuilt = defaultdict(Counter)
    lostshields = Counter()
    heldshields = Counter()
    sold = Counter()
    bought = Counter()
    boughtgold = Counter()
    ngames = 0

    for d in games(deep):
        ngames += 1
        for ln in loglines(d):
            if "deep-want " in ln:
                m = R_WANT.search(ln)
                if m:
                    b = (int(m.group(1)) - 1) // BUCKET * BUCKET + 1
                    picks[b][m.group(4)] += 1
            elif "deep-decay " in ln:
                m = R_DECAY.search(ln)
                if m and m.group(5) == "content":
                    im = m.group(4)
                    c = credit[im]
                    c["n"] += 1
                    if int(m.group(7)) == 0:
                        c["load_now"] += 1
                    else:
                        c["speculative"] += 1
                        c["delay_sum"] += int(m.group(7))
                    c["credit_sum"] += float(m.group(11))
                    c["b_sum"] += float(m.group(6))
                    c["steps_sum"] += float(m.group(8))
            elif "deep-decay-sum " in ln:
                m = R_DSUM.search(ln)
                if m:
                    csum[m.group(4)][0] += float(m.group(5))
                    csum[m.group(4)][1] += float(m.group(13))
            elif "deep-prodswitch " in ln:
                m = R_SWITCH.search(ln)
                if m:
                    if m.group(5):
                        starts[m.group(5)] += 1
                    if m.group(4):
                        bb = (int(m.group(1)) - 1) // BUCKET * BUCKET + 1
                        if m.group(11) == "1":
                            builtlast[m.group(4)] += 1
                            bandbuilt[bb][m.group(4)] += 1
                        else:
                            # the run ended without a delivery: the
                            # penalty this switch charges is charged
                            # against the stock the ABANDONED item built
                            abandoned[m.group(4)] += 1
                            lostshields[m.group(4)] += int(m.group(10))
                            heldshields[m.group(4)] += int(m.group(8))
            elif "deep-fin " in ln:
                m = R_FIN.search(ln)
                if m:
                    sold[m.group(5)] += 1
            elif "deep-buy " in ln:
                m = R_BUY.search(ln)
                if m:
                    bought[m.group(4)] += 1
                    boughtgold[m.group(4)] += int(m.group(5))

    print(f"\n[1] deep-want winners by 25-turn band ({ngames} games, "
          f"city-turns; a line per city per turn, so this is the share "
          f"of city-turns each item held, not of starts)")
    for b in sorted(picks):
        tot = sum(picks[b].values())
        top = picks[b].most_common(6)
        print(f"  T{b:>3}-{b+BUCKET-1:<3} n={tot:<6} " +
              "  ".join(f"{k} {100*v/tot:.1f}%" for k, v in top))

    print("\n[2] deep_content_credit(): who gets it and is it responsive")
    print(f"  {'improvement':<24} {'lines':>6} {'load>0':>7} {'spec':>7} "
          f"{'spec%':>6} {'credit/line':>12} {'mean delay':>10}")
    for im, c in sorted(credit.items(), key=lambda kv: -kv[1]["n"]):
        sp = c["speculative"]
        print(f"  {im:<24} {c['n']:>6} {c['load_now']:>7} {sp:>7} "
              f"{100*sp/c['n']:>5.1f}% {c['credit_sum']/c['n']:>12.1f} "
              f"{(c['delay_sum']/sp if sp else 0):>10.1f}")

    print("\n[3] who authored the final want (deep-decay-sum totals)")
    print(f"  {'improvement':<24} {'stock pre':>12} {'deep add':>12} "
          f"{'deep share':>11}")
    for im, (pre, add) in sorted(csum.items(), key=lambda kv: -kv[1][1])[:12]:
        tot = pre + add
        sh = 100 * add / tot if tot else 0.0
        print(f"  {im:<24} {pre:>12.0f} {add:>12.0f} {sh:>10.1f}%")

    print("\n[4] the shield ledger of each build item, per PLAYER-game")
    print("    starts/built/aband are runs, not shields: `built` is a run "
          "that ended\n    with builtlast=1, `aband` one that ended "
          "without. `delivered` is\n    built x the ruleset cost; "
          "`burnt` is the change-production penalty\n    the abandoning "
          "switch actually destroyed; `carried` is the stock those\n"
          "    abandoned runs were holding when they ended (kept if the "
          "next item is\n    the same class, destroyed in `burnt` if "
          "not).")
    npg = max(player_games(deep), 1)
    print(f"  {'item':<22} {'starts':>7} {'built':>6} {'aband':>6} "
          f"{'deliv%':>7} {'cost':>5} {'upk':>4} {'delivered':>10} "
          f"{'burnt':>7} {'carried':>8} {'sold':>5} {'bought':>7}")
    order = sorted((set(starts) | set(builtlast)) - SKIP,
                   key=lambda k: -(builtlast[k] * rules.get(k, (0, 0))[0]))
    for im in order[:18]:
        cost, upk = rules.get(im, (0, 0))
        st, bl, ab = starts[im], builtlast[im], abandoned[im]
        print(f"  {im:<22} {st:>7} {bl:>6} {ab:>6} "
              f"{100*bl/max(st,1):>6.1f}% {cost:>5} {upk:>4} "
              f"{bl*cost/npg:>10.1f} {lostshields[im]/npg:>7.1f} "
              f"{heldshields[im]/npg:>8.1f} {sold[im]:>5} "
              f"{bought[im]:>7}")

    print("\n[4b] delivered shields per player-game by 25-turn band -- the "
          "same\n     runs as [4], grouped by the band the delivery landed "
          "in. Read the\n     band where the city-count gap opens, not the "
          "whole-game total.")
    heads = sorted(bandbuilt)
    print(f"  {'class':<12} " + " ".join(f"T{b}-{b+BUCKET-1:<3}"
                                         for b in heads))
    for cls, members in CLASSES.items():
        cells = []
        for b in heads:
            cells.append(sum(bandbuilt[b][m] * rules.get(m, (0, 0))[0]
                             for m in members) / npg)
        print(f"  {cls:<12} " + " ".join(f"{c:>8.1f}" for c in cells))
    for cls in ("other",):
        cells = []
        known = {m for ms in CLASSES.values() for m in ms}
        for b in heads:
            cells.append(sum(v * rules.get(k, (0, 0))[0]
                             for k, v in bandbuilt[b].items()
                             if k not in known and k not in SKIP) / npg)
        print(f"  {cls:<12} " + " ".join(f"{c:>8.1f}" for c in cells))

    # realized unhappiness, both arms
    if base:
        print("\n[5] what unhappiness actually materialized (scorelog, "
              "per-player means over the arm)")
        rows = []
        for label, root in (("deep", deep), ("base", base)):
            acc = defaultdict(list)
            for d in games(root):
                sl, plrs = scorelog(d / "scorelog.txt")
                turns = sorted(sl.get("riots", {}))
                for t in turns:
                    if t % BUCKET and t != turns[-1]:
                        continue
                    for tag in ("riots", "unhappypop", "specialists",
                                "contentpop", "happypop", "luxrate"):
                        vals = list(sl.get(tag, {}).get(t, {}).values())
                        if vals:
                            acc[(t, tag)].append(sum(vals) / len(vals))
            rows.append((label, acc))
        turnset = sorted({t for _, a in rows for (t, _) in a})
        print(f"  {'turn':>5} " + " ".join(
            f"{tag[:9]:>10}" for tag in
            ("riots", "unhappypop", "specialists", "contentpop", "luxrate")))
        for t in turnset:
            for label, acc in rows:
                cells = []
                for tag in ("riots", "unhappypop", "specialists",
                            "contentpop", "luxrate"):
                    cells.append(f"{mean(acc.get((t, tag), [])):10.2f}")
                print(f"  {t:>5} " + " ".join(cells) + f"   {label}")

    # end-state inventory from the final saves
    print("\n[6] end-state building inventory (final saves, per player)")
    inv = {}
    for label, root in (("deep", deep), ("base", base)):
        if not root:
            continue
        c = Counter()
        n = 0
        for d in games(root):
            f = finalsave(d)
            if not f:
                continue
            _, plrs = parse_save(f)
            for p in plrs:
                n += 1
                for city in p["cities"]:
                    for b in city["improvements"]:
                        c[b] += 1
        inv[label] = (c, n)
    if inv:
        keys = set()
        for c, _ in inv.values():
            keys |= set(c)
        print(f"  {'improvement':<24} " +
              " ".join(f"{l:>10}" for l in inv) + f"{'ratio':>9}")
        allk = sorted(keys, key=lambda k: -max(c[k] for c, _ in inv.values()))
        for k in allk[:20]:
            cells, vals = [], []
            for label in inv:
                c, n = inv[label]
                v = c[k] / n if n else 0.0
                vals.append(v)
                cells.append(f"{v:>10.2f}")
            r = vals[0] / vals[1] if len(vals) > 1 and vals[1] else float("inf")
            print(f"  {k:<24} " + " ".join(cells) + f"{r:>9.2f}")


# --- section: military -----------------------------------------------

def section_military(deep, base, rules):
    print("=" * 72)
    print("MILITARY: who orders the early army, and against what")
    print("=" * 72)

    ngames = 0
    milpick = defaultdict(Counter)          # bucket -> unit -> city-turns
    # the danger the ordering city saw, joined on (game, turn, city)
    joined = defaultdict(lambda: Counter())  # bucket -> counters
    dens = defaultdict(lambda: Counter())
    for d in games(deep):
        ngames += 1
        defs = {}
        dns = {}
        wants = []
        for ln in loglines(d):
            if "deep-def " in ln:
                m = R_DEF.search(ln)
                if m:
                    defs[(int(m.group(1)), m.group(3))] = m
            elif "deep-density " in ln:
                m = R_DENS.search(ln)
                if m:
                    dns[(int(m.group(1)), m.group(3))] = m
            elif "deep-want " in ln:
                m = R_WANT.search(ln)
                if m:
                    wants.append(m)
        for m in wants:
            t, city, pick = int(m.group(1)), m.group(2), m.group(4)
            if pick in CIVILIAN or pick[0].islower():
                continue
            b = (t - 1) // BUCKET * BUCKET + 1
            dm = defs.get((t, city))
            if dm is None:
                continue
            milpick[b][pick] += 1
            j = joined[b]
            j["n"] += 1
            j["danger_sum"] += int(dm.group(4))
            j["danger0"] += 1 if int(dm.group(4)) == 0 else 0
            j["urgency_sum"] += int(dm.group(5))
            j["urgency0"] += 1 if int(dm.group(5)) == 0 else 0
            j["menace_sum"] += float(dm.group(11))
            j["menace0"] += 1 if float(dm.group(11)) == 0 else 0
            j["ndef_sum"] += int(dm.group(7))
            j["tier_e"] += 1 if int(m.group(6)) else 0
            nm = dns.get((t, city))
            if nm:
                dd = dens[b]
                dd["n"] += 1
                dd["near_sum"] += int(nm.group(4))
                dd["fear_sum"] += int(nm.group(10))
                dd["raw_sum"] += float(nm.group(7))

    print(f"\n[1] military picks by band and the danger the ordering city "
          f"saw ({ngames} games; only picks whose city wrote a deep-def "
          f"line the same turn)")
    print(f"  {'band':<11} {'picks':>7} {'danger=0':>9} {'urgency=0':>10} "
          f"{'menace=0':>9} {'mean menace':>12} {'mean ndef':>10} "
          f"{'nearby':>8} {'fear':>6}")
    for b in sorted(joined):
        j = joined[b]
        dd = dens.get(b, Counter())
        n = max(j["n"], 1)
        dn = max(dd["n"], 1)
        print(f"  T{b:>3}-{b+BUCKET-1:<6} {j['n']:>7} "
              f"{100*j['danger0']/n:>8.1f}% {100*j['urgency0']/n:>9.1f}% "
              f"{100*j['menace0']/n:>8.1f}% {j['menace_sum']/n:>12.2f} "
              f"{j['ndef_sum']/n:>10.2f} {dd['near_sum']/dn:>8.2f} "
              f"{dd['fear_sum']/dn:>6.2f}")
    for b in sorted(milpick):
        tot = sum(milpick[b].values())
        print(f"  T{b:>3}-{b+BUCKET-1:<6} " + "  ".join(
            f"{k} {100*v/tot:.0f}%" for k, v in milpick[b].most_common(5)))

    if not base:
        return
    print("\n[2] standing army and its exchange rate, both arms "
          "(scorelog, per-player means)")
    tags = ("munits", "settlers", "unitsbuilt", "unitslost", "unitskilled",
            "cities", "mfg", "score")
    marks = (1, 5, 10, 15, 20, 25, 30, 40, 50, 60, 75, 100, 125)
    series = {}
    for label, root in (("deep", deep), ("base", base)):
        acc = defaultdict(list)
        for d in games(root):
            sl, _ = scorelog(d / "scorelog.txt")
            turns = sorted(sl.get("munits", {}))
            for t in turns:
                if t not in marks and t != turns[-1]:
                    continue
                for tag in tags:
                    vals = list(sl.get(tag, {}).get(t, {}).values())
                    if vals:
                        acc[(t, tag)].append(sum(vals) / len(vals))
        series[label] = acc
    turnset = sorted({t for a in series.values() for (t, _) in a})
    print(f"  {'turn':>5} " + " ".join(f"{tg[:10]:>11}" for tg in tags))
    for t in turnset:
        for label, acc in series.items():
            print(f"  {t:>5} " + " ".join(
                f"{mean(acc.get((t, tg), [])):11.2f}" for tg in tags)
                + f"   {label}")


# --- section: gold ---------------------------------------------------

def gold_ledger(root, rules, want_buys):
    """Per player-game gold ledger from scorelog + (deep) deep-buy."""
    rows = []
    for d in games(root):
        sl, plrs = scorelog(d / "scorelog.txt")
        turns = sorted(sl.get("gold", {}))
        if not turns:
            continue
        buys = Counter()
        sells = Counter()
        if want_buys:
            for ln in loglines(d):
                if "deep-buy " in ln:
                    m = R_BUY.search(ln)
                    if m:
                        buys[m.group(2)] += int(m.group(5))
                elif "deep-fin " in ln:
                    m = R_FIN.search(ln)
                    if m:
                        sells[m.group(2)] += 1
        per = defaultdict(lambda: Counter())
        for i, t in enumerate(turns[:-1]):
            nxt = turns[i + 1]
            for pid, g in sl["gold"][t].items():
                g2 = sl["gold"].get(nxt, {}).get(pid)
                bnp = sl.get("bnp", {}).get(t, {}).get(pid)
                tax = sl.get("taxrate", {}).get(t, {}).get(pid)
                if g2 is None or bnp is None or tax is None:
                    continue
                p = per[pid]
                p["rev"] += bnp * tax / 100.0
                p["delta"] += g2 - g
                p["bnp"] += bnp
        for pid, p in per.items():
            name = plrs.get(pid, str(pid))
            rows.append({
                "seed": d.name, "player": name,
                "rev": p["rev"], "delta": p["delta"],
                "outflow": p["rev"] - p["delta"],
                "bnp": p["bnp"],
                "buys": buys.get(name, 0),
                "sells": sells.get(name, 0),
                "gold_end": sl["gold"][turns[-1]].get(pid, 0),
                "gold_mean": mean([sl["gold"][t].get(pid, 0)
                                   for t in turns]),
            })
    return rows


def standing_upkeep(root, rules):
    """End-state building gold bill per player from the final saves."""
    out = []
    for d in games(root):
        f = finalsave(d)
        if not f:
            continue
        _, plrs = parse_save(f)
        for p in plrs:
            bill = 0
            nb = 0
            for c in p["cities"]:
                for b in c["improvements"]:
                    bill += rules.get(b, (0, 0))[1]
                    nb += 1
            out.append({"gold": p["gold"], "bill": bill, "nbldg": nb,
                        "ncity": len(p["cities"]),
                        "nunit": sum(p["units"].values()),
                        "gov": p["gov"]})
    return out


def section_gold(deep, base, rules):
    print("=" * 72)
    print("GOLD: a per-player-game ledger, both arms")
    print("=" * 72)
    print("\n  revenue = sum_t bnp_t * taxrate_t / 100 (bnp is the "
          "corruption-adjusted\n  trade surplus, so this is gross tax "
          "before any bill). outflow = revenue -\n  treasury delta: "
          "every gold the treasury paid out, less coinage and\n  "
          "windfalls. Deep's buys are itemized; baseline logs none, so "
          "its buys\n  stay inside its outflow.")
    print(f"\n  {'arm':>6} {'games':>6} {'revenue':>9} {'delta':>8} "
          f"{'outflow':>9} {'buys':>8} {'out-buys':>9} {'gold end':>9} "
          f"{'gold mean':>10} {'sells':>6}")
    for label, root in (("deep", deep), ("base", base)):
        if not root:
            continue
        rows = gold_ledger(root, rules, label == "deep")
        if not rows:
            continue
        n = len(rows)
        print(f"  {label:>6} {n:>6} {mean([r['rev'] for r in rows]):>9.0f} "
              f"{mean([r['delta'] for r in rows]):>8.0f} "
              f"{mean([r['outflow'] for r in rows]):>9.0f} "
              f"{mean([r['buys'] for r in rows]):>8.0f} "
              f"{mean([r['outflow']-r['buys'] for r in rows]):>9.0f} "
              f"{mean([r['gold_end'] for r in rows]):>9.1f} "
              f"{mean([r['gold_mean'] for r in rows]):>10.1f} "
              f"{mean([r['sells'] for r in rows]):>6.2f}")

    print(f"\n  end-state standing bill (final save; ruleset upkeep, no "
          f"Upkeep_Free effects)")
    print(f"  {'arm':>6} {'plr-games':>10} {'gold':>7} {'buildings':>10} "
          f"{'bldg bill/turn':>15} {'cities':>7} {'units':>7} "
          f"{'bill/city':>10}")
    for label, root in (("deep", deep), ("base", base)):
        if not root:
            continue
        rows = standing_upkeep(root, rules)
        if not rows:
            continue
        print(f"  {label:>6} {len(rows):>10} "
              f"{mean([r['gold'] for r in rows]):>7.1f} "
              f"{mean([r['nbldg'] for r in rows]):>10.2f} "
              f"{mean([r['bill'] for r in rows]):>15.2f} "
              f"{mean([r['ncity'] for r in rows]):>7.2f} "
              f"{mean([r['nunit'] for r in rows]):>7.2f} "
              f"{mean([r['bill']/max(r['ncity'],1) for r in rows]):>10.2f}")

    # deep's buys, itemized
    print("\n  deep's purchases, itemized (gold per player-game)")
    per = Counter()
    cnt = Counter()
    npg = 0
    for d in games(deep):
        sl, plrs = scorelog(d / "scorelog.txt")
        npg += len(plrs)
        for ln in loglines(d):
            if "deep-buy " in ln:
                m = R_BUY.search(ln)
                if m:
                    per[m.group(4)] += int(m.group(5))
                    cnt[m.group(4)] += 1
    for k, v in per.most_common(12):
        print(f"    {k:<22} {cnt[k]:>5} buys  {v/max(npg,1):>7.1f} gold")
    print(f"    {'TOTAL':<22} {sum(cnt.values()):>5} buys  "
          f"{sum(per.values())/max(npg,1):>7.1f} gold")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("deep")
    ap.add_argument("base", nargs="?")
    ap.add_argument("--section", default="all",
                    choices=("all", "content", "military", "gold"))
    ap.add_argument("--rules", default=None,
                    help="ruleset dir (default: data/civ2civ3 beside this)")
    a = ap.parse_args()
    rulesdir = a.rules or (Path(__file__).resolve().parent.parent
                           / "data" / "civ2civ3")
    rules = ruleset_upkeep(rulesdir)
    rules.update({k: v for k, v in ruleset_unit_cost(rulesdir).items()
                  if k not in rules})
    deep, base = Path(a.deep), Path(a.base) if a.base else None
    if a.section in ("all", "content"):
        section_content(deep, base, rules)
    if a.section in ("all", "military"):
        section_military(deep, base, rules)
    if a.section in ("all", "gold"):
        section_gold(deep, base, rules)


if __name__ == "__main__":
    sys.exit(main())
