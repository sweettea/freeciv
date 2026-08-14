# W00b — Benchmark harness

**Phase 0. Everything downstream is measured by this. Get it right.**

## Why

Every acceptance criterion in the project resolves to a number this produces. A
subtly wrong harness — unpaired seeds, holdout contamination, a metric that
doesn't mean what it says — invalidates months of work silently.

## Design

```
benchmark/
  run.sh              # --quick (10 paired seeds) | --full (100)
  tune.py             # CMA-ES over struct tuning; TRAIN seeds only
  report.py           # table, confidence intervals, effect sizes
  configs/            # the measurement matrix (below)
  seeds/train.txt     # tuning only
  seeds/holdout.txt   # reporting only — never tuned against
  results/            # content-addressed: (tuning_hash, config, seed)
  corpus/             # saved positions for L1 assertions
```

### Pairing is mandatory

Run every arm on **identical seed lists**. Unpaired at ~30% score SD, detecting a
5% effect needs ~550 games per arm. Paired at ρ≈0.8 needs **~110**.

### The measurement matrix

Minimum ~12 cells:
- Map size / players: small (4p), medium (8p), large (20p)
- Ruleset: civ2civ3, classic
- Settings: default, aggressive-expansion, migration-heavy
- Difficulty: hard

### Crossover for mixed games

When running old-vs-new in the *same* game, run each seed twice with slot
assignment swapped (deep in A/B, baseline in C/D; then reversed). Cancels
position effects and doubles power per map generated.

**Mixed games are the best demonstration and a mediocre measurement.** Use
separate paired games for the headline number.

### Metrics to extract

Final score; mean city size; cities founded vs improvements built; road tiles per
city; turn of first city capture; units lost per capture; settlers lost to
hostiles; techs from huts; explored fraction; migration net; doctrine switch turn;
campaign deadlock count; per-turn wall time; `AIT_*` bucket times.

### Resumability

Cache keyed on `(tuning_hash, config, seed)`. An interrupted `--full` resumes.
This matters more than it sounds when a laptop sleeps mid-run.

## Acceptance — and §5 is a HUMAN gate

## §5 Human verification checklist

**Do not proceed to Phase 1 until a human has confirmed all four.**

1. **Determinism.** Run the same config twice. Results identical.
2. **Pairing.** Run arms A and B. Confirm they saw the same maps (log the map
   hash per game and diff).
3. **Holdout isolation.** `grep -r holdout benchmark/tune.py` — the holdout seed
   file must not appear anywhere in the tuning path.
4. **Sensitivity.** Introduce a deliberate regression (e.g. halve settler want),
   confirm the harness catches it at the stated sample size, revert.

Item 4 is the single best test of the test. Budget a day for this section.

## Commits

```
test: add headless batch runner with fixed map and RNG seeds
test: add paired batch mode using identical seed lists across arms
test: extract per-player score and city metrics to CSV
test: add a content-addressed result cache keyed on tuning hash
test: add crossover slot assignment for mixed-aitype games
test: add saved-position corpus loader and assertion framework
test: add corpus positions for contested border, losing war, hut-rich start
server: allow enabling AI timing buckets via a server setting
test: add report generation with confidence intervals
```
