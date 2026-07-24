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
