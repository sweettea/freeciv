#!/usr/bin/env bash
# S-POX liveness: does the settlement-shape axis actually move where
# deep puts its cities?
#
#   pox-live.sh SERVER [OUTDIR]
#
# Venue: smoke-traits.conf -- the smoke cell with 'traitdistribution
# EVEN', so the four seats draw DIFFERENT expansionist and builder
# values and the axis has something to read. Under the shipping FIXED
# distribution every seat draws the same numbers and the axis is exactly
# identity, which is correct behaviour and unmeasurable behaviour at the
# same time.
#
# Three arms, one key apart each:
#   off   trait-seeded stance, own-crowding price OFF   (control)
#   flat  the derived price, personality OFF            (does the price move spacing?)
#   pox   the derived price scaled by the shape axis    (does the personality separate seats?)
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SERVER=${1:?server binary}
OUT=${2:-$here/results/pox-live}

mkdir -p "$OUT"
: > "$OUT/progress.txt"
ok=1
for arm in off flat pox; do
  TUNING=$here/tuning/pox-$arm.ini \
  LD_LIBRARY_PATH=$(dirname "$SERVER") \
    "$here/run.sh" --count 10 --config "$here/configs/smoke-traits.conf" \
      --arms deep --jobs 3 --portbase 7420 \
      --out "$OUT/$arm" --server "$SERVER" \
      > "$OUT/$arm.log" 2>&1
  rc=$?
  echo "$arm exit=$rc" >> "$OUT/progress.txt"
  [ "$rc" -eq 0 ] || ok=0
done

if [ "$ok" -eq 1 ]; then
  echo "SUCCESS" > "$OUT/DONE"
else
  echo "FAILED" > "$OUT/FAILED"
fi
