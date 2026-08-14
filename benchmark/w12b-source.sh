#!/usr/bin/env bash
# W12 slice 2 (task source beyond the radius ceiling): the proofs this
# epic owes, re-runnable rather than described.
#
#   ./benchmark/w12b-source.sh identity   # 10 paired smoke seeds vs base,
#                                         # deep AND mixed_db cells
#   ./benchmark/w12b-source.sh live       # 2 lategame-80, source on
#   ./benchmark/w12b-source.sh ab [N]     # N (default 16) mirrored
#                                         # lategame-80 seeds, source
#                                         # off vs on
#
# Machine budget: jobs 2, ports 7800-7849, this worktree's own build/.
#
# What is being measured: slice 1 left the worker task SOURCE capped by
# the advisor's radius ceiling -- corridor tiles between distant cities
# are never enumerated, so locality optimised a truncated demand set
# (STATUS "W12 slice 1" sections 2 and 7). The source offers exactly
# those tiles, priced by the same deep_trunk_price() the hook uses and
# discounted by the same worker_travel_discount, gated by
# trunk_source_gain (default 0.0 = today's ceiling byte-identically).
# deep-trunksrc counts the hidden demand (beyond=), what the source
# priced (offered=) and what it assigned (picked= plus a map-distance
# histogram); deep-worklocal's meanwalk= is slice 1's walking-distance
# instrument, read in both arms for the commuting regression watch.
#
# The identity stage builds the base binary from a `git archive` export
# into .w12b-base/ (no git worktree registered), plays the same 10
# smoke seeds on both binaries at full defaults, and asserts scorelogs
# byte-identical seed by seed -- in a deep cell AND a mixed_db cell,
# because the source runs inside deep's own settler_run and the mixed
# cell is where a leak into shared advisor state would show.
#
# Two traps this stage is written around (STATUS 2026-08-12):
#   * `meson setup <builddir>` with no source argument takes the
#     CALLER's cwd; the source dir is always explicit.
#   * A proof can pass because both sides are HEAD. Stage c greps the
#     BASE arm's logs for `deep-trunksrc`, a line only this branch can
#     emit, and fails if it is there.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=50b55eab00
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.w12b-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary a run executes by copying it (with libfreeciv.so --
# RUNPATH is $ORIGIN/) out of build/ before a single game starts. The
# pin directory is PER STAGE: pinning two stages to one path is how a
# live server gets its text pages replaced underneath it (bus error at
# turn 146, STATUS 2026-08-13).
pin_server() {
  pin=$ROOT/.w12b-pin/$1
  mkdir -p "$pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$pin/"
  cp -f "$SERVER" "$pin/freeciv-server"
  SERVER=$pin/freeciv-server
  echo "pinned: $SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
}

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/w12b-identity
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
      base) bin=$BASE_SERVER; port=7800;;
      head) bin=$SERVER;      port=7810;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep,mixed_db \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: scorelog assertion ==="
  fail=0
  for arm in deep mixed_db; do
    same=0; diff_n=0; missing=0
    for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                  | head -n "$SEEDS"); do
      a=$(echo "$OUT"/base/smoke-*/"$arm"/seed-"$seed"/scorelog.txt)
      b=$(echo "$OUT"/head/smoke-*/"$arm"/seed-"$seed"/scorelog.txt)
      if [ ! -r "$a" ] || [ ! -r "$b" ]; then
        echo "$arm seed $seed: MISSING scorelog"; missing=$((missing + 1))
      elif cmp -s "$a" "$b"; then
        same=$((same + 1))
      else
        echo "$arm seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
      fi
    done
    echo "identity $arm: $same/$SEEDS byte-identical, $diff_n differ," \
         "$missing missing"
    [ "$same" -eq "$SEEDS" ] || fail=1
  done

  # The proof must not be able to pass because both sides are HEAD.
  if grep -rqs "deep-trunksrc" "$OUT"/base/; then
    echo "STOP: the base arm emitted deep-trunksrc -- not the base." >&2
    exit 1
  fi
  echo "provenance: base logs carry no deep-trunksrc line, as required"
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

live)
  # Two games with the source on, so the arm can be SEEN executing --
  # deep-trunksrc picked>0 is the liveness criterion the brief sets
  # (a knob-on run with zero beyond-ceiling tasks is a miss to report,
  # not ship). Not an A/B: two seeds price nothing.
  OUT=$ROOT/benchmark/results/w12b-live
  pin_server "$1"
  TUNING=$ROOT/benchmark/tuning/w12-source-on.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/lategame-80.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7820 --out "$OUT"
  echo "=== deep-trunksrc lines ==="
  grep -rh "deep-trunksrc" "$OUT"/lategame-80-*/deep/seed-*/game.log \
    2>/dev/null | tail -30 || true
  echo "=== pick lines ==="
  grep -rh "deep-trunkpick" "$OUT"/lategame-80-*/deep/seed-*/game.log \
    2>/dev/null | tail -10 || true
  ;;

ab)
  # One knob, one binary, both arms: off = tuning/w12-on.ini (slice
  # 1's probe values), on = the same plus trunk_source_gain=1.0.
  # lategame-80 is the venue slice 1's liveness measured both worker
  # mechanisms on, so deep-trunk, deep-worklocal and deep-trunksrc are
  # all readable seed by seed.
  OUT=$ROOT/benchmark/results/w12b-ab
  SEEDS=${2:-16}
  pin_server "$1"
  # ON first, deliberately: if the source turns out not to fire on this
  # venue the whole comparison is void, and that is worth learning in
  # the first arm rather than the second.
  for arm in on off; do
    case $arm in
      on)  ini=$ROOT/benchmark/tuning/w12-source-on.ini; port=7840;;
      off) ini=$ROOT/benchmark/tuning/w12-on.ini;        port=7830;;
    esac
    echo "=== A/B arm $arm: $SEEDS lategame-80 games ==="
    TUNING=$ini ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/lategame-80.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$arm"
  done
  off_cell=$(echo "$OUT"/off/lategame-80-*)
  on_cell=$(echo "$OUT"/on/lategame-80-*)
  echo "=== paired compare ==="
  ./benchmark/paircmp.py "$off_cell" "$on_cell" \
    --pat trunksrc-turns="deep-trunksrc turn=" \
    --pat trunksrc-active="offered=[1-9]" \
    --pat trunk-picks="deep-trunkpick turn=" \
    --arm deep
  ;;

*)
  echo "usage: $0 {identity|live|ab [seeds]}" >&2; exit 1;;
esac
