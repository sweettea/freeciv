#!/usr/bin/env bash
# THE DIPLOPRICE 2x2 (S-DIPLOPRICE): do the two unpriced shield lines
# pay when they are priced?
#
#   setsid bash ./benchmark/diploprice.sh ident   3 contested holdout
#                                                 seeds against the
#                                                 ARCHIVED s6-nogates
#                                                 -war scorelogs, byte
#                                                 for byte (id line
#                                                 excluded)
#   setsid bash ./benchmark/diploprice.sh run     the 2x2, paired,
#                                                 TRAINING seeds, two
#                                                 venues, arm deep
#   ./benchmark/diploprice.sh report              paired score and the
#                                                 two shield lines
#
# Launch from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped. Watch it by DONE
# counts; this box does not report its process table honestly.
#
# THE DESIGN. findings/diploprice-verdict-2026-08-30.md section 1
# measured two lines that leave deep's queue on evidence nobody
# checked:
#
#   D = diplo_price_gate    the defensive diplomat, priced off OBSERVED
#                           diplomat-class presence and the tech gap
#                           instead of stock's "totally arbitrary
#                           16000" on a flag any Warriors can set
#   M = mil_infra_gate      Barracks / City Walls / Coastal Defense,
#                           gated on stock's MEASURED danger rather
#                           than on deep's anticipatory fear
#
# They are a 2x2 and not two A/Bs because both act on the same city's
# same build auction in the same turn band: a shield the diplomat gate
# frees is a shield the mil-infra gate can spend. Cells are named DM.
#
# TWO VENUES, because the census found opposite shapes. contested.conf
# is where the Diplomat line is 138 shields and mil-infra 288;
# lategame-80.conf is where the Diplomat line is 194 and Coastal
# Defense burns 67 shields per player-game in abandoned runs. A gate
# that helps on one and hurts on the other is information.
#
# SEEDS ARE TRAINING (benchmark/seeds/train.txt). This is an
# adjudication, not a report. The identity stage is the one exception
# and it compares against an ARCHIVED corpus rather than measuring.
#
# ATTRIBUTION WARNING, recorded because it will be forgotten.
# S-FEARSEAT and S-SETTLERSINK are adjudicating on the SAME T50-75
# queue at the same time. Nothing here can be added to their results;
# the combined reading needs a factorial after the merge.
#
# NICE. The user plays on this box. Every game process runs at nice 10.
#
# Machine budget: ports 7800-7849, jobs <= 4, this worktree's own
# build/ and its own pinned binary under .pin-bin/fix.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=$here/results/diploprice
JOBS=${JOBS:-4}
NICE=${NICE:-10}
CELLS=${CELLS:-00 10 01 11}
VENUES=${VENUES:-contested lategame-80}
SEEDS=${SEEDS:-$(head -12 "$here/seeds/train.txt")}
ARCHIVE=${ARCHIVE:-/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0}

[ -x "$FIX" ] || { echo "no server at $FIX -- pin it first" >&2; exit 1; }

# ONE DRIVER AT A TIME. This box's process table is namespaced, so
# pgrep sees nothing and "is it still running?" cannot be answered by
# looking.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another diploprice driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# One game, pruned to its census lines as it finishes rather than at
# the end: this box's disk sits above 90%.
one() { # seed port dir tune config
  local seed=$1 port=$2 d=$3 tune=$4 cfg=$5
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" \
    nice -n "$NICE" "$here/run-game.sh" --config "$cfg" --seed "$seed" \
    --arm deep --out "$d" --server "$FIX" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-dipthreat |deep-spy |deep-milinfra |deep-want |deep-prodswitch |deep-buy " \
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
# Both gates ship at their identity value, so the shipped binary must
# reproduce the ARCHIVED corpus byte for byte. The archive is
# tune-postfix/s6-nogates-war, which ran compiled defaults on contested
# -- exactly cell 00. The `id` line is a per-run random token and is
# the one line excluded.
do_ident() {
  local out=$OUT/ident seeds=${IDENT_SEEDS:-101 102 103}
  local same=0 diff=0 seed i=0 a b

  take_lock
  mkdir -p "$out"
  echo "fix $(sha256sum "$FIX")"
  for seed in $seeds; do
    [ -f "$out/deep/seed-$seed/DONE" ] && continue
    one "$seed" $((7840 + i % 8)) "$out/deep/seed-$seed" \
        "$here/tuning/diploprice-00.ini" \
        "$here/configs/contested.conf" &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
  wait
  for seed in $seeds; do
    a=$out/deep/seed-$seed/scorelog.txt
    b=$ARCHIVE/seed-$seed/scorelog.txt
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

# ------------------------------------------------------------ factorial
do_run() {
  local cell seed venue n i=0 cfg ini

  take_lock
  n=$(echo $SEEDS | wc -w)
  for venue in $VENUES; do
    cfg=$here/configs/$venue.conf
    [ -r "$cfg" ] || { echo "no config $cfg" >&2; continue; }
    for cell in $CELLS; do
      ini=$here/tuning/diploprice-$cell.ini
      [ -r "$ini" ] || { echo "no ini for cell $cell" >&2; continue; }
      echo "=== $venue cell $cell ($n games) $(date -Is) ==="
      for seed in $SEEDS; do
        [ -f "$OUT/ab/$venue/$cell/deep/seed-$seed/DONE" ] && continue
        one "$seed" $((7800 + i % 40)) \
            "$OUT/ab/$venue/$cell/deep/seed-$seed" "$ini" "$cfg" &
        i=$((i + 1))
        while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
      done
      wait
      mark "$OUT/ab/$venue/$cell" "$n"
    done
  done
  echo "=== factorial complete $(date -Is) ==="
  touch "$OUT/RUN-DONE"
}

# --------------------------------------------------------------- report
do_report() {
  local cell venue
  for venue in $VENUES; do
    [ -d "$OUT/ab/$venue" ] || continue
    echo "############ venue $venue: paired against cell 00"
    for cell in $CELLS; do
      [ "$cell" = 00 ] && continue
      [ -d "$OUT/ab/$venue/$cell" ] || continue
      echo "---- 00 -> $cell"
      "$here/paircmp.py" "$OUT/ab/$venue/00" "$OUT/ab/$venue/$cell" \
        --arm deep \
        --pat "dipthreat-set=deep-dipthreat .* threat=1 " \
        --pat "dipthreat-real=deep-dipthreat .* dip=[1-9]" \
        --pat "dip-pick=deep-want .* pick=\"Diplomat\"" \
        --pat "dip-refuse=deep-dipthreat .* verdict=refuse" \
        --pat "walls-pick=deep-want .* pick=\"City Walls\"" \
        --pat "barr-pick=deep-want .* pick=\"Barracks" \
        --pat "coast-pick=deep-want .* pick=\"Coastal Defense\"" \
        --pat "infra-refuse=deep-milinfra .* verdict=refuse"
    done
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
  ;;
*) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 1;;
esac
