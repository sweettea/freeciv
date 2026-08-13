#!/usr/bin/env bash
# EXPANSION slice 1 (2026-08-12): liveness and identity for the
# founding-decision counters, and for the net-colony-value knobs that
# respond to what they say.
#
#   ./benchmark/expansion-slice1.sh live      # 2 lategame-80 games, counters on
#   ./benchmark/expansion-slice1.sh identity  # 10 paired smoke seeds vs base
#
# Machine budget for this epic: jobs 2, ports 7250+. A night batch owns
# the rest of the box, so nothing here is an A/B and nothing here runs
# at power.
#
# The identity stage builds the base binary from a `git archive` export
# into .expansion-base/ (no git worktree registered, the shared checkout
# is never touched), then plays the same 10 smoke seeds on both binaries
# with identity tuning on both sides and asserts the scorelogs are
# byte-identical seed by seed. Anything less than 10/10 means a counter
# or a default-off knob moved the game.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=ccf9e10e55
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.expansion-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=2

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

case "${1:-}" in
live)
  OUT=$ROOT/benchmark/results/expansion-live
  echo "=== liveness: 2 lategame-80 games, counters at LOG_NORMAL ==="
  TUNING=$ROOT/benchmark/tuning/identity.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/lategame-80.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7250 --out "$OUT"
  echo "counters: $OUT/lategame-80-*/deep/seed-*/game.log"
  ;;

knob)
  # The same two games with cm_founder_food switched on, so the new
  # arm can be SEEN executing. Not an A/B: two seeds price nothing.
  OUT=$ROOT/benchmark/results/expansion-knob
  echo "=== liveness: 2 lategame-80 games, cm_founder_food = 1.0 ==="
  TUNING=$ROOT/benchmark/tuning/expansion-cmfound.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/lategame-80.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7250 --out "$OUT"
  echo "counters: $OUT/lategame-80-*/deep/seed-*/game.log"
  ;;

identity)
  OUT=$ROOT/benchmark/results/expansion-identity
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
      base) bin=$BASE_SERVER; port=7280;;
      head) bin=$SERVER;      port=7300;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: scorelog assertion ==="
  same=0; diff_n=0; missing=0
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -n "$SEEDS"); do
    a=$(echo "$OUT"/base/smoke-*/deep/seed-"$seed"/scorelog.txt)
    b=$(echo "$OUT"/head/smoke-*/deep/seed-"$seed"/scorelog.txt)
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

*)
  echo "usage: $0 {live|identity}" >&2; exit 1;;
esac
