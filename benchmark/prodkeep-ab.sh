#!/usr/bin/env bash
# THE PRODUCTION COMPLETION BIAS (prod_keep_gate): its adjudication.
#
#   setsid bash ./benchmark/prodkeep-ab.sh identity   10 smoke seeds,
#                                                     pre-epic binary vs
#                                                     this branch's, gate
#                                                     off, scorelogs
#                                                     byte-compared
#   setsid bash ./benchmark/prodkeep-ab.sh ab         the paired A/B on
#                                                     TRAINING seeds:
#                                                     baseline once, then
#                                                     gate off and gate on
#   ./benchmark/prodkeep-ab.sh report                 the tables
#   ./benchmark/prodkeep-ab.sh census DIR...          settler starts vs
#                                                     abandonments per
#                                                     25-turn bucket
#
# Launch it from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped. Watch it by DONE counts;
# this box does not report its process table honestly (STATUS 2026-08-23).
#
# PORTS 7800-7849, this worktree only. Its own pinned binaries under
# .pin-bin/{base,fix}: base is the pre-epic build (the identity's control),
# fix is this branch's. Nothing outside this worktree is built or read.
#
# THE ARMS are one key apart, on top of the five adjudicated gates
# (benchmark/tuning/postfix-start.ini), which is the standing config the
# holdout attribution measured at -41.8 / -34.1:
#
#   off   postfix-start.ini + prod_keep_gate = 0     today's re-bid
#   on    postfix-start.ini + prod_keep_gate = 1     the completion bias
#
# The baseline arm is tuning-independent and is run ONCE; both deep arms
# are paired against it seed by seed, so the arm-vs-arm contrast is also
# paired and the baseline games cancel out of it exactly.
#
# TRAINING SEEDS, never holdout: this is an adjudication, not a report.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

BASE=${BASE:-$ROOT/.pin-bin/base/freeciv-server}
FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
CONFIG=${CONFIG:-$here/configs/contested.conf}
OUT=${OUT:-$here/results/prodkeep}
JOBS=${JOBS:-6}
NICE=${NICE:-10}
COUNT=${COUNT:-20}
SEEDS=${SEEDS:-$(head -n "$COUNT" "$here/seeds/train.txt")}

mkdir -p "$OUT"

