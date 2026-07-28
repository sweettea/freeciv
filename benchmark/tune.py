#!/usr/bin/env python3
"""Tune ai/deep's constants against training seeds (W25).

    tune.py [--config FILE[,FILE...]] [--seeds FILE] [--count N]
            [--gens G] [--pop P] [--params a,b,...] [--server BIN]
            [--jobs J] [--out DIR] [--portbase N] [--mixed]

Multiple --config files give a blended objective: the mean over
configs of the mean paired score diff, so a candidate must not
overfit a single regime.

--mixed switches to head-to-head games: both arms sit in the SAME
game (mixed_db/mixed_bd seat mirroring, see run-game.sh), there is no
separate baseline arm, and the objective is the within-game deep-side
total minus baseline-side total, summed over the two mirrors of each
seed. This is the only objective that can see defects which never
fire in deep-vs-deep worlds (e.g. the naval defence hole, 2026-07-28).
    tune.py --report-holdout TUNINGFILE [--config FILE] [--count N] ...

Optimiser: a dependency-free diagonal (mu, lambda) evolution strategy
(rank-weighted mean of the better half, per-parameter sigma shrinking
on stagnation). Upgrade to full CMA-ES is mechanical if numpy/cma ever
become available; the evaluation plumbing is the hard part and is
optimiser-agnostic.

Objective: mean over seeds of (deep total score - baseline total score)
at game end on the chosen config. Baseline games are tuning-independent
and evaluated once. Every game runs through run-game.sh with a TUNING
file, so results are cached per (tuning content, config, seed) and an
interrupted run resumes.

Discipline: refuses to optimise against the holdout seed file. Report
final numbers with --report-holdout TUNINGFILE, which evaluates a fixed
tuning (no optimisation) on the holdout seeds.
"""

import argparse
import concurrent.futures
import hashlib
import random
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def parse_params(path):
    """[{name, default, lo, hi, is_int}] from tuning-params.ini."""
    out = []
    rx = re.compile(r"^(\w+)\s*=\s*([-\d.]+),\s*([-\d.]+),\s*([-\d.]+),"
                    r"\s*(float|int)\s*$")
    for line in open(path):
        line = line.strip()
        if not line or line.startswith((";", "[", "#")):
            continue
        m = rx.match(line)
        if not m:
            sys.exit(f"bad params line: {line}")
        out.append({"name": m.group(1), "default": float(m.group(2)),
                    "lo": float(m.group(3)), "hi": float(m.group(4)),
                    "is_int": m.group(5) == "int"})
    return out


def write_tuning(path, params, values):
    with open(path, "w") as f:
        f.write("[tuning]\n")
        for p, v in zip(params, values):
            if p["is_int"]:
                f.write(f"{p['name']} = {int(round(v))}\n")
            else:
                f.write(f"{p['name']} = {v:.6f}\n")


def final_score(scorelog):
    """Sum of every player's score at their last logged turn."""
    tags, data = {}, {}
    for line in open(scorelog, errors="replace"):
        parts = line.split(" ")
        if parts[0] == "tag":
            tags[parts[2].strip()] = int(parts[1])
        elif parts[0] == "data":
            t, tag, plr = int(parts[1]), int(parts[2]), int(parts[3])
            data.setdefault(plr, {}).setdefault(tag, {})[t] = parts[4]
    sid = tags["score"]
    total = 0
    for plr, tagdata in data.items():
        turns = tagdata.get(sid, {})
        if turns:
            total += int(turns[max(turns)])
    return total


def mixed_sides(scorelog, arm):
    """(deep_total, base_total) of one mixed game: seats P1, P3, ...
    run deep in mixed_db and baseline in mixed_bd (run-game.sh).
    Civil-war splinters (non-P names) belong to neither side."""
    tags, names, data = {}, {}, {}
    for line in open(scorelog, errors="replace"):
        parts = line.split(" ")
        if parts[0] == "tag":
            tags[parts[2].strip()] = int(parts[1])
        elif parts[0] == "addplayer":
            names[int(parts[2])] = " ".join(parts[3:]).strip()
        elif parts[0] == "data":
            t, tag, plr = int(parts[1]), int(parts[2]), int(parts[3])
            data.setdefault(plr, {}).setdefault(tag, {})[t] = parts[4]
    sid = tags["score"]
    deep = base = 0
    for plr, tagdata in data.items():
        name = names.get(plr, "")
        if not re.fullmatch(r"P\d+", name):
            continue
        turns = tagdata.get(sid, {})
        if not turns:
            continue
        score = int(turns[max(turns)])
        if (int(name[1:]) % 2 == 1) == (arm == "mixed_db"):
            deep += score
        else:
            base += score
    return deep, base


