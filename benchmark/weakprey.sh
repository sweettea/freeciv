#!/usr/bin/env bash
# THE WEAK-PREY GATE (S-WEAKPREY): the proof for weak_prey_gate.
#
#   setsid bash ./benchmark/weakprey.sh go        identity, then the
#                                                 A/B, then the report
#   setsid bash ./benchmark/weakprey.sh identity  10 smoke seeds x
#                                                 {deep, mixed_dc},
#                                                 pre-epic binary vs
#                                                 this branch's, gate
#                                                 off, scorelogs
#                                                 byte-compared
#   setsid bash ./benchmark/weakprey.sh ab        the paired A/B on
#                                                 contested, TRAINING
#                                                 seeds, arm deep
#   setsid bash ./benchmark/weakprey.sh warid     the same identity on
#                                                 contested, where the
#                                                 operations layer
#                                                 actually runs
#   ./benchmark/weakprey.sh report                the tables
#
# Launch it from the repo root of THIS worktree, DETACHED. Resumable:
# a game whose directory carries DONE is skipped. Watch it by DONE
# counts; this box does not report its process table honestly.
#
# THE HYPOTHESIS, and it is not "score goes up". The 2026-08-25 log
# census found deep at ~2x baseline's military with only ~1.06x its
# kills -- it arms and does not convert. The gate makes a militarily
# weaker rival's cities worth more, at the shared conquest worth and at
# both target-ranking loops. The predicted signature is therefore
# UNITSKILLED first and score second, and paircmp.py reports both off
# the same scorelogs.
#
# SEEDS ARE TRAINING (1-16). This is an adjudication, not a report:
# benchmark/seeds/holdout.txt (101-200) is not touched here and must
# not be, or the eventual holdout number means nothing.
#
# VENUE. contested.conf, because that is the venue the deficit census
# measured the 2x-military / 1.06x-kills gap on and the venue the
# defaults-vs-baseline holdout used. tech50war-200 concludes wars
# better and is the natural follow-up cell; it is not this proof.
#
# IDENTITY. Gate off must be the pre-epic binary byte for byte. The
# pre-epic pin is .pin-bin/base (built from the branch point before a
# line of this epic landed) and the shipped pin is .pin-bin/fix. The
# smoke identity is the house standard and it is WEAK here -- smoke is
# 4 players over 100 turns and the operations layer may barely run --
# so `warid` repeats it on contested, where deep-opodds fires.
#
# NICE. The user plays on this box. Every game process runs at nice 10.
#
# Machine budget: ports 7850-7899, jobs <= 3, this worktree's own
# build/ and its own pinned binaries under .pin-bin/.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

BASE=${BASE:-$ROOT/.pin-bin/base/freeciv-server}
FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
CONFIG=$here/configs/contested.conf
OUT=$here/results/weakprey
JOBS=${JOBS:-3}
NICE=${NICE:-10}
AB_SEEDS=${AB_SEEDS:-$(seq 1 16)}

[ -x "$BASE" ] || { echo "no base server at $BASE" >&2; exit 1; }
[ -x "$FIX" ]  || { echo "no fix server at $FIX" >&2; exit 1; }

# ------------------------------------------------------------- tunings
# Two ARCHIVED inis, one key apart, the capstake pattern: the off arm's
# provenance is in a file rather than in the absence of one, and both
# arms therefore load a tuning file so "a file was loaded" is not a
# second difference between them. Nothing is generated at run time, so
# there is no append that can produce the duplicate key that made a
# whole cell silently run on compiled defaults once (S-CAPSTAKE 8).
OFF_INI=$here/tuning/weakprey-off.ini
ON_INI=$here/tuning/weakprey-on.ini

