#!/usr/bin/env bash
# CAPITAL STAKE (S-CAPSTAKE) — the three probes this epic's claims rest
# on. Launch detached from the worktree root, one subcommand at a time:
#
#   setsid bash ./benchmark/capstake.sh derive     # the DERIVATION corpus
#   setsid bash ./benchmark/capstake.sh identity   # gate off == before
#   setsid bash ./benchmark/capstake.sh corpus     # the liveness pair
#   setsid bash ./benchmark/capstake.sh liveness   # the venue cell
#
#   derive     loads the user's Europe save (play-0822-2253, T120, the
#              five turns before Deep_Osmanli's capital fell) on a
#              gate-ON binary and plays four turns, so the stake's own
#              arithmetic is printed for the position the epic exists
#              because of. Nothing is tuned from it — it is the corpus
#              the STATUS derivation quotes.
#   identity   10 smoke seeds x {deep, mixed_dc} on the BASE binary and
#              on this branch's, at compiled defaults (gate 0). Every
#              scorelog must match byte for byte.
#   corpus     the same save played on TWICE, gate off and gate on, one
#              pinned binary and one key apart. The two arms are the
#              identical game up to T120 by construction, which makes
#              this the tightest pair available and puts it on the only
#              position known to contain the defect. Read the capital's
#              garrison column (deep-def ndef= where capital=1) on both.
#   liveness   the same shape on a benchmark war venue, for a cell that
#              is not one position. Needs an empire at civilwarsize
#              (10) before the stake can price anything at all.
#
# Environment: BIN the pinned gate-capable server (default
# .pin-bin/stake/freeciv-server), BASE the pre-epic pin, JOBS (<= 3 on
# this box), PORT0 (7600-7649 is this session's block). Every stage
# writes a DONE marker recording SUCCESS or FAIL.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$WT"

BIN=${BIN:-$WT/.pin-bin/stake/freeciv-server}
BASE=${BASE:-$WT/.pin-bin/base/freeciv-server}
JOBS=${JOBS:-3}
PORT0=${PORT0:-7600}
RES=$WT/benchmark/results
CORPUS=${CORPUS:-/home/sweettea/freeciv/play-0822-2253/saves/freeciv-T0120-Y00380-auto.sav.zst}

export FREECIV_DATA_PATH="$WT/data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"

[ -x "$BIN" ] || { echo "no server at $BIN" >&2; exit 1; }
echo "stake binary $(sha256sum "$BIN")"

mark() { # dir verdict
  echo "$2" > "$1/DONE-CAPSTAKE"
  echo "=== $1: $2 ==="
}

