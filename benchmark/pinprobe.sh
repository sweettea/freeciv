#!/usr/bin/env bash
# NETTHREAT item 2's cheaper probe (2026-08-13): do enemy garrisons
# ever actually come at us?
#
#   ./benchmark/pinprobe.sh identity  # 10 paired smoke seeds vs base
#   ./benchmark/pinprobe.sh corpus    # 8 mirrored tech50war-200 seeds
#
# The probe is counter-only, so identity is not a courtesy here: a
# counter that moved the game would be a counter that cannot be
# trusted to report on it. Ports 7600-7699, jobs 2 -- a benchmark
# batch and other epics own the rest of the box.
#
# Two traps this script is written around, both from STATUS 2026-08-12,
# and both of which produce a PASSING-LOOKING failure:
#   * `meson setup <builddir>` with no source argument takes the
#     CALLER's cwd, which would build the branch as its own base. The
#     source directory is always passed explicitly.
#   * A glob over `smoke-*` expands to two cells once the head binary
#     is rebuilt, and every seed then reads as MISSING rather than as
#     a mismatch. Each side's cell is resolved explicitly and printed.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=4a6fa4578a
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.pin-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=2

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/pin-identity
  SEEDS=10
  echo "=== identity stage a: base binary for $BASE_SHA ==="
  if [ -x "$BASE_SERVER" ]; then
    echo "cached: $BASE_SERVER"
  else
    rm -rf "$BASE_TREE"
    mkdir -p "$BASE_TREE"
    git archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
    # shellcheck disable=SC2086
    meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS
    nice -n 10 ninja -C "$BASE_TREE/build" -j "$JOBS"
  fi

  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7600;;
      head) bin=$SERVER;      port=7620;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: provenance + scorelog assertion ==="
  cell() { ls -1dt "$1"/smoke-* 2>/dev/null | head -n 1; }
  base_cell=$(cell "$OUT/base"); head_cell=$(cell "$OUT/head")
  echo "base cell: $base_cell"
  echo "head cell: $head_cell"
  [ -n "$base_cell" ] && [ -n "$head_cell" ] || {
    echo "STOP: could not resolve both cells." >&2; exit 1; }
  [ "$base_cell" != "$head_cell" ] || {
    echo "STOP: both sides resolved to the SAME cell." >&2; exit 1; }
  if grep -rqm1 'deep-pin ' "$base_cell"/deep/seed-*/game.log; then
    echo "STOP: the BASE arm emits deep-pin, a line only this branch" >&2
    echo "      has; the base build is the branch and the proof void." >&2
    exit 1
  fi
  if ! grep -rqm1 'deep-pin ' "$head_cell"/deep/seed-*/game.log; then
    echo "STOP: the HEAD arm emits no deep-pin at all -- the probe did" >&2
    echo "      not run, so identity proves nothing about it." >&2
    exit 1
  fi
  same=0; diff_n=0; missing=0
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -n "$SEEDS"); do
    a=$base_cell/deep/seed-$seed/scorelog.txt
    b=$head_cell/deep/seed-$seed/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "seed $seed: MISSING scorelog"; missing=$((missing + 1))
    elif cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      echo "seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
    fi
  done
  echo "identity: $same/$SEEDS byte-identical, $diff_n differ, $missing missing"
  [ "$same" -eq "$SEEDS" ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

corpus)
  # The venue where wars conclude -- the brief's own prerequisite,
  # because contested@150 produced 0 eliminations in 16/16 games and a
  # garrison that is never threatened is never pinned either. Mirrored
  # pair so both seatings of deep-vs-baseline contribute.
  OUT=$ROOT/benchmark/results/pin-corpus
  echo "=== corpus: 8 mirrored tech50war-200 seeds ==="
  ./benchmark/run.sh --count 8 --arms mixed_db,mixed_bd \
    --config benchmark/configs/tech50war-200.conf \
    --server "$SERVER" --jobs "$JOBS" --portbase 7640 --out "$OUT"
  ./benchmark/pincensus.py "$OUT"
  ;;

*)
  echo "usage: $0 {identity|corpus}" >&2; exit 1;;
esac
