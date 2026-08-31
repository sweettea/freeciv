#!/usr/bin/env bash
# Compress game.log and drop autosaves for finished games under
# benchmark/results. tune.py's cache needs only the DONE marker and
# scorelog.txt; game.log is kept (compressed ~10:1) for later log
# censuses, never read by the tuner; saves/ of a finished game are
# never read by anything. Runs from a systemd timer while a campaign
# fills disk.
set -u
cd "$(dirname "$0")/results"
find . -name game.log -printf '%h\n' | while read -r d; do
  [ -f "$d/DONE" ] && zstd -q -f --rm -3 "$d/game.log"
done
find . -type d -name saves -printf '%h\n' | while read -r d; do
  [ -f "$d/DONE" ] && rm -rf "$d/saves"
done
exit 0
