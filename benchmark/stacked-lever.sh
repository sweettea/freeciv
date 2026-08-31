#!/usr/bin/env bash
# S-MARGINAL: the stacked lever, at the n the effect actually needs.
#
#   setsid bash ./benchmark/stacked-lever.sh identity   3 contested
#                                                       holdout seeds at
#                                                       compiled defaults,
#                                                       scorelogs byte-
#                                                       compared with the
#                                                       ARCHIVED
#                                                       s6-nogates-war
#                                                       games (`id`
#                                                       excluded)
#   setsid bash ./benchmark/stacked-lever.sh war        off vs on on
#                                                       contested,
#                                                       TRAINING seeds
#   setsid bash ./benchmark/stacked-lever.sh econ       the same on
#                                                       lategame-80
#   ./benchmark/stacked-lever.sh report                 the tables
#   setsid bash ./benchmark/stacked-lever.sh go         identity, then
#                                                       both venues, then
#                                                       the report
#
# Launch from the repo root of THIS worktree, DETACHED. Resumable: a
# game whose directory carries DONE is skipped, so an interrupted run
# picks up where it stopped and a 60-seed interim read is available
# before the 100th seed lands. Watch it by DONE counts; this box does
# not report its process table honestly.
#
# ============================ SIZE ============================
#
#   2 cells x 2 venues x 100 TRAINING seeds  =  400 games
#   + 3 identity                             =  403 games
#
# EXPECTED SEs AT n = 100, per seat, paired on the seed
# (from benchmark/marginalcity.py power over the 13 arms of each venue;
#  pooled paired per-seed sd is score 7.98 / cities 0.319 on contested
#  and score 6.39 / cities 0.816 on lategame-80):
#
#              SE(score)   SE(cities@end)
#   contested     0.80         0.032
#   lategame-80   0.64         0.082
#
# EXPECTED EFFECT, stated before the run, as a RANGE because the
# sub-additivity is measured and real:
#
#                        additive        half-additive
#   war   cities@end   +0.219 +/-0.113      +0.110
#   war   score        +3.16  +/-2.01       +1.58
#   econ  cities@end   +0.423 +/-0.373      +0.212
#   econ  score        +1.80  +/-1.54       +0.90
#
# so at n = 100 the score endpoint is z 4.0 (war, additive) down to
# z 1.4 (econ, half-additive), and the CITY endpoint is z 6.9 down to
# z 2.6. n = 100 is also the whole training pool: benchmark/seeds/
# train.txt is seeds 1-100 and 101-200 are holdout, which may not be
# touched here.
#
# ======================= WHY THIS CELL EXISTS =======================
#
# findings/marginal-city-2026-08-30.md. Four epics on this chain --
# S-FEARSEAT, S-SITEPLAN, S-FEARSINK, S-FRONTIER -- each produced a
# clean mechanism and a city gain of +0.02 to +0.15 per seat, and each
# adjudicated on a score column whose SE was 0.85 to 2.15. A city
# founded in T51-75 is worth +6.98 +/-0.59 score at T150 (contested,
# 120 gate-off training games, 941 seats), so those city gains were
# worth +0.04 to +2.15 points and NOT ONE of those cells could tell
# the predicted effect from zero. This cell stacks the four surviving
# mechanisms so the sum is above the noise floor, and runs the n that
# the noise floor requires.
#
# THE TWO ARMS:
#
#   off   every gate at its compiled 0            = today's binary
#   on    mil_opportunity_gate                    S-FEARSEAT
#         founder_site_cap_gate                   S-SITEPLAN
#         founder_join_guard_gate                 S-SITEPLAN
#         frontier_transit_gate                   S-FRONTIER
#         boat_shadow_gate                        S-FEARSEAT
#
# founder_floor_gate and founder_run_hold are deliberately OUT; see
# benchmark/tuning/stacked-on.ini for the numbers that excluded them.
#
# THIS IS THE CORNER, NOT THE FACTORIAL, AND THAT IS A CHOICE. The cap
# and the price are known to partially disarm each other: with the cap
# on, milopp_fired falls -41.70 +/-9.80 a game (t -4.25) and cap_fired
# rises +22.55 +/-8.55, because the cap withdraws the founder bid the
# price would have charged against (S-FEARSINK S4). The transit gate
# raises the founder population, which raises the search count, which
# raises the occasions the cap binds -- untested, sign unknown. A
# five-cell factorial at n = 100 is 1000 games and is not affordable.
# This cell answers "is the stack worth shipping"; if it clears, the
# decomposition is the next cell and it will be worth its games.
#
# PRE-REGISTERED, before the run:
#
#  1. PRIMARY ENDPOINT is cities per seat at the venue's end turn, not
#     score. The city meter is 2.4x tighter in SD and 5.8x cheaper in
#     games for an expansion lever (marginal-city S4). If the city
#     delta is inside its SE in both venues, the stack is a null on its
#     own mechanism and the score does not need reading.
#  2. SECONDARY ENDPOINT is score per seat, against the range above.
#  3. FALSIFICATION is unitslost. All five gates had it flat or falling
#     individually. A rise at T75 clearing its SE in EITHER venue means
#     the stack bought its foundings with losses and the corner is void
#     whatever the score did.
#  4. WHAT A NULL LOOKS LIKE: cities@end inside its SE in both venues;
#     OR a score delta smaller than half the additive prediction WITH
#     the city delta at full size -- which would mean cities stopped
#     converting at the stacked margin, and would be a new finding
#     rather than a confirmation of the old one.
#  5. SHIP RULE, unchanged: the corner ships only if it clears its SE by
#     a reasonable margin in BOTH venues.
#
# CONSTANTS ARE NOT SWEPT HERE (rule 7). Every one is the PLACEHOLDER
# its parent epic left: mil_opportunity_price 1.0,
# founder_site_cap_slack 1.0, founder_join_guard_mult 1.0,
# frontier_transit_price 1.0, frontier_transit_stance_floor 0,
# boat_shadow_distance 1.0, boat_shadow_count 1. A sweep on top of an
# unadjudicated corner would be unreadable.
#
# SEEDS ARE TRAINING (1-100). holdout.txt is touched ONLY by
# `identity`, which reads no score.
#
# NO CACHE REUSE. Every prior cell pinned a different binary, and the
# off arm here must be re-run rather than borrowed: run.sh's cache is
# keyed on the binary sha for exactly this reason.
#
# NICE AND DISK. The user plays on this box. Every game runs at nice 10,
# jobs 3, ports 7900-7939, and this worktree's own pinned binary. 400
# contested/lategame games is a lot of log: each one is pruned to
# nine census tags and zstd-compressed AS IT FINISHES, not at the end.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=${OUT:-$ROOT/benchmark/results/stacked}
JOBS=${JOBS:-3}
NICE=${NICE:-10}
SEEDS=${SEEDS:-$(seq 1 100)}
CELLS=${CELLS:-off on}
PORT0=${PORT0:-7900}
ARCHIVE=/home/sweettea/freeciv/benchmark/results/tune-postfix/s6-nogates-war/holdout-report/cfg0

