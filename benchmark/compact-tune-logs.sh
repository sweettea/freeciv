#!/usr/bin/env bash
# Compress game.log and drop autosaves for finished games under
# benchmark/results. tune.py's cache needs only the DONE marker and
# scorelog.txt; game.log is kept (compressed ~10:1) for later log
# censuses, never read by the tuner; saves/ of a finished game are
# never read by anything. Runs from a systemd timer while a campaign
# fills disk.
#
# NOT under scoreboard/: its driver reads a finished game's game.log
# (guard) and saves/ (the Europe T150 starts, the europe-late final
# save) in the seconds after DONE, and prunes its own games. The timer
# deleted two T150 starts and one guard's game.log that way
# (2026-11-03/04).
set -u
cd "$(dirname "$0")/results"
find . -path ./scoreboard -prune -o -name game.log -printf '%h\n' | while read -r d; do
  [ -f "$d/DONE" ] && zstd -q -f --rm -3 "$d/game.log"
done
find . -path ./scoreboard -prune -o -type d -name saves -printf '%h\n' | while read -r d; do
  [ -f "$d/DONE" ] && rm -rf "$d/saves"
done
exit 0
