#!/usr/bin/env bash
# S-SITEPLAN: the founder site cap and the founder join guard, adjudicated.
#
#   setsid bash ./benchmark/siteplan.sh identity   3 contested games on
#                                                  seeds 101-103 at compiled
#                                                  defaults, scorelogs
#                                                  byte-compared against the
#                                                  ARCHIVED s6-nogates-war
#                                                  holdout games (the `id`
#                                                  line excluded: it is a
#                                                  per-run random string)
#   setsid bash ./benchmark/siteplan.sh ab         the 2x2, both venues
#   ./benchmark/siteplan.sh report                 the tables
#
# Launch DETACHED from the repo root of THIS worktree. Resumable: a game
# whose directory carries DONE is skipped. Watch it by DONE counts; this
# box does not report its process table honestly (STATUS 2026-08-23).
#
# PORTS 7850-7899, this worktree only, and its own pinned binary under
# .pin-bin/fix (a copy of build/freeciv-server WITH libfreeciv.so beside
# it -- RUNPATH is $ORIGIN/ and a bare copy cannot start).
#
# THE 2x2. Both new gates ship as ONE arm because they are one mechanism:
# the join guard stops deep spending founders and the production cap stops
# it hoarding them instead, and an arm carrying only one of the two would
# measure a defect the pair does not have. The second factor is
# founder_run_hold, which the settler-pipeline finding requires be
# factorial with this and not concurrent: run_hold puts more settlers into
# the pipeline and the guard decides what happens at the end of it.
#
#   c00  compiled defaults                       today
#   c10  + founder_site_cap_gate=1
#        + founder_join_guard_gate=1             this epic
#   c01  + founder_run_hold=1                    the feeder alone
#   c11  all three                               both
#
# TRAINING SEEDS, never holdout: this is an adjudication, not a report.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=${OUT:-$here/results/siteplan}
JOBS=${JOBS:-8}
NICE=${NICE:-10}
COUNT=${COUNT:-20}
SEEDS=${SEEDS:-$(head -n "$COUNT" "$here/seeds/train.txt")}
ARCHIVE=${ARCHIVE:-/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0}

mkdir -p "$OUT"

# The census lines this epic's tables are read on, plus the ones
# settlerpipe.py needs. Everything else in a contested T150 game.log is
# hundreds of megabytes we do not have.
KEEP='deep-settler |deep-join |deep-joinguard |deep-city |deep-siteladder |deep-pool |deep-sitecap |deep-prodswitch |deep-expand '

take_lock() {
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another siteplan driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

one() { # seed arm port dir tune config keep
  local seed=$1 arm=$2 port=$3 d=$4 tune=$5 cfg=$6 keep=${7:-1}
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" nice -n "$NICE" "$here/run-game.sh" \
    --config "$cfg" --seed "$seed" --arm "$arm" --out "$d" \
    --server "$FIX" --port "$port" >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    if [ "$keep" = 1 ]; then
      grep -aE "$KEEP" "$d/game.log" > "$d/game.log.new" 2>/dev/null
      mv -f "$d/game.log.new" "$d/game.log"
    fi
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

# PORTS: the pool must be far larger than JOBS (S-PRODKEEP paid for this
# with a half-finished arm). 40 ports, 8 jobs: the wrap needs 40 launches.
arm_run() { # dir tune config seeds...
  local dir=$1 tune=$2 cfg=$3; shift 3
  local i=0 seed
  for seed in "$@"; do
    if [ ! -f "$dir/seed-$seed/DONE" ]; then
      one "$seed" deep $((7850 + i % 40)) "$dir/seed-$seed" "$tune" "$cfg" &
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
  local src=$here/tuning/defaults.ini cell dest dup
  for cell in c00 c10 c01 c11; do
    dest=$OUT/$cell.ini
    grep -vE '^[[:space:]]*(founder_site_cap_gate|founder_join_guard_gate|founder_run_hold)' \
         "$src" > "$dest"
    case "$cell" in
      c10|c11) echo "founder_site_cap_gate = 1" >> "$dest"
               echo "founder_join_guard_gate = 1" >> "$dest";;
    esac
    case "$cell" in
      c01|c11) echo "founder_run_hold = 1" >> "$dest";;
    esac
    # A duplicate key makes secfile_load refuse the WHOLE file and the
    # cell silently runs on compiled defaults (S-CAPSTAKE section 8).
    dup=$(awk -F= '/^[a-z]/{gsub(/ /,"",$1); if(seen[$1]++) print "DUP:"$1}' \
          "$dest")
    [ -z "$dup" ] || { echo "STOP: duplicate keys in $dest: $dup" >&2; exit 1; }
  done
}

