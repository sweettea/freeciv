#!/usr/bin/env bash
# Wonder race-pass venue probe driver (session S-WONDER-race-pass).
# Sequential cells, jobs 2, ports 7600+. DONE markers make it resumable.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(dirname "$here")
out=$here/results/wonder-probe
mkdir -p "$out"

run() { # cell config tuning binary count
  local cell=$1 cfg=$2 tun=$3 bin=$4 n=$5
  echo "=== $cell $(date '+%H:%M') ==="
  TUNING="$tun" "$here/run.sh" --count "$n" \
    --config "$here/configs/$cfg" --arms deep --jobs 2 \
    --portbase 7600 --server "$root/$bin/freeciv-server" \
    --out "$out/$cell" 2>&1 | tail -4
  echo "$cell finished $(date '+%H:%M')"
}

run econ-pv3  garden2b-150.conf  "$here/tuning/pv3.ini"        stage-inst 4
run econ-base garden2b-150.conf  "$here/tuning/pv3.ini"        stage-base 4
run war-pv3   tech50war-200.conf "$here/tuning/pv3.ini"        stage-inst 4
run econ-d7   garden2b-150.conf  "$root/../../derby5/tuning-derby7.ini" stage-inst 4
touch "$out/ALLDONE"
