#!/usr/bin/env bash
# CHATTY slice 2 (deep LISTENS to its allies): the proofs this epic
# owes, re-runnable rather than described.
#
#   ./benchmark/chatty2.sh identity     # 10 paired smoke seeds vs the
#                                       # branch point, deep AND
#                                       # mixed_db cells, both gates 0
#   ./benchmark/chatty2.sh quiet        # gate ON, nobody says anything:
#                                       # the scorelogs must equal the
#                                       # DEAF arm's, which is the claim
#                                       # that a parser nobody talks to
#                                       # costs nothing
#   ./benchmark/chatty2.sh stage        # smoke stage A: play to T40 and
#                                       # save, so every cell below
#                                       # starts from the SAME position
#   ./benchmark/chatty2.sh smoke CELL   # load that save and play on:
#                                       #   ctl   nothing injected
#                                       #   def   only `defend <ours>`
#                                       #   atk   only `attack <city>`
#                                       #   road  only `road to <ally>`
#                                       #   all   all four + both
#                                       #         refusals (the
#                                       #         transcript cell)
#   ./benchmark/chatty2.sh warner       # FOUR deep players on one team:
#                                       # one threat must produce ONE
#                                       # warning and a nonzero yield
#                                       # counter on the others
#   ./benchmark/chatty2.sh report       # the triples and the assertions
#
# Machine budget: ports 7600-7649, jobs <= 2, this worktree's own
# build/. Every stage is RESUMABLE -- a seed directory holding a DONE
# marker is skipped -- because a tracked background shell on this box
# dies at about an hour.
#
# WHY THE PER-EFFECT CELLS. A first attempt injected all four commands
# in one cell and compared it to the control. That cannot attribute
# anything: the moment ANY decision differs the whole game diverges, so
# by the first decision turn the defended city's danger already carried
# the road request's re-aimed corridor and the attack request's
# re-ordered target. One command per cell, all from one save, is the
# only design in which the difference at the first decision turn is the
# command's.
#
# WHAT THE INJECTION IS, said plainly. A benchmark game has NO CLIENT
# CONNECTED, so no chat packet can ever arrive and the protocol could
# be reasoned about but never run. `aicmd <deep> chat <sender> <line>`
# (ALLOW_ADMIN, server/stdinhand.c) hands the line to the SAME parser
# the chat hook does. Proved through it: the parse, the alliance test
# against the real diplomatic state, the city grounding, the rate cap,
# the reply, all three prices, and both refusals. NOT proved through
# it: the attribution -- over a socket the sender is the connection's
# own player and cannot be chosen, whereas here the operator names it.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"

BASE_SHA=9af8e85966          # epic/w03-horizon-content, this branch's base
SERVER=$ROOT/build/freeciv-server
BASE_TREE=$ROOT/.chatty2-base/base-$BASE_SHA
BASE_SERVER=$BASE_TREE/build/freeciv-server
JOBS=${JOBS:-2}
SMOKE_SEEDS=${SMOKE_SEEDS:-2}
STAGE_END=${STAGE_END:-40}
SMOKE_END=${SMOKE_END:-60}
ALLY=P2                      # the deep player on our team
STRANGER=P3                  # a classic player we are NOT allied to
ME=P1

MESON_OPTS="-Dclients= -Dfcmp= -Dtools= -Dnls=false -Daudio=none
            -Dreadline=false -Daimodules=classic,tex,baseline,deep"

[ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }

# Pin the binary a run executes by copying it -- WITH libfreeciv.so,
# whose RUNPATH is $ORIGIN/ -- out of build/ before a single game
# starts, and print both shas so the pin is checkable after the fact.
pin_server() {
  pin=$ROOT/.chatty2-pin/$1
  mkdir -p "$pin"
  cp -f "$(dirname "$SERVER")/libfreeciv.so" "$pin/"
  cp -f "$SERVER" "$pin/freeciv-server"
  SERVER=$pin/freeciv-server
  echo "pinned: $SERVER"
  sha256sum "$SERVER" "$pin/libfreeciv.so" | sed 's/^/  /'
}

