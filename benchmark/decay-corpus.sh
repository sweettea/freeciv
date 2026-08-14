#!/usr/bin/env bash
# DECAY slice 1 corpus driver: run-game.sh over N seeds with a
# concurrency cap, pinned binary, pv3 tuning, mixed_db on
# tech50war-200. Also runs seed $IDSEED on the base (pre-census)
# binary for the play-level identity check.
#
#   decay-corpus.sh --census BIN --base BIN --out DIR [--seeds "1 2.."]
#                   [--jobs N] [--port0 N]
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CENSUS= BASE= OUT= SEEDS="1 2 3 4 5 6 7 8 9 10 11 12" JOBS=6 PORT0=7500
IDSEED=1
while [ $# -gt 0 ]; do
  case "$1" in
    --census) CENSUS=$2; shift 2;;
    --base)   BASE=$2; shift 2;;
    --out)    OUT=$2; shift 2;;
    --seeds)  SEEDS=$2; shift 2;;
    --jobs)   JOBS=$2; shift 2;;
    --port0)  PORT0=$2; shift 2;;
    *) echo "unknown arg $1" >&2; exit 1;;
  esac
done
[ -n "$CENSUS" ] && [ -n "$OUT" ] || { echo "need --census/--out" >&2; exit 1; }
mkdir -p "$OUT"

export TUNING=$here/tuning/pv3.ini
CONFIG=$here/configs/tech50war-200.conf

port=$PORT0
jobs=()
launch() { # arm-label server seed
  local dir=$OUT/$1-s$3
  if [ -e "$dir/DONE" ]; then echo "skip $1-s$3 (DONE)"; return; fi
  rm -rf "$dir"
  "$here/run-game.sh" --config "$CONFIG" --seed "$3" --arm mixed_db \
      --out "$dir" --server "$2" --port "$port" \
      >"$OUT/$1-s$3.launch.log" 2>&1 &
  jobs+=($!)
  port=$((port + 1))
  while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do sleep 20; done
}

[ -n "$BASE" ] && launch base "$BASE" "$IDSEED"
for s in $SEEDS; do
  launch census "$CENSUS" "$s"
done
wait
echo ALL-DONE
ls "$OUT"/*/DONE 2>/dev/null | wc -l
