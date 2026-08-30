#!/usr/bin/env bash
# THE FEAR SEAT (S-FEARSEAT): what the early army is priced off.
#
#   setsid bash ./benchmark/fearseat.sh go        identity, both A/Bs,
#                                                 then the report
#   setsid bash ./benchmark/fearseat.sh identity  3 contested holdout
#                                                 seeds on compiled
#                                                 defaults, scorelogs
#                                                 byte-compared with
#                                                 the ARCHIVED
#                                                 s6-nogates-war arm
#   setsid bash ./benchmark/fearseat.sh war       the six rungs on
#                                                 contested, TRAINING
#                                                 seeds
#   setsid bash ./benchmark/fearseat.sh econ      the same six on
#                                                 lategame-80
#   ./benchmark/fearseat.sh report                the tables
#
# Launch from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped. Watch it by DONE
# counts; this box does not report its process table honestly.
#
# THE HYPOTHESIS. The 2026-08-30 census
# (findings/content-military-gold-2026-08-30.md) localized deep's
# entire holdout deficit to 0.65 missing cities per player, built in
# T50-75 (contested) / T20-50 (econ) and never moved after. In that
# band deep delivers 131 shields per player of military hardware
# against 15 of settlers, while `unitslost` is identical to baseline
# to +/- 0.03 through T75 in both venues. The early army is pure
# opportunity cost, and the seat that orders it is two flat geographic
# terms: deep_density_fear() (54.9% of T1-25 build-seat evaluations
# against stock's 12.5%) and, from T51, deep_boat_shadow_fear() (a
# flat 25 on every coastal city once any enemy anywhere owns one
# transport).
#
# PRE-REGISTERED FALSIFICATION, from the census, not from this script:
# an arm that ramps anticipatory fear must show FEWER T10-50 military
# deliveries with NO rise in `unitslost` before T75. If unitslost
# rises, the diagnosis is wrong and the document gets re-read -- the
# tuning does NOT get re-swept.
#
# SECOND PRE-REGISTRATION, on the seat itself: the ramp must reduce
# the share of build-seat evaluations carrying a nonzero `density` in
# T1-50 (54.9% / 72.1% today). If deep-fear shows that share
# unchanged, the knob never reached the seat and the arm's score means
# nothing. fearcell.py reports it.
#
# FACTORIAL, NOT THREE ARMS. density_fear_ramp_turns,
# fear_evidence_floor and mil_opportunity_price all move WHICH CITIES
# ORDER A DEFENDER IN THE SAME WINDOW. They get off / each / the
# all-on corner, and the corner is what says whether they compose. The
# boat shadow is a coastal term that fires from T51 and is exempt from
# the ramp by construction, so it is its own arm.
#
# SEEDS ARE TRAINING (1-20). This is an adjudication, not a report:
# benchmark/seeds/holdout.txt (101-200) is touched ONLY by `identity`,
# which compares byte-for-byte against an archive taken on those same
# seeds and reads no score.
#
# ATTRIBUTION. S-SETTLERSINK (site cap + join guard) is being
# adjudicated concurrently in another worktree, and both epics act on
# what the T50-75 queue produces. This A/B is against a base WITHOUT
# those gates; the combined reading needs a 2x2 after both merge.
#
# NICE. The user plays on this box. Every game runs at nice 10, jobs
# 4, ports 7960-7999, and this worktree's own pinned binary.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=$ROOT/benchmark/results/fearseat
JOBS=${JOBS:-4}
NICE=${NICE:-10}
SEEDS=${SEEDS:-$(seq 1 20)}
RUNGS=${RUNGS:-off ramp evid price all boat}
ARCHIVE=/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0

[ -x "$FIX" ] || { echo "no pinned server at $FIX" >&2; exit 1; }

ini_for() { echo "$here/tuning/fearseat-$1.ini"; }

