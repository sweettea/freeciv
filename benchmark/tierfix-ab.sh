#!/usr/bin/env bash
# S-EXPAND-FIX A/B: the two founder-arbitration gates on tech50war-200,
# seeds 37-56, in BOTH seat orderings.
#
#   tierfix-ab.sh SERVER OUTDIR [JOBS] [PORTBASE]
#
# Four arms, run in sequence so one port block is enough:
#
#   off-dc  pv3,          mixed_dc   the funnel probe's cell, re-run on
#                                    THIS binary rather than reused --
#                                    the probe's server predates the
#                                    w12h merge, so its lineage is not
#                                    this branch's and a paired claim
#                                    cannot rest on it
#   on-dc   pv3-tierfix,  mixed_dc
#   off-cd  pv3,          mixed_cd   the ordering the funnel found the
#   on-cd   pv3-tierfix,  mixed_cd   -4.56 city gap in (STATUS §7)
#
# Both orderings because the city gap is a SEAT interaction: the funnel
# measured mixed_dc -1.75 (t -0.84) against mixed_cd -4.56 (t -4.05) on
# the same games, so a single ordering samples one arm of it while still
# passing the pairing check.
#
# Each arm gets its own DONE marker, so an interrupted batch resumes at
# arm granularity and run.sh's own DONE markers resume within an arm.
# A sweeper gzips each game's log as its DONE marker appears: the funnel
# counters live in game.log and the box has under 8 GB free.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SERVER=${1:?server binary}
OUT=${2:?output dir}
JOBS=${3:-4}
PORTBASE=${4:-7410}

mkdir -p "$OUT"
seq 37 56 > "$OUT/seeds.txt"

(
  while [ ! -f "$OUT/BATCHDONE" ]; do
    for d in "$OUT"/*/*/*/seed-*; do
      [ -f "$d/DONE" ] && [ -f "$d/game.log" ] && gzip -f "$d/game.log"
    done
    sleep 30
  done
  for d in "$OUT"/*/*/*/seed-*; do
    [ -f "$d/game.log" ] && gzip -f "$d/game.log"
  done
  touch "$OUT/SWEPT"
) > "$OUT/sweeper.log" 2>&1 &

for spec in "off-dc:pv3.ini:mixed_dc" "on-dc:pv3-tierfix.ini:mixed_dc" \
            "off-cd:pv3.ini:mixed_cd" "on-cd:pv3-tierfix.ini:mixed_cd"; do
  name=${spec%%:*}
  rest=${spec#*:}
  tun=${rest%%:*}
  arm=${rest#*:}
  [ -f "$OUT/$name.DONE" ] && continue
  TUNING=$here/tuning/$tun \
    "$here/run.sh" --count 20 --seeds "$OUT/seeds.txt" \
      --config "$here/configs/tech50war-200.conf" --arms "$arm" \
      --jobs "$JOBS" --portbase "$PORTBASE" --out "$OUT/$name" \
      --server "$SERVER" > "$OUT/$name.log" 2>&1
  echo "exit=$?" > "$OUT/$name.DONE"
done
touch "$OUT/BATCHDONE"
