#!/usr/bin/env bash
# 18-hour follow-up to tune-3day.sh (2026-08-10, quota reset Tue 16:00).
# Launch from YOUR shell (tool-call nohup gets reaped):
#
#   nohup ./benchmark/tune-18h.sh > tune-18h.log 2>&1 &
#
# Context: tune-3day trained MIL on the MIXED objective (8 seeds) to
# +65.8, but the PAIRED holdout came back -32.8 (contested) / -13.7
# (lategame-80).  Two rival explanations:
#   overfit   - 8 training seeds were too few (the human predicted this)
#   mismatch  - the mixed head-to-head objective anti-correlates with
#               paired absolute score (inverse of the a6 lesson)
# Stage A separates them; stage B re-tunes with 2.5x the seeds either way.
#
# Uses the PINNED pair in derby4/ (same bytes tune-3day ran, so all its
# caches stay valid).  Rebuilding build/ during this run is now SAFE.

set -eu
cd "$(dirname "$0")/.."
SERVER=$PWD/derby4/freeciv-server-9275e9bb6d
T=benchmark/results/tuning
J=10
P=benchmark/tuning/principled-v1.ini
D3=$T/d3-mil-r2/best-tuning.ini

echo "=== A1: d3 winner on MIXED holdout (overfit test) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $D3 --mixed \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/a1-d3-mixed-holdout

echo "=== A2: principled-v1 on MIXED holdout (baseline) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $P --mixed \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/a2-pv1-mixed-holdout

echo "=== A3: principled-v1 on PAIRED contested holdout (baseline for -32.8) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $P \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/a3-pv1-paired-war

echo "=== A4: principled-v1 on PAIRED lategame-80 holdout (baseline for -13.7) ($(date)) ==="
python3 benchmark/tune.py --report-holdout $P \
  --config benchmark/configs/lategame-80.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/a4-pv1-paired-econ

echo "=== B: MIL mixed retune, 20 seeds (vs 8 last time) ($(date)) ==="
python3 benchmark/tune.py --blocks MIL --mixed \
  --config benchmark/configs/contested.conf \
  --count 20 --gens 4 --pop 10 \
  --start "$P" \
  --server "$SERVER" --jobs $J --out $T/d5-mil-wide

echo "=== C1: d5 winner on MIXED holdout ($(date)) ==="
python3 benchmark/tune.py --report-holdout $T/d5-mil-wide/best-tuning.ini --mixed \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/c1-d5-mixed-holdout

echo "=== C2: d5 winner on PAIRED contested holdout ($(date)) ==="
python3 benchmark/tune.py --report-holdout $T/d5-mil-wide/best-tuning.ini \
  --config benchmark/configs/contested.conf \
  --count 20 --server "$SERVER" --jobs $J --out $T/c2-d5-paired-war

echo "=== all stages complete ($(date)) ==="
