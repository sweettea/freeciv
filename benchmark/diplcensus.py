#!/usr/bin/env python3
"""Census of where diplomacy ENDED UP, and of deep's peace machinery.

    diplcensus.py RESULTS_DIR [RESULTS_DIR...] [--arm deep]

Two halves, and they answer different questions.

1. THE FINAL-SAVE CENSUS. Freeciv saves, per ordered player pair, both
   the diplstate a pair is in at the end (`current`) and the closest
   they ever got (`closest`, the server's `max_state`). That second
   column is the whole reason this census can be taken from a final
   save at all: "peaked at cease-fire" is not a guess reconstructed
   from turn samples, it is a field.

   The target population is pairs whose closest state is Cease-fire --
   they stopped shooting, reached the one rung that EXPIRES (16 turns,
   diplhand.c:57), and never climbed off it. `findings/deep-dive-
   2026-08-05/diplomacy-gamestate.md` measured that at 20-42% of met
   pairs across the cells.

   Pairs are counted ONCE (unordered, a<b) and read from a's row; the
   diplstate table is symmetric for `current`/`closest`. `embassy` is
   NOT symmetric and is reported per ordered direction.

2. THE DEEP SEAM LINES, when the binary emits them:

   deep-peaceseek   one pre-pass decision inside the cease-fire window,
                    with the love it saw, the bar it needed and whether
                    a proposal went out.
   deep-peacetake   one peace clause deep's own treaty_evaluate ruled
                    on, and which channel ruled.
   deep-embassy     one embassy clause deep priced itself, give side
                    and take side, with the value each got.
   deep-leaders     one per player-turn: who deep thinks leads on tech
                    and production, and whether it found a WIN_CAPITAL
                    strategy underneath.

Read the counters BEFORE the shares. A lever that never executed and a
lever that executed and did nothing look identical in the save census.
"""

import csv
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

PLR = re.compile(r"^\[player(\d+)\]$", re.M)
NUM = re.compile(r"(\w+)=(-?[\d.]+)")


def read_save(path):
    if str(path).endswith(".zst"):
        return subprocess.run(["zstdcat", str(path)], capture_output=True,
                              text=True, check=True).stdout
    return Path(path).read_text(encoding="utf-8", errors="replace")


def diplstates(text):
    """{(a, b): (current, closest, embassy_a_has, vision_a_gives)}, a != b."""
    out = {}
    parts = re.split(PLR, text)
    for i in range(1, len(parts) - 1, 2):
        plr = int(parts[i])
        body = re.split(r"^\[", parts[i + 1], maxsplit=1, flags=re.M)[0]
        m = re.search(r"^diplstate=\{(.*?)^\}", body, re.M | re.S)
        if not m:
            continue
        lines = [x for x in m.group(1).splitlines() if x]
        for other, row in enumerate(lines[1:]):
            cols = next(csv.reader([row]))
            if len(cols) < 8:
                continue
            out[(plr, other)] = (cols[0], cols[1], cols[6] == "TRUE",
                                 cols[7] == "TRUE")
    return out


def finals(d, arm):
    return sorted(d.glob("%s/seed-*/saves/*-final.sav*" % arm))


def logs(d, arm):
    return sorted(d.glob("%s/seed-*/game.log" % arm))


def parse(line):
    d = {k: float(v) for k, v in NUM.findall(line)}
    d.update({k: v for k, v in re.findall(r'(\w+)="([^"]*)"', line)})
    return d


