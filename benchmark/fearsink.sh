#!/usr/bin/env bash
# S-FEARSINK: the pre-registered 2x2 of S-FEARSEAT x S-SITEPLAN.
#
#   setsid bash ./benchmark/fearsink.sh go        identity, both venues,
#                                                 then the report
#   setsid bash ./benchmark/fearsink.sh identity  3 contested holdout
#                                                 seeds at compiled
#                                                 defaults, scorelogs
#                                                 byte-compared with the
#                                                 ARCHIVED s6-nogates-war
#                                                 games (`id` excluded)
#   setsid bash ./benchmark/fearsink.sh war       the five cells on
#                                                 contested, TRAINING
#                                                 seeds
#   setsid bash ./benchmark/fearsink.sh econ      the same five on
#                                                 lategame-80
#   ./benchmark/fearsink.sh report                the tables
#
# Launch from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped. Watch it by DONE
# counts; this box does not report its process table honestly.
#
# WHY THIS CELL EXISTS. Both parent epics were adjudicated on a base
# WITHOUT the other's gates, and both act on what the T50-75 founder
# queue produces:
#
#   findings/fearseat-verdict-2026-08-30.md S4 -- mil_opportunity_gate
#     frees ~8 shields/city of T1-25 military at no combat cost
#     (unitslost flat to its SE in both venues) and converts it to
#     +0.188 +/- 0.064 cities at T50 (t +2.9) which is GONE by T75.
#     "The remaining defect is downstream, in the settler pipeline."
#   findings/siteplan-verdict-2026-08-29.md -- founder_site_cap_gate
#     + founder_join_guard_gate take joins from 24.3/game to 0.03,
#     hold the hoard to ~10 founders against baseline's 19-40, move
#     the war city curve not at all, and close a third of the econ
#     deficit (+0.37 cities/seat at T60, t +4.7).
#
# THE HYPOTHESIS, stated before the run: the shields the price frees
# become settlers that the join then eats. With both gates on, the T50
# city gain should PERSIST to T75 and beyond, and the score should
# move. If it does not, the two mechanisms are independent and the
# missing cities are lost to a third thing.
#
# WHAT A NULL LOOKS LIKE, also stated first: c11 - c00 no larger than
# c10 - c00 plus c01 - c00, with the interaction term inside its SE.
# The 2x2 is the only design that can tell those apart -- two
# concurrent single-factor arms cannot, which is exactly why the
# siteplan verdict's war-venue c11 read as a failure of gates that
# were not failing.
#
# CONSTANTS ARE NOT RETUNED HERE. mil_opportunity_price 1.0,
# founder_site_cap_slack 1.0, founder_join_guard_mult 1.0 are all
# PLACEHOLDER; the cell tests the INTERACTION at the placeholders. A
# sweep on top of an unadjudicated interaction would be unreadable.
#
# THE FIFTH ARM. boat_shadow_gate alone, the fear verdict's secondary
# read: it deletes 81.2% of a geography-blind flat term's firings at a
# measured score cost of +0.00 +/- 0.74 with unitslost DOWN in both
# venues. Outside the factorial (coastal, fires from T51, and it
# touches neither seam the 2x2 is about).
#
# SEEDS ARE TRAINING (1-20), the same 20 and the same two venues both
# parent epics used. holdout.txt is touched ONLY by `identity`, which
# reads no score.
#
# NO CACHE REUSE. Both parents pinned a different binary
# (bc40c9a4540ea1a9 and 14c7c62ecf3acd97) than this merge does, so
# every off-arm game is re-run rather than borrowed: run.sh's cache is
# keyed on the binary sha for exactly this reason.
#
# NICE. The user plays on this box and two other campaigns hold most
# of it. Every game runs at nice 10, jobs 3, ports 7960-7999, and this
# worktree's own pinned binary.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=${OUT:-$ROOT/benchmark/results/fearsink}
JOBS=${JOBS:-3}
NICE=${NICE:-10}
SEEDS=${SEEDS:-$(seq 1 20)}
CELLS=${CELLS:-c00 c10 c01 c11 boat}
ARCHIVE=/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0

# The census lines the tables are read on. Everything else in a
# contested T150 game.log is hundreds of megabytes we do not have --
# the disk is at 93%.
KEEP='deep-tuning|deep-prodswitch |deep-expand |deep-join |deep-joinguard |deep-sitecap |deep-milopp '

[ -x "$FIX" ] || { echo "no pinned server at $FIX" >&2; exit 1; }

ini_for() { echo "$here/tuning/fearsink-$1.ini"; }

# ONE DRIVER AT A TIME. The sandbox namespaces the process table, so
# "is it still running?" cannot be answered by looking, and two drivers
# on one output dir and one port block cost a whole batch on
# 2026-08-23.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another fearsink driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# One game, pruned to its census lines as it finishes rather than at the
# end. A partial directory from an earlier kill is deleted rather than
# resumed: a leftover scorelog.txt silently disables scorelogging for
# the whole game (STATUS 2026-08-13).
one() { # seed port dir tune config
  local seed=$1 port=$2 d=$3 tune=$4 cfg=$5
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" nice -n "$NICE" "$here/run-game.sh" \
    --config "$cfg" --seed "$seed" --arm deep --out "$d" \
    --server "$FIX" --port "$port" >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "$KEEP" "$d/game.log" > "$d/game.log.new"
    mv "$d/game.log.new" "$d/game.log"
    zstd -q --rm "$d/game.log"
    rm -rf "$d/saves"
    touch "$d/.filtered"
  fi
}