def run_game(config, args, arm, seed, outdir, port, tuning=None,
             mixed=False):
    outdir.mkdir(parents=True, exist_ok=True)
    if not (outdir / "DONE").exists():
        env = dict(TUNING=str(tuning)) if tuning else {}
        import os
        cmd = [str(HERE / "run-game.sh"), "--config", config,
               "--seed", str(seed), "--arm", arm, "--out", str(outdir),
               "--port", str(port)]
        if args.server:
            cmd += ["--server", args.server]
        r = subprocess.run(cmd, env={**os.environ, **env},
                           capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"game failed: {arm} seed {seed}: "
                               f"{r.stderr.strip().splitlines()[-1:]}" )
    if mixed:
        return mixed_sides(outdir / "scorelog.txt", arm)
    return final_score(outdir / "scorelog.txt")


def eval_population(args, seeds, cand_dirs, tuning_files, baseline,
                    portbase):
    """Evaluate every candidate's every game in ONE pool, so the whole
    population fills the machine instead of one candidate at a time.
    Returns a list of blended objectives, one per candidate."""
    mixed = getattr(args, "mixed", False)
    arms = ("mixed_db", "mixed_bd") if mixed else ("deep",)
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        futs = {}
        port = portbase
        for k, (cdir, tf) in enumerate(zip(cand_dirs, tuning_files)):
            for c, config in enumerate(args.configs):
                for seed in seeds:
                    for arm in arms:
                        d = cdir / f"cfg{c}" / (f"{arm}/" if mixed
                                                else "") / f"seed-{seed}"
                        futs[ex.submit(run_game, config, args, arm, seed,
                                       d, port, tf, mixed)] = (k, c, seed)
                        port += 1
        diffs = {}
        for f in concurrent.futures.as_completed(futs):
            k, c, seed = futs[f]
            if mixed:
                deep, base = f.result()
                per_seed = diffs.setdefault((k, c), {})
                per_seed[seed] = per_seed.get(seed, 0) + deep - base
            else:
                diffs.setdefault((k, c), {})[seed] \
                    = f.result() - baseline[(c, seed)]
    objs = []
    for k in range(len(cand_dirs)):
        per_cfg = [sum(diffs[(k, c)].values()) / len(diffs[(k, c)])
                   for c in range(len(args.configs))]
        objs.append(sum(per_cfg) / len(per_cfg))
    return objs


