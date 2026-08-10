#!/usr/bin/env bash
# Overnight ECON-only tuning (2026-08-10, rev 2 of the 18h plan).
# Launch from YOUR shell (tool-call nohup gets reaped):
#
#   nohup ./benchmark/tune-tonight.sh > tune-tonight.log 2>&1 &
#
# Why ECON only: stage-A holdouts (tune-18h.log) proved MIL evolution
# on 8 seeds is DESTRUCTIVE (mixed holdout +7.3 vs untuned +55.3) and
# the human call (2026-08-10) is that MIL is not at block-freeze anyway
# (M4b knobs just landed, M4a unmerged, NETTHREAT queued) -- tuning it
# now buys values we would discard. ECON is the block that PROVED it
# transfers: -61.1 -> -13.7 paired lategame-80 holdout from 8 seeds.
# This run doubles the seeds and re-tunes ECON from the composed file.
# G is skipped: provably flat on 8 seeds (delta +0.0); retest at the
# finale. Start file: benchmark/tuning/pv1-econ-d2.ini = compiled
# defaults + pv1 overrides + d2's ECON+G values, MIL principled.
#
# Every stage resumes from the content-addressed cache if interrupted.
# ~1100 games, ~11h at 10 jobs. Safe to leave running past quota reset.

set -eu
cd "$(dirname "$0")/.."
SERVER=$PWD/derby4/freeciv-server-9275e9bb6d
T=benchmark/results/tuning
J=10
START=benchmark/tuning/pv1-econ-d2.ini

echo "=== V1: composed file, paired lategame-80 holdout (expect ~-14) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $START \
  --config benchmark/configs/lategame-80.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/v1-composed-econ

echo "=== V2: composed file, mixed contested holdout (expect ~+55) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $START --mixed \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/v2-composed-mixed

echo "=== ECON pass: 16 seeds (vs 8), from composed file ($(date)) ==="
python3 benchmark/tune.py --blocks ECON \
  --config benchmark/configs/lategame-80.conf,benchmark/configs/isolated-150.conf \
  --count 16 --gens 3 --pop 10 \
  --start $START \
  --server "$SERVER" --jobs $J --out $T/d6-econ-wide

echo "=== F1: d6 winner, paired lategame-80 holdout ($(date)) ==="
python3 benchmark/tune.py --report-holdout $T/d6-econ-wide/best-tuning.ini \
  --config benchmark/configs/lategame-80.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/f1-d6-econ

echo "=== F2: d6 winner, paired isolated-150 holdout ($(date)) ==="
python3 benchmark/tune.py --report-holdout $T/d6-econ-wide/best-tuning.ini \
  --config benchmark/configs/isolated-150.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/f2-d6-isolated

echo "=== F3: d6 winner, mixed contested holdout (MIL-untouched guard, expect ~+55) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $T/d6-econ-wide/best-tuning.ini --mixed \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/f3-d6-mixed

echo "=== all stages complete ($(date)) ==="
