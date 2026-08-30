#!/usr/bin/env bash
# S-FRONTIER: price the closed frontier, and the 2x2 it earns.
#
#   setsid bash ./benchmark/frontier.sh identity   3 contested holdout
#                                                  seeds at compiled
#                                                  defaults, scorelogs
#                                                  byte-compared with
#                                                  the ARCHIVED
#                                                  s6-nogates-war games
#                                                  (`id` excluded), and
#                                                  the wall census read
#                                                  off the same logs
#   setsid bash ./benchmark/frontier.sh war        the 2x2 on contested,
#                                                  TRAINING seeds
#   setsid bash ./benchmark/frontier.sh econ       the same on
#                                                  lategame-80
#   ./benchmark/frontier.sh report                 the tables
#
# Launch from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped. Watch it by DONE
# counts; this box does not report its process table honestly.
#
# WHY THIS CELL EXISTS. findings/city-ledger-2026-08-30.md S8 asked for
# a 2x2 of "price the frontier" against "floor the founder want". Its
# first commit had to establish legality, and legality killed the
# frontier-theft axis outright: no benchmarked ruleset permits founding
# inside a foreign border (scripts/check-found-city-foreign.sh), the
# `offpeace` bucket the census sized is an ALLY's border rather than a
# peace partner's, and `offlegal` on deep-siteladder is 0 in play.
#
# What survives is one level further out and is deep's own policy: the
# site search's pathfinder TB_IGNOREs every non-allied owner's tile
# (deepsettler.c deep_settler_tile_behavior), so free land BEHIND a
# peace partner's border is invisible to it, while stock's phase-1
# search sets no get_TB at all. deep-sitewall is the counterfactual
# census of what that wall hides, and frontier_transit_gate is the
# lever that takes it down.
#
# THE 2x2, stated before the run:
#
#   c00   neither                                  the base
#   c10   frontier_transit_gate                    the wall comes down
#   c01   founder_floor_gate                       an undoable empty
#                                                  search does not zero
#                                                  the founder want
#   c11   both                                     the corner
#
# Both act on the SAME T51-75 founder queue -- the transit gate by
# giving an empty search somewhere to find, the floor gate by keeping
# the founder in the build queue when it finds nothing -- so per the
# S-FEARSINK lesson they want one factorial and not two concurrent
# arms. The interaction is taken PER SEED, because the four cells play
# the same map.
#
# WHAT A NULL LOOKS LIKE, also stated first: c11 - c00 no larger than
# (c10 - c00) + (c01 - c00), with the interaction inside its SE, and
# no cell clearing its own SE by a reasonable margin in BOTH venues.
#
# CONSTANTS ARE NOT SWEPT HERE. frontier_transit_price 1.0,
# frontier_transit_stance_floor 2, founder_floor_want 1.0,
# founder_floor_reach_ref 1.0 are all PLACEHOLDER; the cell tests the
# interaction at the placeholders.
#
# SEEDS ARE TRAINING (1-20). holdout.txt is touched ONLY by `identity`,
# which reads no score.
#
# NICE. The user plays on this box and a screen campaign holds six
# cores for days. Every game runs at nice 10, jobs 3, ports 7960-7999,
# and this worktree's own pinned binary.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=${OUT:-$ROOT/benchmark/results/frontier}
JOBS=${JOBS:-3}
NICE=${NICE:-10}
SEEDS=${SEEDS:-$(seq 1 20)}
CELLS=${CELLS:-c00 c10 c01 c11}
ARCHIVE=/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0

# The census lines the tables are read on. Everything else in a
# contested T150 game.log is hundreds of megabytes we do not have --
# the disk is at 94%.
KEEP='deep-tuning|deep-expand |deep-siteladder |deep-sitewall |deep-city |deep-transit '

[ -x "$FIX" ] || { echo "no pinned server at $FIX" >&2; exit 1; }

ini_for() { echo "$here/tuning/frontier-$1.ini"; }

# ONE DRIVER AT A TIME. The sandbox namespaces the process table, so
# "is it still running?" cannot be answered by looking, and two drivers
# on one output dir and one port block cost a whole batch on
# 2026-08-23.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another frontier driver holds $OUT/.driver.lock" >&2
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
  TUNING="$tune" nice -n "$NICE" "$here/run-game.sh" \
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
# Both gates are off at compiled defaults and the two censuses this
# epic adds (deep-siteladder's `offlegal`, deep-sitewall) take no
# decision, so the binary must reproduce the archived s6-nogates-war
# games byte for byte. Scorelog comparison, `id` excluded (a per-run
# random token).
do_identity() {
  local out=$OUT/identity seed same=0 diff=0 i=0 a b

  take_lock
  mkdir -p "$out"
  echo "fix $(sha256sum "$FIX")"
  printf '[tuning]\n' > "$here/tuning/frontier-defaults.ini"
  for seed in 101 102 103; do
    [ -f "$out/deep/seed-$seed/DONE" ] && continue
    one "$seed" $((7996 + i)) "$out/deep/seed-$seed" \
        "$here/tuning/frontier-defaults.ini" \
        "$here/configs/contested.conf" &
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
  # The proof must not pass vacuously: the pinned binary has to be the
  # one carrying this epic's census lines, and neither gate may have
  # fired. grep -c, not grep -q: with pipefail an early-exiting grep -q
  # kills zstdcat with SIGPIPE and the pipeline reports 141, which
  # reads as "absent" when the string is present.
  local log=$out/deep/seed-101/game.log.zst
  local want miss=0
  for want in 'deep-sitewall ' 'offlegal='; do
    if [ "$(zstdcat "$log" 2>/dev/null | grep -ac "$want")" = 0 ]; then
      echo "STOP: the pinned binary does not emit $want" >&2
      miss=1
    fi
  done
  for want in 'deep-transit ' 'offlegal=[1-9]'; do
    if [ "$(zstdcat "$log" 2>/dev/null | grep -ac "$want")" != 0 ]; then
      echo "STOP: a gate fired, or the theft axis is alive: $want" >&2
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
    "$here/frontier.py" "$OUT/$v"
    for c in $CELLS; do
      [ "$c" = c00 ] && continue
      [ -d "$OUT/$v/$c" ] || continue
      echo "#### paircmp c00 -> $c ($v)"
      "$here/paircmp.py" "$OUT/$v/c00" "$OUT/$v/$c" --arm deep
    done
  done
}

case "${1:-}" in
  identity) do_identity ;;
  war)      venue war "$here/configs/contested.conf" 7960 ;;
  econ)     venue econ "$here/configs/lategame-80.conf" 7960 ;;
  go)       do_identity && venue war "$here/configs/contested.conf" 7960 \
                        && venue econ "$here/configs/lategame-80.conf" 7960
            do_report > "$OUT/report.txt" 2>&1; cat "$OUT/report.txt" ;;
  report)   do_report ;;
  *) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
