#!/usr/bin/env bash
# THE POACH SEQUENCING FIX (S-POACHFIX): the proofs for the two gates.
#
#   setsid bash ./benchmark/poach-fix.sh go        identity, then the
#                                                  ladder, then the
#                                                  muster ledger cell
#   setsid bash ./benchmark/poach-fix.sh identity  10 smoke seeds x
#                                                  {deep, mixed_dc},
#                                                  pre-epic binary vs
#                                                  this branch's, both
#                                                  gates off, scorelogs
#                                                  byte-compared
#   setsid bash ./benchmark/poach-fix.sh rest      everything after the
#                                                  identity, queued
#                                                  behind its marker
#   setsid bash ./benchmark/poach-fix.sh ladder    the 3-arm nested
#                                                  ladder on pv3
#   setsid bash ./benchmark/poach-fix.sh warid     the same identity on
#                                                  the venue where the
#                                                  poach actually fires
#                                                  (needs the ladder)
#   setsid bash ./benchmark/poach-fix.sh ledger    the muster cell, for
#                                                  comparability with
#                                                  the published table
#   ./benchmark/poach-fix.sh report                every table
#
# Launch it from the repo root of THIS worktree, DETACHED. Resumable:
# a game whose directory carries DONE is skipped, so a driver killed at
# any point picks up where it stopped. Watch it by DONE counts; this
# box does not report its process table honestly.
#
# NICE. The user plays on this box. Every game process runs at nice 10
# (the driver itself does not); `nice` propagates to run-game.sh's
# server child, which is the process that actually burns the core.
#
# THE LADDER is nested, not factorial, because the two gates are on one
# causal chain: gate 2 prices a claim gate 1 decides whether to spend.
# Reading them from two separate A/Bs would attribute to the pricing
# what the sequencing already did.
#
#   off      pv3.ini                                the inherited order
#   deploy   pv3.ini + poach_deploy_gate = 1        commit 1
#   marg     deploy + poach_marginal_gate = 1       commit 2
#
# THE LEDGER CELL is the S-POACHPRICE ON-arm shape -- tech50war-200 on
# pv3-muster.ini -- one key apart, so the "76.0% paid for nothing"
# table has a directly comparable after.
#
# IDENTITY. Both gates off must be the pre-epic binary byte for byte.
# The pre-epic pin is .pin-bin/base (built from the branch point before
# a line of this epic landed) and the shipped pin is .pin-bin/fix.
#
# Machine budget: ports 7300-7349 and 7400-7449, jobs <= 3, this
# worktree's own build/ and its own pinned binaries under .pin-bin/.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

BASE=${BASE:-$ROOT/.pin-bin/base/freeciv-server}
FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
CONFIG=$here/configs/tech50war-200.conf
OUT=$here/results/poachfix
JOBS=${JOBS:-3}
NICE=${NICE:-10}
LADDER_SEEDS=${LADDER_SEEDS:-$(seq 17 32)}
LEDGER_SEEDS=${LEDGER_SEEDS:-$(seq 17 26)}

[ -x "$BASE" ] || { echo "no base server at $BASE" >&2; exit 1; }
[ -x "$FIX" ]  || { echo "no fix server at $FIX" >&2; exit 1; }

# ------------------------------------------------------------- tunings
# One arm ini = one archived tuning plus the gate keys, written by
# STRIPPING the key first and then setting it. A plain append produced a
# duplicate key once already (S-CAPSTAKE §8): secfile_load refuses the
# WHOLE file, and the cell silently runs on compiled defaults with no
# tuning and no gate. The awk dedupe below must print nothing, and it
# is checked rather than merely printed.
mktune() { # dest base deploy marginal
  local dest=$1 src=$2 dep=$3 marg=$4 dup
  { grep -vE '^[[:space:]]*(poach_deploy_gate|poach_marginal_gate)' "$src"
    echo "poach_deploy_gate = $dep"
    echo "poach_marginal_gate = $marg"
  } > "$dest"
  dup=$(awk -F= '/^[a-z]/{gsub(/ /,"",$1); if(seen[$1]++){print "DUP:"$1}}' \
        "$dest")
  if [ -n "$dup" ]; then
    echo "STOP: duplicate keys in $dest: $dup" >&2
    return 1
  fi
  return 0
}

