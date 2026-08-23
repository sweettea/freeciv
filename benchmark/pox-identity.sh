#!/usr/bin/env bash
# S-POX identity proof: at the shipping defaults (site_own_overlap 0,
# stance_source 0) every knob this epic adds is inert, so the branch
# binary must play the same games as its base -- including a MIXED cell,
# where a deep seat and a classic seat share one server and one RNG
# stream, which is the arm that catches an accidental extra fc_rand().
#
#   pox-identity.sh BASE_SERVER NEW_SERVER [OUTDIR]
#
# Each server is run with its OWN libfreeciv.so beside it (the staged
# pair), so neither arm can silently link the other's library.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BASE=${1:?base server binary}
NEW=${2:?new server binary}
OUT=${3:-$here/results/pox-identity}

mkdir -p "$OUT"
: > "$OUT/progress.txt"
ok=1
for arm in deep mixed_dc; do
  for side in base new; do
    bin=$BASE
    [ "$side" = new ] && bin=$NEW
    LD_LIBRARY_PATH=$(dirname "$bin") \
      "$here/run.sh" --count 10 --config "$here/configs/smoke.conf" \
        --arms "$arm" --jobs 3 --portbase 7400 \
        --out "$OUT/$arm-$side" --server "$bin" \
        > "$OUT/$arm-$side.log" 2>&1
    rc=$?
    echo "$arm-$side exit=$rc" >> "$OUT/progress.txt"
    [ "$rc" -eq 0 ] || ok=0
  done
done

# DONE records SUCCESS, not completion: a marker written regardless of
# exit code is how a power-cut arm got skipped at 16/20 games once.
if [ "$ok" -eq 1 ]; then
  echo "SUCCESS" > "$OUT/DONE"
else
  echo "FAILED" > "$OUT/FAILED"
fi
