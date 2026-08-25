#!/usr/bin/env bash
# One gate-on smoke game, for eyeballing that the credit fires at all.
# Not part of the adjudication; port 7848, this worktree only.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$here/.." && pwd)
cd "$ROOT"
OUT=${1:-$here/results/prodkeep/probe}
rm -rf "$OUT"
env TUNING="$here/results/prodkeep/on.ini" nice -n 10 "$here/run-game.sh" \
  --config "$here/configs/smoke.conf" --seed 1 --arm deep --out "$OUT" \
  --server "$ROOT/.pin-bin/fix/freeciv-server" --port 7848 >/dev/null 2>&1
echo "prodkeep lines: $(grep -ac 'deep-prodkeep ' "$OUT/game.log")"
echo "prodswitch lines: $(grep -ac 'deep-prodswitch ' "$OUT/game.log")"
echo "prodswitch with keep>0: $(grep -a 'deep-prodswitch ' "$OUT/game.log" \
  | grep -avc 'keep=0\.0*$')"
grep -a 'deep-prodkeep ' "$OUT/game.log" | head -5
