#!/usr/bin/env bash
# ERA instrumentation probe (2026-08-11), two stages.
#
# Launch from YOUR shell (a tool-call nohup gets reaped), from the
# worktree that carries branch epic/era-instrumentation:
#
#   nohup ./benchmark/era-probe.sh > era-probe.log 2>&1 &
#
# STAGE 1 -- counter identity proof. The three ERA counters
# (deep-expgate, deep-opodds, deep-wave) are observation only and must
# be provably inert. This stage plays the same 10 paired seeds twice,
# identity tuning on both sides, with the ONLY difference being the
# server binary: base 063a86bdf7 (the commit the branch forked from)
# against this worktree's build. It then asserts the scorelogs are
# BYTE-IDENTICAL, seed by seed. That is the proof the M4b off-switch
# runs used (sessions/progress-M4b.log, "10/10 scorelogs
# BYTE-IDENTICAL"), and anything less than 10/10 means a counter
# changed the game and must be fixed before stage 2's numbers mean
# anything.
#
# The base binary is built once into .era-probe/ from a `git archive`
# of 063a86bdf7 (no git worktree is registered, nothing in the shared
# checkout is touched) and cached there for reruns.
#
# STAGE 2 -- the decisive A/B. tech50war-200, mixed seat-mirrored, 8
# seeds, arm A = principled-v1 verbatim, arm B = the same file with
# expansion_war_survival = 0.0, which is the documented stock-restore
# value for deep's expansion gate (deepdefence.c:3657). A minus B
# prices deep's replacement gate on the venue where deep's head-to-head
# edge inverts. Both arms run on the SAME binary -- this worktree's --
# so the counters are on in both and every game leaves a deep-expgate /
# deep-opodds / deep-wave stream in its game.log for the mechanism
# read, whatever the score says.
#
# Runtime: stage 1 ~20 games at smoke length plus one base build;
# stage 2 16 games at 8p/32x32/T200, the expensive half (~1h at 6
# jobs, from the calib2 timings). Every game resumes from the
# content-addressed cache if interrupted, so a re-launch is cheap.
#
# Do NOT run this while another benchmark batch owns the machine: it
# builds a binary, and CLAUDE.md's rule is that a build in flight
# taints a run in flight.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=063a86bdf7
SERVER=$ROOT/build/freeciv-server
PROBE=$ROOT/.era-probe
BASE_TREE=$PROBE/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
OUT=$ROOT/benchmark/results/era-probe
JOBS=6
SEEDS=10

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || {
  echo "this worktree has no server binary at $SERVER -- build first" >&2
  exit 1
}

echo "=== ERA probe start ($(date)) ==="
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
  # checkout and nothing there can be perturbed by this script.
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
# Identity tuning on BOTH sides: every tunable at its compiled default,
# so the only difference between the two runs is the binary. (A tuning
# file is still passed rather than none -- tuning=none is not a valid
# control, see benchmark/tuning/identity.ini.)
export TUNING=$ROOT/benchmark/tuning/identity.ini

for side in base head; do
  case $side in
    base) bin=$BASE_SERVER; port=6800;;
    head) bin=$SERVER;      port=6900;;
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
for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -n "$SEEDS"); do
  a=$(echo "$OUT"/base/smoke-*/deep/seed-"$seed"/scorelog.txt)
  b=$(echo "$OUT"/head/smoke-*/deep/seed-"$seed"/scorelog.txt)
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
  echo "STOP: the counters are NOT inert. Stage 2 would measure a" \
       "behaviour change, not the gate. Fix before continuing." >&2
  exit 1
fi
echo "counters proven inert."

########################################################################
echo
echo "=== STAGE 2: tech50war-200 mixed A/B, 8 seeds ($(date)) ==="
########################################################################
# Arm A: principled-v1 verbatim (expansion_war_survival at its compiled
# 0.9 placeholder). Arm B: the same file with the gate off.
echo "--- arm A: era-a-pv1.ini ($(date)) ---"
python3 benchmark/tune.py --evaluate benchmark/tuning/era-a-pv1.ini \
  --mixed --config benchmark/configs/tech50war-200.conf \
  --count 8 --jobs "$JOBS" --server "$SERVER" \
  --portbase 7000 --out "$OUT/a-pv1"

echo "--- arm B: era-b-noexpgate.ini ($(date)) ---"
python3 benchmark/tune.py --evaluate benchmark/tuning/era-b-noexpgate.ini \
  --mixed --config benchmark/configs/tech50war-200.conf \
  --count 8 --jobs "$JOBS" --server "$SERVER" \
  --portbase 7200 --out "$OUT/b-noexpgate"

########################################################################
echo
echo "=== ERA probe complete ($(date)) ==="
########################################################################
echo "A/B: compare the two EVALUATE lines above (mean paired diff,"
echo "     deep side minus stock side, 8 mirrored seeds)."
echo "counters: benchmark/results/era-probe/{a-pv1,b-noexpgate}/evaluate/"
echo "          */*/seed-*/game.log carry deep-expgate, deep-opodds and"
echo "          deep-wave; those are the mechanism read this epic is for,"
echo "          and they are worth reading even if the A/B is null."
