#!/usr/bin/env bash
# THE OP-ARRIVAL FACTORIAL (S-OPARRIVAL): does pricing the march move
# anything the launch gate promised?
#
#   setsid bash ./benchmark/oparrival.sh ident   3 contested holdout
#                                                seeds against the
#                                                ARCHIVED s6-nogates
#                                                -war scorelogs, byte
#                                                for byte (id line
#                                                excluded)
#   setsid bash ./benchmark/oparrival.sh run     the 2x2x2 factorial +
#                                                the pre-registered eta
#                                                cell, paired, TRAINING
#                                                seeds, arm deep
#   ./benchmark/oparrival.sh report              funnel, exchange,
#                                                paired score
#
# Launch from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped. Watch it by DONE
# counts; this box does not report its process table honestly.
#
# THE DESIGN, and why it is one design. findings/op-exchange-rate
# -2026-08-29.md section 9: op_arrival_decay, op_patience_odds_floor
# and the re-gate terms all move WHICH WAVES FIGHT. Two epics on one
# causal chain cannot be read from two separate A/Bs (CLAUDE.md,
# "Parallelise scheduling, not attribution"), so the three gates are
# one 2x2x2 factorial over cells named APR:
#
#   A = op_arrival_gate (decay + eta cap + march loss)
#   P = op_patience_odds_floor
#   R = op_regate_gate (sunk credit + eta freeze)
#
# and 000 is the control. The values in each cell ini are DERIVED from
# the census, never chosen; the derivations are written at the top of
# every cell file. What this adjudicates is the gate DEFAULT, not the
# values -- the values are PLACEHOLDER and owed to a tuning sweep.
#
# The ETA cell runs BESIDE the factorial rather than inside it because
# it is the pre-registered falsification and has to be readable alone:
# op_launch_eta_cap and nothing else. Prediction -- fewer launches,
# fewer unitslost, UNCHANGED unitskilled. If unitskilled falls, the
# diagnosis is wrong and the finding gets re-read, not the tuning
# re-swept.
#
# SEEDS ARE TRAINING (benchmark/seeds/train.txt). This is an
# adjudication, not a report: holdout.txt is not touched here and must
# not be, or the eventual holdout number means nothing. The identity
# stage is the one exception, and it compares against an ARCHIVED
# corpus rather than measuring anything.
#
# VENUE. contested.conf: 8 players, 32x32, T150 -- the venue all four
# corpora in the finding were measured on, and the one where the
# operations layer actually runs.
#
# NICE. The user plays on this box. Every game process runs at nice 10.
#
# Machine budget: ports 7800-7849, jobs <= 8, this worktree's own
# build/ and its own pinned binary under .pin-bin/fix.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
CONFIG=$here/configs/contested.conf
OUT=$here/results/oparrival
JOBS=${JOBS:-8}
NICE=${NICE:-10}
CELLS=${CELLS:-000 100 010 001 110 101 011 111 eta}
SEEDS=${SEEDS:-$(seq 1 16)}
ARCHIVE=${ARCHIVE:-/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0}

[ -x "$FIX" ] || { echo "no server at $FIX" >&2; exit 1; }

