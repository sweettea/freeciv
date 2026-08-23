#!/usr/bin/env bash
# THE POACH'S PRICE (S-POACHPRICE): the corpus for the poach ledger.
#
#   setsid bash ./benchmark/poach-price.sh go       identity, then both
#                                                   arms, then the report
#   setsid bash ./benchmark/poach-price.sh identity 2 seeds, this branch
#                                                   with and without the
#                                                   ledger, byte-compared
#   setsid bash ./benchmark/poach-price.sh arms     the 40-game ON arm,
#                                                   then the 40-game OFF
#   ./benchmark/poach-price.sh report               the tables
#
# Launch it from the repo root of this worktree, DETACHED. It is
# self-contained and resumable: a game whose output directory carries
# DONE is skipped, so a driver killed at any point picks up where it
# stopped. Watch it by DONE counts and scorelog mtimes; this box does
# not report its process table honestly.
#
# ARMS. Both are the muster A/B of 2026-08-22 re-run on the instrumented
# binary, same venue (tech50war-200), same seeds (17-36), both seat
# orderings, mixed deep-vs-classic:
#
#   on   tuning/pv3-muster.ini  (op_muster = 1, op_muster_home = 0) --
#        the arm the departure-cause addendum measured
#   off  tuning/pv3.ini         -- the arm the addendum could only
#        infer about, and which its own text names as the cheapest
#        next run in this workstream
#
# IDENTITY. The ledger observes and must not steer. The proof is 2
# full-length games per side, this branch tip against the same tree
# with the ledger commit removed, scorelogs byte-compared. The base
# tree is built from BASE_SHA with the source dir passed EXPLICITLY:
# `meson setup DIR` alone configures the CWD, which in a worktree is
# the BRANCH, and the proof would compare HEAD with itself.
#
# Machine budget: ports 7400-7449, jobs <= 3, this worktree's own
# build/ and its own pinned binaries under .stage-bin/.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)

BASE_SHA=50ca21b736              # the branch point: the ledger's parent
BUILD=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.poach-base/base-$BASE_SHA
BASE_BUILD=$BASE_TREE/build/freeciv-server
CONFIG=$here/configs/tech50war-200.conf
ON=$here/tuning/pv3-muster.ini
OFF=$here/tuning/pv3.ini
OUT=$here/results/poach-price
SEEDS=${SEEDS:-$(seq 17 36)}
JOBS=${JOBS:-3}

MESON_OPTS="-Dclients= -Dfcmp= -Dtools= -Daimodules=classic,tex,baseline,deep"

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts. A re-pin of the SAME bytes must not copy: cp truncates and
# rewrites, which SIGBUSes every server already running from that path.
pin() {
  local src=$1 dst=$ROOT/.stage-bin/$2 f
  mkdir -p "$dst"
  for f in libfreeciv.so freeciv-server; do
    local from=$(dirname "$src")/$f to=$dst/$f
    [ "$f" = freeciv-server ] && from=$src
    if [ -f "$to" ] \
       && [ "$(sha256sum < "$from")" = "$(sha256sum < "$to")" ]; then
      continue
    fi
    cp -f "$from" "$to"
  done
  echo "$dst/freeciv-server"
}

