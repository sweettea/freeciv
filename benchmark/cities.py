#!/usr/bin/env python3
"""City conquest ledger from deep-city log lines.

    cities.py --root DIR [--arm ARM]

Walks DIR for */seed-*/DONE game directories (see run.sh) and parses
the `deep-city` lines the ai/deep city_got/city_lost hooks write. A
got with founded==turn is a founding; a got with founded<turn is a
conquest; every lost is a defence failure (taken or destroyed -- the
server does not say which). Also pools the deep-surv and deep-tired
tripwires so the combat-workstream distributions ride along: mean
survival probability under threat, share of threatened city-turns
below p=0.5, and tired-attack holds. Only games whose players run
ai/deep produce any of these lines.

Per game and pooled: foundings / conquests / losses per 100 turns.
The conquest and loss rates are the mechanism metrics for the combat
and invasion epics (cities taken, cities kept).
"""

import argparse
import re
import sys
from pathlib import Path

CITY_RE = re.compile(
    r'deep-city turn=(\d+) player="([^"]*)" (got|lost)=(\d+)'
    r' size=(\d+) founded=(-?\d+)'
)
SURV_RE = re.compile(
    r'deep-surv turn=(\d+) player="([^"]*)" city=(\d+) p=([0-9.]+)'
    r' wave=(\d+) fear=(\d+)'
)
TIRED_RE = re.compile(r'deep-tired turn=(\d+) player="([^"]*)"')


def final_turn(scorelog):
    turn = 0
    with open(scorelog, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("turn "):
                turn = max(turn, int(line.split(" ")[1]))
    return turn


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--arm", help="restrict to this arm subdirectory")
    args = ap.parse_args()

    pattern = f"{args.arm or '*'}/seed-*/DONE"
    gamedirs = sorted(p.parent for p in Path(args.root).glob(pattern))
    if not gamedirs:
        sys.exit(f"no completed games under {args.root}/{pattern}")

    tot = {"founded": 0, "conquered": 0, "lost": 0, "turns": 0,
           "surv_n": 0, "surv_sum": 0.0, "surv_low": 0, "tired": 0}
    print("arm,seed,turns,founded,conquered,lost,conq_per_100t,"
          "lost_per_100t,surv_mean,surv_low_share,tired_holds")
    for d in gamedirs:
        text = (d / "game.log").read_text(errors="replace")
        founded = conquered = lost = 0
        for m in CITY_RE.finditer(text):
            turn, _, event, _, _, born = m.groups()
            if event == "lost":
                lost += 1
            elif born == turn:
                founded += 1
            else:
                conquered += 1
        surv = [float(m.group(4)) for m in SURV_RE.finditer(text)]
        tired = len(TIRED_RE.findall(text))
        turns = final_turn(d / "scorelog.txt")
        cr = 100.0 * conquered / turns if turns else 0.0
        lr = 100.0 * lost / turns if turns else 0.0
        sm = sum(surv) / len(surv) if surv else float("nan")
        low = (sum(1 for p in surv if p < 0.5) / len(surv)
               if surv else float("nan"))
        print(f"{d.parent.name},{d.name.removeprefix('seed-')},{turns},"
              f"{founded},{conquered},{lost},{cr:.2f},{lr:.2f},"
              f"{sm:.3f},{low:.3f},{tired}")
        tot["founded"] += founded
        tot["conquered"] += conquered
        tot["lost"] += lost
        tot["turns"] += turns
        tot["surv_n"] += len(surv)
        tot["surv_sum"] += sum(surv)
        tot["surv_low"] += sum(1 for p in surv if p < 0.5)
        tot["tired"] += tired

    cr = 100.0 * tot["conquered"] / tot["turns"] if tot["turns"] else 0.0
    lr = 100.0 * tot["lost"] / tot["turns"] if tot["turns"] else 0.0
    sm = tot["surv_sum"] / tot["surv_n"] if tot["surv_n"] else float("nan")
    low = tot["surv_low"] / tot["surv_n"] if tot["surv_n"] else float("nan")
    print(f"TOTAL,,{tot['turns']},{tot['founded']},{tot['conquered']},"
          f"{tot['lost']},{cr:.2f},{lr:.2f},{sm:.3f},{low:.3f},"
          f"{tot['tired']}")


if __name__ == "__main__":
    main()