# The venue, copied from slice 1's smoke and for slice 1's reason: two
# deep players on one team (players on a team start at DS_TEAM, which
# pplayers_allied() answers TRUE for) so there is an alliance to be
# asked by, four classic players to be at war with, all on one
# continent so a land corridor between the allies can exist at all, on
# tech50war-200's war-forcing recipe so `attack` has a population.
venue_lines() {
  local seed=$1 endturn=$2
  cat <<VENUE
set minplayers 0
set timeout -1
set mapsize XYSIZE
set xsize 32
set ysize 32
set landmass 40
set generator FRACTAL
set startpos ALL
set teamplacement CONTINENT
set aifill 6
set techlevel 50
set startunits ccccccccxxwwwwwwwwww
set dispersion 3
create $ME deep
create $ALLY deep
create $STRANGER classic
create P4 classic
create P5 classic
create P6 classic
team $ME "Team 1"
team $ALLY "Team 1"
team $STRANGER "Team 2"
team P4 "Team 3"
team P5 "Team 4"
team P6 "Team 5"
hard
set endturn $endturn
set autosaves "GAMEOVER"
set scorelog enabled
set scoreloglevel ALL
set scorefile scorelog.txt
set gameseed $seed
set mapseed $seed
start
VENUE
}

run_game() {
  local dir=$1 port=$2

  if [ -f "$dir/DONE" ]; then
    echo "=== $dir: DONE, skipping ==="
    return 0
  fi
  echo "=== $dir on port $port ==="
  ( cd "$dir" && timeout 5400 "$SERVER" -A none -e -p "$port" \
      --read commands.serv --saves saves > game.log 2>&1 ) \
    || { echo "FAILED: $dir" >&2; return 0; }
  touch "$dir/DONE"
}