# ONE DRIVER AT A TIME. This box's process table is namespaced, so
# pgrep sees nothing and "is it still running?" cannot be answered by
# looking; two drivers racing on one output dir and one port block
# cost a whole batch on 2026-08-23.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another oparrival driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# One game, pruned to its census lines as it finishes rather than at
# the end: this box's disk sits above 90%. A partial directory from an
# earlier kill is deleted rather than resumed -- a leftover
# scorelog.txt silently disables scorelogging for the whole game
# (STATUS 2026-08-13).
one() { # seed port dir tune
  local seed=$1 port=$2 d=$3 tune=$4
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" \
    nice -n "$NICE" "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
    --arm deep --out "$d" --server "$FIX" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-op |deep-march |deep-netval |deep-opodds |deep-employop |deep-invade " \
         "$d/game.log" > "$d/game.log.new" 2>/dev/null
    mv "$d/game.log.new" "$d/game.log"
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
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

# ------------------------------------------------------------ identity
# Every gate ships at its identity value, so the shipped binary must
# reproduce the ARCHIVED corpus byte for byte. The archive is
# tune-postfix/s6-nogates-war, which ran compiled defaults on contested
# -- exactly cell 000. The `id` line is a per-run random token and is
# the one line excluded.
do_ident() {
  local out=$OUT/ident seeds=${IDENT_SEEDS:-101 102 103}
  local same=0 diff=0 seed i=0

  take_lock
  mkdir -p "$out"
  echo "fix $(sha256sum "$FIX")"
  for seed in $seeds; do
    [ -f "$out/deep/seed-$seed/DONE" ] && continue
    one "$seed" $((7840 + i % 8)) "$out/deep/seed-$seed" \
        "$here/tuning/oparrival-000.ini" &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
  wait
  for seed in $seeds; do
    local a=$out/deep/seed-$seed/scorelog.txt b=$ARCHIVE/seed-$seed/scorelog.txt
    if [ -r "$a" ] && [ -r "$b" ] \
       && diff -q <(grep -v '^id ' "$a") <(grep -v '^id ' "$b") >/dev/null
    then
      same=$((same + 1))
    else
      diff=$((diff + 1)); echo "DIFF seed $seed"
    fi
  done
  echo "identity: $same identical, $diff different"
  if [ "$diff" -eq 0 ] && [ "$same" -gt 0 ]; then
    echo "SUCCESS ident $same $(date -Is)" > "$out/DONE-BATCH"
  else
    echo "FAIL ident $same/$diff $(date -Is)" > "$out/DONE-BATCH"
  fi
  cat "$out/DONE-BATCH"
  [ "$diff" -eq 0 ]
}

# ------------------------------------------------------------- factorial
do_run() {
  local cell seed n i=0

  take_lock
  n=$(echo $SEEDS | wc -w)
  for cell in $CELLS; do
    local ini=$here/tuning/oparrival-$cell.ini
    [ -r "$ini" ] || { echo "no ini for cell $cell" >&2; continue; }
    echo "=== cell $cell ($n games) $(date -Is) ==="
    for seed in $SEEDS; do
      [ -f "$OUT/ab/$cell/deep/seed-$seed/DONE" ] && continue
      one "$seed" $((7800 + i % 40)) "$OUT/ab/$cell/deep/seed-$seed" "$ini" &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    done
    wait
    mark "$OUT/ab/$cell" "$n"
  done
  echo "=== factorial complete $(date -Is) ==="
  touch "$OUT/RUN-DONE"
}

# --------------------------------------------------------------- report
do_report() {
  local cell
  for cell in $CELLS; do
    [ -d "$OUT/ab/$cell" ] || continue
    "$here/extract.py" --root "$OUT/ab/$cell" \
      --csv "$OUT/ab/$cell/metrics.csv" >/dev/null 2>&1
  done
  echo "############ the operation funnel, per cell"
  python3 "$here/oparrival.py" funnel $OUT/ab/*
  echo
  echo "############ the per-operation ledger, per cell"
  python3 "$here/oparrival.py" exchange $OUT/ab/*
  echo
  echo "############ the march, per cell"
  python3 "$here/oparrival.py" march $OUT/ab/*
  echo
  echo "############ paired score+kills against cell 000"
  for cell in $CELLS; do
    [ "$cell" = 000 ] && continue
    [ -d "$OUT/ab/$cell" ] || continue
    echo "---- 000 -> $cell"
    # The op counters are paired seed by seed alongside score and
    # kills, so "fewer operations" and "unchanged unitskilled" are read
    # off ONE table with one set of standard errors.
    "$here/paircmp.py" "$OUT/ab/000" "$OUT/ab/$cell" --arm deep \
      --pat "ops-formed=deep-op turn=\S+ player=\"[^\"]+\" op=[0-9]+ (form|muster) " \
      --pat "ops-launched=deep-op turn=\S+ player=\"[^\"]+\" op=[0-9]+ march " \
      --pat "ops-strike=deep-op turn=\S+ player=\"[^\"]+\" op=[0-9]+ strike " \
      --pat "ops-take=deep-op turn=\S+ player=\"[^\"]+\" op=[0-9]+ consolidate " \
      --pat "ops-abort-odds=deep-op turn=\S+ player=\"[^\"]+\" op=[0-9]+ abort-odds " \
      --pat "launch-patience=deep-op .* march .* patience=1" \
      --pat "launch-etacap=deep-netval .* etacap " \
      --pat "netval-refuse=deep-netval .* refuse "
  done
}

case "${1:-}" in
ident)  do_ident;;
run)    do_run;;
report) do_report;;
go)
  if ! do_ident; then
    echo "STOP: the gates are not inert at their defaults." >&2
    exit 1
  fi
  do_run
  do_report > "$OUT/report.txt" 2>&1
  echo "report at $OUT/report.txt"
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