# ONE DRIVER AT A TIME. The sandbox namespaces the process table, so
# "is it still running?" cannot be answered by looking, and two
# drivers on one output dir and one port block cost a whole batch on
# 2026-08-23.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another fearseat driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# One game, pruned to its census lines as it finishes rather than at
# the end -- the disk is at 92%. A partial directory from an earlier
# kill is deleted rather than resumed: a leftover scorelog.txt
# silently disables scorelogging for the whole game (STATUS
# 2026-08-13).
one() { # seed port dir tune config
  local seed=$1 port=$2 d=$3 tune=$4 cfg=$5
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" nice -n "$NICE" "$here/run-game.sh" \
    --config "$cfg" --seed "$seed" --arm deep --out "$d" \
    --server "$FIX" --port "$port" >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-tuning|deep-fear|deep-density|deep-shadow|deep-milopp|deep-buy|deep-want |deep-prodswitch|deep-bid|deep-founderbid|deep-loss|deep-floor|deep-def|deep-fin|deep-expand" \
         "$d/game.log" > "$d/game.log.new"
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
  local name=$1 cfg=$2 port0=$3 rung n i=0
  take_lock
  n=$(echo $SEEDS | wc -w)
  for rung in $RUNGS; do
    echo "=== $name/$rung ($n games) $(date -Is) ==="
    # shellcheck disable=SC2086
    arm_run "$OUT/$name/$rung" "$(ini_for "$rung")" "$cfg" \
            $((port0 + i * 8)) $SEEDS
    mark "$OUT/$name/$rung" "$n"
    i=$(((i + 1) % 3))
  done
}

# ------------------------------------------------------------ identity
# Every commit in this epic is either observation or a gate that
# defaults to 0, so compiled defaults must reproduce the archived
# holdout arm EXACTLY. Byte comparison of the scorelog, `id` line
# excluded (it is a per-run random token).
do_identity() {
  local out=$OUT/identity seed same=0 diff=0 i=0

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
    local a=$ARCHIVE/seed-$seed/scorelog.txt
    local b=$out/deep/seed-$seed/scorelog.txt
    if [ -r "$a" ] && [ -r "$b" ] \
       && diff -q <(grep -v '^id ' "$a") <(grep -v '^id ' "$b") >/dev/null
    then
      same=$((same + 1)); echo "SAME seed $seed"
    else
      diff=$((diff + 1)); echo "DIFF seed $seed"
    fi
  done
  echo "identity: $same identical, $diff different"
  # The proof must not be able to pass vacuously: the pinned binary
  # has to be the one that emits this epic's new columns.
  # grep -c, not grep -q: with `set -o pipefail` an early-exiting
  # grep -q kills zstdcat with SIGPIPE and the pipeline reports 141,
  # which reads as "the string is absent" when it is present. That
  # false STOP cost one launch on 2026-08-30.
  if [ "$(zstdcat "$out/deep/seed-101/game.log.zst" 2>/dev/null \
          | grep -ac 'deep-fear .* site=ledger')" = 0 ]; then
    echo "STOP: the pinned binary does not emit site=ledger." >&2
    diff=$((diff + 1))
  fi
  if [ "$same" -eq 3 ] && [ "$diff" -eq 0 ]; then
    echo "SUCCESS identity $(date -Is)" > "$out/DONE-BATCH"
  else
    echo "FAIL identity $same/$diff $(date -Is)" > "$out/DONE-BATCH"
  fi
  cat "$out/DONE-BATCH"
  [ "$diff" -eq 0 ]
}

do_report() {
  local venue rung
  for venue in war econ; do
    [ -d "$OUT/$venue" ] || continue
    for rung in $RUNGS; do
      [ -d "$OUT/$venue/$rung" ] || continue
      "$here/extract.py" --root "$OUT/$venue/$rung" \
        --csv "$OUT/$venue/$rung/metrics.csv" >/dev/null 2>&1
    done
    echo "################################ venue $venue"
    "$here/fearcell.py" "$OUT/$venue" --rungs "$RUNGS"
    for rung in $RUNGS; do
      [ -f "$OUT/$venue/$rung/metrics.csv" ] || continue
      [ "$rung" = off ] && continue
      echo "#### paircmp off -> $rung ($venue)"
      "$here/paircmp.py" "$OUT/$venue/off" "$OUT/$venue/$rung" --arm deep
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
    echo "STOP: the epic is not inert at its defaults." >&2
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