# --------------------------------------------------------------- games
# One game, pruned to its census lines as it finishes rather than at the
# end. A partial directory from an earlier kill is deleted rather than
# resumed: a leftover scorelog.txt silently disables scorelogging for
# the whole game (STATUS 2026-08-13).
one() {
  local seed=$1 arm=$2 port=$3 d=$4 bin=$5 tune=$6
  rm -rf "$d"; mkdir -p "$d"
  env FREECIV_KILL_DYAD=1 TUNING="$tune" \
    nice -n "$NICE" "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
    --arm "$arm" --out "$d" --server "$bin" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-weakprey|deep-opodds|deep-op |deep-anchor|kill-dyad" \
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

# ONE DRIVER AT A TIME. This box's process table is namespaced, so
# `pgrep` and `pkill` see nothing and "is it still running?" cannot be
# answered by looking; two drivers racing on one output dir and one
# port block cost a whole batch on 2026-08-23. Every stage that runs
# games takes this lock first; `report` does not.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another weakprey driver holds $OUT/.driver.lock" >&2
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
        --portbase 7880 --out "$out/$side-$arm" --server "$bin" \
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
  # the base binary cannot emit prey= on its opodds line.
  if grep -rqs "deep-opodds.*prey=" "$out/base-deep" 2>/dev/null; then
    echo "STOP: the base arm emitted prey= -- not the base." >&2
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

# ------------------------------------------------------------------ AB
do_ab() {
  local n i=0 rung

  take_lock
  n=$(echo $AB_SEEDS | wc -w)
  mkdir -p "$OUT/ab"
  for rung in ${RUNGS:-off on}; do
    local ini=$OFF_INI
    [ "$rung" = on ] && ini=$ON_INI
    echo "=== ab/$rung ($n games, $(basename "$ini")) $(date -Is) ==="
    # shellcheck disable=SC2086
    arm_run "$OUT/ab/$rung" "$ini" \
            $((7850 + i * 10)) deep $AB_SEEDS
    mark "$OUT/ab/$rung" "$n"
    i=$((i + 1))
  done
}

# --------------------------------------------------------- war identity
# The smoke identity is the house standard and it is WEAK for this
# epic: smoke is 4 players over 100 turns and the operations layer may
# barely run there, so "identical" can mean "the mechanism never ran".
# This runs the A/B's OFF ini on the PRE-EPIC binary, on contested,
# and byte-compares against the A/B's own off arm. Unknown keys are
# ignored by lookup, so the same ini loads on a binary that has never
# heard of the gate.
do_warid() {
  local out=$OUT/warid seeds=${WARID_SEEDS:-$(seq 1 4)}
  local same=0 diff=0 seed i=0

  take_lock
  mkdir -p "$out"
  [ -d "$OUT/ab/off" ] || { echo "run the ab first" >&2; return 1; }
  for seed in $seeds; do
    [ -f "$out/deep/seed-$seed/DONE" ] && continue
    ( seed=$seed
      rm -rf "$out/deep/seed-$seed"; mkdir -p "$out/deep/seed-$seed"
      env FREECIV_KILL_DYAD=1 TUNING="$OFF_INI" \
        nice -n "$NICE" "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
        --arm deep --out "$out/deep/seed-$seed" --server "$BASE" \
        --port $((7870 + i % 6)) >/dev/null 2>&1
      rm -f "$out/deep/seed-$seed"/saves/*.sav* ) &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
  wait
  for seed in $seeds; do
    local a=$out/deep/seed-$seed/scorelog.txt
    local b=$OUT/ab/off/deep/seed-$seed/scorelog.txt
    if [ -r "$a" ] && [ -r "$b" ] && cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      diff=$((diff + 1)); echo "WAR-DIFF seed $seed"
    fi
  done
  echo "war identity: $same identical, $diff different"
  # And the operations layer must actually have run, or the proof is
  # vacuous.
  echo "formation attempts on the base side:" \
       "$(grep -rhc 'deep-opodds ' "$out" 2>/dev/null | paste -sd+ \
          | bc 2>/dev/null || echo '?')"
  if [ "$diff" -eq 0 ] && [ "$same" -gt 0 ]; then
    echo "SUCCESS warid $same $(date -Is)" > "$out/DONE-BATCH"
  else
    echo "FAIL warid $same/$diff $(date -Is)" > "$out/DONE-BATCH"
  fi
  cat "$out/DONE-BATCH"
}

# -------------------------------------------------------------- report
do_report() {
  local rung
  for rung in off on; do
    [ -d "$OUT/ab/$rung" ] || continue
    "$here/extract.py" --root "$OUT/ab/$rung" \
      --csv "$OUT/ab/$rung/metrics.csv" > /dev/null 2>&1
  done
  if [ -f "$OUT/ab/on/metrics.csv" ]; then
    echo "############ score+kills: off -> on (paired, arm deep)"
    "$here/paircmp.py" "$OUT/ab/off" "$OUT/ab/on" --arm deep
  fi
  # The targeting census: did the factor actually fire, and did the
  # CHOSEN target's prey factor move? prey= is on every deep-opodds
  # line in both arms (it is a pure read, so the off arm reports the
  # counterfactual 1.000 it acted on).
  for rung in off on; do
    [ -d "$OUT/ab/$rung" ] || continue
    echo "############ weak-prey census / $rung"
    "$here/weakprey.py" "$OUT/ab/$rung"
  done
}

case "${1:-}" in
identity) do_identity;;
ab)       do_ab;;
warid)    do_warid;;
report)   do_report;;
go)
  if ! do_identity; then
    echo "STOP: the gate is not inert at its default." >&2
    exit 1
  fi
  do_ab
  do_warid
  do_report > "$OUT/report.txt" 2>&1
  echo "report at $OUT/report.txt"
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