# ----------------------------------------------------------- corpus replay
# One arm of the corpus cell: the user's T120 position played on with
# the gate at $2, on port $3, to endturn $4, into directory $1. Both
# arms are the same game up to T120 by construction, so the pair is as
# tight as a paired seed and it is the position the epic exists for.
replay() {
  local out=$1 gate=$2 port=$3 endturn=$4
  local save=$out/game.sav

  rm -rf "$out"; mkdir -p "$out/saves"
  zstdcat "$CORPUS" > "$save" || { mark "$out" FAIL; return 1; }
  # The user's own tuning with the gate set HERE, so the probe cannot
  # be mistaken for the rider and cannot be broken by it. The filter is
  # not decoration: once the rider landed in derby5/tuning-derby7.ini,
  # a plain append produced the key TWICE, secfile_load refused the
  # WHOLE file, and both arms silently ran on compiled defaults with no
  # derby-7 tuning and no gate. The awk below is the check that catches
  # it; it must print nothing.
  { grep -v '^[[:space:]]*capital_stake_gate' \
         "$WT/derby5/tuning-derby7.ini"
    echo "capital_stake_gate = $gate"
  } > "$out/tuning.ini"
  awk -F= '/^[a-z]/{gsub(/ /,"",$1); if(seen[$1]++){print "DUP:"$1}}' \
      "$out/tuning.ini"
  # The corpus is a game with a HUMAN in it, and a loaded human seat
  # with nobody connected stalls the turn forever (the first attempt
  # sat at the "> " prompt with T120 loaded and never advanced). Every
  # seat with a real username is handed to its AI so the position can
  # be played on; the seat we read is Deep_Osmanli's, which was already
  # deep's.
  # Every `set` goes AFTER the load, because a savegame RESTORES server
  # settings: this corpus was played with a human in seat 12, so it
  # carries minplayers=1 and timeout=0 ("wait for the human"), and a
  # headless server that sets those before loading has them overwritten
  # and then sits at the prompt forever. That is what the first two
  # attempts did.
  { echo "load $save"
    zstdcat "$CORPUS" \
      | awk -F'"' '/^name=/{n=$2} /^username=/{if ($2 != "Unassigned")
                                                 print "aitoggle \"" n "\""}'
    echo "set minplayers 0"
    echo "set timeout -1"
    echo "set endturn $endturn"
    echo "set autosaves \"GAMEOVER\""
    echo "start"
  } > "$out/commands.serv"
  sha256sum "$BIN" "$out/tuning.ini" > "$out/key.txt"
  ( cd "$out" \
    && FREECIV_DEEP_TUNING="$out/tuning.ini" \
       timeout 7200 "$BIN" -A none -e -p "$port" \
       --read commands.serv --saves saves > game.log 2>&1 )
  rm -f "$out/game.sav" "$out"/saves/*.sav*
  if grep -aq "turn=$endturn" "$out/game.log"; then
    mark "$out" SUCCESS
  else
    mark "$out" FAIL
  fi
}

# ---------------------------------------------------------------- derive
derive() {
  local out=$RES/capstake-derive

  replay "$out" 1 "$PORT0" "${ENDTURN:-124}"
  grep -a "deep-capstake " "$out/game.log" | head -40
}

# ------------------------------------------------------------ corpus cell
# The liveness pair: the same position, one key apart, far enough past
# T120 for a starved capital to have been able to fix itself.
corpus() {
  local out=$RES/capstake-corpus

  mkdir -p "$out"
  replay "$out/off" 0 "$PORT0" "${ENDTURN:-135}" &
  replay "$out/on"  1 $((PORT0 + 1)) "${ENDTURN:-135}" &
  wait
  python3 "$WT/benchmark/capstake.py" "$out" | tee "$out/report.txt"
  if [ -f "$out/off/DONE-CAPSTAKE" ] && [ -f "$out/on/DONE-CAPSTAKE" ]; then
    mark "$out" SUCCESS
  else
    mark "$out" FAIL
  fi
}

# -------------------------------------------------------------- identity
identity() {
  local out=$RES/capstake-identity rc=0

  mkdir -p "$out"
  for side in base stake; do
    local bin=$BASE
    [ "$side" = stake ] && bin=$BIN
    for arm in deep mixed_dc; do
      "$WT/benchmark/run.sh" --count 10 --config "$WT/benchmark/configs/smoke.conf" \
        --arms "$arm" --jobs "$JOBS" --portbase $((PORT0 + 10)) \
        --out "$out/$side-$arm" --server "$bin" \
        > "$out/$side-$arm.log" 2>&1 || rc=1
    done
  done
  # Byte comparison, per seed, per arm.
  local same=0 diff=0
  for arm in deep mixed_dc; do
    for a in "$out/base-$arm"/*/"$arm"/seed-*/scorelog.txt; do
      [ -f "$a" ] || continue
      local seed=$(basename "$(dirname "$a")")
      local b=$(echo "$out/stake-$arm"/*/"$arm"/"$seed"/scorelog.txt)
      if [ -f "$b" ] && cmp -s "$a" "$b"; then
        same=$((same + 1))
      else
        diff=$((diff + 1)); echo "DIFF $arm $seed"
      fi
    done
  done
  echo "identity: $same identical, $diff different"
  [ "$diff" -eq 0 ] && [ "$same" -gt 0 ] && [ "$rc" -eq 0 ] \
    && mark "$out" SUCCESS || mark "$out" FAIL
}

# -------------------------------------------------------------- liveness
liveness() {
  local out=$RES/capstake-liveness
  local seeds=${SEEDS:-$(grep -E '^[0-9]+$' "$WT/benchmark/seeds/train.txt" | head -4)}

  mkdir -p "$out"
  cp -f "$WT/benchmark/tuning/capstake-off.ini" "$out/off.ini"
  cp -f "$WT/benchmark/tuning/capstake-on.ini" "$out/on.ini"
  local i=0
  for seed in $seeds; do
    for arm in off on; do
      local d=$out/$arm/seed-$seed
      [ -f "$d/DONE" ] && continue
      rm -rf "$d"; mkdir -p "$d"
      ( TUNING="$out/$arm.ini" \
        "$WT/benchmark/run-game.sh" \
          --config "$WT/benchmark/configs/tech50war-200.conf" \
          --seed "$seed" --arm deep --out "$d" --server "$BIN" \
          --port $((PORT0 + 20 + i % 10)) > /dev/null 2>&1 ) &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    done
  done
  wait
  local done_n
  done_n=$(bfs "$out" -name DONE 2>/dev/null | wc -l)
  echo "liveness games complete: $done_n"
  python3 "$WT/benchmark/capstake.py" "$out" | tee "$out/report.txt"
  [ "$done_n" -gt 0 ] && mark "$out" SUCCESS || mark "$out" FAIL
}

case "${1:-}" in
  derive)   derive;;
  identity) identity;;
  corpus)   corpus;;
  liveness) liveness;;
  go)       derive; identity; corpus;;
  *) echo "usage: capstake.sh derive|identity|corpus|liveness|go" >&2
     exit 1;;
esac
