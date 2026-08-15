#!/usr/bin/env bash
# TECHMIL slice 2 -- identity, liveness and the mixed A/B for the
# research-goal reselection (`techgoal_deep` / `techgoal_distance_power`).
#
#   ./benchmark/techmil2.sh identity  # 10 deep + 2 mixed_dc smoke seeds
#   ./benchmark/techmil2.sh live      # 2 tech50war-200 games, gate on
#   ./benchmark/techmil2.sh ab        # 16 mirrored tech50war-200 seeds x 2 arms
#
# Machine budget for this epic: ports 7600-7649 only.
#
# LAUNCH WITH THE HARNESS'S OWN BACKGROUND FACILITY, NEVER nohup+& FROM
# A TOOL CALL (fence.sh documents the four timestamped corpses).
#
# ONE-BINARY RULE: every stage runs against one pinned copy of the head
# server (binary + libfreeciv.so, RUNPATH is $ORIGIN/), taken out of
# build/ before any game starts.
#
# The identity stage builds the base from a `git archive` of the fork
# point into .techmil2-base/ (no git worktree registered) and asserts
# scorelogs byte-identical seed by seed. The base-is-really-base guard
# differs from fence.sh's: at DEFAULTS the head emits no line the base
# lacks (the gate is off, and both trees carry the slice-1 census), so
# the discriminator is the BINARIES -- the head must contain the
# "deep-techgoal" format string and the base must not.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=1844e16efc
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.techmil2-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-4}

# Deliberately the options this worktree's build/ was configured with:
# base and head must differ in source, not configuration.
MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Ddebug=true
            -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

pin_server() {
  mkdir -p "$ROOT/.techmil2-pin"
  pin=$ROOT/.techmil2-pin/freeciv-server
  want=$(sha256sum "$SERVER" | cut -d' ' -f1)
  have=$(sha256sum "$pin" 2>/dev/null | cut -d' ' -f1 || true)
  if [ "$want" != "${have:-}" ]; then
    cp -f "$(dirname "$SERVER")/libfreeciv.so" "$ROOT/.techmil2-pin/"
    cp -f "$SERVER" "$pin"
  fi
  SERVER=$pin
  echo "pinned: $SERVER sha256=$want (recopied: $([ "$want" != "${have:-}" ] \
    && echo yes || echo no))"
}

