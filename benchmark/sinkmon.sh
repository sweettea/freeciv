#!/usr/bin/env bash
# Emit one line per completed batch stage of the S-FEARSINK run, and
# one whenever the DONE count crosses a multiple of 20. Exits when the
# driver writes DONE-ALL. For the Monitor tool: stdout is the event
# stream, so it must stay thin.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$here/results/fearsink
prev=-1
while true; do
  n=$(find "$OUT" -name DONE 2>/dev/null | wc -l)
  b=$((n / 20))
  if [ "$b" -ne "$prev" ]; then
    echo "fearsink: $n/203 games done $(date -Is)"
    prev=$b
  fi
  if [ -f "$OUT/DONE-ALL" ]; then
    echo "fearsink: ALLDONE $n/203 $(date -Is)"
    exit 0
  fi
  sleep 60
done
