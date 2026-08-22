#!/usr/bin/env python3
"""Run a block-family ablation and report PAIRED per-seed diffs.

Every arm is an all-deep game under one tuning file from
benchmark/ablate-blocks.py, so two arms on the same (config, seed) play
the same map and differ only in the knobs one family sets. The classic
baseline arm that tune.py subtracts is a per-(config, seed) constant
and cancels exactly in an arm-minus-arm diff, so it is not run here:
the question is which family loses to `defaults`, not what either
scores against ai/default.

Reports, per config: each arm's mean total score, the paired diff
against the reference arm with its standard error and t, and the sign
count (how many seeds the arm beat the reference on). Means alone hid
that a 2026-08 "win" rested on one runaway seed.

Also verifies, from each game's own game.log, that the tuning file the
server actually loaded is the arm's file -- the `deep-tuning file=...`
read-back line is the only ground truth for that, because a refused
file is logged once and then silently played as compiled defaults.

Usage:
    ablate-run.py --config CONF[,CONF] --arms DIR --out DIR
                  --server BIN [--count N] [--jobs J] [--portbase N]
                  [--seeds FILE] [--ref defaults] [--report-only]
"""
import argparse
import concurrent.futures
import os
import pathlib
import re
import statistics
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from tune import final_score                          # noqa: E402


def run_one(config, server, arm_ini, seed, outdir, port):
    outdir.mkdir(parents=True, exist_ok=True)
    if not (outdir / "DONE").exists():
        env = {**os.environ, "DEEP_QUIET": "1"}
        if arm_ini is not None:
            env["TUNING"] = str(arm_ini)
        cmd = [str(HERE / "run-game.sh"), "--config", config,
               "--seed", str(seed), "--arm", "deep", "--out", str(outdir),
               "--port", str(port), "--server", server]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(r.stderr.strip().splitlines()[-1:])
    return final_score(outdir / "scorelog.txt")


def loaded_file(outdir):
    """The tuning file the server says it loaded, or a failure token."""
    log = outdir / "game.log"
    if not log.exists():
        return "NO-LOG"
    seen = None
    for line in log.open(errors="replace"):
        if "cannot load tuning file" in line:
            return "LOADFAIL"
        m = re.search(r'deep-tuning file="([^"]+)"', line)
        if m:
            seen = m.group(1)
    return seen or "NO-READBACK"


def paired(a, b, seeds):
    """a-minus-b over the seeds both arms finished."""
    d = [a[s] - b[s] for s in seeds if s in a and s in b]
    if not d:
        return None
    mean = statistics.fmean(d)
    sem = (statistics.stdev(d) / len(d) ** 0.5) if len(d) > 1 else float("nan")
    wins = sum(1 for x in d if x > 0)
    return d, mean, sem, wins, len(d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", required=True)
    ap.add_argument("--arms", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--server", required=True)
    ap.add_argument("--seeds", default=str(HERE / "seeds/train.txt"))
    ap.add_argument("--count", type=int, default=8)
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--portbase", type=int, default=7300)
    ap.add_argument("--ports", type=int, default=50)
    ap.add_argument("--ref", default="defaults")
    ap.add_argument("--report-only", action="store_true")
    args = ap.parse_args()

    if "holdout" in pathlib.Path(args.seeds).name:
        sys.exit("holdout seeds are not for diagnosis; use train.txt")
    seeds = [int(x) for x in
             pathlib.Path(args.seeds).read_text().split()][:args.count]
    configs = args.config.split(",")
    arms = sorted(p for p in pathlib.Path(args.arms).glob("*.ini"))
    out = pathlib.Path(args.out)
    server = str(pathlib.Path(args.server).resolve())

    jobs = []
    for c in configs:
        name = pathlib.Path(c).stem
        for ini in arms:
            for s in seeds:
                jobs.append((name, c, ini, s,
                             out / name / ini.stem / f"seed-{s}"))
    scores = {}
    if not args.report_only:
        print(f"{len(jobs)} games: {len(configs)} configs x {len(arms)} "
              f"arms x {len(seeds)} seeds", flush=True)
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
            futs = {}
            for i, (name, c, ini, s, d) in enumerate(jobs):
                port = args.portbase + i % args.ports
                futs[ex.submit(run_one, c, server, ini, s, d, port)] \
                    = (name, ini.stem, s)
            done = 0
            for f in concurrent.futures.as_completed(futs):
                key = futs[f]
                done += 1
                try:
                    scores[key] = f.result()
                except Exception as exc:               # noqa: BLE001
                    print(f"  LOST {key}: {exc}", flush=True)
                print(f"  [{done}/{len(futs)}] {key}", flush=True)
    else:
        for name, c, ini, s, d in jobs:
            if (d / "DONE").exists():
                scores[(name, ini.stem, s)] = final_score(d / "scorelog.txt")

    print("\n== tuning read-back (game.log ground truth)")
    bad = 0
    for name, c, ini, s, d in jobs:
        got = loaded_file(d)
        if got != str(ini.resolve()):
            bad += 1
            print(f"  {name}/{ini.stem}/seed-{s}: {got}")
    print(f"  {len(jobs) - bad}/{len(jobs)} games loaded their own arm ini")

    for c in configs:
        name = pathlib.Path(c).stem
        print(f"\n== {name}: totals per seed (all-deep game, sum of seats)")
        armnames = [i.stem for i in arms]
        print("  seed    " + "".join(f"{a:>12}" for a in armnames))
        for s in seeds:
            print(f"  {s:<8}" + "".join(
                f"{scores.get((name, a, s), float('nan')):>12.0f}"
                for a in armnames))
        print(f"\n== {name}: paired vs {args.ref}")
        print("  %-10s %9s %9s %8s %7s %6s"
              % ("arm", "mean", "sem", "t", "wins", "n"))
        ref = {s: v for (n, a, s), v in scores.items()
               if n == name and a == args.ref}
        for a in armnames:
            cur = {s: v for (n, aa, s), v in scores.items()
                   if n == name and aa == a}
            r = paired(cur, ref, seeds)
            if r is None:
                print(f"  {a:<10} no paired data")
                continue
            _, mean, sem, wins, n = r
            t = mean / sem if sem and sem == sem and sem > 0 else float("nan")
            print("  %-10s %+9.1f %9.1f %+8.2f %4d/%-2d %6d"
                  % (a, mean, sem, t, wins, n, n))


if __name__ == "__main__":
    main()