cell() { ls -1dt "$1"/"$2"-* 2>/dev/null | head -n 1; }

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/techmil2-identity
  SEEDS=10
  echo "=== identity stage a: base binary for $BASE_SHA ==="
  if [ -x "$BASE_SERVER" ]; then
    echo "cached: $BASE_SERVER"
  else
    rm -rf "$BASE_TREE"
    mkdir -p "$BASE_TREE"
    git archive "$BASE_SHA" | tar -x -C "$BASE_TREE"
    # shellcheck disable=SC2086
    meson setup "$BASE_TREE/build" "$BASE_TREE" $MESON_OPTS
    nice -n 10 ninja -C "$BASE_TREE/build" -j "$JOBS"
  fi

  pin_server
  echo "=== identity guard: the binaries must differ where the branch says ==="
  grep -aqm1 'deep-techgoal' "$SERVER" || {
    echo "STOP: head binary lacks the deep-techgoal format string." >&2
    exit 1; }
  if grep -aqm1 'deep-techgoal' "$BASE_SERVER"; then
    echo "STOP: base binary CONTAINS deep-techgoal -- base is the branch" >&2
    echo "      and the proof would be void." >&2
    exit 1
  fi

  # Both cells at DEFAULTS (identity.ini): the byte-identity claim is
  # "at defaults, including one mixed cell", and identity.ini is the
  # compiled-defaults control the A/B also uses.
  echo "=== identity stage b: base vs head, same seeds both sides ==="
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7600;;
      head) bin=$SERVER;      port=7610;;
    esac
    TUNING=$ROOT/benchmark/tuning/identity.ini \
      ./benchmark/run.sh --count "$SEEDS" --arms deep \
        --config benchmark/configs/smoke.conf \
        --server "$bin" --jobs "$JOBS" --portbase "$port" \
        --out "$OUT/$side"
    TUNING=$ROOT/benchmark/tuning/identity.ini \
      ./benchmark/run.sh --count 2 --arms mixed_dc \
        --config benchmark/configs/smoke.conf \
        --server "$bin" --jobs 2 --portbase $((port + 5)) \
        --out "$OUT/$side-mixed"
  done

  echo "=== identity stage c: scorelog assertion ==="
  same=0; diff_n=0; missing=0; total=0
  check() { # base_cell head_cell arm seed
    total=$((total + 1))
    a=$1/$3/seed-$4/scorelog.txt
    b=$2/$3/seed-$4/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "$3 seed $4: MISSING scorelog"; missing=$((missing + 1))
    elif cmp -s "$a" "$b"; then
      same=$((same + 1))
    else
      echo "$3 seed $4: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
    fi
  }
  bc=$(cell "$OUT/base" smoke); hc=$(cell "$OUT/head" smoke)
  bm=$(cell "$OUT/base-mixed" smoke); hm=$(cell "$OUT/head-mixed" smoke)
  echo "base cell: $bc / $bm"
  echo "head cell: $hc / $hm"
  [ -n "$bc" ] && [ -n "$hc" ] && [ -n "$bm" ] && [ -n "$hm" ] || {
    echo "STOP: could not resolve all four cells." >&2; exit 1; }
  [ "$bc" != "$hc" ] && [ "$bm" != "$hm" ] || {
    echo "STOP: a side resolved to the SAME cell as the other." >&2; exit 1; }
  i=0
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -n "$SEEDS"); do
    check "$bc" "$hc" deep "$seed"
    i=$((i + 1))
    [ "$i" -le 2 ] && check "$bm" "$hm" mixed_dc "$seed"
  done
  echo "identity: $same/$total byte-identical, $diff_n differ, $missing missing"
  [ "$same" -eq "$total" ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

live)
  # Two games price nothing (rule 6) -- this reads the counter only:
  # does the reselection run, and does differ= ever come back 1.
  OUT=$ROOT/benchmark/results/techmil2-live
  pin_server
  echo "=== liveness: 2 tech50war-200 mixed_dc games, gate on ==="
  TUNING=$ROOT/benchmark/tuning/techgoal-on.ini \
    ./benchmark/run.sh --count 2 --arms mixed_dc \
      --config benchmark/configs/tech50war-200.conf \
      --server "$SERVER" --jobs 2 --portbase 7620 --out "$OUT"
  grep -ho 'deep-techgoal .*' "$OUT"/tech50war-200-*/mixed_dc/seed-*/game.log \
    | head -5 || echo "NO deep-techgoal LINES -- the gate never fired"
  ./benchmark/techgoal-report.py "$(cell "$OUT" tech50war-200)"
  ;;

ab)
  # The brief's arm: tech50war-200 mixed, >= 16 mirrored seeds, gate
  # off (identity.ini -- the tuning slice 1 measured the gap under) vs
  # gate on at power 1.0, one binary. tune.py --mixed runs both seat
  # mirrors per seed so seat bias cancels; --report-holdout forces the
  # holdout seed file. Arms run SEQUENTIALLY: the 7600-7649 block
  # cannot hold two arms' 32 consecutive ports at once.
  OUT=$ROOT/benchmark/results/techmil2-ab
  COUNT=${COUNT:-16}
  pin_server
  mkdir -p "$OUT"
  for arm in off on; do
    case $arm in
      off) ini=$ROOT/benchmark/tuning/identity.ini;   port=7600;;
      on)  ini=$ROOT/benchmark/tuning/techgoal-on.ini; port=7618;;
    esac
    echo "=== A/B arm $arm: $ini ==="
    ./benchmark/tune.py --report-holdout "$ini" --mixed \
      --config benchmark/configs/tech50war-200.conf \
      --count "$COUNT" --jobs "$JOBS" --server "$SERVER" \
      --portbase "$port" --out "$OUT/$arm" 2>&1 | tee "$OUT/$arm.log"
  done
  ./benchmark/fencecmp.py "$OUT/off/holdout-report" "$OUT/on/holdout-report"
  ./benchmark/techgoal-report.py "$OUT/off/holdout-report" \
    "$OUT/on/holdout-report"
  touch "$OUT/ALLDONE"
  ;;

*)
  echo "usage: $0 {identity|live|ab}" >&2; exit 1;;
esac