# ------------------------------------------------------------ identity
# With every gate at 0 this branch must reproduce the ARCHIVED games byte
# for byte. Those were run with a different binary at
# benchmark/tuning/defaults.ini on contested.conf, so config, tuning and
# runner shas must match (they are checked below) and only the server
# differs. The `id` line is a per-run random string and is excluded.
do_identity() {
  local out=$OUT/identity same=0 diff=0 rc=0 seed a b i=0

  take_lock
  mkdir -p "$out"
  echo "fix $(sha256sum "$FIX")"
  for seed in 101 102 103; do
    if [ ! -f "$out/seed-$seed/DONE" ]; then
      one "$seed" deep $((7890 + i)) "$out/seed-$seed" \
          "$here/tuning/defaults.ini" "$here/configs/contested.conf" &
      i=$((i + 1))
    fi
  done
  wait
  for seed in 101 102 103; do
    a=$ARCHIVE/seed-$seed/scorelog.txt
    b=$out/seed-$seed/scorelog.txt
    if [ ! -f "$a" ] || [ ! -f "$b" ]; then
      echo "MISSING seed-$seed"; diff=$((diff + 1)); continue
    fi
    if diff -q <(grep -av '^id ' "$a") <(grep -av '^id ' "$b") >/dev/null; then
      same=$((same + 1))
    else
      diff=$((diff + 1)); echo "DIFF seed-$seed"
    fi
  done
  echo "identity: $same identical, $diff different"
  # The proof must not be able to pass vacuously: the archived arm cannot
  # have emitted the counters this epic added, and this one must.
  if ! grep -aqs "deep-siteladder .* tbskip=" "$out"/seed-*/game.log; then
    echo "STOP: the fix arm emitted no tbskip= -- not the fix." >&2
    rc=1
  fi
  if ! grep -aqs "deep-joinguard " "$out"/seed-*/game.log; then
    echo "STOP: the fix arm emitted no deep-joinguard -- not the fix." >&2
    rc=1
  fi
  if grep -aqs "deep-joinguard .* held=1 gate=1" "$out"/seed-*/game.log; then
    echo "STOP: a join was declined with the gate OFF." >&2
    rc=1
  fi
  if grep -aqs "deep-sitecap .* capped=1 gate=1" "$out"/seed-*/game.log; then
    echo "STOP: a bid was withdrawn with the gate OFF." >&2
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
do_ab() { # [venue]
  local n venue cfg cell
  take_lock
  mktunes
  n=$(echo $SEEDS | wc -w)
  for venue in ${1:-war econ}; do
    case "$venue" in
      war)  cfg=$here/configs/contested.conf;;
      econ) cfg=$here/configs/lategame-80.conf;;
      *) echo "unknown venue $venue" >&2; exit 1;;
    esac
    for cell in c00 c10 c01 c11; do
      echo "=== $venue/$cell ($n games) $(date -Is) ==="
      # shellcheck disable=SC2086
      arm_run "$OUT/$venue/$cell" "$OUT/$cell.ini" "$cfg" $SEEDS
      mark "$OUT/$venue/$cell" "$n"
    done
  done
}

case "${1:-}" in
identity) do_identity;;
ab)       shift; do_ab "${1:-war econ}";;
go)
  if ! do_identity; then
    echo "STOP: the gates are not inert at their defaults." >&2
    exit 1
  fi
  do_ab "war econ"
  ;;
report)   shift; "$here/siteplan.py" "${1:-$OUT}";;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
