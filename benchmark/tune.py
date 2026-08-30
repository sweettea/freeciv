#!/usr/bin/env python3
"""Tune ai/deep's constants against training seeds (W25).

    tune.py [--config FILE[,FILE...]] [--seeds FILE] [--count N]
            [--gens G] [--pop P] [--params a,b,...] [--server BIN]
            [--jobs J] [--out DIR] [--portbase N] [--mixed]
            [--blocks S,E,M] [--rounds R]
            [--accept best|paired --accept-z Z]
            [--reeval K --reeval-seeds N]
            [--race K --race-z Z] [--sigma-frac F]
            [--dry-run [--sigma-seed SIGMA]]

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

Every objective is printed with its standard error (sigma across
seeds / sqrt(n), blended over configs), and every candidate with its
paired diff against the block's incumbent on the seeds both ran. An
8-seed objective on a venue whose per-seed sigma is 115 has SE ~41:
without that column a +48.9 reads as a gain, and the 2026-08-25
post-mortem is a whole campaign that selected exactly that.

Selection. By default the block best is the generation's highest
mean -- an UNREPLICATED max over the population, and every report of
it now says so. Two flags replace it with a replicated number, and
they compose:

  --accept paired --accept-z Z   a challenger becomes the block best
      only if its paired diff against the incumbent, on the seeds
      both ran, clears Z standard errors. Z has no default: the right
      threshold is a property of the campaign's budget.
  --reeval K --reeval-seeds N    each generation's top K candidates,
      and the incumbent, are re-evaluated on N seeds the search never
      saw (the ones after --count in the TRAIN file), and the block
      best is taken from THAT number. The incumbent is re-evaluated
      on the same fresh seeds, so the floor a champion must clear is
      measured on the same sample it is.

--race K --race-z Z spends the budget rather than raising it: every
candidate plays the first K seeds, the better half by mean - z*SE
plays the remaining ones, and a raced-out candidate can never be the
block best. Halving a pop-10 generation after 3 of 8 seeds is 25
games instead of 40 with more seeds behind the survivors.

--sigma-frac F sets the initial mutation spread to F * range per
constant. The default is 1/6, unchanged, and range/6 is a
first-generation setting rather than a standing one: from a hand-tuned
start point it puts most of a generation far below the incumbent, and
the stagnation decay is what walks it back down.

--dry-run prints the schedule -- block passes, candidates, seeds,
games, and with --sigma-seed the SE the objective will have -- and
launches nothing, so a campaign is budgeted against the effect it is
chasing before the first game rather than after the last.

Discipline: refuses to optimise against the holdout seed file. Report
final numbers with --report-holdout TUNINGFILE, which evaluates a fixed
tuning (no optimisation) on the holdout seeds.

    tune.py --arena MANIFEST [--config FILE] [--count N] ...

--arena drives the server's own per-seat tuning layer (PERPLAYER,
ai/deep/deeptuning.c): MANIFEST is a file in the format that loader
already parses -- an [arena] section whose keys are seat names (P1)
or slotN fallbacks, each naming one whole tuning ini -- and every
game runs all-deep with FREECIV_DEEP_ARENA pointing at it, so
different seats in the SAME game play different tunings. The report
is per seat: each seat's final score and in-game rank, keyed by the
file it played, confirmed against the server's own deep-arena log
line, plus the per-file mean score and mean rank over all games.
Results are cached like every other mode, under a key that hashes
the WHOLE manifest content and every candidate file it names --
editing any of them misses the cache. Arena fitness is population-
dependent; it triages candidates, it does not replace the paired
protocol (see benchmark/arena.py's header).
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


def mean_se(values):
    """(mean, standard error) of a sample. SE is nan below two points."""
    n = len(values)
    if n == 0:
        return float("nan"), float("nan")
    m = sum(values) / n
    if n < 2:
        return m, float("nan")
    var = sum((v - m) ** 2 for v in values) / (n - 1)
    return m, (var / n) ** 0.5


def blend_stats(per_seed):
    """(objective, SE, points) for one candidate's {(cfg, seed): diff}.

    The objective is what it always was: the mean over configs of that
    config's mean paired diff. The SE is the standard error of exactly
    that statistic -- each config mean has variance s_c^2/n_c and the
    blend divides their sum by the config count, so
    SE = sqrt(sum_c s_c^2/n_c) / C. With one config it is the plain
    sigma-across-seeds / sqrt(n) the post-mortem asked for, and it is
    the number that stops a campaign selecting noise: on a venue with
    per-seed sigma 115, an 8-seed objective reads +48.9 +/- 41.

    A candidate with no finished game at all still ranks last rather
    than being given an invented value."""
    by_cfg = {}
    for (c, _seed), v in per_seed.items():
        by_cfg.setdefault(c, []).append(v)
    if not by_cfg:
        return -float("inf"), float("nan"), 0
    means, var = [], 0.0
    for vals in by_cfg.values():
        m, se = mean_se(vals)
        means.append(m)
        var += se * se
    return (sum(means) / len(means), var ** 0.5 / len(means),
            len(per_seed))


def paired_stats(cand, inc):
    """(mean, SE, points) of cand minus incumbent on the seeds BOTH ran.

    The baseline term cancels, so this is a within-seed contrast and
    its SE is well below either objective's own -- which is the whole
    reason a challenger is judged against the incumbent rather than
    against zero."""
    return blend_stats({k: cand[k] - inc[k] for k in cand if k in inc})


def zscore(mean, se):
    """mean/SE. nan when the SE is unknown (fewer than two points).

    A zero SE means every seed moved the same way by the same amount,
    which is evidence, not a missing measurement: it scores +/-inf so
    a unanimous challenger clears any gate instead of being rejected
    by a division nobody thought about."""
    if se != se:
        return float("nan")
    if se <= 0:
        if mean == 0:
            return float("nan")
        return float("inf") if mean > 0 else -float("inf")
    return mean / se


def pm(mean, se):
    """'+12.3 +/- 41.2' -- an objective is never printed without one."""
    return f"{mean:+.1f} +/- {se:.1f}" if se == se else f"{mean:+.1f} +/- n/a"


def run_game(config, args, arm, seed, outdir, port, tuning=None,
             mixed=False, arena=None):
    outdir.mkdir(parents=True, exist_ok=True)
    if not (outdir / "DONE").exists():
        env = {}
        if tuning:
            env["TUNING"] = str(tuning)
        if arena:
            env["ARENA"] = str(arena)
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

    Returns one {(config index, seed): paired diff} map per candidate.
    Per-seed rather than pre-averaged, because the SE across seeds and
    the paired contrast against the incumbent both need the individual
    seeds; blend_stats() turns a map into the objective and its SE."""
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
        lost = []
        for f in concurrent.futures.as_completed(futs):
            k, c, seed = futs[f]
            # A game that dies takes its datum with it, not the campaign.
            # One crashed server out of 288 used to abort a 22-hour run
            # (EAST2, r0-ALL gen1). Drop the seed, say so loudly, and let
            # the remaining seeds carry the candidate.
            try:
                if mixed:
                    deep, base = f.result()
                    per_cand = diffs.setdefault(k, {})
                    per_cand[(c, seed)] = (per_cand.get((c, seed), 0)
                                           + deep - base)
                else:
                    diffs.setdefault(k, {})[(c, seed)] \
                        = f.result() - baseline[(c, seed)]
            except Exception as exc:                      # noqa: BLE001
                lost.append((k, c, seed))
                print(f"  LOST GAME cand={k} cfg={c} seed={seed}: {exc}",
                      flush=True)
    if lost:
        print(f"  {len(lost)} of {len(futs)} games lost this generation",
              flush=True)
    return [diffs.get(k, {}) for k in range(len(cand_dirs))]


