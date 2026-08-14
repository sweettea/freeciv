#!/usr/bin/env bash
# EXPANSION slice 2 (2026-08-13): liveness and identity for the lift
# demand-supply gap counter, the hull commission it feeds, and the
# muster-phase mission re-validation.
#
#   ./benchmark/expansion-lifts.sh live      # 2 isolated-150, counters on
#   ./benchmark/expansion-lifts.sh knob      # the same, commission on
#   ./benchmark/expansion-lifts.sh identity  # 10 paired smoke seeds vs base
#
# Machine budget for this epic: jobs 2, ports 7100+. The A/B is queued
# for the daytime machine; nothing here runs at power and nothing here
# is a claim that the mechanism pays.
#
# The identity stage builds the base binary from a `git archive` export
# into .explifts-base/ (no git worktree registered, the shared checkout
# is never touched), plays the same 10 smoke seeds on both binaries with
# identity tuning on both sides, and asserts the scorelogs are
# byte-identical seed by seed. Anything less than 10/10 means a counter
# or a default-off knob moved the game.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=304e36af41
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.explifts-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=2

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary this run executes, by copying it out of build/ before
# a single game starts: the pinning rule says a run's binary must not
# be rebuilt under it, and in an epic that alternates "edit, build,
# look" that is a real hazard rather than a formality. FREECIV_DATA_PATH
# is set from the repo by run-game.sh, so a copy anywhere runs the same.
#
# The server's RUNPATH is $ORIGIN/, so the shared library travels with
# it and a pinned copy is self-contained.
pin_server() {
  mkdir -p "$ROOT/.explifts-pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$ROOT/.explifts-pin/"
  cp -f "$SERVER" "$ROOT/.explifts-pin/freeciv-server"
  SERVER=$ROOT/.explifts-pin/freeciv-server
  echo "pinned: $SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
}

case "${1:-}" in
live)
  OUT=$ROOT/benchmark/results/explifts-live
  pin_server
  echo "=== liveness: 2 isolated-150 games, counters at LOG_NORMAL ==="
  TUNING=$ROOT/benchmark/tuning/identity.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/isolated-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7100 --out "$OUT"
  echo "counters: $OUT/isolated-150-*/deep/seed-*/game.log"
  ;;

knob)
  # The same two games with the commission and the muster release on,
  # so both arms can be SEEN executing. Not an A/B: two seeds price
  # nothing, and the games diverge from the first firing.
  OUT=$ROOT/benchmark/results/explifts-knob
  pin_server
  echo "=== liveness: 2 isolated-150 games, expansion-lifts.ini ==="
  TUNING=$ROOT/benchmark/tuning/expansion-lifts.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/isolated-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7100 --out "$OUT"
  echo "counters: $OUT/isolated-150-*/deep/seed-*/game.log"
  ;;

identity)
  OUT=$ROOT/benchmark/results/explifts-identity
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

  pin_server
  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7120;;
      head) bin=$SERVER;      port=7140;;
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
  echo "usage: $0 {live|knob|identity}" >&2; exit 1;;
esac
