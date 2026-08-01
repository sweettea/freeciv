#!/usr/bin/env python3
"""Tune ai/deep's constants against training seeds (W25).

    tune.py [--config FILE[,FILE...]] [--seeds FILE] [--count N]
            [--gens G] [--pop P] [--params a,b,...] [--server BIN]
            [--jobs J] [--out DIR] [--portbase N] [--mixed]
            [--blocks S,E,M] [--rounds R]

--blocks runs alternating block coordinate descent: each named block
(defined in tuning-blocks.ini) is evolved in turn with every other
constant held at the incumbent value, and the winner carries forward
into the next block. With --rounds > 1 the cycle repeats.

The per-round deltas are the point, not just the tuning: if a block
still gains materially in round 2 -- after the other blocks moved --
the blocks are COUPLED and constants tuned in isolation do not
compose (see findings/objective-layer.md). If round 2 is flat, they
are near-separable and one pass per block was enough.

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


def parse_blocks(path):
    """{name: [param, ...]} from tuning-blocks.ini."""
    out = {}
    for line in open(path):
        line = line.split(";", 1)[0].strip()
        if not line or line.startswith(("[", "#")):
            continue
        name, _, rest = line.partition("=")
        out[name.strip()] = [x.strip() for x in rest.split(",") if x.strip()]
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


def run_block(args, seeds, allp, idx, cur, baseline, out, rng, tag):
    """Evolve only allp[idx]; every other constant stays at cur's value.

    Candidates are always written as the FULL parameter vector, so a
    block pass inherits whatever the previous blocks decided rather
    than silently reverting them to compiled defaults."""
    mean = [cur[i] for i in idx]
    sigma = [(allp[i]["hi"] - allp[i]["lo"]) / 6.0 for i in idx]
    best = (None, list(cur))

    for gen in range(args.gens):
        cands = []
        for c in range(args.pop):
            vals = list(cur)
            if not (gen == 0 and c == 0):
                for k, i in enumerate(idx):
                    p = allp[i]
                    vals[i] = min(p["hi"],
                                  max(p["lo"], rng.gauss(mean[k], sigma[k])))
            cands.append(vals)

        cand_dirs, tuning_files = [], []
        for c, vals in enumerate(cands):
            cdir = out / tag / f"gen{gen}" / f"cand{c}"
            cdir.mkdir(parents=True, exist_ok=True)
            tf = cdir / "tuning.ini"
            write_tuning(tf, allp, vals)
            cand_dirs.append(cdir)
            tuning_files.append(tf.resolve())
        objs = eval_population(args, seeds, cand_dirs, tuning_files,
                               baseline, args.portbase)
        scored = sorted(zip(objs, cands), key=lambda t: -t[0])
        for c, (obj, vals) in enumerate(zip(objs, cands)):
            print(f"[{tag}] gen {gen} cand {c}: obj {obj:+.1f}  "
                  + " ".join(f"{allp[i]['name']}={vals[i]:.3g}"
                             for i in idx), flush=True)

        if best[0] is None or scored[0][0] > best[0]:
            best = scored[0]
        else:
            sigma = [s * 0.7 for s in sigma]   # stagnation: narrow

        write_tuning(out / "best-tuning.ini", allp, best[1])
        mu = max(2, args.pop // 2)
        weights = [mu - i for i in range(mu)]
        wsum = sum(weights)
        mean = [sum(w * scored[i][1][j] for i, w in enumerate(weights))
                / wsum for j in idx]
        print(f"[{tag}] gen {gen} best {scored[0][0]:+.1f}; "
              f"block best {best[0]:+.1f}", flush=True)

    return best


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
    ap.add_argument("--evaluate", metavar="TUNINGFILE",
                    help="evaluate one tuning file on the chosen seeds"
                         " and configs without optimising -- how a"
                         " merge of two machines' block results is"
                         " checked against each machine's own best")
    ap.add_argument("--start", metavar="TUNINGFILE",
                    help="initial values for every constant (default:"
                         " compiled defaults). Constants outside"
                         " --blocks/--params keep these values, so this"
                         " is how a regime is pinned (e.g. military"
                         " zeroed while the economy is tuned) and how"
                         " two machines' results are merged.")
    ap.add_argument("--blocks",
                    help="comma list of block names from"
                         " tuning-blocks.ini, evolved in turn")
    ap.add_argument("--rounds", type=int, default=1,
                    help="alternation rounds over --blocks")
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

    if args.evaluate:
        obj = eval_candidate(args, seeds, out / "evaluate",
                             Path(args.evaluate).resolve(), baseline,
                             args.portbase)
        print(f"EVALUATE {args.evaluate}: mean paired diff {obj:+.1f}"
              f" over {len(seeds)} seeds, configs {args.configs}",
              flush=True)
        return

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
    cur = [p["default"] for p in allp]
    if args.start:
        start = {}
        for line in open(args.start):
            line = line.split(";", 1)[0].strip()
            if not line or line.startswith(("[", "#")):
                continue
            k, _, v = line.partition("=")
            try:
                start[k.strip()] = float(v)
            except ValueError:
                sys.exit(f"bad --start line: {line}")
        unknown = set(start) - {p["name"] for p in allp}
        if unknown:
            sys.exit(f"--start has unknown constants: {sorted(unknown)}")
        cur = [start.get(p["name"], p["default"]) for p in allp]
        print(f"start from {args.start}: {len(start)} constants pinned",
              flush=True)

    if args.blocks:
        blocks = parse_blocks(HERE / "tuning-blocks.ini")
        unknown = set(args.blocks.split(",")) - set(blocks)
        if unknown:
            sys.exit(f"unknown blocks: {sorted(unknown)}")
        plan = args.blocks.split(",")
    else:
        blocks = {"all": [p["name"] for p in params]}
        plan = ["all"]

    history = []
    for rnd in range(args.rounds):
        for bname in plan:
            names = set(blocks[bname])
            idx = [i for i, p in enumerate(allp) if p["name"] in names]
            if not idx:
                sys.exit(f"block {bname} matches no tunable parameter")
            tag = f"r{rnd}-{bname}"
            print(f"=== round {rnd} block {bname}: "
                  f"{len(idx)} params", flush=True)
            obj, vals = run_block(args, seeds, allp, idx, cur, baseline,
                                  out, rng, tag)
            gain = obj - history[-1][2] if history else None
            history.append((rnd, bname, obj))
            cur = vals
            print(f"=== round {rnd} block {bname} done: obj {obj:+.1f}"
                  + (f" (delta {gain:+.1f})" if gain is not None else ""),
                  flush=True)

    tf = out / "best-tuning.ini"
    write_tuning(tf, allp, cur)
    print("\nALTERNATION TABLE (objective after each block pass)")
    for rnd, bname, obj in history:
        print(f"  round {rnd}  block {bname:<6} obj {obj:+.1f}")
    if args.rounds > 1:
        first = {}
        for rnd, bname, obj in history:
            first.setdefault(bname, []).append(obj)
        print("\nCOUPLING READ: per-block gain in the last round vs the"
              " first")
        for bname, objs in first.items():
            if len(objs) > 1:
                print(f"  {bname:<6} first {objs[0]:+.1f} -> last "
                      f"{objs[-1]:+.1f}  (moved {objs[-1] - objs[0]:+.1f})")
        print("  A block that still moves late is coupled to the others;"
              " see findings/objective-layer.md")
    print(f"\nBEST tuning written to {tf}", flush=True)
    print("report on holdout with: tune.py --report-holdout "
          f"{tf} --config {args.config}", flush=True)


if __name__ == "__main__":
    main()
