#!/usr/bin/env bash
# Poll helper for the S-FEARSINK batch: block until the results tree
# holds at least N DONE markers, then print the per-cell counts.
#
#   ./benchmark/sinkwait.sh N
#
# Exists because this box namespaces the process table, so "is the
# batch alive?" is answered by DONE counts and by a game log's last
# logged turn, never by looking for a server process.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$here/results/fearsink
want=${1:-1}

while [ "$(find "$OUT" -name DONE 2>/dev/null | wc -l)" -lt "$want" ]; do
  sleep 60
done
date -Is
for d in "$OUT"/*/*/; do
  [ -d "$d" ] || continue
  printf '%-40s %s\n' "${d#"$OUT"/}" \
         "$(find "$d" -name DONE 2>/dev/null | wc -l)"
done
du -sh "$OUT"
df -h /home | tail -1