def save_census(d, arm):
    fs = finals(d, arm)
    if not fs:
        print("  no final saves under %s/%s" % (d.name, arm))
        return
    met = 0
    cur = Counter()
    peak = Counter()
    emb_dir = 0
    emb_tot = 0
    vis_dir = 0
    for f in fs:
        ds = diplstates(read_save(f))
        seen = set()
        for (a, b), (c, k, embassy, vision) in ds.items():
            if a == b:
                continue
            emb_tot += 1
            if embassy:
                emb_dir += 1
            if vision:
                vis_dir += 1
            if (b, a) in seen:
                continue
            seen.add((a, b))
            if c == "Never met" and k == "Never met":
                continue
            met += 1
            cur[c] += 1
            peak[k] += 1
    if not met:
        print("  no met pairs")
        return
    print("  games %d   met pairs %d   (%.1f/game)"
          % (len(fs), met, met / len(fs)))
    print("  ENDED AT      " + "  ".join(
        "%s %5.1f%%" % (s, 100.0 * cur[s] / met)
        for s in ("War", "Cease-fire", "Armistice", "Peace", "Alliance")))
    print("  EVER REACHED  " + "  ".join(
        "%s %5.1f%%" % (s, 100.0 * peak[s] / met)
        for s in ("War", "Cease-fire", "Armistice", "Peace", "Alliance")))
    print("  PEAKED AT CEASE-FIRE  %5.1f%%   <- the trap population"
          % (100.0 * peak["Cease-fire"] / met))
    print("  peace-or-better ever  %5.1f%%   ended peace-or-better %5.1f%%"
          % (100.0 * (peak["Peace"] + peak["Alliance"] + peak["Armistice"])
             / met,
             100.0 * (cur["Peace"] + cur["Alliance"] + cur["Armistice"])
             / met))
    print("  embassy (ordered) %5.1f%%   shared vision (ordered) %5.1f%%"
          % (100.0 * emb_dir / max(emb_tot, 1),
             100.0 * vis_dir / max(emb_tot, 1)))


def log_census(d, arm):
    gl = logs(d, arm)
    ng = max(len(gl), 1)
    seek = []
    take = []
    emb = []
    lead = []
    for g in gl:
        for line in g.read_text(errors="ignore").split("\n"):
            if "deep-peaceseek " in line:
                seek.append(parse(line))
            elif "deep-peacetake " in line:
                take.append(parse(line))
            elif "deep-embassy " in line:
                emb.append(parse(line))
            elif "deep-leaders " in line:
                lead.append(parse(line))
    if not (seek or take or emb or lead):
        print("  no deep diplomacy seam lines (lever off, or older binary)")
        return
    if seek:
        sent = [r for r in seek if r.get("sent", 0) > 0]
        print("  peace pre-pass: windows seen %7.1f/game   PROPOSED %7.1f"
              % (len(seek) / ng, len(sent) / ng))
        if sent:
            print("     love at proposal p50 %6.1f   bar %6.1f"
                  % (sorted(r.get("love", 0) for r in sent)[len(sent) // 2],
                     sorted(r.get("bar", 0) for r in sent)[len(sent) // 2]))
        by = Counter(r.get("why", "?") for r in seek)
        for k, v in by.most_common():
            print("     %-16s %8.1f/game" % (k, v / ng))
    if take:
        ok = [r for r in take if r.get("accept", 0) > 0]
        print("  peace clauses judged by deep %7.1f/game   ACCEPTED %7.1f"
              % (len(take) / ng, len(ok) / ng))
    if emb:
        cleared = [r for r in emb if r.get("accept", 0) > 0]
        print("  embassy clauses priced %7.1f/game   swaps CLEARED %7.1f"
              % (len(emb) / ng, len(cleared) / ng))
        gv = [r["worth"] for r in emb if r.get("give", 0) > 0]
        tk = [r["worth"] for r in emb if r.get("give", 0) <= 0]
        if gv:
            print("     give-side worth mean %8.1f   n=%d"
                  % (sum(gv) / len(gv), len(gv)))
        if tk:
            print("     take-side worth mean %8.1f   n=%d"
                  % (sum(tk) / len(tk), len(tk)))
    if lead:
        biased = [r for r in lead if r.get("bias", 0) > 0]
        print("  leader reads %8.1f/game   with a live leader %8.1f"
              % (len(lead) / ng, len(biased) / ng))
        wc = [r for r in lead if r.get("wincap", 0) > 0]
        print("     WIN_CAPITAL player-turns %8.1f/game" % (len(wc) / ng))
        tl = Counter(r.get("tech", "?") for r in lead)
        print("     distinct tech leaders named %d" % len(tl))


def main():
    args = [a for a in sys.argv[1:]]
    arm = "deep"
    if "--arm" in args:
        i = args.index("--arm")
        arm = args[i + 1]
        del args[i:i + 2]
    if not args:
        raise SystemExit(__doc__)
    for a in args:
        d = Path(a)
        print("=" * 74)
        print("%s   arm=%s" % (d.name, arm))
        print("=" * 74)
        save_census(d, arm)
        log_census(d, arm)
        print()


if __name__ == "__main__":
    main()
