# Benchmark harness

**Built in Phase 0 (`doc/deep-ai/workstreams/W00-harness.md`). Human-verified
before Phase 1 begins.**

```
run.sh              --quick (10 paired seeds) | --full (100, resumable)
tune.py             CMA-ES over struct tuning. TRAIN seeds only.
report.py           table, confidence intervals, effect sizes
configs/            the measurement matrix
seeds/train.txt     tuning only
seeds/holdout.txt   reporting only -- must never appear in tune.py
results/            content-addressed: (tuning_hash, config, seed)
corpus/             saved positions for L1 decision assertions
```

## Non-negotiables

1. **Pair every comparison on identical seeds.** ~110 games per arm paired vs
   ~550 unpaired for the same statistical power.
2. **Tune on train, report on holdout.** Never both.
3. **Primary metric is score ratio in separate games**, not head-to-head —
   head-to-head is gameable by exploiting one opponent's specific weaknesses.
4. **Include self-play.** If a change helps against baseline but not in self-play,
   suspect exploitation rather than improvement.
5. **Crossover for mixed games.** Run each seed twice with slot assignment
   swapped, to cancel starting-position effects.

## Human verification gate

Before trusting any downstream number, confirm:

1. Same config run twice → identical results
2. Arms A and B saw the same maps (log and diff a per-game map hash)
3. `grep -r holdout tune.py` finds nothing
4. A deliberately-introduced regression is caught at the stated sample size

Item 4 is the best test of the test.

## Two-machine tuning split (2026-08-01)

The campaign parallelises across two boxes because the two halves need
different regimes, not just more cores. Plan and rationale:
`doc/deep-ai/findings/objective-layer.md`.

**Machine A -- economy, military zeroed.** Deep's military surcharge
is pinned off by a start file, so only block E moves and the shield
budget it allocates is stable across candidates:

    python3 benchmark/tune.py \
      --start benchmark/tuning/econ-regime.ini \
      --blocks E --rounds 1 \
      --config benchmark/configs/garden2b-150.conf,benchmark/configs/lategame-80.conf \
      --count 8 --gens 4 --pop 6 --jobs $(nproc) \
      --out benchmark/results/tuning/econ

`garden2b-150` (2 players, islands, many settlers/workers, 150 turns)
and `lategame-80` (6 players, techlevel 50, 8 settlers + 10 workers,
80 turns) are truncated copies of the shipped cells -- economy ramps,
short horizons, little time for a war to decide the game.

**Machine B -- the war half, blended objective.** Blocks S and M
against three regimes at once; the objective is the mean over configs,
so a candidate cannot overfit one map shape:

    python3 benchmark/tune.py --blocks S,M --rounds 2 \
      --config benchmark/configs/isolated.conf,benchmark/configs/cramped.conf,benchmark/configs/contested.conf \
      --count 8 --gens 4 --pop 6 --jobs $(nproc) \
      --out benchmark/results/tuning/war

(There is no `compact.conf`; the small crowded cell is `cramped` --
3 players, 30x30, 300 turns.)

**Blended `--config a,b,c` is not the same as `--mixed`.** The first
averages the paired objective over several configs. The second puts
both arms inside ONE game with seat mirroring, which is the only way
to see defects that never fire in deep-vs-deep worlds. Use blended for
the campaign; use `--mixed` for a confirmation pass.

**Merging the two machines, and the experiment hidden in it.**

    # take block E from machine A, blocks S+M from machine B
    python3 - <<'PY'
    def read(p):
        d = {}
        for line in open(p):
            line = line.split(";", 1)[0].strip()
            if line and not line.startswith(("[", "#")):
                k, _, v = line.partition("=")
                d[k.strip()] = v.strip()
        return d
    econ = read("benchmark/results/tuning/econ/best-tuning.ini")
    war = read("benchmark/results/tuning/war/best-tuning.ini")
    blocks = read("benchmark/tuning-blocks.ini")
    e = [x.strip() for x in blocks["E"].split(",")]
    merged = dict(war)
    merged.update({k: v for k, v in econ.items() if k in e})
    with open("benchmark/tuning/merged.ini", "w") as f:
        f.write("[tuning]\n")
        for k, v in sorted(merged.items()):
            f.write(f"{k} = {v}\n")
    PY

    python3 benchmark/tune.py --evaluate benchmark/tuning/merged.ini \
      --config <machine B's config list> --count 8

If the merged file scores at least as well as machine B's own best,
the blocks composed and the split was sound. If it scores WORSE, the
economy constants were tuned against a budget the war constants then
changed -- the coupling is real, and the objective layer has to land
before more tuning is worth buying. Either way the answer is one
cheap evaluation, and it is the same question `--rounds 2` asks on a
single box via the alternation table.

Final numbers come only from `--report-holdout`, never from a
training run.
