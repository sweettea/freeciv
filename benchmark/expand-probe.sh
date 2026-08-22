#!/usr/bin/env bash
# The founder-funnel probe (S-EXPAND): tech50war-200 mixed deep-vs-classic
# on seeds 37-56, with ai/deep's metric log kept so the deep-expand funnel
# can be read per player per turn.
#
#   expand-probe.sh SERVER OUTDIR [JOBS] [PORTBASE]
#
# game.log is the artefact here, and a 200-turn eight-player game writes a
# large one. A sweeper compresses each game's log as soon as its DONE
# marker appears, so peak disk is bounded by JOBS uncompressed logs rather
# than by the whole batch -- the box this runs on has under 10 GB free.
# expandfunnel.py reads game.log.gz directly.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SERVER=${1:?server binary}
OUT=${2:?output dir}
JOBS=${3:-3}
PORTBASE=${4:-7420}

mkdir -p "$OUT"
seq 37 56 > "$OUT/seeds.txt"

# Compress finished logs while the batch runs.
(
  while [ ! -f "$OUT/BATCHDONE" ]; do
    for d in "$OUT"/*/mixed_dc/seed-*; do
      [ -f "$d/DONE" ] && [ -f "$d/game.log" ] && gzip -f "$d/game.log"
    done
    sleep 30
  done
  for d in "$OUT"/*/mixed_dc/seed-*; do
    [ -f "$d/game.log" ] && gzip -f "$d/game.log"
  done
  touch "$OUT/SWEPT"
) > "$OUT/sweeper.log" 2>&1 &

TUNING=${TUNING:-$here/tuning/pv3.ini} \
  "$here/run.sh" --count 20 --seeds "$OUT/seeds.txt" \
    --config "$here/configs/tech50war-200.conf" --arms mixed_dc \
    --jobs "$JOBS" --portbase "$PORTBASE" --out "$OUT" --server "$SERVER" \
    > "$OUT/batch.log" 2>&1
echo "run.sh exit=$?" >> "$OUT/batch.log"
touch "$OUT/BATCHDONE"
