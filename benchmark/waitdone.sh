#!/usr/bin/env bash
# waitdone.sh DIR N [LABEL] -- poll until DIR holds N DONE markers, then
# print one line and exit. A Monitor-friendly waiter: this box's process
# table is namespaced, so "is the batch still running" is answered by
# counting DONE files and nothing else (STATUS 2026-08-23).
set -u
dir=$1
want=$2
label=${3:-batch}
while true; do
  n=$(find "$dir" -name DONE 2>/dev/null | wc -l)
  if [ "$n" -ge "$want" ]; then
    echo "$label DONE $n/$want $(date -Is)"
    exit 0
  fi
  sleep 60
done