def eval_candidate(args, seeds, cand_dir, tuning_file, baseline, portbase):
    """Single-candidate wrapper (incumbent and holdout reports)."""
    return eval_population(args, seeds, [cand_dir], [tuning_file],
                           baseline, portbase)[0]


SIGMA_FRAC_DEFAULT = 1.0 / 6.0


def initial_sigma(allp, idx, frac):
    """Per-parameter mutation sigma at the START of a block pass.

    frac * (hi - lo) per constant. The historical value, and still the
    default, is range/6 -- but that is a FIRST-generation setting, not
    a standing one. From a hand-tuned start point it puts most of the
    population far worse than the incumbent (the post-fix ECON block's
    generation 0: nine of ten candidates between -40 and -108), which
    is fine as an opening spread and wasteful as a steady state. The
    stagnation decay already in run_block narrows it from there; this
    flag sets where it starts."""
    return [(allp[i]["hi"] - allp[i]["lo"]) * frac for i in idx]


def race_survivors(stats, z, protect=()):
    """Indices that continue past a race rung: the better half.

    Ordering is by mean - z*SE, not by mean, so the remaining seeds
    are spent on candidates that are plausibly good rather than on
    ones that merely drew a lucky slice -- a wide SE pushes a
    candidate down its own ranking. protect names indices that
    survive regardless; generation 0's candidate 0 is the incumbent,
    whose full-seed games are already on disk.

    Ties and odd populations round UP, so a population of one keeps
    its one candidate and a population of five keeps three."""
    def key(k):
        m, se, _n = stats[k]
        return m - z * se if se == se else m

    n = len(stats)
    ranked = sorted(range(n), key=lambda k: -key(k))
    return sorted(set(ranked[: max(1, (n + 1) // 2)]) | set(protect))


def incumbent_dir(out, tag):
    """Where the block's unmutated incumbent is evaluated.

    It is generation 0's candidate 0 by construction -- run_block has
    always seeded the population with cur untouched -- so evaluating
    the incumbent here re-uses that directory's DONE markers instead
    of minting new ones. Every generation after the first therefore
    re-reads the incumbent for free, and campaigns run before this
    existed still resume without replaying a single game."""
    return out / tag / "gen0" / "cand0"


def run_block(args, seeds, allp, idx, cur, baseline, out, rng, tag):
    """Evolve only allp[idx]; every other constant stays at cur's value.

    Candidates are always written as the FULL parameter vector, so a
    block pass inherits whatever the previous blocks decided rather
    than silently reverting them to compiled defaults."""
    mean = [cur[i] for i in idx]
    sigma = initial_sigma(allp, idx, args.sigma_frac)
    best = (None, list(cur))
    best_se = float("nan")
    best_label = "incumbent held (no candidate accepted)"

    # The incumbent is fixed for the whole block, so one evaluation
    # serves every generation. Printing it each generation is what
    # makes a population drifting away from a still-unbeaten start
    # visible while the campaign runs rather than in the post-mortem
    # (tune-postfix, 2026-08-25: four stages of "block best" that the
    # incumbent had beaten all along).
    incdir = incumbent_dir(out, tag)
    incdir.mkdir(parents=True, exist_ok=True)
    inc_tf = incdir / "tuning.ini"
    write_tuning(inc_tf, allp, cur)
    inc_diffs = eval_candidate(args, seeds, incdir, inc_tf.resolve(),
                               baseline, args.portbase)
    inc_obj, inc_se, inc_n = blend_stats(inc_diffs)
    print(f"[{tag}] incumbent obj {pm(inc_obj, inc_se)} (n={inc_n})",
          flush=True)

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
        if args.race:
            # Successive halving: everyone plays the first slice, the
            # better half plays the rest. The budget a raced-out
            # candidate would have burned buys seeds for the ones
            # still alive, which is the only lever that raises n per
            # candidate without raising the campaign's cost.
            first, rest = seeds[: args.race], seeds[args.race:]
            diffs = eval_population(args, first, cand_dirs, tuning_files,
                                    baseline, args.portbase)
            keep = race_survivors([blend_stats(d) for d in diffs],
                                  args.race_z,
                                  protect=(0,) if gen == 0 else ())
            more = eval_population(args, rest, [cand_dirs[k] for k in keep],
                                   [tuning_files[k] for k in keep],
                                   baseline, args.portbase)
            for j, k in enumerate(keep):
                diffs[k] = {**diffs[k], **more[j]}
            print(f"[{tag}] gen {gen} race: {len(keep)} of {len(cands)}"
                  f" continue past {len(first)} seeds"
                  f" ({sorted(keep)})", flush=True)
        else:
            keep = list(range(len(cands)))
            diffs = eval_population(args, seeds, cand_dirs, tuning_files,
                                    baseline, args.portbase)
        stats = [blend_stats(d) for d in diffs]
        objs = [st[0] for st in stats]
        order = sorted(range(len(objs)), key=lambda k: -objs[k])
        scored = [(objs[k], cands[k]) for k in order]
        for c, ((obj, se, n), vals) in enumerate(zip(stats, cands)):
            # Every objective carries its SE, and every candidate
            # carries its paired contrast against the incumbent: a
            # "+104" whose SE is 41 and whose paired diff is +12 +/- 30
            # is a candidate nobody should promote.
            pd, pse, _pn = paired_stats(diffs[c], inc_diffs)
            z = zscore(pd, pse)
            print(f"[{tag}] gen {gen} cand {c}: obj {pm(obj, se)}"
                  f" (n={n})  vs inc {pm(pd, pse)}"
                  + (f" z={z:+.2f}" if z == z else "")
                  + ("" if c in keep else " RACED OUT")
                  + "  " + " ".join(f"{allp[i]['name']}={vals[i]:.3g}"
                                    for i in idx), flush=True)

        # --- who, if anyone, becomes the block best -------------
        # The 2026-08-25 post-mortem's first requirement: never select
        # by unreplicated max. The generation's leader is a CANDIDATE
        # for the block best, not the block best.
        # A raced-out candidate was measured on a slice; it is not
        # eligible to be the block best, whatever that slice said.
        elig = [k for k in order if k in keep] or order
        champ = elig[0]
        champ_obj, champ_se, _ = stats[champ]
        champ_diffs, champ_inc = diffs[champ], inc_diffs
        floor_obj = inc_obj
        label = f"unreplicated max over {len(cands)} candidates"

        if args.reeval:
            # Re-evaluate the generation's top K on seeds the search
            # never saw, together with the incumbent, so the champion
            # AND the floor it must clear are measured on one fresh
            # sample.
            topk = elig[: args.reeval]
            rdirs = [cand_dirs[k] for k in topk] + [incdir]
            rtfs = [tuning_files[k] for k in topk] + [inc_tf.resolve()]
            rdiffs = eval_population(args, args.reeval_pool, rdirs, rtfs,
                                     baseline, args.portbase)
            rstats = [blend_stats(d) for d in rdiffs]
            for j, k in enumerate(topk):
                ro, rse, rn = rstats[j]
                print(f"[{tag}] gen {gen} reeval cand {k}: "
                      f"obj {pm(ro, rse)} (n={rn})", flush=True)
            ri_obj, ri_se, ri_n = rstats[-1]
            print(f"[{tag}] gen {gen} reeval incumbent: "
                  f"obj {pm(ri_obj, ri_se)} (n={ri_n})", flush=True)
            j = max(range(len(topk)), key=lambda j: rstats[j][0])
            champ = topk[j]
            champ_obj, champ_se, _ = rstats[j]
            champ_diffs, champ_inc = rdiffs[j], rdiffs[-1]
            floor_obj = ri_obj
            label = (f"replicated on {len(args.reeval_pool)} fresh seeds"
                     f" (top {len(topk)} re-evaluated)")

        pd, pse, _pn = paired_stats(champ_diffs, champ_inc)
        z = zscore(pd, pse)
        # The block best may never sit below the incumbent measured on
        # the same seeds -- without --reeval that is automatic, since
        # generation 0 candidate 0 IS the incumbent, but a fresh-seed
        # champion needs it said.
        #
        # This is the ONE place the default path's behaviour differs
        # from before the W25 fixes: a generation whose leader is the
        # incumbent itself used to set the block best and skip the
        # sigma decay (best was None, so the first branch always
        # took), and now counts as the stagnation it is. The block
        # still returns the same vector with the same objective; only
        # the next generation's spread differs.
        better = champ_obj > floor_obj and (best[0] is None
                                            or champ_obj > best[0])
        if args.accept == "paired":
            take = better and z == z and z >= args.accept_z
            label += ("; paired z=%s vs --accept-z %.2f"
                      % (f"{z:+.2f}" if z == z else "n/a", args.accept_z))
        else:
            take = better
        print(f"[{tag}] gen {gen} champion cand {champ}: "
              f"obj {pm(champ_obj, champ_se)}, incumbent {floor_obj:+.1f},"
              f" paired {pm(pd, pse)} -- "
              f"{'ACCEPTED' if take else 'rejected'}; {label}",
              flush=True)

        if take:
            best = (champ_obj, cands[champ])
            best_se, best_label = champ_se, label
        else:
            sigma = [s * 0.7 for s in sigma]   # stagnation: narrow

        write_tuning(out / "best-tuning.ini", allp, best[1])
        mu = max(2, args.pop // 2)
        weights = [mu - i for i in range(mu)]
        wsum = sum(weights)
        mean = [sum(w * scored[i][1][j] for i, w in enumerate(weights))
                / wsum for j in idx]
        held = best[0] is None
        print(f"[{tag}] gen {gen} best {scored[0][0]:+.1f}; "
              + ("block best none accepted; " if held else
                 f"block best {pm(best[0], best_se)}; ")
              + f"incumbent {pm(inc_obj, inc_se)}", flush=True)

    if best[0] is None:
        # Nothing cleared the gate. The block's answer is its own
        # incumbent, reported with the incumbent's number rather than
        # with some candidate's lucky one.
        return {"obj": inc_obj, "se": inc_se, "vals": list(cur),
                "label": best_label}
    return {"obj": best[0], "se": best_se, "vals": best[1],
            "label": best_label}


def resolve_plan(args, params):
    """(block name -> constants, ordered block names) to be evolved."""
    if args.blocks:
        blocks = parse_blocks(HERE / "tuning-blocks.ini")
        unknown = set(args.blocks.split(",")) - set(blocks)
        if unknown:
            sys.exit(f"unknown blocks: {sorted(unknown)}")
        return blocks, args.blocks.split(",")
    return {"all": [p["name"] for p in params]}, ["all"]


def schedule(args, seeds, allp, blocks, plan):
    """What a campaign would cost, without launching any of it.

    Game counts are UPPER bounds: a candidate vector that repeats
    across blocks or rounds hits the result cache and costs nothing,
    and so does any game a previous run already finished. The
    incumbent is free by construction -- it is generation 0's
    candidate 0 -- so it appears in no count here.

    Expected SEs come from --sigma-seed, a per-seed sigma the campaign
    supplies from its own past data (e.g. 115 on contested); they say
    what the objective's error bar will be BEFORE a game is played,
    which is the number that decides whether the budget can see the
    effect being chased."""
    cfgs = len(args.configs)
    arms = 2 if args.mixed else 1
    n = len(seeds)
    rung = args.race if args.race else n
    surv = max(1, (args.pop + 1) // 2) if args.race else args.pop
    per_gen = (args.pop * cfgs * rung * arms
               + (surv * cfgs * (n - rung) * arms if args.race else 0))
    flat_per_gen = args.pop * cfgs * n * arms
    fresh = len(args.reeval_pool)
    reeval_gen = (args.reeval or 0) * cfgs * fresh * arms
    # The incumbent's fresh-seed games are one directory per block, so
    # they are paid once per block rather than once per generation.
    per_block = args.gens * (per_gen + reeval_gen) + (
        cfgs * fresh * arms if args.reeval else 0)

    rows = []
    for rnd in range(args.rounds):
        for bname in plan:
            names = set(blocks[bname])
            rows.append({
                "round": rnd, "block": bname,
                "params": len([p for p in allp if p["name"] in names]),
                "candidates": args.gens * args.pop,
                "games": per_block,
            })
    baseline = 0 if args.mixed else cfgs * (n + fresh)
    return {
        "rows": rows, "arms": arms, "configs": cfgs, "seeds": n,
        "fresh": fresh, "rung": rung, "survivors": surv,
        "per_gen": per_gen, "flat_per_gen": flat_per_gen,
        "per_block": per_block, "baseline": baseline,
        "total": baseline + sum(r["games"] for r in rows),
        "se": expected_se(args.sigma_seed, cfgs, n),
        "se_rung": expected_se(args.sigma_seed, cfgs, rung),
        "se_fresh": expected_se(args.sigma_seed, cfgs, fresh),
    }


def expected_se(sigma_seed, cfgs, n):
    """SE of the blended objective for a per-seed sigma, before any
    game is run: sigma / sqrt(configs * seeds). None when no sigma
    was supplied -- this tool does not invent one."""
    if sigma_seed is None or n <= 0 or cfgs <= 0:
        return None
    return sigma_seed / (cfgs * n) ** 0.5


def print_schedule(sched, args, seeds):
    print("DRY RUN -- no game is launched\n")
    print(f"configs ({sched['configs']}): {', '.join(args.configs)}")
    print(f"seeds ({sched['seeds']}): "
          + " ".join(str(s) for s in seeds))
    if args.reeval:
        print(f"fresh seeds for --reeval {args.reeval} "
              f"({sched['fresh']}): "
              + " ".join(str(s) for s in args.reeval_pool))
    if args.mixed:
        print("mixed: 2 seat-mirrored games per candidate-seed,"
              " no separate baseline arm")
    print(f"\npopulation {args.pop} x {args.gens} generations"
          f" x {args.rounds} round(s) over {len(sched['rows'])}"
          f" block pass(es)")
    if args.race:
        print(f"race: all {args.pop} play the first {sched['rung']}"
              f" seeds, {sched['survivors']} survivors play the"
              f" remaining {sched['seeds'] - sched['rung']}")
    print(f"games per generation: {sched['per_gen']}"
          + (f" (flat schedule: {sched['flat_per_gen']})"
             if sched['per_gen'] != sched['flat_per_gen'] else ""))
    print(f"games per block pass: {sched['per_block']}")

    print("\n  %-5s %-8s %7s %11s %8s"
          % ("round", "block", "params", "candidates", "games"))
    for r in sched["rows"]:
        print("  %-5d %-8s %7d %11d %8d"
              % (r["round"], r["block"], r["params"], r["candidates"],
                 r["games"]))
    if sched["baseline"]:
        print(f"\nbaseline arm (once, tuning-independent):"
              f" {sched['baseline']} games")
    print(f"TOTAL <= {sched['total']} games"
          " (cache hits make it fewer, never more)")

    if sched["se"] is None:
        print("\nexpected SE: pass --sigma-seed SIGMA (the per-seed"
              " sigma of the paired diff on this venue) to size the"
              " budget against the effect being chased")
    else:
        print(f"\nexpected SE at sigma {args.sigma_seed:g} per seed:")
        print(f"  objective  +/- {sched['se']:.1f}"
              f"  ({sched['seeds']} seeds x {sched['configs']} configs)")
        if args.race:
            print(f"  race rung  +/- {sched['se_rung']:.1f}"
                  f"  ({sched['rung']} seeds x {sched['configs']})")
        if args.reeval:
            print(f"  reeval     +/- {sched['se_fresh']:.1f}"
                  f"  ({sched['fresh']} seeds x {sched['configs']})")
        print("  the paired diff vs the incumbent is tighter than"
              " this: pairing cancels the seed, and its sigma is not"
              " this sigma")
    for r in sched["rows"]:
        if sched["se"] is not None and r["params"] > sched["seeds"]:
            print(f"\nNOTE: block {r['block']} moves {r['params']}"
                  f" constants on {sched['seeds']} seeds per"
                  f" candidate. ~40 noisy scalars cannot set ~80"
                  f" params (findings/tune-postfix-verdict-2026-08-25).")
            break


def parse_arena_manifest(path):
    """[(key, ini path)] from a manifest in the server's OWN format
    (ai/deep/deeptuning.c): an [arena] section whose keys are seat
    names (P1) or slotN fallbacks, each = one tuning ini. This parses
    the subset secfile_load() accepts for that shape; it deliberately
    defines no second format."""
    entries, section = [], None
    for raw in Path(path).read_text().split("\n"):
        line = raw.split(";", 1)[0].strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("["):
            section = line.strip("[] \t")
            continue
        if section != "arena":
            continue
        key, eq, val = line.partition("=")
        if not eq:
            sys.exit(f"{path}: not a key = value line: {raw!r}")
        entries.append((key.strip(), val.strip().strip('"')))
    if not entries:
        sys.exit(f"{path}: no [arena] entries; the server would keep"
                 " every seat on the base tuning")
    return entries


def arena_key(manifest, files):
    """Content key for the result cache: the manifest's bytes and every
    named candidate file's bytes. Editing ANY of them must miss."""
    h = hashlib.sha256()
    h.update(Path(manifest).read_bytes())
    for p in files:
        h.update(b"\0" + str(p).encode() + b"\0")
        h.update(Path(p).read_bytes())
    return h.hexdigest()[:12]


def arena_log_loads(gamedir):
    """(seats, loaded, failed) read from the game's own log.

    seats:  {seat name: (slot, file)} from the deep-arena seat lines --
            which file the server ASSIGNED each seat.
    loaded: {file} from the deep-tuning lines -- files whose values
            were actually parsed and applied.
    failed: {file} the loader refused ("cannot load tuning file"; the
            seat then plays the compiled defaults).

    Both sides are needed: the seat line is written after assignment
    whether or not the file parsed, so on a load failure it names a
    file whose values are NOT in play (seen live with a duplicate-key
    ini, 2026-08-14)."""
    rx_seat = re.compile(r'deep-arena seat=(\S+) slot=(\d+) file="([^"]+)"')
    rx_load = re.compile(r'deep-tuning file="([^"]+)"')
    rx_fail = re.compile(r'cannot load (?:arena manifest|tuning file)'
                         r' "([^"]+)"')
    seats, loaded, failed = {}, set(), set()
    for line in open(Path(gamedir) / "game.log", errors="replace"):
        m = rx_seat.search(line)
        if m:
            seats[m.group(1)] = (int(m.group(2)), m.group(3))
            continue
        m = rx_load.search(line)
        if m:
            loaded.add(m.group(1))
            continue
        m = rx_fail.search(line)
        if m:
            failed.add(m.group(1))
    return seats, loaded, failed


def run_arena(args, seeds, out):
    from arena import seat_scores, validate_tuning

    manifest = Path(args.arena).resolve()
    entries = parse_arena_manifest(manifest)
    files = sorted({p for _, p in entries})
    for p in files:
        # The server loads candidate paths verbatim from ITS cwd (the
        # game dir), so a relative path would silently run the compiled
        # defaults and still produce a plausible game.
        if not Path(p).is_absolute():
            sys.exit(f"manifest names a relative candidate path: {p}")
        if not Path(p).is_file():
            sys.exit(f"manifest names a missing candidate file: {p}")
        validate_tuning(p)
    expect = dict(entries)
    key = arena_key(manifest, files)
    base = out / f"arena-{key}"
    print(f"arena {manifest} key {key}: {len(entries)} seat entries,"
          f" {len(files)} candidate files", flush=True)

    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        futs, port = {}, args.portbase
        for c, config in enumerate(args.configs):
            for s in seeds:
                d = base / f"cfg{c}" / f"seed-{s}"
                futs[ex.submit(run_game, config, args, "deep", s, d,
                               port, None, False, manifest)] = (c, s, d)
                port += 1
        done = []
        for f in concurrent.futures.as_completed(futs):
            c, s, d = futs[f]
            try:
                f.result()
                done.append((c, s, d))
            except Exception as exc:                      # noqa: BLE001
                print(f"  LOST GAME cfg={c} seed={s}: {exc}", flush=True)

    per_file = {}
    for c, s, d in sorted(done):
        rows = seat_scores(d)
        seats, loads, fails = arena_log_loads(d)
        rank_of = {n: i + 1 for i, (n, _)
                   in enumerate(sorted(rows, key=lambda r: -r[1]))}
        print(f"== arena cfg{c} seed {s}  ({d})")
        print("   %-6s %8s %5s  %-9s %s"
              % ("seat", "score", "rank", "confirmed", "file"))
        for name, score in sorted(rows):
            got = seats.get(name)
            want = expect.get(name)
            if want is None and got is not None:
                want = expect.get("slot%d" % got[0])
            if got is None and want is None:
                confirm, path = "base", "-"
            elif got is not None and want == got[1]:
                # Assigned as the manifest says -- but assignment is
                # not application: the seat line is written even when
                # the file itself was refused, and then the seat plays
                # the compiled defaults.
                if got[1] in fails or got[1] not in loads:
                    confirm, path = "LOADFAIL", got[1]
                else:
                    confirm, path = "yes", got[1]
            else:
                # The manifest and the game disagree about this seat;
                # that is the failure this column exists to catch.
                confirm = "MISMATCH"
                path = "manifest=%s log=%s" % (want, got and got[1])
            print("   %-6s %8d %5d  %-9s %s"
                  % (name, score, rank_of[name], confirm, path))
            per_file.setdefault(path if confirm == "yes" else confirm,
                                []).append((score, rank_of[name]))
    if done:
        print(f"\n== per-file over {len(done)} game(s)")
        print("   %-6s %10s %9s  %s" % ("seats", "mean score",
                                        "mean rank", "file"))
        for path, sr in sorted(per_file.items()):
            print("   %-6d %10.1f %9.2f  %s"
                  % (len(sr), sum(s for s, _ in sr) / len(sr),
                     sum(r for _, r in sr) / len(sr), path))
    else:
        sys.exit("no arena game finished")


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
    ap.add_argument("--accept", choices=("best", "paired"),
                    default="best",
                    help="how a challenger becomes the block best."
                         " best (default, legacy): the generation's"
                         " highest mean wins -- an UNREPLICATED max,"
                         " which is what selected noise in the"
                         " 2026-08-25 campaign. paired: it wins only"
                         " if its paired diff against the incumbent on"
                         " the shared seeds clears --accept-z.")
    ap.add_argument("--accept-z", type=float,
                    help="required with --accept paired: how many"
                         " standard errors the challenger's paired"
                         " diff vs the incumbent must clear. No"
                         " default -- the right value is a property of"
                         " the campaign's budget, not of the tool.")
    ap.add_argument("--reeval", type=int, metavar="K",
                    help="re-evaluate each generation's top K"
                         " candidates, and the incumbent, on"
                         " --reeval-seeds seeds the search never saw,"
                         " and take the block best from THAT number."
                         " This is what makes 'block best' a"
                         " replicated quantity.")
    ap.add_argument("--reeval-seeds", type=int, metavar="N",
                    help="required with --reeval: how many fresh"
                         " training seeds (the ones after --count in"
                         " the seed file) the re-evaluation uses.")
    ap.add_argument("--dry-run", action="store_true",
                    help="print the campaign's schedule -- block"
                         " passes, candidates, seeds, games and the"
                         " expected SE -- and launch nothing, so a"
                         " campaign can be budgeted before it starts")
    ap.add_argument("--sigma-seed", type=float, metavar="SIGMA",
                    help="per-seed sigma of the paired diff on this"
                         " venue, taken from past data, used by"
                         " --dry-run to predict the objective's SE."
                         " No default: it is a measurement, not a"
                         " setting.")
    ap.add_argument("--sigma-frac", type=float,
                    default=SIGMA_FRAC_DEFAULT,
                    help="initial mutation sigma as a fraction of each"
                         " constant's range (default 1/6, the value"
                         " the tuner has always used, so leaving it"
                         " alone changes nothing). range/6 is a"
                         " FIRST-GENERATION setting: from a hand-tuned"
                         " start it makes most of the population far"
                         " worse than the incumbent. The stagnation"
                         " decay (x0.7 after a generation that accepts"
                         " nothing) narrows it from there.")
    ap.add_argument("--race", type=int, metavar="K",
                    help="successive halving: evaluate every candidate"
                         " on the FIRST K seeds, drop the worse half"
                         " by mean - z*SE (z from --race-z), and play"
                         " the remaining seeds only for the"
                         " survivors. Concentrates the budget on live"
                         " candidates; a raced-out candidate can never"
                         " be the block best.")
    ap.add_argument("--race-z", type=float,
                    help="required with --race: standard errors"
                         " subtracted from a candidate's mean before"
                         " the rung is ordered. Larger is more"
                         " conservative about dropping a wide"
                         " estimate. No default.")
    ap.add_argument("--arena", metavar="MANIFEST",
                    help="run all-deep games under this per-seat tuning"
                         " manifest (the server's FREECIV_DEEP_ARENA"
                         " format) and report per-seat objectives")
    args = ap.parse_args()

    if args.report_holdout:
        args.seeds = str(HERE / "seeds/holdout.txt")
    elif "holdout" in Path(args.seeds).name:
        sys.exit("refusing to optimise against holdout seeds; "
                 "use --report-holdout TUNINGFILE for final numbers")

    if args.accept == "paired" and args.accept_z is None:
        sys.exit("--accept paired requires --accept-z: the acceptance"
                 " threshold is a campaign decision, and this tool"
                 " will not pick one for you")
    if args.race is not None and args.race_z is None:
        sys.exit("--race requires --race-z: how many standard errors"
                 " to charge a candidate for its own uncertainty is a"
                 " campaign decision, and this tool will not pick one"
                 " for you")
    if args.reeval and not args.reeval_seeds:
        sys.exit("--reeval K requires --reeval-seeds N")
    if args.reeval and (args.report_holdout or args.evaluate):
        # Replicated SELECTION only means anything while something is
        # being selected. In a report the flag would silently widen
        # the seed set the report claims to be over -- and under
        # --report-holdout those extra seeds would be holdout seeds.
        sys.exit("--reeval selects between candidates; a report"
                 " evaluates one file. Drop it, or raise --count.")

    all_seeds = [int(x) for x in Path(args.seeds).read_text().split()]
    seeds = all_seeds[: args.count]
    # Fresh seeds for replicated selection come from the same TRAIN
    # file, after the ones the search itself uses: fresh with respect
    # to the optimiser, still never the holdout.
    args.reeval_pool = []
    if args.reeval:
        args.reeval_pool = all_seeds[args.count:
                                     args.count + args.reeval_seeds]
        if len(args.reeval_pool) < args.reeval_seeds:
            sys.exit(f"--reeval-seeds {args.reeval_seeds} needs"
                     f" {args.count + args.reeval_seeds} seeds in"
                     f" {args.seeds}, which holds {len(all_seeds)}")
    if args.race is not None and not 1 <= args.race < len(seeds):
        sys.exit(f"--race {args.race} must leave seeds for the second"
                 f" rung: 1 <= K < {len(seeds)}")
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

    if args.dry_run:
        if args.arena:
            sys.exit("--dry-run describes an optimisation campaign;"
                     " an --arena run has no schedule to predict")
        b, pl = resolve_plan(args, params)
        print_schedule(schedule(args, seeds, allp, b, pl), args, seeds)
        return

    # Arena games carry their opponents inside themselves: no baseline
    # arm, no optimiser, and the report is per seat.
    if args.arena:
        run_arena(args, seeds, out)
        return

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
                for i, s in enumerate(seeds + args.reeval_pool):
                    futs[ex.submit(run_game, config, args, "baseline", s,
                                   out / "baseline" / f"cfg{c}"
                                   / f"seed-{s}",
                                   args.portbase + 100 + c * 50 + i)] \
                        = (c, s)
            for f in concurrent.futures.as_completed(futs):
                baseline[futs[f]] = f.result()
        print(f"baseline totals: {baseline}", flush=True)

    if args.evaluate:
        obj, se, n = blend_stats(
            eval_candidate(args, seeds, out / "evaluate",
                           Path(args.evaluate).resolve(), baseline,
                           args.portbase))
        print(f"EVALUATE {args.evaluate}: mean paired diff {pm(obj, se)}"
              f" over {len(seeds)} seeds (n={n}),"
              f" configs {args.configs}", flush=True)
        return

    if args.report_holdout:
        cand = out / "holdout-report"
        obj, se, n = blend_stats(
            eval_candidate(args, seeds, cand,
                           Path(args.report_holdout).resolve(),
                           baseline, args.portbase))
        print(f"HOLDOUT report for {args.report_holdout}: "
              f"mean paired diff {pm(obj, se)} over {len(seeds)} seeds"
              f" (n={n})", flush=True)
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

    blocks, plan = resolve_plan(args, params)

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
            res = run_block(args, seeds, allp, idx, cur, baseline,
                            out, rng, tag)
            gain = res["obj"] - history[-1][2] if history else None
            history.append((rnd, bname, res["obj"], res["se"],
                            res["label"]))
            cur = res["vals"]
            print(f"=== round {rnd} block {bname} done: obj"
                  f" {pm(res['obj'], res['se'])}"
                  + (f" (delta {gain:+.1f})" if gain is not None else "")
                  + f" [{res['label']}]", flush=True)

    tf = out / "best-tuning.ini"
    write_tuning(tf, allp, cur)
    print("\nALTERNATION TABLE (objective after each block pass)")
    for rnd, bname, obj, se, label in history:
        print(f"  round {rnd}  block {bname:<6} obj {pm(obj, se)}"
              f"  [{label}]")
    if args.rounds > 1:
        first = {}
        for rnd, bname, obj, _se, _label in history:
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
