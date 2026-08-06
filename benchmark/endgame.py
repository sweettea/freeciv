#!/usr/bin/env python3
"""Census of the endgame horizon and the score mode (session ENDGAME).

    endgame.py RESULTS_DIR [RESULTS_DIR...] [--arm deep] [--window N]

Two mechanisms are reported apart, because they are measured apart and
they answer different questions.

  THE CLAMP is arithmetic. `deep-endgame clamps=/missed=` counts, per
  player per turn, the horizon credits whose planning horizon the bell
  actually shortened (MORT -> end_turn - turn) and the credits refused
  because their first instalment lands after the last turn. It fires
  only inside MORT turns of the end and is zero everywhere else, which
  is what makes "the clamp cannot regress mid-game behaviour" a fact
  about the code and not a hope about the score. The `first=` column
  below is the earliest turn any clamp fired in the arm: if it is
  smaller than ENDTURN - 24, something is wrong with the reading of the
  bell rather than with the mechanism.

  THE MODE is doctrine. `refused=/paid=` counts improvement wants
  deflated for being unfinishable and great wonders paid the completion
  premium; `deep-endgame-unit` is the same deflator on the unit half of
  the auction, one line per event, with the turns-to-build that failed
  the test.

Beside them, the two outcome quantities the session's acceptance asks
for and neither counter can supply:

  BUILD DISTRIBUTION in the final `--window` turns, off the deep-want
  line -- what the auction actually picked once the bell was in view,
  by pick name and by tier. A mode that executes and does not change
  what gets built is a mode that does nothing, and the counters alone
  cannot tell those apart.

  WONDERS COMPLETED in that same window, read off the scorelog's own
  `wonders` tag rather than any intention log: a wonder that exists is
  a wonder that got built. Reported as completions inside the window
  against completions over the whole game, because the wonder premium's
  entire claim is that it moves the FORMER.
"""

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

NUM = re.compile(r"(\w+)=(-?[\d.]+)")
STR = re.compile(r"(\w+)=\"([^\"]*)\"")


def parse(line):
    d = {k: float(v) for k, v in NUM.findall(line)}
    d.update({k: v for k, v in STR.findall(line)})
    return d


