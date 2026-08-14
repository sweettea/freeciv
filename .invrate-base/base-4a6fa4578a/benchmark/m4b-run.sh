#!/usr/bin/env bash
# M4b tracked-background runner: relaunch run.sh until every seed in
# the arm has a DONE marker, then touch a marker of our own.
#
#   m4b-run.sh TUNING_INI SERVER_BIN COUNT CONFIG OUTDIR TAG [JOBS]
#
# A foreground run.sh block is killed on the tool timeout and the DONE
# cache is per game, so every in-flight game is lost. Looping until the
# count is reached is the recipe from findings/cm-completion-2026-08-07
# section 7.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

TUNING_INI=$1; SERVER=$2; COUNT=$3; CONFIG=$4; OUT=$5; TAG=$6
JOBS=${7:-5}
PORTBASE=${PORTBASE:-6600}

mkdir -p "$OUT"
for attempt in $(seq 1 40); do
  TUNING=$TUNING_INI "$here/run.sh" --count "$COUNT" \
    --config "$here/configs/$CONFIG.conf" --arms deep --jobs "$JOBS" \
    --server "$SERVER" --out "$OUT" --portbase "$PORTBASE" \
    >> "$OUT/$TAG.log" 2>&1
  done=$(find "$OUT" -name DONE 2>/dev/null | wc -l)
  echo "attempt $attempt: $done/$COUNT DONE" >> "$OUT/$TAG.progress"
  if [ "$done" -ge "$COUNT" ]; then
    break
  fi
done
find "$OUT" -name DONE 2>/dev/null | wc -l > "$OUT/$TAG.count"
touch "$OUT/$TAG.FINISHED"