# --------------------------------------------------------------- games
# One game, pruned to its ledger lines as it finishes rather than at the
# end -- tech50war-200 writes 31 MB a game and this box has 17 GB. A
# partial directory from an earlier kill is deleted rather than resumed:
# a leftover scorelog.txt silently disables scorelogging for the whole
# game (STATUS 2026-08-13).
one() {
  local seed=$1 arm=$2 port=$3 d=$4 bin=$5 tune=$6
  rm -rf "$d"; mkdir -p "$d"
  env FREECIV_KILL_DYAD=1 TUNING="$tune" \
    nice -n "$NICE" "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
    --arm "$arm" --out "$d" --server "$bin" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-employ|deep-poachmarg|deep-op |deep-writeoff|kill-dyad" \
         "$d/game.log" > "$d/game.log.new"
    mv "$d/game.log.new" "$d/game.log"
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

# A whole arm, resumable, at most JOBS games at once.
arm_run() { # dir tune port0 seatarms seeds...
  local dir=$1 tune=$2 port0=$3 seats=$4; shift 4
  local i=0 seed arm
  for seed in "$@"; do
    for arm in $seats; do
      [ -f "$dir/$arm/seed-$seed/DONE" ] && continue
      one "$seed" "$arm" $((port0 + i % 10)) "$dir/$arm/seed-$seed" \
          "$FIX" "$tune" &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    done
  done
  wait
}

# ONE DRIVER AT A TIME. Paid for on 2026-08-23: a second instance was
# launched while the first was still alive (this box's process table is
# namespaced, so `pgrep` and `pkill` see nothing and "is it still
# running?" cannot be answered by looking). The two raced on the same
# output dirs and the same port block, three games FAILED on a bound
# port, and one arm's log carried two completion summaries. Every stage
# that runs games takes this lock first; `report` does not.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another poach-fix driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# SUCCESS vs FAIL, not mere completion: a marker that only says "the
# driver reached the end" is the one that lies.
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

# ------------------------------------------------------------ identity
do_identity() {
  local out=$OUT/identity rc=0 same=0 diff=0

  take_lock
  mkdir -p "$out"
  echo "base $(sha256sum "$BASE")"
  echo "fix  $(sha256sum "$FIX")"
  for side in base fix; do
    local bin=$BASE
    [ "$side" = fix ] && bin=$FIX
    for arm in deep mixed_dc; do
      [ -f "$out/$side-$arm.done" ] && continue
      nice -n "$NICE" "$here/run.sh" --count 10 \
        --config "$here/configs/smoke.conf" --arms "$arm" --jobs "$JOBS" \
        --portbase 7400 --out "$out/$side-$arm" --server "$bin" \
        > "$out/$side-$arm.log" 2>&1 && touch "$out/$side-$arm.done" || rc=1
    done
  done
  for arm in deep mixed_dc; do
    for a in "$out/base-$arm"/*/"$arm"/seed-*/scorelog.txt; do
      [ -f "$a" ] || continue
      local seed b
      seed=$(basename "$(dirname "$a")")
      b=$(echo "$out/fix-$arm"/*/"$arm"/"$seed"/scorelog.txt)
      if [ -f "$b" ] && cmp -s "$a" "$b"; then
        same=$((same + 1))
      else
        diff=$((diff + 1)); echo "DIFF $arm $seed"
      fi
    done
  done
  echo "identity: $same identical, $diff different"
  # The proof must not be able to pass because both sides are the fix:
  # the base binary cannot emit took= on its poach ledger line.
  if grep -rqs "deep-employpoach.*took=" "$out/base-deep" 2>/dev/null; then
    echo "STOP: the base arm emitted took= -- not the base." >&2
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

# -------------------------------------------------------------- ladder
do_ladder() {
  local n

  take_lock
  n=$(echo $LADDER_SEEDS | wc -w)
  mkdir -p "$OUT/ladder"
  mktune "$OUT/ladder/off.ini"    "$here/tuning/pv3.ini" 0 0 || return 1
  mktune "$OUT/ladder/deploy.ini" "$here/tuning/pv3.ini" 1 0 || return 1
  mktune "$OUT/ladder/marg.ini"   "$here/tuning/pv3.ini" 1 1 || return 1
  local i=0 rung seeds
  for rung in ${RUNGS:-off deploy}; do
    seeds=$LADDER_SEEDS
    # The third rung is the pricing commit's own arm and it rides the
    # ladder's seeds so the three are paired; MARG_SEEDS lets it be a
    # prefix of them when the box cannot afford the full width.
    [ "$rung" = marg ] && seeds=${MARG_SEEDS:-$LADDER_SEEDS}
    n=$(echo $seeds | wc -w)
    echo "=== ladder/$rung ($n games) $(date -Is) ==="
    # shellcheck disable=SC2086
    arm_run "$OUT/ladder/$rung" "$OUT/ladder/$rung.ini" \
            $((7300 + i * 10)) mixed_dc $seeds
    mark "$OUT/ladder/$rung" "$n"
    i=$((i + 1))
  done
}

# --------------------------------------------------------- war identity
# The smoke identity above is the house standard and it is WEAK for
# this epic: smoke is 4 players over 100 turns and the poach may never
# fire there, so "identical" can mean "the mechanism never ran". This
# runs the ladder's OFF ini on the PRE-EPIC binary, on the venue where
# the poach fires 60+ times a game, and byte-compares against the
# ladder's own off arm. Unknown keys are ignored by lookup, so the same
# ini loads on a binary that has never heard of either gate.
do_warid() {
  local out=$OUT/warid seeds=${WARID_SEEDS:-$(seq 17 20)}
  local same=0 diff=0 seed i=0

  take_lock
  mkdir -p "$out"
  [ -f "$OUT/ladder/off.ini" ] || { echo "run the ladder first" >&2; return 1; }
  for seed in $seeds; do
    [ -f "$out/mixed_dc/seed-$seed/DONE" ] && continue
    ( seed=$seed
      rm -rf "$out/mixed_dc/seed-$seed"; mkdir -p "$out/mixed_dc/seed-$seed"
      env FREECIV_KILL_DYAD=1 TUNING="$OUT/ladder/off.ini" \
        nice -n "$NICE" "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
        --arm mixed_dc --out "$out/mixed_dc/seed-$seed" --server "$BASE" \
        --port $((7340 + i % 6)) >/dev/null 2>&1
      rm -f "$out/mixed_dc/seed-$seed"/saves/*.sav* ) &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
  wait
  for seed in $seeds; do
    local a=$out/mixed_dc/seed-$seed/scorelog.txt
    local b=$OUT/ladder/off/mixed_dc/seed-$seed/scorelog.txt
    if [ -r "$a" ] && [ -r "$b" ] && cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      diff=$((diff + 1)); echo "WAR-DIFF seed $seed"
    fi
  done
  echo "war identity: $same identical, $diff different"
  # And the poach must actually have fired, or the proof is vacuous.
  echo "poach claims on the base side:" \
       "$(grep -rhc 'deep-employpoach ' "$out" 2>/dev/null | paste -sd+ \
          | bc 2>/dev/null || echo '?')"
  if [ "$diff" -eq 0 ] && [ "$same" -gt 0 ]; then
    echo "SUCCESS warid $same $(date -Is)" > "$out/DONE-BATCH"
  else
    echo "FAIL warid $same/$diff $(date -Is)" > "$out/DONE-BATCH"
  fi
  cat "$out/DONE-BATCH"
}

# -------------------------------------------------------------- ledger
do_ledger() {
  local n

  take_lock
  n=$(echo $LEDGER_SEEDS | wc -w)
  mkdir -p "$OUT/ledger"
  mktune "$OUT/ledger/off.ini"    "$here/tuning/pv3-muster.ini" 0 0 || return 1
  mktune "$OUT/ledger/deploy.ini" "$here/tuning/pv3-muster.ini" 1 0 || return 1
  local i=0 rung
  for rung in off deploy; do
    echo "=== ledger/$rung ($n games) $(date -Is) ==="
    # shellcheck disable=SC2086
    arm_run "$OUT/ledger/$rung" "$OUT/ledger/$rung.ini" \
            $((7330 + i * 10)) mixed_dc $LEDGER_SEEDS
    mark "$OUT/ledger/$rung" "$n"
    i=$((i + 1))
  done
}

# -------------------------------------------------------------- report
do_report() {
  local cell rung
  for cell in ladder ledger; do
    for rung in off deploy marg; do
      [ -d "$OUT/$cell/$rung" ] || continue
      echo "############ $cell / $rung"
      "$here/employ.py" "$OUT/$cell/$rung" --section poach
      "$here/employ.py" "$OUT/$cell/$rung" --section depart
      "$here/employ.py" "$OUT/$cell/$rung" --section ops
    done
  done
  # paircmp wants a metrics.csv at each root; extract.py writes it from
  # the scorelogs the games already carry.
  for rung in off deploy marg; do
    [ -d "$OUT/ladder/$rung" ] || continue
    "$here/extract.py" --root "$OUT/ladder/$rung" \
      --csv "$OUT/ladder/$rung/metrics.csv" > /dev/null 2>&1
  done
  for rung in deploy marg; do
    [ -f "$OUT/ladder/$rung/metrics.csv" ] || continue
    echo "############ score: off -> $rung (paired, mixed_dc)"
    "$here/paircmp.py" "$OUT/ladder/off" "$OUT/ladder/$rung" --arm mixed_dc
  done
}

case "${1:-}" in
identity) do_identity;;
ladder)   do_ladder;;
rest)
  # Everything after the identity, queued behind it. The identity is
  # 40 smoke games and the ladder wants the same three job slots, so
  # this waits on the identity's own marker rather than oversubscribing
  # a box the user is playing on.
  until [ -f "$OUT/identity/DONE-BATCH" ]; do sleep 60; done
  grep -q SUCCESS "$OUT/identity/DONE-BATCH" \
    || { echo "STOP: identity did not succeed." >&2; exit 1; }
  # STAGES orders the queue and lets a loaded box drop the tail: the
  # ladder's own off/deploy rungs already carry the ledger before/after,
  # so the muster cell is the first thing worth cutting.
  for stage in ${STAGES:-ladder warid marg ledger}; do
    case $stage in
      ladder) RUNGS="off deploy" do_ladder;;
      warid)  do_warid;;
      marg)   RUNGS=marg do_ladder;;
      ledger) do_ledger;;
    esac
  done
  do_report > "$OUT/report.txt" 2>&1
  echo "report at $OUT/report.txt"
  ;;
warid)    do_warid;;
ledger)   do_ledger;;
report)   do_report;;
go)
  if ! do_identity; then
    echo "STOP: the gates are not inert at their defaults." >&2
    exit 1
  fi
  RUNGS="off deploy" do_ladder
  do_warid
  do_ledger
  do_report > "$OUT/report.txt" 2>&1
  echo "report at $OUT/report.txt"
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
