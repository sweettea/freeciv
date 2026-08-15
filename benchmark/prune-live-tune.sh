#!/bin/bash
# Keep a running tune from filling the disk. Each game writes ~46 MB of
# game.log; a 1450-game campaign needs ~66 GB that this box does not have.
# A game whose DONE exists is finished: tune.py re-reads only scorelog.txt
# (final_score) and skips the dir entirely on resume, so the log is dead
# weight the moment DONE lands. Never touches a dir without DONE -- that
# game may still be running.
#
# The saves/ sweep is DONE-gated for the same reason, and the reason is
# not hypothetical: run-game.sh computes maphash.txt from
# saves/freeciv-T0001-*-auto.sav* *after* the server exits, so deleting a
# live game's saves/ makes that game fail its own completion step with
# "FAILED: no turn-1 autosave for map hash". An unconditional sweep here
# cost three games in r1-DECAYTECH gen1.
O=${1:-/home/sweettea/freeciv/benchmark/results/decaytech-tune}
n=0
s=0
for d in $(find "$O" -name DONE -printf '%h\n'); do
  if [ -f "$d/game.log" ]; then rm -f "$d/game.log"; n=$((n+1)); fi
  if [ -d "$d/saves" ]; then rm -rf "$d/saves"; s=$((s+1)); fi
done
echo "pruned=$n saves=$s free=$(df -h /home/sweettea | tail -1 | awk '{print $4}')"
