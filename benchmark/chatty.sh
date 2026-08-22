#!/usr/bin/env bash
# CHATTY (deep's outbound ally comms): the proofs this epic owes,
# re-runnable rather than described.
#
#   ./benchmark/chatty.sh identity     # 10 paired smoke seeds vs the
#                                      # branch point, deep AND mixed_db
#                                      # cells, chatty_gate at 0
#   ./benchmark/chatty.sh smoke CELL   # scripted alliance (two deep on
#                                      # one team), one cell per call:
#                                      #   on   chatty on
#                                      #   on2  chatty on, repeat run
#                                      #        (the determinism pair)
#                                      #   off  same background, silent
#                                      #        (the perturbation control)
#                                      #   d7   derby-7 tuning + chatty
#                                      #        (the Europe rider)
#   ./benchmark/chatty.sh report       # what was said, and the two
#                                      # scorelog assertions
#
# Machine budget: ports 7600-7649, jobs <= 2, this worktree's own
# build/. Every stage is RESUMABLE -- a seed directory holding a DONE
# marker is skipped -- because a tracked background shell on this box
# dies at about an hour and the smoke is longer than that.
#
# What the three assertions are, and why each is needed:
#
#  1. identity. chatty_gate = 0 must be the tree before this epic, byte
#     for byte. The comparison is against a binary built from the
#     branch point, not against this binary with the gate off, because
#     a binary compared with itself cannot fail.
#  2. no perturbation (smoke on vs off). Same seeds, same background
#     knobs, same binary, deep talking or deep silent: the scorelogs
#     must be identical. This is the claim that the messages are OUTPUT
#     and not a decision -- it is a stronger statement than identity at
#     defaults, because here the gate is actually open and the lines are
#     actually being emitted.
#  3. determinism (smoke on vs on2). The same run twice. A message
#     class that read uninitialised memory or iterated a hash could
#     pass 2 and still fail here.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=c19a389a6f          # epic/w03-horizon-content, this branch's base
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.chatty-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}
SMOKE_SEEDS=${SMOKE_SEEDS:-2}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts, and print both shas so the pin is checkable after the fact.
pin_server() {
  pin=$ROOT/.chatty-pin/$1
  mkdir -p "$pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$pin/"
  cp -f "$SERVER" "$pin/freeciv-server"
  SERVER=$pin/freeciv-server
  echo "pinned: $SERVER"
  sha256sum "$SERVER" "$pin/libfreeciv.so" | sed 's/^/  /'
}

# One scripted-alliance game, on a WAR-FORCING venue. Two deep players
# on one team (players on a team start at DS_TEAM, which
# pplayers_allied() answers TRUE for), four classic players to be at
# war with, all on one continent so a land corridor between the allies
# can exist at all.
#
# The war-forcing half is configs/tech50war-200.conf's recipe copied
# verbatim -- techlevel 50, startunits ccccccccxxwwwwwwwwww,
# dispersion 3, small map -- and it is here because a 40-turn probe of
# the peaceful version had deep at PEACE with every classic player by
# T21 and allied to one of them by T37. Two of the four message
# classes (a threat to an ally city, an assault launch) have no
# population in a game nobody fights, and a class that cannot fire for
# want of a war is a venue result, not a liveness result.
smoke_game() {
  local dir=$1 seed=$2 port=$3

  if [ -f "$dir/DONE" ]; then
    echo "=== $dir seed $seed: DONE, skipping ==="
    return 0
  fi
  rm -rf "$dir"; mkdir -p "$dir/saves"
  {
    echo "set minplayers 0"
    echo "set timeout -1"
    echo "set mapsize XYSIZE"
    echo "set xsize 32"
    echo "set ysize 32"
    echo "set landmass 40"
    echo "set generator FRACTAL"
    echo "set startpos ALL"
    echo "set teamplacement CONTINENT"
    echo "set aifill 6"
    echo "set techlevel 50"
    echo "set startunits ccccccccxxwwwwwwwwww"
    echo "set dispersion 3"
    echo "create P1 deep"
    echo "create P2 deep"
    echo "create P3 classic"
    echo "create P4 classic"
    echo "create P5 classic"
    echo "create P6 classic"
    echo 'team P1 "Team 1"'
    echo 'team P2 "Team 1"'
    echo 'team P3 "Team 2"'
    echo 'team P4 "Team 3"'
    echo 'team P5 "Team 4"'
    echo 'team P6 "Team 5"'
    echo "hard"
    echo "set endturn 100"
    echo "set autosaves \"GAMEOVER\""
    echo "set scorelog enabled"
    echo "set scoreloglevel ALL"
    echo "set scorefile scorelog.txt"
    echo "set gameseed $seed"
    echo "set mapseed $seed"
    echo "start"
  } > "$dir/commands.serv"
  echo "=== $dir seed $seed on port $port ==="
  ( cd "$dir" && timeout 5400 "$SERVER" -A none -e -p "$port" \
      --read commands.serv --saves saves > game.log 2>&1 ) \
    || { echo "FAILED: $dir seed $seed" >&2; return 0; }
  touch "$dir/DONE"
}

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/chatty-identity
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
    nice -n 10 ninja -C "$BASE_TREE/build" -j 4
  fi

  pin_server identity
  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7600;;
      head) bin=$SERVER;      port=7605;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep,mixed_db \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: scorelog assertion ==="
  # The NEWEST result dir per side, not a glob: run.sh keys its cell
  # directory on the binary among other things, so a second identity
  # run against a rebuilt head leaves two smoke-* dirs there and a
  # bare glob expands to both -- which reads as "10 missing" and
  # reports a pass as a failure. It did, once.
  basedir=$(ls -dt "$OUT"/base/smoke-*/ | head -1)
  headdir=$(ls -dt "$OUT"/head/smoke-*/ | head -1)
  echo "comparing $basedir against $headdir"
  fail=0
  for arm in deep mixed_db; do
    same=0; diff_n=0; missing=0
    for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                  | head -n "$SEEDS"); do
      a=$basedir/$arm/seed-$seed/scorelog.txt
      b=$headdir/$arm/seed-$seed/scorelog.txt
      if [ ! -r "$a" ] || [ ! -r "$b" ]; then
        echo "$arm seed $seed: MISSING scorelog"; missing=$((missing + 1))
      elif cmp -s "$a" "$b"; then
        same=$((same + 1))
      else
        echo "$arm seed $seed: SCORELOGS DIFFER"; diff_n=$((diff_n + 1))
      fi
    done
    echo "identity $arm: $same/$SEEDS byte-identical, $diff_n differ," \
         "$missing missing"
    [ "$same" -eq "$SEEDS" ] || fail=1
  done
  # The proof must not be able to pass because both sides are HEAD:
  # deep-chat is a token only this branch can write, and the head arm
  # runs with the gate SHUT, so neither side may carry one.
  if grep -rqs "deep-chat " "$OUT"/base/; then
    echo "STOP: the base arm emitted a chatty token -- not the base." >&2
    exit 1
  fi
  if grep -rqs "deep-chat " "$OUT"/head/; then
    echo "STOP: the head arm SPOKE with chatty_gate at 0." >&2
    exit 1
  fi
  echo "provenance: neither arm carries a deep-chat line, as required"
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

