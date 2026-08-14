#!/usr/bin/env bash
# TECHMIL slice 1 -- tech_want writer census, two stages.
#
# Launch from YOUR shell (a tool-call nohup gets reaped), from the
# worktree that carries branch epic/techmil:
#
#   nohup ./benchmark/techmil-probe.sh > techmil-probe.log 2>&1 &
#
# STAGE 1 -- identity proof. The census is observation only: it reads
# tech_want[], diffs it against its own snapshot and logs. It must be
# provably inert. This stage plays the same 10 paired seeds twice,
# identity tuning on both sides, with the ONLY difference being the
# server binary: base 4a6fa4578a (the commit this branch forked from)
# against this worktree's build. It then asserts the scorelogs are
# BYTE-IDENTICAL seed by seed. Anything less than 10/10 means the
# census changed the game and stage 2 would be measuring the census.
#
# The base binary is built once into .techmil-probe/ from a
# `git archive` (no git worktree is registered, nothing in the shared
# checkout is touched) and cached there for reruns.
#
# STAGE 2 -- the census corpus. tech50war-200, mixed_dc and its mirror
# mixed_cd, 8 seeds each. Deep on odd seats and stock (ai/default) on
# even, then swapped, so every seat sees each module once and seat bias
# cancels. Not an A/B: both arms are the same binary at defaults and
# the DELIVERABLE IS THE COUNTER STREAM, not the score. Each game
# leaves deep-techwant (one line per readable AI player per turn, both
# sides) and deep-techsite (one line per writer site per deep player
# per turn) in its game.log.
#
# Venue limits, stated because they bound the reading: tech50war-200
# starts at techlevel 50, so roughly 37 of the ruleset's advances are
# still unresearched at turn 1. That is a live research venue, but a
# LATE one -- the early-game writers (settler ferry want, first
# defender) fire into an array whose cheap military techs are already
# known. A census on a from-scratch cell would weight the sites
# differently and is the obvious follow-up if slice 2 happens.
#
# Ports 7700-7799 only. Do NOT run this while another benchmark batch
# owns this worktree's binary.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=4a6fa4578a
SERVER=$ROOT/build/freeciv-server
PROBE=$ROOT/.techmil-probe
BASE_TREE=$PROBE/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
OUT=$ROOT/benchmark/results/techmil
JOBS=${JOBS:-6}
SEEDS=10
CENSUS_SEEDS=8

# Deliberately the options THIS worktree's build/ was configured with:
# base and head must differ in source, not in configuration.
MESON_OPTS="-Dclients=gtk3.22 -Dfcmp= -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Dserver=enabled -Ddebug=true
            -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || {
  echo "this worktree has no server binary at $SERVER -- build first" >&2
  exit 1
}

echo "=== TECHMIL probe start ($(date)) ==="
echo "head server : $SERVER"
echo "head commit : $(git rev-parse --short HEAD)"
echo "base commit : $BASE_SHA"
echo "out         : $OUT"

########################################################################
echo
echo "=== STAGE 1a: base binary for $BASE_SHA ($(date)) ==="
########################################################################
if [ -x "$BASE_SERVER" ]; then
  echo "cached: $BASE_SERVER"
else
  rm -rf "$BASE_TREE"
  mkdir -p "$BASE_TREE"
  # An export, not a worktree: nothing is registered in the shared
  # checkout and nothing there can be perturbed by this script. meson
  # is given its SOURCE DIR explicitly so it cannot walk up out of the
  # export and configure the real tree.
  git archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
  # shellcheck disable=SC2086
  meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS
  nice -n 10 ninja -C "$BASE_TREE/build" -j "$JOBS"
  echo "built: $BASE_SERVER"
fi

########################################################################
echo
echo "=== STAGE 1b: identity games, base vs head ($(date)) ==="
########################################################################
export TUNING=$ROOT/benchmark/tuning/identity.ini

for side in base head; do
  case $side in
    base) bin=$BASE_SERVER; port=7700;;
    head) bin=$SERVER;      port=7720;;
  esac
  echo "--- $side arm ($bin) ($(date)) ---"
  ./benchmark/run.sh --count "$SEEDS" --arms deep \
    --config benchmark/configs/smoke.conf \
    --server "$bin" --jobs "$JOBS" --portbase "$port" \
    --out "$OUT/$side"
done
unset TUNING

########################################################################
echo
echo "=== STAGE 1c: scorelog identity assertion ($(date)) ==="
########################################################################
same=0
diff_n=0
missing=0
# The NEWEST key dir per side, not a glob. A rebuilt binary changes the
# key, so a re-run leaves the previous run's dir sitting beside the new
# one and a bare `smoke-*` expands to two paths -- which reads as
# MISSING for every seed and fails the assertion for the wrong reason.
# That happened once; this is the fix.
basedir=$(ls -td "$OUT"/base/smoke-*/ 2>/dev/null | head -1)
headdir=$(ls -td "$OUT"/head/smoke-*/ 2>/dev/null | head -1)
echo "base dir: $basedir"
echo "head dir: $headdir"
for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -n "$SEEDS"); do
  a=$basedir/deep/seed-$seed/scorelog.txt
  b=$headdir/deep/seed-$seed/scorelog.txt
  if [ ! -r "$a" ] || [ ! -r "$b" ]; then
    echo "seed $seed: MISSING scorelog"
    missing=$((missing + 1))
  elif cmp -s "$a" "$b"; then
    same=$((same + 1))
  else
    echo "seed $seed: SCORELOGS DIFFER"
    diff_n=$((diff_n + 1))
  fi
done
echo "identity: $same/$SEEDS byte-identical, $diff_n differ, $missing missing"
if [ "$same" -ne "$SEEDS" ]; then
  echo "STOP: the census is NOT inert. Stage 2 would measure the" \
       "instrument. Fix before continuing." >&2
  exit 1
fi
echo "census proven inert."

########################################################################
echo
echo "=== STAGE 2: tech50war-200 mixed census, $CENSUS_SEEDS seeds ($(date)) ==="
########################################################################
./benchmark/run.sh --count "$CENSUS_SEEDS" --arms mixed_dc,mixed_cd \
  --config benchmark/configs/tech50war-200.conf \
  --server "$SERVER" --jobs "$JOBS" --portbase 7740 \
  --out "$OUT/census"

########################################################################
echo
echo "=== TECHMIL probe complete ($(date)) ==="
########################################################################
echo "census: benchmark/techcensus.py $OUT/census/tech50war-200-*"