# The names the smoke has to be able to type, read out of the save
# rather than guessed: an enemy city deep has NOT seen is refused by
# design, so a guessed name would only ever prove the refusal.
pick_names() {
  local save=$1
  eval "$(python3 "$ROOT/benchmark/chatty2-names.py" "$save" "$ME")"
  MY_CITY=${OWN_CITIES%%;*}; MY_CITY=${MY_CITY#*,}
  eval "ALLY_LIST=\${CITIES_$ALLY:-}"
  ALLY_CITY=${ALLY_LIST%%;*}; ALLY_CITY=${ALLY_CITY#*,}
  # The first SEEN city whose owner we are actually AT WAR with. Not
  # "the first city of somebody who is not us": deep answers a target
  # whose owner it is at peace with with an honest refusal naming the
  # owner, so a war-blind pick proves the refusal and never the effect.
  # It did, on the first run -- `attack Sapporo` came back "we are not
  # at war with P4, who holds Sapporo", which was true.
  FOE_CITY=""; FOE_PLAYER=""
  for w in $AT_WAR; do
    FOE_CITY=$(printf '%s' "$SEEN" | tr ';' '\n' \
               | awk -F, -v w="$w" '$1 == w { print $3; exit }')
    if [ -n "$FOE_CITY" ]; then
      FOE_PLAYER=$(printf '%s' "$PLAYER_NAMES" | tr ';' '\n' \
                   | awk -F, -v w="$w" '$1 == w { print $2; exit }')
      break
    fi
  done
}

smoke_dir() { echo "$ROOT/benchmark/results/chatty2-smoke/$1/seed-$2"; }

case "${1:-}" in
identity)
  OUT=$ROOT/benchmark/results/chatty2-identity
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
  # directory on the binary, so a re-run against a rebuilt head leaves
  # two smoke-* dirs and a bare glob reads a pass as a failure. It did,
  # once, in slice 1.
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
  # deep-request is a token only this branch can write, and the head
  # arm runs with both gates SHUT, so neither side may carry one.
  if grep -rqs "deep-request" "$OUT"/base/; then
    echo "STOP: the base arm emitted an inbound token -- not the base." >&2
    exit 1
  fi
  if grep -rqs "deep-request" "$OUT"/head/; then
    echo "STOP: the head arm PARSED with chatty_inbound_gate at 0." >&2
    exit 1
  fi
  echo "provenance: neither arm carries a deep-request line, as required"
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

quiet)
  OUT=$ROOT/benchmark/results/chatty2-quiet
  pin_server quiet
  export FREECIV_DATA_PATH="$ROOT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
  port=7610
  for cell in on deaf; do
    case $cell in
      on)   ini=$ROOT/benchmark/tuning/chatty2-on.ini;;
      deaf) ini=$ROOT/benchmark/tuning/chatty2-deaf.ini;;
    esac
    export FREECIV_DEEP_TUNING=$ini
    for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                  | head -n "$SMOKE_SEEDS"); do
      d=$OUT/$cell/seed-$seed
      if [ ! -f "$d/DONE" ]; then
        rm -rf "$d"; mkdir -p "$d/saves"
        venue_lines "$seed" "$SMOKE_END" > "$d/commands.serv"
        sha256sum "$ini" "$SERVER" > "$d/key.txt"
      fi
      run_game "$d" "$port"
      port=$((port + 1))
    done
  done

  echo "=== assertion: gate ON with nobody talking == DEAF ==="
  fail=0
  for d in "$OUT"/on/seed-*; do
    s=$(basename "$d")
    a=$d/scorelog.txt; b=$OUT/deaf/$s/scorelog.txt
    if [ ! -r "$a" ] || [ ! -r "$b" ]; then
      echo "$s: MISSING"; fail=1
    elif cmp -s "$a" "$b"; then
      echo "$s: scorelogs IDENTICAL (inbound gate on, no traffic vs deaf)"
    else
      echo "$s: SCORELOGS DIFFER -- the parser is not inert"; fail=1
    fi
  done
  n=$(grep -rhc "deep-request" "$OUT"/on/seed-*/game.log 2>/dev/null \
      | paste -sd+ | bc)
  echo "inbound tokens in the gate-on cell: ${n:-0} (must be 0 -- nobody spoke)"
  [ "$fail" -eq 0 ] || { echo "STOP: not inert." >&2; exit 1; }
  ;;

