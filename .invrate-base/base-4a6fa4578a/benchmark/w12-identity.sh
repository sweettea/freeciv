#!/usr/bin/env bash
# W12 identity proofs (CAMPAIGN-v4 11.5 belt-and-braces): the widened
# advisor hook must leave non-deep players byte-identical, and deep
# itself must be byte-identical at the new knobs' zero defaults.
#
# Four batches on the same 10 smoke seeds, run in sequence so one port
# block is enough: stock arms then the deep arm, each on the base
# binary and on this branch's binary.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$here/results/w12-identity
BASE=${1:?base server binary}
NEW=${2:?new server binary}

mkdir -p "$OUT"
for spec in "stock:classic,baseline" "deep:deep"; do
  name=${spec%%:*}
  arms=${spec#*:}
  for side in base new; do
    bin=$BASE
    [ "$side" = new ] && bin=$NEW
    "$here/run.sh" --count 10 --config "$here/configs/smoke.conf" \
      --arms "$arms" --jobs 2 --portbase 7200 \
      --out "$OUT/$name-$side" --server "$bin" \
      > "$OUT/$name-$side.log" 2>&1
    echo "$name-$side exit=$?" >> "$OUT/progress.txt"
  done
done
touch "$OUT/ALLDONE"
