#!/usr/bin/env bash
# The op_muster ON arm, re-run on a binary carrying the member-departure
# counter (deep-employop `dep=`). Same venue, seeds, tuning and lineage
# as the ON arm of the 2026-08-22 muster A/B.
#
#   setsid bash ./benchmark/muster-depart.sh </dev/null >run.log 2>&1 &
#
# Launch it from the repo root of a worktree, detached. It is
# self-contained and resumable: a game whose output directory carries
# DONE is skipped, so a driver killed at any point picks up where it
# stopped. Watch it by DONE counts and scorelog mtimes; this box does
# not report its process table honestly.
#
# Each finished game is pruned to its ledger lines and its savegames
# dropped before the next one starts, which is what keeps 40 games of
# tech50war-200 inside ~60 MB.
#
# Environment: OUT names the result tree (required), BIN the pinned
# server, JOBS the concurrency, PORT0 the base of a ten-port block.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WT=$(CDPATH= cd -- "$here/.." && pwd)

: "${OUT:?set OUT to the result tree}"
: "${BIN:=$WT/stage/freeciv-server}"
: "${JOBS:=4}"
: "${PORT0:=7300}"
: "${TUNE:=$WT/benchmark/tuning/pv3-muster.ini}"
: "${CONFIG:=$WT/benchmark/configs/tech50war-200.conf}"
: "${SEEDS:=$(seq 17 36)}"

[ -x "$BIN" ] || { echo "no server at $BIN" >&2; exit 1; }
echo "binary $(sha256sum "$BIN")"
echo "tuning $(sha256sum "$TUNE")"

one() {
  local seed=$1 arm=$2 port=$3 d=$OUT/$arm/seed-$seed
  # A partial directory from an earlier kill is deleted rather than
  # resumed: a leftover scorelog.txt in a re-run directory silently
  # disables scorelogging for the whole game (STATUS 2026-08-13).
  rm -rf "$d"; mkdir -p "$d"
  env FREECIV_KILL_DYAD=1 TUNING="$TUNE" \
    "$WT/benchmark/run-game.sh" --config "$CONFIG" --seed "$seed" \
    --arm "$arm" --out "$d" --server "$BIN" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-employ|kill-dyad" "$d/game.log" > "$d/game.log.new"
    mv "$d/game.log.new" "$d/game.log"
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

i=0
for seed in $SEEDS; do
  for arm in mixed_dc mixed_cd; do
    [ -f "$OUT/$arm/seed-$seed/DONE" ] && continue
    one "$seed" "$arm" $((PORT0 + i % 10)) &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
done
wait
touch "$OUT/ALLDONE"
echo "complete: $(find "$OUT" -name DONE | wc -l) games"
