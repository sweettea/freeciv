#!/usr/bin/env bash
# Durable supervisor for the S-EXPAND-FIX A/B.
#
#   setsid bash ./benchmark/tierfix-supervisor.sh </dev/null >ab.log 2>&1 &
#
# Launched from the worktree root, detached, it outlives the agent
# session that started it -- which is the failure this exists for: the
# first two launches of this batch were reaped with the tool call that
# started them, once at four games and once at twenty-six.
#
# Every piece of state is on disk. tierfix-ab.sh skips an arm whose
# <arm>.DONE marker exists and run.sh skips a game whose seed dir has a
# DONE marker, so re-invoking the driver is always a resume. The loop
# re-invokes until BATCHDONE, gzipping finished logs on each pass
# (tierfix-ab.sh's own sweeper dies with its parent; this one does not).
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${1:-$here/results/tierfix-ab}
BIN=${2:-$(dirname "$here")/stage/freeciv-server}
JOBS=${3:-4}
PORTBASE=${4:-7410}

sweep() {
  for d in "$OUT"/*/*/*/seed-*; do
    [ -f "$d/DONE" ] && [ -f "$d/game.log" ] && gzip -f "$d/game.log"
  done
  return 0
}

while [ ! -f "$OUT/BATCHDONE" ]; do
  # A seed dir without a DONE marker is a game that was killed
  # mid-flight; drop it so the cache re-runs it from scratch rather
  # than leaving a truncated scorelog for a reader to find.
  for d in "$OUT"/*/*/*/seed-*; do
    [ -d "$d" ] && [ ! -f "$d/DONE" ] && rm -rf "$d"
  done
  "$here/tierfix-ab.sh" "$BIN" "$OUT" "$JOBS" "$PORTBASE"
  sweep
  sleep 5
done
sweep
echo "SUPERVISOR_DONE $(date -u +%FT%TZ)"