def eval_candidate(args, seeds, cand_dir, tuning_file, baseline, portbase):
    """Single-candidate wrapper (holdout reports)."""
    return eval_population(args, seeds, [cand_dir], [tuning_file],
                           baseline, portbase)[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default=str(HERE / "configs/smoke.conf"))
    ap.add_argument("--seeds", default=str(HERE / "seeds/train.txt"))
    ap.add_argument("--count", type=int, default=10)
    ap.add_argument("--gens", type=int, default=4)
    ap.add_argument("--pop", type=int, default=6)
    ap.add_argument("--params", help="comma list; default: all")
    ap.add_argument("--server")
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--out", default=str(HERE / "results/tuning/run1"))
    ap.add_argument("--portbase", type=int, default=6200)
    ap.add_argument("--seed-rng", type=int, default=42)
    ap.add_argument("--report-holdout", metavar="TUNINGFILE")
    ap.add_argument("--mixed", action="store_true",
                    help="head-to-head seat-mirrored games; objective is"
                         " within-game deep-side minus baseline-side")
    args = ap.parse_args()

    if args.report_holdout:
        args.seeds = str(HERE / "seeds/holdout.txt")
    elif "holdout" in Path(args.seeds).name:
        sys.exit("refusing to optimise against holdout seeds; "
                 "use --report-holdout TUNINGFILE for final numbers")

    seeds = [int(x) for x in
             Path(args.seeds).read_text().split()][: args.count]
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    allp = parse_params(HERE / "tuning-params.ini")
    if args.params:
        want = args.params.split(",")
        unknown = set(want) - {p["name"] for p in allp}
        if unknown:
            sys.exit(f"unknown params: {sorted(unknown)}")
        params = [p for p in allp if p["name"] in want]
    else:
        params = allp

    args.configs = args.config.split(",")

    # Baseline arm once per (config, seed) - tuning-independent. In
    # mixed mode the opponent lives inside every game, so there is no
    # separate baseline arm to run.
    print(f"configs={args.configs} seeds={seeds} mixed={args.mixed}"
          f" params={[p['name'] for p in params]}", flush=True)
    baseline = {}
    if not args.mixed:
        with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
            futs = {}
            for c, config in enumerate(args.configs):
                for i, s in enumerate(seeds):
                    futs[ex.submit(run_game, config, args, "baseline", s,
                                   out / "baseline" / f"cfg{c}"
                                   / f"seed-{s}",
                                   args.portbase + 100 + c * 50 + i)] \
                        = (c, s)
            for f in concurrent.futures.as_completed(futs):
                baseline[futs[f]] = f.result()
        print(f"baseline totals: {baseline}", flush=True)

    if args.report_holdout:
        cand = out / "holdout-report"
        obj = eval_candidate(args, seeds, cand,
                             Path(args.report_holdout).resolve(),
                             baseline, args.portbase)
        print(f"HOLDOUT report for {args.report_holdout}: "
              f"mean paired diff {obj:+.1f} over {len(seeds)} seeds",
              flush=True)
        return

    rng = random.Random(args.seed_rng)
    mean = [p["default"] for p in params]
    sigma = [(p["hi"] - p["lo"]) / 6.0 for p in params]
    best = (None, None)   # (objective, values)

    # Generation 0 includes the exact defaults as a control candidate.
    for gen in range(args.gens):
        cands = []
        for c in range(args.pop):
            if gen == 0 and c == 0:
                vals = list(mean)
            else:
                vals = [min(p["hi"], max(p["lo"],
                        rng.gauss(m, s)))
                        for p, m, s in zip(params, mean, sigma)]
            cands.append(vals)

        cand_dirs, tuning_files = [], []
        for c, vals in enumerate(cands):
            cdir = out / f"gen{gen}" / f"cand{c}"
            cdir.mkdir(parents=True, exist_ok=True)
            tf = cdir / "tuning.ini"
            write_tuning(tf, params, vals)
            cand_dirs.append(cdir)
            tuning_files.append(tf.resolve())
        objs = eval_population(args, seeds, cand_dirs, tuning_files,
                               baseline, args.portbase)
        scored = list(zip(objs, cands))
        for c, (obj, vals) in enumerate(scored):
            print(f"gen {gen} cand {c}: obj {obj:+.1f}  "
                  + " ".join(f"{p['name']}={v:.3g}"
                             for p, v in zip(params, vals)), flush=True)

        scored.sort(key=lambda t: -t[0])
        if best[0] is None or scored[0][0] > best[0]:
            best = scored[0]
        else:
            sigma = [s * 0.7 for s in sigma]   # stagnation: narrow search

        # Persist the incumbent after every generation, so a
        # time-capped or interrupted campaign still has a verdict
        # candidate to report on.
        write_tuning(out / "best-tuning.ini", params, best[1])

        mu = max(2, args.pop // 2)
        weights = [mu - i for i in range(mu)]
        wsum = sum(weights)
        mean = [sum(w * scored[i][1][k] for i, w in enumerate(weights))
                / wsum for k in range(len(params))]
        print(f"gen {gen} best {scored[0][0]:+.1f}; overall best "
              f"{best[0]:+.1f}", flush=True)

    tf = out / "best-tuning.ini"
    write_tuning(tf, params, best[1])
    print(f"BEST objective {best[0]:+.1f}; tuning written to {tf}",
          flush=True)
    print("report on holdout with: tune.py --report-holdout "
          f"{tf} --config {args.config}", flush=True)


if __name__ == "__main__":
    main()
