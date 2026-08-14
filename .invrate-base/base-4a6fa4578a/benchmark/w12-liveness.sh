#!/usr/bin/env bash
# W12 liveness: two lategame-80 games with both new knobs on, and the
# same two seeds with both at their shipping zero. The on/off pair is
# what makes the behaviour metric readable -- deep-worklocal's
# meanwalk= only means something next to the same seed's meanwalk with
# the discount absent.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$here/results/w12-live
NEW=${1:?server binary}

mkdir -p "$OUT"

TUNING=$here/tuning/w12-on.ini "$here/run.sh" --count 2 \
  --config "$here/configs/lategame-80.conf" --arms deep --jobs 2 \
  --portbase 7200 --out "$OUT/on" --server "$NEW" > "$OUT/on.log" 2>&1
echo "on exit=$?" >> "$OUT/progress.txt"

"$here/run.sh" --count 2 --config "$here/configs/lategame-80.conf" \
  --arms deep --jobs 2 --portbase 7200 --out "$OUT/off" --server "$NEW" \
  > "$OUT/off.log" 2>&1
echo "off exit=$?" >> "$OUT/progress.txt"

touch "$OUT/ALLDONE"
