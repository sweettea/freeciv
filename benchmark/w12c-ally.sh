#!/usr/bin/env bash
# W12 slice 3 (allied cities as trunk endpoints): the proofs this epic
# owes, re-runnable rather than described.
#
#   ./benchmark/w12c-ally.sh identity    # 10 paired smoke seeds vs the
#                                        # branch point, deep AND
#                                        # mixed_db cells, gate at 0
#   ./benchmark/w12c-ally.sh venue [N]   # does a STANDARD benchmark
#                                        # venue ever give deep an ally?
#   ./benchmark/w12c-ally.sh smoke [N]   # scripted alliance (teams):
#                                        # the liveness proof
#
# Machine budget: jobs 2, ports 7900-7949, this worktree's own build/.
#
# The venue problem, stated because it decides what this epic can
# claim. The mechanism needs deep to HAVE an ally, and nothing in the
# benchmark corpus guarantees one: a knob-on run in a venue where no
# alliance ever forms is off-path identity, not a null result, and must
# not be reported as evidence either way. So there are two stages. The
# `venue` stage asks the standard lategame-80 cell whether alliances
# involving deep occur inside its 80 turns -- one grep for a
# deep-trunkally line with links>0. The `smoke` stage does not ask: it
# SCRIPTS the alliance with the server's own `team` command (players on
# one team start at DS_TEAM, which pplayers_allied() answers TRUE for,
# srv_main.c:3481-3492) and startpos=ALL so the two are on the same
# landmass and a land corridor can exist at all.
#
# The smoke's criterion is not just a counter. It ends by reading the
# FINAL SAVEGAME and finding a tile that (a) a deep-trunkpick line
# named as an ally-corridor task, (b) is not owned by the deep player
# who worked it, and (c) carries a road at game end. That is the claim
# in the brief -- a road actually appearing on a corridor tile outside
# deep's borders -- checked against the game state rather than against
# deep's own log of its intentions.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=31b947ffc7          # epic/w03-horizon-content, this branch's base
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.w12c-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}

MESON_OPTS="-Dclients=stub -Dfcmp=cli -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary a run executes by copying it (with libfreeciv.so --
# RUNPATH is $ORIGIN/) out of build/ before a single game starts. Per
# stage, because pinning two stages to one path is how a live server
# gets its text pages replaced underneath it (STATUS 2026-08-13).
pin_server() {
  pin=$ROOT/.w12c-pin/$1
  mkdir -p "$pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$pin/"
  cp -f "$SERVER" "$pin/freeciv-server"
  SERVER=$pin/freeciv-server
  echo "pinned: $SERVER sha256=$(sha256sum "$SERVER" | cut -d' ' -f1)"
}

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/w12c-identity
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

  pin_server "$1"
  echo "=== identity stage b: 10 paired smoke seeds, base vs head ==="
  export TUNING=$ROOT/benchmark/tuning/identity.ini
  for side in base head; do
    case $side in
      base) bin=$BASE_SERVER; port=7900;;
      head) bin=$SERVER;      port=7910;;
    esac
    ./benchmark/run.sh --count "$SEEDS" --arms deep,mixed_db \
      --config benchmark/configs/smoke.conf \
      --server "$bin" --jobs "$JOBS" --portbase "$port" \
      --out "$OUT/$side"
  done
  unset TUNING

  echo "=== identity stage c: scorelog assertion ==="
  fail=0
  for arm in deep mixed_db; do
    same=0; diff_n=0; missing=0
    for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                  | head -n "$SEEDS"); do
      a=$(echo "$OUT"/base/smoke-*/"$arm"/seed-"$seed"/scorelog.txt)
      b=$(echo "$OUT"/head/smoke-*/"$arm"/seed-"$seed"/scorelog.txt)
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

  # The proof must not be able to pass because both sides are HEAD.
  # ally=  on the pick line is a token only this branch can write.
  if grep -rqs "deep-trunkally\|deep-trunkpick.*ally=" "$OUT"/base/; then
    echo "STOP: the base arm emitted a slice-3 token -- not the base." >&2
    exit 1
  fi
  echo "provenance: base logs carry no slice-3 token, as required"
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

