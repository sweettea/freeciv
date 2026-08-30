#!/usr/bin/env bash
# S-SITEPLAN power extension: run MORE seeds for named cells only.
#
#   FROM=41 TO=80 setsid bash ./benchmark/siteplan-more.sh war c00 c10
#
# Same binary, same inis and same layout as siteplan.sh -- it reuses the
# inis that driver already wrote under results/siteplan/ -- so the extra
# games join the existing cells and siteplan.py pairs each contrast on
# whatever seeds both its cells have. A separate file rather than a
# subcommand because bash re-reads a running script from disk and this
# is launched while the main driver may still be alive.
#
# PORTS 7850-7899, this worktree only.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

FIX=${FIX:-$ROOT/.pin-bin/fix/freeciv-server}
OUT=${OUT:-$here/results/siteplan}
JOBS=${JOBS:-8}
NICE=${NICE:-10}
FROM=${FROM:-41}
TO=${TO:-80}
venue=${1:?venue}
shift
KEEP='deep-settler |deep-join |deep-joinguard |deep-city |deep-siteladder |deep-pool |deep-sitecap |deep-prodswitch |deep-expand '

case "$venue" in
  war)  CFG=$here/configs/contested.conf;;
  econ) CFG=$here/configs/lategame-80.conf;;
  *) echo "unknown venue $venue" >&2; exit 1;;
esac

SEEDS=$(sed -n "${FROM},${TO}p" "$here/seeds/train.txt")

exec 9>"$OUT/.driver.lock"
flock -n 9 || { echo "STOP: another siteplan driver holds the lock" >&2; exit 1; }

one() { # seed port dir tune
  local seed=$1 port=$2 d=$3 tune=$4
  rm -rf "$d"; mkdir -p "$d"
  env TUNING="$tune" nice -n "$NICE" "$here/run-game.sh" \
    --config "$CFG" --seed "$seed" --arm deep --out "$d" \
    --server "$FIX" --port "$port" >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "$KEEP" "$d/game.log" > "$d/game.log.new" 2>/dev/null
    mv -f "$d/game.log.new" "$d/game.log"
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

for cell in "$@"; do
  echo "=== $venue/$cell seeds $FROM-$TO $(date -Is) ==="
  i=0
  for seed in $SEEDS; do
    d=$OUT/$venue/$cell/seed-$seed
    if [ ! -f "$d/DONE" ]; then
      one "$seed" $((7850 + i % 40)) "$d" "$OUT/$cell.ini" &
      i=$((i + 1))
      while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
    fi
  done
  wait
  echo "$venue/$cell now $(find "$OUT/$venue/$cell" -name DONE | wc -l) games"
done