stage)
  OUT=$ROOT/benchmark/results/chatty2-smoke/stage
  pin_server stage
  export FREECIV_DATA_PATH="$ROOT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
  export FREECIV_DEEP_TUNING=$ROOT/benchmark/tuning/chatty2-on.ini
  port=7620
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SMOKE_SEEDS"); do
    d=$OUT/seed-$seed
    if [ ! -f "$d/DONE" ]; then
      rm -rf "$d"; mkdir -p "$d/saves"
      venue_lines "$seed" "$STAGE_END" > "$d/commands.serv"
    fi
    run_game "$d" "$port"
    port=$((port + 1))
    ls "$d"/saves/*.sav* 2>/dev/null | tail -1
  done
  ;;

smoke)
  cell=${2:?usage: chatty2.sh smoke ctl|def|atk|road|all}
  STAGE=$ROOT/benchmark/results/chatty2-smoke/stage
  pin_server "smoke-$cell"
  export FREECIV_DATA_PATH="$ROOT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
  export FREECIV_DEEP_TUNING=$ROOT/benchmark/tuning/chatty2-on.ini
  case $cell in
    ctl)  port=7625;; def) port=7630;; atk) port=7635;;
    road) port=7640;; all) port=7645;;
    *) echo "unknown cell $cell" >&2; exit 1;;
  esac
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SMOKE_SEEDS"); do
    d=$(smoke_dir "$cell" "$seed")
    save=$(ls "$STAGE/seed-$seed"/saves/*.sav* 2>/dev/null | tail -1)
    [ -n "$save" ] || { echo "no stage save for seed $seed -- run stage" >&2
                        exit 1; }
    if [ ! -f "$d/DONE" ]; then
      rm -rf "$d"; mkdir -p "$d/saves"
      cp -f "$save" "$d/saves/"
      base=$(basename "$save")
      pick_names "$save"
      {
        echo "load saves/$base"
        echo "set endturn $SMOKE_END"
        echo "set scorelog enabled"
        echo "set scoreloglevel ALL"
        echo "set scorefile scorelog.txt"
        case $cell in
          ctl)  ;;
          def)  echo "aicmd $ME chat $ALLY defend $MY_CITY";;
          atk)  echo "aicmd $ME chat $ALLY attack $FOE_CITY";;
          road) echo "aicmd $ME chat $ALLY road to $ALLY_CITY";;
          all)
            echo "aicmd $ME chat $ALLY status"
            echo "aicmd $ME chat $ALLY defend $MY_CITY"
            echo "aicmd $ME chat $ALLY road to $ALLY_CITY"
            echo "aicmd $ME chat $ALLY attack $FOE_CITY"
            echo "aicmd $ME chat $ALLY attack $FOE_PLAYER"
            echo "aicmd $ME chat $ALLY defend $ALLY_CITY"
            echo "aicmd $ME chat $STRANGER defend $MY_CITY"
            ;;
        esac
        echo "start"
      } > "$d/commands.serv"
      { echo "my_city=$MY_CITY"; echo "ally_city=$ALLY_CITY"
        echo "foe_city=$FOE_CITY"; echo "foe_player=$FOE_PLAYER"
      } > "$d/names.txt"
      sha256sum "$SERVER" "$ROOT/benchmark/tuning/chatty2-on.ini" \
        > "$d/key.txt"
    fi
    run_game "$d" "$port"
    port=$((port + 1))
  done
  ;;

warner)
  # The Europe defect's own shape, reproduced small: FOUR deep players
  # on one team, so a threat to any one of them is visible to three
  # possible warners at once. The two-deep venue every other cell uses
  # cannot show this at all -- deep_chat_ally_threats() skips the city's
  # own owner, so with two allies there is only ever one candidate.
  OUT=$ROOT/benchmark/results/chatty2-warner
  pin_server warner
  export FREECIV_DATA_PATH="$ROOT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
  export FREECIV_DEEP_TUNING=$ROOT/benchmark/tuning/chatty2-on.ini
  port=7647
  for seed in $(grep -E '^[0-9]+$' benchmark/seeds/train.txt \
                | head -n "$SMOKE_SEEDS"); do
    d=$OUT/seed-$seed
    if [ ! -f "$d/DONE" ]; then
      rm -rf "$d"; mkdir -p "$d/saves"
      venue_lines "$seed" "$SMOKE_END" \
        | sed -e 's/^create P3 classic$/create P3 deep/' \
              -e 's/^create P4 classic$/create P4 deep/' \
              -e 's/^team P3 "Team 2"$/team P3 "Team 1"/' \
              -e 's/^team P4 "Team 3"$/team P4 "Team 1"/' \
        > "$d/commands.serv"
      sha256sum "$SERVER" > "$d/key.txt"
    fi
    run_game "$d" "$port"
    port=$((port + 1))
  done

  echo "=== assertion: one warning per (ally city, threat), not four ==="
  fail=0
  for d in "$OUT"/seed-*; do
    s=$(basename "$d")
    # Every threat line names the city and the threatening player; the
    # subject is that pair, and the pair must be spoken about ONCE
    # across the whole alliance rather than once per member.
    dup=$(grep -h 'deep-chat .*class=1 ' "$d/game.log" 2>/dev/null \
          | sed -E 's/.*text="[^ ]* \(AI\)\* Enemy force approaching your city ([^:]*):.*of ([^,]*), .*/\1|\2/' \
          | sort | uniq -c | awk '$1 > 1' | head -5)
    warn=$(grep -hc 'deep-chat .*class=1 ' "$d/game.log" 2>/dev/null || true)
    yield=$(grep -h 'deep-warner ' "$d/game.log" 2>/dev/null | wc -l)
    echo "$s: warnings=$warn yields=$yield"
    if [ -n "$dup" ]; then
      echo "$s: DUPLICATE SUBJECTS still emitted:"; echo "$dup"; fail=1
    else
      echo "$s: no (city, threat) subject warned about twice"
    fi
    [ "$yield" -gt 0 ] || { echo "$s: NO yields -- election never bound"; }
  done
  echo "=== who yielded to whom (last 10) ==="
  grep -h 'deep-warner ' "$OUT"/seed-*/game.log 2>/dev/null \
    | sed -E 's/^3: //' | tail -10 || true
  [ "$fail" -eq 0 ] || { echo "STOP: election did not bind." >&2; exit 1; }
  ;;