def q(vals, w=8, p=1):
    v = sorted(vals)
    if not v:
        return ("%*s" % (w, "--")) * 3
    n = len(v)
    return "".join("%*.*f" % (w, p, x)
                   for x in (v[n // 10], v[n // 2], v[(9 * n) // 10]))


def wonder_series(scorelog):
    """{player: {turn: wonders}} for the benchmark seats P1..Pn."""
    tags, players, data = {}, {}, defaultdict(dict)
    for line in scorelog.split("\n"):
        p = line.split(" ")
        if p[0] == "tag":
            tags[int(p[1])] = p[2]
        elif p[0] == "addplayer":
            players[int(p[2])] = " ".join(p[3:])
        elif p[0] == "data":
            turn, tag, plr = int(p[1]), int(p[2]), int(p[3])
            data[plr].setdefault(turn, {})[tag] = p[4]
    wid = next((t for t, n in tags.items() if n == "wonders"), None)
    if wid is None:
        return {}
    out = {}
    for plr, turns in data.items():
        if not re.fullmatch(r"P\d+", players.get(plr, "")):
            continue
        out[plr] = {t: int(v[wid]) for t, v in sorted(turns.items())
                    if wid in v}
    return out


def census(d, arm, window):
    gl = sorted(d.glob("%s/seed-*/game.log" % arm))
    ng = max(len(gl), 1)

    eg, units = [], []
    picks_in = Counter()
    picks_out = Counter()
    tier_in = Counter()
    last_turn = 0
    wond_window = 0
    wond_total = 0
    seats = 0

    for g in gl:
        want_rows = []
        for line in g.read_text(errors="ignore").split("\n"):
            if "deep-endgame " in line:
                eg.append(parse(line))
            elif "deep-endgame-unit " in line:
                units.append(parse(line))
            elif "deep-want " in line:
                r = parse(line)
                if "turn" in r:
                    want_rows.append(r)
                    last_turn = max(last_turn, int(r["turn"]))

        # The bell for THIS game: the last turn anything was logged on.
        end = max((int(r["turn"]) for r in want_rows), default=0)
        cut = end - window
        for r in want_rows:
            if int(r["turn"]) > cut:
                picks_in[r.get("pick", "?")] += 1
                tier_in[int(r.get("tier", 0))] += 1
            else:
                picks_out[r.get("pick", "?")] += 1

        sl = g.parent / "scorelog.txt"
        if sl.exists():
            for plr, series in wonder_series(sl.read_text(errors="ignore")).items():
                seats += 1
                turns = sorted(series)
                if not turns:
                    continue
                wond_total += series[turns[-1]] - series[turns[0]]
                inwin = [t for t in turns if t > (turns[-1] - window)]
                if len(inwin) >= 2:
                    wond_window += series[inwin[-1]] - series[inwin[0]]

    print("=" * 74)
    print("%s   games=%d  arm=%s  window=%d" % (d.name, len(gl), arm, window))
    print("=" * 74)

    print("\n1. THE CLAMP (endgame_horizon; arithmetic, no window)")
    if not eg:
        print("   0.00 deep-endgame lines -- the clamp never executed")
    else:
        clamps = sum(r.get("clamps", 0) for r in eg)
        missed = sum(r.get("missed", 0) for r in eg)
        first = min(int(r["turn"]) for r in eg
                    if r.get("clamps", 0) > 0 or r.get("missed", 0) > 0)
        print("   player-turns with any clamp  %9.2f /game" % (len(eg) / ng))
        print("   credits clamped              %9.2f /game" % (clamps / ng))
        print("   credits REFUSED (missed)     %9.2f /game" % (missed / ng))
        print("   earliest turn any fired      %9d" % first)
        print("   horizon left  p10/p50/p90 %s"
              % q([r.get("horizon", 0) for r in eg], 8, 0))

    print("\n2. THE SCORE MODE (endgame_window)")
    refused = sum(r.get("refused", 0) for r in eg)
    paid = sum(r.get("paid", 0) for r in eg)
    print("   improvement wants deflated   %9.2f /game" % (refused / ng))
    print("   great wonders paid           %9.2f /game" % (paid / ng))
    print("   unit wants deflated          %9.2f /game" % (len(units) / ng))
    if units:
        print("   ... their turns-to-build p10/p50/p90 %s"
              % q([r["ttb"] for r in units], 8, 1))
        print("   ... turns left at the time p10/p50/p90 %s"
              % q([r["left"] for r in units], 8, 1))
        print("   ... want removed p10/p50/p90 %s"
              % q([r["want"] - r["to"] for r in units], 8, 1))
        who = Counter(r.get("who", "?") for r in units)
        print("   ... by seam  " + "  ".join("%s=%d" % kv for kv in who.items()))

    print("\n3. WHAT THE AUCTION PICKED IN THE LAST %d TURNS" % window)
    tot = max(sum(picks_in.values()), 1)
    print("   build decisions in window    %9.2f /game" % (tot / ng))
    print("   ... emergency tier           %9.2f /game  (%.1f%%)"
          % (tier_in[1] / ng, 100.0 * tier_in[1] / tot))
    print("   pick                       in-window   share   rest-of-game")
    for name, n in picks_in.most_common(12):
        print("   %-24s %9.2f  %6.1f%%   %9.2f"
              % (name[:24], n / ng, 100.0 * n / tot, picks_out[name] / ng))

    print("\n4. WONDERS COMPLETED (scorelog `wonders`, deep seats only)")
    print("   seats read                   %9d" % seats)
    print("   completed, whole game        %9.2f /seat"
          % (wond_total / max(seats, 1)))
    print("   completed, last %2d turns     %9.2f /seat"
          % (window, wond_window / max(seats, 1)))


def main():
    args = list(sys.argv[1:])
    arm, window = "deep", 15
    if "--arm" in args:
        i = args.index("--arm")
        arm = args[i + 1]
        del args[i:i + 2]
    if "--window" in args:
        i = args.index("--window")
        window = int(args[i + 1])
        del args[i:i + 2]
    for a in args:
        census(Path(a), arm, window)


main()