# The census lines the tables and the identity guards need. Everything
# else in a contested T150 game.log is hundreds of megabytes nobody has.
# deep-sitewall is here because the identity guard greps for it AFTER
# the filter has run.
KEEP='deep-tuning|deep-city |deep-expand |deep-joinguard |deep-sitecap |deep-milopp |deep-transit |deep-shadow |deep-sitewall '

[ -x "$FIX" ] || { echo "no pinned server at $FIX" >&2; exit 1; }

ini_for() { echo "$here/tuning/stacked-$1.ini"; }

# ONE DRIVER AT A TIME. The sandbox namespaces the process table, so
# "is it still running?" cannot be answered by looking, and two drivers
# on one output dir and one port block cost a whole batch on 2026-08-23.
take_lock() {
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  if ! flock -n 9; then
    echo "STOP: another stacked-lever driver holds $OUT/.driver.lock" >&2
    exit 1
  fi
}

# One game, pruned to its census lines as it finishes. A partial
# directory from an earlier kill is deleted rather than resumed: a
# leftover scorelog.txt silently disables scorelogging for the whole
# game (STATUS 2026-08-13).
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
# The off arm is every gate at 0, so it must be the compiled default
# byte for byte. Proven against the archived s6-nogates-war games, `id`
# excluded (a per-run random token). The four epics whose gates this
# cell stacks each passed the same proof against the same games, so a
# failure here means the merge changed behaviour, not that one gate did.
do_identity() {
  local out=$OUT/identity seed same=0 diff=0 i=0 a b

  take_lock
  mkdir -p "$out"
  echo "fix $(sha256sum "$FIX")"
  for seed in 101 102 103; do
    [ -f "$out/deep/seed-$seed/DONE" ] && continue
    one "$seed" $((PORT0 + 36 + i)) "$out/deep/seed-$seed" \
        "$(ini_for off)" "$here/configs/contested.conf" &
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
  # to be the one carrying all five gates' census lines, and not one of
  # them may have fired. grep -c, not grep -q: with pipefail an
  # early-exiting grep -q kills zstdcat with SIGPIPE and the pipeline
  # reports 141, which reads as "absent" when the string is present.
  local log=$out/deep/seed-101/game.log.zst
  local want miss=0
  for want in 'deep-joinguard ' 'deep-sitecap ' 'deep-sitewall '; do
    if [ "$(zstdcat "$log" 2>/dev/null | grep -ac "$want")" = 0 ]; then
      echo "STOP: the pinned binary does not emit $want" >&2
      miss=1
    fi
  done
  for want in 'deep-joinguard .* held=1 gate=1' \
              'deep-sitecap .* capped=1 gate=1' \
              'deep-milopp ' 'deep-transit ' 'deep-shadow '; do
    if [ "$(zstdcat "$log" 2>/dev/null | grep -ac "$want")" != 0 ]; then
      echo "STOP: a gate fired in the off arm: $want" >&2
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

# The vacuity guard for the ON arm, which identity cannot give: every
# gate must be seen to FIRE somewhere, or the arm is a rerun of off.
check_on_fired() { # venue
  local v=$1 log want n bad=0
  log=$(find "$OUT/$v/on" -name 'game.log.zst' | head -1)
  [ -n "$log" ] || { echo "no on-arm log under $OUT/$v/on" >&2; return 1; }
  for want in 'deep-milopp ' 'deep-transit ' 'deep-shadow ' \
              'deep-sitecap .* capped=1 gate=1' \
              'deep-joinguard .* held=1 gate=1'; do
    n=$(zstdcat "$log" 2>/dev/null | grep -ac "$want")
    echo "  on/$v fired $want : $n"
    [ "$n" = 0 ] && bad=1
  done
  [ "$bad" = 0 ] || echo "STOP: a gate never fired in the on arm ($v)" >&2
  return $bad
}

do_report() {
  local v
  for v in war econ; do
    [ -d "$OUT/$v" ] || continue
    echo "################################ venue $v"
    check_on_fired "$v"
    echo "#### PRIMARY: cities and foundings by band, off -> on"
    "$here/marginalcity.py" arms --arm "stacked/$v=$OUT/$v/on:$OUT/$v/off"
    echo "#### the value table this venue's prediction was made with"
    "$here/marginalcity.py" value --root "$v-off=$OUT/$v/off"
    echo "#### SECONDARY: paircmp off -> on"
    "$here/paircmp.py" "$OUT/$v/off" "$OUT/$v/on" --arm deep
    echo "#### the city ledger, births and deaths by band"
    "$here/cityledger.py" --root "$OUT/$v/off" --root "$OUT/$v/on"
  done
}

case "${1:-}" in
identity) do_identity;;
war)      venue war  "$here/configs/contested.conf"   "$PORT0";;
econ)     venue econ "$here/configs/lategame-80.conf" "$PORT0";;
report)   do_report;;
go)
  if ! do_identity; then
    echo "STOP: the off arm is not the compiled default." >&2
    exit 1
  fi
  venue war  "$here/configs/contested.conf"   "$PORT0"
  venue econ "$here/configs/lategame-80.conf" "$PORT0"
  do_report > "$OUT/report.txt" 2>&1
  echo "report at $OUT/report.txt"
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
