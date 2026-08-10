#!/usr/bin/env bash
# Three-day unattended local tuning (2026-08-07, quota outage).
# Launch from YOUR shell (survives; tool-call nohup gets reaped):
#
#   nohup ./benchmark/tune-3day.sh > tune-3day.log 2>&1 &
#
# Uses the PINNED merged binary (derby4/) — do NOT rebuild build/ while
# this runs; the cache is keyed on the binary, so a rebuild wastes
# nothing but a mid-script swap would split stages across binaries.
# Every stage resumes from the content-addressed cache if interrupted:
# rerunning this script skips finished games.
#
# Design (venue discipline per findings/tuning-first-principles + venues):
#   1. MIL block on contested, MIXED head-to-head objective — the only
#      objective that punishes disarmament with real losses (a6 lesson).
#   2. ECON+G blocks on lategame-80 + isolated-150 blend, starting from
#      stage 1's winner.
#   3. MIL round 2 from stage 2's winner — the per-round delta IS the
#      separability measurement (campaign gate 8): flat = near-separable.
#   4. Holdout reports (fixed tuning, no optimisation) on both regimes.
# Never tunes on holdout seeds (tune.py refuses anyway). ~1400 games,
# ~2.5 days at 10 jobs.

set -eu
cd "$(dirname "$0")/.."
SERVER=$PWD/build/freeciv-server
T=benchmark/results/tuning
J=10

echo "=== stage 1: MIL on contested, mixed ($(date)) ==="
python3 benchmark/tune.py --blocks MIL --mixed \
  --config benchmark/configs/contested.conf \
  --count 8 --gens 4 --pop 10 \
  --start benchmark/tuning/principled-v1.ini \
  --server "$SERVER" --jobs $J --out $T/d1-mil-mixed

echo "=== stage 2: ECON+G on econ blend ($(date)) ==="
python3 benchmark/tune.py --blocks ECON,G \
  --config benchmark/configs/lategame-80.conf,benchmark/configs/isolated-150.conf \
  --count 8 --gens 3 --pop 10 \
  --start $T/d1-mil-mixed/best-tuning.ini \
  --server "$SERVER" --jobs $J --out $T/d2-econ

echo "=== stage 3: MIL round 2 (separability) ($(date)) ==="
python3 benchmark/tune.py --blocks MIL --mixed \
  --config benchmark/configs/contested.conf \
  --count 8 --gens 3 --pop 10 \
  --start $T/d2-econ/best-tuning.ini \
  --server "$SERVER" --jobs $J --out $T/d3-mil-r2

echo "=== stage 4: holdout reports ($(date)) ==="
python3 benchmark/tune.py --report-holdout $T/d3-mil-r2/best-tuning.ini \
  --config benchmark/configs/contested.conf --seeds benchmark/seeds/holdout.txt \
  --count 20 --server "$SERVER" --jobs $J --out $T/d4-holdout-war
python3 benchmark/tune.py --report-holdout $T/d3-mil-r2/best-tuning.ini \
  --config benchmark/configs/lategame-80.conf --seeds benchmark/seeds/holdout.txt \
  --count 20 --server "$SERVER" --jobs $J --out $T/d4-holdout-econ

echo "=== all stages complete ($(date)) ==="
