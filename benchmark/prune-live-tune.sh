#!/bin/bash
# Keep a running tune from filling the disk. Each game writes ~46 MB of
# game.log; a 1450-game campaign needs ~66 GB that this box does not have.
# A game whose DONE exists is finished: tune.py re-reads only scorelog.txt
# (final_score) and skips the dir entirely on resume, so the log is dead
# weight the moment DONE lands. Never touches a dir without DONE -- that
# game may still be running.
O=${1:-/home/sweettea/freeciv/benchmark/results/decaytech-tune}
n=0
for d in $(find "$O" -name DONE -printf '%h\n'); do
  if [ -f "$d/game.log" ]; then rm -f "$d/game.log"; n=$((n+1)); fi
done
find "$O" -type d -name saves -exec rm -rf {} + 2>/dev/null
echo "pruned=$n free=$(df -h /home/sweettea | tail -1 | awk '{print $4}')"