# ONE DRIVER AT A TIME (STATUS 2026-08-23 §9: this box's process table is
# namespaced, so "is it still running?" cannot be answered by looking).
take_lock() {
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another prodkeep driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# One game, pruned as it finishes rather than at the end: contested T150
# with eight deep seats writes a large game.log and this box has ~14 GB.
one() { # seed arm port dir bin tune
  local seed=$1 arm=$2 port=$3 d=$4 bin=$5 tune=$6
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" nice -n "$NICE" "$here/run-game.sh" \
    --config "$CONFIG" --seed "$seed" --arm "$arm" --out "$d" \
    --server "$bin" --port "$port" >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-prodswitch|deep-prodkeep|deep-want |deep-runhold" \
         "$d/game.log" > "$d/game.log.new" 2>/dev/null
    mv -f "$d/game.log.new" "$d/game.log"
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

# PORTS: the pool must be far larger than JOBS, not equal to it. Paid
# for on 2026-08-25: an 8-port pool with 8 jobs put game i+8 on game i's
# port while game i was still alive, every bind after the first wave
# failed, and the baseline arm finished 10/20. The wrap now needs 30
# launches to come round, which cannot happen inside one game's life.
# Arms run sequentially (arm_run ends in `wait`), so they share the pool.
arm_run() { # dir arm tune port0 seeds...
  local dir=$1 arm=$2 tune=$3 port0=$4; shift 4
  local i=0 seed
  for seed in "$@"; do
    if [ ! -f "$dir/seed-$seed/DONE" ]; then
      one "$seed" "$arm" $((port0 + i % 30)) "$dir/seed-$seed" "$FIX" "$tune" &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    fi
  done
  wait
}

mark() { # dir want
  local dir=$1 want=$2 got
  got=$(find "$dir" -name DONE | wc -l)
  if [ "$got" -eq "$want" ]; then
    echo "SUCCESS $got/$want games $(date -Is)" > "$dir/DONE-BATCH"
  else
    echo "FAIL $got/$want games $(date -Is)" > "$dir/DONE-BATCH"
  fi
  cat "$dir/DONE-BATCH"
}

mktunes() {
  local src=$here/tuning/postfix-start.ini gate dest dup
  for gate in 0 1; do
    dest=$OUT/$([ "$gate" = 0 ] && echo off || echo on).ini
    grep -vE '^[[:space:]]*(prod_keep_gate|prod_keep_gain)' "$src" > "$dest"
    echo "prod_keep_gate = $gate" >> "$dest"
    # A duplicate key makes secfile_load refuse the WHOLE file and the
    # cell silently runs on compiled defaults (S-CAPSTAKE §8).
    dup=$(awk -F= '/^[a-z]/{gsub(/ /,"",$1); if(seen[$1]++) print "DUP:"$1}' \
          "$dest")
    [ -z "$dup" ] || { echo "STOP: duplicate keys in $dest: $dup" >&2; exit 1; }
  done
}

# ------------------------------------------------------------ identity
# prod_keep_gate = 0 must be the pre-epic binary byte for byte. Unknown
# keys are ignored by lookup, so the same ini loads on a binary that has
# never heard of the key.
do_identity() {
  local out=$OUT/identity same=0 diff=0 rc=0 side bin a b seed

  take_lock
  mkdir -p "$out"
  mktunes
  echo "base $(sha256sum "$BASE")"
  echo "fix  $(sha256sum "$FIX")"
  for side in base fix; do
    bin=$BASE
    [ "$side" = fix ] && bin=$FIX
    [ -f "$out/$side.done" ] && continue
    env TUNING="$OUT/off.ini" nice -n "$NICE" "$here/run.sh" --count 10 \
      --config "$here/configs/smoke.conf" --arms deep --jobs "$JOBS" \
      --portbase 7820 --out "$out/$side" --server "$bin" \
      > "$out/$side.log" 2>&1 && touch "$out/$side.done" || rc=1
  done
  for a in "$out/base"/*/deep/seed-*/scorelog.txt; do
    [ -f "$a" ] || continue
    seed=$(basename "$(dirname "$a")")
    b=$(echo "$out/fix"/*/deep/"$seed"/scorelog.txt)
    if [ -f "$b" ] && cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      diff=$((diff + 1)); echo "DIFF $seed"
    fi
  done
  echo "identity: $same identical, $diff different"
  # The proof must not be able to pass because both sides are the fix.
  if grep -rqs "deep-prodswitch.* keep=" "$out/base" 2>/dev/null; then
    echo "STOP: the base arm emitted keep= -- not the base." >&2
    rc=1
  fi
  if ! grep -rqs "deep-prodswitch.* keep=" "$out/fix" 2>/dev/null; then
    echo "STOP: the fix arm emitted no keep= -- not the fix." >&2
    rc=1
  fi
  if grep -rqs "deep-prodkeep " "$out/fix" 2>/dev/null; then
    echo "STOP: a credit was applied with the gate OFF." >&2
    rc=1
  fi
  if [ "$diff" -eq 0 ] && [ "$same" -gt 0 ] && [ "$rc" -eq 0 ]; then
    echo "SUCCESS identity $same $(date -Is)" > "$out/DONE-BATCH"
  else
    echo "FAIL identity $same/$diff $(date -Is)" > "$out/DONE-BATCH"
  fi
  cat "$out/DONE-BATCH"
  [ "$diff" -eq 0 ] && [ "$rc" -eq 0 ]
}

# ------------------------------------------------------------------ ab
do_ab() {
  local n
  take_lock
  mktunes
  n=$(echo $SEEDS | wc -w)
  echo "=== baseline ($n games) $(date -Is) ==="
  # shellcheck disable=SC2086
  arm_run "$OUT/baseline" baseline "$OUT/off.ini" 7800 $SEEDS
  mark "$OUT/baseline" "$n"
  echo "=== off ($n games) $(date -Is) ==="
  # shellcheck disable=SC2086
  arm_run "$OUT/off" deep "$OUT/off.ini" 7800 $SEEDS
  mark "$OUT/off" "$n"
  echo "=== on ($n games) $(date -Is) ==="
  # shellcheck disable=SC2086
  arm_run "$OUT/on" deep "$OUT/on.ini" 7800 $SEEDS
  mark "$OUT/on" "$n"
}

# ---------------------------------------------------------------- diag
# A DIAGNOSTIC ARM, not an adjudication. The census over the `on` arm
# says 82% of the part-built settlers that are abandoned are abandoned
# WITHOUT the completion bias ever reaching them (keep=0 with shields in
# the stock): the settler was not one of the two candidates in that
# city-turn's auction at all, which is the population the already-landed
# `founder_run_hold` gate (deep_founder_run_held(), commit 46d4cb9ca9)
# was written for and which is 0 in the standing config. This arm turns
# that gate on, alone, on the same seeds, to see whether the leak is at
# the SKIP seam rather than at the comparison seam. One key apart from
# `off`; read it as direction, not as a verdict on that gate.
do_diag() {
  local n dest=$OUT/hold.ini
  take_lock
  mktunes
  grep -vE '^[[:space:]]*founder_run_hold' "$OUT/off.ini" > "$dest"
  echo "founder_run_hold = 1" >> "$dest"
  n=$(echo $SEEDS | wc -w)
  echo "=== hold ($n games) $(date -Is) ==="
  # shellcheck disable=SC2086
  arm_run "$OUT/hold" deep "$dest" 7800 $SEEDS
  mark "$OUT/hold" "$n"
}

case "${1:-}" in
identity) do_identity;;
ab)       do_ab;;
diag)     do_diag;;
go)
  if ! do_identity; then
    echo "STOP: the gate is not inert at its default." >&2
    exit 1
  fi
  do_ab
  ;;
report)   "$here/prodkeep-report.py" "$OUT";;
census)   shift; "$here/prodkeep-report.py" --census "$@";;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
