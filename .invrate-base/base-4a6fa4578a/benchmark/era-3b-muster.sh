#!/usr/bin/env bash
# ERA phase 3b (2026-08-12): identity and liveness for MUSTER.
#
#   ./benchmark/era-3b-muster.sh identity  # 10 paired smoke seeds vs base
#   ./benchmark/era-3b-muster.sh live      # 2 tech50war-200 games, knob on
#
# Machine budget for this epic: jobs 2, ports 7600+. A derby and an A/B
# batch own the rest of the box, so nothing here is an A/B, nothing here
# runs at power, and NO tech-50 A/B is taken tonight -- that arm wants
# >= 16 mirrored seeds against the pv1+reach baseline and is tomorrow's
# machine work.
#
# The identity stage builds the base binary from a `git archive` export
# into .muster-base/ (no git worktree registered, the shared checkout is
# never touched), plays the same 10 smoke seeds on both binaries with
# identity tuning on both sides, and asserts the scorelogs are
# byte-identical seed by seed. Anything less than 10/10 means a counter
# or a default-off knob moved the game.
#
# Two traps this stage is written around, both from STATUS 2026-08-12:
#   * `meson setup <builddir>` with no source argument takes the
#     CALLER's cwd, which would make the "base" binary the branch under
#     test and the proof worthless. The source dir is always explicit.
#   * A proof can pass because both sides are the branch. So stage c
#     also greps the BASE arm's logs for `mform=`, a deep-opodds field
#     only this branch can emit, and fails if it is there.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=c93649faf4
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.muster-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=2

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/muster-identity
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
  if grep -rqm1 'mform=' "$OUT"/base/smoke-*/deep/seed-1/game.log; then
    echo "STOP: the BASE arm emits mform=, a field only this branch has;" >&2
    echo "      the base build is the branch and the proof is void." >&2
    exit 1
  fi
  # Resolve each side's cell EXPLICITLY rather than by glob. The
  # result key includes the server sha, so a rebuilt head lands in a
  # SECOND smoke-<key> directory beside the first -- and a glob that
  # then expands to two paths makes every seed read as "MISSING",
  # which is a passing-looking failure of the wrong kind. Newest cell
  # per side wins, and its name is printed so the pair is auditable.
  cell() { ls -1dt "$1"/smoke-* 2>/dev/null | head -n 1; }
  base_cell=$(cell "$OUT/base"); head_cell=$(cell "$OUT/head")
  echo "base cell: $base_cell"
  echo "head cell: $head_cell"
  [ -n "$base_cell" ] && [ -n "$head_cell" ] || {
    echo "STOP: could not resolve both cells." >&2; exit 1; }
  [ "$base_cell" != "$head_cell" ] || {
    echo "STOP: both sides resolved to the SAME cell." >&2; exit 1; }
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

live)
  # The venue the mechanism was designed for, with the knob on. TWO
  # seeds price nothing (rule 6) -- this reads counters only: do
  # musters form, do they accumulate across turns, and does at least
  # one launch or abandon fire with its counter.
  OUT=$ROOT/benchmark/results/muster-live
  echo "=== liveness: 2 tech50war-200 games, op_muster = 1 ==="
  TUNING=$ROOT/benchmark/tuning/era-3b-muster.ini \
    ./benchmark/run.sh --count 2 --arms deep \
      --config benchmark/configs/tech50war-200.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7640 --out "$OUT"
  echo "counters: $OUT/tech50war-200-*/deep/seed-*/game.log"
  ;;

*)
  echo "usage: $0 {identity|live}" >&2; exit 1;;
esac
