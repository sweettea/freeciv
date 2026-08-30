#!/usr/bin/env bash
# THE OPERATION EXCHANGE-RATE PROBE.
#
#   setsid bash ./benchmark/opxrate.sh probe    6 games, emission check
#   ./benchmark/opxrate.sh report               the census tables
#
# NOT an A/B and not a claim. The census in
# findings/op-exchange-rate-2026-08-29.md was read entirely off
# EXISTING result directories; this probe exists for one reason only --
# two quantities the launch model computes were never written down, so
# no retained log could answer them:
#
#   * `loss=`/`cost=`/`eworth=` on the `deep-op ... march` line: what
#     the launch gate forecast the assault would COST, on the branch
#     that spends the blades. Until now it was logged only when the
#     net-value test said NO (deep-netval refuse).
#   * `def_n=`/`def_best=`/`marched=` on the `deep-op ... regate`
#     lines: the garrison the abort re-gate re-priced against. The
#     abort is the largest single way an operation ends and in 57-71%
#     of them the wave had not shrunk -- so either the garrison grew
#     or the travel estimate inflated, and the line carried only the
#     latter.
#
# Six games is enough to prove the fields emit and to read their first
# distribution. It is NOT enough to price anything, and nothing in the
# findings doc rests on it.
#
# TRAINING seeds (1-6). Holdout is untouched.
# Machine budget: ports 7850-7899, jobs <= 3, nice 10, this worktree's
# own build/freeciv-server.
set -uo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"

SERVER=${SERVER:-$ROOT/build/freeciv-server}
CONFIG=$here/configs/contested.conf
TUNE=${TUNE:-$here/tuning/postfix-start.ini}
OUT=$here/results/opxrate/probe
JOBS=${JOBS:-3}
NICE=${NICE:-10}
SEEDS=${SEEDS:-$(seq 1 6)}

one() {
  local seed=$1 port=$2 d=$3
  rm -rf "$d"; mkdir -p "$d"
  env FREECIV_KILL_DYAD=1 TUNING="$TUNE" \
    nice -n "$NICE" "$here/run-game.sh" --config "$CONFIG" --seed "$seed" \
    --arm deep --out "$d" --server "$SERVER" --port "$port" \
    >/dev/null 2>&1
  if [ -f "$d/DONE" ] && [ ! -f "$d/.filtered" ]; then
    grep -aE "deep-op |deep-opodds|deep-employop|deep-netval|kill-dyad" \
         "$d/game.log" > "$d/game.log.new"
    mv "$d/game.log.new" "$d/game.log"
    rm -f "$d"/saves/*.sav*
    touch "$d/.filtered"
  fi
}

case "${1:-report}" in
probe)
  [ -x "$SERVER" ] || { echo "no server at $SERVER -- build first" >&2; exit 1; }
  mkdir -p "$OUT"
  exec 9>"$OUT/.driver.lock"
  flock -n 9 || { echo "STOP: another opxrate driver holds the lock" >&2; exit 1; }
  i=0
  for seed in $SEEDS; do
    [ -f "$OUT/deep/seed-$seed/DONE" ] && continue
    one "$seed" $((7850 + i % 10)) "$OUT/deep/seed-$seed" &
    i=$((i + 1))
    while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n; done
  done
  wait
  echo "games done: $(find "$OUT" -name DONE | wc -l)"
  ;;
report)
  python3 "$here/opxrate.py" funnel "$OUT"
  ;;
*)
  sed -n '2,30p' "$0"
  ;;
esac
