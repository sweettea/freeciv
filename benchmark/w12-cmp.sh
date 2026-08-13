#!/usr/bin/env bash
# Compare scorelog sha256 per seed between two result trees.
#   w12-cmp.sh DIR_A DIR_B
set -uo pipefail
A=${1:?dir a}
B=${2:?dir b}

for arm in $(cd "$A"/*/ && ls -d */ 2>/dev/null | tr -d /); do
  same=0; diff=0; miss=0
  for sd in "$A"/*/"$arm"/seed-*; do
    seed=$(basename "$sd")
    other=$(echo "$B"/*/"$arm"/"$seed")
    if [ ! -r "$sd/scorelog.txt" ] || [ ! -r "$other/scorelog.txt" ]; then
      miss=$((miss + 1)); continue
    fi
    ha=$(sha256sum "$sd/scorelog.txt" | cut -d' ' -f1)
    hb=$(sha256sum "$other/scorelog.txt" | cut -d' ' -f1)
    if [ "$ha" = "$hb" ]; then same=$((same + 1)); else
      diff=$((diff + 1)); echo "  DIFFERS: $arm $seed"
    fi
  done
  echo "$arm: identical=$same differing=$diff missing=$miss"
done
