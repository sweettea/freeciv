#!/usr/bin/env bash
# Run one headless, deterministic Freeciv game and leave its artefacts
# in an output directory.
#
#   run-game.sh --config FILE --seed N --arm AITYPE --out DIR
#               [--server BIN] [--port N]
#
# Parallel invocations must use distinct --port values: every server
# binds a TCP port and simultaneous binds to the same one fail.
#
# Determinism: mapseed and gameseed are both set to --seed, so two runs
# with the same config, seed and arm are identical, and two arms with the
# same seed play on the same map (verified via maphash.txt, a hash of the
# terrain rows of the turn-1 autosave; an explicit `save` in the command
# script would run in pregame, before the map exists).
#
# Artefacts in DIR:
#   game.log       server output
#   scorelog.txt   SCORELOG2 per-turn statistics for every player
#   maphash.txt    sha256 of the turn-1 autosave's terrain rows
#   commands.serv  the exact server commands used
#   DONE           written last; presence marks a completed run
#   saves/         turn-1 + final savegames (intermediates pruned)

set -uo pipefail

usage() { grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

CONFIG= SEED= ARM= OUT= SERVER= PORT=5555
while [ $# -gt 0 ]; do
  case "$1" in
    --config) CONFIG=$2; shift 2;;
    --seed)   SEED=$2; shift 2;;
    --arm)    ARM=$2; shift 2;;
    --out)    OUT=$2; shift 2;;
    --server) SERVER=$2; shift 2;;
    --port)   PORT=$2; shift 2;;
    *) usage;;
  esac
done
[ -n "$CONFIG" ] && [ -n "$SEED" ] && [ -n "$ARM" ] && [ -n "$OUT" ] || usage

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${SERVER:=$here/../build/freeciv-server}"
SERVER=$(readlink -f "$SERVER")
[ -x "$SERVER" ] || { echo "server not found: $SERVER" >&2; exit 1; }

# Config values (see configs/*.conf): PLAYERS, XSIZE, YSIZE, ENDTURN
# required; SKILL, RULESET, TIMEOUT_S optional.
SKILL=hard RULESET= TIMEOUT_S=1800
. "$(readlink -f "$CONFIG")"
for v in PLAYERS XSIZE YSIZE ENDTURN; do
  [ -n "${!v:-}" ] || { echo "config missing $v" >&2; exit 1; }
done

mkdir -p "$OUT/saves"
OUT=$(readlink -f "$OUT")

{
  [ -n "$RULESET" ] && echo "rulesetdir $RULESET"
  echo "set minplayers 0"
  echo "set timeout -1"
  echo "set xsize $XSIZE"
  echo "set ysize $YSIZE"
  echo "set aifill $PLAYERS"
  for i in $(seq 1 "$PLAYERS"); do
    echo "create P$i $ARM"
  done
  echo "$SKILL"
  echo "set endturn $ENDTURN"
  echo "set autosaves \"TURN|GAMEOVER\""
  echo "set saveturns 1"
  echo "set scorelog enabled"
  echo "set scoreloglevel ALL"
  echo "set scorefile scorelog.txt"
  echo "set gameseed $SEED"
  echo "set mapseed $SEED"
  echo "start"
} > "$OUT/commands.serv"

# scorefile is written relative to the server's cwd; run from OUT so
# parallel games never share files. The uninstalled server finds its
# rulesets relative to cwd, so point it back at the source data dir.
export FREECIV_DATA_PATH="$here/../data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
if ! (cd "$OUT" \
      && timeout "$TIMEOUT_S" "$SERVER" -A none -e -p "$PORT" \
           --read commands.serv --saves saves > game.log 2>&1); then
  echo "FAILED: config=$CONFIG seed=$SEED arm=$ARM (see $OUT/game.log)" >&2
  exit 1
fi

if ! grep -q "Game ended" "$OUT/game.log"; then
  echo "FAILED: game did not reach its end: $OUT/game.log" >&2
  exit 1
fi

initial=$(ls "$OUT"/saves/freeciv-T0001-*-auto.sav* 2>/dev/null | head -1)
if [ -z "$initial" ]; then
  echo "FAILED: no turn-1 autosave for map hash: $OUT" >&2
  exit 1
fi
hash=$(zstdcat "$initial" | grep -E '^t[0-9]+=' | sha256sum | cut -d' ' -f1)
if [ "$hash" = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" ]; then
  echo "FAILED: turn-1 save has no terrain rows: $initial" >&2
  exit 1
fi
echo "$hash" > "$OUT/maphash.txt"

# Keep the turn-1 save (map identity) and the final save; prune the rest.
find "$OUT/saves" -name 'freeciv-T*-auto.sav*' ! -name 'freeciv-T0001-*' \
  -delete

touch "$OUT/DONE"