venue)
  # Does a standard cell ever hand deep an ally? lategame-80 is the
  # venue slices 1 and 2 measured on, so a yes here would mean the
  # score A/B has somewhere to run. Read the answer, do not assume it.
  OUT=$ROOT/benchmark/results/w12c-venue
  SEEDS=${2:-4}
  pin_server "$1"
  TUNING=$ROOT/benchmark/tuning/w12-ally-on.ini \
    ./benchmark/run.sh --count "$SEEDS" --arms deep \
      --config benchmark/configs/lategame-80.conf \
      --server "$SERVER" --jobs "$JOBS" --portbase 7920 --out "$OUT"
  echo "=== deep-trunkally lines (links>0 means deep had an ally) ==="
  grep -rh "deep-trunkally" "$OUT"/lategame-80-*/deep/seed-*/game.log \
    2>/dev/null | tail -20 || true
  n=$(grep -rhc "deep-trunkally" "$OUT"/lategame-80-*/deep/seed-*/game.log \
      2>/dev/null | paste -sd+ | bc 2>/dev/null || echo 0)
  echo "deep-trunkally lines in $SEEDS lategame-80 games: ${n:-0}"
  ;;

smoke)
  # The scripted-alliance liveness proof. Own commands.serv, because
  # `team` has to run AFTER `create` and run-game.sh emits EXTRA_SERV
  # before it.
  OUT=$ROOT/benchmark/results/w12c-smoke
  SEEDS=${2:-3}
  pin_server "$1"
  export FREECIV_DATA_PATH="$ROOT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
  export FREECIV_DEEP_TUNING=$ROOT/benchmark/tuning/w12-ally-on.ini
  port=7930
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SEEDS"); do
    dir=$OUT/seed-$seed
    rm -rf "$dir"; mkdir -p "$dir/saves"
    {
      echo "set minplayers 0"
      echo "set timeout -1"
      echo "set mapsize XYSIZE"
      echo "set xsize 32"
      echo "set ysize 32"
      echo "set landmass 40"
      echo "set generator FRACTAL"
      # startpos ALL puts every player on ONE continent. teamplacement
      # CONTINENT alone is best-effort and did not deliver it: a trial
      # at 40x40 SINGLE seated the two team mates 25 tiles apart on
      # separate landmasses, the ally node was correctly refused for
      # want of a shared continent, and the stage proved nothing.
      echo "set startpos ALL"
      echo "set teamplacement CONTINENT"
      echo "set aifill 4"
      echo "create P1 deep"
      echo "create P2 deep"
      echo "create P3 classic"
      echo "create P4 classic"
      # The alliance, scripted: one team, hence DS_TEAM at start, which
      # is what pplayers_allied() answers TRUE for.
      # Team names are ruleset names ("Team 1"..), not free text: the
      # server answers "No such team alpha" and plays on unallied.
      echo 'team P1 "Team 1"'
      echo 'team P2 "Team 1"'
      echo 'team P3 "Team 2"'
      echo 'team P4 "Team 3"' 
      echo "hard"
      echo "set endturn 80"
      echo "set autosaves \"TURN|GAMEOVER\""
      echo "set saveturns 1"
      echo "set scorelog enabled"
      echo "set scoreloglevel ALL"
      echo "set scorefile scorelog.txt"
      echo "set gameseed $seed"
      echo "set mapseed $seed"
      echo "start"
    } > "$dir/commands.serv"
    echo "=== smoke seed $seed on port $port ==="
    ( cd "$dir" && timeout 2700 "$SERVER" -A none -e -p "$port" \
        --read commands.serv --saves saves > game.log 2>&1 ) \
      || { echo "FAILED: seed $seed (see $dir/game.log)" >&2; }
    port=$((port + 1))
  done

  echo "=== deep-trunkally census (links / gap / offered / picked) ==="
  grep -rh "deep-trunkally" "$OUT"/seed-*/game.log 2>/dev/null | tail -25 \
    || true
  echo "=== ally corridor picks ==="
  grep -rh "deep-trunkpick.*ally=1" "$OUT"/seed-*/game.log 2>/dev/null \
    | tail -15 || true
  echo "=== road-on-the-ground check ==="
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SEEDS"); do
    ./benchmark/w12c-roadcheck.py "$OUT/seed-$seed" || true
  done
  ;;

*)
  echo "usage: $0 {identity|venue [seeds]|smoke [seeds]}" >&2; exit 1;;
esac