smoke)
  cell=${2:?usage: chatty.sh smoke on|on2|off|d7}
  OUT=$ROOT/benchmark/results/chatty-smoke/$cell
  case $cell in
    on)  ini=$ROOT/benchmark/tuning/chatty-on.ini;  port=7620;;
    on2) ini=$ROOT/benchmark/tuning/chatty-on.ini;  port=7625;;
    off) ini=$ROOT/benchmark/tuning/chatty-off.ini; port=7630;;
    d7)  ini=$ROOT/derby5/tuning-derby7.ini;        port=7635;;
    *) echo "unknown cell $cell" >&2; exit 1;;
  esac
  pin_server "smoke-$cell"
  export FREECIV_DATA_PATH="$ROOT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
  export FREECIV_DEEP_TUNING=$ini
  mkdir -p "$OUT"
  sha256sum "$ini" > "$OUT/key.txt"
  sha256sum "$SERVER" >> "$OUT/key.txt"
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SMOKE_SEEDS"); do
    smoke_game "$OUT/seed-$seed" "$seed" "$port"
    port=$((port + 1))
  done
  echo "=== $cell: deep-chat lines ==="
  grep -rh "deep-chat " "$OUT"/seed-*/game.log 2>/dev/null | tail -20 || true
  ;;

report)
  OUT=$ROOT/benchmark/results/chatty-smoke
  echo "=== message classes fired (0 corridor, 1 threat, 2 operation," \
       "3 pact) ==="
  for cell in on on2 off d7; do
    printf '%-4s ' "$cell"
    for k in 0 1 2 3; do
      n=$(grep -rh "deep-chat .*class=$k " "$OUT/$cell"/seed-*/game.log \
          2>/dev/null | wc -l)
      printf 'class%s=%s ' "$k" "$n"
    done
    tot=$(grep -rh "deep-chat " "$OUT/$cell"/seed-*/game.log 2>/dev/null \
          | wc -l)
    echo "total=$tot"
  done

  echo "=== rate cap: any turn over the cap? (said=n/max per line) ==="
  grep -rho "said=[0-9]*/[0-9]*" "$OUT"/{on,d7}/seed-*/game.log 2>/dev/null \
    | sort | uniq -c | sort -rn | head
  awk '{ if (match($0, /said=([0-9]+)\/([0-9]+)/)) {
           split(substr($0, RSTART + 5, RLENGTH - 5), a, "/");
           if (a[1] + 0 > a[2] + 0) { print "CAP BREACHED: " $0; bad++ }
         } }
       END { printf "cap breaches: %d\n", bad + 0 }' \
    "$OUT"/on/seed-*/game.log "$OUT"/d7/seed-*/game.log 2>/dev/null

  echo "=== assertion 2: chatty on vs off, same seeds (no perturbation) ==="
  for d in "$OUT"/on/seed-*; do
    s=$(basename "$d")
    a=$d/scorelog.txt; b=$OUT/off/$s/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "$s: MISSING"
    elif cmp -s "$a" "$b"; then
      echo "$s: scorelogs IDENTICAL (on vs off)"
    else
      echo "$s: SCORELOGS DIFFER (on vs off) -- the channel is not inert"
    fi
  done

  echo "=== assertion 3: chatty on vs on repeated (determinism) ==="
  for d in "$OUT"/on/seed-*; do
    s=$(basename "$d")
    a=$d/scorelog.txt; b=$OUT/on2/$s/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "$s: MISSING"
    elif cmp -s "$a" "$b"; then
      echo "$s: scorelogs IDENTICAL (on vs on2)"
    else
      echo "$s: SCORELOGS DIFFER (on vs on2) -- not deterministic"
    fi
  done

  echo "=== what deep actually said (verbatim, last 25) ==="
  grep -rho 'text="[^"]*"' "$OUT"/{on,d7}/seed-*/game.log 2>/dev/null \
    | tail -25 || true
  ;;

*)
  sed -n '2,40p' "$0"
  exit 1
  ;;
esac