# One game, pruned to its ledger lines as it finishes rather than at the
# end -- 80 games of tech50war-200 do not fit on this box otherwise.
# A partial directory from an earlier kill is deleted rather than
# resumed: a leftover scorelog.txt in a re-run directory silently
# disables scorelogging for the whole game (STATUS 2026-08-13).
one() {
  local seed=$1 arm=$2 port=$3 d=$4 bin=$5 tune=$6 keep=${7:-}
  rm -rf "$d"; mkdir -p "$d"
  env FREECIV_KILL_DYAD=1 TUNING="$tune" \
    "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
    --arm "$arm" --out "$d" --server "$bin" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-employ|kill-dyad|deep-writeoff" "$d/game.log" \
      > "$d/game.log.new"
    mv "$d/game.log.new" "$d/game.log"
    [ -n "$keep" ] || rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

# A whole arm, resumable, at most JOBS games at once.
arm_run() {
  local dir=$1 bin=$2 tune=$3 port0=$4 i=0 seed arm
  for seed in $SEEDS; do
    for arm in mixed_dc mixed_cd; do
      [ -f "$dir/$arm/seed-$seed/DONE" ] && continue
      one "$seed" "$arm" $((port0 + i % 10)) "$dir/$arm/seed-$seed" \
          "$bin" "$tune" &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    done
  done
  wait
}

# SUCCESS vs FAIL, not mere completion: a marker that only says "the
# driver reached the end" is the one that lies.
mark() {
  local dir=$1 want=$2 got
  got=$(find "$dir" -name DONE | wc -l)
  if [ "$got" -eq "$want" ]; then
    echo "SUCCESS $got/$want games $(date -Is)" > "$dir/DONE-BATCH"
  else
    echo "FAIL $got/$want games $(date -Is)" > "$dir/DONE-BATCH"
  fi
  cat "$dir/DONE-BATCH"
}

do_identity() {
  local iseeds="17 18" seed a b same=0 diff_n=0 fail=0
  if [ -x "$BASE_BUILD" ]; then
    echo "cached base binary: $BASE_BUILD"
  else
    rm -rf "$BASE_TREE"
    mkdir -p "$BASE_TREE"
    git -C "$ROOT" archive "$BASE_SHA" | tar -x -C "$BASE_TREE" || exit 1
    # shellcheck disable=SC2086
    meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS || exit 1
    nice -n 10 ninja -C "$BASE_TREE/build" -j 3 || exit 1
  fi
  base_bin=$(pin "$BASE_BUILD" identity-base)
  head_bin=$(pin "$BUILD" identity-head)
  echo "base: $base_bin sha256=$(sha256sum "$base_bin" | cut -d' ' -f1)"
  echo "head: $head_bin sha256=$(sha256sum "$head_bin" | cut -d' ' -f1)"

  local i=0
  for seed in $iseeds; do
    for side in base head; do
      local bin=$base_bin; [ "$side" = head ] && bin=$head_bin
      [ -f "$OUT/identity/$side/seed-$seed/DONE" ] && continue
      one "$seed" mixed_dc $((7440 + i % 4)) \
          "$OUT/identity/$side/seed-$seed" "$bin" "$ON" keep &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    done
  done
  wait

  for seed in $iseeds; do
    a=$OUT/identity/base/seed-$seed/scorelog.txt
    b=$OUT/identity/head/seed-$seed/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "seed $seed: MISSING scorelog"; fail=1
    elif cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      echo "seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1)); fail=1
    fi
    # Free: the same comparison against the tree the departure-cause
    # addendum measured, which says whether this is the SAME corpus or
    # merely the same venue. The branch has moved since; if these
    # differ, the corpus is a re-measurement and not a re-read.
    local old=$here/results/employ-muster/mixed_dc/seed-$seed/scorelog.txt
    if [ -r "$old" ] && [ -r "$b" ]; then
      if cmp -s "$old" "$b"; then
        echo "seed $seed: same games as employ-muster"
      else
        echo "seed $seed: DIFFERS from employ-muster (branch has moved)"
      fi
    fi
  done
  # The proof must not be able to pass because both sides are HEAD.
  if grep -rqs "deep-employpoach" "$OUT"/identity/base/; then
    echo "STOP: the base arm emitted the ledger token -- not the base." >&2
    fail=1
  fi
  echo "poach lines, head arm:" \
       "$(grep -rh 'deep-employpoach' "$OUT"/identity/head/ | wc -l)"
  echo "identity: $same/2 byte-identical, $diff_n differ"
  if [ "$fail" -eq 0 ]; then
    echo "SUCCESS identity $same/2 $(date -Is)" > "$OUT/identity/DONE-BATCH"
  else
    echo "FAIL identity $same/2 $(date -Is)" > "$OUT/identity/DONE-BATCH"
  fi
  cat "$OUT/identity/DONE-BATCH"
  return $fail
}

do_arms() {
  local bin n
  bin=$(pin "$BUILD" arms)
  n=$(echo $SEEDS | wc -w)
  echo "arms binary $bin sha256=$(sha256sum "$bin" | cut -d' ' -f1)"
  echo "on tuning $(sha256sum "$ON")"
  echo "off tuning $(sha256sum "$OFF")"
  # ON first: it is the arm the addendum measured and the one the
  # deliverable table is read from.
  echo "=== ON arm ($((n * 2)) games) $(date -Is) ==="
  arm_run "$OUT/on" "$bin" "$ON" 7400
  mark "$OUT/on" $((n * 2))
  echo "=== OFF arm ($((n * 2)) games) $(date -Is) ==="
  arm_run "$OUT/off" "$bin" "$OFF" 7410
  mark "$OUT/off" $((n * 2))
}

case "${1:-}" in
identity) mkdir -p "$OUT/identity"; do_identity;;
arms)     do_arms;;
go)
  mkdir -p "$OUT/identity"
  if ! do_identity; then
    echo "STOP: the ledger is not inert; no corpus run." >&2
    exit 1
  fi
  do_arms
  "$here/employ.py" "$OUT/on" --section poach
  "$here/employ.py" "$OUT/off" --section poach
  ;;
report)
  for a in on off; do
    echo "=== $a arm ==="
    "$here/employ.py" "$OUT/$a" --section poach
    "$here/employ.py" "$OUT/$a" --section depart
  done
  ;;
*)
  grep '^#' "$0" | sed 's/^# \{0,1\}//'
  exit 1;;
esac
