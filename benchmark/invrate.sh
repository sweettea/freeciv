#!/usr/bin/env bash
# INVASION-RATE (2026-08-13): the reinforcement slope and the
# registered embark allowance.
#
#   ./benchmark/invrate.sh live      # 2 tech65sea-150, counters on
#   ./benchmark/invrate.sh identity  # 10 paired smoke seeds vs base
#   ./benchmark/invrate.sh ab        # 16 tech65sea-150 seeds, slope 0 vs 1
#
# Machine budget for this epic: ports 7500-7599, jobs 2-3, nice.
#
# The identity stage builds the base binary from a `git archive` export
# into .invrate-base/ (no git worktree registered, the shared checkout
# is never touched), plays the same 10 smoke seeds on both binaries
# with identity tuning on both sides, and asserts the scorelogs are
# byte-identical seed by seed. Anything less than 10/10 means the two
# new counters or a default-off knob moved the game.
#
# The A/B is a ONE-KEY comparison on ONE binary: both arms are this
# head build, the control is tuning/identity.ini (every compiled
# default, invasion_reinforce_slope 0 = the flat model that shipped)
# and the test arm is tuning/invrate-slope.ini, which sets that one
# key and nothing else. tech65sea-150 is the venue because it is the
# only cell where invasions happen at all (see the config's header).

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=4a6fa4578a
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.invrate-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary this run executes, by copying it out of build/ before
# a single game starts. The server's RUNPATH is $ORIGIN/, so
# libfreeciv.so must travel with it or the pinned copy cannot exec.
# Per-stage pin dirs: never cp -f onto a binary a live game is running.
pin_server() {
  d=$ROOT/.invrate-pin/$1
  mkdir -p "$d"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$d/"
  cp -f "$SERVER" "$d/freeciv-server"
  SERVER=$d/freeciv-server
  echo "pinned: $SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
}

case "${1:-}" in
live)
  OUT=$ROOT/benchmark/results/invrate-live
  pin_server live
  echo "=== liveness: 2 tech65sea-150 games, robs=/rrate= at LOG_NORMAL ==="
  TUNING=$ROOT/benchmark/tuning/identity.ini \
    nice -n 5 ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/tech65sea-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7500 --out "$OUT"
  echo "counters: $OUT/tech65sea-150-*/deep/seed-*/game.log"
  ;;

identity)
  OUT=$ROOT/benchmark/results/invrate-identity
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

  pin_server identity
  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7520;;
      head) bin=$SERVER;      port=7540;;
    esac
    nice -n 5 ./benchmark/run.sh --count "$SEEDS" --arms deep,baseline \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: scorelog assertion ==="
  same=0; diff_n=0; missing=0; total=0
  for arm in deep baseline; do
    for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                  | head -n "$SEEDS"); do
      total=$((total + 1))
      a=$(echo "$OUT"/base/smoke-*/$arm/seed-"$seed"/scorelog.txt)
      b=$(echo "$OUT"/head/smoke-*/$arm/seed-"$seed"/scorelog.txt)
      if [ ! -r "$a" ] || [ ! -r "$b" ]; then
        echo "$arm seed $seed: MISSING scorelog"; missing=$((missing + 1))
      elif cmp -s "$a" "$b"; then
        same=$((same + 1))
      else
        echo "$arm seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
      fi
    done
  done
  echo "identity: $same/$total byte-identical, $diff_n differ, $missing missing"
  [ "$same" -eq "$total" ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

ab)
  OUT=$ROOT/benchmark/results/invrate-ab
  SEEDS=${SEEDS:-16}
  pin_server ab
  echo "=== A/B: $SEEDS tech65sea-150 seeds, one binary, one key ==="
  for arm in flat slope; do
    case $arm in
      flat)  tf=$ROOT/benchmark/tuning/identity.ini;     port=7560;;
      slope) tf=$ROOT/benchmark/tuning/invrate-slope.ini; port=7580;;
    esac
    TUNING=$tf nice -n 5 ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/tech65sea-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$arm"
  done
  echo "=== paired comparison ==="
  ./benchmark/paircmp.py "$OUT"/flat/tech65sea-150-* \
                         "$OUT"/slope/tech65sea-150-* --arm deep
  ;;

*)
  echo "usage: $0 {live|identity|ab}" >&2; exit 1;;
esac
