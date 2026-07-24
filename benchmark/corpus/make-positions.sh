#!/usr/bin/env bash
# Regenerate the corpus positions. Provenance tool — the checked-in
# saves are the artefacts of record; rerunning this only matters if the
# positions must be rebuilt (e.g. savegame format change), and every
# assertion must be re-validated afterwards.
#
#   make-positions.sh [--server BIN] [--port N]
#
# Each position records its generation parameters in PROVENANCE.txt.

set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SERVER=$here/../../build/freeciv-server
PORT=5555
while [ $# -gt 0 ]; do
  case "$1" in
    --server) SERVER=$2; shift 2;;
    --port)   PORT=$2; shift 2;;
    *) echo "usage: make-positions.sh [--server BIN] [--port N]" >&2; exit 1;;
  esac
done
SERVER=$(readlink -f "$SERVER")
export FREECIV_DATA_PATH="$here/../../data${FREECIV_DATA_PATH:+:$FREECIV_DATA_PATH}"
SQ=$here/../savequery.py

# Play a headless game; artefacts in $1, remaining args are extra
# server commands emitted before "start".
playout() {
  local work=$1 endturn=$2 saveturns=$3 players=$4 seed=$5; shift 5
  mkdir -p "$work/saves"
  {
    echo "set minplayers 0"
    echo "set timeout -1"
    echo "set mapsize XYSIZE"
    echo "set xsize 32"
    echo "set ysize 32"
    echo "set aifill $players"
    echo "hard"
    echo "set endturn $endturn"
    echo "set autosaves \"TURN|GAMEOVER\""
    echo "set saveturns $saveturns"
    echo "set gameseed $seed"
    echo "set mapseed $seed"
    for extra in "$@"; do echo "$extra"; done
    echo "start"
  } > "$work/commands.serv"
  (cd "$work" && timeout 1800 "$SERVER" -A none -e -p "$PORT" \
     --read commands.serv --saves saves > game.log 2>&1)
  grep -q "Game ended" "$work/game.log"
}

provenance() {
  { echo "generated-by: make-positions.sh"
    echo "server: $("$SERVER" --version 2>/dev/null | head -1)"
    echo "$2"
  } > "$1/PROVENANCE.txt"
}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/corpus-gen-XXXXXX")
trap 'rm -rf "$tmp"' EXIT

# --- hut-rich-start: turn-1 position with a dense hut layer ----------
name=hut-rich-start seed=42
echo "== $name"
playout "$tmp/$name" 1 1 4 $seed "set xsize 64" "set ysize 64" "set huts 200"
save=$(ls "$tmp/$name"/saves/freeciv-T0001-*-auto.sav*)
huts=$("$SQ" "$save" extra-count Hut)
[ "$huts" -ge 30 ] || { echo "only $huts huts; raise setting" >&2; exit 1; }
cp "$save" "$here/$name/position.sav.zst"
provenance "$here/$name" \
  "settings: 64x64, 4 players, hard, huts 200, seed $seed, turn 1 ($huts huts)"

# --- contested-border: crowded map, neighbouring empires -------------
name=contested-border
echo "== $name"
for seed in 42 43 44 45; do
  rm -rf "$tmp/$name"
  playout "$tmp/$name" 30 30 6 $seed
  save=$(ls "$tmp/$name"/saves/freeciv-T0030-*-auto.sav*)
  dist=$("$SQ" "$save" min-cross-distance)
  if python3 -c "import sys; sys.exit(0 if float('$dist') <= 6 else 1)"; then
    cp "$save" "$here/$name/position.sav.zst"
    provenance "$here/$name" \
      "settings: 32x32, 6 players, hard, seed $seed, turn 30 (min cross-owner city distance $dist)"
    break
  fi
  echo "  seed $seed: min distance $dist > 6, trying next"
done
[ -e "$here/$name/position.sav.zst" ] || { echo "no seed qualified" >&2; exit 1; }

# --- losing-war: a war in progress with at least one city captured ---
name=losing-war
echo "== $name"
found=
for seed in 1 2 3 42 5 6; do
  rm -rf "$tmp/$name"
  playout "$tmp/$name" 140 10 4 $seed
  for save in $(ls "$tmp/$name"/saves/freeciv-T*-auto.sav* | sort); do
    caps=$("$SQ" "$save" captured | wc -l)
    warcount=$("$SQ" "$save" wars | wc -l)
    if [ "$caps" -ge 1 ] && [ "$warcount" -ge 1 ]; then
      cp "$save" "$here/$name/position.sav.zst"
      provenance "$here/$name" \
        "settings: 32x32, 4 players, hard, seed $seed, turn $("$SQ" "$save" turn) ($caps captured cities, $warcount war pairs)"
      found=1
      break 2
    fi
  done
  echo "  seed $seed: no qualifying autosave"
done
[ -n "$found" ] || { echo "no seed produced a losing-war position" >&2; exit 1; }

echo "done"
