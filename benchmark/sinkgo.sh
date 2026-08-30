#!/usr/bin/env bash
# S-FEARSINK batch: both venues in sequence, then the report. Identity
# is already SUCCESS, so `go` would only re-check it; this is the rest.
set -uo pipefail
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$here/.."
bash "$here/fearsink.sh" war
bash "$here/fearsink.sh" econ
bash "$here/fearsink.sh" report > "$here/results/fearsink/report.txt" 2>&1
echo "ALLDONE $(date -Is)" > "$here/results/fearsink/DONE-ALL"