report)
  OUT=$ROOT/benchmark/results/chatty2-smoke
  seed=$(grep -E '^[0-9]+$' benchmark/seeds/train.txt | head -1)
  echo "=== the transcript: every line heard, and every answer ==="
  grep -h "deep-request \|deep-chat .*class=4" \
    "$OUT/all/seed-$seed/game.log" 2>/dev/null \
    | sed -E 's/^3: //; s/player="[^"]*" //' || true

  echo
  echo "=== decay: the requests retiring on their own ==="
  grep -h "deep-requests " "$OUT/all/seed-$seed/game.log" 2>/dev/null \
    | sed -E 's/^3: //' | head -30 || true

  echo
  echo "=== EFFECT 1 defend: danger for the named city, def vs ctl ==="
  city=$(grep -h "verdict=defend " "$OUT/def/seed-$seed/game.log" 2>/dev/null \
         | grep -oE 'city=[0-9]+' | head -1 | cut -d= -f2)
  echo "requested city id=$city"
  for c in def ctl; do
    printf '%-4s ' "$c"
    grep -h "deep-def .*city=$city " "$OUT/$c/seed-$seed/game.log" 2>/dev/null \
      | grep -oE 'turn=[0-9]+ |danger=[0-9]+' | paste - - | head -8 \
      | tr '\n' ' '
    echo
  done

  echo
  echo "=== EFFECT 2 attack: the operation target, atk vs ctl ==="
  city=$(grep -h "verdict=attack-city " "$OUT/atk/seed-$seed/game.log" \
         2>/dev/null | grep -oE 'city=[0-9]+' | head -1 | cut -d= -f2)
  echo "requested city id=$city"
  for c in atk ctl; do
    printf '%-4s ' "$c"
    grep -h "deep-anchor .*player=\"$ME\"" "$OUT/$c/seed-$seed/game.log" \
      2>/dev/null | grep -oE 'turn=[0-9]+|target=[0-9]+' | paste - - \
      | head -8 | tr '\n' ' '
    echo
  done

  echo
  echo "=== EFFECT 3 road: the ally corridor endpoint, road vs ctl ==="
  for c in road ctl; do
    printf '%-4s ' "$c"
    grep -h "deep-trunklink .*player=\"$ME\" .*ally=1" \
      "$OUT/$c/seed-$seed/game.log" 2>/dev/null \
      | grep -oE 'turn=[0-9]+|to=[0-9]+,[0-9]+' | paste - - | head -8 \
      | tr '\n' ' '
    echo
  done

  echo
  echo "=== rate cap: any turn over the cap? (said=n/max per line) ==="
  awk '{ if (match($0, /said=([0-9]+)\/([0-9]+)/)) {
           split(substr($0, RSTART + 5, RLENGTH - 5), a, "/");
           if (a[1] + 0 > a[2] + 0) { print "CAP BREACHED: " $0; bad++ }
         } }
       END { printf "cap breaches: %d\n", bad + 0 }' \
    "$OUT"/*/seed-*/game.log 2>/dev/null
  ;;

*)
  sed -n '2,50p' "$0"
  exit 1
  ;;
esac
