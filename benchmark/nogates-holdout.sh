#!/usr/bin/env bash
# Holdout report for defaults+gates (benchmark/tuning/defaults.ini)
# on the same 20 holdout seeds and pinned binary as tune-postfix stage 4.
# Decides whether s4's -52.5/-32.6 is tuning damage or a pre-existing
# post-fix gap (findings/tune-postfix-verdict-2026-08-25.md).
#
# Baseline games are tuning-independent, so s4's are hardlinked in
# rather than re-run: tune.py skips any game dir carrying DONE.
set -eu
cd "$(dirname "$0")/.."
SERVER=$PWD/.tune-postfix-bin/freeciv-server
T=benchmark/results/tune-postfix
N="nice -n 10"

for pair in s6-nogates-war:s4-holdout-war s6-nogates-econ:s4-holdout-econ; do
  new=${pair%%:*} old=${pair##*:}
  mkdir -p "$T/$new"
  [ -d "$T/$new/baseline" ] || cp -al "$T/$old/baseline" "$T/$new/baseline"
done

echo "=== gates-off defaults holdout: contested ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout benchmark/tuning/defaults.ini \
  --config benchmark/configs/contested.conf --seeds benchmark/seeds/holdout.txt \
  --count 20 --portbase 7700 --server "$SERVER" --jobs 10 --out $T/s6-nogates-war

echo "=== gates-off defaults holdout: lategame-80 ($(date)) ==="
$N python3 benchmark/tune.py --report-holdout benchmark/tuning/defaults.ini \
  --config benchmark/configs/lategame-80.conf --seeds benchmark/seeds/holdout.txt \
  --count 20 --portbase 7700 --server "$SERVER" --jobs 10 --out $T/s6-nogates-econ

echo "=== defaults holdout complete ($(date)) ==="
touch "$T/S6-DONE"
