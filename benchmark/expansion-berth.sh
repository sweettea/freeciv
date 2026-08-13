#!/usr/bin/env bash
# EXPANSION slice 3 (2026-08-13): the demand side of the lift geometry.
#
#   ./benchmark/expansion-berth.sh live      # 2 isolated-150, counters on
#   ./benchmark/expansion-berth.sh knob      # the same, routing on
#   ./benchmark/expansion-berth.sh identity  # 10 paired smoke seeds vs base
#   ./benchmark/expansion-berth.sh ab        # 16 mirrored isolated-150, off vs on
#
# Machine budget for this epic: jobs 2, ports 7200-7299, its own
# worktree and its own build/. Two derby servers own the rest of the
# box.
#
# What is being measured: slice 2 banked the finding that 60 of its 67
# positive lift gaps were founders queued beside water that closes on
# their own continent, with 4-16 idle berths on the bridging water one
# tile away. deep-berth counts that population from the founder's side
# (coast=1 serves=0), deep-berthroute counts the walks the knob orders,
# and liftgap.py re-reads the SAME gap counter in both arms so the
# before/after is the slice-2 number itself, not a new one.
#
# The identity stage builds the base binary from a `git archive` export
# into .berth-base/ (no git worktree registered, the shared checkout is
# never touched), plays the same 10 smoke seeds on both binaries with
# identity tuning on both sides, and asserts the scorelogs are
# byte-identical seed by seed. Anything less than 10/10 means the census
# or the default-off knob moved the game.
#
# Two traps this stage is written around, both from STATUS 2026-08-12:
#   * `meson setup <builddir>` with no source argument takes the
#     CALLER's cwd, which would make the "base" binary the branch under
#     test and the proof worthless. The source dir is always explicit.
#   * A proof can pass because both sides are the branch. Stage c also
#     greps the BASE arm's logs for `deep-berth `, a counter only this
#     branch can emit, and fails if it is there.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=1bcf470d5a
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.berth-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary this run executes, by copying it out of build/ before a
# single game starts: an epic that alternates edit, build and look is
# exactly where a run gets rebuilt under itself. The server's RUNPATH is
# $ORIGIN/, so libfreeciv.so travels with it and the copy is
# self-contained.
#
# The pin directory is PER STAGE, and that is not tidiness: pinning two
# stages to one path is the same taint by another route. It happened
# here -- a second stage's cp -f landed on the binary a live game was
# executing and the server took a bus error at turn 146, because the
# text pages it was demand-paging had been replaced underneath it. The
# stage name in the path is what makes "do not rebuild the binary a run
# is executing" mechanically true rather than a thing to remember.
pin_server() {
  pin=$ROOT/.berth-pin/$1
  mkdir -p "$pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$pin/"
  cp -f "$SERVER" "$pin/freeciv-server"
  SERVER=$pin/freeciv-server
  echo "pinned: $SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
}

case "${1:-}" in
live)
  OUT=$ROOT/benchmark/results/berth-live
  pin_server "$1"
  echo "=== liveness: 2 isolated-150 games, counters at LOG_NORMAL ==="
  TUNING=$ROOT/benchmark/tuning/identity.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/isolated-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7200 --out "$OUT"
  ./benchmark/berth.py "$OUT"
  ./benchmark/liftgap.py "$OUT" | head -20
  ;;

knob)
  # The same two games with the routing on, so the arm can be SEEN
  # executing. Not an A/B: two seeds price nothing and the games diverge
  # from the first walk.
  OUT=$ROOT/benchmark/results/berth-knob
  pin_server "$1"
  echo "=== liveness: 2 isolated-150 games, expansion-berth.ini ==="
  TUNING=$ROOT/benchmark/tuning/expansion-berth.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/isolated-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7200 --out "$OUT"
  ./benchmark/berth.py "$OUT"
  ./benchmark/liftgap.py "$OUT" | head -20
  ;;

identity)
  OUT=$ROOT/benchmark/results/berth-identity
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

  pin_server "$1"
  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7220;;
      head) bin=$SERVER;      port=7240;;
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

  # The proof must not be able to pass because both sides are HEAD.
  if grep -rqs "deep-berth " "$OUT"/base/; then
    echo "STOP: the base arm emitted deep-berth -- it is not the base." >&2
    exit 1
  fi
  echo "provenance: base logs carry no deep-berth line, as required"
  [ "$same" -eq "$SEEDS" ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

ab)
  # One knob, one binary, both arms. isolated-150 is the venue slice 2
  # measured the gap on, so the before/after of its own counter is
  # readable seed by seed.
  OUT=$ROOT/benchmark/results/berth-ab
  SEEDS=${2:-16}
  pin_server "$1"
  for arm in off on; do
    case $arm in
      off) ini=$ROOT/benchmark/tuning/identity.ini;      port=7260;;
      on)  ini=$ROOT/benchmark/tuning/expansion-berth.ini; port=7280;;
    esac
    echo "=== A/B arm $arm: $SEEDS isolated-150 games ==="
    TUNING=$ini ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/isolated-150.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$arm"
  done
  echo "=== paired compare ==="
  ./benchmark/paircmp.py "$OUT/off" "$OUT/on" --arm deep
  echo "=== gap counter, off arm ==="
  ./benchmark/liftgap.py "$OUT/off" | head -22
  echo "=== gap counter, on arm ==="
  ./benchmark/liftgap.py "$OUT/on" | head -22
  echo "=== berth census, both arms ==="
  ./benchmark/berth.py "$OUT/off"
  ./benchmark/berth.py "$OUT/on"
  ;;

*)
  echo "usage: $0 {live|knob|identity|ab [seeds]}" >&2; exit 1;;
esac
