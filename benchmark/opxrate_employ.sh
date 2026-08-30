#!/bin/bash
# Where the standing army stands, averaged over every player-turn of an
# arm: the deep-employ line's own seat counts. Companion to opxrate.py
# -- that census follows operations, this one follows blades.
set -u
root=${1:?usage: opxrate_employ.sh ARMDIR}
for f in "$root"/*/seed-*/game.log.zst "$root"/seed-*/game.log.zst \
         "$root"/*/seed-*/game.log "$root"/seed-*/game.log; do
  [ -e "$f" ] || continue
  case "$f" in
    *.zst) zstdcat "$f" ;;
    *) cat "$f" ;;
  esac
done | grep " deep-employ " | awk '
{
  for (i = 1; i <= NF; i++) {
    split($i, kv, "=")
    if (kv[1] ~ /^(mil|att|attidle|muster|march|strike|raid|garrison|attack|hunt|idle)$/) {
      sum[kv[1]] += kv[2]
    }
  }
  n++
}
END {
  if (n == 0) { print "no deep-employ lines"; exit }
  printf "player-turns %d\n", n
  split("mil att attidle muster march strike raid garrison attack hunt idle", k, " ")
  for (i = 1; i <= 11; i++) {
    share = 0
    if (sum["mil"] > 0) { share = 100 * sum[k[i]] / sum["mil"] }
    printf "  %-9s %8.3f per player-turn  (%5.1f%% of mil)\n",
           k[i], sum[k[i]] / n, share
  }
}'
