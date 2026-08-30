#!/usr/bin/env bash
# Event stream for the S-FEARSEAT batch: one line per rung that
# finishes, then one line when the driver writes its report.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$here/results/fearseat
prev=""
while true; do
  cur=$(find "$OUT" -name DONE-BATCH 2>/dev/null | sort | while read -r f; do
          d=${f%/DONE-BATCH}
          echo "${d#$OUT/} $(cat "$f")"
        done)
  comm -13 <(echo "$prev") <(echo "$cur") 2>/dev/null
  prev=$cur
  if [ -f "$OUT/report.txt" ]; then
    echo "REPORT READY"
    break
  fi
  sleep 120
done