arm_run() { # dir tune config port0 seeds...
  local dir=$1 tune=$2 cfg=$3 port0=$4; shift 4
  local i=0 seed
  for seed in "$@"; do
    if [ -f "$dir/deep/seed-$seed/DONE" ]; then continue; fi
    one "$seed" $((port0 + i % 8)) "$dir/deep/seed-$seed" "$tune" "$cfg" &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
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

venue() { # name config port0
  local name=$1 cfg=$2 port0=$3 c n i=0
  take_lock
  n=$(echo $SEEDS | wc -w)
  for c in $CELLS; do
    echo "=== $name/$c ($n games) $(date -Is) ==="
    # shellcheck disable=SC2086
    arm_run "$OUT/$name/$c" "$(ini_for "$c")" "$cfg" \
            $((port0 + i * 8)) $SEEDS
    mark "$OUT/$name/$c" "$n"
    i=$(((i + 1) % 4))
  done
  echo "=== $name COMPLETE $(date -Is) ==="
}

# ------------------------------------------------------------ identity
# This epic adds no mechanism -- it is the 2x2 of two epics that are
# each already inert at their defaults -- so the MERGE of the two must
# be inert too, and that is what this proves. Byte comparison of the
# scorelog against the archived s6-nogates-war games, `id` excluded (a
# per-run random token).
do_identity() {
  local out=$OUT/identity seed same=0 diff=0 i=0 a b

  take_lock
  mkdir -p "$out"
  echo "fix $(sha256sum "$FIX")"
  printf '[tuning]\n' > "$here/tuning/defaults.ini"
  for seed in 101 102 103; do
    [ -f "$out/deep/seed-$seed/DONE" ] && continue
    one "$seed" $((7996 + i)) "$out/deep/seed-$seed" \
        "$here/tuning/defaults.ini" "$here/configs/contested.conf" &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
  wait
  for seed in 101 102 103; do
    a=$ARCHIVE/seed-$seed/scorelog.txt
    b=$out/deep/seed-$seed/scorelog.txt
    if [ -r "$a" ] && [ -r "$b" ] \
       && diff -q <(grep -v '^id ' "$a") <(grep -v '^id ' "$b") >/dev/null
    then
      same=$((same + 1)); echo "SAME seed $seed"
    else
      diff=$((diff + 1)); echo "DIFF seed $seed"
    fi
  done
  echo "identity: $same identical, $diff different"
  # The proof must not be able to pass vacuously: the pinned binary has
  # to be the one carrying BOTH epics' census lines, and neither gate
  # may have fired. grep -c, not grep -q: with pipefail an early-exiting
  # grep -q kills zstdcat with SIGPIPE and the pipeline reports 141,
  # which reads as "absent" when the string is present.
  local log=$out/deep/seed-101/game.log.zst
  local want miss=0
  for want in 'deep-joinguard ' 'deep-sitecap '; do
    if [ "$(zstdcat "$log" 2>/dev/null | grep -ac "$want")" = 0 ]; then
      echo "STOP: the pinned binary does not emit $want" >&2
      miss=1
    fi
  done
  for want in 'deep-joinguard .* held=1 gate=1' 'deep-sitecap .* capped=1 gate=1' 'deep-milopp '; do
    if [ "$(zstdcat "$log" 2>/dev/null | grep -ac "$want")" != 0 ]; then
      echo "STOP: a gate fired at compiled defaults: $want" >&2
      miss=1
    fi
  done
  diff=$((diff + miss))
  if [ "$same" -eq 3 ] && [ "$diff" -eq 0 ]; then
    echo "SUCCESS identity $(date -Is)" > "$out/DONE-BATCH"
  else
    echo "FAIL identity $same/$diff $(date -Is)" > "$out/DONE-BATCH"
  fi
  cat "$out/DONE-BATCH"
  [ "$diff" -eq 0 ]
}

do_report() {
  local v c
  for v in war econ; do
    [ -d "$OUT/$v" ] || continue
    echo "################################ venue $v"
    "$here/fearsink.py" "$OUT/$v"
    for c in $CELLS; do
      [ "$c" = c00 ] && continue
      [ -d "$OUT/$v/$c" ] || continue
      echo "#### paircmp c00 -> $c ($v)"
      "$here/paircmp.py" "$OUT/$v/c00" "$OUT/$v/$c" --arm deep
    done
  done
}

case "${1:-}" in
identity) do_identity;;
war)      venue war  "$here/configs/contested.conf"   7960;;
econ)     venue econ "$here/configs/lategame-80.conf" 7960;;
report)   do_report;;
go)
  if ! do_identity; then
    echo "STOP: the merge is not inert at its defaults." >&2
    exit 1
  fi
  venue war  "$here/configs/contested.conf"   7960
  venue econ "$here/configs/lategame-80.conf" 7960
  do_report > "$OUT/report.txt" 2>&1
  echo "report at $OUT/report.txt"
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
